// GSX-R1000 K1/K2 ECU dash-stream decoder for Arduino Uno.
// Stand-alone version of faults.py: reads the ECU's pin 56 (TECH) stream
// and prints mode, coolant temperature and fault codes to the Serial
// Monitor. Listen-only: nothing is ever sent to the ECU.
//
// Wiring (the Uno runs this sketch, so NO jumper from RESET to GND):
//   ECU pin 56 --[1 k]-- Uno D2      (NOT Uno pin 1 any more)
//   ECU pin 35 ---------- Uno GND    (plus the 12 V supply negative)
//   Uno D4: leave empty (unused transmit pin required by SoftwareSerial)
//
// Serial Monitor: 115200 baud.
//
// Frame: 7812.5 baud 8N1, 8 bytes every 0.5 s.
//   byte 1      coolant sensor voltage, high 8 bits (FF = sensor open)
//   byte 2      bits 7-6 coolant low 2 bits, bit 4 dealer mode,
//               bits 2,1 throttle-position mark (dealer mode only)
//   byte 3-7    fault bits; only those in KNOWN[] are identified
//   byte 8      checksum: all 8 bytes sum to 0 mod 256

#include <SoftwareSerial.h>
#include <math.h>

const uint8_t ECU_RX_PIN = 2;       // from ECU pin 56 via 1 k
const uint8_t UNUSED_TX_PIN = 4;    // leave unconnected
const long ECU_BAUD = 7812;
const unsigned long FRAME_GAP_MS = 50;      // silence that ends a frame
const unsigned long NO_DATA_MS = 5000;      // the dash shows CHEC after 5 s

SoftwareSerial ecu(ECU_RX_PIN, UNUSED_TX_PIN);

// Fault bits confirmed on the bench: byte number (1-8), bit (0-7), code.
// Add a line here each time a test identifies another bit.
struct KnownFault { uint8_t byteNo; uint8_t bit; uint8_t code; };
const KnownFault KNOWN[] = {
  {3, 2, 31},
  {4, 2, 13},
  {4, 3, 14},
  {4, 4, 15},
  {4, 5, 21},
  {4, 6, 22},
  {4, 7, 23},
  {5, 6, 29},
};
const uint8_t KNOWN_COUNT = sizeof(KNOWN) / sizeof(KNOWN[0]);
const uint8_t FIRST_FAULT_BYTE = 3, LAST_FAULT_BYTE = 7;

// Coolant: the raw 10-bit value is the voltage on ECU pin 51 as a fraction
// of 5 V. The ECU pulls that pin up through about 1.8 k, so
// sensor resistance = PULLUP * raw / (1023 - raw).
const float PULLUP = 1796.0;
// Service manual p. 5-8: sensor resistance (ohm) at temperature (C).
const float ECT_OHM[] = {2450, 811, 318, 142, 88};
const float ECT_C[]   = {20,   50,  80,  110, 130};
const uint8_t ECT_POINTS = 5;

uint8_t frame[8];
uint8_t count = 0;
uint8_t lastKey[7];
bool haveKey = false;
bool noDataReported = false;
unsigned long lastByteMs = 0, lastFrameMs = 0;
unsigned long goodFrames = 0, badFrames = 0;

const __FlashStringHelper* faultName(uint8_t code) {
  switch (code) {
    case 11: return F("Camshaft position sensor");
    case 12: return F("Crankshaft position sensor");
    case 13: return F("Intake air pressure sensor");
    case 14: return F("Throttle position sensor");
    case 15: return F("Engine coolant temperature sensor");
    case 21: return F("Intake air temperature sensor");
    case 22: return F("Atmospheric pressure sensor");
    case 23: return F("Tip-over sensor");
    case 24: return F("Ignition signal #1");
    case 25: return F("Ignition signal #2");
    case 26: return F("Ignition signal #3");
    case 27: return F("Ignition signal #4");
    case 28: return F("Secondary throttle valve actuator");
    case 29: return F("Secondary throttle position sensor");
    case 31: return F("Gear position signal");
    case 32: return F("Injector signal #1");
    case 33: return F("Injector signal #2");
    case 34: return F("Injector signal #3");
    case 35: return F("Injector signal #4");
    case 41: return F("Fuel pump control system");
    case 42: return F("Ignition switch signal (anti-theft)");
    case 46: return F("Exhaust control valve actuator");
  }
  return F("?");
}

// Sensor resistance -> degrees C, interpolating ln(R) against 1/T.
float tempC(float ohm) {
  if (ohm < 1.0) ohm = 1.0;
  float x = log(ohm);
  uint8_t i = 0;
  while (i < ECT_POINTS - 2 && x < log(ECT_OHM[i + 1])) i++;
  float x0 = log(ECT_OHM[i]),     y0 = 1.0 / (ECT_C[i] + 273.15);
  float x1 = log(ECT_OHM[i + 1]), y1 = 1.0 / (ECT_C[i + 1] + 273.15);
  float y = y0 + (x - x0) * (y1 - y0) / (x1 - x0);   // extrapolates past either end
  return 1.0 / y - 273.15;
}

void printHexByte(uint8_t b) {
  if (b < 0x10) Serial.print('0');
  Serial.print(b, HEX);
}

