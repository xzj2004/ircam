DIY 一款超便携的 WiFi 热成像相机

一、项目背景
在进行硬件电路调试时，我们经常会遇到元器件异常发热的情况，例如芯片过载、稳压器短路或是虚焊导致的大电流等。快速定位这些发热点，是诊断问题的关键。然而，专业的红外热像仪价格昂贵，对于广大电子爱好者和独立开发者来说是一笔不小的开销。
有没有一种方案，既能满足基本的发热点定位需求，又足够轻便、成本低廉，并且使用方便呢？正是出于这个目的，我设计了这款 DIY 超便携 WiFi 热成像相机。它的核心目标是打造一个通过手机或电脑浏览器就能实时查看热成像画面的工具，将硬件调试的门槛进一步降低，让每一位爱好者都能拥有自己的“火眼金睛”。
二、准备工作
本项目的设计目标是无线化和小型化，大部分元器件都选用了市面上常见的芯片，方便大家采购和制作。
表1 主要材料清单
| 序号 | 名称 | 规格/型号 | 数量 |
| :--- | :--- | :--- | :--- |
| 1 | 主控芯片 | ESP32-C3 模组 (内置4M Flash) | 1 |
| 2 | 红外传感器 | MLX90640 (BAA 型) | 1 |
| 3 | 锂电池 | 3.7V | 1 |
| 4 | 充电芯片 | MCP73831T | 1 |
| 5 | 稳压芯片 | ME6212C33 3.3V LDO | 1 |
| 6 | 外壳 | 3D 打印 | 1 |
软件与工具：
开发环境： Visual Studio Code + PlatformIO 插件 使用的esp-idf开发环境
3D 打印机： 用于制作外壳（可选）
电烙铁加热台等焊接工具
三、核心内容
1. 总体设计
本项目的系统框架非常清晰（如图1所示）。MLX90640 红外传感器负责采集热辐射数据，通过 I2C 总线将 32x24 像素的温度矩阵发送给 ESP32-C3 主控。主控接收到数据后，利用其内置的 WiFi 功能建立一个无线接入点（AP），并启动一个微型网页服务器。用户设备（如手机、电脑）连接上这个 WiFi 后，通过浏览器访问特定网址，主控就会将实时温度数据流式传输到网页端。最后，由网页中的 JavaScript 程序将这些数据动态渲染成五彩斑斓的热成像图像。
code
Mermaid
graph TD
    A[MLX90640 红外传感器] -- I2C 总线 --> B[ESP32-C3 主控];
    B -- WiFi AP & Web Server --> C[用户设备 (手机/电脑)];
    C -- 浏览器访问 --> B;
    B -- 温度数据流 --> C;
图1 系统设计框图
这种设计的最大优点是将复杂的图像处理和显示任务转移到了功能强大的浏览器上，极大地减轻了微控制器的负担，使得用一颗低成本的 MCU 实现流畅的画面刷新成为可能。
2. 硬件设计
主控选择： 我选择了 ESP32-C3 芯片。相比于经典的 ESP32 系列，它在成本上更具优势，同时保留了核心的 WiFi 功能，性能完全足够本项目使用。
传感器选择： 传感器是热像仪的灵魂。MLX90640 是一款高性价比的 32x24 像素红外阵列传感器，通过 I2C 接口即可读取每个像素点的温度，非常适合 DIY 项目。它有两个常见型号：BAA 型拥有 110°×75° 的广角视野，适合近距离观察电路板全貌；BAB 型则为 55°×35° 的窄视野，噪声更小，测温更准，适合远距离观察。本项目中我使用的是 BAA 型。
电路部分： 电路设计以简洁实用为原则。一块锂电池通过 MCP73831T 进行充电管理，再经过一颗 ME6212C33 3.3V 的低压差线性稳压器（LDO）为主控和传感器提供稳定纯净的电源。整体电路非常简单，即便是新手也能轻松焊接。
外壳设计： 为了实现“超便携”，我设计了一款小巧的 3D 打印外壳，同时预留了钥匙扣孔。外壳采用了卡扣式结构，无需任何螺丝或胶水即可完成组装，既美观又方便拆卸维护。
3. 软件设计
软件是本项目的核心与亮点，主要分为固件程序和前端网页两部分。
固件程序 (Firmware)：
程序基于 PlatformIO 的 ESP-IDF 环境开发。核心功能是在 main.c 中实现的。
WiFi AP 模式与 DNS 服务： 程序启动后，ESP32-C3 会创建一个名为 “IRCam”、密码为 “12345678” 的 WiFi 热点。同时，它还内置了 DNS 解析服务，会将 ircam.com 这个域名指向自己，这样用户就无需记忆繁琐的 IP 地址，使用起来非常方便。
数据采集与传输： 在主循环中，程序通过 I2C 协议不断地从 MLX90640 读取最新的温度数据矩阵，并通过 WebServer 将这些数据实时推送给已连接的浏览器客户端。
前端网页 (Web Interface)：
前端界面由 HTML、CSS 和 JavaScript 构成，存储在 ESP32-C3 的 Flash 文件系统中。
web_script.js 是前端的逻辑核心，它负责接收从主控发来的原始温度数据，然后通过 HTML5 Canvas 技术将这 768 个温度点“绘制”成图像。为了让图像更清晰易读，我还加入了多种功能，例如：
色彩映射： 提供“彩虹”等多种伪彩色方案，将不同温度映射为不同颜色。
双线性插值： 通过算法对低分辨率的图像进行平滑处理，使其看起来更“高清”。
动态调节： 用户可以在网页上实时调整温度的显示范围、是否启用高斯模糊等参数，以达到最佳的观察效果。

