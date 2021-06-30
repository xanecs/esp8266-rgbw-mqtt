#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "config.h"
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include <ArduinoJson.h>
#include "state.hpp"

WiFiClientSecure client;
Adafruit_MQTT_Client mqtt(&client, MQTT_HOST, MQTT_PORT);
Adafruit_MQTT_Subscribe commandTopic(&mqtt, MQTT_TOPIC_COMMAND);
Adafruit_MQTT_Publish stateTopic(&mqtt, MQTT_TOPIC_STATE);

State state;


void setupWifi();
void ensureConnection();
void onCommand(char* data, uint16_t len);
void applyState();
void sendState();

void setup()
{
	static uint8_t pins[] = {PIN_R, PIN_G, PIN_B, PIN_W};
	for (uint8_t i = 0; i < sizeof(pins); i++) {
		pinMode(pins[i], OUTPUT);
		digitalWrite(pins[i], LOW);
	}

	Serial.begin(115200);
	Serial.println();
	setupWifi();
	client.setInsecure();
	commandTopic.setCallback(onCommand);
	mqtt.subscribe(&commandTopic);
}

void setupWifi() {
	Serial.print("Connecting WiFi");
	WiFi.mode(WIFI_STA);
	WiFi.begin(WIFI_SSID, WIFI_PSK);

	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}
	Serial.println();
	Serial.println(WiFi.localIP());
}

void loop()
{
	ensureConnection();
	mqtt.processPackets(10000);

	if (!mqtt.ping()) {
		mqtt.disconnect();
	}

}

void ensureConnection() {
	int8_t ret;

	if (mqtt.connected()) return;

	Serial.print("Connecting MQTT...");
	uint8_t retries = 3;
	while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
		Serial.println(mqtt.connectErrorString(ret));
		Serial.println("Retrying MQTT connection in 5 seconds...");
		mqtt.disconnect();
		delay(5000);  // wait 5 seconds
		retries--;
		if (retries == 0) {
			// basically die and wait for WDT to reset me
			while (1);
		}
	}
	Serial.println("OK");
	sendState();
}

void onCommand(char *data, uint16_t len) {
	Serial.println("Received message");
	static StaticJsonDocument<256> doc;
	DeserializationError err = deserializeJson(doc, data, len);
	if (err) {
		Serial.print("invalid message:");
		Serial.println(err.f_str());
		return;
	}

	if (doc.containsKey("state")) {
		Serial.print("State ");
		state.power = strcmp(doc["state"], "ON") == 0;
	}
	if (doc.containsKey("brightness")) {
		Serial.print("Brightness ");
		state.brightness = doc["brightness"];
	}
	if (doc.containsKey("white_value")) {
		Serial.print("White ");
		state.w = doc["white_value"];
	}
	if (doc.containsKey("color")) {
		Serial.print("Color ");
		JsonObject color = doc["color"];
		if (color.containsKey("r")) {
			state.r = color["r"];
		}
		if (color.containsKey("g")) {
			state.g = color["g"];
		}
		if (color.containsKey("b")) {
			state.b = color["b"];
		}
	}
	Serial.println();

	applyState();
	sendState();
}

void applyState() {
	if (!state.power) {
		analogWrite(PIN_R, 0);
		analogWrite(PIN_G, 0);
		analogWrite(PIN_B, 0);
		analogWrite(PIN_W, 0);
		return;
	}
	uint16_t r = (uint32_t)state.brightness * (uint32_t)state.r;
	uint16_t g = (uint32_t)state.brightness * (uint32_t)state.g;
	uint16_t b = (uint32_t)state.brightness * (uint32_t)state.b;

	uint16_t out_r = map(r, 0, 255*255, 0, 1023);
	uint16_t out_g = map(g, 0, 255*255, 0, 1023);
	uint16_t out_b = map(b, 0, 255*255, 0, 1023);
	uint16_t out_w = map(state.w, 0, 255, 0, 1023);

	analogWrite(PIN_R, out_r);
	analogWrite(PIN_G, out_g);
	analogWrite(PIN_B, out_b);
	analogWrite(PIN_W, out_w);
}

void sendState() {
	Serial.println("Sending state");
	static StaticJsonDocument<256> doc;
	static char outputBuffer[256];
	doc["state"] = state.power ? "ON" : "OFF";
	doc["white_value"] = state.w;
	doc["brightness"] = state.brightness;

	JsonObject color = doc.createNestedObject("color");
	color["r"] = state.r;
	color["g"] = state.g;
	color["b"] = state.b;

	serializeJson(doc, outputBuffer);
	Serial.println(outputBuffer);
	if (!stateTopic.publish(outputBuffer)) {
		Serial.println("Failed to publish state");
	}
}