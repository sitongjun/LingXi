# 灵犀 LingXi —— 大模型驱动的居家安全管家

嵌入式系统课程大作业。老人对着桌面语音终端说话，就能控制全屋设备、查询环境；
大模型负责“听懂和规划”，STM32 负责“可靠执行”，断网时本地安全功能照常工作。

## 系统组成

```
┌────────────── 普中 STM32F103ZET6（家居主控）──────────────┐
│ 传感器 / 执行器 / 按键 / 3.5 寸屏 / 本地规则引擎            │
└───────────────────────┬──────────────────────────────────┘
                        │ USART3 自定义帧协议（CRC16 + 应答重传）
┌───────────────────────▼──────────────────────────────────┐
│ 正点原子 DNESP32S3（语音终端）                              │
│ 唤醒 / 语音对话 / 根据 STM32 能力清单动态注册 MCP 工具       │
└───────────────────────┬──────────────────────────────────┘
                        │ WiFi
                   大模型服务（语音识别 / 大模型 / 语音合成）
```

## 目录

| 路径 | 内容 |
|---|---|
| `docs/接口设计.md` | 接线、引脚、串口帧协议、能力清单格式 |
| `docs/开发进度与继续指南.md` | 当前进度、编译下载步骤、下一步计划 |
| `stm32/LingXi_STM32` | STM32 家居主控 Keil 工程 |
| `stm32/LingXi_BoardTest` | 板级引脚验证程序（LED、蜂鸣器、按键、USART3 自环） |
| `tools/stm32_isp.py` | 串口一键下载 / 读芯片信息 / 监视调试输出 |
| `tools/link_test.py` | 在电脑上扮演 ESP32 测试帧协议 |
| [LingXi_ESP32](https://github.com/sitongjun/LingXi_ESP32) | DNESP32S3 固件（基于 xiaozhi-esp32，板型 `lingxi-dnesp32s3`） |

## 进度

- [x] M1 打通链路：语音“打开客厅灯”→ 大模型调用 MCP 工具 → 串口 → STM32 点亮 DS0
- [x] M2 STM32 功能完善：3.5 寸彩屏状态面板、阈值语音设置、超阈值自动报警（DHT11 待接入）
- [ ] M2+ FreeRTOS、触摸、更多外设
- [ ] M3 创新功能：大模型生成规则、STM32 本地执行、多级容灾
- [ ] M4 数据层：MQTT / EMQX / MySQL / 看板

## 编译与下载（STM32）

```powershell
python tools\build_stm32.py stm32\LingXi_STM32     # ARM GCC 编译（Keil 评估版有 32KB 限制）
python tools\stm32_isp.py COM7 flash stm32\LingXi_STM32\build_gcc\LingXi.hex
```

STM32 源文件为 UTF-8 编码，用 Keil 打开前请设置 Edit → Configuration → Editor → Encoding 为 UTF-8。
