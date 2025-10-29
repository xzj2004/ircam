# MLX90640 Thermal Imaging Camera Project

## Project Introduction

This is a smart thermal imaging camera project based on ESP32-C3 and MLX90640 infrared thermal imaging sensor. The project is developed using the ESP-IDF framework, providing a Web interface through WiFi hotspot to achieve real-time thermal imaging display, temperature measurement, and image processing functions.

**Project Author:** Hardware Engineer Xin  
**Open Source Address:** https://github.com/xzj2004/ircam  
**Project Collaboration WeChat:** mcugogogo  
**Open Source License:** MIT (Free modification, distribution, and commercial use allowed)

## Core Features

### 1. Thermal Imaging Data Collection
- **Sensor:** MLX90640 32×24 pixel infrared thermal imaging sensor
- **Resolution:** Supports multiple interpolation resolutions (32×24, 96×72, 160×120, 224×168, 288×216, 384×288)
- **Sampling Frequency:** Adjustable data acquisition frequency
- **Temperature Range:** Supports automatic and manual temperature range settings

### 2. Real-time Image Processing
- **Interpolation Algorithms:** Bilinear interpolation and nearest neighbor interpolation
- **Color Mapping:** Supports rainbow, heat map, hellfire, turbo, and other color schemes
- **Image Enhancement:** Advanced processing features like Gaussian blur, temperature range smoothing
- **Hotspot Detection:** Automatically identifies and marks the highest temperature location

### 3. Web Interface Control
- **Responsive Design:** Supports full-screen display and multiple resolutions
- **Real-time Control:** Start/stop acquisition, parameter adjustment
- **Advanced Settings:** Emissivity adjustment, manual temperature range setting
- **FPS Display:** Real-time frame rate monitoring

### 4. Network Functions
- **WiFi Hotspot:** Automatically creates "IRCam" hotspot network
- **Web Server:** Built-in HTTP server provides control interface
- **Multi-client Support:** Supports up to 4 clients connected simultaneously

## Project Structure

```
platformio-code-Project/
├── src/                          # Main source code
│   ├── main.c                   # Main program file (779 lines)
│   └── CMakeLists.txt          # CMake build configuration
├── lib/                         # Project libraries
│   ├── MLX90640/               # MLX90640 sensor driver library
│   │   ├── MLX90640_API.h      # API interface header file
│   │   ├── MLX90640_API.cpp    # API implementation (1184 lines)
│   │   └── MLX90640_I2C_Driver.h # I2C driver interface
│   └── README                   # Library documentation
├── data/                        # Web resources
│   └── web_script.js           # Frontend JavaScript code (469 lines)
├── platformio.ini              # PlatformIO project configuration
├── partitions.csv              # ESP32 partition table configuration
└── extra_script.py             # Custom build script
```

## Core Code Location Description

### 1. Main Program Logic - `src/main.c`

#### Thermal Imaging Sensor Initialization and Configuration
```c
// Lines 120-130: I2C configuration
#define I2C_MASTER_SCL_IO    GPIO_NUM_5        // I2C clock pin
#define I2C_MASTER_SDA_IO    GPIO_NUM_4        // I2C data pin
#define I2C_MASTER_FREQ_HZ   400000            // I2C frequency 400kHz

// Lines 132-135: MLX90640 configuration
const uint8_t MLX90640_I2C_ADDR = 0x33;       // Sensor I2C address
static paramsMLX90640 mlx90640_params;         // Sensor parameters
static float mlx90640_temperatures[768];       // Temperature data buffer
```

#### I2C Communication Driver Implementation
```c
// Lines 150-200: MLX90640_I2CRead function
// Implements I2C communication to read data from MLX90640

// Lines 200-250: MLX90640_I2CWrite function  
// Implements I2C communication to write data to MLX90640
```

#### Data Acquisition Task
```c
// Lines 300-400: frameCaptureTask function
// FreeRTOS task responsible for continuously collecting thermal imaging data
```

#### Web Server and HTTP Handling
```c
// Lines 500-600: HTTP request handling functions
// Includes API interfaces for data acquisition, parameter settings, etc.
```

#### HTML Page Generation
```c
// Lines 50-110: HTML page template
// Contains complete Web interface HTML code
```

### 2. Frontend JavaScript - `data/web_script.js`

#### Canvas Rendering and Image Processing
```javascript
// Lines 1-50: Canvas initialization and basic settings
const canvas = document.getElementById('thermalCanvas');
const ctx = canvas.getContext('2d');

// Lines 50-100: Color mapping and interpolation algorithms
const colormaps = {
    hot: [[0,0,0],[0.3,0,0],[0.6,0.3,0],...],
    jet: [[0,0,0.5],[0,0,1],[0,1,1],...],
    // ... other color schemes
};
```

#### Real-time Data Acquisition and Display
```javascript
// Lines 200-300: Data acquisition and rendering loop
// Implements data communication with backend and real-time display updates
```

#### User Interaction Control
```javascript
// Lines 300-400: User interface control logic
// Includes resolution switching, color scheme selection, parameter adjustment, etc.
```

### 3. MLX90640 Driver Library - `lib/MLX90640/`

#### API Interface Definition
```cpp
// MLX90640_API.h - Lines 1-73
// Defines all API function interfaces for sensor operations
```

#### Core Algorithm Implementation
```cpp
// MLX90640_API.cpp - Lines 1-1184
// Contains core algorithms for temperature calculation, calibration, data processing, etc.
```

## Hardware Connection

| ESP32-C3 Pin | MLX90640 Pin | Function Description |
|-------------|-------------|----------|
| GPIO5      | SCL         | I2C clock line |
| GPIO4      | SDA         | I2C data line |
| 3.3V       | VDD         | Power positive |
| GND        | VSS         | Power ground |

## Usage Instructions

### 1. Compilation and Flashing
```bash
# Compile using PlatformIO
pio run

# Flash to ESP32-C3
pio run --target upload
```

### 2. Connection and Configuration
1. Connect the MLX90640 sensor to the specified pins on ESP32-C3
2. After powering on, ESP32-C3 will automatically create an "IRCam" WiFi hotspot
3. Connect to the WiFi hotspot (password: 12345678)
4. Access the device IP address in a browser

### 3. Functional Operation
- **Start Acquisition:** Click the "Start Acquisition" button to start thermal imaging
- **Resolution Adjustment:** Select different interpolation resolutions
- **Color Scheme:** Choose different temperature color mappings
- **Advanced Settings:** Adjust emissivity, temperature range, and other parameters

## Technical Features

- **Real-time Performance:** Optimized data acquisition and rendering algorithms
- **Multi-resolution Support:** Multiple resolutions from original 32×24 to 384×288
- **Advanced Image Processing:** Supports various interpolation algorithms and image enhancements
- **Responsive Web Interface:** Supports various device screen sizes
- **Open Source License:** MIT license, supports commercial use

## Notes

- This project is for learning and exchange purposes only, please do not use for illegal purposes
- Please ensure the MLX90640 sensor is correctly connected before use
- It is recommended to use in a well-ventilated environment to avoid sensor overheating
- Temperature measurement accuracy is affected by environmental factors, regular calibration is recommended

## Contribution and Collaboration

Welcome to submit Issues and Pull Requests to improve the project. If you have creative ideas or project implementation needs, please contact the author through WeChat `mcugogogo` for collaboration.

## License

This project is licensed under the MIT open source license, please refer to the LICENSE file for details.
