#include <string.h>
#include <stdio.h>
#include <math.h>
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

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define THERMAL_DATA_SIZE 768  // 32x24
#define JSON_BUFFER_SIZE 8192  // 足够大的缓冲区来存储JSON数据

static const char *TAG = "thermal_cam"; // 更新TAG名称
static EventGroupHandle_t s_wifi_event_group;

// HTML页面 - 分段存储以避免编译器限制
static const char* html_start = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>MLX90640 热成像相机</title><style>";
static const char* html_style = "body{font-family:Arial,sans-serif;margin:0;padding:0;background-color:#000;height:100vh;overflow:hidden}"
    ".container{width:100vw;height:100vh;margin:0;padding:0;background-color:#000;display:flex;flex-direction:column}"
    "h1{color:#fff;text-align:center;margin:5px 0;font-size:1.2em;display:flex;align-items:center;justify-content:center}"
    ".fps-display{color:#4CAF50;margin-left:15px;font-size:0.9em}"
    "#thermalCanvas{width:100%;height:calc(100vh - 120px);margin:0 auto;display:block;background-color:#000}"
    ".controls{text-align:center;margin:5px 0;background:rgba(0,0,0,0.7);padding:5px}"
    ".controls button,.controls select{padding:5px 10px;margin:0 3px;font-size:12px;cursor:pointer;background-color:#4CAF50;color:white;border:none;border-radius:3px}"
    ".controls button:hover,.controls select:hover{background-color:#45a049}"
    "#tempRange{margin:5px 0;text-align:center;color:#fff;font-size:0.9em;background:rgba(0,0,0,0.7);padding:5px 0}";
static const char* html_head_end = "</style></head><body><div class=\"container\">";
static const char* html_body = "<h1>MLX90640 热成像相机<span class=\"fps-display\">(<span id=\"frameRate\">0</span> FPS)</span></h1>"
    "<canvas id=\"thermalCanvas\"></canvas>"
    "<div class=\"controls\">"
    "<select id=\"colormap\" onchange=\"updateColormap()\">"
    "<option value=\"jet\">彩虹图</option>"
    "<option value=\"hot\">热力图</option>"
    "<option value=\"inferno\">地狱火</option>"
    "<option value=\"turbo\">涡轮图</option>"
    "</select>"
    "<select id=\"interpolation\" onchange=\"updateColormap()\">"
    "<option value=\"bilinear\">双线性插值</option>"
    "<option value=\"nearest\">最近邻插值</option>"
    "</select>"
    "<select id=\"resolution\" onchange=\"changeResolution()\">"
    "<option value=\"fullscreen\">全屏</option>"
    "<option value=\"1\">32×24 (原始)</option>"
    "<option value=\"3\">96×72 (3倍)</option>"
    "<option value=\"5\">160×120 (5倍)</option>"
    "<option value=\"7\">224×168 (7倍)</option>"
    "<option value=\"9\">288×216 (9倍)</option>"
    "<option value=\"12\">384×288 (12倍)</option>"
    "</select>"
    "<button onclick=\"startStream()\">开始采集</button>"
    "<button onclick=\"stopStream()\">停止采集</button>"
    "</div>"
    "<div id=\"tempRange\">"
    "<span>最低温度: <span id=\"minTemp\">--</span>&#176;C</span>"
    "<span style=\"margin: 0 20px;\">最高温度: <span id=\"maxTemp\">--</span>&#176;C</span>"
    "</div>";
