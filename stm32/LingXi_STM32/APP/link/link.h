#ifndef _LINK_H
#define _LINK_H

#include "system.h"

/*
 * STM32 <-> DNESP32S3 串口帧协议（USART3: PB10 TX / PB11 RX, 115200 8N1）
 *
 *   | 0xAA | 0x55 | LEN_L | LEN_H | CMD | SEQ | PAYLOAD(LEN 字节) | CRC_L | CRC_H |
 *
 *   CRC16-CCITT(poly 0x1021, init 0xFFFF)，范围：LEN_L .. PAYLOAD 末尾
 *   应答帧 CMD = 请求 CMD | 0x80，SEQ 与请求相同
 */
#define LINK_MAX_PAYLOAD   2048

enum {
	CMD_PING      = 0x01,   /* -> PONG: uptime_ms(u32 LE)                         */
	CMD_GET_CAPS  = 0x02,   /* -> 能力清单 JSON                                    */
	CMD_SET       = 0x03,   /* dev_id(u8) value(s16 LE) -> status(u8) dev_id value */
	CMD_GET_STATE = 0x04,   /* -> 状态 JSON                                        */
	CMD_EVENT     = 0x40,   /* STM32 主动上报事件 JSON，无需应答                    */
};
#define CMD_RESP(c)  ((u8)((c) | 0x80))

typedef struct {
	u8  cmd;
	u8  seq;
	u16 len;
	u8  data[LINK_MAX_PAYLOAD];
} link_frame_t;

void link_init(u32 baud);
u8   link_poll(link_frame_t *f);                 /* 收到完整且校验正确的帧返回 1 */
void link_send(u8 cmd, u8 seq, const u8 *data, u16 len);
u16  link_crc16(u16 crc, const u8 *d, u16 n);
u32  link_crc_errors(void);

#endif
