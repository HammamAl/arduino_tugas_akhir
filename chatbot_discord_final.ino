#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

// Pin Sensor
#define DHTPIN 17
const int ammoniaSensorPin = 35;
const int relayPin = 32;

// Nama SSID default dan password untuk fallback
const char* fallbackSSID = "karim";
const char* fallbackPassword = "960929ka";

// Reconnect wifi
unsigned long lastWiFiAttempt = 0;
const unsigned long wifiRetryInterval = 30000;  // 30 detik
int wifiReconnectAttempts = 0;
const int maxWiFiReconnectAttempts = 10;
bool isAlreadyConnect = false;


// HiveMQ MQTT Broker Configuration
const char* mqtt_server = "499b37cd93464a848333539b957a57ef.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_user = "percobaan1";
const char* mqtt_password = "Percobaan2024";

// MQTT topic and subtopic
const char* mqtt_relay_control_topic = "esp32/relay";
const char* mqtt_heartbeat_topic = "esp32/heartbeat";
const char* mqtt_restart_topic = "esp32/restart";
const char* mqtt_isonline_topic = "esp32/isonline";
const char* mqtt_relay_status_topic = "relay/notifications";
const char* mqtt_relay_setting_topic = "relay/setting";
const char* mqtt_ammonia_threshold_topic = "relay/ammonia";
const char* mqtt_sensor_data_topic = "sensor/data";
const char* mqtt_wifi_topic = "sensor/wifi";
const char* mqtt_voltage_mems_topic = "sensor/voltage";

// Interval Publish Data ke MQTT
unsigned long lastPublishTime = 0;            // Untuk pengiriman data MQTT
const unsigned long publishInterval = 15000;  // Publikasikan data
bool publishDefaultData = true;

// Interval updateLCD, handleData
unsigned long previousMillisLCD = 0;
const long intervalLCD = 1000;  // intervalLCD 1 detik

// MEMS NH3 Konfigurasi
const float voltageInput = 5.0;
const int numReadings = 10;
float ppm = 0;
float readings[numReadings];
int readIndex = 0;
float total = 0;
float average = 0;

// DHT21 Konfigurasi
#define DHTTYPE DHT21  // Define sensor type DHT21
DHT dht(DHTPIN, DHTTYPE);
float suhu;
int kelembapan;

// Relay Konfigurasi
bool relayActive = false;
bool isManualMode = false;  // Menandakan apakah relay dalam mode manual
bool toggleManualRelay = false;
bool toggleRelay = false;
String status_mode = "Otomatis";
unsigned long lastRelayChange = 0;
unsigned long relayOnDuration = 5000;       // 5 detik ON
unsigned long relayOffDuration = 25200000;  // 7 jam OFF
unsigned long manualRelayDuration = 0;      // timer relay manual
unsigned long relayStartTime = 0;

// Buzzer
const int buzzerPin = 18;
unsigned long buzzerStartTime = 0;
bool toggleBuzzer = false;
bool toggleBuzzerRelayAutoOn = false;
bool buzzerOn = false;


// Ambang batas amonia
float ammonia_threshold = 15;

// Validasi mode offline/online
bool isOnline = false;

// Notification setting LCD
unsigned long notifStartTime = 0;
const unsigned long notifDuration = 5000;  // 5 detik
bool isShowingNotif = false;
bool isSettingRelayOn = false;
bool isSettingRelayOff = false;
bool isSettingAmmonia = false;
bool notifIsOnline = false;
bool isSettingRelayManualOn = false;
bool isNotifTimerOn = false;
bool isSetTimer = false;

// Inisiasi arduino JSON
StaticJsonDocument<200> data;

// ESP32 Networking Setup
WiFiClientSecure espClient;
PubSubClient client(espClient);

// LCD I2C
LiquidCrystal_I2C lcd(0x27, 20, 4);

// Preferences untuk menyimpan kredensial WiFi
Preferences preferences;

// Inisialisasi WebServer untuk konfigurasi WiFi
WebServer server(80);

// Deklarasi prototipe fungsi
void connectToFallback();

