












#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Wire.h>
#include <SensirionI2CSen5x.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

#include "SparkFunCCS811.h" 







// --- Configuración Firebase ---
#define API_KEY "AIzaSyBbgHvC5AHPUfDYQ5nfgSyrK5TRo-p3_8w"
#define DATABASE_URL "https://aircare-8aed5-default-rtdb.europe-west1.firebasedatabase.app/"

// --- Configuración MQTT ---
#define MQTT_BROKER "test.mosquitto.org"
#define MQTT_PORT 1883
#define MQTT_CLIENT_ID_MASTER "esp8266_master_aircare_fusion"

// Topics MQTT
#define MQTT_TOPIC_BASE_SENSORES "aircare/sensores"
#define MQTT_TEMP_TOPIC MQTT_TOPIC_BASE_SENSORES "/temperature"
#define MQTT_HUM_TOPIC MQTT_TOPIC_BASE_SENSORES "/humidity"
#define MQTT_CO2_TOPIC MQTT_TOPIC_BASE_SENSORES "/co2"
#define MQTT_TVOC_TOPIC MQTT_TOPIC_BASE_SENSORES "/tvoc"
#define MQTT_PM1_TOPIC MQTT_TOPIC_BASE_SENSORES "/pm1"
#define MQTT_PM25_TOPIC MQTT_TOPIC_BASE_SENSORES "/pm2_5"
#define MQTT_PM4_TOPIC MQTT_TOPIC_BASE_SENSORES "/pm4"
#define MQTT_PM10_TOPIC MQTT_TOPIC_BASE_SENSORES "/pm10"
#define MQTT_MONTH_TOPIC MQTT_TOPIC_BASE_SENSORES "/month"

// Comandos (Nodo 2)
#define MQTT_TOPIC_BASE_NODO2 "aircare/nodo2"
#define MQTT_SERVO_COMMAND_TOPIC MQTT_TOPIC_BASE_NODO2 "/servo_command"
#define MQTT_VENTILATOR_COMMAND_TOPIC MQTT_TOPIC_BASE_NODO2 "/ventilator_command"

// Comandos (Nodo 1)
#define MQTT_TOPIC_BASE_NODO1 "aircare/nodo1"
#define MQTT_FILTRO_COMMAND_TOPIC MQTT_TOPIC_BASE_NODO1 "/filtro_command"



// --- Definiciones de direcciones I2C ---
#define SEN55_ADDRESS 0x69
#define CCS811_ADDR 0x5A 

// --- Constantes del Programa ---
#define SEND_INTERVAL 60000 // Intervalo de envío de datos en ms (1 minuto)

SensirionI2CSen5x sen5x;
CCS811 mySensor(CCS811_ADDR);

const int CO2_UMBRAL = 1000;
const float PM1_UMBRAL= 8.0;
const float PM25_UMBRAL= 8.0;
const float PM4_UMBRAL= 15.0;
const float PM10_UMBRAL= 18.0;

// --- Variables para los sensores ---
float temperature = 0.0;
float humidity = 0.0;
int co2 = 0;
int tvoc = 0;
float pm1 = 0.0;
float pm2_5 = 0.0;
float pm4 = 0.0;
float pm10 = 0.0;
float voc_index = 0.0;
float nox_index = 0.0;
int currentMonth = 0;



// --- Variables para comandos ---
//Nodo2
int servoGrados = 0;
const char *ventilatorCommandString = "0";

//Nodo1
const char *filtroCommandString = "0";


// --- Objetos Firebase ---
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
bool signupOK = false;

// --- Objetos MQTT ---
WiFiClient espClient;
PubSubClient mqttClient(espClient);
char buffer[50];

// --- Objetos NTP ---
WiFiUDP ntpUDP;
NTPClient ntpClient(ntpUDP, "pool.ntp.org", 3600);

// --- Temporizador ---
unsigned long sendDataPrevMillis = 0;

