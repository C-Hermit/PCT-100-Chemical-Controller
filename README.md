# PCT-100 化工控制实训装置

> **智能环境控制器** — 基于 ESP32-C3，集成温度监测、光照传感、双通道继电器控制、MQTT 物联通信  
> 版本 **V1.5** | 适用于化工控制实训教学场景

<div align="center">

![MCU](https://img.shields.io/badge/MCU-ESP32--C3-green)
![IDE](https://img.shields.io/badge/IDE-Arduino_CLI-orange)
![Host](https://img.shields.io/badge/Host-Electron-blue)
![MQTT](https://img.shields.io/badge/Protocol-MQTT-purple)
![License](https://img.shields.io/badge/License-ISC-lightgrey)

</div>

---

## 目录

- [产品概述](#产品概述)
- [核心功能](#核心功能)
- [硬件规格](#硬件规格)
- [引脚定义](#引脚定义)
- [系统架构](#系统架构)
- [快速开始 — 固件](#快速开始--固件)
- [快速开始 — 上位机](#快速开始--上位机)
- [MQTT 通信协议](#mqtt-通信协议)
- [串口命令](#串口命令)
- [WS2812 状态指示](#ws2812-状态指示)
- [项目结构](#项目结构)

---

## 产品概述

PCT-100 是基于 **ESP32-C3** 微控制器设计的嵌入式实训装置，集成传感器采集、继电器控制、无线通信和人机交互于一体，可作为化工控制实训教学平台。

**核心硬件组成：**

| 组件 | 型号/规格 | 用途 |
|------|-----------|------|
| MCU | ESP32-C3 (RISC-V, 160MHz) | 主控，WiFi/BLE |
| 温度传感器 | DS18B20 | ±0.5°C 精度，-10~85°C |
| 光照传感器 | LDR 光敏电阻 | 模拟量采集 |
| 显示屏 | 0.96" OLED (128×64) | 状态显示，I2C 接口 |
| 指示灯 | WS2812 RGB LED | 6 种优先级状态动画 |
| 继电器 | 2 通道 | LED 灯（GPIO6）、风扇（GPIO7） |
| 按键 | KEY1 (锁死)、KEY2 (回弹) | 总闸开关、模式切换 |

---

## 核心功能

- **总闸控制**：KEY1 按下上电，松开断电，切断所有输出
- **双模式运行**：
  - **自动模式**：光照低于阈值 → LED 灯开启；温度高于阈值 → 风扇开启
  - **手动模式**：KEY2 循环切换 00→01→10→11 四种输出组合
- **传感器监测**：DS18B20 温度 + LDR 光照强度实时采集
- **MQTT 物联**：WiFi 联网，远程状态上报与命令控制
- **OLED 显示**：实时显示温度、光照、继电器状态、模式等
- **WS2812 指示**：多色彩灯动画，直观反映设备运行状态
- **串口调试**：115200 波特率命令行，支持 WiFi 配网、参数设置
- **上位机监控**：Electron 桌面应用，MQTT 远程监控与控制

---

## 硬件规格

| 参数 | 值 |
|------|-----|
| MCU | ESP32-C3，RISC-V 单核 160MHz |
| Flash | 4MB |
| WiFi | 802.11 b/g/n，2.4GHz |
| 工作电压 | 5V DC (USB) |
| 温度传感器 | DS18B20，精度 ±0.5°C，量程 -10°C ~ 85°C |
| 光照传感器 | LDR 模拟量，ADC 读取 |
| 显示屏 | 0.96" OLED 128×64，SSD1306，I2C |
| 指示灯 | WS2812 RGB LED |
| 继电器 | 2 路，5V 驱动，LED 灯 / 风扇 |
| 按键 | KEY1 锁死（GPIO20），KEY2 回弹（GPIO21） |
| 通信接口 | USB-UART (115200)、WiFi、MQTT |

---

## 引脚定义

| 引脚 | 外设 | 说明 |
|------|------|------|
| GPIO0 | WS2812 | NeoPixel 数据线 |
| GPIO1 | LDR | 光敏电阻模拟输入 (ADC) |
| GPIO4 | OLED SDA | I2C 数据线 |
| GPIO5 | OLED SCL | I2C 时钟线 |
| GPIO6 | 继电器 LED | 灯光继电器（高电平开启） |
| GPIO7 | 继电器 FAN | 风扇继电器（高电平开启） |
| GPIO10 | DS18B20 | 单总线数据线 |
| GPIO20 | KEY1 | 锁死按键，按下保持 = 总闸开启 |
| GPIO21 | KEY2 | 回弹按键，短按 = 手动循环，长按 = 模式切换 |

---

## 系统架构

### 四层单向依赖架构

```
┌──────────────────────────────────────────────┐
│            编排层 · Orchestration             │
│    src.ino (setup/loop)  scheduler  device    │
│              (全局状态 SystemState)            │
└──────────────────────┬───────────────────────┘
                       ▼
┌──────────────────────────────────────────────┐
│             业务层 · Business                 │
│  controller  mqtt_handler  ws2812_indicator   │
│    (领域逻辑、约束控制、状态同步、Side Effects)   │
└──────────────────────┬───────────────────────┘
                       ▼
┌──────────────────────────────────────────────┐
│             传输层 · Transport                │
│   wifi_drv  mqtt_client_drv  serial_cmd       │
│         (通信协议栈、网络连接管理)               │
└──────────────────────┬───────────────────────┘
                       ▼
┌──────────────────────────────────────────────┐
│             驱动层 · Drivers                  │
│  key  relay  ldr  ds18b20  oled_ui  ws2812    │
│           (硬件抽象，不依赖上层逻辑)             │
└──────────────────────────────────────────────┘
```

### 设计原则

- **无反向依赖**：驱动层头文件绝不包含业务层头文件
- **无 `delay()` 阻塞主循环**：基于 FreeRTOS 任务调度，多任务并行
- **ISR 只做一件事**：设置 `volatile` 标志位，不在中断中调用 I2C/SPI/Serial
- **状态变量单一写入者**：所有共享状态通过 `controller.cpp` 写入，不分散在各模块
- **副作用集中在业务层**：`ctrl_set_relay()` 同时更新硬件、同步 sys、唤醒 OLED、推送 MQTT

### 全局状态结构体

```cpp
struct SystemState {
    bool power_on;             // 总闸：true=开启
    bool is_auto;              // 模式：true=自动，false=手动
    uint8_t manual_code;       // 手动状态码 00~11 (bit1=LED, bit0=FAN)
    float current_temp;        // 当前温度
    unsigned int current_lux;  // 当前光照
    float temp_th;             // 温度阈值
    unsigned int light_th;     // 光照阈值
    bool wifi_connected;       // WiFi 连接状态
    bool mqtt_connected;       // MQTT 连接状态
    bool led_relay_on;         // LED 继电器状态
    bool fan_relay_on;         // 风扇继电器状态
};
```

### FreeRTOS 调度任务

| 任务 | 周期 | 栈大小 | 说明 |
|------|------|--------|------|
| `serial_cmd_loop` | 主循环 | (main) | 串口命令解析 |
| `wifi_drv_loop` | 主循环 | (main) | 异步连接 FSM + 自动重连 |
| `mqtt_client_loop` | 主循环 | (main) | MQTT 泵送 + 自动重连 |
| `key_scan` | 主循环 | (main) | 微秒级按键状态机 |
| `ctrl_key_logic` | 20ms | 2048 | 按键触发：总闸、模式、手动循环 |
| `ws2812_indicator_loop` | 20ms | 2048 | WS2812 优先级动画引擎 |
| `ctrl_auto_light` | 50ms | 2048 | LDR 采样 + 自动灯光控制 |
| `ctrl_auto_fan` | 2000ms | 4096 | DS18B20 采样 + 自动风扇控制 |
| `task_oled_refresh` | 3000ms | 4096 | OLED 显示刷新（由 scheduler 驱动） |

### 数据流

```
KEY1/KEY2 按键 → key_scan() → ctrl_key_logic()
  ├── 自动模式：LDR < 阈值 → LED 开；温度 > 阈值 → FAN 开
  └── 手动模式：KEY2 循环 00→01→10→11 (bit0=FAN, bit1=LED)

串口命令 → serial_cmd_loop() → ctrl_set_*()
MQTT 命令 → mqtt_client_loop() → mqtt_handler → ctrl_set_*()

状态变更 → controller 函数
  ├── sched_wake(oled_task) → OLED 立即刷新
  └── mqtt_handler_send_status() → MQTT 状态推送
```

---

## 快速开始 — 固件

### 环境准备

安装 [Arduino CLI](https://arduino.github.io/arduino-cli/) 并添加 ESP32 支持：

```bash
arduino-cli core update-index
arduino-cli core install esp32:esp32
```

### 安装依赖库

```bash
arduino-cli lib install "ArduinoJson"
arduino-cli lib install "U8g2"
arduino-cli lib install "PubSubClient"
arduino-cli lib install "DS18B20"
arduino-cli lib install "Adafruit NeoPixel"
```

### 编译与烧录

```bash
# 编译
arduino-cli compile --fqbn esp32:esp32:esp32c3 src/src.ino

# 烧录（请确认串口设备路径）
arduino-cli compile --fqbn esp32:esp32:esp32c3 --upload -p /dev/ttyACM0 src/src.ino

# 串口监视器
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

### 首次上电流程

1. 连接 USB 串口，打开串口监视器 (115200 baud)
2. 输入 `wifi scan` 扫描周围网络
3. 输入 `wifi list` 查看扫描结果
4. 输入 `wifi conn <编号> <密码>` 连接 WiFi
5. 输入 `mqtt set ip=<服务器地址>` 和 `mqtt set port=<端口>` 配置 MQTT
6. (可选) 输入 `mqtt set device_id=<ID>` 自定义设备 ID
7. 按下 KEY1 总闸，设备进入运行状态

---

## 快速开始 — 上位机

上位机是基于 Electron 的桌面监控应用，通过 MQTT 协议与 PCT-100 通信。

### 启动

```bash
cd host
npm install
npm start
```

### 打包为 Windows 便携版

```bash
npm run dist
```

输出到 `host/dist/` 目录。

### 使用说明

1. 点击 **齿轮按钮** 进入设置页面
2. 配置 MQTT 服务器地址、端口、设备 ID（默认 `PCT_100_013`）
3. 返回显示页面，点击 **连接** 按钮
4. 连接成功后实时显示传感器数据和设备状态
5. 通过 **胶囊开关** 控制继电器输出（需在手动模式下）
6. 在设置页面可调整温度/光照阈值

### 上位机架构

```
main.js (主进程)            preload.js (IPC 桥)          js/app.js (渲染进程)
┌──────────────────┐       ┌─────────────────┐         ┌──────────────────┐
│ MQTT client      │ ◄IPC─►│ contextBridge   │ ◄─DOM──►│ 页面交互与状态显示│
│ ipcMain handlers │       │ window.host API │         │ 命令发送与日志   │
│ 状态轮询 5s      │       │ connect/send/on │         │ 约束控制逻辑     │
└──────────────────┘       └─────────────────┘         └──────────────────┘
```

### 界面约束逻辑

上位机 UI 完整映射了固件的安全约束：

- **总闸断开** → 所有操作锁定（开关灰化）
- **自动模式** → 继电器开关灰化，显示"自动控制"提示
- **手动模式** → 继电器开关可操作
- **设备 12 秒无响应** → 标记离线，自动锁定

---

## MQTT 通信协议

### 连接参数

| 参数 | 说明 |
|------|------|
| Broker | 任意标准 MQTT Broker（EMQX、Mosquitto 等） |
| 端口 | 1883 (TCP) / 8081 (WebSocket) |
| 协议版本 | MQTT v3.1.1 |

### Topic

| Topic | 方向 | 说明 |
|-------|------|------|
| `chemctrl/{deviceId}/status` | 设备 → Broker | 设备状态上报 (JSON) |
| `chemctrl/{deviceId}/command` | Broker → 设备 | 远程控制命令 (JSON) |

> 默认 Device ID: `PCT_100_013`

### 状态上报 (JSON)

设备在任何状态变更时主动推送（事件驱动，非轮询）：

```json
{
  "temperature": 25.3,
  "light": 450,
  "mode": "auto",
  "key1_lock": true,
  "relay3": true,
  "relay4": false,
  "temp_th": 30.0,
  "light_th": 300
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `temperature` | float | 当前温度 (°C) |
| `light` | int | 当前光照 (Lux) |
| `mode` | string | `"auto"` / `"manual"` |
| `key1_lock` | bool | 总闸状态 |
| `relay3` | bool | LED 继电器 |
| `relay4` | bool | 风扇继电器 |
| `temp_th` | float | 温度阈值 (°C) |
| `light_th` | int | 光照阈值 (Lux) |

### 远程命令

| 命令 | JSON | 说明 |
|------|------|------|
| 设置继电器 | `{"cmd":"set_relay","relay":3,"value":true}` | 仅在手动模式下有效 |
| 切换模式 | `{"cmd":"set_mode","mode":"manual"}` | `"auto"` / `"manual"` |
| 设定阈值 | `{"cmd":"set_threshold","temp":30.5,"light":200}` | 可单独设温度或光照 |
| 查询状态 | `{"cmd":"get_status"}` | 触发一次状态上报 |
| 重启设备 | `{"cmd":"reboot"}` | 远程重启 ESP32-C3 |

### 安全约束

- **总闸关闭** (`key1_lock=false`) → 拒绝所有命令，MCU 串口输出拒绝日志
- **自动模式** (`mode=auto`) → 拒绝 `set_relay`，仅允许切换模式和设阈值
- **手动模式** (`mode=manual`) → 接受 `set_relay`，relay 3=LED, relay 4=FAN

---

## 串口命令

通过串口 (115200 baud) 使用命令行操作设备。

### WiFi 命令

| 命令 | 说明 |
|------|------|
| `wifi scan` | 异步扫描周围 WiFi 网络 |
| `wifi list` | 按 RSSI 信号强度显示扫描结果 |
| `wifi conn <n> [pwd]` | 连接编号为 n 的网络 |
| `wifi status` | 显示 WiFi 连接状态、IP、信号强度 |
| `wifi forget` | 清除 NVS 中保存的 WiFi 凭据 |

### MQTT 命令

| 命令 | 说明 |
|------|------|
| `mqtt status` | 显示 MQTT 服务器、端口、连接状态 |
| `mqtt set <key>=<value>` | 设置参数并保存到 Flash（自动重连） |
| `mqtt reconnect` | 强制重连 MQTT |

可配置键: `ip`, `port`, `user`, `pass`, `device_id`

### 系统命令

| 命令 | 说明 |
|------|------|
| `set temp=<value>` | 设置温度阈值 |
| `set light=<value>` | 设置光照阈值 |
| `set mode=auto\|manual` | 切换工作模式 |
| `get status` | 显示全部系统参数 |
| `help` | 显示命令帮助 |

---

## WS2812 状态指示

WS2812 RGB LED 根据系统状态自动切换动画，优先级从高到低：

| 优先级 | 触发条件 | 动画模式 |
|--------|----------|----------|
| **1** | LED + 风扇均开启 | 严重警报：R100→灭50→R100→灭50→G100→灭50→G100→灭50→B100→灭50→B100→灭350 (循环) |
| **2** | 仅 LED 开启 | 光照越界：R300→灭200→G300→灭200→B300→灭200 (循环) |
| **3** | 仅风扇开启 | 温度越界：R500→G500→B500→灭700 (循环) |
| **4** | MQTT 断连 (WiFi 正常) | 红快照→渐绿500ms→渐灭500ms→渐蓝500ms→渐灭500ms (循环) |
| **5** | WiFi 断开 | R200ms→G200ms→灭500ms (循环) |
| **6** | 一切正常 | 灭→渐红1s→渐绿1s→渐蓝1s→渐灭1s→灭200ms (循环) |

引擎运行周期 20ms，支持 RGB 插值渐变过渡。

---

## 项目结构

```
PCT_100/
├── src/                          # 下位机固件 (Arduino C++)
│   ├── src.ino                   # 主程序：setup/loop，全局 sys 实例
│   ├── device.h                  # SystemState 结构体定义
│   ├── version.h                 # 版本号 V1.5、设备 ID
│   ├── scheduler.h / .cpp        # FreeRTOS 任务调度器 (最大 8 任务)
│   ├── controller.h / .cpp       # 业务层：控制逻辑、约束、配置管理
│   ├── mqtt_handler.h / .cpp     # MQTT 回调解析 + JSON 状态上报
│   ├── ws2812_indicator.h / .cpp # WS2812 优先级动画引擎
│   ├── serial_cmd.h / .cpp       # 串口命令行解释器
│   ├── wifi_drv.h / .cpp         # 传输层：WiFi 异步连接驱动
│   ├── mqtt_client_drv.h / .cpp  # 传输层：MQTT 客户端驱动
│   ├── key.h / .cpp              # 驱动层：按键状态机 (KEY1/KEY2)
│   ├── relay.h / .cpp            # 驱动层：继电器控制 (GPIO6/7)
│   ├── ldr.h / .cpp              # 驱动层：光敏电阻采集
│   ├── ds18b20_drv.h / .cpp      # 驱动层：DS18B20 温度传感器
│   ├── oled_ui.h / .cpp          # 驱动层：OLED 128×64 显示
│   └── ws2812_drv.h / .cpp       # 驱动层：WS2812 底层驱动
├── host/                         # 上位机桌面应用 (Electron)
│   ├── main.js                   # Electron 主进程：MQTT 客户端、IPC 处理
│   ├── preload.js                # contextBridge：安全暴露 window.host API
│   ├── index.html                # 监控界面 UI（双页面：显示 + 设置）
│   ├── js/app.js                 # 渲染进程：事件绑定、状态更新、约束控制
│   ├── css/style.css             # 界面样式（暗色主题、胶囊开关、动画）
│   └── package.json              # npm 依赖与构建配置
├── docs/                         # 产品说明书网站 (GitHub Pages)
│   ├── index.html                # 产品说明书 (总览/架构/引脚/MQTT/指示)
│   ├── specs.html                # 技术规格 (MCU/传感器/电气/时序)
│   ├── usage.html                # 应用场景 (快速开始/命令/MQTT/开发)
│   ├── css/style.css
│   └── js/scroll-spy.js
├── CLAUDE.md                     # Claude Code 项目指引
└── README.md
```

---

## 构建状态

| 组件 | 状态 |
|------|------|
| 固件 (ESP32-C3) | ✅ V1.5 完成 |
| 上位机 (Electron) | ✅ 完成 |
| 产品说明书 (GitHub Pages) | ✅ 已发布 |

---

## License

ISC License
