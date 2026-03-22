// ===== CENTRAL HELMO LoRa + MySQL + ML EMBEBIDO =====
// Archivo: central_node.ino
// Propósito: Nodo central PTAP - Loop abierto (operario decide), ML predictivo

#include <RadioLib.h>                          // LoRa SX1262
#include <WiFi.h>                              // WiFi AP seguro
#include <WebServer.h>                         // Servidor web PTAP
#include <Wire.h>                              // I2C OLED
#include "HT_SSD1306Wire.h"                    // Driver OLED Heltec
#include "mbedtls/aes.h"                       // Cifrado AES datos LoRa
#include <ESP32Servo.h>                        // Servo actuadores
#include <MySQL_Connection.h>                  // MySQL Arduino
#include <MySQL_Cursor.h>                      // Queries MySQL
#include "config_central.h"                    // Pines y credenciales
#include "mysql_helpers.cpp"                   // Funciones MySQL/ML

// 🔑 CLAVE AES (16 bytes para AES-128)
const unsigned char aes_key[16] = {
  'E','s','t','e','b','a','n','L','o','R','a','2','0','2','6','!'
};

// 📡 Objetos globales
SSD1306Wire pantalla(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);
SX1262* lora;                                  // LoRa radio
WebServer server(80);                          // Web port 80
Servo servoTurbidez;                           // Servo turbidez

// 📊 Datos sensores (globales para web/OLED/MySQL)
float turb_val = 0, turb_raw = 0;              // Turbidez + voltaje RAW
float ph_val = 7.0;                            // pH agua
float nivel_val = 0, nivel_dist_cm = 0, nivel_altura_real = 0;  // Nivel calculado
String estadoGlobal = "SIN DATOS";             // Estado general sistema
String accionRecomendada = "INICIANDO";        // Recomendación operario
float turb_prediccion_ml = 0;                  // Predicción ML próxima hora

// 📡 Variables LoRa
volatile bool receivedFlag = false;            // Flag interrupción LoRa
uint8_t rxBuffer[32];                          // Buffer recepción desencriptado

// ⚡ Interrupción LoRa (ISR optimizada ESP32)
#if defined(ESP32)
IRAM_ATTR
#endif
void setFlag(void) {
  receivedFlag = true;                         // Marcar recepción
}

void setup() {
  Serial.begin(115200);                        // Monitor serie debug
  delay(1000);
  Serial.println("🚀 HELMO CENTRAL PTAP v2.0 - ML+MySQL");

  // 🔌 Hardware inicialización
  pinMode(VEXT_PIN, OUTPUT);                   // Alimentación sensores
  digitalWrite(VEXT_PIN, LOW);
  delay(100);

  // 🖥️ OLED inicialización
  pantalla.init();
  pantalla.clear();
  pantalla.setFont(ArialMT_Plain_10);
  pantalla.drawString(0, 0, "HELMO Central PTAP");
  pantalla.drawString(0, 15, "ML + MySQL Ready");
  pantalla.display();

  // 📡 LoRa configuración (parámetros estándar HELMO)
  SPI.begin(9, 11, 10, 8);                     // SPI pines Heltec V3
  Module* radio = new Module(8, 14, 12, 13);   // CS,DIO1,RST,BUSY
  lora = new SX1262(radio);
  
  int state = lora->begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CR, 
                         LORA_SYNC, LORA_POWER, LORA_CURRENT, LORA_RSSI);
  if (state == RADIOLIB_ERR_NONE) {
    lora->setDio1Action(setFlag);              // Habilitar interrupciones
    lora->startReceive();                      // Modo RX continuo
    Serial.println("✓ LoRa 915MHz configurado");
  } else {
    Serial.print("✗ LoRa error: "); Serial.println(state);
    while(1);                                  // Parar si falla LoRa
  }

  // 🎛️ Actuadores (loop abierto - operario PTAP controla vía web)
  pinMode(LED_NIVEL_BAJO, OUTPUT);             // LED visual
  pinMode(BUZZER_ALERTA, OUTPUT);              // Audio alerta
  digitalWrite(LED_NIVEL_BAJO, LOW);
  digitalWrite(BUZZER_ALERTA, LOW);
  
  servoTurbidez.attach(SERVO_TURBIDEZ);        // Servo señal
  servoTurbidez.write(0);                      // Posición inicial ABIERTO

  // 🔐 WiFi AP seguro (operario PTAP se conecta desde celular)
  WiFi.softAP(WIFI_SSID, WIFI_PASS);
  IPAddress ip = WiFi.softAPIP();
  Serial.print("📱 Web PTAP: http://"); Serial.println(ip);

  // 🌐 Servidor web con autenticación
  server.on("/", webCompletaPTAP);             // Ruta principal protegida
  server.begin();

  // 🗄️ MySQL conexión (persistencia para ML)
  conectarMySQL();                             // Inicializar base datos

  // 🖥️ OLED estado final
  pantalla.clear();
  pantalla.drawString(0, 0, "Web: " + ip.toString());
  pantalla.drawString(0, 15, "MySQL: " + String(mysql_conectado ? "OK" : "OFF"));
  pantalla.display();
}

