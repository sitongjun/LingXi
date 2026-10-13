#include "home.h"
#include "bsp.h"
#include <stdio.h>
#include <string.h>

#define FW_VERSION "0.1.0"

typedef struct {
	u8          id;
	const char *name;      /* 英文标识，ESP32 端据此生成 MCP 工具名 */
	const char *desc;      /* 给大模型看的说明 */
	u8          type;
	s16         min, max;
	s16         value;
	void      (*apply)(s16 v);
} home_dev_t;

static void apply_living_light(s16 v)  { DS0 = v ? 0 : 1; }
static void apply_bedroom_light(s16 v) { DS1 = v ? 0 : 1; }
static void apply_alarm(s16 v)         { if (!v) BEEP = 0; }   /* 响铃节奏在 home_tick 中产生 */

/* 设备表：新增外设只需在这里加一行，ESP32 端会自动生成对应的 MCP 工具 */
static home_dev_t devs[] = {
	{ 1, "living_light",  "Living room light (DS0 on board)",                DEV_SWITCH, 0, 1, 0, apply_living_light },
	{ 2, "bedroom_light", "Bedroom light (DS1 on board)",                    DEV_SWITCH, 0, 1, 0, apply_bedroom_light },
	{ 3, "alarm",         "Buzzer alarm. On = beeping, off = silent. Turn on only for emergencies or when the user asks.",
	                                                                          DEV_SWITCH, 0, 1, 0, apply_alarm },
};
#define DEV_COUNT  (sizeof(devs) / sizeof(devs[0]))

static char json[LINK_MAX_PAYLOAD];
static u8   evt_seq = 0;

static home_dev_t *find_dev(u8 id)
{
	u8 i;
	for (i = 0; i < DEV_COUNT; i++)
		if (devs[i].id == id) return &devs[i];
	return 0;
}

static home_dev_t *find_dev_by_name(const char *name)
{
	u8 i;
	for (i = 0; i < DEV_COUNT; i++)
		if (strcmp(devs[i].name, name) == 0) return &devs[i];
	return 0;
}

static u16 build_caps(void)
{
	u16 n;
	u8 i;

	n = (u16)snprintf(json, sizeof(json), "{\"node\":\"stm32_home\",\"fw\":\"%s\",\"devices\":[", FW_VERSION);
	for (i = 0; i < DEV_COUNT; i++)
	{
		n += (u16)snprintf(json + n, sizeof(json) - n,
		     "%s{\"id\":%u,\"name\":\"%s\",\"desc\":\"%s\",\"type\":\"%s\",\"min\":%d,\"max\":%d}",
		     i ? "," : "", devs[i].id, devs[i].name, devs[i].desc,
		     devs[i].type == DEV_SWITCH ? "switch" : "level", devs[i].min, devs[i].max);
	}
	n += (u16)snprintf(json + n, sizeof(json) - n, "]}");
	return n;
}

static u16 build_state(void)
{
	u16 n;
	u8 i;

	n = (u16)snprintf(json, sizeof(json), "{");
	for (i = 0; i < DEV_COUNT; i++)
		n += (u16)snprintf(json + n, sizeof(json) - n, "\"%s\":%d,", devs[i].name, devs[i].value);
	n += (u16)snprintf(json + n, sizeof(json) - n, "\"uptime_s\":%lu}", (unsigned long)(millis() / 1000));
	return n;
}

static u8 set_dev(home_dev_t *d, s16 v)
{
	if (v < d->min || v > d->max) return ST_RANGE;
	d->value = v;
	d->apply(v);
	return ST_OK;
}

/* 本地操作（按键）后主动上报，让 ESP32/大模型知道状态变化 */
static void report_event(const char *src, home_dev_t *d)
{
	u16 n = (u16)snprintf(json, sizeof(json), "{\"src\":\"%s\",\"dev\":\"%s\",\"value\":%d}",
	                      src, d->name, d->value);
	link_send(CMD_EVENT, evt_seq++, (const u8 *)json, n);
}

void home_init(void)
{
	u8 i;
	for (i = 0; i < DEV_COUNT; i++)
		devs[i].apply(devs[i].value);
}

void home_handle_frame(const link_frame_t *f)
{
	u8 resp[4];
	u16 n;

	switch (f->cmd)
	{
	case CMD_PING:
	{
		u32 t = millis();
		resp[0] = (u8)t; resp[1] = (u8)(t >> 8); resp[2] = (u8)(t >> 16); resp[3] = (u8)(t >> 24);
		link_send(CMD_RESP(CMD_PING), f->seq, resp, 4);
		break;
	}
	case CMD_GET_CAPS:
		n = build_caps();
		link_send(CMD_RESP(CMD_GET_CAPS), f->seq, (const u8 *)json, n);
		printf("[LINK] caps requested (%u bytes)\r\n", n);
		break;

	case CMD_GET_STATE:
		n = build_state();
		link_send(CMD_RESP(CMD_GET_STATE), f->seq, (const u8 *)json, n);
		break;

	case CMD_SET:
	{
		home_dev_t *d;
		s16 v = 0;
		u8 st;

		if (f->len != 3)
		{
			st = ST_BAD_LEN;
			resp[1] = 0;
		}
		else
		{
			resp[1] = f->data[0];
			v = (s16)(f->data[1] | (f->data[2] << 8));
			d = find_dev(f->data[0]);
			st = d ? set_dev(d, v) : ST_NO_DEV;
			if (d) v = d->value;
		}
		resp[0] = st;
		resp[2] = (u8)v;
		resp[3] = (u8)((u16)v >> 8);
		link_send(CMD_RESP(CMD_SET), f->seq, resp, 4);
		printf("[LINK] SET dev=%u value=%d -> status %u\r\n", resp[1], v, st);
		break;
	}
	default:
		printf("[LINK] unknown cmd 0x%02X\r\n", f->cmd);
		break;
	}
}

void home_tick(void)
{
	static u32 last_key = 0, last_beep = 0;
	home_dev_t *alarm = find_dev_by_name("alarm");
	u32 now = millis();

	/* 报警节奏：响 300ms / 停 300ms */
	if (alarm->value && now - last_beep >= 300)
	{
		last_beep = now;
		BEEP = !BEEP;
	}

	/* 按键：K1 客厅灯、K2 卧室灯、K3 报警，KEY_UP 一键全关 */
	if (now - last_key >= 10)
	{
		u8 k = bsp_key_scan();
		home_dev_t *d = 0;
		u8 i;

		last_key = now;
		if (k & KEY_1) d = find_dev_by_name("living_light");
		if (k & KEY_2) d = find_dev_by_name("bedroom_light");
		if (k & KEY_3) d = alarm;
		if (d)
		{
			set_dev(d, !d->value);
			report_event("key", d);
			printf("[KEY] %s -> %d\r\n", d->name, d->value);
		}
		if (k & KEY_UP)
		{
			for (i = 0; i < DEV_COUNT; i++)
			{
				if (devs[i].value)
				{
					set_dev(&devs[i], 0);
					report_event("key", &devs[i]);
				}
			}
			printf("[KEY] all off\r\n");
		}
	}
}
