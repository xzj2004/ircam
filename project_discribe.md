## 项目简介
这是一款DIY超便携WiFi热成像相机，使用MLX09640尺寸小巧方便携带且支持充电，外壳卡扣设计，无需螺丝或胶水，可在手机或电脑网页端实时查看图像，支持放大插值、色彩映射和温度测量，允许以及鼓励自由修改以及商用。

本项目对焊接技术要求较高

成品购买，[点我](https://h5.m.taobao.com/awp/core/detail.htm?ft=t&id=979512393076)

## 项目功能
我最初设计它的目的，是为了在 PCB 硬件调试中快速定位发热异常的元器件。相比昂贵的专业热像仪，这个 DIY 方案更轻便、低成本，元件购买方便，同时也便于随时使用。

## 项目参数

* 本设计采用MLX09640红外摄像头，32×24像素，I2C接口通信,兼容3.3V/5V电平；
* 主控采用esp32-c3，支持wifi蓝牙，相比于esp32系列成本更低
* 主控软件开发部分使用的是platformio的esp-idf环境
* esp32c3工作在ap模式，内置网页和dns解析服务。

## 原理解析

本项目由以下部分组成，降压及电源选择部分、主控部分、充电部分、i2c通信部分、下载部分，本项目主要是通过红外传感器接收红外数据，esp23传输到用户用户网页端进行解析显示图像，预置了多种调节参数，大家可自行尝试体验。



## 软件代码

github源码[点我跳转-ircam](https://github.com/xzj2004/ircam)
* 主程序在src\main.c
* 网页js在data\web_script.js
* MLX90640驱动在lib\MLX90640\MLX90640_API.cpp
* extra_script.py为烧录js文件到flash的脚本

## 注意事项

* 热成像的热点名称为：IRCAM-XXXX（xxxx为随机的四个字母数字组合）无密码
![image.png](https://image.lceda.cn/oshwhub/pullImage/9961d66ac7664ac69ef608bce960c187.png)
* 浏览器访问的地址为ircam.com或192.168.1.4，**必须**先连上wifi再访问[点我跳转-热成像网页端](http://ircam.com/)
* esp32c3芯片要买带内置flash的，4M就够用了
* MLXX90640有俩种型号选择大家可以根据需求自行选择，这里我用的是BAA

![image.png](https://image.lceda.cn/oshwhub/pullImage/a358fea5b5444f71b4c80e7a92234953.png)

## 烧录流程
配置好platformio环境之后（具体教程可在b站搜索“platformio安装教程”）
打开项目工程-选择好端口-点击窗口下方的的右箭头烧录
![image.png](https://image.lceda.cn/oshwhub/pullImage/df4bbec4ab7c47149c353613a28e89dd.png)
烧录完成后程序自动运行，连接上名为“IRCam”的WiFi（密码12345678）浏览器访问ircam.com即可查看热成像画面

![image.png](https://image.lceda.cn/oshwhub/pullImage/72c2dc0a371a4e4eab8fe9fdb64c4259.png)
有很多参数和模式可以调整达到自己想要的效果

## 焊接提示
板子焊好先进行软件烧录测试，热成像头可以先不焊，成功后再焊接

热成像头不建议紧贴板子焊，容易造成短路
建议如下焊接，浮空一点距离
![image.png](https://image.lceda.cn/oshwhub/pullImage/97c4cc602cde48e8b6b01b70b6b7015c.png)
锡只要完全覆盖了这些孔洞，就会出现无法开机、不显示图像等问题
![image.png](https://image.lceda.cn/oshwhub/pullImage/898e26cc3c0c4563aa89b7aeb7a315a2.png)

## Q&A 问题排查
Q:插上usb电脑提示无法识别的设备/usb插上无反应，
A:多半是type-c的d+d-没有焊好，检查一下

## 实物图
图1：实物图
![DSC006516.jpg](https://image.lceda.cn/oshwhub/pullImage/9b3be6d651d54d32af320eeaf3ad4744.jpg)


图2：网页界面图

![8b2583fe32d14b74280cc6c6f9c1355d.png](https://image.lceda.cn/oshwhub/pullImage/32bec04df57c46a599047a2f93cd0021.png)

## 复刻交流群


![0e5c7aacb9aed2c1007df990c71438b8.jpg](https://image.lceda.cn/oshwhub/pullImage/8d5c86fbf55348efb326146db78ffa28.jpg)
