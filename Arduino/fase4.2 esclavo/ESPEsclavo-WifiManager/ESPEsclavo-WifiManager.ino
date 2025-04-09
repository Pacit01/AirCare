#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>

// Direcciones EEPROM para SSID, contraseña y bandera
#define EEPROM_SSID_ADDR 0
#define EEPROM_PASSWORD_ADDR 32
#define EEPROM_FLAG_ADDR 96

void setup() {
  Serial.begin(115200);

  EEPROM.begin(97); // Inicializar EEPROM con el mismo tamaño

  // Esperar a que la bandera esté activada
  while (EEPROM.read(EEPROM_FLAG_ADDR) != 1) {
    Serial.print("Esperando credenciales. Bandera: ");
    Serial.println(EEPROM.read(EEPROM_FLAG_ADDR)); // Imprimir el valor leído
    delay(1000);
}

  // Leer SSID y contraseña de EEPROM
  char ssid[32];
  char password[64];

  for (int i = 0; i < 32; i++) {
    ssid[i] = EEPROM.read(EEPROM_SSID_ADDR + i);
  }
  ssid[31] = '\0';

  for (int i = 0; i < 64; i++) {
    password[i] = EEPROM.read(EEPROM_PASSWORD_ADDR + i);
  }
  password[63] = '\0';

  // Imprimir credenciales leídas
  Serial.print("SSID leído: ");
  Serial.println(ssid);
  Serial.print("Contraseña leída: ");
  Serial.println(password);

  // Conectar a la red Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConectado a Wi-Fi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Mensaje de conexión a la red principal
  Serial.print("ESP Esclavo conectado a la red: ");
  Serial.println(ssid);
}

void loop() {
  // No hacemos nada en el loop por ahora
}