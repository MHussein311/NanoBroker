#include "nanobroker/video_protocol.hpp"
#include <iostream>
#include <string>
#include <vector>

void print_help() {
    std::cout << "Usage: nanoadmin <command> [args]\n"
              << "Commands:\n"
              << "  stats       Show buffer status and active consumers\n"
              << "  kick <id>   Forcefully remove a dead consumer ID\n"
              << "  clean       Delete the shared memory file (Fix startup error)\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_help();
        return 1;
    }

    std::string command = argv[1];
    std::string topic = Protocol::TOPIC_NAME; // "video_stream"

    try {
        if (command == "clean") {

            NanoBroker::Broker<Protocol::CameraFrame, Protocol::BUFFER_SIZE>::unlink_memory(topic);
            return 0;
        }


        NanoBroker::Broker<Protocol::CameraFrame, Protocol::BUFFER_SIZE, Protocol::MAX_CONSUMERS> 
    broker(topic, false, -99);

        if (command == "stats") {
            broker.print_stats();
        } 
        else if (command == "kick") {
            if (argc < 3) {
                std::cerr << "Error: Provide consumer ID to kick.\n";
                return 1;
            }
            int id = std::stoi(argv[2]);
            broker.force_disconnect_consumer(id);
        }
        else {
            print_help();
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << "(Is the producer running? Admin tool needs the memory to exist for stats/kick)" << std::endl;
        return 1;
    }

    return 0;
}