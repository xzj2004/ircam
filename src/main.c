//**代码作者：搞硬件的辛工，开源地址：https://github.com/xzj2004/ircam 
//**项目合作微信：mcugogogo，如果您有任何创意或想法或者项目落地需求都可以和我联系 
//**本项目开源协议MIT，允许自由修改发布及商用 
//**本项目仅用于学习交流，请勿用于非法用途 

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "driver/i2c.h"      // 用于I2C通信
#include "MLX90640_API.h"    // MLX90640 API库
#include "esp_spiffs.h"      // 添加SPIFFS头文件
#include "esp_netif_ip_addr.h"
#include "esp_netif.h"
#include "lwip/dns.h"
#include "dhcpserver/dhcpserver.h"
#include "esp_mac.h" // 添加MAC地址相关的头文件
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/ip4_addr.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define THERMAL_DATA_SIZE 768  // 32x24
#define JSON_BUFFER_SIZE 8192  // 足够大的缓冲区来存储JSON数据

#define EXAMPLE_ESP_WIFI_PASS      ""
#define EXAMPLE_MAX_STA_CONN       4

static const char *TAG = "thermal_cam"; // 更新TAG名称
static EventGroupHandle_t s_wifi_event_group;

// HTML页面 - 分段存储以避免编译器限制
static const char* html_start = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>MLX90640 热成像相机</title><style>";
static const char* html_style = "body{font-family:Arial,sans-serif;margin:0;padding:0;background-color:#000;height:100vh;overflow:hidden}"
    ".container{width:100vw;height:100vh;margin:0;padding:0;background-color:#000;display:flex;flex-direction:column;position:relative}"
    "h1{color:#fff;text-align:center;margin:5px 0;font-size:1.2em;display:flex;align-items:center;justify-content:center}"
    ".fps-display{color:#4CAF50;margin-left:15px;font-size:0.9em}"
    "#thermalCanvas{width:100%;height:calc(100vh - 120px);margin:0 auto;display:block;background-color:#000}"
    ".controls{text-align:center;margin:5px 0;background:rgba(0,0,0,0.7);padding:5px}"
    ".controls button,.controls select{padding:5px 10px;margin:0 3px;font-size:12px;cursor:pointer;background-color:#4CAF50;color:white;border:none;border-radius:3px}"
    ".controls button:hover,.controls select:hover{background-color:#45a049}"
    "#tempRange{margin:5px 0;text-align:center;color:#fff;font-size:0.9em;background:rgba(0,0,0,0.7);padding:5px 0}"
    "#advancedControls{position:absolute;top:10px;left:10px;background:rgba(0,0,0,0.8);padding:10px;border-radius:5px;z-index:1000;width:300px}"
    "#advancedControls label{display:block;margin:5px 0;color:#fff}"
    "#advancedControls input[type='number']{width:60px;padding:2px 5px;margin-right:5px}"
    "#advancedControls input[type='range']{width:100px;margin-right:5px}"
    "#advancedControls button{margin:2px 5px;padding:3px 8px;background-color:#4CAF50;color:white;border:none;border-radius:3px;cursor:pointer}"
    "#advancedControls button:hover{background-color:#45a049}";