4. 关键代码讲解
为了帮助大家更好地理解项目实现原理，这里详细解析几个核心代码片段。

4.1 主函数初始化流程
固件的入口函数 `app_main()` 展现了整个系统的初始化流程：

```c
void app_main(void)
{
    // 初始化NVS存储
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 初始化SPIFFS文件系统
    ESP_ERROR_CHECK(init_spiffs());

    // 初始化I2C总线
    ESP_LOGI(TAG, "Initializing I2C master...");
    if (i2c_master_init() != ESP_OK) {
        ESP_LOGE(TAG, "I2C master initialization failed.");
        return;
    }

    // 初始化MLX90640传感器
    ESP_LOGI(TAG, "Initializing MLX90640 sensor...");
    if (mlx90640_sensor_init() != ESP_OK) {
        ESP_LOGE(TAG, "MLX90640 sensor initialization failed.");
    } else {
        ESP_LOGI(TAG, "MLX90640 sensor initialized successfully.");
    }

    // 启动WiFi AP模式
    wifi_init_softap();
    
    // 启动Web服务器
    start_webserver();
}
```

这个初始化流程清晰地展现了系统启动的步骤：存储初始化 → 文件系统挂载 → I2C通信建立 → 传感器配置 → WiFi热点创建 → Web服务器启动。

4.2 WiFi热点配置
WiFi AP模式的配置是实现无线访问的关键：

```c
void wifi_init_softap(void)
{
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "IRCam",                    // WiFi名称
            .ssid_len = strlen("IRCam"),
            .password = "12345678",             // WiFi密码
            .max_connection = 4,                // 最大连接数
            .authmode = WIFI_AUTH_WPA_WPA2_PSK, // 加密方式
            .channel = 1,                       // 信道
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // 启动DNS服务器任务，实现域名解析
    xTaskCreate(dns_server_task, "dns_server", 4096, NULL, 5, NULL);
}
```

这段代码不仅创建了WiFi热点，还启动了DNS服务器，使用户可以通过 `ircam.com` 域名访问设备，而无需记忆IP地址。

4.3 温度数据采集任务
温度数据的持续采集是通过FreeRTOS任务实现的：

```c
static void frame_capture_task(void *pvParameters) {
    float tr; // 反射温度
    
    while (taskRunning) {
        // 从MLX90640获取原始帧数据
        int status = MLX90640_GetFrameData(MLX90640_I2C_ADDR, mlx90640Frame);
        if (status < 0) {
            ESP_LOGE(TAG, "GetFrameData failed with status: %d", status);
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // 获取环境温度并计算反射温度
        float ta = MLX90640_GetTa(mlx90640Frame, &mlx90640_params);
        tr = ta - TA_SHIFT;

        // 使用互斥锁保护数据，计算每个像素的实际温度
        if (xSemaphoreTake(frameMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            MLX90640_CalculateTo(mlx90640Frame, &mlx90640_params, emissivity, tr, mlx90640_temperatures);
            xSemaphoreGive(frameMutex);
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // 短暂延迟避免CPU过载
    }
    
    vTaskDelete(NULL);
}
```

这个任务在后台持续运行，将MLX90640的原始ADC数据转换为实际的温度值，并通过互斥锁确保数据访问的线程安全。

4.4 前端图像渲染核心
前端的图像处理是整个项目的亮点，核心函数 `updateThermalImage()` 实现了复杂的图像处理算法：

