#include <OneWire.h>
#include <DallasTemperature.h>
#include "UbidotsESPMQTT.h"

// Token dan detail WiFi untuk Ubidots
#define TOKEN "BBUS-kpFW4GtN4Q4sUVJVnYljDbvNjhHtmn" // Ganti dengan TOKEN Ubidots Anda
#define WIFINAME "Naira" // Ganti dengan SSID WiFi Anda
#define WIFIPASS "yovan111" // Ganti dengan password WiFi Anda

Ubidots client(TOKEN);

// Pin untuk sensor suhu DS18B20
#define ONE_WIRE_BUS D4 // Menghubungkan pin data DS18B20 ke D4

// Mengatur OneWire dan DallasTemperature library
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Pin untuk relay
int relay1 = D1; // Pin untuk Relay 1
int relay2 = D2; // Pin untuk Relay 2

// Pin saluran CD4051
int turbiditySensorPin = 0; // Saluran Y0 untuk sensor kekeruhan pada CD4051
int pHSensorPin = 1;        // Saluran Y1 untuk sensor pH pada CD4051
int tempSensorPin = 2;      // Saluran Y2 untuk sensor suhu pada CD4051

// Variabel untuk manajemen waktu relay
unsigned long relay1StartTime = 0; // Waktu mulai untuk relay 1
unsigned long relay2StartTime = 0; // Waktu mulai untuk relay 2
bool relay1Active = false;         // Status relay 1
bool relay2Active = false;         // Status relay 2

const unsigned long relayDuration = 30 * 60 * 1000; // Durasi aktif relay dalam milidetik (30 menit)

// Pin kontrol untuk CD4051
int S1 = D5;
int S2 = D6;
int S3 = D7;

// Konstanta untuk kalibrasi pH
const float pHCalibrationValue = 6.86; // Nilai kalibrasi cairan pH
const int pHCalibrationRaw = 512;      // Nilai ADC pada cairan kalibrasi pH
const float pHSlope = 3.5;             // Estimasi perbedaan pH per 1023 nilai ADC

// Fungsi callback untuk Ubidots
void callback(char *topic, byte *payload, unsigned int length) {
  Serial.print("Pesan diterima [");
  Serial.print(topic);
  Serial.print("]: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void selectChannel(int channel) {
  digitalWrite(S1, channel & 0x01);         // Bit paling rendah
  digitalWrite(S2, (channel >> 1) & 0x01); // Bit tengah
  digitalWrite(S3, (channel >> 2) & 0x01); // Bit tertinggi
}

float calculatePH(int rawValue) {
  return pHCalibrationValue + ((rawValue - pHCalibrationRaw) / 1023.0) * pHSlope;
}

void setup() {
  Serial.begin(115200);

  // Inisialisasi relay
  pinMode(relay1, OUTPUT);
  pinMode(relay2, OUTPUT);
  digitalWrite(relay1, HIGH); // Memastikan relay 1 mati
  digitalWrite(relay2, HIGH); // Memastikan relay 2 mati

  // Inisialisasi pin CD4051
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT);

  // Inisialisasi sensor suhu
  sensors.begin();

  // Setup koneksi ke Ubidots
  client.setDebug(true);
  client.wifiConnection(WIFINAME, WIFIPASS);
  client.begin(callback);
}

void loop() {
  // Pastikan terhubung ke Ubidots
  if (!client.connected()) {
    client.reconnect();
  }

  // Membaca nilai dari sensor turbidity
  selectChannel(turbiditySensorPin);
  int turbidityValue = analogRead(A0);
  float ntuValue = map(turbidityValue, 0, 1023, 0, 10);
  Serial.print("Nilai Kekeruhan (NTU): ");
  Serial.println(ntuValue);

  // Membaca nilai dari sensor pH
  selectChannel(pHSensorPin);
  int pHRawValue = analogRead(A0);
  float pH = calculatePH(pHRawValue);
  Serial.print("Nilai pH: ");
  Serial.println(pH);

  // Membaca nilai dari sensor suhu
  selectChannel(tempSensorPin);
  sensors.requestTemperatures();
  float temperatureC = sensors.getTempCByIndex(0);
  Serial.print("Suhu Air (C): ");
  Serial.println(temperatureC);

  // Logika untuk kontrol relay berdasarkan nilai kekeruhan
  if (ntuValue < 10) {
    digitalWrite(relay1, HIGH);
    digitalWrite(relay2, HIGH);
    relay1Active = false;
    relay2Active = false;
    Serial.println("Relay OFF (Kekeruhan < 10 NTU)");
  } else {
    if (!relay1Active) {
      relay1StartTime = millis();
      digitalWrite(relay1, LOW);
      relay1Active = true;
      Serial.println("Relay 1 ON (Kekeruhan >= 10 NTU)");
    }
  }

  if (relay1Active && (millis() - relay1StartTime >= relayDuration)) {
    digitalWrite(relay1, HIGH);
    relay1Active = false;
    relay2StartTime = millis();
    digitalWrite(relay2, LOW);
    relay2Active = true;
    Serial.println("Relay 1 OFF setelah 30 menit, Relay 2 ON");
  }

  if (relay2Active && (millis() - relay2StartTime >= relayDuration)) {
    digitalWrite(relay2, HIGH);
    relay2Active = false;
    Serial.println("Relay 2 OFF setelah 30 menit");
  }

  // Mengirim data ke Ubidots
  client.add("turbidity", ntuValue);
  client.add("pH", pH);
  client.add("temperature", temperatureC);
  client.ubidotsPublish("water-monitoring");

  client.loop();
  delay(1000);
}