static const char* html_head_end = "</style></head><body><div class=\"container\">";
static const char* html_body = "<div id=\"advancedControls\" style=\"display:none;\">"
    "<div style=\"margin:5px 0;\">"
    "<label>高斯模糊: <input type=\"range\" id=\"blurRadius\" min=\"0\" max=\"5\" value=\"0\" step=\"1\"><span id=\"blurValue\">0</span></label>"
    "<label>温度范围平滑: <input type=\"checkbox\" id=\"tempSmooth\"></label>"
    "<label>最小温度: <input type=\"number\" id=\"minTempManual\" value=\"15\" step=\"1\"></label>"
    "<label>最大温度: <input type=\"number\" id=\"maxTempManual\" value=\"35\" step=\"1\"></label>"
    "<button id=\"applyManualRangeButton\">应用温度范围</button>"
    "<button id=\"resetAutoRangeButton\">自动温度范围</button>"
    "<label>发射率: <input type=\"number\" id=\"emissivity\" value=\"0.95\" min=\"0.1\" max=\"1.0\" step=\"0.01\"></label>"
    "<button id=\"updateEmissivityButton\">更新发射率</button>"
    "</div>"
    "</div>"
    "<h1>MLX90640 热成像相机<span class=\"fps-display\">(<span id=\"frameRate\">0</span> FPS)</span></h1>"
    "<canvas id=\"thermalCanvas\"></canvas>"
    "<div class=\"controls\">"
    "<select id=\"colormap\">"
    "<option value=\"jet\">彩虹图</option>"
    "<option value=\"hot\">热力图</option>"
    "<option value=\"inferno\">地狱火</option>"
    "<option value=\"turbo\">涡轮图</option>"
    "</select>"
    "<select id=\"interpolation\">"
    "<option value=\"bilinear\">双线性插值</option>"
    "<option value=\"nearest\">最近邻插值</option>"
    "</select>"
    "<select id=\"resolution\">"
    "<option value=\"fullscreen\">全屏</option>"
    "<option value=\"1\">32×24 (原始)</option>"
    "<option value=\"3\">96×72 (3倍)</option>"
    "<option value=\"5\">160×120 (5倍)</option>"
    "<option value=\"7\">224×168 (7倍)</option>"
    "<option value=\"9\">288×216 (9倍)</option>"
    "<option value=\"12\">384×288 (12倍)</option>"
    "</select>"
    "<button id=\"startButton\">开始采集</button>"
    "<button id=\"stopButton\">停止采集</button>"
    "<button id=\"toggleAdvancedButton\">高级设置</button>"
    "</div>"
    "<div id=\"tempRange\">"
    "<span>最低温度: <span id=\"minTemp\">--</span>&#176;C</span>"
    "<span style=\"margin: 0 20px;\">最高温度: <span id=\"maxTemp\">--</span>&#176;C</span>"
    "</div>";

// Updated html_script to link to the external JS file
static const char* html_script = "<script src=\"/web_script.js\"></script></body></html>";

// I2C 配置
#define I2C_MASTER_SCL_IO    GPIO_NUM_9        /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO    GPIO_NUM_10        /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM       I2C_NUM_0         /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ   400000            /*!< I2C master clock frequency (400kHz) */
#define I2C_MASTER_TX_BUF_DISABLE 0            /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0            /*!< I2C master doesn't need buffer */

// MLX90640 配置
const uint8_t MLX90640_I2C_ADDR = 0x33; // MLX90640 默认I2C地址
#define TA_SHIFT 8 // 来自库示例的环境温度偏移
static paramsMLX90640 mlx90640_params;
static float mlx90640_temperatures[THERMAL_DATA_SIZE];
static uint16_t eeMLX90640[832]; // EEPROM数据缓冲区
static bool mlx90640_initialized = false;

// 添加新的数据缓冲区和任务控制变量
static uint16_t mlx90640Frame[834]; // 静态以避免栈溢出
static SemaphoreHandle_t frameMutex = NULL;
static TaskHandle_t frameCaptureTaskHandle = NULL;
static bool taskRunning = false;
static float emissivity = 0.95;

// MLX90640 I2C驱动函数 (ESP-IDF实现)
void MLX90640_I2CInit() {
    // ESP-IDF的I2C初始化已在 i2c_master_init() 中完成
}

void MLX90640_I2CFreqSet(int freq) {
    // ESP-IDF的I2C频率已在 i2c_master_init() 中设置
    // 此函数可以为空，或者如果需要动态更改频率，则在此实现
    ESP_LOGI(TAG, "MLX90640_I2CFreqSet called with %d kHz (ignored, set in i2c_master_init)", freq);
}

