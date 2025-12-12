#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>
#include "../include/nanobroker/video_protocol.hpp"

namespace py = pybind11;

class PyVideoBroker {

    using FrameType = Protocol::CameraFrame;
    NanoBroker::Broker<FrameType, Protocol::BUFFER_SIZE, Protocol::MAX_CONSUMERS> broker;

public:
    PyVideoBroker(const std::string& name, bool is_producer, int consumer_id) 
        : broker(name, is_producer, consumer_id) {
            std::cout << "Consumer (Python) Struct Size: " << sizeof(FrameType) << std::endl;
    }

    // ----- Consumer API -----
py::object get_next_frame() {
        const FrameType* frame = broker.wait_and_peek(); 
        
        if (!frame) return py::none();

        // Atomically snapshot metadata to avoid tearing
        int w = frame->width;
        int h = frame->height;
        int c = frame->channels;
        size_t size = frame->data_size;
        int pid = frame->producer_id;
        int fid = frame->frame_id;
      
        std::vector<ssize_t> shape;
        std::vector<ssize_t> strides;

        if (size > Protocol::MAX_SIZE) {
            size = Protocol::MAX_SIZE; 
        }

        if (w > 0 && h > 0 && c > 0) {
            
            size_t expected = static_cast<size_t>(w * h * c);
            
            if (expected != size) {
                // Fallback to 1D in case of mismatch
                shape = { (ssize_t)size };
                strides = { (ssize_t)1 };
            } else {
                shape = { (ssize_t)h, (ssize_t)w, (ssize_t)c };
                strides = { (ssize_t)(w * c), (ssize_t)c, (ssize_t)1 };
            }
        } else {
            shape = { (ssize_t)size };
            strides = { (ssize_t)1 };
        }

        auto array = py::array_t<uint8_t>(
            shape, 
            strides, 
            frame->pixels, 
            py::capsule(frame, [](void* p) { })
        );

        return py::make_tuple(pid, fid, array);
    }

    void release_frame() {
        broker.release();
    }

    // ------ Producer API ------
    bool publish_frame(int id, int w, int h, py::array_t<uint8_t> input_array) {

        py::buffer_info buf = input_array.request();

        // Safety check
        if (static_cast<size_t>(buf.size) > Protocol::MAX_SIZE) {
            std::cerr << "[C++] Frame too big for shared memory!" << std::endl;
            return false;
        }

        auto* slot = broker.prepare_publish();
        if (!slot) return false;

        slot->frame_id = id;
        slot->width = w;
        slot->height = h;
        
        // Copy bytes from Python Buffer to Shared Memory
        std::memcpy(slot->pixels, buf.ptr, buf.size * sizeof(uint8_t));
        
        auto now = std::chrono::high_resolution_clock::now();
        slot->timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()).count();

        broker.commit_publish();
        return true;
    }
};

// ------ Python Module Definition ------


PYBIND11_MODULE(nanobroker, m) {
    m.doc() = R"pbdoc(
        NanoBroker: Zero-Copy IPC Library
        ---------------------------------
        High-performance bridge between C++ and Python using Shared Memory.
        Supports 1-to-N and N-to-N broadcasting with Ring Buffer architecture.
    )pbdoc";

    m.attr("DEFAULT_TOPIC") = Protocol::TOPIC_NAME; 

    py::class_<PyVideoBroker>(m, "VideoBroker")
        .def(py::init<std::string, bool, int>(), 
            py::arg("topic"), py::arg("is_producer") = false, py::arg("consumer_id") = 0,
            R"pbdoc(
                Connect to a NanoBroker topic.
                
                Args:
                    topic (str): The shared memory name (e.g. "video_stream").
                    is_producer (bool): Set True if you intend to write data (Master).
                    consumer_id (int): Unique ID (0-15) for this consumer process.
                                     Ignored if is_producer is True.
            )pbdoc"
        )
        .def("get_next_frame", &PyVideoBroker::get_next_frame,
            R"pbdoc(
                Wait for the next available frame (Zero-Copy).
                
                Returns:
                    tuple: (producer_id, frame_id, numpy_array) OR None if empty.
                    
                    The numpy_array is a direct view into Shared Memory.
                    It is read-only unless you explicitly .copy() it.
            )pbdoc"
        )
        .def("release_frame", &PyVideoBroker::release_frame,
            R"pbdoc(
                Release the current slot so the producer can reuse it.
                MUST be called after processing the frame to advance the tail.
            )pbdoc"
        )
        .def("publish_frame", &PyVideoBroker::publish_frame,
            py::arg("id"), py::arg("w"), py::arg("h"), py::arg("input_array"),
            R"pbdoc(
                Write a NumPy array to shared memory.
                
                Args:
                    id (int): Frame Sequence ID.
                    w (int): Width.
                    h (int): Height.
                    input_array (numpy.ndarray): The data (must be uint8).
                Returns:
                    bool: True if successful, False if buffer full.
            )pbdoc"
        );
}