#include <Arduino.h>
#include <ESP8266WiFi.h>

// --- WiFi Manager ---
#include <DNSServer.h>          // Requerido por WiFiManager
#include <ESP8266WebServer.h>   // Requerido por WiFiManager
#include <WiFiManager.h>


// --- Firebase ---
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h" //  Firebase
#include "addons/RTDBHelper.h"  //  Firebase
#include <ArduinoJson.h>        // Firebase 


// --- MQTT ---
#include <PubSubClient.h>

// --- NTP (Time) ---
#include <WiFiUdp.h>
#include <NTPClient.h>


// --- Configuración Firebase ---
#define API_KEY "AIzaSyBbgHvC5AHPUfDYQ5nfgSyrK5TRo-p3_8w" // Tu API Key
#define DATABASE_URL "https://aircare-8aed5-default-rtdb.europe-west1.firebasedatabase.app/" // Tu URL de DB



// --- Configuración MQTT ---
#define MQTT_BROKER "test.mosquitto.org" // Broker público, considera uno privado para producción
#define MQTT_PORT 1883
#define MQTT_CLIENT_ID_MASTER "esp8266_master_aircare_fusion" // ID único


// Topics MQTT (iguales que en tu código MQTT)
#define MQTT_TOPIC_BASE_SENSORES "aircare/sensores"
#define MQTT_TEMP_TOPIC       MQTT_TOPIC_BASE_SENSORES "/temperature"
#define MQTT_HUM_TOPIC        MQTT_TOPIC_BASE_SENSORES "/humidity"
#define MQTT_CO2_TOPIC        MQTT_TOPIC_BASE_SENSORES "/co2"
#define MQTT_PM25_TOPIC       MQTT_TOPIC_BASE_SENSORES "/pm2_5"
#define MQTT_PM4_TOPIC        MQTT_TOPIC_BASE_SENSORES "/pm4"
#define MQTT_PM8_TOPIC        MQTT_TOPIC_BASE_SENSORES "/pm8"
#define MQTT_PM10_TOPIC       MQTT_TOPIC_BASE_SENSORES "/pm10"
#define MQTT_MONTH_TOPIC      MQTT_TOPIC_BASE_SENSORES "/month" // 


// Comandos (Nodo 2)
#define MQTT_TOPIC_BASE_NODO2 "aircare/nodo2"
#define MQTT_SERVO_COMMAND_TOPIC      MQTT_TOPIC_BASE_NODO2 "/servo_command"
#define MQTT_VENTILATOR_COMMAND_TOPIC MQTT_TOPIC_BASE_NODO2 "/ventilator_command" // Nombre estandarizado

// --- Constantes del Programa ---
#define SEND_INTERVAL 15000 // Intervalo de envío de datos en ms (15 segundos)
const int CO2_UMBRAL = 1000;
const int FAKE_CO2_MIN = 350;  // Ajustado para coincidir con Firebase
const int FAKE_CO2_MAX = 2000; // Ajustado para coincidir con Firebase


// --- Objetos Firebase ---
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
bool signupOK = false;


// --- Objetos MQTT ---
WiFiClient espClient; // Cliente WiFi para MQTT
PubSubClient mqttClient(espClient);


// --- Objetos NTP ---
WiFiUDP ntpUDP;
// Ajusta el offset UTC según tu zona horaria (ej: 3600 para UTC+1) y horario de verano si aplica
NTPClient ntpClient(ntpUDP, "pool.ntp.org", 3600);

// --- Variables Globales para Datos ---
float temperature = 0;
float humidity = 0;
int co2 = 0;
int pm2_5 = 0;
int pm4 = 0;
int pm8 = 0;
int pm10 = 0;
int currentMonth = 0; // Variable para el mes actual
int servoGrados = 0; // Grados calculados para el servo
const char* ventilatorCommandString = "0"; // Variable global para el comando ("0" o "1")

// --- Temporizador ---
unsigned long sendDataPrevMillis = 0;

