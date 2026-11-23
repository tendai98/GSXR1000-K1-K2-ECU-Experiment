# Suzuki GSX-R1000 K1/K2 ECU – Bench Reverse-Engineering Notes

Work log for powering, probing and communicating with a 2001–2002 Suzuki
GSX-R1000 engine control unit on the bench.

**Status (23 Nov 2026):**

- **Done:** the ECU's diagnostic stream to the dash (pin 56) is captured
  and largely decoded: baud rate, framing, checksum, coolant reading,
  dealer-mode flag, throttle-position mark and eight fault codes. Every
  harness input that can be exercised with 5 V or a meter has been
  tested (section 6).
- **K1 unit dead (found 23 Nov):** the 5 V sensor supply (pin 48) is
  shorted to ground inside the ECU, 2.3 Ω both ways (6.16). The case
  cannot be opened, so it is not repairable here.
- **Mix-up corrected:** from the burnt resistors on 22 Nov until 23 Nov a
  K3 ECU was on the bench by mistake. Everything measured on it has been
  moved to `../K3-K4/captures/session_2026-11-22_K3.md`; this README now
  holds K1 data only.
- **Scope:** firmware and map reading (6.17, section 10) is not being
  pursued. The project is diagnostics through the pin 56 stream.
- **Next:** see section 11.

---

## 1. Unit identification

| Label marking   | Meaning                                                        |
|-----------------|----------------------------------------------------------------|
| 32920-40F30     | Suzuki part number. 32920 = FI control unit, 40F = 2001–02 GSX-R1000 |
| 112100-0820     | Denso's own part number (Denso manufactured the ECU)           |
| 12V NEPO6J      | Voltage rating plus a Denso production/lot code                |

- Listed as the FI control unit in the 2002 GSX-R1000 parts diagram.
- Related numbers for the same bike: 32920-40F00 (Denso 112100-0790),
  32920-40FD0 (Denso 112100-1210), 32920-40FA0 (Denso 112100-1180).
- Not interchangeable with 2003+ GSX-R1000 ECUs.
- Service manual (p. 4-14): CPU, ROM and I/O sections; eight fuel maps
  are programmed in the ROM.
- Processor, per mr_hollywood's site (not yet confirmed on this unit):
  a Motorola CPU16 microcontroller, either **MC68HC916R3** (flash, can be
  read and reflashed over the K-line) or **MC68HC16R3** (mask ROM, never
  reprogrammable, silent on the K-line). Denso fitted both and the label
  does not say which; each unit has to be tested.
- Memory on the flash version: 96 KB program at $00000–$17FFF (fuel and
  timing maps in the last 32 KB, $10000–$17FFF), 2 KB bootloader at
  $90000, and a separate 8-pin non-volatile memory holding 512 bytes of
  fuel-trim (Yoshbox/Teka) settings.
- His list of ECUs already dumped includes **40F30-0820, this exact
  unit**. Whether that one was a flashable example is not readable from
  the page text.
- The board has a BDM debug port on its top edge; reaching it means
  cutting a notch in the case. That is the fallback read route for a
  mask-ROM unit (his "Nano-BDM", built on an Arduino Nano).
- No commercial flash kit exists for 2001–02 (kits are sold for 2003–04,
  2005–06 and 2007 onward).
- The 2005–06 forum claim that the factory pins can write but not read
  does **not** apply here: on this family the bootloader reads without
  erasing (6.17).
- Case: solid plastic shell with rubbery gel in front of the connectors.
  How it opens is not yet known; photograph all six sides first.

---

## 2. Safety rules (read first)

1. **Never put 12 V on pin 29.** It is the flash programming-voltage
   pin, used only during erase/flash. mr_hollywood: it must come from a
   regulated 12 V supply, and 0.5 V too much destroys the ECU. Reading
   does not use it at all. Leave it unconnected.
2. **Leave pin 32 alone.** Not used by the published method; function
   unknown.
3. **Pin 30 is the bootloader enable.** Grounded at power-up, the ECU
   runs its bootloader and not the engine program. Published as a 5 V
   pin with an internal pull-up that needs about 1.2 kΩ or less to
   ground. Our early load test suggested a much stiffer source (~35 Ω),
   so pull it down through 1 kΩ and meter it before using a plain link.
3a. **Never let 12 V touch pin 21 (map select).** mr_hollywood: a 12 V
   short there destroys the ECU. Pin 21 sits in the same connector as
   pin 28, where 12 V was applied on 22 Nov.
4. **Never connect a 5 V USB-TTL adapter directly to pin 31.** It is a
   12 V line.
5. **All grounds common:** ECU pin 35, the 12 V supply negative and the
   USB adapter / Arduino GND must be joined.
6. **Check supply polarity with a meter before connecting.** The manual
   (p. 4-3) says reverse polarity damages the FI system instantly.
7. **No writing until a backup of the firmware or a known stock binary
   for 32920-40F30 exists.**
8. **Listen before driving.** Everything decoded so far came from
   listening on pin 56 and putting resistors on sensor inputs. Nothing
   has been transmitted to the ECU on that line, or on pin 31.
9. **12 V on an input only through a resistor (9.87 kΩ used so far),
   and count the pin twice.** Pin 28 sits between pin 27 (ground) and
   pin 29 (never 12 V).
