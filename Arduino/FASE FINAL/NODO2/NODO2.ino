#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Servo.h>

#define WIFI_SSID "Proyectos"
#define WIFI_PASSWORD "pacito123"

#define MQTT_BROKER "test.mosquitto.org"
#define MQTT_PORT 1883

#define MQTT_CLIENT_ID_SLAVE "esp8266_slave_aircare"
#define MQTT_SERVO_COMMAND_TOPIC "aircare/nodo2/servo_command"
#define MQTT_VENTILADOR_COMMAND_TOPIC "aircare/nodo2/ventilator_command"

#define SERVO_PIN D2
Servo myServo;
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
      mqttClient.subscribe(MQTT_SERVO_COMMAND_TOPIC);
      Serial.print("Suscrito al topic: ");
      Serial.println(MQTT_SERVO_COMMAND_TOPIC);
      mqttClient.subscribe(MQTT_VENTILADOR_COMMAND_TOPIC);
      Serial.print("Suscrito al topic: ");
      Serial.println(MQTT_VENTILADOR_COMMAND_TOPIC);
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

  if (String(topic) == MQTT_VENTILADOR_COMMAND_TOPIC) {
    if (message == "1") {
      Serial.println("Activar ventilador");
      myServo.write(180);
      //if (String(topic) == MQTT_SERVO_COMMAND_TOPIC) {
       // Serial.print("Comando servo recibido: ");
       // Serial.println(message);
       // int servoGrados = message.toInt();
       // myServo.detach();//Desadjuntar después de mover
       // Serial.print("Moviendo servo a: ");
      //  Serial.println(servoGrados);
       // myServo.write(servoGrados);
        //delay(1000); //Pequeña pausa para que el servo se mueva
      // Aquí iría la lógica para activar el ventilador
      
    } else if (message == "0") {
      Serial.println("Desactivar ventilador");
      myServo.write(0);
      // Aquí iría la lógica para desactivar el ventilador
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  connectWiFi();
  mqttClient.setClient(espClient);
  connectMQTT();
  myServo.attach(SERVO_PIN);
  Serial.println("ESP Esclavo Aircare Iniciado");
}

void loop() {
  Serial.println("Estado MQTT Conectado: ");
  Serial.println(mqttClient.connected());
  //if (!mqttClient.connected()) {
  //  connectMQTT();
 // }
  mqttClient.loop(); // Asegúrate de llamar mqttClient.loop() aquí
  delay(1000);
}