#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#define WIFI_SSID "Proyectos"
#define WIFI_PASSWORD "pacito123"

#define MQTT_BROKER "test.mosquitto.org"
#define MQTT_PORT 1883

#define MQTT_CLIENT_ID_SLAVE "esp8266_slave_aircare"
#define MQTT_FILTRO_COMMAND_TOPIC "aircare/nodo1/filtro_command"

// Define el pin GPIO al que está conectado el control del relé
const int relayPin = D3; // Puedes cambiarlo al pin que estés usando

WiFiClient espClient;
PubSubClient mqttClient(espClient);

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
  mqttClient.setCallback(callback);
  mqttClient.setBufferSize(512);

  int attempts = 0;
  while (!mqttClient.connected() && attempts < 5) {
    Serial.print("Conectando a MQTT Broker...");
    if (mqttClient.connect(MQTT_CLIENT_ID_SLAVE)) {
      Serial.println("Conectado a MQTT Broker");
      mqttClient.subscribe(MQTT_FILTRO_COMMAND_TOPIC);
      Serial.print("Suscrito al topic: ");
      Serial.println(MQTT_FILTRO_COMMAND_TOPIC);
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

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println("Callback ejecutado");
  Serial.print("Mensaje recibido en topic: ");
  Serial.print(topic);
  Serial.print(", mensaje: ");
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);

  if (String(topic) == MQTT_FILTRO_COMMAND_TOPIC) {
    if (message == "1") {
      Serial.println("Activar relé del filtro");
      digitalWrite(relayPin, HIGH); // Activa el relé (asumiendo que HIGH lo activa)
    } else if (message == "0") {
      Serial.println("Desactivar relé del filtro");
      digitalWrite(relayPin, LOW);  // Desactiva el relé (asumiendo que LOW lo desactiva)
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  connectWiFi();
  mqttClient.setClient(espClient);
  connectMQTT();
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW); // Inicialmente el relé está desactivado
  Serial.println("ESP Esclavo Aircare Iniciado");
}

void loop() {
 
  mqttClient.loop(); // Asegúrate de llamar mqttClient.loop() aquí
  delay(1000);
}