int MLX90640_I2CRead(uint8_t slaveAddr, unsigned int startAddress, unsigned int nWordsRead, uint16_t *data) {
    if (slaveAddr != MLX90640_I2C_ADDR) {
        ESP_LOGE(TAG, "I2CRead: Incorrect slave address: 0x%X", slaveAddr);
        return -1; // 或者其他错误码
    }
    
    uint8_t write_buf[2] = {(uint8_t)(startAddress >> 8), (uint8_t)(startAddress & 0xFF)};
    uint8_t* read_buf = (uint8_t*)data;
    unsigned int nBytesRead = nWordsRead * 2;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (slaveAddr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, write_buf, 2, true);
    i2c_master_start(cmd); // 重复启动
    i2c_master_write_byte(cmd, (slaveAddr << 1) | I2C_MASTER_READ, true);
    if (nBytesRead > 1) {
        i2c_master_read(cmd, read_buf, nBytesRead - 1, I2C_MASTER_ACK);
    }
    if (nBytesRead > 0) {
       i2c_master_read_byte(cmd, read_buf + nBytesRead - 1, I2C_MASTER_NACK);
    }
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2CRead failed: %s. StartAddr: 0x%04X, nWords: %d", esp_err_to_name(ret), startAddress, nWordsRead);
        return -1;
    }

    // 将小端字节序数据转换为大端字数据 (MLX90640数据手册通常指明字数据，高字节先传)
    // SparkFun库的读取方式是 MSB first, LSB second for each word.
    // ESP-IDF i2c_master_read 直接按字节读取到缓冲区，所以需要转换。
    for (unsigned int i = 0; i < nWordsRead; ++i) {
        uint8_t msb = read_buf[2 * i];
        uint8_t lsb = read_buf[2 * i + 1];
        data[i] = (uint16_t)msb << 8 | lsb;
    }
    
    return 0; // 成功
}

int MLX90640_I2CWrite(uint8_t slaveAddr, unsigned int writeAddress, uint16_t data_val) {
     if (slaveAddr != MLX90640_I2C_ADDR) {
        ESP_LOGE(TAG, "I2CWrite: Incorrect slave address: 0x%X", slaveAddr);
        return -1;
    }
    uint8_t write_buf[4];
    write_buf[0] = (uint8_t)(writeAddress >> 8);   // MSB of address
    write_buf[1] = (uint8_t)(writeAddress & 0xFF); // LSB of address
    write_buf[2] = (uint8_t)(data_val >> 8);        // MSB of data
    write_buf[3] = (uint8_t)(data_val & 0xFF);      // LSB of data

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (slaveAddr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, write_buf, 4, true);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2CWrite failed: %s. Addr: 0x%04X, Data: 0x%04X", esp_err_to_name(ret), writeAddress, data_val);
        return -1;
    }
    vTaskDelay(pdMS_TO_TICKS(1)); // 根据MLX90640数据手册，写操作后可能需要短暂延时
    return 0; // 成功
}


// I2C 主机初始化
static esp_err_t i2c_master_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