10. **Meter every resistor and every pin before trusting a result.**
    Mislabelled resistors and a probe one pin off have each cost a test.
11. **Supply resistor must be rated 2 W or more.** The ECU draws about
    190 mA, which is 0.36 W in the 10 Ω; ¼ W parts burn.

---

## 3. Connectors and pin numbering

- Two connectors: 34-pin and 26-pin, 4-row TE Connectivity style
  (used on GSX-R1000 through 2004).
- Numbering runs continuously: pins 1–34 on the large plug, 35–60 on the
  small plug.
- Row layout: 1–9 / 10–17 / 18–25 / 26–34 and 35–41 / 42–47 / 48–53 /
  54–60.
- Numbering **verified**: continuity between ground pins (6.1), the ECU
  powering up from pin 17, and every sensor-input test in 6.8 landing on
  the pin the manual names.
- Full 60-terminal table with wire colours: `gsxr1000-ecm-pinout.html`
  (and `.pdf`), built from the service manual.

---

## 4. Pin map (pins used or measured so far)

| Pin | Label | Function | Measured / observed | Status |
|-----|-------|----------|---------------------|--------|
| 15  | ?     | 7-way coupler line (Bl/R) | 4.939 V open; 3.258 V with 9.87 kΩ load → ~5.1 kΩ pull-up to 5 V | Pulled-up line, most likely an input |
| 16  | ?     | 7-way coupler line (Bl/Y) | 4.939 V open; 3.258 V with 9.87 kΩ load → ~5.1 kΩ pull-up to 5 V | Pulled-up line, most likely an input |
| 17  | +B    | +12 V power input (switched) | 1.1 MΩ to GND unpowered; ECU powers up | Confirmed |
| 18  | E02   | Ground | ~1 Ω to pin 35 | Confirmed |
| 19  | CLT   | Clutch lever switch | Linked to pin 35, live and after power cycle: no change in the stream | Not reported in the stream |
| 20  | NT    | Neutral signal | Linked to pin 35, live and after power cycle: no change in the stream | Not reported in the stream |
| 21  | MS    | **Map select**: grounded = map set MS0, open = MS1 (per mr_hollywood; stock maps are identical). 12 V here destroys the ECU | 4.939 V open; 3.263 V with 9.87 kΩ load → ~5.1 kΩ pull-up to 5 V; grounding it changes nothing in the stream | Input; floats high on the bench unless linked to pin 35 |
| 22  | TS    | Dealer-mode select (ground to enable) | Sets two bits in the pin 56 stream | Confirmed |
| 23  | ?     | 7-way coupler line (Bl/W) | 4.358 V open; 0.032 V with 9.87 kΩ load → ~1.3 MΩ source | High impedance, not driven |
| 24  | ?     | 7-way coupler line (Bl/B) | 4.357 V open; 0.031 V with 9.87 kΩ load → ~1.3 MΩ source | High impedance, not driven |
| 25  | ?     | 7-way coupler line (Bl/G) | 4.358 V open; 0.032 V with 9.87 kΩ load → ~1.3 MΩ source | High impedance, not driven |
| 26  | E03   | Ground per manual | 7.3 kΩ to pin 35 | **Anomaly** – should be a hard ground |
| 27  | E01   | Ground | ~1 Ω to pin 35 | Confirmed |
| 28  | STA   | Starter button signal (12 V when pressed) | 2.439 V open; 1.731 V with 9.87 kΩ to ground; 10.21 V when fed from 12 V through 9.87 kΩ (~0.2 mA) | Input confirmed; c11/c12 test not completed (6.14) |
| 29  | –     | **Program**: flash programming voltage, 12 V regulated during erase/flash only (per mr_hollywood) | 4.933 V idle; ~520 Ω source | Leave unconnected |
| 30  | –     | **OBD enable**: ground at power-up to start the bootloader (per mr_hollywood) | 4.919 V idle; early load test suggested ~35 Ω source, which conflicts with a simple pull-up | Re-measure through 1 kΩ before grounding |
| 31  | –     | 12 V-level line, no harness wire | 9.3 V idle on the meter | **K-line** of the bootloader, 9600 baud 8N1 (per mr_hollywood); only active with pin 30 grounded at power-up |
| 32  | –     | FWE / write enable (believed), no harness wire | 0.015 V | Leave untouched |
| 33  | ?     | Strap tied to sensor ground in the harness (position read off the drawing) | 4.939 V open; 3.256 V with 9.87 kΩ load → ~5.1 kΩ pull-up to 5 V; grounding it changes nothing in the stream | Input; floats high on the bench unless linked to pin 35 |
| 35  | E1    | Ground (main) | Reference | Confirmed |
| 38  | MPS   | Exhaust valve position sensor | 0 V open; 2.475 V from a divider barely loaded by the pin; no change in the stream after power cycle | High-impedance input; not reported on a bare bench |
| 42  | VM    | 12 V from fuel pump relay (supply monitor) | 0 V open | Input, pulled low; **not yet tested** (c41) |
| 45  | STP   | Secondary throttle position sensor | 2.5 V clears c29 | Confirmed |
| 48  | VCC   | 5 V sensor supply | Used as 5 V source for dividers | Confirmed |
| 49  | VTA   | Throttle position sensor | 2.5 V clears c14 | Confirmed |
| 50  | THA   | Intake air temperature sensor | 1 kΩ to pin 54 clears c21 | Confirmed |
| 51  | THW   | Coolant temperature sensor | ~1.8 kΩ internal pull-up to 5 V | Confirmed, calibrated |
| 52  | PA    | Atmospheric pressure sensor | 2.5 V clears c22 | Confirmed |
| 54  | E2    | Sensor ground (return for all sensors) | – | Confirmed |
| 56  | TECH  | **Serial data to the dash** | 7812.5 baud stream, see section 7; meter reads 4.8–4.9 V | Confirmed, 5 V logic level |
| 57  | GP    | Gear position signal | 4.942 V open, and the same with 10 kΩ to pin 48 | Input, pulled up to 5 V internally |
| 58  | PM    | Intake air pressure sensor | 2.5 V clears c13 | Confirmed |
| 59  | DON   | Tip-over sensor | 4.9 V open; 0.8 V from a divider (pin 48 – 5.04 kΩ – pin 59 – 1 kΩ – pin 54) clears c23; a true 1 kΩ to pin 54 alone gives 0 V and does not | Confirmed |
| 60  | AT    | Probably anti-theft / ignition switch signal | 9 V open, 8.43 V with 9.87 kΩ to ground | Was pulled up to about 9 V through roughly 0.7 kΩ; function inferred from code c42 remark. |


