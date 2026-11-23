#!/usr/bin/env python3
"""Listen-only raw capture: rawlisten.py BAUD SECONDS OUTFILE. Never writes to the port."""
import sys, time, serial
baud, secs, out = int(sys.argv[1]), float(sys.argv[2]), sys.argv[3]
ser = serial.Serial('/dev/ttyACM0', baud, timeout=0.02)
ser.reset_input_buffer()
t0 = time.time(); total = 0
with open(out, 'wb') as f:
    while time.time() - t0 < secs:
        d = ser.read(256)
        if d:
            total += len(d); f.write(d)
            print(f'{time.time()-t0:7.2f}s  {len(d):3d} bytes  {d[:24].hex(" ")}', flush=True)
ser.close()
print(f'total {total} bytes in {secs:.0f} s at {baud} baud')