void printFrame() {
  Serial.print(F("["));
  Serial.print(millis() / 1000);
  Serial.print(F(" s]  "));
  for (uint8_t i = 0; i < 8; i++) {
    printHexByte(frame[i]);
    Serial.print(' ');
  }
  Serial.println();

  uint16_t raw = ((uint16_t)frame[0] << 2) | (frame[1] >> 6);
  uint8_t flags = frame[1] & 0x3F;
  bool dealer = flags & 0x10;

  Serial.print(F("  mode     "));
  Serial.println(dealer ? F("DEALER MODE ON") : F("DEALER MODE OFF"));

  Serial.print(F("  coolant  "));
  if (raw == 0x3FF) {
    Serial.println(F("sensor open / no reading"));
  } else {
    float ohm = PULLUP * raw / (1023.0 - raw);
    Serial.print(F("about "));
    Serial.print(tempC(ohm), 0);
    Serial.print(F(" C   (sensor "));
    Serial.print(ohm, 0);
    Serial.print(F(" ohm, raw "));
    Serial.print(raw);
    Serial.println(')');
  }

  if (dealer) {
    Serial.print(F("  throttle "));
    switch ((flags >> 1) & 3) {
      case 2: Serial.println(F("HIGH side (1.18 V and above)")); break;
      case 1: Serial.println(F("LOW side (1.06 V and below)")); break;
      case 3: Serial.println(F("MIDDLE - correct position (seen at 1.12 V)")); break;
      default: Serial.println(F("no mark bits set - not seen in dealer mode before")); break;
    }
  }

  uint8_t other = flags & ~0x16;
  if (other != 0x09) {
    Serial.print(F("  NOTE     byte 2 flag bits changed: 0x"));
    printHexByte(other);
    Serial.println(F(" (0x09 until now)"));
  }

  // Known faults first, then anything set that has no name yet.
  uint8_t named = 0, unnamed = 0;
  for (uint8_t k = 0; k < KNOWN_COUNT; k++) {
    if (frame[KNOWN[k].byteNo - 1] >> KNOWN[k].bit & 1) {
      Serial.print(F("  fault    c"));
      Serial.print(KNOWN[k].code);
      Serial.print(F("  "));
      Serial.println(faultName(KNOWN[k].code));
      named++;
    }
  }
  for (uint8_t b = FIRST_FAULT_BYTE; b <= LAST_FAULT_BYTE; b++) {
    for (uint8_t bit = 0; bit < 8; bit++) {
      if (!(frame[b - 1] >> bit & 1)) continue;
      bool known = false;
      for (uint8_t k = 0; k < KNOWN_COUNT; k++)
        if (KNOWN[k].byteNo == b && KNOWN[k].bit == bit) known = true;
      if (known) continue;
      Serial.print(unnamed ? F(", ") : F("  not yet identified: "));
      Serial.print(F("byte "));
      Serial.print(b);
      Serial.print(F(" bit "));
      Serial.print(bit);
      unnamed++;
    }
  }
  if (unnamed) Serial.println();
  if (!named && !unnamed) Serial.println(F("  faults   none (c00)"));
  Serial.println();
}

void handleFrame() {
  uint8_t sum = 0;
  for (uint8_t i = 0; i < 8; i++) sum += frame[i];
  if (sum != 0) { badFrames++; return; }
  goodFrames++;
  lastFrameMs = millis();
  noDataReported = false;

  // Print only when something changes; ignore the one-count flicker in
  // the coolant value's low two bits.
  uint8_t key[7];
  key[0] = frame[0];
  key[1] = frame[1] & 0x3F;
  for (uint8_t i = 2; i < 7; i++) key[i] = frame[i];
  if (haveKey && memcmp(key, lastKey, 7) == 0) return;
  memcpy(lastKey, key, 7);
  haveKey = true;

  // The next frame is ~490 ms away, so there is time to print now.
  printFrame();
  Serial.flush();
}

void setup() {
  Serial.begin(115200);
  pinMode(ECU_RX_PIN, INPUT);
  ecu.begin(ECU_BAUD);
  Serial.println(F("GSX-R1000 K1/K2 ECU fault reader - listening on D2 at 7812 baud"));
  Serial.println(F("Prints when the frame changes.\n"));
  lastFrameMs = millis();
}

void loop() {
  unsigned long now = millis();

  while (ecu.available()) {
    uint8_t b = ecu.read();
    now = millis();
    if (count && now - lastByteMs > FRAME_GAP_MS) count = 0;   // stale partial frame
    lastByteMs = now;
    if (count < 8) frame[count] = b;
    count++;
  }

  // A frame ends when the line has been quiet for FRAME_GAP_MS.
  if (count && now - lastByteMs > FRAME_GAP_MS) {
    if (count == 8) handleFrame(); else badFrames++;
    count = 0;
  }

  if (!noDataReported && now - lastFrameMs > NO_DATA_MS) {
    Serial.print(F("No data from ECU for 5 s (the dash would show CHEC). Good frames so far: "));
    Serial.print(goodFrames);
    Serial.print(F(", bad: "));
    Serial.println(badFrames);
    Serial.println();
    noDataReported = true;
    haveKey = false;          // print the next frame even if unchanged
  }
}