### Pins 29–32

The bike harness has **no wire** on pins 29, 30, 31 or 32, and the manual
never mentions them. They are a factory or bench port, not part of normal
diagnostics. Forum descriptions conflict:

| Pin | Scheme A (5 V boot serial)       | Scheme B (K-line)                       |
|-----|----------------------------------|-----------------------------------------|
| 29  | ECU TX → adapter RX              | Programming pin, 12 V erases ECU        |
| 30  | ECU RX ← adapter TX              | Ground it to enter read/write mode      |
| 31  | –                                | K-line                                  |
| 32  | FWE                              | Unknown                                 |

**Scheme B is the right one.** mr_hollywood's adaptor drawing
(`reference/mr_hollywood/kline_obd_adaptor_wiring.jpg`), which lists
32920-40Fxx GSX-R1000 among its ECU models, gives:

| ECU pin | Role | Goes to |
|---------|------|---------|
| 17 | +12 V | supply positive, and OBD pin 16 |
| 18 | Ground | supply negative, and OBD pin 4 |
| 30 | OBD enable | ground |
| 31 | K-line | OBD pin 7 |
| 29 | Program | +12 V, only during flash/erase |

Pin 32 is not used. The tests in 6.5, which transmitted on pin 29 and
listened on pin 30, were aimed at the wrong pins.

### The 7-way coupler beside the ECM

Traced on the '02 harness diagram (manual PDF p. 462). An unlabelled
7-way coupler next to the ECM carries:

- five ECM lines: pins 15 (Bl/R), 16 (Bl/Y), 23 (Bl/W), 24 (Bl/B),
  25 (Bl/G) – pin numbers read from position in the drawing, so probable
- a red wire joined to VCC (5 V sensor supply, pin 48)
- B/Br sensor ground

Five signal lines plus 5 V and ground on an unnamed coupler looks like a
powered accessory or tuning port. **Unverified idea:** this may be where
aftermarket fuel-trim boxes for these bikes plugged in.

Measurements (6.12): pins 15 and 16 are pulled up to 5 V through about
5.1 kΩ each. Pins 23, 24 and 25 are high impedance (about 1.3 MΩ to
5 V): the ECU drives them neither high nor low at idle. So the coupler
is two pulled-up lines plus three floating ones, with 5 V and ground.
None toggles while the ECU idles. Function still unknown.

---

## 5. Bench power setup

- Supply: 12.14 V, 1.5 A DC wall adapter (11.98 V measured on 23 Nov).
- Measured 23 Nov: 1.91 V across the 10 Ω, so about 190 mA and 0.36 W in
  the resistor; pin 17 sits at about 10 V.
- Current limit: 10 Ω resistor in series with +12 V (a 12 V 5 W filament
  bulb also works; an LED does **not**).
- Wiring: supply − → pin 35, supply + → 10 Ω → pin 17.
- Measuring current with a voltmeter: volts across the 10 Ω ÷ 10 = amps.

| Volts across 10 Ω | Current   | Meaning                         |
|-------------------|-----------|---------------------------------|
| ~0 V              | ~0 mA     | Not powered / wrong pin         |
| 0.5–2.5 V         | 50–250 mA | Normal, ECU running             |
| near 12 V         | >1 A      | Short – disconnect immediately  |

Note: a ¼ W resistor runs hot in normal operation. Use ≥1–2 W, or keep
powered time short.

**Power-cycling:** leave the ECU off for about ten seconds. A short
off-time once failed to clear a latched fault (6.9).

---

## 6. Tests performed and results

### 6.1 Continuity (unpowered)
- 35↔18 ≈ 1 Ω, 35↔27 ≈ 1 Ω, 35↔26 = 7.3 kΩ, 35↔17 = 1.1 MΩ.
- Confirms pin numbering and no short on the power pin.

