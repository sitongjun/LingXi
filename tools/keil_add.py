"""向 Keil uvprojx 工程添加源文件 / 头文件路径

用法: python keil_add.py <工程.uvprojx> <分组名> <源文件...> [--inc 路径;路径]
例:   python keil_add.py Template.uvprojx APP .\\APP\\lcd\\lcd.c --inc .\\APP\\lcd
"""
import ntpath
import re
import sys


def main():
    args = sys.argv[1:]
    inc = None
    if '--inc' in args:
        i = args.index('--inc')
        inc = args[i + 1]
        del args[i:i + 2]
    proj, group, files = args[0], args[1], args[2:]
    s = open(proj, encoding='utf-8').read()

    for f in files:
        if f'<FilePath>{f}</FilePath>' in s:
            continue
        m = re.search(rf'(<GroupName>{re.escape(group)}</GroupName>\s*<Files>)', s)
        if not m:
            sys.exit(f'找不到分组 {group}')
        entry = (f"\n            <File>\n              <FileName>{ntpath.basename(f)}</FileName>\n"
                 f"              <FileType>1</FileType>\n              <FilePath>{f}</FilePath>\n            </File>")
        s = s[:m.end()] + entry + s[m.end():]
        print('添加文件', f)

    if inc:
        m = re.search(r'(<Cads>.*?<IncludePath>)(.*?)(</IncludePath>)', s, re.S)
        paths = m.group(2).split(';')
        for p in inc.split(';'):
            if p not in paths:
                paths.append(p)
                print('添加头文件路径', p)
        s = s[:m.start(2)] + ';'.join(paths) + s[m.end(2):]

    open(proj, 'w', encoding='utf-8').write(s)


if __name__ == '__main__':
    main()
