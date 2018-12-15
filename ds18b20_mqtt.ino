#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <OneWire.h>
#include <DallasTemperature.h>


/************ WIFI and MQTT Information******************/
const char* ssid = "SSID";
const char* password = "WIFI_PASSWORD";
const char* mqtt_server = "BROKER_IP";
const char* mqtt_username = "USERNAME";
const char* mqtt_password = "PASSWORD";
const int mqtt_port = 1883;

/**************************** FOR OTA **************************************************/
#define EPSNAME "freezer"
#define OTApassword "PASSWORD"
int OTAport = 8266;

/************* MQTT TOPICS  **************************/
const char* temperature_topic = "freezer/temperature";
const char* availability_topic = "freezer/availability";

const char* birthMessage = "online";
const char* lwtMessage = "offline";

/*********************************** PIN Defintions ********************************/
#define ONE_WIRE_BUS    5
OneWire oneWire(ONE_WIRE_BUS);

WiFiClient espClient;
PubSubClient client(espClient);

DallasTemperature DS18B20(&oneWire);

long lastMsg = 0;
float temp = 0;

/********************************** START SETUP*****************************************/
void setup() {
  Serial.begin(115200);
  delay(10);

  DS18B20.begin();

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);

  //OTA SETUP
  ArduinoOTA.setPort(OTAport);
  ArduinoOTA.setHostname(EPSNAME);

  // No authentication by default
  ArduinoOTA.setPassword((const char *)OTApassword);

  ArduinoOTA.onStart([]() {
    Serial.println("Starting");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

  Serial.println("Ready");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

}

/********************************** START SETUP WIFI*****************************************/
void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void publish_all_states() {
  long now = millis();
  if (now - lastMsg > 60000) {
    lastMsg = now;
    DS18B20.setResolution(12);
    DS18B20.requestTemperatures(); // Send the command to get temperatures
    if (DS18B20.getTempCByIndex(0) != temp) {
      temp = DS18B20.getTempCByIndex(0);
      Serial.println(temp);
      if ((temp > -20) && (temp < 60))
      {
        client.publish(temperature_topic, String(temp).c_str(), true);
      }
    }
  }
}


/********************************** START RECONNECT*****************************************/
void reconnect() {
  Serial.print("Attempting MQTT connection...");
  // Attempt to connect
  if (client.connect(EPSNAME, mqtt_username, mqtt_password, availability_topic, 0, true, lwtMessage)) {
    Serial.println("Connected!");

    // Publish the birth message on connect/reconnect
    publish_birth_message();

    publish_all_states();

  }
  else {
    Serial.print("failed, rc=");
    Serial.print(client.state());
    delay(5000);
  }
}

void publish_birth_message() {
  // Publish the birthMessage
  Serial.print("Publishing birth message \"");
  Serial.print(birthMessage);
  Serial.print("\" to ");
  Serial.print(availability_topic);
  Serial.println("...");
  client.publish(availability_topic, birthMessage, true);
}


/********************************** START MAIN LOOP*****************************************/
void loop() {

  if (!client.connected()) {
    reconnect();
  }

  if (WiFi.status() != WL_CONNECTED) {
    delay(1);
    Serial.print("WIFI Disconnected. Attempting reconnection.");
    setup_wifi();
    return;
  }

  publish_all_states();

  client.loop();

  ArduinoOTA.handle();

}