```javascript
function updateThermalImage(data) {
    if (!data || !data.length) return;
    lastData = data;
    const temperatures = new Float32Array(data);
    
    // 应用高斯模糊滤波
    const blurRadius = parseInt(document.getElementById('blurRadius').value);
    const processedTemps = blurRadius > 0 ? 
        gaussianBlur(temperatures, srcWidth, srcHeight, blurRadius) : temperatures;

    // 快速寻找温度范围和最热点位置
    let rawMinTemp = processedTemps[0];
    let rawMaxTemp = processedTemps[0];
    let maxTempIndex = 0;
    
    let i = 0;
    for (const temp of processedTemps) {
        if (temp < rawMinTemp) rawMinTemp = temp;
        if (temp > rawMaxTemp) {
            rawMaxTemp = temp;
            maxTempIndex = i;
        }
        i++;
    }

    // 动态或手动设置温度显示范围
    if (manualTempRange) {
        minTemp = manualMinTemp;
        maxTemp = manualMaxTemp;
    } else if (tempRangeSmoothed) {
        // 平滑的温度范围变化，避免画面跳跃
        minTemp = prevMinTemp * 0.8 + rawMinTemp * 0.2;
        maxTemp = prevMaxTemp * 0.2 + rawMaxTemp * 0.8;
        prevMinTemp = minTemp;
        prevMaxTemp = maxTemp;
    } else {
        minTemp = rawMinTemp;
        maxTemp = rawMaxTemp;
    }
}
```

4.5 双线性插值算法
为了让32×24的低分辨率图像看起来更清晰，项目实现了双线性插值算法：

```javascript
function bilinearInterpolate(temps, x, y) {
    const x1 = Math.floor(x);
    const y1 = Math.floor(y);
    const x2 = Math.min(x1 + 1, srcWidth - 1);
    const y2 = Math.min(y1 + 1, srcHeight - 1);
    const fx = x - x1;  // x方向的小数部分
    const fy = y - y1;  // y方向的小数部分
    
    // 获取四个相邻像素的温度值
    const a = temps[y1 * srcWidth + x1];  // 左上
    const b = temps[y1 * srcWidth + x2];  // 右上
    const c = temps[y2 * srcWidth + x1];  // 左下
    const d = temps[y2 * srcWidth + x2];  // 右下
    
    // 先在x方向插值
    const top = a + (b - a) * fx;
    const bottom = c + (d - c) * fx;
    
    // 再在y方向插值
    return top + (bottom - top) * fy;
}
```

这个算法通过计算目标像素周围四个源像素的加权平均值，实现了图像的平滑放大效果。

4.6 色彩映射与查找表优化
为了提高渲染性能，项目使用了预计算的色彩查找表：

```javascript
function generateColormapLUT(colormap) {
    const lut = new Uint32Array(lutSize);
    const colors = colormaps[colormap];
    const alphaChannel = 255 << 24;  // 设置不透明度
    
    for (let i = 0; i < lutSize; i++) {
        const t = i / (lutSize - 1);  // 归一化的温度比例
        const color = interpolateColor(colors, t);
        
        // 将RGB值打包成32位整数，提高内存访问效率
        const r = Math.min(255, Math.max(0, Math.round(color[0] * 255)));
        const g = Math.min(255, Math.max(0, Math.round(color[1] * 255)));
        const b = Math.min(255, Math.max(0, Math.round(color[2] * 255)));
        lut[i] = alphaChannel | (b << 16) | (g << 8) | r;
    }
    return lut;
}
```

通过预计算256个色彩值并存储在查找表中，避免了渲染时的重复计算，大幅提升了画面刷新率。

5. 烧录与运行
首先，使用 PlatformIO 环境编译并烧录固件。烧录成功后，程序会自动运行。此时，用手机搜索 WiFi，找到名为 “IRCam” 的网络并连接。然后打开手机浏览器，在地址栏输入 ircam.com 并访问，即可看到实时的热成像画面了（如图2所示）。
图2 网页界面图
四、收尾部分
1. 成品展示
经过一番努力，这台小巧的热成像相机终于诞生了（如图3所示）。它的大小仅有火柴盒一般，可以轻松放入口袋。无论是检查家中电器的工作状态，还是寻找电路板上的故障点，它都能派上用场。
图3 成品实物图
2. 优缺点总结
优点： 成本极低、体积小巧便携、无需安装任何 APP、开源且可自由修改。
缺点： 原始分辨率较低（32x24）、画面刷新率受 WiFi 传输限制、BAA 型号传感器的图像噪声相对明显。
3. 未来改进
这个项目还有很大的想象空间。未来可以尝试更换为噪声更小的 BAB 型传感器以提升精度，或者为其增加一个本地显示屏，使其可以脱离手机独立工作。甚至，还可以利用 ESP32-C3 的蓝牙功能，探索更多有趣的玩法。
4. 心得体会
从一个简单的想法，到最终捧在手心的成品，这个过程充满了探索的乐趣。这个项目不仅让我对红外成像技术有了更深的理解，也让我再次感受到了开源软硬件生态的强大。它证明了，只要我们善于利用现有的工具和知识，就能以很低的成本创造出实用而酷炫的工具。希望这个项目能给广大爱好者带来启发，一起享受创造的快乐！