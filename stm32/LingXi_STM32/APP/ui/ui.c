#include "ui.h"
#include "gfx.h"
#include "home.h"
#include "bsp.h"
#include <stdio.h>
#include <string.h>

/* ---------------- 配色（深色主题） ---------------- */
#define COL_BG        RGB(10, 16, 26)
#define COL_CARD      RGB(22, 32, 48)
#define COL_TEXT      RGB(232, 238, 246)
#define COL_SUB       RGB(132, 148, 170)
#define COL_DIM       RGB(64, 78, 98)
#define COL_HEAD      RGB(20, 40, 78)
#define COL_ACCENT    RGB(0, 196, 180)
#define COL_ALARM_A   RGB(200, 30, 40)
#define COL_ALARM_B   RGB(110, 14, 22)
#define COL_OK        RGB(64, 214, 140)
#define COL_WARN      RGB(255, 178, 40)
#define COL_DANGER    RGB(255, 74, 74)
#define COL_TEMP      RGB(255, 140, 64)
#define COL_HUMI      RGB(72, 168, 255)
#define COL_LIGHT_ON  RGB(255, 208, 64)

/* ---------------- 布局 ---------------- */
#define HEAD_H        56
#define ENV_Y         64
#define ENV_H         150
#define TILE_W        148
#define TREND_Y       222
#define TREND_H       118
#define PLOT_X        18
#define PLOT_Y        (TREND_Y + 34)
#define PLOT_W        284
#define PLOT_H        74
#define DEV_Y         348
#define DEV_H         88
#define FOOT_Y        444

#define HIST_N        72          /* 每 2 秒一个点，约 2.4 分钟 */

static s16 hist_t[HIST_N];        /* 温度 ×10 */
static u8  hist_h[HIST_N];
static u8  hist_count = 0;

/* 上一次绘制的状态，用于增量刷新 */
static struct {
	s16 temp_x10;
	u8  humi;
	u8  sensor_ok;
	u8  over;
	s16 t_lim, h_lim;
	s16 dev[3];
	u8  online;
	u8  alarm_phase;
	u16 evt_seq;
	u32 uptime_s;
	u32 sample_ms;
} last;

static const char *dev_names[3]  = { "living_light", "bedroom_light", "alarm" };
static const char *dev_labels[3] = { "客厅灯", "卧室灯", "报警器" };

/* ---------------- 工具函数 ---------------- */

static u16 level_color(s16 v, s16 limit)
{
	if (v >= limit) return COL_DANGER;
	if (v * 10 >= limit * 9) return COL_WARN;     /* 达到阈值的 90% 预警 */
	return COL_OK;
}

static void card(u16 x, u16 y, u16 w, u16 h)
{
	gfx_round_rect(x, y, w, h, 10, COL_CARD, COL_BG);
}

/* ---------------- 顶栏 ---------------- */

static void draw_header(u8 alarm_phase)
{
	const home_env_t *e = home_env();

	if (!alarm_phase)
	{
		lcd_fill(0, 0, LCD_W, HEAD_H - 3, COL_HEAD);
		lcd_fill(0, HEAD_H - 3, LCD_W, 3, COL_ACCENT);
		gfx_text(14, 12, "灵犀 · 居家安全管家", &font_24, COL_TEXT, COL_HEAD);
	}
	else
	{
		u16 c = (alarm_phase == 1) ? COL_ALARM_A : COL_ALARM_B;
		const char *msg = "报警：手动触发";
		lcd_fill(0, 0, LCD_W, HEAD_H, c);
		if ((e->over & OVER_TEMP) && (e->over & OVER_HUMI)) msg = "报警：温湿度超限";
		else if (e->over & OVER_TEMP) msg = "报警：温度过高";
		else if (e->over & OVER_HUMI) msg = "报警：湿度过高";
		gfx_text(14, 12, msg, &font_24, C_WHITE, c);
	}
	last.online = 0xFF;            /* 顶栏重绘后在线状态也要重画 */
}

