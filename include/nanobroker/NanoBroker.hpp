#ifndef NANOBROKER_HPP
#define NANOBROKER_HPP

#include <atomic>
#include <cstring>
#include <fcntl.h>
#include <immintrin.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <thread>
#include <type_traits>
#include <unistd.h>
#include <random>

namespace NanoBroker {


const uint64_t MAGIC_NUMBER = 0x4E414E4F42524F4B; // "NANOBROK" in hex
const uint32_t PROTOCOL_VERSION = 2;
const int MAX_CONSUMERS = 16;

enum class OverflowPolicy { BLOCK, OVERWRITE_OLD };


enum class SlotState : uint32_t {
    FREE = 0,
    WRITING = 1,
    READY = 2
};

struct BrokerSettings {
    OverflowPolicy overflow_policy = OverflowPolicy::BLOCK;
    int64_t producer_timeout_ms = 10000;
    int spin_iterations = 1000;
    int yield_iterations = 10000;
};

template <typename T> void validate_type() {
    static_assert(std::is_trivially_copyable<T>::value,
                  "NanoBroker Error: Data type must be POD.");
}


template <typename T>
struct alignas(64) SlotWrapper {
    std::atomic<uint64_t> sequence{0};   
    std::atomic<SlotState> state{SlotState::FREE}; 
    T data;
};

template <typename T, size_t BufferSize, size_t MaxConsumers>
struct alignas(64) SharedChannel {

    uint64_t magic;
    uint32_t version;
    uint32_t struct_size;
    uint32_t buffer_capacity;
    uint64_t producer_epoch;
    
    alignas(64) std::atomic<size_t> head;
    alignas(64) std::atomic<size_t> tails[MaxConsumers];
    alignas(64) std::atomic<bool> slot_active[MaxConsumers];
    alignas(64) std::atomic<int64_t> heartbeats[MaxConsumers];
    alignas(64) std::atomic_flag write_lock = ATOMIC_FLAG_INIT;

    alignas(64) SlotWrapper<T> slots[BufferSize];
};

template <size_t N>
struct NanoString {
    char buffer[N];
    NanoString() { buffer[0] = '\0'; }
    NanoString(const char *str) { std::strncpy(buffer, str, N - 1); buffer[N - 1] = '\0'; }
    NanoString(const std::string &str) { std::strncpy(buffer, str.c_str(), N - 1); buffer[N - 1] = '\0'; }
    const char *c_str() const { return buffer; }
    NanoString &operator=(const std::string &str) { std::strncpy(buffer, str.c_str(), N - 1); buffer[N - 1] = '\0'; return *this; }
    NanoString &operator=(const char *str) { std::strncpy(buffer, str, N - 1); buffer[N - 1] = '\0'; return *this; }
};

template <typename T, size_t BufferSize = 30, size_t MaxConsumers = 16>
class Broker {
private:
    std::string name;
    int shm_fd;
    SharedChannel<T, BufferSize, MaxConsumers> *channel;
    bool is_owner;
    int consumer_id;
    uint64_t local_epoch_cache = 0;
    BrokerSettings settings;
    SlotWrapper<T>* pending_slot = nullptr; 