void loop() {
  server.handleClient();                       // Procesar peticiones web

  // 📡 Procesar paquetes LoRa recibidos
  if (receivedFlag) {
    receivedFlag = false;
    Serial.println("\n📡 Paquete LoRa recibido");

    // Leer y desencriptar datos
    int state = lora->readData(rxBuffer, 32);
    if (state == RADIOLIB_ERR_NONE) {
      // 🔓 Desencriptar AES-128
      unsigned char dec[32];
      mbedtls_aes_context aes;
      mbedtls_aes_init(&aes);
      mbedtls_aes_setkey_dec(&aes, aes_key, 128);
      mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, rxBuffer, dec);
      mbedtls_aes_free(&aes);

      String msg = (char*)dec;                 // Convertir a String
      Serial.print("📦 Payload: "); Serial.println(msg);

      // Parsear formato HELMO: "ID,val,raw,dist"
      int c1 = msg.indexOf(',');               // Primera coma
      int c2 = msg.indexOf(',', c1 + 1);       // Segunda coma
      int c3 = msg.indexOf(',', c2 + 1);       // Tercera coma
      
      if (c1 > 0 && c2 > c1 && c3 > c2) {
        int id = msg.substring(0, c1).toInt(); // Sensor ID
        float val = msg.substring(c1 + 1, c2).toFloat();     // Valor principal
        float extra1 = msg.substring(c2 + 1, c3).toFloat();  // RAW o extra
        float extra2 = msg.substring(c3 + 1).toFloat();      // Distancia si aplica

        // 📊 Procesar por sensor
        if (id == 0) {                         // Nodo TURBIDEZ
          turb_val = val;
          turb_raw = extra1;
          Serial.printf("🌊 Turb: %.1f NTU | RAW: %.0f\n", turb_val, turb_raw);
          
          // 🧠 ML automático + guardar MySQL
          guardarSensor(id, val, extra1, 0, 0, 0);  // 0s para campos no aplicables
          
        } else if (id == 1) {                  // Nodo pH
          ph_val = val;
          Serial.printf("🧪 pH: %.2f\n", ph_val);
          guardarSensor(id, val, 0, 0, 0, 0);
          
        } else if (id == 2) {                  // Nodo NIVEL
          nivel_dist_cm = val;
          nivel_altura_real = ALTURA_TANQUE_CM - nivel_dist_cm;
          Serial.printf("📏 Dist: %.1fcm | Altura: %.1fcm\n", nivel_dist_cm, nivel_altura_real);
          guardarSensor(id, nivel_altura_real, 0, nivel_dist_cm, nivel_altura_real, 0);
        }

        evaluarEstadoPTAP();                   // Recomendaciones operario
      }
    }
    lora->startReceive();                      // Volver a modo RX
  }

  actualizarOLED();                            // Refrescar pantalla
  delay(100);                                  // Estabilidad loop
}

// 🎛️ CONTROL LOOP ABIERTO - Recomendaciones para operario PTAP
void evaluarEstadoPTAP() {
  // 🧠 Usar predicción ML para alertas proactivas
  bool turbOK = (turb_val <= TURB_MAX && turb_prediccion_ml < TURB_MAX * 1.2);
  bool phOK = (ph_val >= PH_MIN && ph_val <= PH_MAX);
  bool nivelOK = (nivel_altura_real >= NIVEL_MIN_CM);

  if (turbOK && phOK && nivelOK) {
    estadoGlobal = "🟢 OPTIMA";
    accionRecomendada = "Continuar operación normal";
  } else {
    estadoGlobal = "🔴 ALERTA";
    if (!turbOK) {
      accionRecomendada = "Filtrar agua - Predicción: " + String(turb_prediccion_ml,0) + "NTU";
    } else if (!nivelOK) {
      accionRecomendada = "Llenar tanque inmediatamente";
    } else {
      accionRecomendada = "Corregir pH químico";
    }
  }
}

