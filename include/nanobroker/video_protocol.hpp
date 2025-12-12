#ifndef VIDEO_PROTOCOL_HPP
#define VIDEO_PROTOCOL_HPP


#include "NanoBroker.hpp"
#include <cstdint>

namespace Protocol {
const int MAX_WIDTH = 1920;
const int MAX_HEIGHT = 1080; // 1080p
const int CHANNELS = 3;
const size_t MAX_SIZE = MAX_WIDTH * MAX_HEIGHT * CHANNELS;

// --- User Configuration ---

const size_t BUFFER_SIZE = 30;
const size_t MAX_CONSUMERS = 16;

struct CameraFrame {
  int producer_id;
  int frame_id;
  int64_t timestamp_ns;
  int width;
  int height;
  int channels;

  size_t data_size;

  NanoBroker::NanoString<16> format;
  alignas(64) uint8_t pixels[MAX_SIZE];
};

const std::string TOPIC_NAME = "video_stream";
} // namespace Protocol
#endif
