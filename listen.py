#!/usr/bin/env python3
"""Listen-only capture from the Uno in USB-serial mode (RESET->GND).
Never writes to the port. Usage: listen.py [baud ...]  (default: sweep)"""
import serial, sys, time, collections

PORT = '/dev/ttyACM0'
SWEEP = [300, 600, 1200, 2400, 4800, 7812, 9600, 10400, 15625, 19200, 31250, 38400, 57600, 115200]
SECS = 3.0

def capture(baud, secs=SECS):
    s = serial.Serial(PORT, baud, timeout=0.005)
    s.reset_input_buffer()
    chunks, t0 = [], time.time()
    while time.time() - t0 < secs:
        d = s.read(4096)
        if d:
            chunks.append((time.time() - t0, d))
    s.close()
    return chunks

def report(baud, chunks):
    data = b''.join(d for _, d in chunks)
    print(f'--- {baud} baud: {len(data)} bytes in {SECS:.0f}s, {len(chunks)} chunks')
    if not data:
        return data
    top = collections.Counter(data).most_common(8)
    print('   distinct:', len(set(data)), ' top:', ' '.join(f'{b:02X}x{n}' for b, n in top))
    # bursts: gaps > 20 ms between chunks
    bursts, cur, last = [], b'', None
    for t, d in chunks:
        if last is not None and t - last > 0.02:
            bursts.append(cur); cur = b''
        cur += d; last = t
    bursts.append(cur)
    print('   bursts:', len(bursts), ' lengths:', [len(b) for b in bursts[:20]])
    for b in bursts[:6]:
        print('   ', b[:48].hex(' '))
    return data

if __name__ == '__main__':
    bauds = [int(a) for a in sys.argv[1:]] or SWEEP
    for b in bauds:
        try:
            data = report(b, capture(b))
            if data:
                open(f'/home/tendai/Projects/GSXR-ECU/K1-K2/captures/tech56_{b}.bin', 'wb').write(data)
        except Exception as e:
            print(f'--- {b} baud: ERROR {e}')
