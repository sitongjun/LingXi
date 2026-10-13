"""STM32F1 串口 ISP 下载/监视工具（适配普中一键下载电路：RTS->BOOT0，DTR->复位）

用法:
  python stm32_isp.py COM7 flash  firmware.hex   # 擦除+写入+校验+运行
  python stm32_isp.py COM7 info                  # 读取芯片 ID / Flash 容量
  python stm32_isp.py COM7 monitor [秒数]         # 运行用户程序并打印 USART1 输出
"""
import sys, time, serial

ACK, NACK = 0x79, 0x1F


def parse_hex(path):
    mem, base = {}, 0
    for line in open(path):
        line = line.strip()
        if not line.startswith(':'):
            continue
        b = bytes.fromhex(line[1:])
        n, addr, typ, data = b[0], (b[1] << 8) | b[2], b[3], b[4:4 + b[0]]
        if typ == 0x00:
            for i, v in enumerate(data):
                mem[base + addr + i] = v
        elif typ == 0x04:
            base = ((data[0] << 8) | data[1]) << 16
        elif typ == 0x02:
            base = ((data[0] << 8) | data[1]) << 4
        elif typ == 0x01:
            break
    start, end = min(mem), max(mem) + 1
    img = bytearray(b'\xff' * (end - start))
    for a, v in mem.items():
        img[a - start] = v
    while len(img) % 4:
        img.append(0xFF)
    return start, bytes(img)


class Isp:
    def __init__(self, port):
        self.s = serial.Serial()
        self.s.port, self.s.baudrate = port, 115200
        self.s.parity, self.s.timeout = serial.PARITY_EVEN, 1
        self.s.dtr = self.s.rts = False
        self.s.open()

    def reset(self, boot):
        self.s.rts = boot          # rts=True -> BOOT0=1
        self.s.dtr = True          # 拉低复位
        time.sleep(0.1)
        self.s.dtr = False
        time.sleep(0.15)

    def ack(self, timeout=1):
        self.s.timeout = timeout
        r = self.s.read(1)
        self.s.timeout = 1
        if r != bytes([ACK]):
            raise RuntimeError(f'no ACK (got {r!r})')

    def cmd(self, c, timeout=1):
        self.s.write(bytes([c, c ^ 0xFF]))
        self.ack(timeout)

    def enter(self):
        for _ in range(3):
            self.reset(True)
            self.s.reset_input_buffer()
            self.s.write(b'\x7f')
            if self.s.read(1) in (bytes([ACK]), bytes([NACK])):
                return
        raise RuntimeError('无法进入 bootloader，检查 COM 口 / 一键下载跳线')

    def addr(self, a):
        b = a.to_bytes(4, 'big')
        self.s.write(b + bytes([b[0] ^ b[1] ^ b[2] ^ b[3]]))
        self.ack()

    def read(self, a, n):
        self.cmd(0x11); self.addr(a)
        self.s.write(bytes([n - 1, (n - 1) ^ 0xFF])); self.ack()
        return self.s.read(n)

    def get_id(self):
        self.cmd(0x02)
        d = self.s.read(3); self.ack()
        return (d[1] << 8) | d[2]

    def erase_all(self):
        self.cmd(0x43)
        self.s.write(b'\xff\x00')
        self.ack(timeout=40)

    def write(self, a, data):
        self.cmd(0x31); self.addr(a)
        n = len(data) - 1
        cs = n
        for v in data:
            cs ^= v
        self.s.write(bytes([n]) + data + bytes([cs]))
        self.ack(timeout=2)

    def run(self):
        self.s.rts = False
        self.reset(False)


def main():
    port, action = sys.argv[1], sys.argv[2]
    isp = Isp(port)
    if action in ('info', 'flash'):
        isp.enter()
        pid = isp.get_id()
        kb = int.from_bytes(isp.read(0x1FFFF7E0, 2), 'little')
        print(f'芯片 PID=0x{pid:03X}  Flash={kb}KB')
    if action == 'flash':
        start, img = parse_hex(sys.argv[3])
        print(f'镜像 0x{start:08X}  {len(img)} 字节，擦除中...')
        isp.erase_all()
        t = time.time()
        for off in range(0, len(img), 256):
            isp.write(start + off, img[off:off + 256])
            print(f'\r写入 {min(off + 256, len(img))}/{len(img)}', end='')
        print(f'  ({time.time() - t:.1f}s)')
        for off in range(0, len(img), 256):
            chunk = img[off:off + 256]
            if isp.read(start + off, len(chunk)) != chunk:
                raise RuntimeError(f'校验失败 @0x{start + off:08X}')
        print('校验通过')
    if action in ('info', 'flash', 'monitor'):
        isp.run()
    if action == 'monitor' or (action == 'flash' and len(sys.argv) > 4):
        secs = float(sys.argv[3] if action == 'monitor' and len(sys.argv) > 3 else
                     sys.argv[4] if action == 'flash' else 5)
        isp.s.close()
        m = serial.Serial(); m.port, m.baudrate, m.timeout = port, 115200, 0.2
        m.dtr = m.rts = False; m.open()
        if action == 'monitor':
            m.dtr = True; time.sleep(0.1); m.dtr = False
        end = time.time() + secs
        while time.time() < end:
            d = m.read(256)
            if d:
                sys.stdout.write(d.decode('utf-8', 'replace')); sys.stdout.flush()
        m.close()
    print('\n完成')


if __name__ == '__main__':
    main()
