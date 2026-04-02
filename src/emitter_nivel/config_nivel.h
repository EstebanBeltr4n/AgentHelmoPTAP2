// ===== CONFIGURACIÓN EMISOR NIVEL HELMO =====
// Archivo: config_nivel.h
// Autor: Esteban Eduardo Escarraga Tuquerres - 21 Marzo 2026
// Propósito: Constantes HC-SR04 ultrasónico para artículo PTAP académico

#ifndef CONFIG_NIVEL_H
#define CONFIG_NIVEL_H

// 📡 IDENTIFICACIÓN NODO HELMO (red multiagente)
#define NODE_ID 2                           // ID único nivel (0=turb,1=ph,2=nivel,3=central)
#define NODE_TYPE "EMISOR_NIVEL"            // Identificador sistema

// 🔌 PINES HELTEC V3 ESP32 LoRa (estándar HELMO)
#define SDA_OLED 17                         // SDA pantalla OLED
#define SCL_OLED 18                         // SCL pantalla OLED
#define RST_OLED 21                         // Reset OLED
#define VEXT_PIN 36                         // Alimentación externa 5V

// 📡 PINS LoRa SX1262 Heltec V3 (interoperable)
#define LORA_CS   8                         // Chip Select
#define LORA_SCK  9                         // SPI Clock
#define LORA_MOSI 10                        // SPI MOSI
#define LORA_MISO 11                        // SPI MISO
#define LORA_RST  12                        // Reset LoRa
#define LORA_BUSY 13                        // Busy indicator
#define LORA_DIO1 14                        // DIO1 TX/RX

// 📏 SENSOR ULTRASÓNICO HC-SR04
#define TRIG_PIN 4                          // Trigger pulso ultrasónico
#define ECHO_PIN 5                          // Echo recepción
#define TIMEOUT_US 50000                    // Timeout 50ms (17m máx)
#define VELOCIDAD_SONIDO_CM_US 0.034        // 343m/s → cm/µs

// 📊 GEOMETRÍA TANQUE (calibración física)
#define ALTURA_TANQUE_CM 4.0                // Altura máxima medible (ajustar)
#define DIST_SENSOR_FONDO_CM 0.0            // Offset sensor-fondo
#define DIST_MINIMA_CM 2.0                  // Distancia mínima válida
#define DIST_MAXIMA_CM 10.0                 // Distancia máxima válida

// 🧠 MODELO LOCAL NIVEL (EMA + tendencia)
#define EMA_ALPHA_NIVEL 0.3                 // Suavizado exponencial
#define HIST_VENTANA_NIVEL 10               // Buffer histórico
#define TENDENCIA_MIN_MUESTRAS 3            // Mínimo tendencia

// 📡 CONFIG LoRa HELMO (unificada todos nodos)
#define LORA_FREQ_MHZ 915.0                 // Colombia MHz
#define LORA_SPREADING_FACTOR 7             // SF7 balanceado
#define LORA_BANDWIDTH_KHZ 125.0            // 125kHz
#define LORA_CODING_RATE 5                  // CR 4/5
#define LORA_SYNC_WORD 0x12                 // HELMO sync
#define LORA_TX_POWER_DBM 22                // Potencia máxima
#define LORA_CURRENT_LIMIT 8                // Corriente
#define LORA_RSSI_OFFSET 1.6                // RSSI calibración

// 🔐 AES-128 HELMO (seguridad datos críticos)
const unsigned char AES_KEY_HELMO[16] = {
  'E','s','t','e','b','a','n','L','o','R','a','2','0','2','6','!'
};

#endif // CONFIG_NIVEL_H
