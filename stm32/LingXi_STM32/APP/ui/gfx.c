#include "gfx.h"

u16 gfx_blend(u16 fg, u16 bg, u8 a)
{
	u16 r = (u16)((((fg >> 11) & 0x1F) * a + ((bg >> 11) & 0x1F) * (255 - a)) / 255);
	u16 g = (u16)((((fg >> 5) & 0x3F) * a + ((bg >> 5) & 0x3F) * (255 - a)) / 255);
	u16 b = (u16)(((fg & 0x1F) * a + (bg & 0x1F) * (255 - a)) / 255);
	return (u16)((r << 11) | (g << 5) | b);
}

/* 解码一个 UTF-8 字符，返回码点并前移指针 */
static u16 utf8_next(const char **ps)
{
	const u8 *s = (const u8 *)*ps;
	u16 c;

	if (s[0] < 0x80)
	{
		c = s[0];
		*ps += 1;
	}
	else if ((s[0] & 0xE0) == 0xC0)
	{
		c = (u16)(((s[0] & 0x1F) << 6) | (s[1] & 0x3F));
		*ps += 2;
	}
	else
	{
		c = (u16)(((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F));
		*ps += 3;
	}
	return c;
}

static const glyph_t *find_glyph(const font_t *f, u16 code)
{
	int lo = 0, hi = f->count - 1;
	while (lo <= hi)
	{
		int mid = (lo + hi) / 2;
		if (f->glyphs[mid].code == code) return &f->glyphs[mid];
		if (f->glyphs[mid].code < code) lo = mid + 1;
		else hi = mid - 1;
	}
	return 0;
}

u16 gfx_text_width(const font_t *f, const char *s)
{
	u16 w = 0;
	while (*s)
	{
		const glyph_t *g = find_glyph(f, utf8_next(&s));
		w += g ? g->w : f->h / 2;
	}
	return w;
}

u16 gfx_text(u16 x, u16 y, const char *s, const font_t *f, u16 fg, u16 bg)
{
	u16 pal[16];
	u16 x0 = x;
	u8 i;

	for (i = 0; i < 16; i++)
		pal[i] = gfx_blend(fg, bg, (u8)(i * 17));

	while (*s)
	{
		const glyph_t *g = find_glyph(f, utf8_next(&s));
		const u8 *p;
		u16 row, col, stride;

		if (!g)
		{
			lcd_fill(x, y, f->h / 2, f->h, bg);    /* 字库中没有的字符留空 */
			x += f->h / 2;
			continue;
		}
		if (x + g->w > LCD_W) break;
		p = f->bits + g->off;
		stride = (u16)((g->w + 1) / 2);
		lcd_set_window(x, y, x + g->w - 1, y + f->h - 1);
		for (row = 0; row < f->h; row++)
		{
			for (col = 0; col < g->w; col++)
			{
				u8 b = p[col >> 1];
				lcd_write_px(pal[(col & 1) ? (b & 0x0F) : (b >> 4)]);
			}
			p += stride;
		}
		x += g->w;
	}
	return (u16)(x - x0);
}

void gfx_text_box(u16 x, u16 y, u16 w, const char *s, const font_t *f, u16 fg, u16 bg, u8 align)
{
	u16 tw = gfx_text_width(f, s);
	u16 off = 0;

	if (tw > w) tw = w;
	if (align == ALIGN_CENTER) off = (u16)((w - tw) / 2);
	else if (align == ALIGN_RIGHT) off = (u16)(w - tw);

	if (off) lcd_fill(x, y, off, f->h, bg);
	gfx_text(x + off, y, s, f, fg, bg);
	if (off + tw < w) lcd_fill(x + off + tw, y, w - off - tw, f->h, bg);
}

void gfx_hline(u16 x, u16 y, u16 w, u16 color) { lcd_fill(x, y, w, 1, color); }
void gfx_vline(u16 x, u16 y, u16 h, u16 color) { lcd_fill(x, y, 1, h, color); }

/* 圆角矩形：四个角做简单抗锯齿 */
void gfx_round_rect(u16 x, u16 y, u16 w, u16 h, u8 r, u16 color, u16 bg)
{
	u16 i;
	int r2 = r * r;

	lcd_fill(x, y + r, w, h - 2 * r, color);
	for (i = 0; i < r; i++)
	{
		int dy = r - i;                 /* 当前行到圆心的竖直距离 */
		int dx = 0;
		u16 inset;
		while ((dx + 1) * (dx + 1) + dy * dy <= r2) dx++;
		inset = (u16)(r - dx);
		lcd_fill(x + inset, y + i, w - 2 * inset, 1, color);
		lcd_fill(x + inset, y + h - 1 - i, w - 2 * inset, 1, color);
		if (inset)
		{
			/* 边缘像素按覆盖比例混合 */
			u16 edge = gfx_blend(color, bg, 110);
			lcd_pixel(x + inset - 1, y + i, edge);
			lcd_pixel(x + w - inset, y + i, edge);
			lcd_pixel(x + inset - 1, y + h - 1 - i, edge);
			lcd_pixel(x + w - inset, y + h - 1 - i, edge);
		}
	}
}

void gfx_circle(u16 cx, u16 cy, u8 r, u16 color, u16 bg)
{
	int dy;
	int r2 = r * r;

	for (dy = -r; dy <= r; dy++)
	{
		int dx = 0;
		while ((dx + 1) * (dx + 1) + dy * dy <= r2) dx++;
		lcd_fill(cx - dx, cy + dy, 2 * dx + 1, 1, color);
		if (dx < r)
		{
			u16 edge = gfx_blend(color, bg, 110);
			lcd_pixel(cx - dx - 1, cy + dy, edge);
			lcd_pixel(cx + dx + 1, cy + dy, edge);
		}
	}
}

void gfx_line(int x0, int y0, int x1, int y1, u16 color)
{
	int dx = x1 > x0 ? x1 - x0 : x0 - x1;
	int dy = y1 > y0 ? y0 - y1 : y1 - y0;
	int sx = x0 < x1 ? 1 : -1;
	int sy = y0 < y1 ? 1 : -1;
	int err = dx + dy;

	while (1)
	{
		lcd_pixel((u16)x0, (u16)y0, color);
		if (x0 == x1 && y0 == y1) break;
		{
			int e2 = 2 * err;
			if (e2 >= dy) { err += dy; x0 += sx; }
			if (e2 <= dx) { err += dx; y0 += sy; }
		}
	}
}

void gfx_vgradient(u16 x, u16 y, u16 w, u16 h, u16 top, u16 bottom)
{
	u16 i;
	for (i = 0; i < h; i++)
		lcd_fill(x, y + i, w, 1, gfx_blend(bottom, top, (u8)(i * 255 / (h - 1))));
}