static const char* html_script = "<script>"
    "const canvas=document.getElementById('thermalCanvas');"
    "canvas.style.imageRendering='pixelated';"  // 添加像素化渲染
    "const ctx=canvas.getContext('2d');"
    "ctx.imageSmoothingEnabled=false;"  // 禁用平滑处理
    "const srcWidth=32,srcHeight=24;"
    "let dstWidth=96,dstHeight=72;"  // 默认3倍
    "let streaming=false,frameCount=0,lastTime=Date.now();"
    "const colormaps={"
    "hot:[[0,0,0],[0.3,0,0],[0.6,0.3,0],[0.9,0.6,0],[1,0.9,0],[1,1,0.3],[1,1,0.6],[1,1,1]],"
    "jet:[[0,0,0.5],[0,0,1],[0,1,1],[1,1,0],[1,0,0],[0.5,0,0]],"
    "inferno:[[0,0,0],[0.2,0.1,0.3],[0.4,0,0.4],[0.6,0,0.5],[0.8,0.3,0.3],[1,0.6,0],[1,0.9,0.2]],"
    "turbo:[[0.18995,0.07176,0.23217],[0.25107,0.37408,0.77204],[0.19802,0.72896,0.90123],[0.83327,0.86547,0.24252],[0.99942,0.62738,0.09517]]"
    "};"
    // 最近邻插值函数
    "function nearestNeighbor(temps,x,y){"
    "const ix=Math.floor(x);"
    "const iy=Math.floor(y);"
    "return temps[iy*srcWidth+ix];}"
    // 双线性插值函数
    "function bilinearInterpolate(temps,x,y){"
    "const x1=Math.floor(x);"
    "const y1=Math.floor(y);"
    "const x2=Math.min(x1+1,srcWidth-1);"
    "const y2=Math.min(y1+1,srcHeight-1);"
    "const fx=x-x1;"
    "const fy=y-y1;"
    "const a=temps[y1*srcWidth+x1];"
    "const b=temps[y1*srcWidth+x2];"
    "const c=temps[y2*srcWidth+x1];"
    "const d=temps[y2*srcWidth+x2];"
    "const top=a+(b-a)*fx;"
    "const bottom=c+(d-c)*fx;"
    "return top+(bottom-top)*fy;}"
    "function interpolateColor(colors,t){"
    "if(t<=0)return colors[0];"
    "if(t>=1)return colors[colors.length-1];"
    "const idx=t*(colors.length-1);"
    "const i=Math.floor(idx);"
    "const f=idx-i;"
    "if(!colors[i] || !colors[i+1]) return colors[0];"  // 添加安全检查
    "const c1=colors[i];"
    "const c2=colors[i+1];"
    "return["
    "c1[0]+(c2[0]-c1[0])*f,"
    "c1[1]+(c2[1]-c1[1])*f,"
    "c1[2]+(c2[2]-c1[2])*f"
    "]}"
    "function getColor(value,min,max){"
    "const ratio=Math.min(1, Math.max(0, (value-min)/(max-min)));"  // 确保ratio在0-1之间
    "const colormap=colormaps[document.getElementById('colormap').value] || colormaps.hot;"  // 添加默认颜色方案
    "const color=interpolateColor(colormap,ratio);"
    "const r=Math.min(255, Math.max(0, Math.round(color[0]*255)));"
    "const g=Math.min(255, Math.max(0, Math.round(color[1]*255)));"
    "const b=Math.min(255, Math.max(0, Math.round(color[2]*255)));"
    "return`rgb(${r},${g},${b})`"
    "}"
    "function updateColormap(){if(lastData)updateThermalImage(lastData);}"
    "function changeResolution(){"
    "const scaleOption=document.getElementById('resolution').value;"
    "const canvas=document.getElementById('thermalCanvas');"
    "if(scaleOption==='fullscreen'){"
    "const viewportWidth=window.innerWidth;"
    "const viewportHeight=window.innerHeight - 120;" // 减去控制栏和信息栏的高度
    "const aspectRatio=srcWidth/srcHeight;"
    "if(viewportWidth/viewportHeight > aspectRatio){"
    "dstHeight=Math.floor(viewportHeight);"
    "dstWidth=Math.floor(dstHeight*aspectRatio);"
    "}else{"
    "dstWidth=Math.floor(viewportWidth);"
    "dstHeight=Math.floor(dstWidth/aspectRatio);"
    "}"
    "canvas.style.width=dstWidth+'px';"
    "canvas.style.height=dstHeight+'px';"
    "}else{"
    "const scale=parseInt(scaleOption);"
    "dstWidth=srcWidth*scale;"
    "dstHeight=srcHeight*scale;"
    "canvas.style.width='auto';"
    "canvas.style.height='auto';"
    "}"
    "canvas.width=dstWidth;"
    "canvas.height=dstHeight;"
    "if(lastData)updateThermalImage(lastData);"
    "}"
    "window.addEventListener('resize', () => {"
    "if (document.getElementById('resolution').value === 'fullscreen') {"
    "changeResolution();"
    "}"
    "});"
    "document.addEventListener('DOMContentLoaded', () => {"
    "document.getElementById('resolution').value = 'fullscreen';" // 设置默认为全屏
    "document.getElementById('colormap').value = 'jet';" // 设置默认颜色方案为彩虹图
    "document.getElementById('interpolation').value = 'bilinear';" // 设置默认为双线性插值
    "changeResolution();"
    "});"
    "setInterval(()=>{"
    "const currentTime=Date.now();"
    "const fps=Math.round(frameCount/((currentTime-lastTime)/1000));"
    "document.getElementById('frameRate').textContent=fps;"
    "frameCount=0;lastTime=currentTime},1000);"
    "let lastData=null;"
    "function updateThermalImage(data){"
    "if(!data || !data.length) return;"  // 添加数据有效性检查
    "lastData=data;"
    "const temperatures=new Float32Array(data);"
    "const minTemp=Math.min(...temperatures);"
    "const maxTemp=Math.max(...temperatures);"
    "document.getElementById('minTemp').textContent=minTemp.toFixed(1);"
    "document.getElementById('maxTemp').textContent=maxTemp.toFixed(1);"
    "const imageData=ctx.createImageData(dstWidth,dstHeight);"
    "for(let y=0;y<dstHeight;y++){"
    "for(let x=0;x<dstWidth;x++){"
    "const srcX=(x/dstWidth)*(srcWidth-1);"
    "const srcY=(y/dstHeight)*(srcHeight-1);"
    "let temp;"
    "const interpolation=document.getElementById('interpolation').value;"
    "if(interpolation==='bilinear'){"
    "temp=bilinearInterpolate(temperatures,srcX,srcY);"
    "}else{"
    "temp=nearestNeighbor(temperatures,srcX,srcY);"
    "}"
    "const color=getColor(temp,minTemp,maxTemp);"
    "const rgb=color.match(/\\d+/g);"
    "if(!rgb || rgb.length<3) continue;"  // 添加颜色值检查
    "const idx=(y*dstWidth+x)*4;"
    "imageData.data[idx]=parseInt(rgb[0]);"
    "imageData.data[idx+1]=parseInt(rgb[1]);"
    "imageData.data[idx+2]=parseInt(rgb[2]);"
    "imageData.data[idx+3]=255}}"
    "ctx.putImageData(imageData,0,0);"
    "frameCount++}"
    "function startStream(){if(!streaming){streaming=true;fetchThermalData()}}"
    "function stopStream(){streaming=false}"
    "async function fetchThermalData(){while(streaming){"
    "try{const response=await fetch('/thermal-data');"
    "const data=await response.json();"
    "updateThermalImage(data);"
    "await new Promise(resolve=>setTimeout(resolve,50))}" // 降低延迟以提高帧率
    "catch(error){console.error('获取热成像数据错误:',error);streaming=false}}}"
    "</script></body></html>";