static void draw_online(u8 online, u16 bg)
{
	u16 x = LCD_W - 82;
	lcd_fill(x, 18, 72, 20, bg);
	gfx_circle(x + 8, 28, 5, online ? COL_OK : COL_DIM, bg);
	gfx_text(x + 18, 18, online ? "AI 在线" : "AI 离线", &font_16, online ? COL_TEXT : COL_SUB, bg);
}

/* ---------------- 温湿度卡片 ---------------- */

static void draw_env_frame(void)
{
	card(8, ENV_Y, TILE_W, ENV_H);
	card(8 + TILE_W + 8, ENV_Y, TILE_W, ENV_H);
	gfx_circle(26, ENV_Y + 20, 5, COL_TEMP, COL_CARD);
	gfx_text(38, ENV_Y + 11, "温度", &font_16, COL_SUB, COL_CARD);
	gfx_circle(26 + TILE_W + 8, ENV_Y + 20, 5, COL_HUMI, COL_CARD);
	gfx_text(38 + TILE_W + 8, ENV_Y + 11, "湿度", &font_16, COL_SUB, COL_CARD);
}

static void draw_env_tile(u16 x, s16 value, s16 limit, u8 ok, u8 is_temp)
{
	char buf[16];
	u16 color = ok ? level_color(is_temp ? value / 10 : value, limit) : COL_DIM;
	u16 vx = x + 14, vy = ENV_Y + 34;
	u16 w, bar_w = TILE_W - 28, fill;

	/* 数值 */
	if (!ok) snprintf(buf, sizeof(buf), "--");
	else if (is_temp) snprintf(buf, sizeof(buf), "%d.%d", value / 10, (value < 0 ? -value : value) % 10);
	else snprintf(buf, sizeof(buf), "%d", value);
	lcd_fill(vx, vy, TILE_W - 20, font_num48.h, COL_CARD);
	w = gfx_text(vx, vy, buf, &font_num48, ok ? COL_TEXT : COL_DIM, COL_CARD);
	gfx_text(vx + w + 4, vy + font_num48.h - font_24.h - 8, is_temp ? "°C" : "%", &font_24, COL_SUB, COL_CARD);

	/* 阈值 */
	snprintf(buf, sizeof(buf), is_temp ? "阈值 %d°C" : "阈值 %d%%", limit);
	gfx_text_box(vx, ENV_Y + 104, bar_w, buf, &font_16, COL_SUB, COL_CARD, ALIGN_LEFT);

	/* 进度条：当前值 / 阈值 */
	{
		s16 v = ok ? (is_temp ? value / 10 : value) : 0;
		if (v < 0) v = 0;
		fill = (u16)((u32)bar_w * (v > limit ? limit : v) / limit);
		gfx_round_rect(vx, ENV_Y + 130, bar_w, 8, 4, COL_DIM, COL_CARD);
		if (fill >= 8) gfx_round_rect(vx, ENV_Y + 130, fill, 8, 4, color, COL_DIM);
	}
}

/* ---------------- 趋势图 ---------------- */

static void draw_trend_frame(void)
{
	card(8, TREND_Y, LCD_W - 16, TREND_H);
	gfx_text(22, TREND_Y + 9, "温湿度趋势", &font_16, COL_SUB, COL_CARD);
	gfx_round_rect(LCD_W - 150, TREND_Y + 15, 14, 4, 2, COL_TEMP, COL_CARD);
	gfx_text(LCD_W - 132, TREND_Y + 9, "温度", &font_16, COL_SUB, COL_CARD);
	gfx_round_rect(LCD_W - 82, TREND_Y + 15, 14, 4, 2, COL_HUMI, COL_CARD);
	gfx_text(LCD_W - 64, TREND_Y + 9, "湿度", &font_16, COL_SUB, COL_CARD);
}

