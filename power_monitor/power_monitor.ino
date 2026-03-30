/*
 * =============================================================
 * XIAO ESP32C6 — Monitor ATS / Planta de Emergencia
 * =============================================================
 *
 * Lógica:
 * - < 5s : Fallo Transitorio (Inmediato)
 * - 5s - 13s : Entrada normal de planta (Espera recierre < 3s para notificar)
 * - > 13s : Fallo de planta (Espera recierre < 3s para notificar)
 *
 * Envía notificaciones de WhatsApp usando CallMeBot.
 *
 * HARDWARE: 
 *   - Batería LiPo 3.7V (pads BAT)
 *   - Divisor de voltaje: Pin VBUS (5V) → R1(10kΩ) → GPIO(D0) → R2(10kΩ) → GND
 * =============================================================
 */

#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <Preferences.h>
#include <time.h>

WiFiMulti wifiMulti;

// =============================================
//  CONFIGURACIÓN Wi-Fi Y WHATSAPP
// =============================================
// Se agregaron ambas redes a WiFiMulti en la función setup()
const long  NTP_OFFSET     = -7 * 3600;           // Hermosillo UTC-7

String phone_number = "+5216621890372";
String apikey = "8345377";
// =============================================

// --- Hardware ---
#define POWER_SENSE_PIN   D0          
#define LED_PIN           LED_BUILTIN 
#define VOLTAGE_THRESHOLD 1000        

// --- Web Server y Flash ---
WebServer server(80);
Preferences prefs;
#define MAX_EVENTS 20

// --- Estructura para historial web (opcional local) ---
struct PowerEvent {
  char timestamp[24];     
  char type[32];           
  unsigned long duration; 
};
PowerEvent events[MAX_EVENTS];
int eventCount = 0;

// --- Máquina de Estados ATS ---
enum SystemState {
  NORMAL,
  PRIMARY_OUTAGE,
  WAITING_ATS,
  ATS_DROP
};
SystemState currentState = NORMAL;

bool powerPresent = true;
unsigned long outageStart = 0;
unsigned long outageDur = 0;
unsigned long atsDropStart = 0;
String currentOutDate = "";
String currentOutTime = "";

// --- NTP ---
const char* NTP_SERVER = "pool.ntp.org";
bool timeReady = false;

// =============================================================
// Funciones de tiempo
// =============================================================
void setupNTP() {
  configTime(NTP_OFFSET, 0, NTP_SERVER, "time.nist.gov");
  Serial.print("[NTP] Sincronizando hora Hermosillo");
  struct tm timeinfo;
  int retries = 0;
  while (!getLocalTime(&timeinfo) && retries < 15) {
    delay(500); Serial.print("."); retries++;
  }
  if (retries < 15) timeReady = true;
  Serial.println(timeReady ? " OK" : " FALLO_NTP");
}

void getTimestampVars(String &dateOut, String &timeOut) {
  struct tm timeinfo;
  if (timeReady && getLocalTime(&timeinfo)) {
    char d[16];
    char t[16];
    strftime(d, sizeof(d), "%d/%m/%Y", &timeinfo);
    strftime(t, sizeof(t), "%H:%M:%S", &timeinfo);
    dateOut = String(d);
    timeOut = String(t);
  } else {
    dateOut = "SIN_FECHA";
    timeOut = "SIN_HORA";
  }
}

// =============================================================
// Historial Local
// =============================================================
void loadEvents() {
  prefs.begin("pwrmon", true);
  eventCount = prefs.getInt("count", 0);
  for (int i = 0; i < eventCount && i < MAX_EVENTS; i++) {
    String ts = prefs.getString(("ts"+String(i)).c_str(), "");
    strncpy(events[i].timestamp, ts.c_str(), sizeof(events[i].timestamp));
    String tp = prefs.getString(("tp"+String(i)).c_str(), "");
    strncpy(events[i].type, tp.c_str(), sizeof(events[i].type));
    events[i].duration = prefs.getULong(("dur"+String(i)).c_str(), 0);
  }
  prefs.end();
}