### 6.2 Power-up
- ECU draws current, resistor warms, 5 V logic levels present on 29/30.
- Internal 5 V regulator working.

### 6.3 Load test (10.2 kΩ = 2 × 5.1 kΩ to ground)
- Pin 29: 4.933 → 4.693 V ⇒ ~520 Ω to 5 V (likely a 510 Ω resistor).
- Pin 30: 4.919 → 4.902 V ⇒ ~35 Ω (actively driven).

### 6.4 Passive listening on pins 29/30 (Arduino Uno)
- Uno used as 5 V USB-serial (RESET jumpered to GND; listen on Uno pin 1),
  and with a pulse-timing sketch on D2/D3.
- Signal fed through 1 kΩ (2 × 510 Ω in series).
- Result: **no activity on pin 29 or pin 30**. With GND disconnected,
  noise appears (expected). With GND connected, only a single pulse at
  ECU power-off (the line collapsing – not data).

### 6.5 Active probing on pins 29/30
- Sent on pin 29 (Uno D3), listened on pin 30 (Uno D2), using
  `SERIALCOM/SERIALCOM.ino`.
- Speeds: 1200, 2400, 4800, 7812, 9600, 10400, 15625, 19200, 31250,
  38400, 57600 baud.
- Messages: Suzuki KWP2000 start (`81 12 F1 81 05`) with 25 ms wake pulse,
  generic start (`C1 33 F1 81 66`), single `00`, single `55`.
- Result: **no response at any speed**.

### 6.6 Pin 56 (TECH): stream found
- The meter showed a fluctuating voltage on pin 56.
- Listened with the Uno in USB-serial mode, pin 56 → 1 kΩ → Uno pin 1.
- Swept 300–115200 baud. The shortest low pulse measured 130–143 µs at
  every rate, i.e. one bit at **7812.5 baud** (128 µs).
- At 7812 baud: the same 8-byte frame every 500 ms, 40 times in a row.
- First frame seen (bare ECU): `FF C9 00 FC 40 00 00 FC`.

### 6.7 Dealer mode (pin 22)
- Grounding pin 22 to pin 35: byte 2 `C9` → `DD`, checksum follows.
- Lifting it: back to `C9`. Nothing else changes; the fault bytes are
  sent in both modes.
- All eight bytes sum to 0 mod 256 in every frame, confirming the 8N1
  framing and that byte 8 is a checksum.

### 6.8 Fault-bit mapping
One sensor input at a time was given a valid level and the stream watched
for the bit that cleared.

| Pin | What was connected | Change | Result |
|-----|--------------------|--------|--------|
| 51 (THW) | resistor to pin 54 | byte 4 `FC` → `EC` | bit 4 = c15 |
| 50 (THA) | 1 kΩ to pin 54 | byte 4 `FC` → `DC` | bit 5 = c21 |
| 59 (DON) | 0.8 V divider midpoint (first run was logged as "1 kΩ to pin 54", probably a mislabelled 10 kΩ) | byte 4 `FC` → `7C` | bit 7 = c23 |
| 57 (GP)  | 10 kΩ to pin 48 | none, also after power cycle | byte 5 bit 6 is **not** c31 |
| 57 (GP)  | 1 kΩ to pin 54 (pin at 2.488 V) | none | still a valid gear voltage; pull-up is ~1 kΩ |
| 57 (GP)  | wire link to pin 54 | byte 3 `00` → `04` | byte 3 bit 2 = c31 |
| 45 (STP) | 2.5 V divider | byte 5 `40` → `20` | bit 6 = c29; bit 5 appears |
| 58 (PM)  | 2.5 V divider | byte 4 `FC` → `F8` | bit 2 = c13 |
| 49 (VTA) | 2.5 V divider | byte 4 `FC` → `F4` | bit 3 = c14 |
| 52 (PA)  | 2.5 V divider | byte 4 `FC` → `BC` | bit 6 = c22 |

- 2.5 V divider = two 5.1 kΩ in series from pin 48 (VCC) to pin 54 (E2),
  midpoint to the input.
- With pin 45 valid, byte 5 bit 5 sets. Probably c28 (secondary throttle
  actuator): the manual notes that when both the actuator and its sensor
  are missing, only c29 is shown.
- With 2.5 V on pin 49 in dealer mode, bytes 2 and 3 did not change; the
  throttle-position mark was found later at lower voltages (6.10).

### 6.9 Coolant calibration (pin 51)
Resistor across pins 51 and 54:

| Resistor | Raw (10-bit) | Resistance implied | Shown as |
|----------|--------------|--------------------|----------|
| open     | 1023         | –                  | no reading |
| 5.04 kΩ (metered) | 754 | 5034 Ω            | about 3 °C |
| 2.16 kΩ (metered) | 559 | 2164 Ω            | about 23 °C |
| 505 Ω (metered)   | 226 | 509 Ω             | about 64 °C |

- The raw value is the pin 51 voltage as a fraction of 5 V. The ECU pulls
  the pin up through about **1.8 kΩ**:
  `R_sensor = 1796 × raw / (1023 − raw)`.
