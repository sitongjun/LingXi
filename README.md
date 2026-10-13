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
| `stm32/LingXi_STM32` | STM32 家居主控 Keil 工程 |
| `stm32/LingXi_BoardTest` | 板级引脚验证程序（LED、蜂鸣器、按键、USART3 自环） |
| `tools/stm32_isp.py` | 串口一键下载 / 读芯片信息 / 监视调试输出 |
| `tools/link_test.py` | 在电脑上扮演 ESP32 测试帧协议 |
| [LingXi_ESP32](https://github.com/sitongjun/LingXi_ESP32) | DNESP32S3 固件（基于 xiaozhi-esp32，板型 `lingxi-dnesp32s3`） |

## 进度

- [x] M1 打通链路：语音“打开客厅灯”→ 大模型调用 MCP 工具 → 串口 → STM32 点亮 DS0
- [ ] M2 STM32 功能完善：3.5 寸彩屏状态面板、温湿度采集、超阈值报警、FreeRTOS
- [ ] M3 创新功能：大模型生成规则、STM32 本地执行、多级容灾
- [ ] M4 数据层：MQTT / EMQX / MySQL / 看板

## 编译与下载（STM32）

```powershell
D:\keil5\UV4\UV4.exe -b stm32\LingXi_STM32\Template.uvprojx
python tools\stm32_isp.py COM7 flash stm32\LingXi_STM32\Obj\Template.hex
```

STM32 源文件为 GBK 编码（Keil 默认），仓库中以 UTF-8 存储，检出时自动转换（见 `.gitattributes`）。
