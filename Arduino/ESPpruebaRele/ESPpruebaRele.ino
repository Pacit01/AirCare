// Define el pin GPIO al que está conectado el control del relé
const int relayPin = D3; // Puedes cambiarlo al pin que estés usando

void setup() {
  // Inicializa el pin del relé como salida
  pinMode(relayPin, OUTPUT);
  // Inicialmente, desactiva el relé (dependiendo de tu relé, podría ser HIGH o LOW)
  digitalWrite(relayPin, LOW); // Cambia a HIGH si tu relé se activa con HIGH

  Serial.begin(115200);
  Serial.println("Relé listo para ser controlado.");
}

void loop() {
  // Aquí puedes agregar la lógica para activar o desactivar el relé
  // Por ejemplo, podrías leer datos de un sensor, recibir comandos por WiFi, etc.

  // Para probar, puedes activar el relé durante un segundo y luego desactivarlo
  Serial.println("Activando el relé...");
  digitalWrite(relayPin, HIGH); // Activa el relé (si se activa con HIGH)
  // digitalWrite(relayPin, LOW); // Activa el relé (si se activa con LOW)
  delay(5000); // Espera 1 segundo

  Serial.println("Desactivando el relé...");
  digitalWrite(relayPin, LOW);  // Desactiva el relé (si se activa con HIGH)
  // digitalWrite(relayPin, HIGH); // Desactiva el relé (si se activa con LOW)
  delay(3000); // Espera 2 segundos antes de repetir
}