// Halaman web HTML untuk konfigurasi WiFi
const char* index_html = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
    <title>WiFi Configuration</title>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta charset="UTF-8">
    <link href="https://fonts.googleapis.com/css2?family=Roboto:wght@400;500;700&display=swap" rel="stylesheet">
    <style>
        body {
            font-family: 'Roboto', sans-serif;
            text-align: center;
            margin: 0;
            padding: 0;
            background: #f0f4f8;
            color: #333;
        }

        .container {
            max-width: 500px;
            margin: 50px auto;
            background: #ffffff;
            border-radius: 8px;
            box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
            padding: 20px;
        }

        h1 {
            font-size: 24px;
            margin-bottom: 10px;
            color: #1e88e5;
        }

        p {
            margin: 10px 0;
            color: #555;
        }

        label {
            display: block;
            text-align: left;
            font-weight: 500;
            margin-bottom: 5px;
            font-size: 14px;
        }

        input[type="text"], input[type="password"] {
            width: calc(100% - 20px);
            padding: 10px;
            margin: 10px 0 20px;
            border: 1px solid #ccc;
            border-radius: 4px;
            font-size: 16px;
            outline: none;
        }

        input[type="text"]:focus, input[type="password"]:focus {
            border-color: #1e88e5;
        }

        .btn {
            display: inline-block;
            background-color: #1e88e5;
            color: #fff;
            font-size: 16px;
            padding: 10px;
            width: calc(100% - 40px);
            border: none;
            border-radius: 4px;
            text-align: center;
            cursor: pointer;
            transition: background-color 0.3s ease;
        }

        .btn:hover {
            background-color: #1565c0;
        }

        .btn-danger {
            background-color: #e53935;
        }

        .btn-danger:hover {
            background-color: #d32f2f;
        }

        .button-container {
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            margin-top: 20px;
        }

        footer {
            text-align: center;
            margin-top: 20px;
            font-size: 13px;
            color: #777;
        }

        .disconnect {
            margin-top: 20px;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>WiFi Configuration</h1>
        <p>Masukkan SSID WiFi dan password untuk menghubungkan ESP32 ke WiFi Anda.</p>
        <p>Jika wifi tidak mempunyai password masukan "-" (tanpa petik).</p>
        <!-- Form untuk koneksi WiFi -->
        <form action="/setup" method="POST">
            <label for="ssid">WiFi SSID</label>
            <input type="text" id="ssid" name="ssid" placeholder="Masukkan SSID WiFi" required>
            <label for="password">Password</label>
            <input type="password" id="password" name="password" placeholder="Masukkan Password WiFi"required>
            <button type="submit" class="btn">Koneksikan WiFi</button>
        </form>

        <!-- Tombol putuskan koneksi -->
        <div class="button-container">
            <form action="/disconnect" method="GET" class="disconnect">
                <button type="submit" class="btn btn-danger">Putuskan Koneksi</button>
            </form>
        </div>
    </div>
    
    <footer>
        ESP32 Configuration Portal &copy; 2025
    </footer>
</body>
</html>
)rawliteral";

// Buzzer alert dengan delay
void buzzerAlert() {
  digitalWrite(buzzerPin, HIGH);
  delay(500);
  digitalWrite(buzzerPin, LOW);
}

// ----------------- Konfigurasi Wifi (START) -----------------
// Handle form menyimpan SSID dan password
void handleFormSubmission() {
  String ssidInput = server.arg("ssid");
  String passwordInput = server.arg("password");

  // Simpan kredensial ke Preferences
  preferences.begin("wifi-creds", false);
  preferences.putString("ssid", ssidInput);
  preferences.putString("password", passwordInput);
  preferences.end();

  server.send(200, "text/html", "<h1>WiFi credentials saved. Restarting...</h1>");
  Serial.println("WiFi credentials received:");
  Serial.println("SSID: " + ssidInput);
  Serial.println("Password: " + passwordInput);

  delay(2000);
  ESP.restart();  // Restart ESP32 untuk koneksi kembali
}

// Fungsi untuk menangani perintah disconnect WiFi
void handleDisconnectWiFi() {

  server.send(200, "text/html", "<h1>Disconnected from WiFi. Restarting in Access Point mode...</h1>");
  delay(2000);
  Serial.println("WiFi disconnected. Restarting in AP mode...");
  // Hapus credential WiFi dari Preferences
  preferences.begin("wifi-creds", false);
  preferences.clear();
  preferences.end();
  // Putuskan koneksi WiFi dan masuk ke mode AP
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  delay(1000);
  ESP.restart();  // Restart ESP32 untuk masuk mode AP
}

