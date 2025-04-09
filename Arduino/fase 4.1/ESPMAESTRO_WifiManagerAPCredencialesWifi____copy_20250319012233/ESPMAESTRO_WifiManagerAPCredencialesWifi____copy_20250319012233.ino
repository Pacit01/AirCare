#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

//PARA WIFI MANAGER
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h> // Incluir WiFiManager


// Configuración de Firebase
#define API_KEY "AIzaSyBbgHvC5AHPUfDYQ5nfgSyrK5TRo-p3_8w"
#define DATABASE_URL "https://aircare-8aed5-default-rtdb.europe-west1.firebasedatabase.app/"

// Firebase
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

// Variables para datos simulados
float temperature = 0;
float humidity = 0;
int co2 = 0;
int pm2_5 = 0;
int pm4 = 0;
int pm8 = 0;
int pm10 = 0;


// Configuracion con Wifimanger AP para credenciales
void setup() {
    Serial.begin(115200);

    // WiFiManager
    WiFiManager wm;
    wm.resetSettings(); // Descomentar para borrar las credenciales guardadas

    // Intentar conectar usando credenciales guardadas
    // Si falla, inicia el portal cautivo
    if (!wm.autoConnect("AirCareAP-PinkPanther")) {
        Serial.println("failed to connect and hit timeout");
        delay(3000);
        //reset and try again, or maybe put it to deep sleep
        ESP.reset();
        delay(5000);
    }

    Serial.println("WiFi connected");
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
    Serial.println();

    // Obtener credenciales de WiFiManager
    String ssid = WiFi.SSID();
    String password = WiFi.psk();

    // Configurar Firebase
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;

    // Iniciar sesión anónima
    if (Firebase.signUp(&config, &auth, "", "")) {
        Serial.println("ok");
        signupOK = true;
    } else {
        Serial.printf("%s\n", config.signer.signupError.message.c_str());
    }

    config.token_status_callback = tokenStatusCallback;

    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);



  // Enviar credenciales a Firebase
    FirebaseJson json;
    json.set("ssid", ssid);
    json.set("password", password);

    if (Firebase.RTDB.setJSON(&fbdo, "/wifiCredentials", &json)) {
        Serial.println("Credenciales enviadas a Firebase");
    } else {
        Serial.println("Error al enviar credenciales a Firebase: " + fbdo.errorReason());
    }

    // Inicializar semilla aleatoria
    randomSeed(analogRead(0));
}

void loop() {
    // Generar datos ficticios
    generateFakeData();

    // Enviar datos a Firebase cada 15 segundos
    if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0)) {
        sendDataPrevMillis = millis();

        // Enviar datos a Firebase
        sendDataToFirebase();
    }
}

void generateFakeData() {
    // Generar datos ficticios
    temperature = random(180, 320) / 10.0;  // Temperatura entre 18.0 y 32.0 °C
    humidity = random(300, 900) / 10.0;     // Humedad entre 30.0 y 90.0 %
    co2 = random(350, 2000);                // CO2 entre 350 y 2000 ppm
    pm2_5 = random(5, 100);                 // PM2.5 entre 5 y 100 µg/m³
    pm4 = random(10, 120);                  // PM4 entre 10 y 120 µg/m³
    pm8 = random(15, 150);                  // PM8 entre 15 y 150 µg/m³
    pm10 = random(20, 200);                 // PM10 entre 20 y 200 µg/m³

    // Imprimir lecturas en el monitor serial
    Serial.println("Fake Sensor Readings:");
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println(" °C");

    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");

    Serial.print("CO2: ");
    Serial.print(co2);
    Serial.println(" ppm");

    Serial.print("PM2.5: ");
    Serial.print(pm2_5);
    Serial.println(" µg/m³");

    Serial.print("PM4: ");
    Serial.print(pm4);
    Serial.println(" µg/m³");

    Serial.print("PM8: ");
    Serial.print(pm8);
    Serial.println(" µg/m³");

    Serial.print("PM10: ");
    Serial.print(pm10);
    Serial.println(" µg/m³");
}

void sendDataToFirebase() {
    // Enviar todo como un solo objeto JSON
    FirebaseJson json;
    json.set("temperature", temperature);
    json.set("humidity", humidity);
    json.set("co2", co2);
    json.set("pm2_5", pm2_5);
    json.set("pm4", pm4);
    json.set("pm8", pm8);
    json.set("pm10", pm10);
    json.set("timestamp", Firebase.getCurrentTime());

    if (Firebase.RTDB.setJSON(&fbdo, "sensors/latest", &json)) {
        Serial.println("All data sent successfully as JSON");
    } else {
        Serial.println("Failed to send JSON data: " + fbdo.errorReason());
    }
}