void saveEventLocally(String eventType, unsigned long duration) {
  String d, t; getTimestampVars(d, t);
  String fd = d + " " + t;
  
  if (eventCount >= MAX_EVENTS) {
    for (int i = 0; i < MAX_EVENTS - 1; i++) events[i] = events[i + 1];
    eventCount = MAX_EVENTS - 1;
  }
  strncpy(events[eventCount].timestamp, fd.c_str(), sizeof(events[eventCount].timestamp));
  strncpy(events[eventCount].type, eventType.c_str(), sizeof(events[eventCount].type));
  events[eventCount].duration = duration;
  eventCount++;

  prefs.begin("pwrmon", false);
  prefs.putInt("count", eventCount);
  prefs.putString(("ts"+String(eventCount-1)).c_str(), fd);
  prefs.putString(("tp"+String(eventCount-1)).c_str(), eventType);
  prefs.putULong(("dur"+String(eventCount-1)).c_str(), duration);
  prefs.end();
}

// =============================================================
// WhatsApp API CallMeBot
// =============================================================
void sendWhatsAppMessage(int msgType, unsigned long duration, String date, String timeOfDay) {
  // Asegurar Wi-Fi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WIFI] Reconectando a redes de respaldo para enviar mensaje...");
    long startAttempt = millis();
    while (wifiMulti.run() != WL_CONNECTED && millis() - startAttempt < 25000) {
      delay(500); Serial.print(".");
    }
    Serial.println();
  }

  if (WiFi.status() == WL_CONNECTED) {
    if(!timeReady) setupNTP(); // Intentar re-sincronizar hora si no teniamos antes

    String area = "AREA: UNIDAD DE CUIDADOS INTENSIVOS UCI - ";
    String text = "";
    
    if (msgType == 1) {
      text = area + "Fallo Transitorio de : " + String(duration) + " Seg. el Dia: " + date + " a las " + timeOfDay;
    } else if (msgType == 2) {
      text = area + "Fallo de energia entrada de PLANTA DE EMERGENCIA tiempo de entradas de planta : " + String(duration) + " Seg. el Dia: " + date + " a las " + timeOfDay;
    } else if (msgType == 3) {
      text = area + "Fallo de Planta de emergencia, SE DEBE REVISAR SISTEMA, no cumple con el tiempo de 13seg.!! tiempo : " + String(duration) + " Seg. el Dia: " + date + " a las " + timeOfDay;
    }
    
    Serial.println("[WHATSAPP] Enviando: " + text);

    // Codificación URL ultra simple para WhatsApp CallMeBot
    text.replace(" ", "+");
    
    String url = "https://api.callmebot.com/whatsapp.php?phone=" + phone_number + "&text=" + text + "&apikey=" + apikey;
    
    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();
    if(httpCode > 0) {
      Serial.printf("[WHATSAPP] Enviado OK. Codigo HTTP: %d\n", httpCode);
    } else {
      Serial.printf("[WHATSAPP] Error enviando: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  } else {
    Serial.println("[WHATSAPP] ERROR CRÍTICO: No hay WiFi. No se pudo enviar.");
  }
}

// =============================================================
// Detección de energía y Web
// =============================================================
bool readPowerState() {
  uint32_t mv = 0;
  for (int i = 0; i < 8; i++) mv += analogReadMilliVolts(POWER_SENSE_PIN);
  return (mv / 8) > VOLTAGE_THRESHOLD;
}

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html><html lang="es"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0"><title>Monitor ATS</title>
<style>body{font-family:sans-serif;background:#0a0e17;color:#fff;padding:20px;}table{width:100%;border-collapse:collapse;margin-top:20px;}th,td{padding:10px;border-bottom:1px solid #333;text-align:left;}th{color:#00d4ff;}</style></head><body>
<h2>⚡ Monitor ATS Planta de Emergencia</h2>
<h3 style="color:)rawliteral";
  html += powerPresent ? "#00e676" : "#ff1744";
  html += R"rawliteral(">)rawliteral";
  html += powerPresent ? "ENERGÍA PRESENTE" : "SIN ENERGÍA";
  html += "</h3><table><tr><th>Fecha/Hora</th><th>Evento ATS</th><th>Duracion</th></tr>";
  for (int i = eventCount - 1; i >= 0; i--) {
    html += "<tr><td>" + String(events[i].timestamp) + "</td><td>" + String(events[i].type) + "</td><td>" + String(events[i].duration) + "s</td></tr>";
  }
  html += "</table><br><form action='/clear' method='POST'><button>Limpiar Historial</button></form></body></html>";
  server.send(200, "text/html", html);
}
void handleClear() {
  prefs.begin("pwrmon", false); prefs.clear(); prefs.end(); eventCount = 0;
  server.sendHeader("Location", "/"); server.send(303);
}

