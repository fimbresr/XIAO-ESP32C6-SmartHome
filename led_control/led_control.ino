/*
 * =============================================================
 * XIAO ESP32C6 — Control del LED Interno
 * =============================================================
 * 
 * Placa:    Seeed Studio XIAO ESP32C6
 * LED:      GPIO 15 (LED_BUILTIN) — color naranja
 * Board:    XIAO_ESP32C6 (en Arduino IDE)
 * Package:  esp32 >= 3.0.0
 * 
 * Funcionalidad:
 *   - Enciende/apaga el LED con comandos Serial
 *   - Envía "ON" para encender, "OFF" para apagar
 *   - También parpadea al inicio para confirmar que funciona
 * 
 * URL Board Manager:
 *   https://espressif.github.io/arduino-esp32/package_esp32_index.json
 * =============================================================
 */

#define LED_PIN LED_BUILTIN  // GPIO 15 en XIAO ESP32C6

bool ledState = false;

void setup() {
  Serial.begin(115200);
  delay(1000);  // Esperar a que se abra el Serial Monitor
  
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);  // LED apagado al inicio (activo-bajo)
  
  // --- Parpadeo inicial (3 veces) para confirmar que está vivo ---
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, LOW);   // Encender (activo-bajo)
    delay(200);
    digitalWrite(LED_PIN, HIGH);  // Apagar
    delay(200);
  }
  
  Serial.println("=====================================");
  Serial.println(" XIAO ESP32C6 — LED Control");
  Serial.println("=====================================");
  Serial.println(" Comandos disponibles:");
  Serial.println("   ON   -> Encender LED");
  Serial.println("   OFF  -> Apagar LED");
  Serial.println("   BLINK -> Parpadeo continuo");
  Serial.println("   STOP  -> Detener parpadeo");
  Serial.println("=====================================");
  Serial.println(" Estado actual: OFF");
  Serial.println();
}

bool blinking = false;
unsigned long lastBlink = 0;
const unsigned long BLINK_INTERVAL = 500;  // ms
String inputBuffer = "";

void loop() {
  // --- Leer comandos del Serial (sin bloquear el loop) ---
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (inputBuffer.length() > 0) {
        inputBuffer.trim();
        inputBuffer.toUpperCase();
        
        if (inputBuffer == "ON") {
          blinking = false;
          ledState = true;
          digitalWrite(LED_PIN, LOW);   // Encender (activo-bajo)
          Serial.println("[LED] Encendido");
          
        } else if (inputBuffer == "OFF") {
          blinking = false;
          ledState = false;
          digitalWrite(LED_PIN, HIGH);  // Apagar (activo-bajo)
          Serial.println("[LED] Apagado");
          
        } else if (inputBuffer == "BLINK") {
          blinking = true;
          lastBlink = millis();         // Resetear timer
          ledState = false;
          Serial.println("[LED] Parpadeo activado (500ms)");
          
        } else if (inputBuffer == "STOP") {
          blinking = false;
          ledState = false;
          digitalWrite(LED_PIN, HIGH);  // Apagar (activo-bajo)
          Serial.println("[LED] Parpadeo detenido");
          
        } else {
          Serial.println("[ERROR] Comando: " + inputBuffer);
          Serial.println("        Usa: ON, OFF, BLINK, STOP");
        }
        inputBuffer = "";
      }
    } else {
      inputBuffer += c;
    }
  }
  
  // --- Modo parpadeo (no bloqueante) ---
  if (blinking) {
    unsigned long now = millis();
    if (now - lastBlink >= BLINK_INTERVAL) {
      lastBlink = now;
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState ? LOW : HIGH);  // Activo-bajo
    }
  }
}
