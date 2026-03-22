// ===== PROCESAMIENTO pH + LoRa HELMO =====
// Archivo: ph_ml.cpp
// Propósito: Lógica sensor pH-4502C + modelo local + cifrado LoRa académico

#include <Arduino.h>
#include <RadioLib.h>
#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include "mbedtls/aes.h"
#include "config_ph.h"

// 📊 Variables estado globales (accesibles desde .ino principal)
float ema_ph = 7.0;                             // Media móvil exponencial pH
float tendencia_ph = 0.0;                       // Tendencia 3 últimas muestras
float historial_ph[ HIST_VENTANA_PH ];          // Buffer circular histórico
int idx_historial_ph = 0;                       // Índice buffer circular
int num_muestras_ph = 0;                        // Muestras válidas acumuladas

SSD1306Wire pantalla(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);
SX1262* lora_ptr_ph;                            // Puntero LoRa global

// 🔍 LEER SENSOR pH-4502C + CONVERTIR A pH
float leerSensorPH() {
  int raw_adc = analogRead(SENSOR_PH_PIN);       // Leer ADC 0-4095
  float voltaje = raw_adc * ADC_VREF / ADC_RESOLUTION;  // Convertir voltios
  
  // Fórmula lineal calibración pH-4502C: pH = 7 + (2.5 - V)/0.18
  float ph_calculado = 7.0 + ((VOLT_PH7 - voltaje) / SENSIBILIDAD_PH);
  ph_calculado = constrain(ph_calculado, PH_MIN, PH_MAX);  // Limitar rango
  
  Serial.printf("pH RAW:%d V:%.3f pH:%.2f\n", raw_adc, voltaje, ph_calculado);
  return ph_calculado;                            // Retornar pH calibrado
}

// 🧠 MODELO LOCAL: EMA + TENDENCIA pH
void actualizarModeloPH(float ph_actual) {
  // EMA: filtro pasa-bajos temporal (suavizado)
  ema_ph = EMA_ALPHA_PH * ph_actual + (1.0 - EMA_ALPHA_PH) * ema_ph;
  
  // Buffer histórico circular
  historial_ph[ idx_historial_ph ] = ph_actual;
  idx_historial_ph = (idx_historial_ph + 1) % HIST_VENTANA_PH;
  if (num_muestras_ph < HIST_VENTANA_PH) num_muestras_ph++;
  
  // Tendencia: promedio reciente vs EMA (positivo=alcalinizando)
  if (num_muestras_ph >= TENDENCIA_MIN_MUESTRAS_PH) {
    int idx_reciente = (idx_historial_ph + HIST_VENTANA_PH - 3) % HIST_VENTANA_PH;
    float promedio_3 = (historial_ph[ idx_reciente ] + 
                       historial_ph[ (idx_reciente + 1) % HIST_VENTANA_PH ] +
                       historial_ph[ (idx_reciente + 2) % HIST_VENTANA_PH ]) / 3.0;
    tendencia_ph = promedio_3 - ema_ph;
  }
}

// 📊 CLASIFICAR ESTADO pH (normativa potable Colombia)
String clasificarEstadoPH(float ph) {
  if (ph >= 6.5 && ph <= 8.5) return "🟢 POTABLE";      // RAS 2000 Colombia
  else if (ph >= 5.5 && ph <= 9.5) return "🟡 CORREGIR"; // Límite tratamiento
  else return "🔴 EMERGENCIA";                          // Fuera rango crítico
}

// 🔐 CIFRAR PAYLOAD LoRa AES-128 (formato: "1,7.12,0.03")
void cifrarPayloadPH(String payload_plano, uint8_t* buffer_encriptado) {
  unsigned char plain[16] = {0};                    // Buffer plano 16 bytes
  payload_plano.getBytes(plain, 16);                // String → bytes
  
  mbedtls_aes_context aes;                          // Contexto AES-128
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, AES_KEY_HELMO, 128);  // Configurar clave
  mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, plain, buffer_encriptado);  // Cifrar ECB
  mbedtls_aes_free(&aes);
}

// 📡 TRANSMITIR PAQUETE LoRa CIFRADO
int enviarPaqueteLoRaPH(float ph_valor, float tendencia) {
  // Payload HELMO: "ID,pH,TENDENCIA" ej: "1,7.12,0.034"
  String payload = String(NODE_ID) + "," + String(ph_valor, 2) + "," + String(tendencia, 3);
  
  uint8_t enc[16];                                  // Buffer AES encriptado
  cifrarPayloadPH(payload, enc);                    // Cifrar datos
  
  int estado_tx = lora_ptr_ph->transmit(enc, 16);   // Enviar LoRa 16 bytes
  
  Serial.print("📦 pH TX: "); Serial.print(payload);  // Log legible
  Serial.print(" | LoRa: "); Serial.println(estado_tx == RADIOLIB_ERR_NONE ? "OK" : "FAIL");
  return estado_tx;
}

// 🖥️ INTERFACE OLED OPERADOR
void actualizarOLED_PH(float ph_actual, float voltaje_raw, String estado_ph) {
  pantalla.clear();
  pantalla.setFont(ArialMT_Plain_10);
  
  // Fila 1: Valor actual + voltaje
  pantalla.drawString(0, 0, "pH:" + String(ph_actual, 2) + " V:" + String(voltaje_raw, 2));
  
  // Fila 2: EMA suavizada
  pantalla.drawString(0, 14, "EMA:" + String(ema_ph, 2));
  
  // Fila 3: Estado normativo
  pantalla.drawString(0, 28, estado_ph);
  
  // Fila 4: Tendencia + estabilidad
  pantalla.drawString(0, 42, "T:" + String(tendencia_ph, 3));
  
  pantalla.display();
}

// 🎯 INICIALIZAR LoRa HELMO (configuración estándar)
bool inicializarLoRaPH() {
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);  // Bus SPI Heltec
  
  Module* radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY, SPI);
  lora_ptr_ph = new SX1262(radio);                     // Instancia SX1262
  
  // Configuración LoRa optimizada HELMO académica
  int estado = lora_ptr_ph->begin(
    LORA_FREQ_MHZ, LORA_BANDWIDTH_KHZ, LORA_SPREADING_FACTOR,
    LORA_CODING_RATE, LORA_SYNC_WORD, LORA_TX_POWER_DBM,
    LORA_CURRENT_LIMIT, LORA_RSSI_OFFSET
  );
  
  return (estado == RADIOLIB_ERR_NONE);     // True si inicialización exitosa
}
