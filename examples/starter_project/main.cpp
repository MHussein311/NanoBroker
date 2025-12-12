#include <nanobroker/video_protocol.hpp>
#include <iostream>

int main() {
    std::cout << "NanoBroker headers found successfully!" << std::endl;
    
    // Instantiate test
    NanoBroker::Broker<Protocol::CameraFrame, 30> broker("test_channel", true);
    
    std::cout << "Broker initialized. Ready to write code." << std::endl;
    return 0;
}