- The ECU sends the voltage; the dash converts it to degrees.
- An early run recorded as "1 kΩ" gave raw 870, which is 10.2 kΩ by this
  formula. It was probably the 2 × 5.1 kΩ pair; unconfirmed.
- **c15 latches.** It sets as soon as the input opens and stays set after
  a valid reading returns, until the ECU is power-cycled. One short power
  cycle did not clear it; a longer one did.
- The pin 51 voltage has not been metered directly.

### 6.10 Throttle-position mark (pin 49, dealer mode on)
A resistor divider from pin 48 to pin 54 set the voltage on pin 49; the
midpoint was metered each time.

| Pin 49 voltage | Byte 2 | Bits 2,1 | Mark |
|----------------|--------|----------|------|
| 0.625 V | `DB` | `01` | low |
| 0.939 V | `DB` | `01` | low |
| 1.06 V  | `DB` | `01` | low |
| 1.12 V  | `DF` | `11` | middle (correct position) |
| 1.18 V  | `DD` | `10` | high |
| 1.25 V  | `DD` | `10` | high |
| 2.5 V, and input open | `DD` | `10` | high |

- The "correct" window is narrower than 0.12 V, somewhere between 1.06
  and 1.18 V. Its exact edges were not measured.
- The mark updates live. Which state the dash draws as its upper or
  lower line is assumed, not checked against a dash.
- Dealer mode is bit 4 alone; with dealer mode off bits 2 and 1 are both
  clear.
- Removing the divider's bottom leg lets pin 49 float to 5 V and latches
  c14.

### 6.11 Open-pin voltages and gear position (pin 57)
- Open, powered: pin 57 = 4.942 V, pin 60 = 9 V, pin 42 = 0 V,
  pin 38 = 0 V, pin 56 = 4.8–4.9 V (flickering with data).
- Pin 57 with 1 kΩ to pin 54 reads 2.488 V, so its pull-up is about 1 kΩ.
  That is still a valid gear voltage and sets no fault.
- Pin 57 linked directly to pin 54: byte 3 `00` → `04` (c31).
- c31 stays set after the link is removed, so it latches like c15.

### 6.12 7-way coupler lines (pins 15, 16, 23, 24, 25)
Metered to pin 35, powered; load = 9.87 kΩ (metered) from the pin to
pin 35.

| Pin | Open | Loaded | Source resistance | Reading |
|-----|------|--------|-------------------|---------|
| 15  | 4.939 V | 3.258 V | ~5.1 kΩ  | Pulled up to 5 V |
| 16  | 4.939 V | 3.258 V | ~5.1 kΩ  | Pulled up to 5 V |
| 23  | 4.358 V | 0.032 V | ~1.3 MΩ  | High impedance |
| 24  | 4.357 V | 0.031 V | ~1.3 MΩ  | High impedance |
| 25  | 4.358 V | 0.032 V | ~1.3 MΩ  | High impedance |

- Pins 15 and 16: a 5.1 kΩ pull-up each, the pattern of an input meant
  to be pulled low by whatever plugs in (or an open-drain line).
- Pins 23–25: the 4.36 V open reading was the meter, not a diode drop.
  A 10 MΩ meter on a 1.3 MΩ source at 4.94 V reads 4.36 V. The ECU is
  not driving these lines; they are inputs or switched-off outputs.

### 6.13 Remaining harness inputs (dealer mode on)
None of these changed the frame from `FF DD 00 FC 40 00 00 E8`, live or
after a ten-second power cycle.

| Pin | What was connected | Result |
|-----|--------------------|--------|
| 38 (MPS) | 2.475 V from a 5.04 kΩ / 5.01 kΩ divider, pin 48 to pin 54 | no change |
| 19 (CLT) | wire link to pin 35 | no change |
| 20 (NT)  | wire link to pin 35 | no change |
| 21 (MS)  | wire link to pin 35 | no change |
| 33       | wire link to pin 35 (with pin 21 also linked) | no change |

- Pin 60 load test: 9 V open, 8.43 V with 9.87 kΩ to pin 35, so a
  pull-up of roughly 0.7 kΩ to about 9 V.
- Pins 21 and 33 are pulled up to 5 V through about 5.1 kΩ and are tied
  to ground by the harness. On the bench they float high unless linked.
- So the always-set bits 3 and 0 of byte 2 are not clutch, neutral or
  either strap.

### 6.14 Starter signal (pin 28): not completed
- Early readings of 0 V "on pin 28" were on pin 27 (ground).
- Pin 28 idles at 2.439 V through about 4 kΩ (1.731 V with 9.87 kΩ to
  ground). Fed from 12 V through 9.87 kΩ it rises to 10.21 V and takes
  about 0.2 mA: a normal 12 V switch input.
- Held at 10.2 V for 5 s with c23 still set: no change in the stream.
- The manual sets c11 and c12 after 4 s of cranking with no signal, so
  5 s by hand is too close to call. A 15 s hold with the tip-over fault
  cleared was planned but not run before the K1 failed (6.16).
- Tip-over (pin 59) correction made on the way: a true 1 kΩ to pin 54
  pulls the pin to 0 V, which is itself a fault. A divider
  (pin 48 – 5.04 kΩ – pin 59 – 1 kΩ – pin 54) gives 0.8 V and clears
  c23 after a power cycle (byte 4 `FC` → `7C`).

