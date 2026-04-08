/* =============================================================
   PROYECTO: SISTEMA MULTI-AGENTE DE MONITOREO HÍDRICO HELMO (PTAP)
   NODO: AGENTE EMISOR DE TURBIDEZ (ID: 0)
   =============================================================
   Autor: Esteban Eduardo Escarraga
   Propósito: Adquisición, filtrado y transmisión segura de NTU
   Hardware: Heltec WiFi LoRa 32 V3 + Sensor Turbidez (con divisor de tensión)
   FECHA: 31 de marzo de 2026
   DESCRIPTOR: Clasificación de calidad mediante análisis óptico
   ============================================================= */

#define NODE_ID 0           // Identificador único para el Nodo Central
#define NODE_TYPE "TURB"    // Etiqueta del tipo de sensor para depuración

#include <RadioLib.h>       // Protocolo de comunicación física LoRa
#include <Wire.h>           // Protocolo I2C para periféricos
#include "HT_SSD1306Wire.h" // Driver para pantalla OLED (Heltec V3)
#include "mbedtls/aes.h"    // Motor de cifrado avanzado (Hardware Acceleration)

// --- CAPA DE SEGURIDAD: LLAVE AES-128 ---
// Debe ser idéntica en el Nodo Central para permitir la desincronización
const unsigned char aes_key[16] = {
  'E','s','t','e','b','a','n','L','o','R','a','2','0','2','6','!'
};

// --- MAPEO DE HARDWARE (HELTEC ESP32-S3 V3) ---
#define SDA_OLED 17
#define SCL_OLED 18
#define RST_OLED 21
#define VEXT_PIN 36 // Control de alimentación para periféricos externos

#define LORA_CS    8
#define LORA_SCK   9
#define LORA_MOSI 10
#define LORA_MISO 11
#define LORA_RST   12
#define LORA_BUSY 13
#define LORA_DIO1 14

// --- CONFIGURACIÓN DEL SENSOR DE TURBIDEZ ---
#define TURB_AO 1             // Pin de entrada analógica (ADC)
const float VOLT_REF = 3.3;   // Voltaje de referencia del ESP32-S3
const int   ADC_MAX  = 4095;  // Resolución de 12 bits para mayor precisión

// --- FACTOR DE CORRECCIÓN (DIVISOR DE TENSIÓN) ---
// Compensa la reducción de 5.0V a 3.3V usando resistencias de 5.1k y 10k
// Multiplicador: (R1 + R2) / R2 = (5.1 + 10) / 10 = 1.51
const float FACTOR_DIVISOR = 1.51; 

// --- PARÁMETROS DE CLASIFICACIÓN (UMBRALES) ---
// Valor crítico obtenido mediante pruebas de laboratorio para agua no tratada
int RAW_UMBRAL_TURBIO = 500;  

// Inicialización de objetos de hardware
SSD1306Wire pantalla(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);
SX1262* lora;

// Variable para el Filtro de Media Exponencial (EMA)
// Permite suavizar el ruido eléctrico del sensor analógico
float ema_ntu = 0.0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("=== EMISOR TURBIDEZ ID 0 ===");

  // Activación del bus de energía Vext para encender la pantalla OLED
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, LOW); 
  delay(100);

  pantalla.init();
  pantalla.clear();
  pantalla.setFont(ArialMT_Plain_10);
  pantalla.drawString(0, 0, "Emisor TURB ID 0");
  pantalla.display();

  pinMode(TURB_AO, INPUT); // Configuración del puerto del sensor

  // --- CONFIGURACIÓN DEL TRANSCEPTOR LORA ---
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  Module* radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY, SPI);
  lora = new SX1262(radio);

  // Frecuencia 915MHz, Ancho de banda 125kHz, Spreading Factor 7
  int state = lora->begin(915.0, 125.0, 7, 5, 0x12, 22, 8, 1.6);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("LoRa TURB OK");
  } else {
    Serial.print("LoRa error: "); Serial.println(state);
    while (1); // Bloqueo de seguridad si falla la RF
  }
}

// Función Matemática: Mapeo de precisión para datos tipo float
float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void loop() {
  // 1) Adquisición de Datos: Lectura de la señal analógica
  int raw = analogRead(TURB_AO);
  
  // 2) Ajuste de Hardware: Reconstrucción del voltaje real del sensor
  float volt_pin = (raw * VOLT_REF) / ADC_MAX;        // Voltaje leído en el pin (máx 3.3V)
  float volt_sensor = volt_pin * FACTOR_DIVISOR;      // Voltaje original del sensor (máx 5.0V)

  // 3) Procesamiento: Conversión a Unidades de Turbidez Nefelométrica (NTU)
  // Mapeamos considerando el voltaje máximo real del sensor (5.0V) en lugar de 3.3V
  float ntu = mapFloat(volt_sensor, 0.0, 5.0, 0.0, 1000.0);
  if (ntu < 0) ntu = 0;
  if (ntu > 1000) ntu = 1000;

  // 4) Análisis de Tendencia (Edge Computing):
  // Se aplica EMA (Exponential Moving Average) con factor de 0.3 para detectar cambios bruscos
  ema_ntu = 0.3 * ntu + 0.7 * ema_ntu;
  float tendencia = ntu - ema_ntu; // Diferencial de cambio

  // 5) Lógica de Clasificación Binaria
  String estadoAgua;
  if (raw >= RAW_UMBRAL_TURBIO) {
    estadoAgua = "TURBIA";
  } else {
    estadoAgua = "LIMPIA";
  }

  // 6) Construcción del Payload (Protocolo CSV Optimizado)
  // Formato: ID_NODO,VALOR_NTU,VALOR_RAW,VALOR_TENDENCIA
  String payload = String(NODE_ID) + "," +
                   String(ntu, 1) + "," +
                   String(raw) + "," +
                   String(tendencia, 2);

  // 7) CAPA CRIPTOGRÁFICA (AES-128)
  // Preparación del bloque de texto plano de 32 bytes
  unsigned char plain[32] = {0};
  payload.toCharArray((char*)plain, 32);

  unsigned char enc[32]; // Buffer para la trama cifrada
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, aes_key, 128); // Carga de llave simétrica
  mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, plain, enc); // Cifrado ECB
  mbedtls_aes_free(&aes);

  // Transmisión de largo alcance (LoRa)
  int txState = lora->transmit(enc, 32);

  // 8) Telemetría vía Puerto Serial (Debug)
  Serial.println("---- TURBIDEZ TX ----");
  Serial.print("RAW ADC: "); Serial.println(raw);
  Serial.print("Volt Pin: "); Serial.print(volt_pin); Serial.println(" V");
  Serial.print("Volt Sensor (Reconstruido): "); Serial.print(volt_sensor); Serial.println(" V");
  Serial.print("Estado: ");  Serial.println(estadoAgua);
  Serial.print("Payload: "); Serial.println(payload);
  Serial.print("TX estado: "); Serial.println(txState);

  // 9) HMI Local: Actualización de la interfaz OLED
  pantalla.clear();
  pantalla.setFont(ArialMT_Plain_10);
  pantalla.drawString(0, 0, "RAW:" + String(raw));
  pantalla.drawString(0, 12, "NTU:" + String(ntu, 1));
  pantalla.drawString(0, 24, "Est:" + estadoAgua);
  pantalla.drawString(0, 36, "Tr:" + String(tendencia, 2));
  pantalla.display();

  // Ciclo de muestreo: 2000ms (Frecuencia de 0.5 Hz)
  delay(2000); 
}