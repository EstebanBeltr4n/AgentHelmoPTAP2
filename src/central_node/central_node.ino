/* ============================================================================
   NODO CENTRAL - SISTEMA MULTI-AGENTE HELMO (V3.1 DASHBOARD + MYSQL)
   ============================================================================
   Autor: Esteban Eduardo Escarraga
   Descripción: Recibe LoRa (AES), envía a MySQL (XAMPP) y sirve HMI Web Local.
   ============================================================================ */

#include <RadioLib.h>       
#include <WiFi.h>           
#include <WebServer.h>      
#include <HTTPClient.h>     
#include <Wire.h>           
#include "HT_SSD1306Wire.h" 
#include "mbedtls/aes.h"    
#include <ESP32Servo.h>     

// --- CONFIGURACIÓN DE RED ---
const char* WIFI_SSID = "iPhepe"; 
const char* WIFI_PASS = "3434000000";
const char* SERVER_URL = "http://10.212.251.145/helmo/insertar.php"; // IP según tu CMD

// --- SEGURIDAD ---
const unsigned char aes_key[16] = {'E','s','t','e','b','a','n','L','o','R','a','2','0','2','6','!'};

// --- MAPEADO DE HARDWARE (SEGÚN TUS ALERTAS FÍSICAS) ---
#define SDA_OLED 17 
#define SCL_OLED 18 
#define RST_OLED 21 
#define VEXT_PIN 36 
#define LORA_CS 8 
#define LORA_DIO1 14
#define SERVO_TURBIDEZ 4   // Servo para derivación
#define LED_NIVEL_BAJO 38  // Alerta nivel
#define BUZZER_ALERTA 2    // Alerta crítica

// --- VARIABLES DE ESTADO ---
float turb_ntu = 0, turb_raw = 0, ph_val = 7.0, altura_real = 0, dist_cm = 0;
String estadoGlobal = "NORMAL";
String accionRecomendada = "SISTEMA OPERATIVO";

// --- PERSISTENCIA EN RAM (PARA EL DASHBOARD WEB) ---
const int HIST_MAX = 40;
struct Registro {
  int sensor_id;
  float turb_ntu_, turb_raw_, dist_cm_, altura_, ph_;
};
Registro hist[HIST_MAX];
int hist_idx = 0;
int hist_count = 0;

// --- OBJETOS ---
SSD1306Wire pantalla(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);
SX1262* lora;
WebServer server(80);
Servo servoTurbidez;
volatile bool receivedFlag = false;
uint8_t rxBuffer[32];

// --- FUNCIONES ---
void addHist(int id, float ntu, float raw, float dist, float alt, float ph) {
  hist[hist_idx] = {id, ntu, raw, dist, alt, ph};
  hist_idx = (hist_idx + 1) % HIST_MAX;
  if (hist_count < HIST_MAX) hist_count++;
}

void evaluarEstado() {
  if (turb_ntu > 500 || altura_real < 5.0) {
    estadoGlobal = "ALERTA CRITICA";
    accionRecomendada = (turb_ntu > 500) ? "PURGAR DRENAJES" : "ACTIVAR LLENADO";
  } else {
    estadoGlobal = "OPTIMA";
    accionRecomendada = "Mantener Operación";
  }
}

void controlarActuadores() {
  servoTurbidez.write(turb_ntu > 500 ? 90 : 0); //
  digitalWrite(LED_NIVEL_BAJO, altura_real < 5.0 ? HIGH : LOW); //
  digitalWrite(BUZZER_ALERTA, (turb_ntu > 500 || altura_real < 5.0) ? HIGH : LOW); //
}

void enviarMySQL(int id, float ntu, float raw, float dist, float alt, float ph) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(SERVER_URL);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    String data = "sensor_id="+String(id)+"&ntu="+String(ntu)+"&raw="+String(raw)+"&dist="+String(dist)+"&alt="+String(alt)+"&ph="+String(ph);
    http.POST(data);
    http.end();
  }
}

