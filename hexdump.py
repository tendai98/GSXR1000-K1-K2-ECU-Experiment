#!/usr/bin/env python3
"""Live hexdump of the ECU -> dash stream on pin 56 (TECH).

Listen-only: this script never writes to the port.
Uno in USB-serial mode (RESET -> GND), ECU line on Uno pin 1 via ~1 k.

Usage:
  hexdump.py                 show every frame
  hexdump.py -c              show a frame only when it differs from the last
  hexdump.py -l run.log      also append the output to a file
  hexdump.py -b 9600 -p /dev/ttyUSB0
Ctrl-C to stop.
"""
import argparse
import sys
import time

import serial

GAP = 0.05          # silence (s) that ends a frame; frames arrive every 0.5 s
RED, DIM, OFF = '\033[1;31m', '\033[2m', '\033[0m'


def fmt(frame, prev, color):
    out = []
    for i, b in enumerate(frame):
        changed = prev is not None and (i >= len(prev) or prev[i] != b)
        out.append(f'{RED}{b:02X}{OFF}' if changed and color else f'{b:02X}')
    return ' '.join(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('-p', '--port', default='/dev/ttyACM0')
    ap.add_argument('-b', '--baud', type=int, default=7812)
    ap.add_argument('-c', '--changes', action='store_true',
                    help='only print frames that differ from the previous one')
    ap.add_argument('-l', '--log', help='append plain-text output to this file')
    a = ap.parse_args()

    color = sys.stdout.isatty()
    log = open(a.log, 'a') if a.log else None
    ser = serial.Serial(a.port, a.baud, timeout=0.005)
    ser.reset_input_buffer()
    print(f'Listening on {a.port} at {a.baud} baud (Ctrl-C to stop)')
    print('    time      n  frame                      sum  note')

    buf, last_rx, prev, count, same = b'', None, None, 0, 0
    t0 = time.time()

    def emit(frame):
        nonlocal prev, count, same
        count += 1
        ok = sum(frame) & 0xFF == 0
        note = '' if ok else 'bad checksum'
        if len(frame) != 8:
            note = f'{len(frame)} bytes (expected 8)'
            ok = False
        if len(frame) > 16:
            note += ' - line held low / noise?'
        is_new = frame != prev
        if a.changes and not is_new:
            same += 1
            return
        if a.changes and same:
            print(f'{DIM if color else ""}             ... repeated {same} more times{OFF if color else ""}')
            same = 0
        stamp = time.time() - t0
        shown = frame[:16]
        tail = ' ..' if len(frame) > 16 else ''
        mark = 'ok ' if ok else '!! '
        print(f'{stamp:8.2f} {count:6d}  {fmt(shown, prev, color)}{tail:<3}    {mark}  {note}')
        if log:
            log.write(f'{time.strftime("%H:%M:%S")} {stamp:8.2f} {frame.hex(" ")} {mark.strip()} {note}\n')
            log.flush()
        prev = frame

    try:
        while True:
            d = ser.read(256)
            now = time.time()
            if d:
                buf += d
                last_rx = now
            elif buf and now - last_rx > GAP:
                # Backlog buffered before we started arrives as one lump:
                # split it if it is a clean run of valid 8-byte frames.
                parts = [buf[i:i + 8] for i in range(0, len(buf), 8)]
                if len(buf) > 8 and len(buf) % 8 == 0 and all(sum(p) & 0xFF == 0 for p in parts):
                    for p in parts:
                        emit(p)
                else:
                    emit(buf)
                buf = b''
    except KeyboardInterrupt:
        print(f'\n{count} frames in {time.time() - t0:.0f} s')
    finally:
        ser.close()
        if log:
            log.close()


if __name__ == '__main__':
    main()
