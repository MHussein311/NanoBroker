
# NanoBroker README

This README provides full instructions for installing NanoBroker, creating a new project, configuring CMake, building and running producers/consumers in both C++ and Python, and understanding the available API. It is written for users with **no prior knowledge** of the project.

---

## 1. Starting Point and Repository Setup

Begin in your home directory:

```
cd ~
```

Clone the NanoBroker repository:

```
git clone https://github.com/yourusername/NanoBroker.git
cd NanoBroker
```


---

## 2. System Dependencies

Install compiler tools, Python headers, and CMake. Setup can be achieved by running the following script:

```
sudo ./install_deps.sh
```

Or manually install:

```
sudo apt update
sudo apt install -y build-essential cmake python3-dev python3-venv
```

If you are capturing images or video, install OpenCV development headers:

```
sudo apt install -y libopencv-dev
```

---

## 3. Recommended: Python Virtual Environment

Create a dedicated venv inside NanoBroker:

```
python3 -m venv venv
source venv/bin/activate
```

Install NanoBroker into the venv:

```
pip install .
```

This installs:

- Python module: `nanobroker`
- Include directory for C++ headers
- Administrative tool: `nanoadmin`

---

## 4. Use NanoBroker in your own project

Your application code should be in its own directory. NanoBroker will be imported after

```
cd YourApp
```

Create a virtual env for your application:

```
python3 -m venv venv
source venv/bin/activate
```

Install NanoBroker from the local clone (use the appropiate path from your project to NanoBroker):

```
pip install ~/NanoBroker
```

---

## 5. Required CMakeLists.txt

Create a file named `CMakeLists.txt` in your project root:

```
cmake_minimum_required(VERSION 3.10)
project(MyRobotApp)

set(CMAKE_CXX_STANDARD 17)

# --- NANO-BROKER CONFIGURATION ---
execute_process(
    COMMAND python3 -c "import nanobroker_config; print(nanobroker_config.get_include())"
    OUTPUT_VARIABLE NANOBROKER_INCLUDE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE PYTHON_RESULT
)

if(NOT "${PYTHON_RESULT}" STREQUAL "0")
    message(FATAL_ERROR "NanoBroker not found! Did you run 'pip install ~/NanoBroker'?")
endif()

include_directories(${NANOBROKER_INCLUDE})

# Find OpenCV if needed
find_package(OpenCV REQUIRED)

# Builds the example executable
add_executable(my_producer producer.cpp)
target_link_libraries(my_producer rt pthread ${OpenCV_LIBS})
```

This automatically finds the NanoBroker headers inside your venv.

---

## 6. Building the C++ Project

Run:

```
mkdir build
cd build
cmake ..
make -j8
```

Your binary (e.g., `my_producer`) will appear in `build/`.

---

## 7. Using NanoBroker — Role-Based API

NanoBroker supports all combinations:

- C++ → C++
- C++ → Python
- Python → Python
- Python → C++

Below are the minimal code lines required to set up each role.

---

## 7A. C++ Producer (Writer)

Required includes:

```
#include <nanobroker/video_protocol.hpp>
#include <cstring>
```

Initialization:

```
NanoBroker::Broker<Protocol::CameraFrame, Protocol::BUFFER_SIZE> broker(Protocol::TOPIC_NAME, true);
```

Publishing:

```
auto* slot = broker.prepare_publish();
if (slot) {
    slot->width = 1920;
    slot->height = 1080;
    slot->channels = 3;
    slot->data_size = slot->width * slot->height * slot->channels;

    std::memcpy(slot->pixels, my_buffer, slot->data_size);

    broker.commit_publish();
}
```

---

## 7B. C++ Consumer (Reader)

Initialization:

```
NanoBroker::Broker<Protocol::CameraFrame, 30> broker("video_stream", false, 0);
```

Reading:

```
const auto* frame = broker.wait_and_peek();
int w = frame->width;
int h = frame->height;
auto data_ptr = frame->pixels;
broker.release();
```

---

## 7C. Python Consumer

```
import nanobroker

topic = nanobroker.DEFAULT_TOPIC
broker = nanobroker.VideoBroker(topic, False, 0)

while True:
    data = broker.get_next_frame()
    if data:
        pid, fid, img = data
        # Process img...
        broker.release_frame()
```

---

## 7D. Python Producer

```
import nanobroker
import numpy as np

broker = nanobroker.VideoBroker("video_stream", True, 0)

arr = np.random.randint(0,255,(720,1280,3),dtype=np.uint8)
broker.publish_frame(0, 1280, 720, arr)
```

---

## 8. API Reference

### C++ API

**prepare_publish()**

- Returns pointer to next free slot or `nullptr`
- Locks slot for writing

**commit_publish()**

- Makes frame visible to consumers

**wait_and_peek()**

- Blocks until unread frame exists

**release()**

- Marks frame as consumed by this consumer ID

---

### Python API (`nanobroker`)

**VideoBroker(topic, is_producer, consumer_id)**  
- `topic`: Shared memory name  
- `is_producer`: Bool  
- `consumer_id`: Unique integer 0–15  

**get_next_frame()**

- Returns `(producer_id, frame_id, numpy_array)`  
- Or `None`

**release_frame()**

- Moves read pointer forward  

**publish_frame(frame_id, width, height, numpy_array)**  
- Returns True on success

---

## 9. Administrative Tool

Check SHM status:

```
nanoadmin stats
```

Kick a dead consumer:

```
nanoadmin kick <ID>
```

Delete shared memory:

```
nanoadmin clean
```

---

## 10. Configuration & Protocol Limits

NanoBroker relies on **compile-time definitions** for maximum
performance.\
To change the resolution, buffer size, or data format, update the
protocol header:

**File: `include/nanobroker/video_protocol.hpp`**

``` cpp
namespace Protocol {
    const int MAX_WIDTH = 1920;  // Change to 3840 for 4K
    const int MAX_HEIGHT = 1080;
    const size_t BUFFER_SIZE = 30; // Increase for higher latency tolerance
    // ...
}
```

**Note:** After changing this file, recompile both the C++ projects
and the Python bindings:

    pip install .       # Rebuilds Python module with updated sizes
    cd build && make    # Rebuilds C++ tools
    nanoadmin clean     # Deletes old shared memory file

------------------------------------------------------------------------

## 11. Troubleshooting

### **"Bus Error" (SIGBUS)**

**Cause:** The C++ Producer and Python Consumer were compiled with
different `BUFFER_SIZE` or struct layouts.\
The Consumer is reading memory that no longer matches the Producer's
file.

**Fix:** 1. Run `nanoadmin clean` (or `rm /dev/shm/video_stream`) 2.
Recompile both C++ and Python modules 3. Restart the Producer *first*

------------------------------------------------------------------------

### **Producer Freezes / "Buffer Full"**

**Cause:**\
A Consumer crashed without deregistering, or is processing frames too
slowly.

**Fix:**

    nanoadmin stats     # Identify lagging consumer ID
    nanoadmin kick <ID> # Force-disconnect the slow or dead consumer

Note: NanoBroker includes an **auto-kick timeout (default 10s)** that
clears dead consumers automatically.

------------------------------------------------------------------------

### **Python Consumer Reads "Frame 0" Repeatedly**

**Cause:**\
Producer was restarted (creating a new SHM file) while the old
Consumer is still pointed at the deleted one.

**Fix:** Restart the Consumer.


