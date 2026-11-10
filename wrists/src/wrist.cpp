#include <Arduino.h>
#include <Adafruit_DRV2605.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_now.h>
#include <esp_wifi.h>

// Configure
#define WIFI_CHANNEL 1 //will check instructor board to make sure correct
#define LEFT_SDA     8
#define LEFT_SCL     9
#define RIGHT_SDA    17
#define RIGHT_SCL    18

enum Command : uint8_t { CMD_NONE = 0, CMD_LEFT = 1, CMD_RIGHT = 2, CMD_STOP = 3, CMD_START = 4 };

// DRV2605L library effect IDs
#define FX_STRONG_BUZZ  14 // strong buzz 100%
#define FX_ALERT_1000   16 // 1000 ms alert 100%
#define FX_SOFT_BUZZ    49 // buzz 3, 60%
#define WAIT_100MS      (0x80 | 10) // bit 7 set = delay, units of 10 ms

struct __attribute__((packed)) Packet { uint8_t magic; uint8_t cmd; uint8_t seq; }; 
static const uint8_t MAGIC = 0xA5;

Adafruit_DRV2605 drvL, drvR;
bool leftOK = false, rightOK = false;

volatile uint8_t pendingCmd = CMD_NONE;   
static int lastSeq = -1;

//haptics
bool initDriver(Adafruit_DRV2605 &d, TwoWire &bus, int sda, int scl, const char *name) {
  bus.setPins(sda, scl);                
  if (!d.begin(&bus)) {
    Serial.printf("ERROR: %s DRV2605L not found (check SDA/SCL/power)\n", name);
    return false;
  }
  d.selectLibrary(1); // library 1 = ERM motors
  d.useERM();// coin motor is ERM
  d.setMode(DRV2605_MODE_INTTRIG); // internal trigger, started by go()
  Serial.printf("%s driver ready\n", name);
  return true;
}

void play(Adafruit_DRV2605 &d, bool ok, const uint8_t *fx, uint8_t n) {
  if (!ok) return;
  for (uint8_t i = 0; i < n && i < 7; i++) d.setWaveform(i, fx[i]);
  d.setWaveform(n < 8 ? n : 7, 0); // 0 terminates the sequence
  d.go();
}

void handleCommand(uint8_t cmd) {
  static const uint8_t twoBuzz[]   = {FX_STRONG_BUZZ, WAIT_100MS, FX_STRONG_BUZZ};
  static const uint8_t longHard[]  = {FX_ALERT_1000};
  static const uint8_t softShort[] = {FX_SOFT_BUZZ};

  switch (cmd) {
    case CMD_LEFT:// left motor only
      Serial.println("LEFT");
      play(drvL, leftOK, twoBuzz, 3);
      break;
    case CMD_RIGHT:// right motor only
      Serial.println("RIGHT");
      play(drvR, rightOK, twoBuzz, 3);
      break;
    case CMD_STOP: // both, hard and long
      Serial.println("STOP");
      play(drvL, leftOK, longHard, 1);
      play(drvR, rightOK, longHard, 1);
      break;
    case CMD_START:// both, soft and short
      Serial.println("START");
      play(drvL, leftOK, softShort, 1);
      play(drvR, rightOK, softShort, 1);
      break;
    default:
      Serial.printf("Unknown command: %u\n", cmd);
  }
}

void runSelfTest() {
  Serial.println("Self-test: LEFT, RIGHT, STOP, START");
  for (uint8_t c = CMD_LEFT; c <= CMD_START; c++) {
    handleCommand(c);
    delay(1800);
  }
  Serial.println(leftOK && rightOK ? "PASS: both drivers responded" : "FAIL: a driver is missing");
}

// esp receive
#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
#else
void onReceive(const uint8_t *mac, const uint8_t *data, int len) {
#endif
  if (len != sizeof(Packet) || data[0] != MAGIC) return;
  if (data[2] == lastSeq) return;// duplicate (instructor sends broadcasts twice)
  lastSeq = data[2];
  pendingCmd = data[1];                              
}

void setup() {
  Serial.begin(115200);
  delay(500);

  leftOK  = initDriver(drvL, Wire,  LEFT_SDA,  LEFT_SCL,  "LEFT");
  rightOK = initDriver(drvR, Wire1, RIGHT_SDA, RIGHT_SCL, "RIGHT");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  Serial.print("Wrist MAC: ");
  Serial.println(WiFi.macAddress());                 

  if (esp_now_init() == ESP_OK) {
    esp_now_register_recv_cb(onReceive);
    Serial.println("ESP-NOW receiver ready");
  } else {
    Serial.println("ERROR: ESP-NOW init failed");
  }
  Serial.println("Keys: 1=left 2=right 3=stop 4=start t=self-test");
}

void loop() {
  if (pendingCmd != CMD_NONE) {
    uint8_t c = pendingCmd;
    pendingCmd = CMD_NONE;
    handleCommand(c);
  }

  if (Serial.available()) { // bench testing without the instructor
    char c = Serial.read();
    if (c >= '1' && c <= '4') handleCommand(c - '0');
    else if (c == 't') runSelfTest();
  }
}