// =============================================================
// Setup
// =============================================================
void setup() {
  Serial.begin(115200); delay(1000);
  pinMode(LED_PIN, OUTPUT); digitalWrite(LED_PIN, HIGH);
  pinMode(POWER_SENSE_PIN, INPUT);

  Serial.println("\n============ MONITOREO ATS INICIANDO ============");
  loadEvents();

  // Agregar las dos redes para Failover (Respaldo Automático)
  wifiMulti.addAP("INFINITUM5021_2.4", "nsHsK4HtXh");      // Prioritaria 1
  wifiMulti.addAP("HOSPITAL SAN DIEGO", "SanDiego#23");    // Respaldo 2

  WiFi.mode(WIFI_STA); // Forzar modo Estación
  WiFi.disconnect();   // Limpiar conexiones en memoria
  delay(100);

  Serial.println("\n[WIFI] Buscando redes disponibles...");
  int r = 0;
  // Intenta conectar a la mejor red disponible
  while (wifiMulti.run() != WL_CONNECTED && r < 30) { delay(500); Serial.print("."); r++; }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WIFI] CONECTADO A: " + WiFi.SSID());
    Serial.println("[WIFI] IP: " + WiFi.localIP().toString());
    setupNTP();
  } else {
    Serial.println("\n[WIFI] Fallo conexion inicial. Se intentara luego.");
  }
  
  server.on("/", handleRoot);
  server.on("/clear", HTTP_POST, handleClear);
  server.begin();

  powerPresent = readPowerState();
  currentState = powerPresent ? NORMAL : PRIMARY_OUTAGE;
  if(!powerPresent) outageStart = millis(); // Si arranca ya sin luz
}

// =============================================================
// Loop Principal - Lógica de Máquina de Estados
// =============================================================
unsigned long lastCheck = 0;
int debounceCount = 0;