// Membuat Access Point
void createWiFiAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32_Config");  // Nama AP dan password sementara
  buzzerAlert();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("AP Mode Enabled");
  lcd.setCursor(0, 1);
  lcd.print("ESP32_config");

  unsigned long startTime = millis();

  // Tunggu 60 detik untuk client connect
  while (!WiFi.softAPgetStationNum() && (millis() - startTime < 30000)) {
    lcd.setCursor(0, 2);
    lcd.print("Menunggu client: ");
    lcd.print(30 - (millis() - startTime) / 1000);
    lcd.print(" ");
    lcd.setCursor(0, 3);
    lcd.print("IP: ");
    lcd.print(WiFi.softAPIP());
  }

  if (!WiFi.softAPgetStationNum()) {
    // Jika tidak ada client yang connect, coba ke SSID fallback
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Trying fallback");
    lcd.setCursor(0, 1);
    lcd.print("SSID: karim");
    connectToFallback();
  }
}

// Fungsi mencoba koneksi ke fallback SSID
void connectToFallback() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(fallbackSSID, fallbackPassword);
  Serial.println("Connecting to fallback SSID...");
  unsigned long startTime = millis();

  while (WiFi.status() != WL_CONNECTED && (millis() - startTime < 10000)) {  // Tunggu 10 detik
    lcd.setCursor(0, 2);
    lcd.print("Connecting... ");
    delay(1000);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    // Jika berhasil terhubung
    buzzerAlert();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Connected to:");
    lcd.setCursor(0, 1);
    lcd.print(fallbackSSID);
    lcd.setCursor(0, 2);
    lcd.print("SSID: ");
    lcd.print(WiFi.SSID());
    lcd.setCursor(0, 3);
    lcd.print("IP: ");
    lcd.print(WiFi.localIP());
    Serial.println("\nFallback success!");
    isOnline = true;
    isAlreadyConnect = true;
    delay(2000);
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Fallback wifi gagal!");
    lcd.setCursor(0, 1);
    lcd.print("Beralih ke mode Offline");
    return;
  }
}

// Fungsi konfigurasi WiFi
void setupWiFi() {
  preferences.begin("wifi-creds", true);
  String savedSSID = preferences.getString("ssid", "");
  String savedPassword = preferences.getString("password", "");
  preferences.end();

  if (savedSSID != "" && savedPassword != "") {
    WiFi.mode(WIFI_STA);
    if (savedPassword == "-") {
      WiFi.begin(savedSSID.c_str());
    } else {
      WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
    }
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Connecting WiFi...");
    Serial.print("Connecting to WiFi...");

    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 15000) {  // Tunggu 15 detik
      delay(1000);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      buzzerAlert();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("WiFi Connected!");
      lcd.setCursor(0, 1);
      lcd.print("SSID: ");
      lcd.print(WiFi.SSID());
      lcd.setCursor(0, 2);
      lcd.print("IP: ");
      lcd.print(WiFi.localIP());
      isOnline = true;
      isAlreadyConnect = true;
      delay(5000);
    } else {
      // Jika gagal masuk ke mode AP
      Serial.println("\nFailed to connect. Entering AP mode...");
      createWiFiAP();
    }
  } else {
    // Masuk ke mode AP jika tidak ada kredensial
    Serial.println("No WiFi credentials found. Entering AP mode...");
    createWiFiAP();
  }
}

// ----------------- Konfigurasi Wifi (END) -----------------

// ----------------- Konfigurasi Sensor (START) -----------------
// Fungsi membaca data dari sensor
void handleData() {
  // Reset total jika minus
  if (total < 0) {
    total = 0;
    for (int i = 0; i < numReadings; i++) {
      readings[i] = 0;
    }
  }

  total = total - readings[readIndex];
  int sensorValue = analogRead(ammoniaSensorPin);
  float voltage = abs((sensorValue * voltageInput) / 4095.0);  // Gunakan abs() untuk nilai positif
  readings[readIndex] = voltage;
  total = total + voltage;
  readIndex++;
  if (readIndex >= numReadings) {
    readIndex = 0;
  }

  average = total / numReadings;
  if (average < 0) average = 0;

  ppm = (628.81 * average) + 0.857;
  if (ppm < 1) ppm = 0;
  if (ppm > 200) ppm = 200;

  float h = dht.readHumidity();
  suhu = dht.readTemperature();
  kelembapan = (h <= 99) ? h : 99.0;
  if (isnan(kelembapan) || isnan(suhu)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }
}
// ----------------- Konfigurasi Sensor (END) -----------------