// I2C 配置
#define I2C_MASTER_SCL_IO    GPIO_NUM_5        /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO    GPIO_NUM_4        /*!< GPIO number used for I2C master data  */
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

    // 设置刷新率 (例如 4Hz: 0x03, 8Hz: 0x04)
    status = MLX90640_SetRefreshRate(MLX90640_I2C_ADDR, 0x03); // 4Hz
    if (status != 0) {
        ESP_LOGE(TAG, "Failed to set refresh rate (status = %d)", status);
        // 继续，即使刷新率设置失败
    } else {
        ESP_LOGI(TAG, "Refresh rate set to 4Hz.");
    }
    
    // 设置为棋盘模式 (Chess mode)
    status = MLX90640_SetChessMode(MLX90640_I2C_ADDR);
    if (status != 0) {
        ESP_LOGE(TAG, "Failed to set chess mode (status = %d)", status);
    } else {
        ESP_LOGI(TAG, "Chess mode set.");
    }

    mlx90640_initialized = true;
    return ESP_OK;
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
    }
}

// WiFi初始化函数
void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");
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

    static uint16_t mlx90640Frame[834]; // 静态以避免栈溢出
    float emissivity = 0.95;
    float tr; // 反射温度

    // 读取两个子页面以获取完整帧
    for (int subpage = 0; subpage < 2; subpage++) {
        int status = MLX90640_GetFrameData(MLX90640_I2C_ADDR, mlx90640Frame);
        if (status < 0) {
            ESP_LOGE(TAG, "GetFrameData failed with status: %d", status);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }

        //float vdd = MLX90640_GetVdd(mlx90640Frame, &mlx90640_params); // 可选
        float ta = MLX90640_GetTa(mlx90640Frame, &mlx90640_params);
        tr = ta - TA_SHIFT; // 根据环境温度计算反射温度

        MLX90640_CalculateTo(mlx90640Frame, &mlx90640_params, emissivity, tr, mlx90640_temperatures);
    }

    char* json_buffer = malloc(JSON_BUFFER_SIZE);
    if (json_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for JSON buffer");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    strcpy(json_buffer, "[");
    int current_pos = 1;
    for(int i = 0; i < THERMAL_DATA_SIZE; i++) {
        int written = snprintf(json_buffer + current_pos, JSON_BUFFER_SIZE - current_pos,
                                 "%s%.1f",
                                 i == 0 ? "" : ",", mlx90640_temperatures[i]);
        if (written < 0 || written >= JSON_BUFFER_SIZE - current_pos) {
            ESP_LOGE(TAG, "JSON buffer overflow");
            free(json_buffer);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        current_pos += written;
    }
    strcat(json_buffer, "]");
    
    httpd_resp_set_type(req, "application/json");
    esp_err_t res = httpd_resp_send(req, json_buffer, strlen(json_buffer));
    free(json_buffer);
    return res;
}

// HTTP服务器配置
static const httpd_uri_t root = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t thermal_data_uri = { // 变量名修改避免冲突
    .uri       = "/thermal-data",
    .method    = HTTP_GET,
    .handler   = thermal_data_get_handler,
    .user_ctx  = NULL
};

// 启动Web服务器
static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.lru_purge_enable = true; // 启用LRU清理

    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &thermal_data_uri); // 使用修改后的变量名
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

    // 初始化I2C
    ESP_LOGI(TAG, "Initializing I2C master...");
    if (i2c_master_init() != ESP_OK) {
        ESP_LOGE(TAG, "I2C master initialization failed.");
        return; // 或者进行错误处理
    }
    ESP_LOGI(TAG, "I2C master initialized successfully.");

    // 初始化MLX90640传感器
    ESP_LOGI(TAG, "Initializing MLX90640 sensor...");
    if (mlx90640_sensor_init() != ESP_OK) {
        ESP_LOGE(TAG, "MLX90640 sensor initialization failed.");
        // 即使传感器初始化失败，也可能希望继续运行WiFi和Web服务器以便调试
    } else {
        ESP_LOGI(TAG, "MLX90640 sensor initialized successfully.");
    }
    
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();
    
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to AP. Starting webserver...");
        start_webserver();
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to WiFi.");
    } else {
        ESP_LOGE(TAG, "UNEXPECTED WIFI EVENT");
    }
}