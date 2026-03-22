// ===== EMISOR TURBIDEZ HELMO PTAP - NODO ID=0 =====
// Archivo: emitter_turbidez.ino
// Autor: Esteban Eduardo Escarraga Tuquerres
// Fecha: 21 Marzo 2026 - Proyecto académico calidad agua
// Propósito: Nodo emisor LoRa turbidez → Central ML MySQL

#include "config_turbidez.h"                    // Constantes + pines
#include "turbidez_ml.cpp"                      // Funciones sensor/LoRa

// ⚡ Variables globales accesibles
float ultimo_ntu = 0.0;                         // Última medición NTU
unsigned long ultimo_envio = 0;                 // Timestamp envío LoRa
const unsigned long INTERVALO_ENVIO = 4000;     // 4 segundos entre paquetes

void setup() {
  Serial.begin(115200);                         // Inicializar monitor serie
  delay(1000);                                  // Estabilidad USB
  
  Serial.println("🚀 EMISOR TURBIDEZ HELMO ID=0");
  Serial.println("🎓 Proyecto académico PTAP 2026");

  // 🔌 Hardware power-up
  pinMode(VEXT_PIN, OUTPUT);                    // Control alimentación
  digitalWrite(VEXT_PIN, LOW);                  // Apagar inicialmente
  delay(100);
  digitalWrite(VEXT_PIN, HIGH);                 // Encender sensores

  // 🖥️ OLED boot screen
  pantalla.init();                              // Inicializar I2C OLED
  pantalla.clear();
  pantalla.setFont(ArialMT_Plain_10);
  pantalla.drawString(0, 0, "HELMO Turbidez ID:0");
  pantalla.drawString(0, 15, "PTAP Smart 2026");
  pantalla.display();

  // 📡 LoRa inicialización
  if (!inicializarLoRa()) {                     // Configurar SX1262
    Serial.println("✗ ERROR LoRa crítico");
    while(1);                                   // Parar si falla radio
  }
  
  Serial.println("✅ Sistema listo - Enviando...");
  delay(2000);
}

void loop() {
  // ⏰ Control intervalo envío (evitar spam LoRa)
  if (millis() - ultimo_envio >= INTERVALO_ENVIO) {
    
    // 1. 🌊 LEER SENSOR
    float ntu_actual = leerTurbidezNTU();       // ADC → NTU calibrado
    
    // 2. 🧠 PROCESAR MODELO LOCAL
    actualizarModeloLocal(ntu_actual);          // EMA + tendencia
    
    // 3. 📊 CLASIFICAR ESTADO
    String estado_agua = clasificarEstadoAgua(ntu_actual);
    
    // 4. 📡 ENVIAR LoRa AES
    int estado_tx = enviarPaqueteLoRa(ntu_actual, tendencia_turbidez);
    
    // 5. 🖥️ ACTUALIZAR VISUAL
    actualizarOLED(ntu_actual, ultimo_ntu * ADC_VREF / ADC_RESOLUTION, estado_agua);
    
    // 📈 LOG ACADEMICO COMPLETO
    Serial.println("========== HELMO TURBIDEZ ==========");
    Serial.printf("NTU: %.1f | EMA: %.1f | Tend: %.2f\n", ntu_actual, ema_turbidez, tendencia_turbidez);
    Serial.println(estado_agua);
    Serial.printf("LoRa TX: %s\n", estado_tx == RADIOLIB_ERR_NONE ? "OK" : "ERROR");
    Serial.println("====================================");
    
    ultimo_ntu = ntu_actual;                    // Guardar última
    ultimo_envio = millis();                    // Reset timer
  }
  
  delay(100);                                   // Estabilidad loop principal
}