// Inserción de tu función Web Dashboard
void webCompleta() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'><meta http-equiv='refresh' content='3'>";
  html += "<title>PTAP Central HELMO</title>";
  html += "<style>body{font-family:Arial;text-align:center;background:#1e293b;color:white;margin:0;padding:10px}";
  html += ".card{display:inline-block;width:250px;margin:10px;padding:12px;background:#0f172a;border-radius:12px; border: 1px solid #334155;}";
  html += "table{width:95%;margin:auto;border-collapse:collapse;font-size:12px;} th,td{border-bottom:1px solid #334155;padding:8px} th{color:#38bdf8;}</style></head><body>";
  html += "<h1>📊 Panel Central PTAP - HELMO</h1>";
  
  html += "<div class='card'><h3>🌊 Turbidez</h3><b>" + String(turb_ntu, 1) + " NTU</b></div>";
  html += "<div class='card'><h3>🧪 pH</h3><b>" + String(ph_val, 2) + "</b></div>";
  html += "<div class='card'><h3>📏 Nivel</h3><b>" + String(altura_real, 1) + " cm</b></div>";
  
  html += "<h2 style='color:" + String(estadoGlobal == "OPTIMA" ? "#4ade80" : "#f87171") + ";'>" + estadoGlobal + "</h2>";
  html += "<h3>Historial LoRa (Últimos 40 paquetes)</h3><table><tr><th>ID</th><th>NTU</th><th>Alt(cm)</th><th>pH</th></tr>";

  for (int i = 0; i < hist_count; i++) {
    int idx = (hist_idx - 1 - i + HIST_MAX) % HIST_MAX;
    html += "<tr><td>" + String(hist[idx].sensor_id) + "</td><td>" + String(hist[idx].turb_ntu_, 1) + "</td><td>" + String(hist[idx].altura_, 1) + "</td><td>" + String(hist[idx].ph_, 2) + "</td></tr>";
  }
  html += "</table></body></html>";
  server.send(200, "text/html", html);
}

#if defined(ESP32)
IRAM_ATTR
#endif
void setFlag(void) { receivedFlag = true; }

void setup() {
  Serial.begin(115200);
  pinMode(VEXT_PIN, OUTPUT); digitalWrite(VEXT_PIN, LOW);
  pinMode(LED_NIVEL_BAJO, OUTPUT); pinMode(BUZZER_ALERTA, OUTPUT);
  servoTurbidez.attach(SERVO_TURBIDEZ);
  
  pantalla.init();
  pantalla.drawString(0, 0, "Conectando WiFi..."); pantalla.display();

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }
  
  // Configuración LoRa
  SPI.begin(9, 11, 10, 8); 
  Module* radio = new Module(LORA_CS, LORA_DIO1, 12, 13, SPI);
  lora = new SX1262(radio);
  lora->begin(915.0, 125.0, 7, 5, 0x12, 22, 8, 1.6);
  lora->setDio1Action(setFlag);
  lora->startReceive();

  // Ruta del Servidor
  server.on("/", webCompleta);
  server.begin();
  
  Serial.print("🌐 Dashboard en: http://"); Serial.println(WiFi.localIP());
}

void loop() {
  server.handleClient();

  if (receivedFlag) {
    receivedFlag = false;
    int state = lora->readData(rxBuffer, sizeof(rxBuffer));
    if (state == RADIOLIB_ERR_NONE) {
      // Descifrado AES
      unsigned char dec[32] = {0};
      mbedtls_aes_context aes;
      mbedtls_aes_init(&aes);
      mbedtls_aes_setkey_dec(&aes, aes_key, 128);
      mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, (const unsigned char*)rxBuffer, dec);
      mbedtls_aes_free(&aes);

      String msg = (char*)dec;
      int c1 = msg.indexOf(',');
      int c2 = msg.indexOf(',', c1 + 1);
      
      if (c1 > 0) {
        int id = msg.substring(0, c1).toInt();
        float val = msg.substring(c1 + 1, c2).toFloat();

        if (id == 0) { turb_ntu = val; turb_raw = (c2 > 0) ? msg.substring(c2+1).toFloat() : 0; }
        else if (id == 1) { dist_cm = val; altura_real = (c2 > 0) ? msg.substring(c2+1).toFloat() : 0; }
        else if (id == 2) { ph_val = val; }

        evaluarEstado();
        controlarActuadores();
        addHist(id, turb_ntu, turb_raw, dist_cm, altura_real, ph_val);
        enviarMySQL(id, turb_ntu, turb_raw, dist_cm, altura_real, ph_val);
      }
    }
    lora->startReceive();
  }
}