### 6.15 Supply resistors burning (late 22 Nov)
During the pin 28 work the 10 Ω supply resistors began burning up. That
was the K1 failing (6.16). The unit connected next was a K3 ECU, by
mistake; its measurements are in `../K3-K4/captures/` and not here.

### 6.16 K1 failure: 5 V supply shorted (23 Nov)
Measured on the K1 after the mix-up was found:

- Unpowered, pin 17 to pin 35: 6.36 MΩ, OL reversed. The 12 V input is
  not shorted.
- Powered through the 12 V 5 W bulb, nothing else connected: bulb dim,
  pin 17 6.51 V, **pin 48 1.813 V** (4.99 V when healthy).
- Unpowered, **pin 48 to pin 35: 2.3 Ω, 2.5 Ω reversed.** A hard short
  on the 5 V sensor supply, inside the ECU.
- This is what burned the ¼ W supply resistors: the regulator sits in
  current limit feeding the short.
- Likely failed part: a decoupling capacitor, the 5 V regulator, or a
  chip on the 5 V line. Cause not proven; 12 V reaching a 5 V pin during
  the pin 28 / divider work is the leading candidate.
- Finding the part needs the case open (low-voltage, current-limited
  feed into pin 48 and look for the part that warms). The case cannot be
  opened here, so the unit is retired.

### 6.17 mr_hollywood's published method (not pursued)
From his site and documents, copied into `reference/mr_hollywood/`.

- **Bootloader.** Flash-version ECUs carry a small loader at $90000. It
  runs instead of the engine program when pin 30 is held to ground
  during power-up, and talks on pin 31: ISO 9141 K-line levels, 9600
  baud, 8N1, half duplex.
- **Test for it.** Send one byte `20` (a space). A flashable ECU answers
  `66 21` ("f!"). A mask-ROM ECU answers nothing. The interface echoes
  whatever is sent, so the echo must not be mistaken for a reply.
- **No-transmit test.** With pin 30 grounded at power-up a flashable ECU
  does not run: on the bike the fuel pump does not prime. On the bench
  the pin 56 stream should then be absent (our inference, not his).
- **Reading.** The loader accepts a program of up to 1,792 bytes into
  RAM at $80100 and runs it; that program sends the memory back. Nothing
  is erased. His Flash tool (Windows, demo version is read-only) reads
  the 96 KB flash, the 2 KB bootloader, the registers and the 512-byte
  fuel-trim memory.
- **Writing.** Needs his licensed version and 12 V on pin 29. Only the
  map area ($10000–$17FFF) is reflashed. A bad flash of the first 32 KB
  can kill the bootloader, leaving BDM as the only recovery.
- **Upload format:** `53`, `08 01` (load address $080100), `00`, 2-byte
  length, then the program, which starts with a short branch, the ID
  bytes `49 91`, a zero and a checksum byte making the 8-bit sum zero.
  Data must flow without gaps. The watchdog resets the ECU unless
  serviced about every 65 ms.
- **Hardware he uses:** a VAG-COM KKL 409.1 USB cable with a CH340
  chip (not FTDI), plus a hand-made adaptor. A 5 V UART to K-line
  schematic is in `ttl_to_obd_iso9141_interface.jpg`.
- Written for the 600/750; his drawing and the Flash tool manual extend
  it to the GSX-R1000 and Hayabusa units.

### Conclusion so far
- **Pin 56 is the ECU's diagnostic output.** It sends the coolant sensor
  voltage and the full fault list in user mode and dealer mode alike
  (twice a second). Pin 22 only
  tells the dash to display the codes.
- **The K1 unit is dead**: 5 V supply shorted internally (6.16).
- **There is no K-line or diagnostic connector in the bike harness.**
  Normal diagnostics on this model are dealer mode plus this stream.
- **The stream reports sensor faults only.** Clutch, neutral, the two
  ground straps and the exhaust valve position input do not appear.
- **Pins 29, 30 and 31 are the bootloader port** (6.17): 30 enables it,
  31 is its K-line, 29 is the programming voltage.
- **Reading does not erase.** It needs only pins 30 and 31, and works
  only if this unit has the flash processor.
- **The 7-way coupler lines (pins 15, 16, 23, 24, 25)** are still
  unexplained and unwatched at power-up.

---

## 7. Pin 56 dash stream protocol

- One-way, ECU → dash. Logic-level UART, idle high, not inverted.
- **7812.5 baud, 8N1.** One 8-byte frame every 500 ms.
- The dash shows "CHEC" if no frame arrives for 5 seconds (manual
  p. 4-28).

```
byte:   1   2   3   4   5   6   7   8
        FF  C9  00  FC  40  00  00  FC     bare ECU, dealer mode off
```

