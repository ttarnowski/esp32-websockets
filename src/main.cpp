#include <Arduino.h>
#include <WebSocketsClient.h>
#include <WiFiMulti.h>

#include <ArduinoJson.h>

#define WIFI_SSID "your_wifi_network_name"
#define WIFI_PASSWORD "your_wifi_password"

#define WS_HOST "host.to.your.aws.api.gateway.websockets"
#define WS_PORT 443
#define WS_URL "/dev"

#define JSON_DOC_SIZE 2048
#define MSG_SIZE 256

WiFiMulti wifiMulti;
WebSocketsClient wsClient;

void sendErrorMessage(const char *error) {
  char msg[MSG_SIZE];

  sprintf(msg, "{\"action\":\"msg\",\"type\":\"error\",\"body\":\"%s\"}",
          error);

  wsClient.sendTXT(msg);
}

void sendOkMessage() {
  wsClient.sendTXT("{\"action\":\"msg\",\"type\":\"status\",\"body\":\"ok\"}");
}

uint8_t toMode(const char *val) {
  if (strcmp(val, "output") == 0) {
    return OUTPUT;
  }

  if (strcmp(val, "input_pullup") == 0) {
    return INPUT_PULLUP;
  }

  return INPUT;
}

void handleMessage(uint8_t *payload) {
  StaticJsonDocument<JSON_DOC_SIZE> doc;

  DeserializationError error = deserializeJson(doc, payload);

  // Test if parsing succeeds.
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    sendErrorMessage(error.c_str());
    return;
  }

  if (!doc["type"].is<const char *>()) {
    sendErrorMessage("invalid message type format");
    return;
  }

  if (strcmp(doc["type"], "cmd") == 0) {
    if (!doc["body"].is<JsonObject>()) {
      sendErrorMessage("invalid command body");
      return;
    }

    if (strcmp(doc["body"]["type"], "pinMode") == 0) {
      /*
      Uncomment this code for better validation of pinMode command

      if (!doc["body"]["mode"].is<const char *>()) {
        sendErrorMessage("invalid pinMode mode type");
        return;
      }

      if (strcmp(doc["body"]["mode"], "input") != 0 &&
          strcmp(doc["body"]["mode"], "input_pullup") != 0 &&
          strcmp(doc["body"]["mode"], "output") != 0) {
        sendErrorMessage("invalid pinMode mode value");
        return;
      }
      */

      pinMode(doc["body"]["pin"], toMode(doc["body"]["mode"]));
      sendOkMessage();
      return;
    }

    if (strcmp(doc["body"]["type"], "digitalWrite") == 0) {
      digitalWrite(doc["body"]["pin"], doc["body"]["value"]);
      sendOkMessage();
      return;
    }

    if (strcmp(doc["body"]["type"], "digitalRead") == 0) {
      auto value = digitalRead(doc["body"]["pin"]);

      char msg[MSG_SIZE];

      sprintf(msg, "{\"action\":\"msg\",\"type\":\"output\",\"body\":%d}",
              value);

      wsClient.sendTXT(msg);
      return;
    }

    sendErrorMessage("unsupported command type");
    return;
  }

  sendErrorMessage("unsupported message type");
  return;
}

void onWSEvent(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
  case WStype_CONNECTED:
    Serial.println("WS Connected");
    break;
  case WStype_DISCONNECTED:
    Serial.println("WS Disconnected");
    break;
  case WStype_TEXT:
    Serial.printf("WS Message: %s\n", payload);

    handleMessage(payload);

    break;
  }
}

void setup() {
  Serial.begin(921600);
  pinMode(LED_BUILTIN, OUTPUT);

  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

  while (wifiMulti.run() != WL_CONNECTED) {
    delay(100);
  }

  Serial.println("Connected");

  wsClient.beginSSL(WS_HOST, WS_PORT, WS_URL, "", "wss");
  wsClient.onEvent(onWSEvent);
}

void loop() {
  digitalWrite(LED_BUILTIN, WiFi.status() == WL_CONNECTED);
  wsClient.loop();
}
