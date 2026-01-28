# MLX90640 热成像相机项目

[English Version](README.md)

![实物图](IMG/physical_device.jpg)

## 项目简介

这是一款DIY超便携WiFi热成像相机，使用MLX90640尺寸小巧方便携带且支持充电，外壳卡扣设计，无需螺丝或胶水，可在手机或电脑网页端实时查看图像，支持放大插值、色彩映射和温度测量，允许以及鼓励自由修改以及商用。



成品购买：[点我](https://h5.m.taobao.com/awp/core/detail.htm?ft=t&id=979512393076)

**项目作者：** SuperJun  
**开源地址：** https://github.com/xzj2004/ircam  
**项目合作微信：** mcugogogo  
**开源协议：** MIT (允许自由修改发布及商用)

## 项目功能

我最初设计它的目的，是为了在 PCB 硬件调试中快速定位发热异常的元器件。相比昂贵的专业热像仪，这个 DIY 方案更轻便、低成本，元件购买方便，同时也便于随时使用。

## 项目参数

* 本设计采用MLX90640红外摄像头，32×24像素，I2C接口通信，兼容3.3V/5V电平
* 主控采用ESP32-C3，支持WiFi蓝牙，相比于ESP32系列成本更低
* 主控软件开发部分使用的是PlatformIO的ESP-IDF环境
* ESP32-C3工作在AP模式，内置网页和DNS解析服务

## 核心功能

### 1. 热成像数据采集
- **传感器：** MLX90640 32×24像素红外热成像传感器
- **分辨率：** 支持多种插值分辨率 (32×24, 96×72, 160×120, 224×168, 288×216, 384×288)
- **采样频率：** 可调节的数据采集频率
- **温度范围：** 支持自动和手动温度范围设置

### 2. 实时图像处理
- **插值算法：** 双线性插值和最近邻插值
- **色彩映射：** 支持彩虹图、热力图、地狱火、涡轮图等多种配色方案
- **图像增强：** 高斯模糊、温度范围平滑等高级处理功能
- **热点检测：** 自动识别并标记最高温度位置

### 3. Web界面控制
- **响应式设计：** 支持全屏显示和多种分辨率
- **实时控制：** 开始/停止采集、参数调节
- **高级设置：** 发射率调节、温度范围手动设置
- **FPS显示：** 实时帧率监控

### 4. 网络功能
- **WiFi热点：** 自动创建"IRCAM-XXXX"热点网络（XXXX为随机4位字母或数字）
- **Web服务器：** 内置HTTP服务器提供控制界面
- **多客户端支持：** 最多支持4个客户端同时连接

## 原理解析

本项目由以下部分组成：降压及电源选择部分、主控部分、充电部分、I2C通信部分、下载部分。本项目主要是通过红外传感器接收红外数据，ESP32传输到用户网页端进行解析显示图像，预置了多种调节参数，大家可自行尝试体验。

## 实物图

### 图1：实物图
![实物图](IMG/physical_device.jpg)

### 图2：网页界面图
![网页界面图](IMG/web_interface.png)

## 安卓APP

除了网页端，我还开发了适用于安卓设备的专属APP，提供更流畅的使用体验和更多功能。

**APP说明和下载地址：** [点击跳转](http://yunqian.xyz/products/thermal-imaging/)

![安卓APP界面](IMG/android_app.png)

## 项目结构

```
platformio-code-Project/
├── src/                          # 主要源代码
│   ├── main.c                   # 主程序文件 (779行)
│   └── CMakeLists.txt          # CMake构建配置
├── lib/                         # 项目库文件
│   ├── MLX90640/               # MLX90640传感器驱动库
│   │   ├── MLX90640_API.h      # API接口头文件
│   │   ├── MLX90640_API.cpp    # API实现 (1184行)
│   │   └── MLX90640_I2C_Driver.h # I2C驱动接口
│   └── README                   # 库说明文档
├── data/                        # Web资源文件
│   └── web_script.js           # 前端JavaScript代码 (469行)
├── platformio.ini              # PlatformIO项目配置
├── partitions.csv              # ESP32分区表配置
└── extra_script.py             # 自定义构建脚本
```

## 软件代码

GitHub源码：[点我跳转-ircam](https://github.com/xzj2004/ircam)
* 主程序在 `src/main.c`
* 网页JS在 `data/web_script.js`
* MLX90640驱动在 `lib/MLX90640/MLX90640_API.cpp`
* `extra_script.py` 为烧录JS文件到flash的脚本

## 核心代码位置说明

### 1. 主程序逻辑 - `src/main.c`

#### 热成像传感器初始化和配置
```c
// 第120-130行：I2C配置
#define I2C_MASTER_SCL_IO    GPIO_NUM_5        // I2C时钟引脚
#define I2C_MASTER_SDA_IO    GPIO_NUM_4        // I2C数据引脚
#define I2C_MASTER_FREQ_HZ   400000            // I2C频率400kHz

// 第132-135行：MLX90640配置
const uint8_t MLX90640_I2C_ADDR = 0x33;       // 传感器I2C地址
static paramsMLX90640 mlx90640_params;         // 传感器参数
static float mlx90640_temperatures[768];       // 温度数据缓冲区
```

#### I2C通信驱动实现
```c
// 第150-200行：MLX90640_I2CRead函数
// 实现从MLX90640读取数据的I2C通信

// 第200-250行：MLX90640_I2CWrite函数  
// 实现向MLX90640写入数据的I2C通信
```

#### 数据采集任务
```c
// 第300-400行：frameCaptureTask函数
// 负责持续采集热成像数据的FreeRTOS任务
```

#### Web服务器和HTTP处理
```c
// 第500-600行：HTTP请求处理函数
// 包括数据获取、参数设置等API接口
```

#### HTML页面生成
```c
// 第50-110行：HTML页面模板
// 包含完整的Web界面HTML代码
```

### 2. 前端JavaScript - `data/web_script.js`

#### 画布渲染和图像处理
```javascript
// 第1-50行：画布初始化和基本设置
const canvas = document.getElementById('thermalCanvas');
const ctx = canvas.getContext('2d');

// 第50-100行：色彩映射和插值算法
const colormaps = {
    hot: [[0,0,0],[0.3,0,0],[0.6,0.3,0],...],
    jet: [[0,0,0.5],[0,0,1],[0,1,1],...],
    // ... 其他配色方案
};
```

#### 实时数据获取和显示
```javascript
// 第200-300行：数据获取和渲染循环
// 实现与后端的数据通信和实时显示更新
```

#### 用户交互控制
```javascript
// 第300-400行：用户界面控制逻辑
// 包括分辨率切换、色彩方案选择、参数调节等
```

### 3. MLX90640驱动库 - `lib/MLX90640/`

#### API接口定义
```cpp
// MLX90640_API.h - 第1-73行
// 定义传感器操作的所有API函数接口
```

#### 核心算法实现
```cpp
// MLX90640_API.cpp - 第1-1184行
// 包含温度计算、校准、数据处理等核心算法
```

## 硬件连接

| ESP32-C3引脚 | MLX90640引脚 | 功能说明 |
|-------------|-------------|----------|
| GPIO5      | SCL         | I2C时钟线 |
| GPIO4      | SDA         | I2C数据线 |
| 3.3V       | VDD         | 电源正极 |
| GND        | VSS         | 电源地线 |

## 注意事项

* 热成像的热点名称为：IRCAM-XXXX（XXXX为随机的四个字母数字组合）无密码
![WiFi连接](IMG/wifi_connection.png)
* 浏览器访问的地址为 ircam.com 或 192.168.1.4，**必须**先连上WiFi再访问 [点我跳转-热成像网页端](http://ircam.com/)
* ESP32-C3芯片要买带内置flash的，4M就够用了
* MLX90640有两种型号选择，大家可以根据需求自行选择，这里我用的是BAA

![MLX90640型号](IMG/mlx90640_models.png)

## 烧录流程

配置好PlatformIO环境之后（具体教程可在B站搜索"platformio安装教程"）：
打开项目工程 - 选择好端口 - 点击窗口下方的右箭头烧录

![烧录流程](IMG/flashing_process.png)

烧录完成后程序自动运行，连接上名为"IRCAM-XXXX"的WiFi（无需密码），浏览器访问 ircam.com 即可查看热成像画面

![设置界面](IMG/settings_interface.png)

有很多参数和模式可以调整达到自己想要的效果

## 焊接提示

板子焊好先进行软件烧录测试，热成像头可以先不焊，成功后再焊接

热成像头不建议紧贴板子焊，容易造成短路
建议如下焊接，浮空一点距离：

![焊接提示](IMG/soldering_tips.png)

锡只要完全覆盖了这些孔洞，就会和GND短路出现无法开机、不显示图像等问题：

![焊接注意](IMG/solder_warning.png)

## Q&A 问题排查

**Q:** 插上USB电脑提示无法识别的设备/USB插上无反应  
**A:** 多半是Type-C的D+/D-没有焊好，检查一下

## 使用方法

### 1. 编译和烧录
```bash
# 使用PlatformIO编译
pio run

# 烧录到ESP32-C3
pio run --target upload
```

### 2. 连接和配置
1. 将MLX90640传感器连接到ESP32-C3的指定引脚
2. 上电后ESP32-C3会自动创建"IRCAM-XXXX" WiFi热点（XXXX为随机4位字母或数字）
3. 连接WiFi热点（无需密码）
4. 在浏览器中访问 ircam.com 或 192.168.1.4

### 3. 功能操作
- **开始采集：** 点击"开始采集"按钮启动热成像
- **分辨率调节：** 选择不同的插值分辨率
- **色彩方案：** 选择不同的温度色彩映射
- **高级设置：** 调节发射率、温度范围等参数

## 技术特点

- **实时性能：** 优化的数据采集和渲染算法
- **多分辨率支持：** 从原始32×24到384×288多种分辨率
- **高级图像处理：** 支持多种插值算法和图像增强
- **响应式Web界面：** 支持各种设备屏幕尺寸
- **开源协议：** MIT协议，支持商业使用

## 其他注意事项

- 本项目仅用于学习交流，请勿用于非法用途
- 使用前请确保MLX90640传感器正确连接
- 建议在通风良好的环境中使用，避免传感器过热
- 温度测量精度受环境因素影响，建议定期校准

## 复刻交流群

![复刻交流群](IMG/community_group.jpg)

## 贡献和合作

欢迎提交Issue和Pull Request来改进项目。如果您有创意想法或项目落地需求，欢迎通过微信 `mcugogogo` 联系作者进行合作。

## 许可证

本项目采用MIT开源协议，详情请参考LICENSE文件。
