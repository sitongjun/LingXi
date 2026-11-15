# 灵犀 LingXi：大模型驱动的居家安全管家

> An LLM-powered home-safety assistant: an ESP32-S3 voice terminal lets the LLM call home devices as MCP tools, while an STM32 controller executes them reliably and keeps local safety functions running offline.

嵌入式系统课程大作业。老人对着桌面语音终端说话，就能控制全屋设备、查询环境状态。系统的分工是：**大模型负责听懂和规划，STM32 负责可靠执行**；断网时，本地安全功能照常工作。

## 亮点

- **能力清单驱动的 MCP 工具注册**：ESP32 启动后向 STM32 发送 `GET_CAPS`，按返回的设备表动态注册 MCP 工具（`self.home.get_state`、`self.home.<设备>.set`）。STM32 新增设备时只改设备表，语音端固件不用改。
- **可靠的串口帧协议**：自定义 USART 帧，带 CRC16 校验、应答、超时重发，保证大模型下发的指令可靠落到硬件上。
- **本地安全兜底**：温湿度阈值可用语音设置；超阈值报警带回差，恢复正常后自动解除，断网也照常运行。
- **打通“语音 → 大模型 → MCP 工具 → 串口 → MCU 执行”闭环**：在实机上验证了“打开客厅灯”“关报警”“查询状态”“全部关灯”等指令。

## 系统组成

```
┌────────────── 普中 STM32F103ZET6（家居主控）──────────────┐
│ 传感器 / 执行器 / 按键 / 3.5 寸彩屏 / 本地规则与报警        │
└───────────────────────┬──────────────────────────────────┘
                        │ USART3 自定义帧协议（CRC16 + 应答重传）
┌───────────────────────▼──────────────────────────────────┐
│ 正点原子 DNESP32S3（语音终端）                              │
│ 唤醒 / 语音对话 / 按 STM32 能力清单动态注册 MCP 工具          │
└───────────────────────┬──────────────────────────────────┘
                        │ Wi-Fi
                大模型服务（语音识别 / 大模型 / 语音合成）
```

## 仓库结构

| 路径 | 内容 |
|---|---|
| `docs/接口设计.md` | 接线、引脚、串口帧协议、能力清单格式 |
| `docs/开发进度与继续指南.md` | 开发进度、编译下载步骤、后续计划 |
| `stm32/LingXi_STM32` | STM32 家居主控工程 |
| `stm32/LingXi_BoardTest` | 板级引脚验证程序（LED、蜂鸣器、按键、USART3 自环） |
| `tools/stm32_isp.py` | 串口一键下载、读取芯片信息、监视调试输出 |
| `tools/link_test.py` | 在电脑上模拟 ESP32，测试帧协议 |
| `tools/gen_font.py` | 扫描源码中用到的汉字，生成抗锯齿中文字库 |
| [LingXi_ESP32](https://github.com/sitongjun/LingXi_ESP32) | 语音终端固件（基于 xiaozhi-esp32 二次开发，板型 `lingxi-dnesp32s3`） |

## 进度

- [x] M1 打通链路：说“打开客厅灯”→ 大模型调用 MCP 工具 → 串口 → STM32 点亮 LED
- [x] M2 STM32 功能：3.5 寸彩屏状态面板、语音设置阈值、超阈值自动报警
- [ ] M2+ FreeRTOS 多任务、触摸交互、更多外设
- [ ] M3 大模型生成规则后下发到 STM32 本地执行，实现断网容灾
- [ ] M4 数据层：MQTT / EMQX / MySQL / 看板

## 编译与下载（STM32）

```powershell
python tools\build_stm32.py stm32\LingXi_STM32     # ARM GCC 编译（Keil 评估版有 32KB 限制）
python tools\stm32_isp.py COM7 flash stm32\LingXi_STM32\build_gcc\LingXi.hex
```

STM32 源文件为 UTF-8 编码。用 Keil 打开前，请把 Edit → Configuration → Editor → Encoding 设为 UTF-8。

ESP32 固件的编译方法见 [LingXi_ESP32](https://github.com/sitongjun/LingXi_ESP32)。

## 技术栈

STM32F103（C）· ESP32-S3（ESP-IDF 5.4.2，C++）· MCP 工具调用 · 串口帧协议（CRC16）· Python 工具链
