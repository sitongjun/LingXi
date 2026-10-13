"""为 STM32 彩屏生成抗锯齿字库（4bpp，16 级灰度）

扫描工程源码中字符串字面量里出现的汉字，加上全部可打印 ASCII，
用 Windows 自带字体渲染成位图，输出 APP/ui/font_data.c。

用法: python gen_font.py <STM32 工程目录>
改了界面文字后重新运行一次即可。
"""
import glob
import os
import re
import sys

from PIL import Image, ImageDraw, ImageFont

FONT_DIR = r'C:\Windows\Fonts'

# 名称、字体文件、字号(px)、字符集（'scan' = ASCII + 源码中出现的所有非 ASCII 字符）
FONTS = [
    ('font_16', 'msyh.ttc', 16, 'scan'),
    ('font_24', 'msyhbd.ttc', 24, 'scan'),
    ('font_num48', 'bahnschrift.ttf', 52, '0123456789.-:% '),
]

ASCII = ''.join(chr(c) for c in range(0x20, 0x7F))


def scan_chars(proj):
    chars = set()
    for path in glob.glob(os.path.join(proj, 'APP', '**', '*.c'), recursive=True) + \
            glob.glob(os.path.join(proj, 'User', '*.c')):
        if path.endswith('font_data.c'):
            continue
        src = open(path, encoding='utf-8', errors='ignore').read()
        src = re.sub(r'/\*.*?\*/', '', src, flags=re.S)      # 去掉注释
        src = re.sub(r'//[^\n]*', '', src)
        for lit in re.findall(r'"((?:[^"\\\n]|\\.)*)"', src):
            chars.update(ch for ch in lit if ord(ch) > 0x7E)
    return chars


def render(font_file, size, chars):
    font = ImageFont.truetype(os.path.join(FONT_DIR, font_file), size)
    ascent, descent = font.getmetrics()
    height = ascent + descent
    glyphs = []
    for ch in sorted(set(chars), key=ord):
        adv = max(1, round(font.getlength(ch)))
        img = Image.new('L', (adv, height), 0)
        ImageDraw.Draw(img).text((0, 0), ch, font=font, fill=255)
        px = img.load()
        data = bytearray()
        for y in range(height):
            row = [px[x, y] >> 4 for x in range(adv)]
            if len(row) % 2:
                row.append(0)
            for i in range(0, len(row), 2):
                data.append((row[i] << 4) | row[i + 1])
        glyphs.append((ord(ch), adv, bytes(data)))
    return height, glyphs


def main():
    proj = sys.argv[1]
    scanned = scan_chars(proj)
    out = ['/* 由 tools/gen_font.py 自动生成，请勿手工修改 */', '#include "font.h"', '']
    total = 0
    for name, file, size, charset in FONTS:
        chars = (ASCII + ''.join(scanned)) if charset == 'scan' else charset
        height, glyphs = render(file, size, chars)
        offsets, blob = [], bytearray()
        for code, w, data in glyphs:
            offsets.append(len(blob))
            blob += data
        total += len(blob)
        out.append(f'static const u8 {name}_bits[{len(blob)}] = {{')
        for i in range(0, len(blob), 24):
            out.append('\t' + ','.join(f'0x{b:02X}' for b in blob[i:i + 24]) + ',')
        out.append('};')
        out.append(f'static const glyph_t {name}_glyphs[{len(glyphs)}] = {{')
        for (code, w, _), off in zip(glyphs, offsets):
            out.append(f'\t{{ 0x{code:04X}, {w}, {off} }},')
        out.append('};')
        out.append(f'const font_t {name} = {{ {height}, {len(glyphs)}, {name}_glyphs, {name}_bits }};')
        out.append('')
        print(f'{name}: {file} {size}px, 行高 {height}, {len(glyphs)} 个字符, {len(blob)} 字节')
    dst = os.path.join(proj, 'APP', 'ui', 'font_data.c')
    open(dst, 'w', encoding='utf-8', newline='\r\n').write('\n'.join(out))
    print(f'非 ASCII 字符 {len(scanned)} 个: {"".join(sorted(scanned))}')
    print(f'字库合计 {total / 1024:.1f} KB -> {dst}')


if __name__ == '__main__':
    main()
