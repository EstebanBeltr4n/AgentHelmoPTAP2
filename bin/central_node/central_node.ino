// ===== RECEPTOR CENTRAL AGENTE OBJETIVO - DISTANCIA REAL + RAW =====
#define NODE_ID 3

#include <RadioLib.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include "mbedtls/aes.h"
#include <ESP32Servo.h>

const unsigned char aes_key[16] = {
  'E','s','t','e','b','a','n','L','o','R','a','2','0','2','6','!'
};

// Pines Heltec V3 + Actuadores
#define SDA_OLED 17
#define SCL_OLED 18
#define RST_OLED 21
#define VEXT_PIN 36
#define SERVO_TURBIDEZ  4
#define LED_NIVEL_BAJO  38  
#define BUZZER_ALERTA   2

SSD1306Wire pantalla(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);
SX1262* lora;
WebServer server(80);
Servo servoTurbidez;

// Variables COMPLETAS con RAW y distancia
float turb_val = 0, turb_raw = 0;      // Turbidez + RAW
float ph_val = 7;                      // pH
float nivel_val = 0, nivel_dist_cm = 0, nivel_altura_real = 0;  // Nivel + distancia + altura real
String estadoGlobal = "SIN DATOS";
String accionRecomendada = "INICIANDO";

volatile bool receivedFlag = false;
uint8_t rxBuffer[32];  // Buffer más grande para datos extras

#if defined(ESP32)
IRAM_ATTR
#endif
void setFlag(void) {
  receivedFlag = true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("🚀 CENTRAL con DISTANCIA REAL + RAW");

  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, LOW);
  delay(100);

  // OLED
  pantalla.init();
  pantalla.clear();
  pantalla.setFont(ArialMT_Plain_10);
  pantalla.drawString(0, 0, "Central Avanzada");
  pantalla.display();

  // LoRa
  SPI.begin(9, 11, 10, 8);
  Module* radio = new Module(8, 14, 12, 13);
  lora = new SX1262(radio);
  
  int state = lora->begin(915.0, 125.0, 7, 5, 0x12, 22, 8, 1.6);
  if (state == RADIOLIB_ERR_NONE) {
    lora->setDio1Action(setFlag);
    lora->startReceive();
    Serial.println("✓ LoRa OK");
  } else {
    Serial.print("✗ LoRa: "); Serial.println(state);
    while(1);
  }

  // 🎛️ Actuadores
  pinMode(LED_NIVEL_BAJO, OUTPUT);
  pinMode(BUZZER_ALERTA, OUTPUT);
  digitalWrite(LED_NIVEL_BAJO, LOW);
  digitalWrite(BUZZER_ALERTA, LOW);
  
  servoTurbidez.attach(SERVO_TURBIDEZ);
  servoTurbidez.write(0);

  // 🔐 WiFi SEGURO
  WiFi.softAP("LoRaCentral_Seguro", "ptap2026!");
  IPAddress ip = WiFi.softAPIP();
  Serial.print("📱 Web: http://"); Serial.println(ip);

  // 🌐 WEB con DISTANCIA REAL + RAW
  server.on("/", []() {
    if (!server.authenticate("admin", "ptap2026!")) {
      return server.requestAuthentication();
    }
    webCompleta();
  });
  server.begin();

  pantalla.clear();
  pantalla.drawString(0, 0, "http://" + ip.toString());
  pantalla.drawString(0, 15, "user:admin");
  pantalla.display();
}

void loop() {
  server.handleClient();

  if (receivedFlag) {
    receivedFlag = false;
    Serial.println("\n📡 RX!");

    int state = lora->readData(rxBuffer, 32);
    if (state == RADIOLIB_ERR_NONE) {
      // 🔓 AES
      unsigned char dec[32];
      mbedtls_aes_context aes;
      mbedtls_aes_init(&aes);
      mbedtls_aes_setkey_dec(&aes, aes_key, 128);
      mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, rxBuffer, dec);
      mbedtls_aes_free(&aes);

      String msg = (char*)dec;
      Serial.print("📦 "); Serial.println(msg);

      // Parsear formato extendido: "ID,val,raw,dist_real"
      int c1 = msg.indexOf(',');
      int c2 = msg.indexOf(',', c1 + 1);
      int c3 = msg.indexOf(',', c2 + 1);
      
      if (c1 > 0 && c2 > c1 && c3 > c2) {
        int id = msg.substring(0, c1).toInt();
        float val = msg.substring(c1 + 1, c2).toFloat();
        float extra1 = msg.substring(c2 + 1, c3).toFloat();
        float extra2 = msg.substring(c3 + 1).toFloat();

        if (id == 0) {  // TURBIDEZ
          turb_val = val;
          turb_raw = extra1;
          Serial.println("🌊 Turb: " + String(val) + " | RAW: " + String(turb_raw));
        }
        else if (id == 1) {  // pH
          ph_val = val;
          Serial.println("🧪 pH: " + String(val));
        }
        else if (id == 2) {  // NIVEL (ultrasónico)
          nivel_dist_cm = val;                    // Distancia cruda del sensor
          nivel_altura_real = 25.0 - nivel_dist_cm;  // Altura real (tanque 25cm)
          Serial.println("📏 Dist: " + String(nivel_dist_cm) + "cm | Altura: " + String(nivel_altura_real) + "cm");
        }

        controlarActuadores();
        evaluarEstado();
      }
    }
    lora->startReceive();
  }

  actualizarOLED();
  delay(100);
}