// MLX90640 传感器初始化
static esp_err_t mlx90640_sensor_init(void) {
    int status;
    status = MLX90640_DumpEE(MLX90640_I2C_ADDR, eeMLX90640);
    if (status != 0) {
        ESP_LOGE(TAG, "Failed to load EEPROM (status = %d)", status);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "EEPROM loaded successfully.");

    status = MLX90640_ExtractParameters(eeMLX90640, &mlx90640_params);
    if (status != 0) {
        ESP_LOGE(TAG, "Failed to extract parameters (status = %d)", status);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Parameters extracted successfully.");

    // 提高刷新率到8Hz (0x04) 或 16Hz (0x05)
    status = MLX90640_SetRefreshRate(MLX90640_I2C_ADDR, 0x05); // 16Hz
    if (status != 0) {
        ESP_LOGE(TAG, "Failed to set refresh rate (status = %d)", status);
        // 继续，即使刷新率设置失败
    } else {
        ESP_LOGI(TAG, "Refresh rate set to 16Hz.");
    }
    
    // 设置为棋盘模式 (Chess mode)
    status = MLX90640_SetChessMode(MLX90640_I2C_ADDR);
    if (status != 0) {
        ESP_LOGE(TAG, "Failed to set chess mode (status = %d)", status);
    } else {
        ESP_LOGI(TAG, "Chess mode set.");
    }

    // 创建互斥锁保护数据缓冲区
    frameMutex = xSemaphoreCreateMutex();
    if (frameMutex == NULL) {
        ESP_LOGE(TAG, "Failed to create frame mutex");
        return ESP_FAIL;
    }

    mlx90640_initialized = true;
    return ESP_OK;
}

// 新增：持续捕获帧的任务
static void frame_capture_task(void *pvParameters) {
    float tr; // 反射温度
    
    while (taskRunning) {
        // 获取帧数据
        int status = MLX90640_GetFrameData(MLX90640_I2C_ADDR, mlx90640Frame);
        if (status < 0) {
            ESP_LOGE(TAG, "GetFrameData failed with status: %d", status);
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        float ta = MLX90640_GetTa(mlx90640Frame, &mlx90640_params);
        tr = ta - TA_SHIFT; // 根据环境温度计算反射温度

        // 获取互斥锁并更新温度数据
        if (xSemaphoreTake(frameMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            MLX90640_CalculateTo(mlx90640Frame, &mlx90640_params, emissivity, tr, mlx90640_temperatures);
            xSemaphoreGive(frameMutex);
        }

        // 不需要两帧间等待太久，传感器本身会按照配置的刷新率工作
        vTaskDelay(pdMS_TO_TICKS(10)); // 短暂延迟以避免任务占用过多CPU
    }
    
    vTaskDelete(NULL);
}

// 启动和停止帧捕获任务
static void start_frame_capture_task() {
    if (!taskRunning && mlx90640_initialized) {
        taskRunning = true;
        xTaskCreate(frame_capture_task, "frame_capture", 4096, NULL, 5, &frameCaptureTaskHandle);
        ESP_LOGI(TAG, "Frame capture task started");
    }
}

static void stop_frame_capture_task() {
    if (taskRunning) {
        taskRunning = false;
        // 等待任务自行退出
        vTaskDelay(pdMS_TO_TICKS(100));
        ESP_LOGI(TAG, "Frame capture task stopped");
    }
}

// WiFi事件处理函数
static void event_handler(void* arg, esp_event_base_t event_base,
                         int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        // 重试连接WiFi (如果需要)
        // esp_wifi_connect(); 
        // ESP_LOGI(TAG, "retry to connect to the AP");
        ESP_LOGI(TAG, "WiFi disconnected.");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        char mac_str[18];
        snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
                 event->mac[0], event->mac[1], event->mac[2],
                 event->mac[3], event->mac[4], event->mac[5]);
        ESP_LOGI(TAG, "Station %s joined, AID=%d", mac_str, event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        char mac_str[18];
        snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
                 event->mac[0], event->mac[1], event->mac[2],
                 event->mac[3], event->mac[4], event->mac[5]);
        ESP_LOGI(TAG, "Station %s left, AID=%d", mac_str, event->aid);
    }
}

// DNS服务器任务
static void dns_server_task(void *pvParameters)
{
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char rx_buffer[128];
    
    // 创建UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Failed to create socket");
        vTaskDelete(NULL);
        return;
    }
    
    // 配置服务器地址
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(53);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    
    // 绑定socket
    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind socket");
        close(sock);
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "DNS Server started");
    
    while(1) {
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer), 0,
                          (struct sockaddr *)&client_addr, &client_addr_len);
                          
        if (len > 0) {
            // 简单的DNS响应，将所有查询都指向AP的IP地址
            uint8_t dns_reply[] = {
                0x00, 0x00, // Transaction ID (将被替换)
                0x81, 0x80, // Flags
                0x00, 0x01, // Questions
                0x00, 0x01, // Answer RRs
                0x00, 0x00, // Authority RRs
                0x00, 0x00, // Additional RRs
                // Query
                0x05, 'i', 'r', 'c', 'a', 'm',
                0x03, 'c', 'o', 'm',
                0x00, // Root
                0x00, 0x01, // Type A
                0x00, 0x01, // Class IN
                // Answer
                0xc0, 0x0c, // Pointer to domain name
                0x00, 0x01, // Type A
                0x00, 0x01, // Class IN
                0x00, 0x00, 0x00, 0x3c, // TTL (60 seconds)
                0x00, 0x04, // Data length
                192, 168, 4, 1 // IP address (192.168.4.1)
            };
            
            // 复制请求ID到响应
            dns_reply[0] = rx_buffer[0];
            dns_reply[1] = rx_buffer[1];
            
            sendto(sock, dns_reply, sizeof(dns_reply), 0,
                   (struct sockaddr *)&client_addr, sizeof(client_addr));
        }
    }
}

