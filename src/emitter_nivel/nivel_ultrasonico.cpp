// ===== PROCESAMIENTO NIVEL + LoRa HELMO =====
// Archivo: nivel_ultrasonico.cpp
// Propósito: HC-SR04 + modelo predictivo + cifrado LoRa académico PTAP

#include <Arduino.h>
#include <RadioLib.h>
#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include "mbedtls/aes.h"
#include "config_nivel.h"

// 📊 Variables globales estado (accesibles .ino)
float ema_nivel = 0.0;                          // EMA altura agua cm
float tendencia_nivel = 0.0;                    // Tendencia nivel
float historial_nivel[ HIST_VENTANA_NIVEL ];    // Buffer histórico
int idx_historial_nivel = 0;                    // Índice circular
int num_muestras_nivel = 0;                     // Muestras válidas

SSD1306Wire pantalla(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);
SX1262* lora_ptr_nivel;                         // Puntero LoRa global

// 📏 MEDIR DISTANCIA HC-SR04 ULTRASÓNICO
float medirDistanciaUltrasonico() {
  digitalWrite(TRIG_PIN, LOW);                  // Reset trigger
  delayMicroseconds(5);
  digitalWrite(TRIG_PIN, HIGH);                 // Pulso 10µs
  delayMicroseconds(12);
  digitalWrite(TRIG_PIN, LOW);                  // Fin pulso
  
  long duracion_us = pulseIn(ECHO_PIN, HIGH, TIMEOUT_US);  // Esperar eco
  
  float distancia_cm = 0.0;
  if (duracion_us > 100 && duracion_us < 45000) {  // Validar rango
    distancia_cm = (duracion_us * VELOCIDAD_SONIDO_CM_US) / 2.0;  // Ida/vuelta
  }
  
  Serial.printf("HC-SR04: %ldµs → %.1fcm\n", duracion_us, distancia_cm);
  return constrain(distancia_cm, DIST_MINIMA_CM, DIST_MAXIMA_CM);
}

// 🧠 CALCULAR ALTURA AGUA TANQUE
float calcularAlturaAgua(float distancia_sensor) {
  // Altura = altura_tanque - distancia_sensor + offset
  float altura_cm = ALTURA_TANQUE_CM - distancia_sensor + DIST_SENSOR_FONDO_CM;
  altura_cm = constrain(altura_cm, 0.0, ALTURA_TANQUE_CM);  // 0 a máxima
  
  float porcentaje = (altura_cm / ALTURA_TANQUE_CM) * 100.0;
  Serial.printf("Altura: %.2fcm (%.1f%%)\n", altura_cm, porcentaje);
  return altura_cm;
}

// 🧠 MODELO LOCAL NIVEL (EMA + tendencia predictiva)
void actualizarModeloNivel(float altura_actual) {
  // EMA suavizado temporal (reduce ruido ultrasónico)
  ema_nivel = EMA_ALPHA_NIVEL * altura_actual + (1.0 - EMA_ALPHA_NIVEL) * ema_nivel;
  
  // Buffer histórico circular
  historial_nivel[ idx_historial_nivel ] = altura_actual;
  idx_historial_nivel = (idx_historial_nivel + 1) % HIST_VENTANA_NIVEL;
  if (num_muestras_nivel < HIST_VENTANA_NIVEL) num_muestras_nivel++;
  
  // Tendencia predictiva (3 recientes vs EMA)
  if (num_muestras_nivel >= TENDENCIA_MIN_MUESTRAS) {
    int idx_recientes = (idx_historial_nivel + HIST_VENTANA_NIVEL - 3) % HIST_VENTANA_NIVEL;
    float prom_recientes = (historial_nivel[ idx_recientes ] + 
                           historial_nivel[ (idx_recientes + 1) % HIST_VENTANA_NIVEL ] +
                           historial_nivel[ (idx_recientes + 2) % HIST_VENTANA_NIVEL ]) / 3.0;
    tendencia_nivel = prom_recientes - ema_nivel;  // Positiva=subiendo
  }
}

// 📊 CLASIFICAR ESTADO NIVEL (operacional PTAP)
String clasificarNivel(float altura_cm, float distancia_cm) {
  if (altura_cm > 3.0) return "🟢 NIVEL ALTO";      // >75% tanque
  else if (altura_cm > 1.5) return "🟡 MEDIO";       // 37-75%
  else if (distancia_cm <= 8.0) return "🟠 BAJO";    // 18-37%
  else return "🔴 VACÍO";                           // <18% crítico
}

// 🔐 CIFRAR LoRa AES-128 ("2,2.45,0.12")
void cifrarPayloadNivel(String payload, uint8_t* enc_buffer) {
  unsigned char plain[16] = {0};
  payload.getBytes(plain, 16);
  
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, AES_KEY_HELMO, 128);
  mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, plain, enc_buffer);
  mbedtls_aes_free(&aes);
}

// 📡 TRANSMITIR LoRa NIVEL
int enviarNivelLoRa(float altura, float tendencia) {
  String payload = String(NODE_ID) + "," + String(altura, 2) + "," + String(tendencia, 3);
  
  uint8_t enc[16];
  cifrarPayloadNivel(payload, enc);
  
  int tx_estado = lora_ptr_nivel->transmit(enc, 16);
  
  Serial.print("📦 Nivel TX: "); Serial.print(payload);
  Serial.print(" | LoRa: "); Serial.println(tx_estado == RADIOLIB_ERR_NONE ? "OK" : "FAIL");
  return tx_estado;
}

// 🖥️ OLED INTERFACE
void actualizarOLED_Nivel(float dist_cm, float altura_cm, String estado_nivel) {
  pantalla.clear();
  pantalla.setFont(ArialMT_Plain_10);
  
  pantalla.drawString(0, 0, "Dist:" + String(dist_cm, 1) + "cm");
  pantalla.drawString(0, 12, "Alt:" + String(altura_cm, 2) + "cm");
  pantalla.drawString(0, 24, estado_nivel);
  pantalla.drawString(0, 36, "EMA:" + String(ema_nivel, 2));
  pantalla.drawString(0, 48, "T:" + String(tendencia_nivel, 3));
  
  pantalla.display();
}

// 🎯 INICIALIZAR LoRa HELMO
bool inicializarLoRaNivel() {
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  
  Module* radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY, SPI);
  lora_ptr_nivel = new SX1262(radio);
  
  int estado = lora_ptr_nivel->begin(
    LORA_FREQ_MHZ, LORA_BANDWIDTH_KHZ, LORA_SPREADING_FACTOR,
    LORA_CODING_RATE, LORA_SYNC_WORD, LORA_TX_POWER_DBM,
    LORA_CURRENT_LIMIT, LORA_RSSI_OFFSET
  );
  
  return (estado == RADIOLIB_ERR_NONE);
}