// ----------------- Konfigurasi Buzzer (START) -----------------
void buzzerActive(unsigned long currentTime, unsigned long buzzerDuration = 500) {
  if (buzzerOn) {
    if (currentTime - buzzerStartTime >= buzzerDuration) {
      buzzerOn = false;
      digitalWrite(buzzerPin, LOW);
      toggleBuzzer = false;
      toggleBuzzerRelayAutoOn = false;
    }
  } else {
    digitalWrite(buzzerPin, HIGH);
    buzzerOn = true;
    buzzerStartTime = currentTime;
  }
}
// ----------------- Konfigurasi Buzzer (END) -----------------

// ----------------- Publish Data Relay (START) -----------------
void publishRelayStatus(const char* status, const char* mode, const char* affirmation = "") {
  data.clear();
  data["status"] = status;
  data["mode"] = mode;
  data["affirmation"] = affirmation;  // Kosongkan jika tidak digunakan
  String output;
  serializeJson(data, output);
  if (!client.publish(mqtt_relay_status_topic, output.c_str())) {
    Serial.println("Error: Failed to publish MQTT message");
  }
}

void publishRelaySetting(const char* command, unsigned long duration) {
  data.clear();
  data["command"] = command;
  data["duration"] = duration;
  String output;
  serializeJson(data, output);
  if (!client.publish(mqtt_relay_setting_topic, output.c_str())) {
    Serial.println("Error: Failed to publish MQTT message");
  }
}
// ----------------- Publish Data  Relay (END) -----------------

