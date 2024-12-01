#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Servo.h>
#include <Ultrasonic.h>
#include <DHT.h>

#define DHTPIN 15        // Pin yang digunakan untuk sensor DHT
#define DHTTYPE DHT11    // Tipe sensor DHT
#define TRIG_PIN 0      // Pin untuk Trigger
#define ECHO_PIN 4      // Pin untuk Echo
#define BUZZER_PIN 13     // Pin untuk Buzzer
#define SERVO1_PIN 1     // Pin untuk Servo
#define SERVO2_PIN 2     // Pin untuk Servo
#define sensorPin A0     // Pin untuk Sensor Proximity

// Definisi WiFi dan MQTT Broker
const char* ssid = "Lab PnP";               // Nama WiFi
const char* password = "12345678";        // Password WiFi
const char* mqttServer = "mqttakbar.sija-bridge.tech"; // Alamat broker Mosquitto
const int mqttPort = 3333;                // Port MQTT default
const char* topicProximity = "gopilah/proxi"; // Topik untuk publish proximity
const char* topicHumidity = "gopilah/kelembaban"; // Topik untuk publish kelembaban
const char* topicTemperature = "gopilah/suhu"; // Topik untuk publish suhu

int threshold = 20; // Threshold untuk menentukan "tidak ada objek"
DHT dht(DHTPIN, DHTTYPE);
Servo myServo1;   //
Servo myServo2;  // Buat objek servo

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  Serial.begin(115200);          // Mulai Serial Monitor dengan baud rate 115200
  dht.begin();
  pinMode(TRIG_PIN, OUTPUT);     // Set TRIG_PIN sebagai OUTPUT
  pinMode(ECHO_PIN, INPUT);      // Set ECHO_PIN sebagai INPUT
  pinMode(BUZZER_PIN, OUTPUT);    // Set BUZZER_PIN sebagai OUTPUT

  // Set motor off pada awal
  myServo2.attach(SERVO1_PIN); 
  myServo2.attach(SERVO2_PIN);     // Hubungkan servo ke pin D4

  // Hubungkan ke WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");

  // Set server MQTT
  client.setServer(mqttServer, mqttPort);
}

void loop() {
  float suhu = dht.readTemperature();
  float kelembaban = dht.readHumidity();

  // Pastikan client terhubung ke broker
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  long duration, distance;

  // Mengatur TRIG_PIN ke HIGH selama 10 mikrodetik
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Membaca durasi dari ECHO_PIN
  duration = pulseIn(ECHO_PIN, HIGH);

  // Menghitung jarak dalam cm
  distance = (duration * 0.0343) / 2;

  // Menampilkan jarak ke Serial Monitor
  Serial.print("Jarak: ");
  Serial.print(distance);
  Serial.println(" cm");

  // Cek jarak, jika kurang dari 10 cm
  if (distance < 10) {
    digitalWrite(BUZZER_PIN, HIGH); // Nyalakan buzzer
    myServo2.write(90);   // Gerakkan servo ke sudut 90 derajat
  } else {
    digitalWrite(BUZZER_PIN, LOW);  // Matikan buzzer
    myServo2.write(0);               // Kembalikan servo ke sudut 0 derajat
  }

  if (isnan(suhu) || isnan(kelembaban)) {
    Serial.println("Gagal membaca dari DHT sensor, motor berhenti.");
    myServo1.write(0);
    return;
  }

  Serial.print("Suhu: ");
  Serial.print(suhu);
  Serial.println(" *C");

  Serial.print("Kelembaban: ");
  Serial.print(kelembaban);
  Serial.println(" %");

  client.publish(topicTemperature, String(suhu).c_str());
  client.publish(topicHumidity, String(kelembaban).c_str());

  // Publish pesan ke topik proximity
  String message; // Deklarasikan variabel message di sini

  int sensorValue = analogRead(sensorPin); // Membaca nilai analog

  // Tentukan pesan yang akan dikirim berdasarkan nilai sensor
  if (sensorValue > threshold) {
    Serial.println("Nilai sensor: ");
    Serial.println(sensorValue);
    myServo1.write(90);  // Arah motor ke kiri
    message = "Sampah terdeteksi: Jenis sampah adalah logam/anorganik";
  } else {
    Serial.println("Tidak ada objek");
    message = "Tidak ada sampah terdeteksi";
    if (kelembaban > 55 && suhu > 27) {
      Serial.println("Sampah Organik - Motor ke arah kanan");
      myServo1.write(-90);  // Arah motor ke kanan
    } else {
      Serial.println("Sampah tidak teridentifikasi, motor berhenti.");
      myServo1.write(0);  // Motor berhenti
    }
    
    if (kelembaban <= 55) {
      Serial.println("Sampah Anorganik - Motor ke arah kiri");
      myServo1.write(90);;  // Arah motor ke kiri
    } else {
      Serial.println("Sampah tidak teridentifikasi, motor berhenti.");
      myServo1.write(0);  // Motor berhenti
    }
  }

  // Publish pesan proximity
  client.publish(topicProximity, message.c_str());
  Serial.print("Message sent: "); // Tampilkan pesan yang dikirim di Serial Monitor
  Serial.println(message);


  delay(1500);
}

void reconnect() {
  // Loop hingga terhubung kembali
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect("ESP8266Client")) {
      Serial.println("Connected to MQTT Broker!");
    } else {
      Serial.print("Failed with state: ");
      Serial.println(client.state());
      delay(2000);
    }
  }
}