// --- Rangos Psicrométricos ---
int veranoTempIdeal = 27;
int veranoHumIdeal = 50;
int otonoTempIdeal = 22;
int otonoHumIdeal = 55;
int inviernoTempIdeal = 20;
int inviernoHumIdeal = 60;
int primaveraTempIdeal = 25;
int primaveraHumIdeal = 55;

// --- Prototipos de Funciones ---
void connectMQTT();
void generateAndProcessData();
void sendDataToFirebase();
void sendDataToMQTT();
void callback(char *topic, byte *payload, unsigned int length);
void readSEN55();
void readCCS811();

// ========================================================================
// ===                      SETUP                                   ===
// ========================================================================

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }
  Serial.println("\n\nIniciando ESP Maestro Fusionado (Firebase + MQTT)");

  // --- WiFiManager ---
  WiFiManager wm;
  // wm.resetSettings(); // Descomentar para borrar credenciales guardadas en test
  wm.setConfigPortalTimeout(180);

  if (!wm.autoConnect("AirCareAP-Maestro")) {
    Serial.println("Fallo al conectar WiFi o se alcanzó el timeout. Reiniciando...");
    delay(3000);
    ESP.reset();
    delay(5000);
  }

  Serial.println("WiFi conectado!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Conectado a la red: ");
  Serial.println(WiFi.SSID());
  Serial.println();

  // --- NTP (Obtener Hora) ---
  Serial.println("Iniciando NTP Client...");
  ntpClient.begin();
  ntpClient.update();
  Serial.println("NTP Client iniciado.");

  // --- Configurar Firebase ---
  Serial.println("Configurando Firebase...");
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  // Iniciar sesión anónima (importante para RTDB si no usas otro método)
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase Signup OK");
    signupOK = true;
  } else {
    Serial.printf("Firebase Signup Error: %s\n", config.signer.signupError.message.c_str());
    // Considera qué hacer si el signup falla (¿reiniciar?)
  }

  // Asignar callback para el estado del token JWT (útil para debugging)
  config.token_status_callback = tokenStatusCallback; // Definida en Firebase_ESP_Client.h

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  Serial.println("Firebase iniciado.");

  // --- Configurar MQTT ---
  Serial.println("Configurando MQTT...");
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setClient(espClient);
  // mqttClient.setCallback(callback); // Habilita esto si necesitas recibir mensajes MQTT en el Maestro
  Serial.println("MQTT configurado.");

  // --- Conectar a MQTT ---
  connectMQTT();

  // --- Inicializar Semilla Aleatoria ---
  randomSeed(analogRead(0));
  Wire.begin(); 
  //CCS811
  if (mySensor.begin() == false)
  {
    Serial.print("CCS811 error. Please check wiring. Freezing...");
    while (1);
  }
  
  //SEN55
  sen5x.begin(Wire); 

  uint16_t error;
  uint16_t error_start;
  char errorMessage[256];

  error = sen5x.deviceReset();
  if (error) {
    Serial.print("Error trying to execute deviceReset(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  }

  float tempOffset = 0.0;
  error = sen5x.setTemperatureOffsetSimple(tempOffset);
  if (error) {
    Serial.print("Error trying to execute setTemperatureOffsetSimple(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  } else {
    Serial.print("Temperature Offset set to ");
    Serial.print(tempOffset);
    Serial.println(" deg. Celsius (SEN54/SEN55 only");
  }
  error_start = sen5x.startMeasurement();
  if (error_start) {
    Serial.print("Error starting measurement: ");
    errorToString(error_start, errorMessage, 256);
    Serial.println(errorMessage);
  } else {
    Serial.println("Measurement started...");
  }

  Serial.println("Waiting for PM sensor start-up (60 seconds)...");
  delay(60000); // Wait for 60 seconds
  Serial.println("Setup completo. Iniciando bucle principal...");
}

// ========================================================================
// ===                       LOOP                                   ===
// ========================================================================

void loop() {
  // --- Mantener Conexión MQTT ---
  if (!mqttClient.connected()) {
    connectMQTT();
  }
  mqttClient.loop();

  // --- Enviar Datos Periódicamente ---
  if (millis() - sendDataPrevMillis >= SEND_INTERVAL) {
    sendDataPrevMillis = millis();

    // 1. Generar nuevos datos y calcular comandos
    generateAndProcessData();

    // 2. Enviar a Firebase (si está listo)
    if (Firebase.ready() && signupOK) {
      sendDataToFirebase();
    } else {
      Serial.println("Firebase no está listo para enviar datos.");
      if (!signupOK)
        Serial.println("  (Signup fallido)");
    }

    // 3. Enviar a MQTT (si está conectado)
    if (mqttClient.connected()) {
      sendDataToMQTT();
    } else {
      Serial.println("MQTT no conectado. No se enviaron datos MQTT.");
    }
  }
}

// ========================================================================
// ===                      FUNCIONES AUXILIARES                      ===
// ========================================================================

/**
 * @brief Intenta conectar (o reconectar) al Broker MQTT.
 */
void connectMQTT() {
  int retries = 0;
  while (!mqttClient.connected() && retries < 5) {
    Serial.print("Intentando conexión MQTT (Broker: ");
    Serial.print(MQTT_BROKER);
    Serial.print(")... ");

    if (mqttClient.connect(MQTT_CLIENT_ID_MASTER)) {
      Serial.println("Conectado!");
      // Aquí podrías subscribirte a topics si el Maestro necesitara recibir comandos
      // mqttClient.subscribe("algún/topic/de/comando");
    } else {
      Serial.print("falló, rc=");
      Serial.print(mqttClient.state());
      Serial.println(". Reintentando en 5 segundos...");
      retries++;
      delay(5000);
    }
  }
  if (!mqttClient.connected()) {
    Serial.println("No se pudo conectar al Broker MQTT después de varios intentos.");
  }
}

/**
 * @brief Genera datos simulados, obtiene la hora/mes y calcula comandos.
 */
void generateAndProcessData() {
  Serial.println("\nGenerando nuevos datos y procesando...");

  readSEN55(); // **LEER DATOS DEL SEN55**
  readCCS811(); // **LEER DATOS DEL CCS811**

  // --- Obtener Mes actual ---
  if (WiFi.status() == WL_CONNECTED) {
    ntpClient.update();
    time_t epochTime = ntpClient.getEpochTime();
    struct tm *ptm = localtime(&epochTime);
    currentMonth = ptm->tm_mon + 1;
  } else {
    Serial.println("WARN: No hay conexión WiFi, no se puede actualizar NTP/mes.");
  }

  // --- Imprimir lecturas ---
  Serial.println("Lectura del SEN55:");
  Serial.print("  Temp: ");
  Serial.print(temperature);
  Serial.println(" C");
  Serial.print("  Hum: ");
  Serial.print(humidity);
  Serial.println(" %");
  Serial.print("  PM1.0: ");
  Serial.print(pm1);
  Serial.println(" ug/m3");
  Serial.print("  PM2.5: ");
  Serial.print(pm2_5);
  Serial.println(" ug/m3");
  Serial.print("  PM4: ");
  Serial.print(pm4);
  Serial.println(" ug/m3");
  Serial.print("  PM10: ");
  Serial.print(pm10);
  Serial.println(" ug/m3");

  Serial.println("Lectura del CS811:");
  Serial.print("  CO2: ");
  Serial.print(co2);
  Serial.println(" ppm");
  Serial.print("  CTVOC: ");
  Serial.print(tvoc);
  Serial.println(" ppb");

  Serial.print("  Mes Actual (NTP): ");
  Serial.println(currentMonth);

  // --- Lógica de Control (del código MQTT) ---
  // Calcular grados del servo basado en estación (mes), temp y humedad
  servoGrados = 0;
  if (currentMonth > 0) {
    //Verano
    if (currentMonth >= 6 && currentMonth <= 8) {
      if (temperature > veranoTempIdeal && humidity > veranoHumIdeal)
        servoGrados = 180;
      else if (temperature < veranoTempIdeal && humidity < veranoHumIdeal)
        servoGrados = 50;
      else
        servoGrados = 180;
    }
    // Otoño
    else if (currentMonth >= 9 && currentMonth <= 11) {
      if (temperature > otonoTempIdeal && humidity > otonoHumIdeal)
        servoGrados = 180;
      else if (temperature < otonoTempIdeal && humidity < otonoHumIdeal)
        servoGrados = 75;
      else
        servoGrados = 180;
    }
    // Invierno
    else if (currentMonth >= 12 || currentMonth <= 2) {
      if (temperature < inviernoTempIdeal && humidity < inviernoHumIdeal)
        servoGrados = 180;
      else if (temperature > inviernoTempIdeal && humidity > inviernoHumIdeal)
        servoGrados = 50;
      else
        servoGrados = 180;
    }
    // Primavera (Marzo, Abril, Mayo)
    else {
      if (temperature > primaveraTempIdeal && humidity > primaveraHumIdeal)
        servoGrados = 180;
      else if (temperature < primaveraTempIdeal && humidity < primaveraHumIdeal)
        servoGrados = 75;
      else
        servoGrados = 180;
    }
  } else {
    Serial.println("WARN: Mes no válido, usando servoGrados por defecto.");
  }

  // Determinar comando del ventilador basado en CO2
  ventilatorCommandString = (co2 > CO2_UMBRAL) ? "1" : "0";

  // Determinar comando del filtr basado en las particulas PM
  filtroCommandString = "0";
  if (pm1 > PM1_UMBRAL) {
    filtroCommandString = "1";
    
  } else if (pm2_5 > PM25_UMBRAL) {
    filtroCommandString = "1";
    
  } else if (pm4 > PM4_UMBRAL) {
    filtroCommandString = "1";
   
  } else if (pm10 > PM10_UMBRAL) {
    filtroCommandString = "1";
  }

  Serial.println("Comandos Calculados:");
  Serial.print("  Servo Grados: ");
  Serial.println(servoGrados);
  Serial.print("  Comando Ventilador: ");
  Serial.println(ventilatorCommandString);
  Serial.print("  Comando Filtro: ");
  Serial.println(filtroCommandString);
}

/**
 * @brief Envía los datos actuales al Firebase Realtime Database.
 */
void sendDataToFirebase() {
  Serial.println("Enviando datos a Firebase (estructura granular)...");
  String basePathSensors = "aircare/sensores";
  String basePathNodo2 = "aircare/nodo2";
  String basePathNodo1 = "aircare/nodo1";
  bool success = true;

  // Enviar datos de sensores
  if (!Firebase.RTDB.setFloat(&fbdo, basePathSensors + "/temperature", temperature))
    success = false;
  if (!Firebase.RTDB.setFloat(&fbdo, basePathSensors + "/humidity", humidity))
    success = false;
  if (!Firebase.RTDB.setInt(&fbdo, basePathSensors + "/co2", co2))
    success = false;
  if (!Firebase.RTDB.setInt(&fbdo, basePathSensors + "/tvoc", tvoc))
    success = false;
  if (!Firebase.RTDB.setFloat(&fbdo, basePathSensors + "/pm1", pm1))
    success = false;
  if (!Firebase.RTDB.setFloat(&fbdo, basePathSensors + "/pm2_5", pm2_5))
    success = false;
  if (!Firebase.RTDB.setFloat(&fbdo, basePathSensors + "/pm4", pm4))
    success = false;
  if (!Firebase.RTDB.setFloat(&fbdo, basePathSensors + "/pm10", pm10))
    success = false;
  if (!Firebase.RTDB.setInt(&fbdo, basePathSensors + "/month", currentMonth))
    success = false;

  // Enviar Comandos+

  //NODO 2
  if (!Firebase.RTDB.setInt(&fbdo, basePathNodo2 + "/servo_command", servoGrados))
    success = false;

  int ventilatorCommandInt = (strcmp(ventilatorCommandString, "1") == 0) ? 1 : 0;
  if (!Firebase.RTDB.setInt(&fbdo, basePathNodo2 + "/ventilator_command", ventilatorCommandInt))
    success = false;


  //NODO 1
  int filtroCommandInt  = (strcmp(filtroCommandString , "1") == 0) ? 1 : 0;
  if (!Firebase.RTDB.setInt(&fbdo, basePathNodo1 + "/filtro_command", filtroCommandInt))
    success = false;


  // Añadir un timestamp general para saber cuándo fue la última actualización
  if (!Firebase.RTDB.setTimestamp(&fbdo, "aircare/last_update"))
    success = false;

  if (success) {
    Serial.println("  Firebase: OK");
  } else {
    Serial.println("  Firebase: Falló uno o más envíos.");
    Serial.print("    Último error Firebase: ");
    Serial.println(fbdo.errorReason());
  }
}

/**
 * @brief Envía los datos y comandos a MQTT con estructura granular.
 */
void sendDataToMQTT() {
  Serial.println("Enviando datos y comandos por MQTT (estructura granular)...");
  char buffer[20];

  // Publicar Datos de Sensores
  dtostrf(temperature, 4, 1, buffer);
  mqttClient.publish(MQTT_TEMP_TOPIC, buffer);
  dtostrf(humidity, 4, 1, buffer);
  mqttClient.publish(MQTT_HUM_TOPIC, buffer);
  itoa(co2, buffer, 10);
  mqttClient.publish(MQTT_CO2_TOPIC, buffer);
  itoa(tvoc, buffer, 10);
  mqttClient.publish(MQTT_TVOC_TOPIC, buffer);
  dtostrf(pm1, 4, 1, buffer);
  mqttClient.publish(MQTT_PM1_TOPIC, buffer);
  dtostrf(pm2_5, 4, 1, buffer);
  mqttClient.publish(MQTT_PM25_TOPIC, buffer);
  dtostrf(pm4, 4, 1, buffer);
  mqttClient.publish(MQTT_PM4_TOPIC, buffer);
  dtostrf(pm10, 4, 1, buffer);
  mqttClient.publish(MQTT_PM10_TOPIC, buffer);
  itoa(currentMonth, buffer, 10);
  mqttClient.publish(MQTT_MONTH_TOPIC, buffer);

  // Publicar Comandos
  itoa(servoGrados, buffer, 10);
  mqttClient.publish(MQTT_SERVO_COMMAND_TOPIC, buffer);
  mqttClient.publish(MQTT_VENTILATOR_COMMAND_TOPIC, ventilatorCommandString);
  mqttClient.publish(MQTT_FILTRO_COMMAND_TOPIC, filtroCommandString);

  // Imprimir para depuración (opcional pero útil)
  Serial.println("  MQTT Pubs:");
  Serial.printf("    %s: %.1f\n", MQTT_TEMP_TOPIC, temperature);
  Serial.printf("    %s: %.1f\n", MQTT_HUM_TOPIC, humidity);
  Serial.printf("    %s: %d\n", MQTT_CO2_TOPIC, co2);
  Serial.printf("    %s: %.1f\n", MQTT_PM1_TOPIC, pm1);
  Serial.printf("    %s: %.1f\n", MQTT_PM25_TOPIC, pm2_5);
  Serial.printf("    %s: %.1f\n", MQTT_PM4_TOPIC, pm4);
  Serial.printf("    %s: %.1f\n", MQTT_PM10_TOPIC, pm10);
  Serial.printf("    %s: %d\n", MQTT_MONTH_TOPIC, currentMonth);
  Serial.printf("    %s: %d\n", MQTT_SERVO_COMMAND_TOPIC, servoGrados);
  Serial.printf("    %s: %s\n", MQTT_VENTILATOR_COMMAND_TOPIC, ventilatorCommandString);
  Serial.printf("    %s: %s\n", MQTT_FILTRO_COMMAND_TOPIC, filtroCommandString);
}

// Callback MQTT (si se habilita)
void callback(char *topic, byte *payload, unsigned int length) {
  Serial.print("Mensaje recibido [");
  Serial.print(topic);
  Serial.print("]: ");
  payload[length] = '\0';
  Serial.println((char *)payload);

  // Añadir lógica si es necesario
}

// ========================================================================
// ===                      LECTURAS SENSORES                    ===
// ========================================================================

// ----------- SEN55 (ADAPTACIÓN CON CÓDIGO COMPLETO) ---------------
#include <stdint.h> // Include for uint16_t

void readSEN55() {
  uint16_t error;
  char errorMessage[256];

  delay(1000);

  // Read Measurement
  float massConcentrationPm1p0;
  float massConcentrationPm2p5;
  float massConcentrationPm4p0;
  float massConcentrationPm10p0;
  float ambientHumidity;
  float ambientTemperature;
  float vocIndex;
  float noxIndex;

  error = sen5x.readMeasuredValues(
    massConcentrationPm1p0, massConcentrationPm2p5, massConcentrationPm4p0,
    massConcentrationPm10p0, ambientHumidity, ambientTemperature, vocIndex,
    noxIndex);

  if (error) {
    Serial.print("Error trying to execute readMeasuredValues(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  } else {
    Serial.print("MassConcentrationPm1p0:");
    pm1=massConcentrationPm1p0;
    Serial.print(pm1);
    Serial.print("\t");
    Serial.print("MassConcentrationPm2p5:");
    pm2_5=massConcentrationPm2p5;
    Serial.print(pm2_5);
    Serial.print("\t");
    Serial.print("MassConcentrationPm4p0:");
    pm4=massConcentrationPm4p0;
    Serial.print(pm4);
    Serial.print("\t");
    Serial.print("MassConcentrationPm10p0:");
    pm10=massConcentrationPm10p0;
    Serial.print(pm10);
    Serial.print("\t");
    Serial.print("AmbientHumidity:");
    if (isnan(ambientHumidity)) {
      Serial.print("n/a");
    } else {      
      humidity = ambientHumidity;
      Serial.print(humidity);
    }
    Serial.print("\t");
    Serial.print("AmbientTemperature:");
    if (isnan(ambientTemperature)) {
      Serial.print("n/a");
    } else {
      temperature = ambientTemperature;
      Serial.print(temperature);
    }
    Serial.print("\t");
    Serial.print("VocIndex:");
    if (isnan(vocIndex)) {
      Serial.print("n/a");
    } else {
      voc_index = vocIndex;
      Serial.print(voc_index);
    }
    Serial.print("\t");
    Serial.print("NoxIndex:");
    if (isnan(noxIndex)) {
      Serial.println("n/a");
    } else {     
      nox_index = noxIndex;
      Serial.println(nox_index);
    }
  }
}

// -------------------------------- -------- CCS811 ------------------------------------

void readCCS811() {
int co2 = 0;
    int tvoc = 0;
  if (mySensor.dataAvailable())
  {
    //If so, have the sensor read and calculate the results.
    //Get them later
    mySensor.readAlgorithmResults();

    Serial.print("CO2[");
    //Returns calculated CO2 reading
    co2=mySensor.getCO2();
    Serial.print(co2);
    
    Serial.print("] tVOC[");
    tvoc=mySensor.getTVOC();
    Serial.print(tvoc);
    Serial.print("] millis[");
    //Display the time since program start
    Serial.print(millis());
    Serial.print("]");
    Serial.println();
  }

  delay(10); //Don't spam the I2C bus
}