// ----------------- Konfigurasi MQTT & Relay (START) -----------------
// Fungsi Callback MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';                   // Akhiri string
  String command = String((char*)payload);  // Konversi payload ke string
  command.trim();

  // Reboot
  if (strcmp(topic, mqtt_restart_topic) == 0) {
    if (command == "restart") {
      buzzerAlert();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Sistem akan restart");
      lcd.setCursor(0, 1);
      lcd.print("dalam 5 detik");
      WiFi.disconnect();
      WiFi.mode(WIFI_OFF);
      digitalWrite(buzzerPin, LOW);
      digitalWrite(relayPin, LOW);
      delay(5000);
      ESP.restart();
    }
  }
  // Kontrol relay mode otomatis dan manual
  if (strcmp(topic, mqtt_relay_control_topic) == 0) {
    if (command == "AUTO") {
      relayActive = false;  // Matikan relay secara manual
      digitalWrite(relayPin, LOW);
      isManualMode = false;  // Kembali ke mode otomatis
      toggleBuzzer = true;
      status_mode = "Otomatis  ";
      publishRelayStatus("Relay OFF", "AUTO");

    } else if (command == "MANUAL") {
      relayActive = false;  // Matikan relay secara manual
      toggleBuzzer = true;
      digitalWrite(relayPin, LOW);
      isManualMode = true;  // Kembali ke mode manual
      status_mode = "Manual    ";
      publishRelayStatus("Relay OFF", "MANUAL");

    } else if (command == "ON") {
      relayActive = true;
      toggleBuzzer = true;
      digitalWrite(relayPin, HIGH);
      publishRelayStatus("Relay ON", "MANUAL");

    } else if (command == "OFF") {
      relayActive = false;  // Matikan relay secara manual
      toggleBuzzer = true;
      digitalWrite(relayPin, LOW);
      toggleManualRelay = false;
      manualRelayDuration = 0;
      publishRelayStatus("Relay OFF", "MANUAL");
      publishRelaySetting("timer_on", manualRelayDuration);

    } else if (command == "TIMER") {
      toggleBuzzer = true;
      isNotifTimerOn = true;
      toggleManualRelay = true;
      toggleRelay = false;  // Reset toggle relay
      publishRelayStatus("Relay ON", "MANUAL", "timer");
    }
  }

  // Setting waktu relay mode otomatis
  if (strcmp(topic, mqtt_relay_setting_topic) == 0) {
    data.clear();
    DeserializationError error = deserializeJson(data, command);
    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return;
    }
    String command = data["command"];
    int duration = data["duration"];
    String key = data["key"];

    if (command == "relay_on" && key == "begin") {
      relayOnDuration = duration;
      publishRelaySetting("relay_on", relayOnDuration);
    } else if (command == "relay_off" && key == "begin") {
      relayOffDuration = duration;
      publishRelaySetting("relay_off", relayOffDuration);
    } else if (command == "timer_on" && key == "begin") {
      manualRelayDuration = duration;
      publishRelaySetting("timer_on", manualRelayDuration);
    } else if (command == "relay_on" && key == "running") {
      relayOnDuration = duration;
      isSettingRelayOn = true;
      toggleBuzzer = true;
      publishRelaySetting("relay_on", relayOnDuration);
    } else if (command == "relay_off" && key == "running") {
      relayOffDuration = duration;
      isSettingRelayOff = true;
      toggleBuzzer = true;
      publishRelaySetting("relay_off", relayOffDuration);
    } else if (command == "timer_on" && key == "running") {
      manualRelayDuration = duration;
      isSettingRelayManualOn = true;
      toggleBuzzer = true;
      isSetTimer = true;
      publishRelaySetting("timer_on", manualRelayDuration);
    }
  }

  // Menerima data ambang batas ammonia
  if (strcmp(topic, mqtt_ammonia_threshold_topic) == 0) {
    data.clear();
    // deserialization json
    DeserializationError error = deserializeJson(data, command);
    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return;
    }
    float data_ammonia_threshold = data["value"];
    String key = data["key"];

    // publish data to discord bot

    if (key == "begin") {
      ammonia_threshold = data_ammonia_threshold;
      Serial.println(ammonia_threshold);
      client.publish(mqtt_ammonia_threshold_topic, String(ammonia_threshold).c_str());
    }
    if (key == "running") {
      isSettingAmmonia = true;
      toggleBuzzer = true;
      ammonia_threshold = data_ammonia_threshold;
      Serial.println(ammonia_threshold);
      client.publish(mqtt_ammonia_threshold_topic, String(ammonia_threshold).c_str());
    }
  }
  // Test apakah ESP32 Online
  if (strcmp(topic, mqtt_isonline_topic) == 0) {
    if (command == "online") {
      notifIsOnline = true;
      toggleBuzzer = true;
    }
  }
}

// MQTT Reconnect
void reconnect() {
  const int maxRetry = 5;  // Batas maksimum percobaan ulang
  int attempt = 0;

  while (!client.connected() && attempt < maxRetry) {
    Serial.print("Hubungkan MQTT...");
    if (client.connect("ESP32Client", mqtt_user, mqtt_password)) {
      Serial.println("MQTT Terhubung!");
      client.subscribe(mqtt_relay_setting_topic);
      client.subscribe(mqtt_relay_control_topic);
      client.subscribe(mqtt_ammonia_threshold_topic);
      client.subscribe(mqtt_restart_topic);
      client.subscribe(mqtt_isonline_topic);
      break;
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
      attempt++;
    }
  }

  if (!client.connected()) {
    Serial.println("Error: MQTT connection failed after max retries.");
    isOnline = false;  // Set ke offline agar tidak terus melakukan publish
  }
}

// ----------------- Konfigurasi Timer Relay ON manual (START) -----------------
void timerRelayOn(unsigned long currentTime) {
  // Aktivasi awal timer
  if (!toggleRelay && manualRelayDuration > 0) {
    toggleRelay = true;
    relayActive = true;
    relayStartTime = currentTime;
    digitalWrite(relayPin, HIGH);
    return;
  }

  // Cek waktu dan matikan relay
  if (toggleRelay && (currentTime - relayStartTime >= manualRelayDuration)) {
    toggleRelay = false;
    relayActive = false;
    toggleManualRelay = false;
    digitalWrite(relayPin, LOW);
    toggleBuzzer = true;
    publishRelayStatus("Relay OFF", "MANUAL", "timer");
    relayStartTime = 0;
  }
}
// ----------------- Konfigurasi Timer Relay ON manual (END) -----------------

