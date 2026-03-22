// ===== CONFIGURACIÓN EMISOR pH HELMO =====
// Archivo: config_ph.h
// Autor: Esteban Eduardo Escarraga Tuquerres - 21 Marzo 2026
// Propósito: Constantes sensor pH-4502C para artículo académico PTAP

#ifndef CONFIG_PH_H
#define CONFIG_PH_H

// 📡 IDENTIFICACIÓN NODO HELMO (único en red multiagente)
#define NODE_ID 1                           // ID único pH (0=turb,1=ph,2=nivel,3=central)
#define NODE_TYPE "EMISOR_PH"               // Identificador logs/OLED

// 🔌 PINES HELTEC V3 ESP32 LoRa (estándar HELMO)
#define SDA_OLED 17                         // SDA pantalla OLED
#define SCL_OLED 18                         // SCL pantalla OLED
#define RST_OLED 21                         // Reset OLED
#define VEXT_PIN 36                         // Alimentación externa 5V

// 📡 PINS LoRa SX1262 Heltec V3 (compatible todos nodos)
#define LORA_CS   8                         // Chip Select LoRa
#define LORA_SCK  9                         // SPI Clock
#define LORA_MOSI 10                        // SPI MOSI
#define LORA_MISO 11                        // SPI MISO
#define LORA_RST  12                        // Reset LoRa
#define LORA_BUSY 13                        // Busy pin LoRa
#define LORA_DIO1 14                        // DIO1 interrupciones

// 🧪 SENSOR pH-4502C ANALÓGICO
#define SENSOR_PH_PIN 2                     // Pin analógico PO (0-4095)
#define ADC_RESOLUTION 4095                 // Resolución ADC ESP32 12-bit
#define ADC_VREF 3.3                        // Referencia voltaje ADC

// 📊 CALIBRACIÓN pH-4502C (experimental - ajustar tampón pH4/7/10)
#define VOLT_PH7 2.5                        // Voltaje sensor pH=7 (agua neutra)
#define VOLT_PH4 2.0                        // Voltaje aproximado pH=4 (ácido)
#define VOLT_PH10 3.0                       // Voltaje aproximado pH=10 (básico)
#define SENSIBILIDAD_PH 0.18                // mV/pH (típico -4502C ~17-20mV/pH)
#define PH_MIN 0.0                          // Rango físico mínimo
#define PH_MAX 14.0                         // Rango físico máximo

// 🧠 MODELO LOCAL pH (EMA + tendencia para ML central)
#define EMA_ALPHA_PH 0.3                    // Factor suavizado exponencial
#define HIST_VENTANA_PH 10                  // Ventana histórica tendencia
#define TENDENCIA_MIN_MUESTRAS_PH 3         // Mínimo para tendencia

// 📡 CONFIG LoRa HELMO (idéntica todos nodos - interoperabilidad)
#define LORA_FREQ_MHZ 915.0                 // Frecuencia Colombia MHz
#define LORA_SPREADING_FACTOR 7             // SF7 rango/batería
#define LORA_BANDWIDTH_KHZ 125.0            // Bandwidth kHz
#define LORA_CODING_RATE 5                  // Coding Rate 4/5
#define LORA_SYNC_WORD 0x12                 // Sync HELMO único
#define LORA_TX_POWER_DBM 22                // Potencia máxima legal
#define LORA_CURRENT_LIMIT 8                // Límite corriente
#define LORA_RSSI_OFFSET 1.6                // Calibración RSSI

// 🔐 CLAVE AES-128 (seguridad comunicaciones HELMO)
const unsigned char AES_KEY_HELMO[16] = {
  'E','s','t','e','b','a','n','L','o','R','a','2','0','2','6','!'
};

#endif // CONFIG_PH_H