// 🔧 Control manual actuadores (operario PTAP vía web o inspección)
void controlarActuadoresManual() {
  // Solo alertas visuales/sonoras - NO actuadores automáticos
  // Loop ABIERTO: operario PTAP decide acción final
  digitalWrite(LED_NIVEL_BAJO, nivel_altura_real < NIVEL_MIN_CM ? HIGH : LOW);
  
  bool alerta_critica = (turb_val > TURB_MAX * 1.5 || nivel_altura_real < 2.0);
  digitalWrite(BUZZER_ALERTA, alerta_critica ? HIGH : LOW);
  
  // Servo en posición NEUTRA - operario mueve manualmente
  // servoTurbidez.write(45);  // ← COMENTADO: control manual PTAP
}

// 🖥️ Actualizar OLED con ML
void actualizarOLED() {
  pantalla.clear();
  pantalla.setFont(ArialMT_Plain_10);
  pantalla.drawString(0, 0, "T:" + String(turb_val,0) + " P:" + String(ph_val,1));
  pantalla.drawString(0, 12, "RAW:" + String(turb_raw,0) + " H:" + String(nivel_altura_real,1));
  pantalla.drawString(0, 24, "Pred ML:" + String(turb_prediccion_ml,0));
  pantalla.drawString(0, 36, estadoGlobal);
  pantalla.drawString(0, 48, accionRecomendada.substring(0,21));  // Truncar
  pantalla.display();
}

// 🌐 WEB COMPLETA PTAP con ML (loop abierto)
void webCompletaPTAP() {
  // 🔐 Autenticación obligatoria operario
  if (!server.authenticate(WEB_USER, WEB_PASS)) {
    return server.requestAuthentication();
  }

  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'><meta http-equiv='refresh' content='3'>";
  html += "<title>🟢 PTAP HELMO Central ML</title>";
  html += "<style>body{font-family:Arial;background:linear-gradient(135deg,#667eea,#764ba2);";
  html += "color:white;text-align:center;padding:20px}.card{display:inline-block;width:320px;";
  html += "margin:10px;padding:20px;background:rgba(255,255,255,0.9);border-radius:15px;";
  html += "box-shadow:0 10px 30px rgba(0,0,0,0.3)}.valor{font-size:2.2em;font-weight:bold}";
  html += ".rojo{color:#d32f2f}.verde{color:#388e3c}.pred{color:#ff9800}</style>";
  html += "</head><body><h1 style='font-size:3em'>🔐 PTAP HELMO <span style='color:#00ff88'>Central</span></h1>";

  // 🌊 Card TURBIDEZ + ML
  html += "<div class='card'><h3>🌊 Turbidez</h3>";
  html += "<div class='valor'>" + String(turb_val,1) + " NTU</div>";
  html += "<div>RAW: <b>" + String(turb_raw,0) + "</b></div>";
  html += "<div class='pred'>🤖 Predicción: <b>" + String(turb_prediccion_ml,0) + "</b> NTU</div>";
  html += turb_val <= TURB_MAX ? "<div class='verde'>✅ NORMAL</div>" : "<div class='rojo'>❌ FILTRAR</div>";
  html += "</div>";

  // 🧪 Card pH
  html += "<div class='card'><h3>🧪 pH</h3>";
  html += "<div class='valor'>" + String(ph_val,2) + "</div>";
  html += ph_val >= PH_MIN && ph_val <= PH_MAX ? "<div class='verde'>✅ POTABLE</div>" : "<div class='rojo'>❌ CORREGIR</div>";
  html += "</div>";

  // 📏 Card NIVEL
  html += "<div class='card'><h3>📏 Nivel Tanque</h3>";
  html += "<div class='valor'>" + String(nivel_altura_real,1) + " cm</div>";
  html += "<div>Sensor: " + String(nivel_dist_cm,1) + " cm</div>";
  html += "</div>";

  // 🎛️ Estado global + recomendación operario
  html += "<div class='card' style='width:100%;background:rgba(0,255,0,0.2)'>";
  html += "<h2>" + estadoGlobal + "</h2>";
  html += "<p style='font-size:1.3em'>" + accionRecomendada + "</p>";
  html += "<small>🗄️ MySQL: " + String(mysql_conectado ? "✅ ONLINE" : "❌ OFFLINE") + "</small>";
  html += "</div>";

  html += "<div style='margin-top:30px;font-size:1.2em'>";
  html += "Control LOOP ABIERTO | Operario PTAP autorizado";
  html += "</div></body></html>";

  server.send(200, "text/html", html);         // Enviar página responsive
}