// Manajemen relay otomatis
void manageRelay(unsigned long currentTime) {
  if (isManualMode) {
    return;
  }
  if (ppm > ammonia_threshold && !relayActive && (currentTime - lastRelayChange >= relayOffDuration)) {
    relayActive = true;
    digitalWrite(relayPin, HIGH);
    lastRelayChange = currentTime;
    toggleBuzzerRelayAutoOn = true;

    // publish data json relay_mode
    if (isOnline) {
      publishRelayStatus("Relay ON", "AUTO");
    }
  }

  if (relayActive && (currentTime - lastRelayChange >= relayOnDuration)) {
    relayActive = false;
    digitalWrite(relayPin, LOW);
    lastRelayChange = currentTime;
    toggleBuzzer = true;
    // publish data json
    if (isOnline) {
      publishRelayStatus("Relay OFF", "AUTO", "OFF");
    }
  }
}
// ----------------- Konfigurasi MQTT & Relay (END) -----------------

// Publish data default sekali
void publishBeginningData() {
  buzzerAlert();
  data.clear();
  data["status"] = "Relay OFF";
  data["mode"] = "AUTO";
  data["affirmation"] = "";
  String outputJson1;
  serializeJson(data, outputJson1);
  if (client.publish(mqtt_relay_status_topic, outputJson1.c_str())) {
    Serial.println("Data status & mode relay berhasil dipublikasikan");
  } else {
    Serial.println("Gagal mempublikasikan data status & mode relay");
  }
  data.clear();
  data["command"] = "relay_on";
  data["duration"] = relayOnDuration;
  String outputJson2;
  serializeJson(data, outputJson2);
  if (client.publish(mqtt_relay_setting_topic, outputJson2.c_str())) {
    Serial.println("Data durasi relay ON saat mode auto berhasil dipublikasikan");
  } else {
    Serial.println("Gagal mempublikasikan durasi relay ON");
  }
  // Data durasi relay OFF saat mode auto
  data.clear();
  data["command"] = "relay_off";
  data["duration"] = relayOffDuration;
  String outputJson3;
  serializeJson(data, outputJson3);
  if (client.publish(mqtt_relay_setting_topic, outputJson3.c_str())) {
    Serial.println("Data durasi relay OFF saat mode auto berhasil dipublikasikan");
  } else {
    Serial.println("Gagal mempublikasikan data durasi relay OFF");
  }

  // Data setting timmer relay ON mode manual
  data.clear();
  data["command"] = "timer_on";
  data["duration"] = manualRelayDuration;
  String outputJson4;
  serializeJson(data, outputJson4);
  if (client.publish(mqtt_relay_setting_topic, outputJson4.c_str())) {
    Serial.println("Data durasi relay ON saat mode manual berhasil dipublikasikan");
  } else {
    Serial.println("Gagal mempublikasikan durasi relay ON");
  }

  // Data ambang batas amonia
  if (client.publish(mqtt_ammonia_threshold_topic, String(ammonia_threshold).c_str())) {
    Serial.println("Data ambang batas amonia berhasil dipublikasikan");
  } else {
    Serial.println("Gagal mempublikasikan data ambang batas amonia");
  }
  // Heartbeat
  if (client.publish(mqtt_heartbeat_topic, "alive")) {
    Serial.println("Data heartbeat amonia berhasil dipublikasikan");
  } else {
    Serial.println("Gagal mempublikasikan data heartbeat");
  }
}


// Publikasi data suhu, kelembapan, amonia real - time
void publishSensorData() {
  data.clear();
  data["suhu"] = suhu;
  data["kelembapan"] = kelembapan;
  data["amonia"] = ppm;
  String output;
  serializeJson(data, output);
  if (client.publish(mqtt_sensor_data_topic, output.c_str())) {
    Serial.println("Data sensor berhasil dipublikasikan");
  } else {
    Serial.println("Gagal mempublikasikan data sensor");
  }
  // Publikasi data wifi
  data.clear();
  data["wifi_status"] = WiFi.status();
  data["ssid"] = WiFi.SSID();
  data["ipaddress"] = WiFi.localIP();
  String output_data_wifi;
  serializeJson(data, output_data_wifi);
  if (client.publish(mqtt_wifi_topic, output_data_wifi.c_str())) {
    Serial.println("Data wifi berhasil dipublikasikan");
  } else {
    Serial.println("Gagal mempublikasikan data wifi");
  }
  // Publikasi data Rs/Ro
  if (client.publish(mqtt_voltage_mems_topic, String(average).c_str())) {
    Serial.println("Data tegangan berhasil dipublikasikan");
  } else {
    Serial.println("Gagal mempublikasikan data tegangan");
  }
}