// --- Rangos Psicrométricos (del código MQTT) ---
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
void callback(char* topic, byte* payload, unsigned int length); // Callback para mensajes MQTT (si lo necesitas más adelante)


// ========================================================================
// ===                          SETUP                                   ===
// ========================================================================


void setup() {
    Serial.begin(115200);
    Serial.println("\n\nIniciando ESP Maestro Fusionado (Firebase + MQTT)");

    // --- WiFiManager ---
    WiFiManager wm;
    wm.resetSettings(); // Descomentar para borrar credenciales guardadas en test
    wm.setConfigPortalTimeout(180); // Timeout del portal en segundos (3 minutos)

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
    ntpClient.update(); // Intenta obtener la hora una vez al inicio
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
    Firebase.reconnectWiFi(true); // Permitir que Firebase gestione la reconexión WiFi si es necesario
    Serial.println("Firebase iniciado.");

    // --- Configurar MQTT ---
    Serial.println("Configurando MQTT...");
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setClient(espClient); // Indicarle a PubSub que use el cliente WiFi
    // mqttClient.setCallback(callback); // Habilita esto si necesitas recibir mensajes MQTT en el Maestro
    Serial.println("MQTT configurado.");

    // --- Conectar a MQTT ---
    connectMQTT(); // Intenta conectar a MQTT

    // --- Inicializar Semilla Aleatoria ---
    randomSeed(analogRead(0));

    Serial.println("Setup completo. Iniciando bucle principal...");
}



// ========================================================================
// ===                           LOOP                                   ===
// ========================================================================
void loop() {
    // --- Mantener Conexión MQTT ---
    if (!mqttClient.connected()) {
        connectMQTT(); // Intenta reconectar si se pierde la conexión
    }
    mqttClient.loop(); // INDISPENSABLE para procesar mensajes entrantes y mantener conexión

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
             if (!signupOK) Serial.println("  (Signup fallido)");
        }

        // 3. Enviar a MQTT (si está conectado)
        if (mqttClient.connected()) {
            sendDataToMQTT();
        } else {
            Serial.println("MQTT no conectado. No se enviaron datos MQTT.");
        } 
    }
     delay(10000); //10 segundos de espera envio datos
    
}



// ========================================================================
// ===                      FUNCIONES AUXILIARES                      ===
// ========================================================================

/**
 * @brief Intenta conectar (o reconectar) al Broker MQTT.
 */
