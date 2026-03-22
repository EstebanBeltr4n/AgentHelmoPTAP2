// ===== CONFIGURACIÓN EMISOR TURBIDEZ HELMO =====
// Archivo: config_turbidez.h
// Autor: Esteban Eduardo Escarraga Tuquerres - 21 Marzo 2026
// Propósito: Constantes específicas sensor turbidez para artículo académico

#ifndef CONFIG_TURBIDEZ_H
#define CONFIG_TURBIDEZ_H

// 📡 IDENTIFICACIÓN NODO HELMO (único en red LoRa)
#define NODE_ID 0                           // ID único turbidez (0,1,2,3)
#define NODE_TYPE "EMISOR_TURB"             // Tipo para logs/OLED

// 🔌 PINES HELTEC V3 ESP32 LoRa
#define SDA_OLED 17                         // SDA pantalla OLED
#define SCL_OLED 18                         // SCL pantalla OLED
#define RST_OLED 21                         // Reset OLED
#define VEXT_PIN 36                         // Alimentación externa 5V

// 📡 PINS LoRa SX1262 Heltec V3 (estándar HELMO)
#define LORA_CS   8                         // Chip Select LoRa
#define LORA_SCK  9                         // SPI Clock
#define LORA_MOSI 10                        // SPI MOSI
#define LORA_MISO 11                        // SPI MISO
#define LORA_RST  12                        // Reset LoRa
#define LORA_BUSY 13                        // Busy pin LoRa
#define LORA_DIO1 14                        // DIO1 interrupciones

// 🌊 SENSOR TURBIDEZ ANALÓGICO
#define SENSOR_TURBIDEZ_PIN 1               // Pin analógico AO (0-4095)
#define ADC_RESOLUTION 4095                 // Resolución ADC ESP32 12-bit
#define ADC_VREF 3.3                        // Referencia voltaje ADC

// 📊 CALIBRACIÓN ACADEMICA (ajustar experimentalmente)
#define RAW_AGUA_LIMPIA 1000                // Valor ADC agua destilada (0 NTU)
#define RAW_AGUA_SUCIA  800                 // Valor ADC agua turbia (1000 NTU)
#define NTU_MIN 0                           // Rango NTU mínimo
#define NTU_MAX 1000                        // Rango NTU máximo potable

// 🧠 MODELO LOCAL (EMA + tendencia para ML central)
#define EMA_ALPHA 0.3                       // Factor suavizado exponencial (0-1)
#define HIST_VENTANA 10                     // Ventana histórica tendencia (muestras)
#define TENDENCIA_MIN_MUESTRAS 3            // Mínimo para calcular tendencia

// 📡 CONFIG LoRa HELMO (compatible todos nodos)
#define LORA_FREQ_MHZ 915.0                 // Frecuencia Colombia MHz
#define LORA_SPREADING_FACTOR 7             // SF7 rango/batería balanceado
#define LORA_BANDWIDTH_KHZ 125.0            // BW 125kHz
#define LORA_CODING_RATE 5                  // CR 4/5
#define LORA_SYNC_WORD 0x12                 // Sync único HELMO
#define LORA_TX_POWER_DBM 22                // Potencia máxima legal
#define LORA_CURRENT_LIMIT 8                // Límite corriente
#define LORA_RSSI_OFFSET 1.6                // Calibración RSSI

// 🔐 CLAVE AES-128 HELMO (seguridad comunicaciones)
const unsigned char AES_KEY_HELMO[16] = {
  'E','s','t','e','b','a','n','L','o','R','a','2','0','2','6','!'
};

#endif // CONFIG_TURBIDEZ_H