void tryReconnectWiFi(unsigned long currentMillis) {
  // Cek apakah sudah online atau belum waktunya mencoba lagi
  if (isOnline || currentMillis - lastWiFiAttempt < wifiRetryInterval) {
    return;
  }

  // Cek apakah sudah mencapai batas percobaan
  if (wifiReconnectAttempts >= maxWiFiReconnectAttempts) {
    // Reset counter setelah beberapa waktu
    if (currentMillis - lastWiFiAttempt >= wifiRetryInterval * 2) {
      wifiReconnectAttempts = 0;
    }
    return;
  }

  // Catat waktu percobaan
  lastWiFiAttempt = currentMillis;
  wifiReconnectAttempts++;

  // Tampilkan status di LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Reconnecting WiFi");
  lcd.setCursor(0, 1);
  lcd.print("Attempt: ");
  lcd.print(wifiReconnectAttempts);

  // Ambil kredensial WiFi
  preferences.begin("wifi-creds", true);
  String savedSSID = preferences.getString("ssid", "");
  String savedPassword = preferences.getString("password", "");
  preferences.end();

  if (savedSSID == "") {
    return;  // Tidak ada kredensial tersimpan
  }

  // Coba hubungkan
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  if (savedPassword == "-") {
    WiFi.begin(savedSSID.c_str());
  } else {
    WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
  }

  // Tunggu beberapa detik tanpa blocking
  unsigned long connectStartTime = millis();
  while (millis() - connectStartTime < 10000) {  // Tunggu max 10 detik
    if (WiFi.status() == WL_CONNECTED) {
      buzzerAlert();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("WiFi Connected!");
      lcd.setCursor(0, 1);
      lcd.print("SSID: ");
      lcd.print(WiFi.SSID());
      lcd.setCursor(0, 2);
      lcd.print("IP: ");
      lcd.print(WiFi.localIP());

      isOnline = true;
      wifiReconnectAttempts = 0;

      // Inisialisasi MQTT jika belum
      if (isOnline && !client.connected()) {
        espClient.setInsecure();
        client.setServer(mqtt_server, mqtt_port);
        client.setCallback(callback);
      }

      delay(2000);  // Tampilkan pesan sebentar
      lcd.clear();
      return;
    }
    delay(100);  // Polling setiap 100ms
    yield();     // Beri kesempatan ESP32 menangani tugas lain
  }

  // Jika gagal, tampilkan pesan
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Connect Failed");
  lcd.setCursor(0, 1);
  lcd.print("Will retry in 30s");
  delay(2000);
  lcd.clear();
}

void notifLCD(unsigned long currentTime) {
  // Handle notifikasi
  if (isShowingNotif) {
    if (currentTime - notifStartTime >= notifDuration) {
      lcd.clear();
      isShowingNotif = false;

      // Reset semua flag notifikasi
      isSettingRelayOn = false;
      isSettingRelayOff = false;
      isSettingAmmonia = false;
      notifIsOnline = false;
      isNotifTimerOn = false;
      isSetTimer = false;
    }
  }
  // Tampilkan notifikasi jika ada dan tidak sedang menampilkan notif lain
  if (!isShowingNotif) {
    if (isSettingRelayOn) {
      isShowingNotif = true;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Setting Berhasil!");
      lcd.setCursor(0, 1);
      lcd.print("Relay ON : ");
      lcd.print(relayOnDuration / 1000);
      lcd.print(" s");
      notifStartTime = currentTime;
    } else if (isSettingRelayOff) {
      isShowingNotif = true;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Setting Berhasil!");
      lcd.setCursor(0, 1);
      lcd.print("Cooldown : ");
      lcd.print(relayOffDuration / 1000);
      lcd.print(" s");
      notifStartTime = currentTime;
    } else if (isSettingAmmonia) {
      isShowingNotif = true;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Setting Berhasil!");
      lcd.setCursor(0, 1);
      lcd.print("Ammonia : ");
      lcd.print(ammonia_threshold);
      lcd.print(" PPM");
      notifStartTime = currentTime;
    } else if (notifIsOnline) {
      isShowingNotif = true;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("ESP32 Terhubung");
      lcd.setCursor(0, 1);
      lcd.print("ke Platform Discord");
      lcd.setCursor(0, 2);
      lcd.print("Chabot Siap!");
      notifStartTime = currentTime;
    } else if (isNotifTimerOn) {
      isShowingNotif = true;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Timer Aktif");
      lcd.setCursor(0, 1);
      lcd.print("Relay akan aktif");
      lcd.setCursor(0, 2);
      lcd.print("Selama ");
      lcd.print(manualRelayDuration / 1000);
      lcd.print(" s");
      notifStartTime = currentTime;
    } else if (isSetTimer) {
      isShowingNotif = true;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Setting Berhasil");
      lcd.setCursor(0, 1);
      lcd.print("Durasi Timer Relay");
      lcd.setCursor(0, 2);
      lcd.print("Menjadi: ");
      lcd.print(manualRelayDuration / 1000);
      lcd.print(" s");
      notifStartTime = currentTime;
    }
  }
}

