"""在电脑上扮演 ESP32，按 LingXi 帧协议与 STM32 通信（测试用）

用法: python link_test.py COM7
需要 STM32 固件以 LINK_ON_USART1 编译（协议走 CH340），调试文本会混在其中，脚本会自动跳过。
"""
import json, struct, sys, time, serial

CMD_PING, CMD_GET_CAPS, CMD_SET, CMD_GET_STATE, CMD_EVENT = 0x01, 0x02, 0x03, 0x04, 0x40


def crc16(data, crc=0xFFFF):
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def build(cmd, seq, payload=b''):
    body = struct.pack('<HBB', len(payload), cmd, seq) + payload
    return b'\xAA\x55' + body + struct.pack('<H', crc16(body))


class Link:
    def __init__(self, port):
        self.s = serial.Serial()
        self.s.port, self.s.baudrate, self.s.timeout = port, 115200, 0.05
        self.s.dtr = self.s.rts = False
        self.s.open()
        self.buf = bytearray()
        self.seq = 0
        self.text = bytearray()

    def _frames(self):
        self.buf += self.s.read(512)
        while True:
            i = self.buf.find(b'\xAA\x55')
            if i < 0:
                self.text += self.buf[:-1] if self.buf.endswith(b'\xAA') else self.buf
                del self.buf[:max(len(self.buf) - 1, 0) if self.buf.endswith(b'\xAA') else len(self.buf)]
                return
            self.text += self.buf[:i]
            del self.buf[:i]
            if len(self.buf) < 8:
                return
            ln, cmd, seq = struct.unpack_from('<HBB', self.buf, 2)
            if ln > 1024:
                del self.buf[:2]; continue
            if len(self.buf) < 8 + ln:
                return
            body = bytes(self.buf[2:6 + ln])
            crc, = struct.unpack_from('<H', self.buf, 6 + ln)
            del self.buf[:8 + ln]
            if crc16(body) == crc:
                yield cmd, seq, body[4:]

    def request(self, cmd, payload=b'', timeout=0.5):
        self.seq = (self.seq + 1) & 0xFF
        t0 = time.time()
        self.s.write(build(cmd, self.seq, payload))
        while time.time() - t0 < timeout:
            for c, sq, data in self._frames():
                if c == (cmd | 0x80) and sq == self.seq:
                    return data, (time.time() - t0) * 1000
                if c == CMD_EVENT:
                    print('  [EVENT]', data.decode())
        raise TimeoutError(f'cmd 0x{cmd:02X} 无应答')

    def set(self, dev_id, value):
        data, ms = self.request(CMD_SET, struct.pack('<Bh', dev_id, value))
        st, did, val = struct.unpack('<BBh', data)
        return st, val, ms


def main():
    ln = Link(sys.argv[1])
    time.sleep(0.3); list(ln._frames()); ln.text.clear()

    data, ms = ln.request(CMD_PING)
    print(f'PING  -> uptime {struct.unpack("<I", data)[0]} ms   ({ms:.1f} ms)')

    data, ms = ln.request(CMD_GET_CAPS)
    caps = json.loads(data)
    print(f'CAPS  -> {len(data)} 字节 ({ms:.1f} ms)')
    for d in caps['devices']:
        print(f"   id={d['id']} {d['name']:<14} {d['type']:<6} {d['desc']}")

    for d in caps['devices']:
        if d['name'] == 'alarm':
            continue
        st, val, ms = ln.set(d['id'], 1)
        print(f"SET {d['name']}=1 -> status {st}, value {val} ({ms:.1f} ms)")
        time.sleep(0.6)
    st, val, ms = ln.set(99, 1)
    print(f'SET 不存在的设备 99 -> status {st} (期望 1)')
    st, val, ms = ln.set(1, 5)
    print(f'SET 越界值 5     -> status {st} (期望 2)')

    data, ms = ln.request(CMD_GET_STATE)
    print(f'STATE -> {data.decode()}')

    # 压力测试：100 次 PING，统计成功率与时延
    ok, lat = 0, []
    for _ in range(100):
        try:
            _, ms = ln.request(CMD_PING, timeout=0.3); ok += 1; lat.append(ms)
        except TimeoutError:
            pass
    print(f'压力测试 PING x100: 成功 {ok}/100, 平均 {sum(lat)/len(lat):.1f} ms, 最大 {max(lat):.1f} ms')

    for d in caps['devices']:
        ln.set(d['id'], 0)
    print('已全部关闭。STM32 调试输出：')
    print('  ' + ln.text.decode('utf-8', 'replace').strip().replace('\n', '\n  ')[-600:])


if __name__ == '__main__':
    main()
