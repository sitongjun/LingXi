#include "home.h"
#include "bsp.h"
#include "dht11.h"
#include <stdio.h>
#include <string.h>

#define FW_VERSION        "0.2.0"
#define SAMPLE_PERIOD_MS  2000
#define TEMP_HYST_X10     10      /* 温度回差 1.0°C */
#define HUMI_HYST         3       /* 湿度回差 3% */

typedef struct {
	u8          id;
	const char *name;      /* 英文标识，ESP32 端据此生成 MCP 工具名 */
	const char *label;     /* 中文名，用于屏幕显示和工具说明 */
	const char *desc;      /* 给大模型看的说明 */
	u8          type;
	s16         min, max;
	s16         value;
	void      (*apply)(s16 v);
} home_dev_t;

static void apply_living_light(s16 v)  { DS0 = v ? 0 : 1; }
static void apply_bedroom_light(s16 v) { DS1 = v ? 0 : 1; }
static void apply_alarm(s16 v)         { if (!v) BEEP = 0; }   /* 响铃节奏在 home_tick 中产生 */
static void apply_none(s16 v)          { (void)v; }

/* 设备表：新增外设只需在这里加一行，ESP32 端会自动生成对应的 MCP 工具 */
static home_dev_t devs[] = {
	{ 1, "living_light",  "客厅灯", "Living room light.", DEV_SWITCH, 0, 1, 0, apply_living_light },
	{ 2, "bedroom_light", "卧室灯", "Bedroom light.",     DEV_SWITCH, 0, 1, 0, apply_bedroom_light },
	{ 3, "alarm",         "报警器",
	  "Buzzer alarm. Turns on automatically when temperature or humidity exceeds its limit. "
	  "Turn on manually only for emergencies or when the user asks; turning it off while still over the limit silences it.",
	  DEV_SWITCH, 0, 1, 0, apply_alarm },
	{ 4, "temp_limit",    "温度阈值",
	  "Temperature alarm threshold in degrees Celsius. The alarm sounds when the room temperature reaches it.",
	  DEV_LEVEL, 10, 50, 30, apply_none },
	{ 5, "humi_limit",    "湿度阈值",
	  "Relative humidity alarm threshold in percent. The alarm sounds when the room humidity reaches it.",
	  DEV_LEVEL, 20, 95, 70, apply_none },
};
#define DEV_COUNT  (sizeof(devs) / sizeof(devs[0]))

static char json[LINK_MAX_PAYLOAD];
static u8   evt_seq = 0;
static u32  last_rx_ms = 0;
static home_env_t   env;
static home_event_t last_evt;

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

static const char *src_name(u8 src)
{
	return src == SRC_VOICE ? "voice" : (src == SRC_KEY ? "key" : "auto");
}

static u16 build_caps(void)
{
	u16 n;
	u8 i;

	n = (u16)snprintf(json, sizeof(json), "{\"node\":\"stm32_home\",\"fw\":\"%s\",\"devices\":[", FW_VERSION);
	for (i = 0; i < DEV_COUNT; i++)
	{
		n += (u16)snprintf(json + n, sizeof(json) - n,
		     "%s{\"id\":%u,\"name\":\"%s\",\"label\":\"%s\",\"desc\":\"%s\",\"type\":\"%s\",\"min\":%d,\"max\":%d}",
		     i ? "," : "", devs[i].id, devs[i].name, devs[i].label, devs[i].desc,
		     devs[i].type == DEV_SWITCH ? "switch" : "level", devs[i].min, devs[i].max);
	}
	n += (u16)snprintf(json + n, sizeof(json) - n,
	     "],\"sensors\":[{\"name\":\"temperature\",\"unit\":\"C\"},{\"name\":\"humidity\",\"unit\":\"%%\"}]}");
	return n;
}

static u16 build_state(void)
{
	u16 n;
	u8 i;

	n = (u16)snprintf(json, sizeof(json), "{");
	for (i = 0; i < DEV_COUNT; i++)
		n += (u16)snprintf(json + n, sizeof(json) - n, "\"%s\":%d,", devs[i].name, devs[i].value);
	if (env.sensor_ok)
		n += (u16)snprintf(json + n, sizeof(json) - n, "\"temperature\":%d.%d,\"humidity\":%u,",
		                   env.temp_x10 / 10, (env.temp_x10 < 0 ? -env.temp_x10 : env.temp_x10) % 10, env.humi);
	else
		n += (u16)snprintf(json + n, sizeof(json) - n, "\"sensor\":\"offline\",");
	n += (u16)snprintf(json + n, sizeof(json) - n, "\"over_temp\":%s,\"over_humi\":%s,\"uptime_s\":%lu}",
	                   (env.over & OVER_TEMP) ? "true" : "false", (env.over & OVER_HUMI) ? "true" : "false",
	                   (unsigned long)(millis() / 1000));
	return n;
}

static void record_event(u8 src, home_dev_t *d)
{
	last_evt.ms = millis() ? millis() : 1;
	last_evt.src = src;
	last_evt.label = d->label;
	last_evt.value = d->value;
	last_evt.seq++;
}

