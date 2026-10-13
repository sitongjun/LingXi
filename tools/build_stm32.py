"""用 ARM GCC 编译 Keil 工程（绕开 MDK-Lite 的 32KB 限制）

源文件、头文件路径、宏定义全部从 .uvprojx 中读取，Keil 工程仍是唯一的配置来源；
Keil 的汇编启动文件换成 stm32/gcc 下的 C 版本，并补上 newlib 系统调用。

用法: python build_stm32.py <工程目录> [--flash COM7]
输出: <工程目录>/build_gcc/LingXi.hex
"""
import os
import re
import subprocess
import sys
import time

GCC_DIR = r'D:\tools\arm-gnu-toolchain\bin'
HERE = os.path.dirname(os.path.abspath(__file__))
GCC_SUPPORT = os.path.join(HERE, '..', 'stm32', 'gcc')

# GCC 下不需要/不兼容的文件
SKIP = {'core_cm3.c'}


def tool(name):
    return os.path.join(GCC_DIR, 'arm-none-eabi-' + name)


def parse_uvprojx(proj_dir):
    path = next(os.path.join(proj_dir, f) for f in os.listdir(proj_dir) if f.endswith('.uvprojx'))
    s = open(path, encoding='utf-8').read()
    cads = re.search(r'<Cads>(.*?)</Cads>', s, re.S).group(1)
    incs = re.search(r'<IncludePath>(.*?)</IncludePath>', cads).group(1).split(';')
    defs = re.search(r'<Define>(.*?)</Define>', cads).group(1).split(',')
    files = re.findall(r'<FileType>1</FileType>\s*<FilePath>(.*?)</FilePath>', s)
    norm = lambda p: os.path.normpath(os.path.join(proj_dir, p.replace('\\', os.sep)))
    srcs = [norm(f) for f in files if os.path.basename(f) not in SKIP]
    srcs += [os.path.join(GCC_SUPPORT, 'startup_stm32f10x_hd.c'), os.path.join(GCC_SUPPORT, 'syscalls.c')]
    return [norm(i) for i in incs if i], [d.strip() for d in defs if d.strip()], srcs


def main():
    proj = os.path.abspath(sys.argv[1])
    out_dir = os.path.join(proj, 'build_gcc')
    os.makedirs(out_dir, exist_ok=True)
    incs, defs, srcs = parse_uvprojx(proj)

    cflags = ['-mcpu=cortex-m3', '-mthumb', '-Os', '-g', '-std=gnu99',
              '-ffunction-sections', '-fdata-sections', '-Wall', '-Wno-unused-function',
              '-Wno-pointer-sign', '-Wno-char-subscripts']
    cflags += ['-D' + d for d in defs] + ['-I' + i for i in incs]

    objs, failed = [], False
    t0 = time.time()
    for src in srcs:
        obj = os.path.join(out_dir, os.path.splitext(os.path.basename(src))[0] + '.o')
        objs.append(obj)
        if os.path.exists(obj) and os.path.getmtime(obj) > os.path.getmtime(src) and \
                all(os.path.getmtime(obj) > os.path.getmtime(os.path.join(d, h))
                    for d in incs if os.path.isdir(d) for h in os.listdir(d) if h.endswith('.h')):
            continue
        r = subprocess.run([tool('gcc')] + cflags + ['-c', src, '-o', obj], capture_output=True, text=True,
                           encoding='utf-8', errors='replace')
        msg = (r.stdout + r.stderr).strip()
        if msg:
            print(msg)
        if r.returncode:
            failed = True
    if failed:
        sys.exit('编译失败')

    elf = os.path.join(out_dir, 'LingXi.elf')
    hexf = os.path.join(out_dir, 'LingXi.hex')
    ld = os.path.join(GCC_SUPPORT, 'stm32f103ze.ld')
    r = subprocess.run([tool('gcc'), '-mcpu=cortex-m3', '-mthumb', '-T', ld, '-Wl,--gc-sections',
                        '-Wl,-Map=' + os.path.join(out_dir, 'LingXi.map'),
                        '--specs=nano.specs', '-o', elf] + objs + ['-lc', '-lm', '-lgcc'],
                       capture_output=True, text=True, encoding='utf-8', errors='replace')
    if r.returncode:
        print(r.stdout + r.stderr)
        sys.exit('链接失败')
    subprocess.run([tool('objcopy'), '-O', 'ihex', elf, hexf], check=True)
    size = subprocess.run([tool('size'), elf], capture_output=True, text=True).stdout.splitlines()[-1].split()
    text, data, bss = int(size[0]), int(size[1]), int(size[2])
    print(f'编译完成 {time.time() - t0:.1f}s：Flash {(text + data) / 1024:.1f} KB / 512 KB，'
          f'RAM {(data + bss) / 1024:.1f} KB / 64 KB -> {hexf}')

    if '--flash' in sys.argv:
        port = sys.argv[sys.argv.index('--flash') + 1]
        subprocess.run([sys.executable, os.path.join(HERE, 'stm32_isp.py'), port, 'flash', hexf, '3'])


if __name__ == '__main__':
    main()