| Byte | Bits | Meaning | Basis |
|------|------|---------|-------|
| 1    | all  | Coolant sensor voltage, high 8 bits of 10 | Proven |
| 2    | 7–6  | Coolant sensor voltage, low 2 bits | Fits all calibration points |
| 2    | 4    | Dealer mode (pin 22 grounded) | Proven |
| 2    | 2, 1 | Throttle-position mark, dealer mode only: `10` high, `11` middle, `01` low | Proven, see 6.10 |
| 2    | 3, 0 | Always set so far; not clutch, neutral, pin 21 or pin 33 | Unknown |
| 2    | 5    | Always clear so far | Unknown |
| 3    | 2    | c31 gear position signal (sets with pin 57 at 0 V) | Proven |
| 3    | 0–1, 3–7 | Always clear so far; presumably more fault bits | Unknown |
| 4    | 0, 1 | Probably c11, c12 (need 4 s of cranking to set) | Untested; 5 s starter hold gave nothing (6.14) |
| 4    | 2    | c13 intake air pressure sensor | Proven |
| 4    | 3    | c14 throttle position sensor | Proven |
| 4    | 4    | c15 coolant temperature sensor | Proven |
| 4    | 5    | c21 intake air temperature sensor | Proven |
| 4    | 6    | c22 atmospheric pressure sensor | Proven |
| 4    | 7    | c23 tip-over sensor | Proven |
| 5    | 5    | Probably c28 secondary throttle actuator; sets once pin 45 is valid (6.8) | Inferred |
| 5    | 6    | c29 secondary throttle position sensor | Proven |
| 5    | 0–4, 7 | Always clear so far | Unknown |
| 6, 7 | all  | Always `00` so far; presumably more fault bits | Unknown |
| 8    | all  | Checksum: bytes 1–8 sum to 0 mod 256 | Proven on every frame |

Coolant value: `raw = byte1 × 4 + (byte2 >> 6)`; 1023 means sensor open.

Byte 4 follows the manual's code order (c11 at bit 0). Byte 5 does not
simply continue it: counting back from c29 at bit 6 would put c24–c27 at
bits 1–4 and c31 at bit 7, which is untested.

Reference frames:

| Frame | Condition |
|-------|-----------|
| `FF C9 00 FC 40 00 00 FC` | Bare ECU, dealer mode off |
| `FF DD 00 FC 40 00 00 E8` | Bare ECU, pin 22 grounded |
| `38 89 00 EC 40 00 00 13` | 505 Ω on pins 51/54, c15 cleared |
| `FF DD 00 FC 20 00 00 08` | 2.5 V on pin 45, pin 22 grounded |
| `FF C9 04 FC 40 00 00 F8` | Pin 57 linked to pin 54 (c31 set) |
| `FF DF 00 FC 40 00 00 E6` | 1.12 V on pin 49, pin 22 grounded (throttle mark middle) |

Faults expected on a bare bench but **not** showing: c41 (fuel pump
relay), c42 (ignition switch signal), c46 (exhaust valve actuator).

---

## 8. Tools in this folder

| File | Purpose |
|------|---------|
| `faults.py` | Live decoder: dealer mode on/off, coolant temperature, named fault codes, and any set bits not yet identified. Prints when the frame changes. |
| `hexdump.py` | Live hex view of every frame with checksum check; changed bytes in red. `-c` shows changes only, `-l FILE` logs. |
| `listen.py` | Baud-rate sweep and raw capture into `captures/`. |
| `rawlisten.py` | Timestamped raw capture at one baud rate: `rawlisten.py BAUD SECONDS OUTFILE`. Used for pin 31. |
| `ECUFAULTS/ECUFAULTS.ino` | Stand-alone Uno version of `faults.py`: ECU pin 56 → 1 kΩ → Uno D2, Serial Monitor at 115200. Compiled, not yet run against the ECU. |
| `SERIALCOM/SERIALCOM.ino` | Arduino sketch from 6.5: active probing on pins 29/30. |
| `captures/` | Raw captures, logs and `coolant_calibration.csv`. |
| `captures/session_2026-11-22.md` | Chronological log of every test, frame and meter reading from 22–23 Nov, up to the K1 failure. K3 measurements from the mix-up are in `../K3-K4/captures/`. |
| `gsxr1000-ecm-pinout.html` / `.pdf` | Full ECM coupler pinout. |
| `gsxr1000.pdf` | 2001 service manual (image scan, no text layer). |
| `reference/mr_hollywood/` | mr_hollywood's adaptor wiring, K-line test, bootloader protocol, Flash tool manual and board photos, with text copies. See `SOURCE.md` there. |

All four Python scripts are listen-only and default to `/dev/ttyACM0` at
7812 baud. To add a newly identified fault bit, add one line to the
`KNOWN` table at the top of `faults.py` (and to `KNOWN[]` in
`ECUFAULTS.ino`).

Hookup: pin 56 → 1 kΩ → Uno pin 1, Uno GND → pin 35, Uno RESET → GND.

Useful manual pages (PDF page numbers): 197 FI wiring diagram with ECM
labels; 198–203 self-diagnosis and dealer mode; 204–205 code table;
206–223 per-code charts; 384 "CHEC" note; 458–464 high-resolution wiring
diagrams.

---

## 9. Arduino Uno notes

- **USB-serial mode (no sketch):** jumper RESET → GND. Labels are
  reversed in this mode:
  - Pin 1 (marked TX) = data **into** the PC → connect the ECU line here.
  - Pin 0 (marked RX) = data **from** the PC.
