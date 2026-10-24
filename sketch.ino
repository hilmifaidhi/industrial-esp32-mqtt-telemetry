#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ArduinoJson.h>

// --- KONFIGURASI WIFI & MQTT BROKER ---
const char* WIFI_SSID = "Wokwi-GUEST"; // Default Wi-Fi gratisan Wokwi
const char* WIFI_PASSWORD = "";
const char* MQTT_SERVER = "broker.hivemq.com"; // Public Broker gratis untuk tes
const int MQTT_PORT = 1883;

// Topic MQTT khusus untuk proyek kita
const char* MQTT_TOPIC_PUB = "industrial/telemetry/hilmi/data";

// --- PIN DEFINITION ---
#define DHTPIN 15
#define DHTTYPE DHT22
#define TRIG_PIN 5
#define ECHO_PIN 18
#define LED_PIN 2

DHT dht(DHTPIN, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0;
const long interval = 2000; // Kirim data tiap 2 detik

void setupWiFi() {
  delay(100);
  Serial.println("\n--- Menghubungkan ke WiFi ---");
  Serial.print("Connecting to: ");
  Serial.println(WIFI_SSID);

  // Cukup panggil begin tanpa WiFi.mode(WIFI_STA) untuk simulasi Wokwi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20) { // Timeout 10 detik
    delay(500);
    Serial.print(".");
    timeout++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Terhubung!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi Gagal Terhubung, mencoba ulang...");
  }
}

void reconnectMQTT() {
  // Loop sampai terhubung ke MQTT Broker
  while (!client.connected()) {
    Serial.print("Mencoba koneksi ke MQTT Broker...");
    
    // Bikin Client ID unik pakai random angka
    String clientId = "ESP32-Hilmi-";
    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str())) {
      Serial.println(" Terhubung!");
    } else {
      Serial.print(" Gagal, rc=");
      Serial.print(client.state());
      Serial.println(" Coba lagi dalam 5 detik...");
      delay(5000);
    }
  }
}

float bacaJarak() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return -1.0;
  return (duration * 0.0343) / 2.0;
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);

  dht.begin();
  setupWiFi();
  
  client.setServer(MQTT_SERVER, MQTT_PORT);
}

void loop() {
  // Cek koneksi MQTT, jika putus maka re-connect
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();

  // Pengiriman data non-blocking menggunakan millis()
  unsigned long now = millis();
  if (now - lastMsg > interval) {
    lastMsg = now;

    float temp = dht.readTemperature();
    float hum = dht.readHumidity();
    float dist = bacaJarak();

    if (isnan(temp) || isnan(hum)) {
      Serial.println("Gagal membaca dari sensor DHT!");
      return;
    }

    // --- BIKIN PAYLOAD DATA DENGAN ARDUINOJSON ---
    StaticJsonDocument<200> doc;
    doc["device_id"] = "ESP32_IND_01";
    doc["temperature"] = temp;
    doc["humidity"] = hum;
    doc["distance_cm"] = dist;
    doc["status"] = (temp > 40.0 || dist < 10.0) ? "WARNING" : "NORMAL";

    char jsonBuffer[256];
    serializeJson(doc, jsonBuffer);

    // --- PUBLISH DATA KE MQTT BROKER ---
    client.publish(MQTT_TOPIC_PUB, jsonBuffer);
    Serial.print("Data Terkirim via MQTT: ");
    Serial.println(jsonBuffer);

    // Indikator Alert lokal
    if (temp > 40.0 || dist < 10.0) {
      digitalWrite(LED_PIN, HIGH);
    } else {
      digitalWrite(LED_PIN, LOW);
    }
  }
}