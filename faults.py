#!/usr/bin/env python3
"""Decode the ECU -> dash stream on pin 56 (TECH) into fault codes.

Listen-only: this script never writes to the port.
Prints a decoded block whenever the frame changes. Ctrl-C to stop.

Frame (7812.5 baud 8N1, every 0.5 s), as understood so far:
  byte 1      coolant sensor voltage, high 8 bits (FF = sensor open)
  byte 2      bits 7-6: coolant sensor voltage, low 2 bits
              bit 4: dealer mode (pin 22 grounded)
              bits 2,1: throttle-position mark, only in dealer mode
              bits 3,0: always set so far, meaning unknown
  byte 3-7    fault bits; only those in KNOWN below are identified
  byte 8      checksum: all 8 bytes sum to 0 mod 256
"""
import argparse
import math
import sys
import time

import serial

GAP = 0.05
# Fault bits confirmed on the bench: (byte number 1-8, bit 0-7) -> code.
# Add a line here each time a test identifies another bit.
KNOWN = {
    (3, 2): 'c31',
    (4, 2): 'c13',
    (4, 3): 'c14',
    (4, 4): 'c15',
    (4, 5): 'c21',
    (4, 6): 'c22',
    (4, 7): 'c23',
    (5, 6): 'c29',
}

NAMES = {
    'c11': 'Camshaft position sensor',
    'c12': 'Crankshaft position sensor',
    'c13': 'Intake air pressure sensor',
    'c14': 'Throttle position sensor',
    'c15': 'Engine coolant temperature sensor',
    'c21': 'Intake air temperature sensor',
    'c22': 'Atmospheric pressure sensor',
    'c23': 'Tip-over sensor',
    'c24': 'Ignition signal #1',
    'c25': 'Ignition signal #2',
    'c26': 'Ignition signal #3',
    'c27': 'Ignition signal #4',
    'c28': 'Secondary throttle valve actuator',
    'c29': 'Secondary throttle position sensor',
    'c31': 'Gear position signal',
    'c32': 'Injector signal #1',
    'c33': 'Injector signal #2',
    'c34': 'Injector signal #3',
    'c35': 'Injector signal #4',
    'c41': 'Fuel pump control system',
    'c42': 'Ignition switch signal (anti-theft)',
    'c46': 'Exhaust control valve actuator',
}
FAULT_BYTES = (3, 4, 5, 6, 7)               # byte numbers that carry fault bits


def fault_bits(frame):
    """Return (known codes, unidentified set bits)."""
    known, unknown = [], []
    for byte in FAULT_BYTES:
        for bit in range(8):
            if frame[byte - 1] >> bit & 1:
                code = KNOWN.get((byte, bit))
                if code:
                    known.append(code)
                else:
                    unknown.append(f'byte {byte} bit {bit}')
    return known, unknown


# Coolant: the raw 10-bit value is the voltage on pin 51 as a fraction of 5 V.
# The ECU pulls that pin up through about 1.8 k (fitted from 2.16 k and 5.04 k
# test resistors), so sensor resistance = PULLUP * raw / (1023 - raw).
PULLUP = 1796.0
# Service manual p. 5-8: sensor resistance (ohm) at temperature (C).
ECT_TABLE = [(2450, 20), (811, 50), (318, 80), (142, 110), (88, 130)]


def temp_c(r):
    """Sensor resistance -> degrees C, interpolating ln(R) against 1/T."""
    pts = [(math.log(ohm), 1 / (c + 273.15)) for ohm, c in ECT_TABLE]
    x = math.log(max(r, 1.0))
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
        if x >= x1:
            break
    y = y0 + (x - x0) * (y1 - y0) / (x1 - x0)      # extrapolates past either end
    return 1 / y - 273.15


def decode(frame):
    lines = []
    b1, b2, b3 = frame[0], frame[1], frame[2]
    raw = b1 << 2 | b2 >> 6
    flags = b2 & 0x3F

    mode = 'DEALER MODE ON' if flags & 0x10 else 'DEALER MODE OFF'
    lines.append(f'  mode     {mode}')

    if raw == 0x3FF:
        lines.append('  coolant  sensor open / no reading')
    else:
        r = PULLUP * raw / (1023 - raw)
        lines.append(f'  coolant  about {temp_c(r):.0f} C   (sensor {r:.0f} ohm, raw {raw})')

    if flags & 0x10:
        mark = {0b10: 'HIGH side (1.18 V and above)',
                0b01: 'LOW side (1.06 V and below)',
                0b11: 'MIDDLE - correct position (seen at 1.12 V)',
                0b00: 'no mark bits set - not seen in dealer mode before'}[flags >> 1 & 3]
        lines.append(f'  throttle {mark}')
    other = flags & ~0x16
    if other != 0x09:
        lines.append(f'  NOTE     byte 2 flag bits changed: {other:#04x} (0x09 until now)')

    known, unknown = fault_bits(frame)
    if not known and not unknown:
        lines.append('  faults   none (c00)')
    for code in known:
        lines.append(f'  fault    {code}  {NAMES[code]}')
    if unknown:
        lines.append(f'  not yet identified: {", ".join(unknown)}')
    return lines


def frames(ser):
    """Yield valid 8-byte frames; resync on anything that fails the checksum."""
    buf, last = b'', None
    while True:
        d = ser.read(256)
        now = time.time()
        if d:
            buf += d
            last = now
        elif buf and now - last > GAP:
            # take the newest 8 bytes that form a valid frame
            tail = buf[-8:]
            if len(tail) == 8 and sum(tail) & 0xFF == 0:
                yield tail
            else:
                yield None
            buf = b''


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('-p', '--port', default='/dev/ttyACM0')
    ap.add_argument('-b', '--baud', type=int, default=7812)
    a = ap.parse_args()

    ser = serial.Serial(a.port, a.baud, timeout=0.005)
    ser.reset_input_buffer()
    print(f'Listening on {a.port} at {a.baud} baud (Ctrl-C to stop)')
    print()

    prev, bad, seen = None, 0, 0
    try:
        for f in frames(ser):
            if f is None:
                bad += 1
                continue
            seen += 1
            # ignore the 1-count flicker in the temperature's low bits
            key = bytes([f[0], f[1] & 0x3F]) + f[2:7]
            if key == prev:
                continue
            prev = key
            print(f'{time.strftime("%H:%M:%S")}  {f.hex(" ").upper()}')
            print('\n'.join(decode(f)))
            print()
    except KeyboardInterrupt:
        pass
    finally:
        print(f'{seen} good frames, {bad} bad')
        try:
            ser.close()
        except KeyboardInterrupt:
            pass


if __name__ == '__main__':
    main()
