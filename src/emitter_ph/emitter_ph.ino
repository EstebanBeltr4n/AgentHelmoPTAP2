// ===== EMISOR pH HELMO PTAP - NODO ID=1 =====
// Archivo: emitter_ph.ino
// Autor: Esteban Eduardo Escarraga Tuquerres
// Fecha: 21 Marzo 2026 - Proyecto académico calidad agua potable
// Propósito: Nodo LoRa pH-4502C → Central MySQL ML análisis

#include "config_ph.h"                          // Constantes + calibración
#include "ph_ml.cpp"                            // Funciones sensor/LoRa

// 📊 Variables control envío
float ultimo_ph = 7.0;                          // Último valor pH
unsigned long ultimo_envio_ph = 0;              // Timestamp envío LoRa
const unsigned long INTERVALO_PH = 4000;        // 4 segundos intervalo

void setup() {
  Serial.begin(115200);                         // Monitor serie debug
  delay(1000);
  
  Serial.println("🚀 EMISOR pH HELMO ID=1");
  Serial.println("🎓 Proyecto PTAP Smart 2026");

  // 🔌 Inicialización hardware
  pinMode(VEXT_PIN, OUTPUT);                    // Control power sensor
  digitalWrite(VEXT_PIN, LOW);                  // Power off inicial
  delay(100);
  digitalWrite(VEXT_PIN, HIGH);                 // Power on sensor pH

  // 🖥️ Pantalla OLED boot
  pantalla.init();
  pantalla.clear();
  pantalla.setFont(ArialMT_Plain_10);
  pantalla.drawString(0, 0, "HELMO pH ID:1");
  pantalla.drawString(0, 15, "PTAP Calidad 2026");
  pantalla.display();

  // 📡 Radio LoRa inicialización
  if (!inicializarLoRaPH()) {
    Serial.println("✗ CRÍTICO: LoRa pH falló");
    while(1);                                   // Freeze si no radio
  }
  
  Serial.println("✅ Nodo pH operacional");
  delay(2000);
}

void loop() {
  // ⏰ Control intervalo anti-spam LoRa
  if (millis() - ultimo_envio_ph >= INTERVALO_PH) {
    
    // 1. 🧪 LECTURA SENSOR
    float ph_actual = leerSensorPH();           // ADC → pH calibrado
    
    // 2. 🧠 MODELO LOCAL
    actualizarModeloPH(ph_actual);              // EMA + tendencia
    
    // 3. 📊 CLASIFICACIÓN
    String estado_ph = clasificarEstadoPH(ph_actual);
    
    // 4. 📡 TRANSMISIÓN LoRa
    int tx_result = enviarPaqueteLoRaPH(ph_actual, tendencia_ph);
    
    // 5. 🖥️ VISUALIZACIÓN
    actualizarOLED_PH(ph_actual, ultimo_ph * ADC_VREF / ADC_RESOLUTION, estado_ph);
    
    // 📈 LOG ACADÉMICO COMPLETO
    Serial.println("========== HELMO pH ==========");
    Serial.printf("pH: %.2f | EMA: %.2f | Tend: %.3f\n", ph_actual, ema_ph, tendencia_ph);
    Serial.println(estado_ph);
    Serial.printf("LoRa: %s\n", tx_result == RADIOLIB_ERR_NONE ? "OK" : "ERROR");
    Serial.println("=============================");
    
    ultimo_ph = ph_actual;                      // Guardar estado
    ultimo_envio_ph = millis();                 // Reset temporizador
  }
  
  delay(100);                                   // Estabilidad loop
}
