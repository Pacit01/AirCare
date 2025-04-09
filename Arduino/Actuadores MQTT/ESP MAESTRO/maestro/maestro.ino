#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include <WiFiUdp.h>
#include <NTPClient.h>

#define WIFI_SSID "Proyectos"
#define WIFI_PASSWORD "pacito123"

#define MQTT_BROKER "test.mosquitto.org"
#define MQTT_PORT 1883

#define MQTT_CLIENT_ID_MASTER "esp8266_master_aircare"
#define MQTT_CO2_TOPIC "aircare/nodo2/co2_level"
#define MQTT_TEMP_TOPIC "aircare/nodo2/temperatura"
#define MQTT_HUM_TOPIC "aircare/nodo2/humedad"
#define MQTT_MES_TOPIC "aircare/nodo2/mes"
#define MQTT_SERVO_COMMAND_TOPIC "aircare/nodo2/servo_comando"
#define MQTT_VENTILADOR_COMMAND_TOPIC "aircare/nodo2/ventilador_comando"

const int CO2_UMBRAL = 1000;
const int FAKE_CO2_MIN = 400;
const int FAKE_CO2_MAX = 1200;

WiFiClient espClient;
PubSubClient mqttClient(espClient);
WiFiUDP ntpUDP;
NTPClient ntpClient(ntpUDP, "pool.ntp.org");



// Rangos psicrométricos ideales (ejemplos, ajustar según datos del BOE)
int veranoTempIdeal = 27;
int veranoHumIdeal = 50;

int otonoTempIdeal = 22;
int otonoHumIdeal = 55;

int inviernoTempIdeal = 20;
int inviernoHumIdeal = 60;

int primaveraTempIdeal = 25;
int primaveraHumIdeal = 55;



void connectWiFi() {
  Serial.print("Conectando a WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi Conectado");
    Serial.print("Dirección IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Fallo de conexión WiFi");
    Serial.print("Estado de WiFi: ");
    Serial.println(WiFi.status());
  }
}

void connectMQTT() {
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setBufferSize(512);

  int attempts = 0;
  while (!mqttClient.connected() && attempts < 5) {
    Serial.print("Conectando a MQTT Broker...");
    if (mqttClient.connect(MQTT_CLIENT_ID_MASTER)) {
      Serial.println("Conectado a MQTT Broker");
    } else {
      Serial.print("falló, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" Reintentando en 5 segundos");
      delay(5000);
      attempts++;
    }
  }
  if (!mqttClient.connected()) {
    Serial.println("Fallo de conexión MQTT");
  }
}

int getFakeCO2() {
  return random(FAKE_CO2_MIN, FAKE_CO2_MAX);
}

void publishAircareData() {
  int co2Level = getFakeCO2();
  float temperatura = random(15, 35);
  float humedad = random(40, 80);
  
  ntpClient.update();
  time_t epochTime = ntpClient.getEpochTime();
  struct tm *ptm = localtime((time_t *)&epochTime);
  int mes = ptm->tm_mon + 1; // Obtener el mes (1-12)


  
  
  Serial.print("Nivel de CO2 simulado: ");
  Serial.print(co2Level);
  Serial.print(" ppm, Temperatura: ");
  Serial.print(temperatura);
  Serial.print(" °C, Humedad: ");
  Serial.print(humedad);
  Serial.print(" %, Estación (mes): ");
  Serial.println(mes);

  char buffer[20];
  dtostrf(temperatura, 4, 2, buffer);
  mqttClient.publish(MQTT_TEMP_TOPIC, buffer);
  dtostrf(humedad, 4, 2, buffer);
  mqttClient.publish(MQTT_HUM_TOPIC, buffer);
  itoa(mes, buffer, 10);
  mqttClient.publish(MQTT_MES_TOPIC, buffer);
  itoa(co2Level, buffer, 10);
  mqttClient.publish(MQTT_CO2_TOPIC, buffer);

  // Lógica de control del servo (comentada por ahora)
  
  int servoGrados = 0;

  if (mes >= 6 && mes <= 8) { // Verano
    if (temperatura > veranoTempIdeal && humedad > veranoHumIdeal) {
      servoGrados = 90; // 100% recirculación si hace calor y hay mucha humedad
    } else if (temperatura < veranoTempIdeal && humedad < veranoHumIdeal) {
      servoGrados = 25; // 25% recirculación si hace frío y hay poca humedad
    } else {
      servoGrados = 50; // Ajuste intermedio si las condiciones son normales
    }
  } else if (mes >= 9 && mes <= 11) { // Otoño
    if (temperatura > otonoTempIdeal && humedad > otonoHumIdeal) {
      servoGrados = 80;
    } else if (temperatura < otonoTempIdeal && humedad < otonoHumIdeal) {
      servoGrados = 30;
    } else {
      servoGrados = 50;
    }
  } else if (mes >= 12 || mes <= 2) { // Invierno
    if (temperatura > inviernoTempIdeal && humedad > inviernoHumIdeal) {
      servoGrados = 70;
    } else if (temperatura < inviernoTempIdeal && humedad < inviernoHumIdeal) {
      servoGrados = 90; // 100% recirculación si hace mucho frío y hay mucha humedad
    } else {
      servoGrados = 50;
    }
  } else { // Primavera
    if (temperatura > primaveraTempIdeal && humedad > primaveraHumIdeal) {
      servoGrados = 80;
    } else if (temperatura < primaveraTempIdeal && humedad < primaveraHumIdeal) {
      servoGrados = 30;
    } else {
      servoGrados = 50;
    }
  }
 
  itoa(servoGrados, buffer, 10);
  mqttClient.publish(MQTT_SERVO_COMMAND_TOPIC, buffer);
  

  const char* ventiladorCommand = (co2Level > CO2_UMBRAL) ? "1" : "0";
  mqttClient.publish(MQTT_VENTILADOR_COMMAND_TOPIC, ventiladorCommand);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  connectWiFi();
  mqttClient.setClient(espClient);
  connectMQTT();
  randomSeed(analogRead(0));

  ntpClient.begin(); // Inicializar NTPClient
  ntpClient.setTimeOffset(3600); // Ajuste para la zona horaria (ejemplo: +1 hora)


  Serial.println("ESP Maestro Aircare Iniciado");
}

void loop() {
  if (!mqttClient.connected()) {
    connectMQTT();
  }
  mqttClient.loop();

  static unsigned long lastPublishTime = 0;
  if (millis() - lastPublishTime > 5000) {
    lastPublishTime = millis();
    publishAircareData();
  }
}