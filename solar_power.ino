#include <WiFi.h>
#include <PubSubClient.h>
#include <math.h>
#include "esp_pm.h"
#include "esp_system.h"

// WLAN-Zugangsdaten
const char* ssid = "<ssid>";
const char* password = "<pass>";

// MQTT-Einstellungen
const char* mqtt_server = "homeassistant.local";
const char* mqtt_user = "<user>";
const char* mqtt_password = "<pass>";
const char* mqtt_topic = "solar/ac/power";

// Pins
const int dacPin = 17;     // DAC-Ausgang
const int ledPin = 15;     // PWM-fähiger Pin für LED-Helligkeit

// Konstanten
const float maxWatt = 800.0;

int currentDAC = 0;

WiFiClient espClient;
PubSubClient client(espClient);

// Funktionsdeklarationen
void mqttCallback(char* topic, byte* payload, unsigned int length);
void reconnectMQTT();
int convertToDAC(float watt);
int convertToPWM(float watt);

void setup() {
  setCpuFrequencyMhz(60);

  pinMode(ledPin, OUTPUT);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    analogWrite(ledPin, 0); delay(125);
    analogWrite(ledPin, 255); delay(125);
    analogWrite(ledPin, 0); delay(125);
    analogWrite(ledPin, 255); delay(125);
  }

  client.setServer(mqtt_server, 1883);
  client.setCallback(mqttCallback);

  sweepGauge();
}


void loop() {
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();
}

void reconnectMQTT() {
  while (!client.connected()) {
    if (client.connect("ESP32S2Client", mqtt_user, mqtt_password)) {
      client.subscribe(mqtt_topic);
    } else {
      analogWrite(ledPin, 0); delay(500);
      analogWrite(ledPin, 255); delay(500);
      analogWrite(ledPin, 0); delay(500);
      analogWrite(ledPin, 255); delay(500);
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  float watt = msg.toFloat();

  int pwmValue = convertToPWM(watt);
  analogWrite(ledPin, 0); delay(20);
  analogWrite(ledPin, 255); delay(20);
  analogWrite(ledPin, pwmValue);

  int targetDAC = convertToDAC(watt);
  int diffDAC = targetDAC - currentDAC;

  int stepDuration = 10;
  int transitionDuration = 2500;
  int steps = transitionDuration / stepDuration;

  for (int i = 1; i <= steps; i++) {
    float t = (float)i / (float)steps;  // 0.0 → 1.0
    float eased = (1 - cos(t * PI)) / 2; // sinus easing
    int newDAC = currentDAC + diffDAC * eased;

    dacWrite(dacPin, newDAC);
    delay(stepDuration);
  }

  currentDAC = targetDAC;
}

int convertToDAC(float watt) {
  watt = constrain(watt, 0.0, maxWatt);
  float ratio = watt / maxWatt;
  return int(ratio * 255.0);
}

int convertToPWM(float watt) {
  watt = constrain(watt, 0.0, maxWatt);
  float ratio = watt / maxWatt;
  return int(ratio * 255.0);
}

void sweepGauge() {
  const int stepDelay = 10;
  const int duration  = 1000;
  int steps = duration / stepDelay;

  for (int i = 0; i <= steps; i++) {
    float t = (float)i / (float)steps;
    float eased = (1 - cos(t * PI)) / 2;
    int newDAC = (int)(eased * 255.0);

    dacWrite(dacPin, newDAC);
    delay(stepDelay);
  }

  for (int i = 0; i <= steps; i++) {
    float t = (float)i / (float)steps;
    float eased = (1 - cos(t * PI)) / 2;
    int newDAC = (int)((1.0 - eased) * 255.0);

    dacWrite(dacPin, newDAC);
    delay(stepDelay);
  }

  currentDAC = 0;
}