void connectMQTT() {
    // Bucle hasta conectar (con reintentos limitados para no bloquear indefinidamente)
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
            delay(5000); // Espera 5 segundos antes de reintentar
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

    // --- Generar datos simulados (como en Firebase code) ---
    temperature = random(10, 40) / 10.0; // 1.0 - 40.0 °C
    humidity = random(300, 900) / 10.0;    // 30.0 - 90.0 %
    co2 = random(FAKE_CO2_MIN, FAKE_CO2_MAX); // 350 - 2000 ppm
    pm2_5 = random(5, 100);                // 5 - 100 µg/m³
    pm4 = random(10, 120);                 // 10 - 120 µg/m³
    pm8 = random(15, 150);                 // 15 - 150 µg/m³
    pm10 = random(20, 200);                // 20 - 200 µg/m³

    // --- Obtener Mes actual ---
    if (WiFi.status() == WL_CONNECTED) { // Solo intenta actualizar NTP si hay WiFi
        ntpClient.update();
        time_t epochTime = ntpClient.getEpochTime();
        struct tm *ptm = localtime(&epochTime);
        currentMonth = ptm->tm_mon + 1; // Mes (1-12)
    } else {
        Serial.println("WARN: No hay conexión WiFi, no se puede actualizar NTP/mes.");
        // Podríamos mantener el mes anterior o usar uno por defecto. Mantengamos el anterior.
    }


    // --- Imprimir lecturas ---
    Serial.println("Lecturas Simuladas:");
    Serial.print("  Temp: "); Serial.print(temperature); Serial.println(" C");
    Serial.print("  Hum: "); Serial.print(humidity); Serial.println(" %");
    Serial.print("  CO2: "); Serial.print(co2); Serial.println(" ppm");
    Serial.print("  PM2.5: "); Serial.print(pm2_5); Serial.println(" ug/m3");
    Serial.print("  PM4: "); Serial.print(pm4); Serial.println(" ug/m3");
    Serial.print("  PM8: "); Serial.print(pm8); Serial.println(" ug/m3");
    Serial.print("  PM10: "); Serial.print(pm10); Serial.println(" ug/m3");
    Serial.print("  Mes Actual (NTP): "); Serial.println(currentMonth);


    // --- Lógica de Control (del código MQTT) ---
    // Calcular grados del servo basado en estación (mes), temp y humedad
    servoGrados = 0; // Valor por defecto tapando rejilla ventilacion
    if (currentMonth > 0) { // Solo calcula si tenemos un mes válido
        //Verano
        if (currentMonth >= 6 && currentMonth <= 8) { // Verano
            if (temperature > veranoTempIdeal && humidity > veranoHumIdeal) servoGrados = 180;  
            else if (temperature < veranoTempIdeal && humidity < veranoHumIdeal) servoGrados = 50; //25%
            else servoGrados = 180; 

        // Otoño
        } else if (currentMonth >= 9 && currentMonth <= 11) { 
            if (temperature > otonoTempIdeal && humidity > otonoHumIdeal) servoGrados = 180;   
            else if (temperature < otonoTempIdeal && humidity < otonoHumIdeal) servoGrados = 75;  //50%
            else servoGrados = 180;

        // Invierno
        } else if (currentMonth >= 12 || currentMonth <= 2) { 
            if (temperature < inviernoTempIdeal && humidity < inviernoHumIdeal) servoGrados = 180;  
            else if (temperature > inviernoTempIdeal && humidity > inviernoHumIdeal) servoGrados = 50; 
            else servoGrados = 180;
        } 
        
        // Primavera (Marzo, Abril, Mayo)
        else { 
            if (temperature > primaveraTempIdeal && humidity > primaveraHumIdeal) servoGrados = 180;
            else if (temperature < primaveraTempIdeal && humidity < primaveraHumIdeal) servoGrados = 75;
            else servoGrados = 180;
        }
    } else {
        Serial.println("WARN: Mes no válido, usando servoGrados por defecto.");
    }

    // Determinar comando del ventilador basado en CO2
    ventilatorCommandString = (co2 > CO2_UMBRAL) ? "1" : "0";

    Serial.println("Comandos Calculados:");
    Serial.print("  Servo Grados: "); Serial.println(servoGrados);
    Serial.print("  Comando Ventilador: "); Serial.println(ventilatorCommandString);
}

/**
 * @brief Envía los datos actuales al Firebase Realtime Database.
 */
void sendDataToFirebase() {
    Serial.println("Enviando datos a Firebase (estructura granular)...");
    String basePathSensors = "aircare/sensores";
    String basePathNodo2 = "aircare/nodo2";
    bool success = true; // Para rastrear si todo fue bien

    // Enviar datos de sensores
    // Usa setFloat para números decimales
    if (!Firebase.RTDB.setFloat(&fbdo, basePathSensors + "/temperature", temperature)) success = false;
    if (!Firebase.RTDB.setFloat(&fbdo, basePathSensors + "/humidity", humidity)) success = false;

    // Usa setInt para números enteros
    if (!Firebase.RTDB.setInt(&fbdo, basePathSensors + "/co2", co2)) success = false;
    if (!Firebase.RTDB.setInt(&fbdo, basePathSensors + "/pm2_5", pm2_5)) success = false;
    if (!Firebase.RTDB.setInt(&fbdo, basePathSensors + "/pm4", pm4)) success = false;
    if (!Firebase.RTDB.setInt(&fbdo, basePathSensors + "/pm8", pm8)) success = false;
    if (!Firebase.RTDB.setInt(&fbdo, basePathSensors + "/pm10", pm10)) success = false;
    if (!Firebase.RTDB.setInt(&fbdo, basePathSensors + "/month", currentMonth)) success = false;

    // Enviar comandos
    if (!Firebase.RTDB.setInt(&fbdo, basePathNodo2 + "/servo_command", servoGrados)) success = false;

    // Convertir comando ventilador "0"/"1" (C-string) a entero 0/1 para Firebase usando strcmp
    // strcmp devuelve 0 si las cadenas son iguales
    int ventilatorCommandInt = (strcmp(ventilatorCommandString, "1") == 0) ? 1 : 0;
    if (!Firebase.RTDB.setInt(&fbdo, basePathNodo2 + "/ventilator_command", ventilatorCommandInt)) success = false;

    // Añadir un timestamp general para saber cuándo fue la última actualización
    if (!Firebase.RTDB.setTimestamp(&fbdo, "aircare/last_update")) success = false;

    if (success) {
        Serial.println("  Firebase: OK");
    } else {
        Serial.println("  Firebase: Falló uno o más envíos.");
        // Imprimir el último error conocido para depuración
        Serial.print("    Último error Firebase: ");
        Serial.println(fbdo.errorReason());
    }
}
/**
 * @brief Envía los datos y comandos a MQTT con estructura granular.
 */
