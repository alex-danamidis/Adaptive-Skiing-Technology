// 1=LEFT  2=RIGHT  3=STOP  4=START
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// configure
#define WIFI_CHANNEL   1 // check if same as board
#define USE_BROADCAST  1 // 1 = no MAC needed 0 = send only to WRIST_MAC.
// only used when USE_BROADCAST is 0.
static uint8_t WRIST_MAC[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

struct __attribute__((packed)) Packet { uint8_t magic; uint8_t cmd; uint8_t seq; }; 
static const uint8_t MAGIC = 0xA5;
static const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static uint8_t seqNum;
static const uint8_t *target;

void setup() {
  Serial.begin(115200);
  delay(300);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE); // same channel on both boards

  if (esp_now_init() != ESP_OK) {
    Serial.println("ERROR: ESP-NOW init failed");
    return;
  }
  target = USE_BROADCAST ? BROADCAST_MAC : WRIST_MAC;

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, target, 6);
  peer.channel = WIFI_CHANNEL;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) Serial.println("ERROR: add peer failed");

  seqNum = (uint8_t)esp_random();
  Serial.printf("Instructor ready (%s). Send 1=left 2=right 3=stop 4=start\n",
                USE_BROADCAST ? "broadcast" : "unicast");
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c < '1' || c > '4') continue; // ignore newlines and junk

    Packet p = {MAGIC, (uint8_t)(c - '0'), ++seqNum};
    esp_err_t r = esp_now_send(target, (uint8_t *)&p, sizeof(p));
#if USE_BROADCAST
    // broadcast has no ACK/retry, so send twice
    delay(15);
    esp_now_send(target, (uint8_t *)&p, sizeof(p));
#endif
    Serial.printf("cmd %c %s\n", c, r == ESP_OK ? "sent" : "SEND ERROR");
  }
}