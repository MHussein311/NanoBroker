#include "nanobroker/video_protocol.hpp"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <thread>
#include <chrono>

int main(int argc, char* argv[]) {
    std::cout << "Producer Struct Size: " << sizeof(Protocol::CameraFrame) << std::endl;
    int my_id = 0;
    if (argc > 1) my_id = std::atoi(argv[1]);

    try {
        bool is_creator = (my_id == 0);
        NanoBroker::BrokerSettings settings;
        settings.overflow_policy = NanoBroker::OverflowPolicy::OVERWRITE_OLD;
        
        NanoBroker::Broker<Protocol::CameraFrame, Protocol::BUFFER_SIZE, Protocol::MAX_CONSUMERS> broker(Protocol::TOPIC_NAME, true, 0, settings);


        const int W = 640;
        const int H = 480;
        
        std::cout << "[Producer] Streaming generated video..." << std::endl;

        int frame_count = 0;
        int x_pos = 0; 

        while (true) {
            Protocol::CameraFrame* frame_ptr = broker.prepare_publish();

            if (frame_ptr) {
   
                // Zero copy mechanism
                cv::Mat img(H, W, CV_8UC3, frame_ptr->pixels);

                img = cv::Scalar(0, 0, 0);

                cv::circle(img, cv::Point(x_pos, H/2), 50, cv::Scalar(0, 0, 255), -1);
          

                std::string text = "Cam " + std::to_string(my_id) + " | Frame: " + std::to_string(frame_count);
                cv::putText(img, text, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 2);

 
                x_pos = (x_pos + 5) % W;

                frame_ptr->frame_id = frame_count++;
                frame_ptr->width = W;
                frame_ptr->height = H;
                frame_ptr->channels = 3;

                frame_ptr->data_size = W * H * 3; 

                frame_ptr->format = "BGR";
                
                auto now = std::chrono::high_resolution_clock::now();
                frame_ptr->timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    now.time_since_epoch()).count();

                frame_ptr->producer_id = my_id;

                broker.commit_publish();
                
                if (frame_count % 30 == 0) {
                    std::cout << "[Producer " << my_id << "] Sent Frame " << frame_count << std::endl;
                }

            } else {
   
                static int full_log = 0;
                if (full_log++ % 30 == 0) {
                    std::cout << "[Producer " << my_id << "] Buffer Full! Waiting for consumers..." << std::endl;
                }

            }

            std::this_thread::sleep_for(std::chrono::milliseconds(33));
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