- Loopback check: wire pin 0 to pin 1, type in Serial Monitor, text echoes.
- ECU data is **raw binary bytes**, not text. Use the scripts above or a
  hex-view terminal.
- The Uno's USB chip accepts 7812 baud exactly and does not report
  framing errors.
- Uno's pin 0 has an on-board 1 kΩ resistor; it may not pull a strongly
  pulled-up ECU input low enough. Use another pin via SoftwareSerial for
  sending.
- Only one program can hold the serial port. Close the Arduino Serial
  Monitor and any other running script first.

---

## 10. K-line interface for pin 31 (Uno + one NPN transistor)

Not pursued (see status). Kept as an untested early draft only. Parts: 1 × NPN (2N3904 / 2N2222 / BC547 / S8050), 2.2 kΩ,
510 Ω (≥0.5 W), 10 kΩ, 6.8 kΩ.

```
Sending (Uno D3 pulls K-line low):

  Uno D3 ──[2.2k]── base
                     NPN
  +12V ──[510]──┬── collector
                │
                └──────────────── ECU pin 31 (K-line)
                     emitter ──── common GND

Reading (divider brings 12 V down to ~4.9 V):

  ECU pin 31 ──[10k]──┬── Uno D2
                      │
                    [6.8k]
                      │
                  common GND

Common GND = Uno GND + ECU pin 35 + 12 V supply negative
+12V for the 510 Ω pull-up = the SAME supply powering the ECU
```

- Logic is inverted on the send side: D3 HIGH → K-line LOW.
- Divider check: 12.14 V × 6.8 / 16.8 ≈ 4.9 V at D2. Never use a
  10k/10k divider (≈6 V, over the Uno's limit).

### Bench-test before connecting to the ECU
1. K-line wire unconnected: should read ~12 V.
2. D3 HIGH → K-line < 1 V. D3 LOW → back to ~12 V.
3. D2 must stay below 5 V in both states.

### `SERIALCOM.ino` must be changed before use here
- It idles D3 HIGH with non-inverted serial, which through the transistor
  would hold the K-line low permanently. Transmit must be inverted.
- The read side is not inverted, and SoftwareSerial's inverse flag flips
  both directions, so transmit needs bit-banging or separate handling.
- A K-line is one wire: D2 will see every byte the Uno sends. The sketch
  would report that echo as ECU activity.
- It only counts pulses; it should capture and print bytes.

---

## 11. Next steps

The K1 is dead (6.16). Any further diagnostics need a working ECU, and
the K3's pin map must be checked against section 4 before reusing the
bench wiring: so far only power, pins 22, 51/54 and 56 are known to
match. Always use a 10 Ω supply resistor rated 2 W or more, or the
12 V 5 W bulb.

**Fault-stream tests left over**

1. **Cranking codes c11/c12:** clear c23 with the pin 59 divider, then
   12 V through 9.87 kΩ on pin 28 for 15 s. Expect byte 4 bits 0 and 1.
2. **Fuel pump relay code c41:** 12 V through 9.87 kΩ on pin 42 (VM),
   power-cycle, watch bytes 5–7.
3. Find the edges of the throttle "middle" window (between 1.06 and
   1.18 V on pin 49).
4. **7-way coupler:** listen on pins 15, 16, 23, 24, 25 at power-up.

---

## 12. Open questions

- What shorted the K1's 5 V supply (6.16), and which part failed.
- Exact function of pins 29 and 30 (both measured as high, low-impedance).
- Why pin 26 reads 7.3 kΩ when the manual shows it as ground E03.
- What the 7-way coupler is for, and what pins 15/16/23/24/25 do.
- Which code byte 5 bit 5 is, and the layout of bytes 5–7.
- Why c41, c42 and c46 do not show on a bare bench.
- Meaning of byte 3 and of the always-set bits 3 and 0 in byte 2.
- Whether this unit has the flash processor (answers `66 21`) or the
  mask-ROM one.
- Why pin 30 load-tested as a ~35 Ω source when it is published as a
  pulled-up input.
- What pin 60 (AT) is for, and why it had such a strong pull-up.
- What the pin 33 strap selects.
- How the case opens.
- Processor type and whether a stock 32920-40F30 binary exists.

---

## Sources

- Suzuki 2001 GSX-R1000 service manual (`gsxr1000.pdf`, from ManualsLib)
- RevZilla – 2002 GSX-R1000 electrical parts diagram
- Carmo Electronics – GSX-R1000 K1/K2 ECU part number list
- gixxer.com – "Gsxr1000 k1 k2 ecu flashing"
- gixxer.com – "Reflash computer"
- gixxer.com – "Found a wire disconnected from the ecu"
- gixxer.com – "Ecu pin out diagram needed"
- gixxer.com – "05/06 US Gsxr 1000 ECM Flash DIY"
- gsxr.com – "How To ReFlash/Remap 05/06 GSXR1000 stock ECU"
- mr_hollywood – https://sites.google.com/site/mrhollywoodk3 (Denso
  16-bit ECU tools and documents)
- gsxr.com – "16bit GSXR ECU reflashing / hacking"
- flashecu.com – "03-04 Suzuki GSXR 1000 Data-Link ECU Flashing Kits"
- Hayabusa K2–K7 ECU reflashing interface notes (slideplayer)