/* 温度按 0~50°C、湿度按 0~100% 映射到绘图区高度 */
static int plot_y_temp(s16 t_x10)
{
	int v = t_x10 < 0 ? 0 : (t_x10 > 500 ? 500 : t_x10);
	return PLOT_Y + PLOT_H - 1 - v * (PLOT_H - 1) / 500;
}

static int plot_y_humi(u8 h)
{
	int v = h > 100 ? 100 : h;
	return PLOT_Y + PLOT_H - 1 - v * (PLOT_H - 1) / 100;
}

static void draw_trend(void)
{
	u8 i;
	int step = PLOT_W / (HIST_N - 1);

	lcd_fill(PLOT_X, PLOT_Y, PLOT_W, PLOT_H, COL_CARD);
	/* 网格线 */
	for (i = 0; i <= 4; i++)
		gfx_hline(PLOT_X, PLOT_Y + i * (PLOT_H - 1) / 4, PLOT_W, gfx_blend(COL_DIM, COL_CARD, 120));

	if (hist_count < 2)
	{
		gfx_text(PLOT_X + 80, PLOT_Y + 28, "等待采样数据…", &font_16, COL_DIM, COL_CARD);
		return;
	}
	for (i = 1; i < hist_count; i++)
	{
		int x0 = PLOT_X + (HIST_N - hist_count + i - 1) * step;
		int x1 = x0 + step;
		gfx_line(x0, plot_y_humi(hist_h[i - 1]), x1, plot_y_humi(hist_h[i]), COL_HUMI);
		gfx_line(x0, plot_y_temp(hist_t[i - 1]), x1, plot_y_temp(hist_t[i]), COL_TEMP);
		gfx_line(x0, plot_y_temp(hist_t[i - 1]) + 1, x1, plot_y_temp(hist_t[i]) + 1, COL_TEMP);
	}
}

static void push_history(s16 t, u8 h)
{
	if (hist_count == HIST_N)
	{
		memmove(hist_t, hist_t + 1, (HIST_N - 1) * sizeof(hist_t[0]));
		memmove(hist_h, hist_h + 1, (HIST_N - 1) * sizeof(hist_h[0]));
		hist_count--;
	}
	hist_t[hist_count] = t;
	hist_h[hist_count] = h;
	hist_count++;
}

/* ---------------- 设备卡片 ---------------- */

static void draw_dev_frame(void)
{
	card(8, DEV_Y, LCD_W - 16, DEV_H);
}

static void draw_device(u8 i, s16 value)
{
	u16 col_w = (LCD_W - 16) / 3;
	u16 x = 8 + i * col_w;
	u16 cx = x + col_w / 2;
	u16 on_color = (i == 2) ? COL_DANGER : COL_LIGHT_ON;

	lcd_fill(x + 4, DEV_Y + 6, col_w - 8, DEV_H - 12, COL_CARD);
	if (value)
	{
		gfx_circle(cx, DEV_Y + 26, 18, gfx_blend(on_color, COL_CARD, 60), COL_CARD);   /* 光晕 */
		gfx_circle(cx, DEV_Y + 26, 12, on_color, gfx_blend(on_color, COL_CARD, 60));
	}
	else
	{
		gfx_circle(cx, DEV_Y + 26, 12, COL_DIM, COL_CARD);
		gfx_circle(cx, DEV_Y + 26, 8, COL_CARD, COL_DIM);
	}
	gfx_text_box(x + 4, DEV_Y + 48, col_w - 8, dev_labels[i], &font_16, COL_TEXT, COL_CARD, ALIGN_CENTER);
	gfx_text_box(x + 4, DEV_Y + 66, col_w - 8, value ? (i == 2 ? "报警中" : "已开启") : "已关闭",
	             &font_16, value ? on_color : COL_SUB, COL_CARD, ALIGN_CENTER);
}

/* ---------------- 底栏 ---------------- */