void controlarActuadores() {
  // 🔒 Servo: turbidez > 500 = CERRAR
  servoTurbidez.write(turb_val > 500 ? 90 : 0);
  
  // 💡 LED: altura real < 5cm
  digitalWrite(LED_NIVEL_BAJO, nivel_altura_real < 5.0 ? HIGH : LOW);
  
  // 🚨 Buzzer: emergencia
  bool alerta = (turb_val > 500 || nivel_altura_real < 5.0);
  digitalWrite(BUZZER_ALERTA, alerta ? HIGH : LOW);
}

void evaluarEstado() {
  bool turbOK = turb_val <= 500;
  bool phOK = ph_val >= 6.5 && ph_val <= 7.5;
  bool nivelOK = nivel_altura_real >= 5.0;

  if (turbOK && phOK && nivelOK) {
    estadoGlobal = "🟢 OPTIMA";
    accionRecomendada = "Todo OK";
  } else {
    estadoGlobal = "🔴 ALERTA";
    if (!turbOK) accionRecomendada = "Filtrar";
    else if (!nivelOK) accionRecomendada = "Llenar";
    else accionRecomendada = "pH";
  }
}

void actualizarOLED() {
  pantalla.clear();
  pantalla.setFont(ArialMT_Plain_10);
  pantalla.drawString(0, 0, "T:" + String(turb_val,0));
  pantalla.drawString(64, 0, "P:" + String(ph_val,1));
  pantalla.drawString(0, 12, "RAW:" + String(turb_raw,0));
  pantalla.drawString(64, 12, "H:" + String(nivel_altura_real,1));
  pantalla.drawString(0, 24, estadoGlobal);
  pantalla.drawString(0, 36, accionRecomendada);
  pantalla.drawString(0, 48, "S:" + String(servoTurbidez.read()));
  pantalla.display();
}

void webCompleta() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'><meta http-equiv='refresh' content='2'>";
  html += "<title>PTAP Central 📱</title>";
  html += "<style>";
  html += "body{font-family:Arial,sans-serif;margin:20px;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);";
  html += "color:white;text-align:center} .card{display:inline-block;width:300px;margin:15px;padding:20px;";
  html += "background:white;color:#333;border-radius:15px;box-shadow:0 8px 25px rgba(0,0,0,0.3)}";
  html += "h1{font-size:2.5em;margin-bottom:20px;text-shadow:2px 2px 4px rgba(0,0,0,0.3)}";
  html += ".valor{font-size:2em;font-weight:bold;color:#00695c}";
  html += ".estado{font-size:1.5em;font-weight:bold;margin:20px 0}";
  html += ".rojo{color:#d32f2f}.verde{color:#388e3c}";
  html += "</style></head><body>";
  
  html += "<h1>🔐 <span style='color:#fff'>PTAP</span> Central</h1>";
  
  // 🌊 TURBIDEZ
  html += "<div class='card'>";
  html += "<h3>🌊 <span style='color:#00695c'>Turbidez</span></h3>";
  html += "<div class='valor'>" + String(turb_val,1) + " NTU</div>";
  html += "<div>RAW: <b>" + String(turb_raw,0) + "</b></div>";
  html += (turb_val <= 500 ? "<div class='verde'>✅ OK</div>" : "<div class='rojo'>❌ FILTRAR</div>");
  html += "</div>";
  
  // 🧪 pH
  html += "<div class='card'>";
  html += "<h3>🧪 <span style='color:#00695c'>pH</span></h3>";
  html += "<div class='valor'>" + String(ph_val,2) + "</div>";
  html += (ph_val >= 6.5 && ph_val <= 7.5 ? "<div class='verde'>✅ OK</div>" : "<div class='rojo'>❌ CORREGIR</div>");
  html += "</div>";
  
  // 📏 NIVEL
  html += "<div class='card'>";
  html += "<h3>📏 <span style='color:#00695c'>Nivel</span></h3>";
  html += "<div class='valor'>" + String(nivel_altura_real,1) + " cm</div>";
  html += "<div>Dist sensor: " + String(nivel_dist_cm,1) + " cm</div>";
  html += (nivel_altura_real >= 5.0 ? "<div class='verde'>✅ OK</div>" : "<div class='rojo'>❌ LLENAR</div>");
  html += "</div>";
  
  // Estado global
  html += "<div class='estado " + String(estadoGlobal=="🟢 OPTIMA" ? "verde" : "rojo") + "'>";
  html += estadoGlobal + "<br><small>" + accionRecomendada + "</small></div>";
  
  // Actuadores
  html += "<div style='margin-top:20px'>";
  html += "🎛️ Servo: <b>" + String(servoTurbidez.read()) + "°</b> | ";
  html += "💡 LED: <b>" + String(digitalRead(LED_NIVEL_BAJO) ? "ON" : "OFF") + "</b>";
  html += "</div>";
  
  html += "</body></html>";
  server.send(200, "text/html", html);
}
