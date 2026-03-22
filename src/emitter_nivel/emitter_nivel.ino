// ===== EMISOR NIVEL HELMO PTAP - NODO ID=2 =====
// Archivo: emitter_nivel.ino
// Autor: Esteban Eduardo Escarraga Tuquerres
// Fecha: 21 Marzo 2026 - Proyecto académico monitoreo agua
// Propósito: HC-SR04 nivel tanque → Central MySQL ML predictivo

#include "config_nivel.h"                       // Constantes HC-SR04
#include "nivel_ultrasonico.cpp"                // Funciones sensor/LoRa

// 📊 Control temporización
float ultimo_nivel_cm = 0.0;                    // Última altura
unsigned long ultimo_tx_nivel = 0;              // Timestamp LoRa
const unsigned long INTERVALO_NIVEL = 4000;     // 4s anti-spam

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("🚀 EMISOR NIVEL HELMO ID=2");
  Serial.println("🎓 PTAP Monitoreo Tanques 2026");

  // 🔌 Power-up secuencial
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, LOW);
  delay(100);
  digitalWrite(VEXT_PIN, HIGH);  // HC-SR04 on

  // 🖥️ OLED inicialización
  pantalla.init();
  pantalla.clear();
  pantalla.setFont(ArialMT_Plain_10);
  pantalla.drawString(0, 0, "HELMO Nivel ID:2");
  pantalla.drawString(0, 15, "PTAP Tanques 2026");
  pantalla.display();

  // 📡 HC-SR04 pines
  pinMode(TRIG_PIN, OUTPUT);
  digitalWrite(TRIG_PIN, LOW);
  pinMode(ECHO_PIN, INPUT_PULLDOWN);

  // 📡 LoRa boot
  if (!inicializarLoRaNivel()) {
    Serial.println("✗ FATAL: LoRa nivel falló");
    while(1);
  }
  
  Serial.println("✅ Nodo nivel activo");
  delay(2000);
}

void loop() {
  // ⏰ Temporizador envío
  if (millis() - ultimo_tx_nivel >= INTERVALO_NIVEL) {
    
    // 1. 📏 DISTANCIA SENSOR
    float distancia_cm = medirDistanciaUltrasonico();
    
    // 2. 🧮 ALTURA TANQUE
    float altura_actual = calcularAlturaAgua(distancia_cm);
    
    // 3. 🧠 MODELO LOCAL
    actualizarModeloNivel(altura_actual);
    
    // 4. 📊 ESTADO OPERACIONAL
    String estado_nivel = clasificarNivel(altura_actual, distancia_cm);
    
    // 5. 📡 ENVÍO LoRa
    int resultado_tx = enviarNivelLoRa(altura_actual, tendencia_nivel);
    
    // 6. 🖥️ VISUAL
    actualizarOLED_Nivel(distancia_cm, altura_actual, estado_nivel);
    
    // 📈 LOG ACADÉMICO
    Serial.println("========== HELMO NIVEL ==========");
    Serial.printf("Dist: %.1fcm | Alt: %.2fcm\n", distancia_cm, altura_actual);
    Serial.println(estado_nivel);
    Serial.printf("EMA: %.2f | Tend: %.3f\n", ema_nivel, tendencia_nivel);
    Serial.printf("LoRa: %s\n", resultado_tx == RADIOLIB_ERR_NONE ? "OK" : "ERROR");
    Serial.println("================================");
    
    ultimo_nivel_cm = altura_actual;
    ultimo_tx_nivel = millis();
  }
  
  delay(100);
}
