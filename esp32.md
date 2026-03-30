# Seeed Studio XIAO ESP32C6 — Referencia Técnica

> **Fuente oficial:** [Wiki Seeed Studio — Getting Started](https://wiki.seeedstudio.com/xiao_esp32c6_getting_started/)
> **Tienda:** [Seeed Studio XIAO ESP32C6](https://www.seeedstudio.com/Seeed-Studio-XIAO-ESP32C6-p-5884.html)

---

## 1. Especificaciones del Hardware

| Parámetro | Valor |
|---|---|
| **SoC** | Espressif ESP32-C6 |
| **Arquitectura** | Dual RISC-V 32-bit |
| **Procesador HP** | Hasta 160 MHz |
| **Procesador LP** | Hasta 20 MHz |
| **SRAM** | 512 KB |
| **Flash** | 4 MB |
| **Tamaño PCB** | 21 × 17.8 mm |
| **Headers** | No soldados de fábrica (soldar manualmente) |

---

## 2. Conectividad Inalámbrica

| Protocolo | Detalle |
|---|---|
| **Wi-Fi** | 2.4 GHz Wi-Fi 6 (802.11ax) |
| **Bluetooth** | Bluetooth 5.3 (LE) |
| **Zigbee** | Soportado (IEEE 802.15.4) |
| **Thread** | Soportado (IEEE 802.15.4) |
| **Matter** | Soporte nativo — ideal para smart home interoperable |

### Alcance RF
- Antena cerámica integrada: **hasta 80 m** (BLE/Wi-Fi)
- Conector **UFL** para antena externa

### Configuración de Antena (RF Switch)
- Requiere configurar **GPIO3** (LOW) para activar el switch RF
- **GPIO14 LOW** (default) → antena cerámica integrada
- **GPIO14 HIGH** → antena externa UFL

```cpp
void setup() {
  pinMode(3, OUTPUT);        // WIFI_ENABLE
  digitalWrite(3, LOW);      // Activa RF switch control
  delay(100);
  pinMode(14, OUTPUT);       // WIFI_ANT_CONFIG
  digitalWrite(14, HIGH);    // Usar antena externa
}
```

---

## 3. Modos de Energía y Consumo

| Modo | Consumo |
|---|---|
| **Activo (HP)** | Normal (según carga) |
| **Activo (LP)** | Reducido — procesador LP a 20 MHz |
| **Light Sleep** | Bajo consumo, despertar rápido |
| **Deep Sleep** | **~15 μA** |

### Capacidades de ahorro de energía
- 4 modos de trabajo disponibles
- Soporte nativo para gestión de carga de batería LiPo
- Almacenamiento en **RTC Memory** persiste entre reboots en deep sleep

---

## 4. Batería y Carga

### Conexión de Batería
- Batería recomendada: **LiPo 3.7V recargable**
- Pad negativo (−): lado izquierdo, cerca de silk screen **"D8"**
- Pad positivo (+): lado derecho, cerca de silk screen **"D5"**
- ⚠️ **Con batería, el pin 5V no tiene voltaje**

### Indicador LED rojo de carga
| Estado | Comportamiento LED |
|---|---|
| Sin batería + USB | Se enciende, se apaga a los 30s |
| Cargando (batería + USB) | Parpadea (flash) |
| Carga completa | Se apaga |

### Lectura de Voltaje de Batería
- Requiere soldar **resistencia de 200kΩ** en configuración 1:2 (divisor de voltaje)
- Lectura por puerto analógico **A0**

```cpp
#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  pinMode(A0, INPUT);
}

void loop() {
  uint32_t Vbatt = 0;
  for (int i = 0; i < 16; i++) {
    Vbatt += analogReadMilliVolts(A0);
  }
  float Vbattf = 2 * Vbatt / 16 / 1000.0; // Compensa divisor 1:2
  Serial.println(Vbattf, 3);
  delay(1000);
}
```

---

## 5. Deep Sleep y Wake-Up

### GPIOs con función RTC para wake-up externo (ESP32-C6)
- **GPIO 0-7** son válidos para `ext1` wake-up
- ⚠️ `ext0` **NO es compatible** con ESP32-C6, usar `ext1`

### Demo 1: Wake-Up por señal externa (botón en GPIO 0)

```cpp
#define BUTTON_PIN_BITMASK (1ULL << GPIO_NUM_0)
RTC_DATA_ATTR int bootCount = 0;

void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();
  switch (reason) {
    case ESP_SLEEP_WAKEUP_EXT1:  Serial.println("Wakeup: external signal"); break;
    case ESP_SLEEP_WAKEUP_TIMER: Serial.println("Wakeup: timer"); break;
    default: Serial.printf("Wakeup: other (%d)\n", reason); break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  ++bootCount;
  Serial.println("Boot #" + String(bootCount));
  print_wakeup_reason();
  esp_sleep_enable_ext1_wakeup(BUTTON_PIN_BITMASK, ESP_EXT1_WAKEUP_ANY_HIGH);
  Serial.println("Going to sleep...");
  esp_deep_sleep_start();
}

void loop() {} // No se ejecuta
```

### Demo 2: Wake-Up por Timer (cada N segundos)

```cpp
#define uS_TO_S_FACTOR 1000000ULL
#define TIME_TO_SLEEP  5  // Segundos

RTC_DATA_ATTR int bootCount = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  ++bootCount;
  Serial.println("Boot #" + String(bootCount));
  
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Sleeping " + String(TIME_TO_SLEEP) + "s...");
  Serial.flush();
  esp_deep_sleep_start();
}

void loop() {} // No se ejecuta
```

---

## 6. Bootloader y Reset

### Entrar en modo BootLoader (soluciona puertos no reconocidos o fallos de upload)
1. Mantener presionado el botón **BOOT**
2. Conectar cable USB **sin soltar BOOT**
3. Soltar BOOT después de conectar
4. Subir programa (ej: Blink)

### Reset
- Presionar botón **Reset** una vez para reiniciar
- **BOOT + Reset** simultáneo = entrar en BootLoader

---

## 7. Entorno de Desarrollo

### Arduino IDE
- Versión mínima del paquete de board: **esp32 ≥ 3.0.0**
- Board Manager URL:
  ```
  https://espressif.github.io/arduino-esp32/package_esp32_index.json
  ```
- Board a seleccionar: **`XIAO_ESP32C6`**
- LED integrado: **naranja** (lado derecho)

### Primer programa (Blink)
- `File > Examples > 01.Basics > Blink`
- Seleccionar board `XIAO_ESP32C6` y puerto correcto

---

## 8. Pinout y GPIOs Destacados

| GPIO | Función especial |
|---|---|
| **GPIO 0** | Wake-up externo (RTC), ADC |
| **GPIO 3** | RF switch enable (set LOW para activar) |
| **GPIO 14** | Selector de antena (LOW=interna, HIGH=externa) |
| **A0** | Lectura analógica / voltaje batería |
| **D5** | Cercano a pad positivo batería |
| **D8** | Cercano a pad negativo batería |
| **GPIO 0-7** | Válidos para deep sleep ext1 wake-up |

---

## 9. Recursos y Documentación

| Recurso | Enlace |
|---|---|
| Datasheet ESP32-C6 | [PDF](https://files.seeedstudio.com/wiki/SeeedStudio-XIAO-ESP32C6/res/esp32-c6_datasheet_en.pdf) |
| Esquemático | [PDF](https://files.seeedstudio.com/wiki/SeeedStudio-XIAO-ESP32C6/XIAO_ESP32_C6_v1.0_SCH_260114.pdf) |
| KiCad Project (SCH+PCB) | [ZIP](https://files.seeedstudio.com/wiki/SeeedStudio-XIAO-ESP32C6/XIAO_ESP32_C6_v1.0_SCH%26PCB_260114.zip) |
| Pinout Sheet | [XLSX](https://files.seeedstudio.com/wiki/SeeedStudio-XIAO-ESP32C6/res/XIAO_ESP32C6_Pinout.xlsx) |
| Modelo 3D | [GrabCAD](https://grabcad.com/library/seeed-studio-xiao-esp32-c6-1) |
| KiCad Footprints | [ZIP](https://files.seeedstudio.com/wiki/XIAO-KiCad-Library/XIAO_Series_Footprints.zip) |
| KiCad Symbols | [ZIP](https://files.seeedstudio.com/wiki/XIAO-KiCad-Library/XIAO_Series_SCH_Symbols.zip) |
| Ebook | [Big Power, Small Board](https://mjrovai.github.io/XIAO_Big_Power_Small_Board-ebook/) |

---

## 10. Zigbee + Alexa (PROBADO Y FUNCIONANDO ✅)

### Configuración Arduino IDE para Zigbee
| Opción | Valor |
|---|---|
| Board | `XIAO_ESP32C6` |
| **Zigbee Mode** | **`Zigbee ED (End Device)`** |
| **Partition Scheme** | **`Zigbee 4MB with spiffs`** |
| Package esp32 | >= 3.0.0 |

### API ZigbeeLight (Arduino)
```cpp
#include "Zigbee.h"

ZigbeeLight zbLight = ZigbeeLight(ENDPOINT_NUMBER);
zbLight.onLightChange(callbackFunction);           // Callback ON/OFF
zbLight.setManufacturerAndModel("Maker", "Model"); // Identificación
Zigbee.addEndpoint(&zbLight);
Zigbee.begin();                                    // Inicia stack
Zigbee.factoryReset();                             // Borra red guardada
```

### Emparejamiento con Alexa
1. App Alexa > Dispositivos > + > Añadir dispositivo > Luz > Otro
2. Seleccionar Zigbee como tipo de conexión
3. Alexa usa su hub Zigbee integrado automáticamente
4. El ESP32-C6 aparece como luz inteligente

### Modelos Echo con hub Zigbee
- Echo (4ta gen) ✅
- Echo Plus (1ra/2da gen) ✅
- Echo Show 10 (3ra gen) ✅
- Echo Studio ✅
- Echo Dot ❌ (NO tiene hub Zigbee)

### Notas importantes
- LED interno (GPIO 15) es **activo-bajo**: LOW=encendido, HIGH=apagado
- Reset de fábrica Zigbee: mantener BOOT 5 segundos
- `ext0` wake-up NO soportado en ESP32-C6, usar `ext1`

---

## 11. Proyectos Implementados

### Proyecto 1: Monitor de Cortes de Energía
**Descripción:** Detecta cuándo se corta la energía de la pared y mantiene un historial accesible desde un servidor web local, calculando el tiempo exacto que duró el apagón.

**Hardware Utilizado:**
- Batería LiPo 3.7V conectada a los pines `BAT+` / `BAT-`.
- Divisor de voltaje: `Pin VBUS` (5V del USB) → Resistencia `10kΩ` → Pin `D0` (A0) → Resistencia `10kΩ` → `GND`.

**Funcionamiento:**
1. El XIAO se conecta a un cargador de celular a la pared.
2. El pin `VBUS` tiene voltaje solo cuando hay energía conectada al USB. El divisor rebaja esto a 2.5V para no quemar el pin `D0` (máximo 3.3V).
3. Si la luz se va, el voltaje en `VBUS` baja a 0. La batería LiPo automáticamente mantiene vivo el ESP32 (gracias al gestor de carga interno).
4. El programa detecta la caída en `D0` (con *debounce* para ignorar transitorios) e inicia un contador.
5. Los eventos se guardan en la memoria Flash usando `Preferences` para que el historial sobreviva a reinicios.
6. Muestra un dashboard en la IP local servida por WiFi.

---

## 12. Ideas y Oportunidades para Aplicaciones

### Por sus capacidades de conectividad:
- 🏠 **Smart Home con Matter/Thread** — interoperabilidad nativa con dispositivos Matter
- 📡 **Gateway Zigbee/Thread** — puente entre sensores 802.15.4 y Wi-Fi/cloud
- 📶 **Mesh networking** — redes Thread para cobertura extendida
- 🔵 **BLE Beacons / Proximity** — Bluetooth 5.3 LE para tracking y presencia

### Por su bajo consumo:
- 🔋 **Sensores IoT con batería** — deep sleep a 15 μA, wake-up por timer o evento
- 🌿 **Monitoreo ambiental** — temperatura, humedad, calidad de aire (alimentado por batería solar)
- 💧 **Riego inteligente** — sensores de humedad del suelo con reportes periódicos

### Por su tamaño compacto (21×17.8mm):
- ⌚ **Wearables** — dispositivos portátiles de salud o fitness
- 🏷️ **Tags IoT** — etiquetas inteligentes con tracking
- 🎮 **Controladores portátiles** — mandos BLE compactos

### Por su procesador dual RISC-V:
- 🤖 **Edge AI / TinyML** — inferencia local con procesador HP mientras LP gestiona conectividad
- 📊 **Data logging inteligente** — procesamiento local antes de enviar al cloud
- 🔐 **Seguridad IoT** — secure boot, encryption y TEE integrados en chip