static void draw_footer_event(void)
{
	const home_event_t *ev = home_last_event();
	char buf[64];
	static const char *src_cn[] = { "语音", "按键", "自动" };

	if (!ev->ms)
	{
		gfx_text_box(12, FOOT_Y + 8, 210, "等待操作…", &font_16, COL_SUB, COL_BG, ALIGN_LEFT);
		return;
	}
	if (strstr(ev->label, "阈值"))
		snprintf(buf, sizeof(buf), "[%s] %s 设为 %d", src_cn[ev->src], ev->label, ev->value);
	else
		snprintf(buf, sizeof(buf), "[%s] %s %s", src_cn[ev->src], ev->label, ev->value ? "开" : "关");
	gfx_text_box(12, FOOT_Y + 8, 210, buf, &font_16, ev->src == SRC_AUTO ? COL_WARN : COL_TEXT, COL_BG, ALIGN_LEFT);
}

static void draw_uptime(u32 s)
{
	char buf[24];
	snprintf(buf, sizeof(buf), "运行 %02lu:%02lu:%02lu",
	         (unsigned long)(s / 3600), (unsigned long)(s / 60 % 60), (unsigned long)(s % 60));
	gfx_text_box(LCD_W - 112, FOOT_Y + 8, 100, buf, &font_16, COL_SUB, COL_BG, ALIGN_RIGHT);
}

/* ---------------- 对外接口 ---------------- */

void ui_init(void)
{
	lcd_clear(COL_BG);
	memset(&last, 0xFF, sizeof(last));
	draw_header(0);
	last.alarm_phase = 0;
	draw_env_frame();
	draw_trend_frame();
	draw_trend();
	draw_dev_frame();
	gfx_hline(12, FOOT_Y, LCD_W - 24, COL_DIM);
	draw_footer_event();
	last.evt_seq = home_last_event()->seq;
}

void ui_tick(void)
{
	static u32 last_ms = 0;
	const home_env_t *e = home_env();
	u32 now = millis();
	s16 t_lim = home_value("temp_limit");
	s16 h_lim = home_value("humi_limit");
	u8 alarm_on = (u8)home_value("alarm");
	u8 phase, online, i;

	if (now - last_ms < 100) return;     /* 10Hz 刷新 */
	last_ms = now;

	/* 顶栏：报警时红色闪烁 */
	phase = alarm_on ? (u8)((now / 500) % 2 + 1) : 0;
	if (phase != last.alarm_phase)
	{
		draw_header(phase);
		last.alarm_phase = phase;
	}
	online = home_link_online();
	if (online != last.online)
	{
		draw_online(online, phase ? (phase == 1 ? COL_ALARM_A : COL_ALARM_B) : COL_HEAD);
		last.online = online;
	}

	/* 温湿度 */
	if (e->sensor_ok != last.sensor_ok || e->temp_x10 != last.temp_x10 || t_lim != last.t_lim)
		draw_env_tile(8, e->temp_x10, t_lim, e->sensor_ok, 1);
	if (e->sensor_ok != last.sensor_ok || e->humi != last.humi || h_lim != last.h_lim)
		draw_env_tile(8 + TILE_W + 8, e->humi, h_lim, e->sensor_ok, 0);
	last.sensor_ok = e->sensor_ok;
	last.temp_x10 = e->temp_x10;
	last.humi = e->humi;
	last.t_lim = t_lim;
	last.h_lim = h_lim;

	/* 每次新采样追加到趋势图 */
	if (e->sensor_ok && e->updated_ms != last.sample_ms)
	{
		last.sample_ms = e->updated_ms;
		push_history(e->temp_x10, e->humi);
		draw_trend();
	}

	/* 设备 */
	for (i = 0; i < 3; i++)
	{
		s16 v = home_value(dev_names[i]);
		if (v != last.dev[i])
		{
			draw_device(i, v);
			last.dev[i] = v;
		}
	}

	/* 底栏 */
	if (home_last_event()->seq != last.evt_seq)
	{
		last.evt_seq = home_last_event()->seq;
		draw_footer_event();
	}
	if (now / 1000 != last.uptime_s)
	{
		last.uptime_s = now / 1000;
		draw_uptime(last.uptime_s);
	}
}