void sendDataToMQTT() {
    Serial.println("Enviando datos y comandos por MQTT (estructura granular)...");
    char buffer[20]; // Buffer para conversiones

    // Publicar Datos de Sensores
    dtostrf(temperature, 4, 1, buffer); mqttClient.publish(MQTT_TEMP_TOPIC, buffer);
    dtostrf(humidity, 4, 1, buffer); mqttClient.publish(MQTT_HUM_TOPIC, buffer);
    itoa(co2, buffer, 10); mqttClient.publish(MQTT_CO2_TOPIC, buffer);
    itoa(pm2_5, buffer, 10); mqttClient.publish(MQTT_PM25_TOPIC, buffer);
    itoa(pm4, buffer, 10); mqttClient.publish(MQTT_PM4_TOPIC, buffer);
    itoa(pm8, buffer, 10); mqttClient.publish(MQTT_PM8_TOPIC, buffer);
    itoa(pm10, buffer, 10); mqttClient.publish(MQTT_PM10_TOPIC, buffer);
    itoa(currentMonth, buffer, 10); mqttClient.publish(MQTT_MONTH_TOPIC, buffer);

    // Publicar Comandos
    itoa(servoGrados, buffer, 10); mqttClient.publish(MQTT_SERVO_COMMAND_TOPIC, buffer);
    // Usa la variable global correcta 'ventilatorCommandString'
    mqttClient.publish(MQTT_VENTILATOR_COMMAND_TOPIC, ventilatorCommandString); // Enviar "0" o "1"

    // Imprimir para depuración (opcional pero útil)
    Serial.println("  MQTT Pubs:");
    Serial.printf("    %s: %.1f\n", MQTT_TEMP_TOPIC, temperature);
    Serial.printf("    %s: %.1f\n", MQTT_HUM_TOPIC, humidity);
    Serial.printf("    %s: %d\n", MQTT_CO2_TOPIC, co2);
    Serial.printf("    %s: %d\n", MQTT_PM25_TOPIC, pm2_5);
    Serial.printf("    %s: %d\n", MQTT_PM4_TOPIC, pm4);
    Serial.printf("    %s: %d\n", MQTT_PM8_TOPIC, pm8);
    Serial.printf("    %s: %d\n", MQTT_PM10_TOPIC, pm10);
    Serial.printf("    %s: %d\n", MQTT_MONTH_TOPIC, currentMonth);
    Serial.printf("    %s: %d\n", MQTT_SERVO_COMMAND_TOPIC, servoGrados);
    Serial.printf("    %s: %s\n", MQTT_VENTILATOR_COMMAND_TOPIC, ventilatorCommandString);
}

// Callback MQTT (si se habilita)
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensaje recibido ["); Serial.print(topic); Serial.print("]: ");
  payload[length] = '\0'; // Null-terminate
  Serial.println((char*)payload);

    // Añadir lógica si es necesario
}


