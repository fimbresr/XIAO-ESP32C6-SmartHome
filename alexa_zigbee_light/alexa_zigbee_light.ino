/*
 * =============================================================
 * XIAO ESP32C6 — Zigbee On/Off Light para Alexa
 * =============================================================
 *
 * Este sketch convierte tu XIAO ESP32C6 en una "luz Zigbee"
 * que Alexa puede descubrir y controlar por voz.
 *
 * CONFIGURACIÓN ARDUINO IDE (¡IMPORTANTE!):
 *   Board:            XIAO_ESP32C6
 *   Zigbee Mode:      Zigbee ED (End Device)
 *   Partition Scheme:  Zigbee 4MB with spiffs
 *   Package esp32:    >= 3.0.0
 *
 * URL Board Manager:
 *   https://espressif.github.io/arduino-esp32/package_esp32_index.json
 *
 * COMANDOS DE VOZ (después de emparejar con Alexa):
 *   "Alexa, enciende [nombre]"
 *   "Alexa, apaga [nombre]"
 *
 * EMPAREJAMIENTO:
 *   1. Sube este sketch al ESP32C6
 *   2. El LED parpadeará rápido indicando que busca red
 *   3. En la app Alexa: Dispositivos > + > Añadir dispositivo > Otro
 *   4. Alexa buscará y encontrará "XIAO Zigbee Light"
 *   5. Dale un nombre (ej: "luz oficina")
 *
 * RESET DE FÁBRICA ZIGBEE:
 *   Mantén presionado BOOT por 5+ segundos -> borra la red
 *   Zigbee y vuelve a modo emparejamiento
 *
 * =============================================================
 */

#include "Zigbee.h"

// --- Configuración ---
#define ZIGBEE_LIGHT_ENDPOINT  10
#define LED_PIN                LED_BUILTIN  // GPIO 15 en XIAO ESP32C6
#define BOOT_BUTTON_PIN        9            // Botón BOOT del XIAO ESP32C6

// --- Objeto Zigbee Light ---
ZigbeeLight zbLight = ZigbeeLight(ZIGBEE_LIGHT_ENDPOINT);

// --- Estado ---
bool lightState = false;

// =============================================================
// Callback: Alexa/Hub envía comando ON/OFF
// =============================================================
void onZigbeeLightChange(bool state) {
  lightState = state;

  // LED activo-bajo: LOW = encendido, HIGH = apagado
  if (state) {
    digitalWrite(LED_PIN, LOW);   // ENCENDER
    Serial.println("[ZIGBEE] LED ENCENDIDO (comando remoto)");
  } else {
    digitalWrite(LED_PIN, HIGH);  // APAGAR
    Serial.println("[ZIGBEE] LED APAGADO (comando remoto)");
  }
}

// =============================================================
// Setup
// =============================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  // --- Configurar LED ---
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);  // Apagado al inicio (activo-bajo)

  // --- Configurar botón BOOT para reset de fábrica ---
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

  // --- Parpadeo inicial: indicar que está arrancando ---
  for (int i = 0; i < 5; i++) {
    digitalWrite(LED_PIN, LOW);
    delay(100);
    digitalWrite(LED_PIN, HIGH);
    delay(100);
  }

  Serial.println("=========================================");
  Serial.println(" XIAO ESP32C6 — Zigbee Light para Alexa");
  Serial.println("=========================================");

  // --- Configurar Zigbee Light ---
  zbLight.onLightChange(onZigbeeLightChange);
  zbLight.setManufacturerAndModel("SeeedStudio", "XIAO_ESP32C6_Light");

  // --- Añadir endpoint e iniciar Zigbee ---
  Zigbee.addEndpoint(&zbLight);

  Serial.println("[ZIGBEE] Iniciando stack Zigbee...");
  Serial.println("[ZIGBEE] Buscando red Zigbee (pon Alexa en modo emparejamiento)...");

  // begin() inicia el stack y la búsqueda de red
  if (!Zigbee.begin()) {
    Serial.println("[ERROR] Fallo al iniciar Zigbee! Revisa:");
    Serial.println("  - Zigbee Mode = Zigbee ED (End Device)");
    Serial.println("  - Partition Scheme = Zigbee 4MB with spiffs");
    Serial.println("  Reiniciando en 5 segundos...");
    delay(5000);
    ESP.restart();
  }

  Serial.println("[ZIGBEE] Stack Zigbee iniciado correctamente");
  Serial.println("[ZIGBEE] Esperando conexión con hub Zigbee...");
  Serial.println();
  Serial.println("NOTA: Mantén BOOT presionado 5s para reset de fábrica Zigbee");
}

// =============================================================
// Loop
// =============================================================
unsigned long bootButtonPressStart = 0;
bool bootButtonHeld = false;

void loop() {
  // --- Verificar botón BOOT para reset de fábrica ---
  if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
    if (!bootButtonHeld) {
      bootButtonHeld = true;
      bootButtonPressStart = millis();
    } else if (millis() - bootButtonPressStart > 5000) {
      // Mantener BOOT 5 segundos = reset de fábrica Zigbee
      Serial.println("[ZIGBEE] RESET DE FABRICA!");
      Serial.println("[ZIGBEE] Borrando configuración de red...");

      // Parpadeo rápido para indicar reset
      for (int i = 0; i < 10; i++) {
        digitalWrite(LED_PIN, LOW);
        delay(50);
        digitalWrite(LED_PIN, HIGH);
        delay(50);
      }

      Zigbee.factoryReset();
      // El dispositivo se reiniciará automáticamente
    }
  } else {
    bootButtonHeld = false;
  }

  delay(100);
}
