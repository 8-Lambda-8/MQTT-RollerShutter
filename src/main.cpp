#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h>

uint8_t RelayPins[] = {14, 12};

#include "mqttPwd.h"
#include "wifiPwd.h"

const char* mqtt_server = "PiServer";

WiFiClient espClient;
PubSubClient client(espClient);

long mill, mqttConnectMillis, wifiConnectMillis;

const String id = "0";

const String RollerTopic = "/Roller/" + id + '/';
const String host = "Rolladen_" + id;

const char* version = __DATE__ " / " __TIME__;

char* str2ch(String command) {
  if (command.length() != 0) {
    char* p = const_cast<char*>(command.c_str());
    return p;
  }
  return const_cast<char*>("");
}

void setup_wifi() {
  if (WiFi.status() != WL_CONNECTED) {
    delay(10);
    // We start by connecting to a WiFi network
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);
    WiFi.setHostname(str2ch(host));

    delay(500);
    randomSeed(micros());
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("");
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
    }
  }
}

void SwitchRelay(uint8_t x, boolean b) {
  Serial.print("switching Relay ");
  Serial.print(x);
  Serial.print(" on Pin ");
  Serial.print(RelayPins[x]);
  Serial.print(" to ");
  Serial.println(b);

  digitalWrite(RelayPins[x], b);  // invert because Relay is on when on GND
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.printf("\nMessage arrived [%s](L=%d)\n", topic, length);
  /* Serial.print(topic);
  Serial.print("] "); */
  String msg = "";
  for (uint16_t i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    msg += (char)payload[i];
  }
  Serial.println();

  String topicStr = String(topic);

  if (topicStr.startsWith(RollerTopic + "state")) {
    SwitchRelay(0, false);
    SwitchRelay(1, false);
    delay(200);
    if (payload[0] == '1') {  // up
      SwitchRelay(0, true);
      SwitchRelay(1, false);
    } else if (payload[0] == '2') {  // down
      SwitchRelay(0, false);
      SwitchRelay(1, true);
    }
  }
}

void reconnect() {
  // Loop until we're reconnected
  if (WiFi.status() == WL_CONNECTED && !client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = host;
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pwd,
                       str2ch(RollerTopic + "Status"), 0, true,
                       str2ch("OFFLINE"))) {
      Serial.println("connected");

      client.subscribe(str2ch(RollerTopic + "#"));

      Serial.println("Publishing IP: " + WiFi.localIP().toString());
      client.publish(str2ch(RollerTopic + "IP"),
                     str2ch(WiFi.localIP().toString()), true);
      client.publish(str2ch(RollerTopic + "Version"), version, true);

    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
    }
  }
}

void setup() {
  Serial.begin(74880);
  Serial.println("Start");

  pinMode(RelayPins[0], OUTPUT);
  pinMode(RelayPins[1], OUTPUT);
  SwitchRelay(0, false);
  SwitchRelay(1, false);

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  MDNS.begin(host);
  MDNS.addService("http", "tcp", 80);

  ArduinoOTA.begin();
  ArduinoOTA.setHostname(str2ch(host));

  reconnect();
}

void loop() {
  ArduinoOTA.handle();

  if ((millis() - wifiConnectMillis) > 10000) {
    setup_wifi();
    wifiConnectMillis = millis();
  }

  if ((millis() - mqttConnectMillis) > 5000) {
    reconnect();
    mqttConnectMillis = millis();
  }

  client.loop();

  if ((millis() - mill) > 10000) {
    client.publish(str2ch(RollerTopic + "Status"), str2ch("ONLINE"), true);
    mill = millis();
  }
}