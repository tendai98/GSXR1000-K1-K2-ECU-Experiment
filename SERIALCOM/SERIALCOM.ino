#include <SoftwareSerial.h>

const uint8_t WATCH_PIN = 2;   // from ECU pin 30, via ~1k
const uint8_t TX_PIN    = 3;   // to ECU pin 29, direct
SoftwareSerial ecu(8, TX_PIN); // pin 8 is a dummy receive pin, leave it empty

const long BAUDS[] = {1200, 2400, 4800, 7812, 9600, 10400,
                      15625, 19200, 31250, 38400, 57600};

const uint8_t MSG_SUZUKI[]  = {0x81, 0x12, 0xF1, 0x81, 0x05};
const uint8_t MSG_GENERIC[] = {0xC1, 0x33, 0xF1, 0x81, 0x66};
const uint8_t MSG_ZERO[]    = {0x00};
const uint8_t MSG_55[]      = {0x55};

// Watch the ECU line for any low pulse; report what was seen.
void watch(const char* label) {
  unsigned long edges = 0, shortest = 0xFFFFFFFF;
  unsigned long t = millis();
  while (millis() - t < 300) {
    unsigned long w = pulseIn(WATCH_PIN, LOW, 20000UL);
    if (w > 0) { edges++; if (w < shortest) shortest = w; }
  }
  if (edges > 0) {
    Serial.print("  *** ACTIVITY after ");
    Serial.print(label);
    Serial.print(": ");
    Serial.print(edges);
    Serial.print(" pulses, shortest ");
    Serial.print(shortest);
    Serial.print(" us (about ");
    Serial.print(1000000UL / shortest);
    Serial.println(" baud)");
  }
}

void probe(const uint8_t* msg, uint8_t len, const char* label, bool wake) {
  if (wake) {                      // 25 ms low, 25 ms high wake-up pulse
    digitalWrite(TX_PIN, LOW);  delay(25);
    digitalWrite(TX_PIN, HIGH); delay(25);
  }
  ecu.write(msg, len);
  watch(label);
  delay(200);
}

void setup() {
  Serial.begin(115200);
  pinMode(WATCH_PIN, INPUT);
  pinMode(TX_PIN, OUTPUT);
  digitalWrite(TX_PIN, HIGH);
  Serial.println("Power the ECU, then watch below.");
}

void loop() {
  for (uint8_t i = 0; i < sizeof(BAUDS) / sizeof(BAUDS[0]); i++) {
    Serial.print("--- ");
    Serial.print(BAUDS[i]);
    Serial.println(" baud ---");
    ecu.begin(BAUDS[i]);
    probe(MSG_SUZUKI,  sizeof(MSG_SUZUKI),  "Suzuki greeting",  true);
    probe(MSG_GENERIC, sizeof(MSG_GENERIC), "generic greeting", true);
    probe(MSG_ZERO,    sizeof(MSG_ZERO),    "single 00",        false);
    probe(MSG_55,      sizeof(MSG_55),      "single 55",        false);
    ecu.end();
    pinMode(TX_PIN, OUTPUT);
    digitalWrite(TX_PIN, HIGH);
  }
  Serial.println("=== round done ===\n");
  delay(3000);
}