void loop() {
  server.handleClient();

  // Lectura rápida de energía (250ms) con 3 lecturas requeridas (debounce de 750ms)
  // ideal para atrapar la caída del recierre < 3s sin fallar
  if (millis() - lastCheck >= 250) {
    lastCheck = millis();
    bool currentPower = readPowerState();

    if (currentPower != powerPresent) {
      debounceCount++;
      if (debounceCount >= 3) {
        powerPresent = currentPower;
        debounceCount = 0;

        // =====================================================
        // TRANSICIÓN 1: LA ENERGÍA SE VUELVE CERO (CAÍDA)
        // =====================================================
        if (!powerPresent) {
          digitalWrite(LED_PIN, LOW); // Enciende LED
          
          if (currentState == NORMAL) {
            // CORTE PRIMARIO CFE
            currentState = PRIMARY_OUTAGE;
            outageStart = millis();
            getTimestampVars(currentOutDate, currentOutTime);
            Serial.println("\n[ATS] -----> SE PERDIO CFE. ESPERANDO RESPUESTA...");
            saveEventLocally("Corte CFE", 0);
          } 
          else if (currentState == WAITING_ATS) {
            // CAÍDA POR RECIERRE ATS (esperamos que dure < 3s)
            currentState = ATS_DROP;
            atsDropStart = millis();
            Serial.println("\n[ATS] -----> DETECTADO POSIBLE RECIERRE CFE (CAIDA BREVE)...");
          }
        } 
        
        // =====================================================
        // TRANSICIÓN 2: LA ENERGÍA REGRESA A NORMALIDAD (HIGH)
        // =====================================================
        else {
          digitalWrite(LED_PIN, HIGH); // Apaga LED
          
          if (currentState == PRIMARY_OUTAGE) {
            // REGRESÓ DEL CORTE INICIAL
            outageDur = (millis() - outageStart) / 1000;
            Serial.printf("[ATS] <----- ENERGIA REGRESO. Tiempo fuera: %lu seg.\n", outageDur);
            
            if (outageDur < 5) {
              // Menor a 5s -> FALLO TRANSITORIO. Se envía y termina el flujo.
              Serial.println("[ATS] Es Fallo Transitorio. Enviando Whatsapp INMEDIATAMENTE.");
              sendWhatsAppMessage(1, outageDur, currentOutDate, currentOutTime);
              saveEventLocally("Transitorio", outageDur);
              currentState = NORMAL;
            } else {
              // Mayor a 5s -> Entró PLANTA (Suposición). Esperaremos al ATS final.
              Serial.println("[ATS] Corte >= 5s. Suponiendo PLANTA. Esperando recierre de CFE para reportar...");
              currentState = WAITING_ATS;
            }
          } 
          else if (currentState == ATS_DROP) {
            // REGRESÓ DESPUÉS DE LA CAÍDA DEL ATS
            unsigned long atsDropDur = (millis() - atsDropStart) / 1000;
            Serial.printf("[ATS] <----- ENERGIA REGRESO. Caida de recierre: %lu seg.\n", atsDropDur);
            
            if (atsDropDur <= 5) { // Damos hasta 5 segundos de margen para el cruce del ATS
              // ES EL RECIERRE CORRECTO DEL ATS
              Serial.println("[ATS] Recierre exitoso identificado. Generando reporte de la falla principal.");
              
              if (outageDur >= 5 && outageDur <= 13) {
                 sendWhatsAppMessage(2, outageDur, currentOutDate, currentOutTime);
                 saveEventLocally("Entrada Planta", outageDur);
              } else if (outageDur > 13) {
                 sendWhatsAppMessage(3, outageDur, currentOutDate, currentOutTime);
                 saveEventLocally("Fallo Planta (>13s)", outageDur);
              }
              currentState = NORMAL; // Todo vuelve a la normalidad
            } else {
              // La caída duró demasiado (> 5s). NO fue el ATS, significa que la planta falló!
              // Lo trataremos como un nuevo corte inicial desde este nuevo punto.
              Serial.println("[ATS] La caida duro mas de 5s! Posible falla de planta... reseteando.");
              outageDur = atsDropDur; 
              getTimestampVars(currentOutDate, currentOutTime); // Resella tiempo
              outageStart = millis() - (atsDropDur*1000); // Simulamos
              
              if (outageDur < 5) {
                sendWhatsAppMessage(1, outageDur, currentOutDate, currentOutTime);
                saveEventLocally("Anomalia Transitoria Planta", outageDur);
                currentState = NORMAL;
              } else {
                currentState = WAITING_ATS; // Vuelve a esperar
              }
            }
          }
        }
      } // Fin Debounce == 3
    } else {
      debounceCount = 0;
    }
  }

  // Debug states (opcional)
  static unsigned long lastLog = 0;
  if (!powerPresent && (millis() - lastLog >= 10000)) {
     lastLog = millis();
     Serial.printf("[STATE_DEBUG] Estado=%d, Duración corte: %lu s\n", (int)currentState, (millis()-outageStart)/1000);
  }

  // =====================================================
  // SEGURIDAD: Reinicio preventivo cada 24 horas
  // Evita fragmentación de RAM al usar Strings a largo plazo
  // =====================================================
  static unsigned long lastReboot = millis();
  // 86,400,000 ms = 24 horas
  if (currentState == NORMAL && (millis() - lastReboot > 86400000UL)) {
    Serial.println("\n[SISTEMA] Mantenimiento: Reinicio preventivo de 24 horas...");
    delay(1000);
    ESP.restart();
  }
}