    int64_t now_ms() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

public:
    Broker(const std::string &channel_name, bool create = false, int id = 0,
           BrokerSettings custom_settings = BrokerSettings())
        : name("/" + channel_name), shm_fd(-1), channel(nullptr),
          is_owner(create), consumer_id(create ? -1 : id), settings(custom_settings)
    {
        validate_type<T>();

        if (create) {
            
            shm_unlink(name.c_str());

            shm_fd = shm_open(name.c_str(), O_CREAT | O_RDWR, 0666);
            if (shm_fd == -1) throw std::runtime_error("Failed to create shared memory");
            if (ftruncate(shm_fd, sizeof(SharedChannel<T, BufferSize, MaxConsumers>)) == -1)
                throw std::runtime_error("Resize failed");
        } else {
            shm_fd = shm_open(name.c_str(), O_RDWR, 0666);
            if (shm_fd == -1) throw std::runtime_error("Failed to open shared memory (Producer not running?)");
        }

        void *ptr = mmap(0, sizeof(SharedChannel<T, BufferSize, MaxConsumers>),
                         PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        if (ptr == MAP_FAILED) throw std::runtime_error("mmap failed");

        channel = static_cast<SharedChannel<T, BufferSize, MaxConsumers> *>(ptr);

        if (create) {
            channel->magic = MAGIC_NUMBER;
            channel->version = PROTOCOL_VERSION;
            std::random_device rd;
            std::mt19937_64 gen(rd());
            std::uniform_int_distribution<uint64_t> dis;
            channel->producer_epoch = dis(gen);
            channel->struct_size = sizeof(T);
            channel->buffer_capacity = BufferSize;

            new (&channel->head) std::atomic<size_t>(0);
            for (size_t i = 0; i < MaxConsumers; i++) {
                new (&channel->tails[i]) std::atomic<size_t>(0);
                new (&channel->slot_active[i]) std::atomic<bool>(false);
                new (&channel->heartbeats[i]) std::atomic<int64_t>(0);
            }
   
            for(size_t i=0; i<BufferSize; i++) {
                new (&channel->slots[i].sequence) std::atomic<uint64_t>(0);
                new (&channel->slots[i].state) std::atomic<SlotState>(SlotState::FREE);
            }
        } else {
    
            if (channel->magic != MAGIC_NUMBER) throw std::runtime_error("SHM Magic Mismatch! (Old/Corrupt Memory)");
            if (channel->version != PROTOCOL_VERSION) throw std::runtime_error("Protocol Version Mismatch!");
            if (channel->struct_size != sizeof(T)) throw std::runtime_error("Data Struct Size Mismatch!");

            if (id != -99) {
                if (consumer_id < 0 || consumer_id >= static_cast<int>(MaxConsumers)) {
                    throw std::runtime_error("Invalid Consumer ID");
                }
            
                size_t h = channel->head.load(std::memory_order_relaxed);
                channel->tails[consumer_id].store(h, std::memory_order_release);
                channel->heartbeats[consumer_id].store(now_ms(), std::memory_order_release);
                channel->slot_active[consumer_id].store(true, std::memory_order_release);
            }
        }
    }

    ~Broker() {
        if (!is_owner && channel && consumer_id != -99) {
            channel->slot_active[consumer_id].store(false, std::memory_order_release);
        }
        if (channel) munmap(channel, sizeof(SharedChannel<T, BufferSize, MaxConsumers>));
        if (shm_fd != -1) close(shm_fd);
        
    }



    T *prepare_publish(int64_t timeout_ms = 2000) {

        while (channel->write_lock.test_and_set(std::memory_order_acquire)) { _mm_pause(); }

        size_t current_head = channel->head.load(std::memory_order_relaxed);
        size_t next_head = (current_head + 1) % BufferSize;
        int64_t now = now_ms();
        bool full = false;


        for (size_t i = 0; i < MaxConsumers; i++) {
            if (channel->slot_active[i].load(std::memory_order_relaxed)) {
                size_t t = channel->tails[i].load(std::memory_order_acquire);
                if (next_head == t) {
           
                    int64_t last = channel->heartbeats[i].load(std::memory_order_relaxed);
                    if ((now - last) > timeout_ms) {
                        channel->slot_active[i].store(false, std::memory_order_release);
                        std::cerr << "[NanoBroker] Auto-kicked consumer " << i << std::endl;
                        continue; 
                    }
                    
                    if (settings.overflow_policy == OverflowPolicy::BLOCK) {
                        full = true; break;
                    } else {
                        channel->tails[i].store((t + 1) % BufferSize, std::memory_order_release);
                    }
                }
            }
        }

        if (full) {
            channel->write_lock.clear(std::memory_order_release);
            return nullptr;
        }

        pending_slot = &channel->slots[current_head];
        pending_slot->state.store(SlotState::WRITING, std::memory_order_release);
        
        return &pending_slot->data;
    }

void commit_publish() {
        if (!pending_slot) return;

        size_t current_head = channel->head.load(std::memory_order_relaxed);
        

        
        uint64_t seq = pending_slot->sequence.load(std::memory_order_relaxed);
        pending_slot->sequence.store(seq + 1, std::memory_order_release);

        pending_slot->state.store(SlotState::READY, std::memory_order_release);
        
        size_t next_head = (current_head + 1) % BufferSize;
        channel->head.store(next_head, std::memory_order_release);
        
        channel->write_lock.clear(std::memory_order_release); 
        pending_slot = nullptr;
    }



    const T *peek() {

        uint64_t current_epoch = channel->producer_epoch; 
        

        if (local_epoch_cache == 0) local_epoch_cache = current_epoch;


        if (current_epoch != local_epoch_cache) {
            std::cerr << "[NanoBroker] Producer restarted! Resetting tail." << std::endl;
    
            size_t new_head = channel->head.load(std::memory_order_relaxed);
            channel->tails[consumer_id].store(new_head, std::memory_order_release);
            local_epoch_cache = current_epoch;
            return nullptr; 
        }

        if (!channel->slot_active[consumer_id].load(std::memory_order_relaxed)) {
            throw std::runtime_error("Consumer disconnected.");
        }
        channel->heartbeats[consumer_id].store(now_ms(), std::memory_order_relaxed);

        size_t current_tail = channel->tails[consumer_id].load(std::memory_order_relaxed);
        if (current_tail == channel->head.load(std::memory_order_acquire)) {
            return nullptr;
        }

        auto* slot = &channel->slots[current_tail];
        
      
        uint64_t seq_before = slot->sequence.load(std::memory_order_acquire);

 
        int spin = 0;
        while (slot->state.load(std::memory_order_acquire) != SlotState::READY) {
            _mm_pause();
            if (++spin > 10000) return nullptr;
        }


        uint64_t seq_after = slot->sequence.load(std::memory_order_acquire);

        if (seq_before != seq_after) {

            release(); 
            return nullptr; 
        }

        return &slot->data;
    }

    void release() {
       channel->heartbeats[consumer_id].store(now_ms(), std::memory_order_relaxed);
        
        size_t current_tail = channel->tails[consumer_id].load(std::memory_order_relaxed);
        channel->tails[consumer_id].store((current_tail + 1) % BufferSize, std::memory_order_release);
    }

    const T *wait_and_peek() {
        int spin_count = 0;
        const T *ptr = nullptr;
        while ((ptr = peek()) == nullptr) {
            if (spin_count < settings.spin_iterations) { _mm_pause(); spin_count++; }
            else if (spin_count < settings.yield_iterations) { std::this_thread::yield(); spin_count++; }
            else { std::this_thread::sleep_for(std::chrono::microseconds(1)); }
        }
        return ptr;
    }


    
    void print_stats() {
        size_t h = channel->head.load(std::memory_order_relaxed);
        int64_t now = now_ms();
        std::cout << "--- NanoBroker Stats [" << name << "] ---" << std::endl;
        std::cout << "Magic: " << std::hex << channel->magic << std::dec << std::endl;
        std::cout << "Head: " << h << std::endl;
        
        for (size_t i=0; i<MaxConsumers; i++) {
            if (channel->slot_active[i].load(std::memory_order_relaxed)) {
                size_t t = channel->tails[i].load(std::memory_order_relaxed);
                int64_t hb = channel->heartbeats[i].load(std::memory_order_relaxed);
                int64_t age = now - hb;
                std::cout << "  [ID " << i << "] Tail: " << t << " | Age: " << age << "ms" << std::endl;
            }
        }
        std::cout << "-----------------------------------" << std::endl;
    }
    
    void force_disconnect_consumer(int id) {
        if (id < 0 || id >= (int)MaxConsumers) return;
        channel->slot_active[id].store(false, std::memory_order_release);
    }

    static void unlink_memory(const std::string &name) {
        std::string path = "/" + name;
        shm_unlink(path.c_str());
    }
};

} // namespace NanoBroker
#endif