// WiFi初始化函数 - AP模式
void wifi_init_softap(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));

    // 获取MAC地址
    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_AP, mac));
    
    // 生成SSID
    char ssid[32];
    snprintf(ssid, sizeof(ssid), "IRCAM-%02X%02X", mac[4], mac[5]);

    wifi_config_t wifi_config = {
        .ap = {
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_OPEN,
            .channel = 1,
        },
    };
    
    // 复制SSID到配置结构
    strlcpy((char*)wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid));
    wifi_config.ap.ssid_len = strlen(ssid);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(ap_netif, &ip_info);
    ESP_LOGI(TAG, "Set up softAP with IP: " IPSTR, IP2STR(&ip_info.ip));

    // 启动DNS服务器任务
    xTaskCreate(dns_server_task, "dns_server", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             ssid, EXAMPLE_ESP_WIFI_PASS, wifi_config.ap.channel);
}

// HTTP GET处理函数 - 主页
static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    
    esp_err_t res = ESP_OK;
    
    if ((res = httpd_resp_send_chunk(req, html_start, strlen(html_start))) != ESP_OK) return res;
    if ((res = httpd_resp_send_chunk(req, html_style, strlen(html_style))) != ESP_OK) return res;
    if ((res = httpd_resp_send_chunk(req, html_head_end, strlen(html_head_end))) != ESP_OK) return res;
    if ((res = httpd_resp_send_chunk(req, html_body, strlen(html_body))) != ESP_OK) return res;
    if ((res = httpd_resp_send_chunk(req, html_script, strlen(html_script))) != ESP_OK) return res;
    
    return httpd_resp_send_chunk(req, NULL, 0);
}

// HTTP GET处理函数 - 热成像数据
static esp_err_t thermal_data_get_handler(httpd_req_t *req)
{
    if (!mlx90640_initialized) {
        ESP_LOGE(TAG, "MLX90640 not initialized yet.");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // 确保帧捕获任务正在运行
    if (!taskRunning) {
        start_frame_capture_task();
    }

    // 分配JSON缓冲区 - 使用静态缓冲区以避免频繁内存分配
    static char json_buffer[JSON_BUFFER_SIZE];
    
    // 从最新的温度数据生成JSON
    if (xSemaphoreTake(frameMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        int current_pos = 0;
        
        // 使用更快的方式构建JSON
        json_buffer[current_pos++] = '[';
        
        for(int i = 0; i < THERMAL_DATA_SIZE; i++) {
            // 使用1位小数来减少数据大小，同时保持足够的精度
            int written = snprintf(json_buffer + current_pos, JSON_BUFFER_SIZE - current_pos,
                                    "%s%.1f", i == 0 ? "" : ",", mlx90640_temperatures[i]);
                                    
            if (written < 0 || written >= JSON_BUFFER_SIZE - current_pos) {
                ESP_LOGE(TAG, "JSON buffer overflow");
                xSemaphoreGive(frameMutex);
                httpd_resp_send_500(req);
                return ESP_FAIL;
            }
            current_pos += written;
        }
        
        json_buffer[current_pos++] = ']';
        json_buffer[current_pos] = '\0';
        
        xSemaphoreGive(frameMutex);
    } else {
        // 无法获取互斥锁
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    // 设置缓存控制以优化性能
    httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json_buffer, strlen(json_buffer));
}

// HTTP GET处理函数 - 更新发射率
static esp_err_t update_emissivity_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;
    char param[32];
    float new_emissivity = 0.95;

    // 获取查询字符串长度
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            // 提取value参数
            if (httpd_query_key_value(buf, "value", param, sizeof(param)) == ESP_OK) {
                new_emissivity = atof(param);
                // 检查合法范围
                if (new_emissivity < 0.1) new_emissivity = 0.1;
                if (new_emissivity > 1.0) new_emissivity = 1.0;
                
                // 获取互斥锁并更新发射率
                if (xSemaphoreTake(frameMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    emissivity = new_emissivity;
                    xSemaphoreGive(frameMutex);
                    ESP_LOGI(TAG, "Emissivity updated to %.2f", emissivity);
                }
            }
        }
        free(buf);
    }

    httpd_resp_set_type(req, "text/plain");
    char resp_str[32];
    snprintf(resp_str, sizeof(resp_str), "Emissivity: %.2f", emissivity);
    httpd_resp_send(req, resp_str, strlen(resp_str));
    
    return ESP_OK;
}