void updateLCD() {
  // Menampilkan data ke LCD
  lcd.setCursor(0, 0);
  lcd.print("Wifi : ");
  isOnline ? lcd.print("Connected") : lcd.print("Not Connected");
  lcd.setCursor(0, 1);
  if (WiFi.softAPgetStationNum()) {
    lcd.print("IP   : ");
    lcd.print(WiFi.softAPIP());
  } else {
    lcd.print("Mode : ");
    lcd.print(status_mode);
  }
  lcd.setCursor(0, 2);
  lcd.print("NH3  : ");
  lcd.print(ppm, 3);
  lcd.print(" PPM  ");
  lcd.setCursor(0, 3);
  lcd.print("Suhu : ");
  lcd.print(suhu);
  lcd.print(F(" \xDF"
              "C   "));
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  lcd.init();
  lcd.backlight();

  // Inisialisasi array dengan 0
  for (int i = 0; i < numReadings; i++) {
    readings[i] = 0;
  }

  // Inisiasi buzzer
  pinMode(buzzerPin, OUTPUT);

  // Inisiasi relay
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);
  isManualMode = false;

  // Inisiasi DHT21
  dht.begin();

  // Inisiasi Wifi
  setupWiFi();

  // Setup HTTP Server untuk handle wifi
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", index_html);
  });
  server.on("/setup", HTTP_POST, handleFormSubmission);
  server.on("/disconnect", HTTP_GET, handleDisconnectWiFi);
  server.begin();
  Serial.println("Web server started, waiting for requests...");
  lcd.clear();


  // Inisiasi MQTT
  if (isOnline) {
    espClient.setInsecure();
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);
  }

}

void loop() {
  unsigned long currentMillis = millis();

  if (!isOnline && !WiFi.softAPgetStationNum() && isAlreadyConnect) {
    tryReconnectWiFi(currentMillis);
  }

  // Webserver Wifi
  server.handleClient();

  // MQTT messages
  if (isOnline) {
    if (!client.connected()) reconnect();
    client.loop();
  }

  // Kontrol relay
  if (!isManualMode) manageRelay(currentMillis);

  // Timer Relay Manual - cek sebelum manageRelay
  if (toggleManualRelay) timerRelayOn(currentMillis);


  // Publish Data Default Sistem Hanya Sekali ke MQTT
  if (isOnline && publishDefaultData) {
    publishBeginningData();
    publishDefaultData = false;
  }

  // Membaca data, menampilkan ke LCD
  if (currentMillis - previousMillisLCD >= intervalLCD) {
    previousMillisLCD = currentMillis;
    handleData();
    if (!isShowingNotif) updateLCD();
  }

  // Publish data ke MQTT setiap 15 detik sekali
  if (isOnline) {
    if (currentMillis - lastPublishTime >= publishInterval) {
      lastPublishTime = currentMillis;  // Simpan waktu publikasi terakhir
      publishSensorData();
    }
  }
  // Notifikasi LCD untuk feedback saat setting
  notifLCD(currentMillis);

  // Buzzer
  if (toggleBuzzer) buzzerActive(currentMillis);
  if (toggleBuzzerRelayAutoOn) buzzerActive(currentMillis, 3000);
}
