#include <ESP8266WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include <DHT.h>
#include <NewPing.h>
#include "secrets.h"  // contains WIFI_SSID, WIFI_PASS, AIO_USERNAME, AIO_KEY

// ------------------------------
//   Config
// ------------------------------
#define SIMULATION 1   // set to 0 if running on hardware

#define AIO_SERVER     "io.adafruit.com"
#define AIO_SERVERPORT 1883

// Pins
#define DHTPIN D5
#define DHTTYPE DHT11
#define MOISTURE_PIN A0
#define TRIG_PIN D6
#define ECHO_PIN D7

// Loop timing (slower in real mode)
#define LOOP_DELAY_MS  (SIMULATION ? 5000 : 15000)

// ------------------------------
//   WiFi + MQTT Setup
// ------------------------------
WiFiClient net;
Adafruit_MQTT_Client mqtt(&net, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// Sensors
DHT dht(DHTPIN, DHTTYPE);
NewPing sonar(TRIG_PIN, ECHO_PIN);

// ------------------------------
//   Feeds
// ------------------------------
Adafruit_MQTT_Publish tempFeed  = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/temperature");
Adafruit_MQTT_Publish humidFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/humidity");
Adafruit_MQTT_Publish moistFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/moisture");
Adafruit_MQTT_Publish distFeed  = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/distance");
Adafruit_MQTT_Publish alertFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/alerts");

// NEW: subscribe to mode feed (focus / relax / sleep)
Adafruit_MQTT_Subscribe modeSub = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/mode");

// ------------------------------
//   Thresholds + State
// ------------------------------
const int MOISTURE_EMPTY = 400; // above this = "glass present"
int CLOSE_DISTANCE_CM = 20;     // default
unsigned long HYDRATE_INTERVAL = 30UL * 60UL * 1000UL; // 30 min (ms)

unsigned long lastDrinkTime = 0;
unsigned long lastConnectTry = 0;

// ------------------------------
//   WiFi + MQTT Connection
// ------------------------------
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("WiFi: connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
}

void MQTT_connect() {
  if (mqtt.connected()) return;
  if (millis() - lastConnectTry < 2000) return; // avoid spamming
  Serial.print("MQTT: connecting... ");
  int8_t ret = mqtt.connect();
  if (ret == 0) Serial.println("OK");
  else {
    Serial.println(mqtt.connectErrorString(ret));
    mqtt.disconnect();
  }
  lastConnectTry = millis();
}

// ------------------------------
//   Mode Handler
// ------------------------------
void applyMode(const char* mode) {
  String m = String(mode);
  m.trim(); m.toLowerCase();

  if (m == "focus") {
    CLOSE_DISTANCE_CM = 25;
    HYDRATE_INTERVAL = 20UL * 60UL * 1000UL;
  } else if (m == "relax") {
    CLOSE_DISTANCE_CM = 30;
    HYDRATE_INTERVAL = 45UL * 60UL * 1000UL;
  } else if (m == "sleep") {
    CLOSE_DISTANCE_CM = 15;
    HYDRATE_INTERVAL = 120UL * 60UL * 1000UL;
  } else {
    Serial.print("[mode] unknown: ");
    Serial.println(mode);
    return;
  }

  Serial.printf("[mode] %s -> close<%dcm hydrate=%lumin\n",
                m.c_str(), CLOSE_DISTANCE_CM, HYDRATE_INTERVAL / 60000UL);
}

// ------------------------------
//   Setup
// ------------------------------
void setup() {
  Serial.begin(115200);
  randomSeed(ESP.getChipId());

  if (!SIMULATION) dht.begin();

  connectWiFi();

  // Subscribe to mode updates
  mqtt.subscribe(&modeSub);

  MQTT_connect();
}

// ------------------------------
//   Main Loop
// ------------------------------
void loop() {
  MQTT_connect();

  // Handle incoming subscriptions (mode updates)
  Adafruit_MQTT_Subscribe *sub;
  while ((sub = mqtt.readSubscription(10))) {
    if (sub == &modeSub) {
      applyMode((const char*)modeSub.lastread);
    }
  }

  // ------------------------------
  //   Sensor Reading
  // ------------------------------
  float tempC, humidPct;
  int moisture, distanceCm;

  if (SIMULATION) {
    tempC = random(180, 300) / 10.0; // 18–30 °C
    humidPct = random(30, 70);
    moisture = random(380, 420);
    distanceCm = random(10, 50);
  } else {
    tempC = dht.readTemperature();
    humidPct = dht.readHumidity();
    moisture = analogRead(MOISTURE_PIN);
    distanceCm = sonar.ping_cm();
  }

  Serial.printf("T: %.1fC H:%d%% M:%d D:%dcm | close=%d hydrate=%lus\n",
                tempC, (int)humidPct, moisture, distanceCm,
                CLOSE_DISTANCE_CM, HYDRATE_INTERVAL / 1000UL);

  // ------------------------------
  //   Publish telemetry
  // ------------------------------
  tempFeed.publish(tempC);
  humidFeed.publish(humidPct);
  moistFeed.publish(moisture);
  distFeed.publish(distanceCm);

  // ------------------------------
  //   Rule logic
  // ------------------------------
  unsigned long now = millis();

  // Hydration tracking
  if (moisture > MOISTURE_EMPTY) lastDrinkTime = now;
  if (now - lastDrinkTime > HYDRATE_INTERVAL) {
    alertFeed.publish("Reminder: Take a sip of water!");
    lastDrinkTime = now;
  }

  // Distance check
  if (distanceCm > 0 && distanceCm < CLOSE_DISTANCE_CM) {
    alertFeed.publish("You're too close to the screen!");
  }

  // Comfort alerts
  if (tempC > 28) alertFeed.publish("It's hot — open a window!");
  else if (tempC < 18) alertFeed.publish("It's cold — consider heating.");
  if (humidPct < 40) alertFeed.publish("Air is dry — ventilate or hydrate.");

  delay(LOOP_DELAY_MS);
}