/* 本地发生的变化（按键、自动报警）主动上报，让 ESP32/大模型知道 */
static void report_event(u8 src, home_dev_t *d)
{
	u16 n = (u16)snprintf(json, sizeof(json), "{\"src\":\"%s\",\"dev\":\"%s\",\"label\":\"%s\",\"value\":%d",
	                      src_name(src), d->name, d->label, d->value);
	if (src == SRC_AUTO && env.sensor_ok)
		n += (u16)snprintf(json + n, sizeof(json) - n, ",\"over_temp\":%s,\"over_humi\":%s,\"temperature\":%d,\"humidity\":%u",
		                   (env.over & OVER_TEMP) ? "true" : "false", (env.over & OVER_HUMI) ? "true" : "false",
		                   env.temp_x10 / 10, env.humi);
	n += (u16)snprintf(json + n, sizeof(json) - n, "}");
	link_send(CMD_EVENT, evt_seq++, (const u8 *)json, n);
	record_event(src, d);
}

static u8 set_dev(home_dev_t *d, s16 v)
{
	if (v < d->min || v > d->max) return ST_RANGE;
	d->value = v;
	d->apply(v);
	return ST_OK;
}

/* 用户（语音/按键）操作报警器：超限期间关闭视为静音，直到恢复正常 */
static void user_set_alarm(home_dev_t *alarm, s16 v)
{
	if (!v && env.over) env.silenced = 1;
	if (v) env.silenced = 0;
	set_dev(alarm, v);
}

/* 根据最新温湿度判断是否超限，自动开/关报警 */
static void evaluate_alarm(void)
{
	home_dev_t *alarm = find_dev_by_name("alarm");
	s16 t_lim = (s16)(find_dev_by_name("temp_limit")->value * 10);
	s16 h_lim = find_dev_by_name("humi_limit")->value;
	u8 over = env.over;

	if (!env.sensor_ok) return;

	/* 带回差：达到阈值置位，回落到阈值以下一定幅度才清除 */
	if (env.temp_x10 >= t_lim) over |= OVER_TEMP;
	else if (env.temp_x10 <= t_lim - TEMP_HYST_X10) over &= (u8)~OVER_TEMP;
	if (env.humi >= h_lim) over |= OVER_HUMI;
	else if (env.humi <= h_lim - HUMI_HYST) over &= (u8)~OVER_HUMI;

	if (over && !env.over)
	{
		env.over = over;
		printf("[ALARM] over limit: T=%d.%d H=%u\r\n", env.temp_x10 / 10, env.temp_x10 % 10, env.humi);
		if (!env.silenced && !alarm->value)
		{
			set_dev(alarm, 1);
			report_event(SRC_AUTO, alarm);
		}
	}
	else if (!over && env.over)
	{
		env.over = 0;
		env.silenced = 0;
		printf("[ALARM] back to normal\r\n");
		if (alarm->value)
		{
			set_dev(alarm, 0);
			report_event(SRC_AUTO, alarm);
		}
	}
	else
	{
		env.over = over;
	}
}

static void sample_sensor(void)
{
	s16 t;
	u8 h;
	u8 ok = dht11_read(&t, &h);

	if (ok)
	{
		env.temp_x10 = t;
		env.humi = h;
		env.updated_ms = millis();
		if (!env.sensor_ok) printf("[DHT11] online\r\n");
		env.sensor_ok = 1;
	}
	else if (env.sensor_ok && millis() - env.updated_ms > 10000)
	{
		/* 连续 10 秒读不到才判定离线，偶发校验错误忽略 */
		env.sensor_ok = 0;
		printf("[DHT11] offline\r\n");
	}
	evaluate_alarm();
}

void home_init(void)
{
	u8 i;
	for (i = 0; i < DEV_COUNT; i++)
		devs[i].apply(devs[i].value);
	dht11_init();
}

void home_handle_frame(const link_frame_t *f)
{
	u8 resp[4];
	u16 n;

	last_rx_ms = millis() ? millis() : 1;
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
		home_dev_t *d = 0;
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
			if (!d) st = ST_NO_DEV;
			else if (v < d->min || v > d->max) st = ST_RANGE;
			else
			{
				if (strcmp(d->name, "alarm") == 0) user_set_alarm(d, v);
				else set_dev(d, v);
				st = ST_OK;
				record_event(SRC_VOICE, d);
				if (d->type == DEV_LEVEL) evaluate_alarm();   /* 阈值变化后立即重新判断 */
			}
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
	static u32 last_key = 0, last_beep = 0, last_sample = 0;
	home_dev_t *alarm = find_dev_by_name("alarm");
	u32 now = millis();

	/* 报警节奏：响 300ms / 停 300ms */
	if (alarm->value && now - last_beep >= 300)
	{
		last_beep = now;
		BEEP = !BEEP;
	}

	if (!last_sample || now - last_sample >= SAMPLE_PERIOD_MS)
	{
		last_sample = now ? now : 1;
		sample_sensor();
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
			if (d == alarm) user_set_alarm(d, (s16)!d->value);
			else set_dev(d, (s16)!d->value);
			report_event(SRC_KEY, d);
			printf("[KEY] %s -> %d\r\n", d->name, d->value);
		}
		if (k & KEY_UP)
		{
			for (i = 0; i < DEV_COUNT; i++)
			{
				if (devs[i].type == DEV_SWITCH && devs[i].value)
				{
					if (&devs[i] == alarm) user_set_alarm(alarm, 0);
					else set_dev(&devs[i], 0);
					report_event(SRC_KEY, &devs[i]);
				}
			}
			printf("[KEY] all off\r\n");
		}
	}
}

const home_env_t *home_env(void)          { return &env; }
const home_event_t *home_last_event(void) { return &last_evt; }

s16 home_value(const char *name)
{
	home_dev_t *d = find_dev_by_name(name);
	return d ? d->value : 0;
}

u8 home_link_online(void)
{
	return last_rx_ms && millis() - last_rx_ms < 10000;
}