// 初始化SPIFFS
static esp_err_t init_spiffs(void)
{
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    return ESP_OK;
}

// 修改JavaScript文件处理函数，从SPIFFS读取文件
static esp_err_t script_js_get_handler(httpd_req_t *req)
{
    char filepath[32];
    FILE *fd = NULL;
    struct stat file_stat;
    esp_err_t ret = ESP_OK;
    
    // 构建完整的文件路径
    sprintf(filepath, "/spiffs/web_script.js");
    ESP_LOGI(TAG, "Attempting to serve JS file from path: %s", filepath);
    
    // 获取文件信息
    if (stat(filepath, &file_stat) != 0) {
        ESP_LOGE(TAG, "Failed to get file status : %s", filepath);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "File not found");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "File stats - size: %ld", file_stat.st_size);
    
    fd = fopen(filepath, "r");
    if (!fd) {
        ESP_LOGE(TAG, "Failed to read file : %s", filepath);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read file");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Successfully opened file");
    
    // 设置Content-Type
    httpd_resp_set_type(req, "application/javascript");
    
    // 分配缓冲区
    char *chunk = malloc(1024);
    if (chunk == NULL) {
        fclose(fd);
        ESP_LOGE(TAG, "Failed to allocate memory for chunk");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to allocate memory");
        return ESP_FAIL;
    }
    
    // 读取并发送文件内容
    size_t chunksize;
    size_t total_sent = 0;
    do {
        chunksize = fread(chunk, 1, 1024, fd);
        if (chunksize > 0) {
            if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
                ret = ESP_FAIL;
                ESP_LOGE(TAG, "File sending failed!");
                break;
            }
            total_sent += chunksize;
            ESP_LOGI(TAG, "Sent %d bytes, total: %d", chunksize, total_sent);
        }
    } while (chunksize != 0);
    
    // 清理
    fclose(fd);
    free(chunk);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Successfully sent entire file (%d bytes)", total_sent);
        // 发送空块表示响应结束
        httpd_resp_send_chunk(req, NULL, 0);
    } else {
        httpd_resp_sendstr_chunk(req, NULL);
    }
    
    return ret;
}

// HTTP服务器配置
static const httpd_uri_t root = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t thermal_data_uri = {
    .uri       = "/thermal-data",
    .method    = HTTP_GET,
    .handler   = thermal_data_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t emissivity_uri = {
    .uri       = "/update-emissivity",
    .method    = HTTP_GET,
    .handler   = update_emissivity_handler,
    .user_ctx  = NULL
};

// Add the new URI for script.js
static const httpd_uri_t script_js_uri = {
    .uri       = "/web_script.js",
    .method    = HTTP_GET,
    .handler   = script_js_get_handler,
    .user_ctx  = NULL
};

// 启动Web服务器
static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.lru_purge_enable = true;
    config.max_open_sockets = 7;
    config.recv_wait_timeout = 5;
    config.send_wait_timeout = 5;

    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &thermal_data_uri);
        httpd_register_uri_handler(server, &emissivity_uri);
        httpd_register_uri_handler(server, &script_js_uri); // Register the new JS file handler
        
        start_frame_capture_task();
        return server;
    }

    ESP_LOGE(TAG, "Error starting server!");
    return NULL;
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 初始化SPIFFS
    ESP_ERROR_CHECK(init_spiffs());

    // 初始化I2C
    ESP_LOGI(TAG, "Initializing I2C master...");
    if (i2c_master_init() != ESP_OK) {
        ESP_LOGE(TAG, "I2C master initialization failed.");
        return;
    }
    ESP_LOGI(TAG, "I2C master initialized successfully.");

    // 初始化MLX90640传感器
    ESP_LOGI(TAG, "Initializing MLX90640 sensor...");
    if (mlx90640_sensor_init() != ESP_OK) {
        ESP_LOGE(TAG, "MLX90640 sensor initialization failed.");
    } else {
        ESP_LOGI(TAG, "MLX90640 sensor initialized successfully.");
    }

    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    wifi_init_softap();
    
    // 启动Web服务器
    start_webserver();
    
    // 注册关机回调以清理资源
    esp_register_shutdown_handler(stop_frame_capture_task);
}