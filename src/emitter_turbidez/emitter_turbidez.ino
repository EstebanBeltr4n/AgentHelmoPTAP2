/* ==============================================================================
   PROYECTO: SISTEMA MULTI-AGENTE DE MONITOREO HÍDRICO HELMO (PTAP)
   NODO: AGENTE EMISOR DE TURBIDEZ (ID: 0)
   VERSIÓN: 2.0 (Lectura Directa & Optimización Energética AN996)
   ==============================================================================
   Descripción: 
   Adquisición directa, filtrado digital (EMA), empaquetado en buffer y 
   transmisión inalámbrica LoRa segura.
   
   Estrategia Keep-Alive (Hardware Powerbank): 
   Se utiliza una batería AN996 (5V/2.4A, 18.5Wh) que corta el suministro tras 
   30s de inactividad. El ciclo de trabajo de este nodo ejecuta lecturas cada 2.5s 
   y transmisiones RF cada 25s (Buffer de 10), garantizando que el consumo de 
   corriente del ESP32 y el radio SX1262 evite la suspensión de la fuente.

   HMI Local: OLED SSD1306 con telemetría en tiempo real (Voltaje, NTU, %, Estado).
   
   Nota de Hardware: El divisor de tensión ha sido removido. El módulo de turbidez 
   inyecta la señal directamente al ADC del ESP32-S3. Se utiliza la atenuación 
   interna de 11dB para tolerar rangos lógicos hasta ~3.3V.
   ============================================================================== */

#define NODE_ID 0           // Identificador único en la topología HELMO
#define NODE_TYPE "TURB"    // Clasificación del agente

#include <RadioLib.h>       // Gestión del transceptor físico LoRa
#include <Wire.h>           // Bus I2C para periféricos
#include "HT_SSD1306Wire.h" // Controlador HMI OLED (Heltec V3)
#include "mbedtls/aes.h"    // Aceleración criptográfica por hardware (ESP-IDF)

// --- CAPA DE SEGURIDAD: LLAVE AES-128 ---
const unsigned char aes_key[16] = {
  'E','s','t','e','b','a','n','L','o','R','a','2','0','2','6','!'
};

// --- MAPEO DE HARDWARE (HELTEC ESP32-S3 V3) ---
#define SDA_OLED 17
#define SCL_OLED 18
#define RST_OLED 21
#define VEXT_PIN 36 // Control del bus de alimentación periférica

// Pines del bus SPI dedicados al módulo LoRa interno
#define LORA_CS    8
#define LORA_SCK   9
#define LORA_MOSI 10
#define LORA_MISO 11
#define LORA_RST   12
#define LORA_BUSY 13
#define LORA_DIO1 14

// --- CONFIGURACIÓN DEL SENSOR ÓPTICO DE TURBIDEZ ---
#define TURB_AO 4             // Canal ADC (Lectura analógica directa)
const float VOLT_REF = 3.3;   // Voltaje lógico máximo tolerado por el ADC
const int   ADC_MAX  = 4095;  // Resolución ampliada (12 bits)

// --- GESTIÓN DE ENERGÍA Y BUFFER DE TRANSMISIÓN ---
const int MAX_BUFFER = 10;    
int current_buffer = 0;       
long sum_raw = 0;             
float sum_ntu = 0.0;          

// --- OBJETOS DE SISTEMA ---
SSD1306Wire pantalla(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);
SX1262* lora;
float ema_ntu = 0.0;          // Variable para Filtro de Media Exponencial

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n========================================");
  Serial.println("  INICIANDO AGENTE HELMO - TURBIDEZ ID 0  ");
  Serial.println("  Estrategia Keep-Alive: Activa (25s TX)  ");
  Serial.println("========================================");

  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, LOW); 
  delay(100);

  pantalla.init();
  pantalla.clear();
  pantalla.setFont(ArialMT_Plain_10);
  pantalla.drawString(0, 0, "Agente Turb");
  pantalla.drawString(0, 15, "Iniciando hardware...");
  pantalla.display();

  // Configuración del ADC con atenuación máxima para tolerar lecturas directas
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  pinMode(TURB_AO, INPUT); 

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  Module* radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY, SPI);
  lora = new SX1262(radio);

  int state = lora->begin(915.0, 125.0, 7, 5, 0x12, 22, 8, 1.6);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("[LoRa] Transceptor listo y calibrado.");
  } else {
    Serial.printf("[LoRa] Fallo crítico. Error: %d\n", state);
    while (1); 
  }
}

float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void loop() {
  // ---------------------------------------------------------
  // 1. ADQUISICIÓN Y PROCESAMIENTO DIRECTO DE SEÑAL
  // ---------------------------------------------------------
  int raw = analogRead(TURB_AO);
  float volt_pin = (raw * VOLT_REF) / ADC_MAX;        

  // Lógica de cálculo estándar para sensores ópticos de turbidez:
  // Mayor voltaje = Agua limpia. Menor voltaje = Agua turbia.
  // Se asume un límite superior lógico de 3.3V debido a la arquitectura del ESP32.
  
  float ntu = 0.0;
  if(volt_pin < 1.0) {
    ntu = 3000.0; // Saturación por opacidad extrema
  } else {
    // Aproximación polinómica inversa o mapeo lineal
    ntu = mapFloat(volt_pin, 3.3, 1.0, 0.0, 3000.0);
  }
  
  if (ntu < 0) ntu = 0;
  if (ntu > 3000) ntu = 3000;
  
  // Cálculo porcentual (0% = Cristalina, 100% = Máxima opacidad medida)
  float porcentaje = (ntu / 3000.0) * 100.0;

  // Clasificación de estados basada en niveles de voltaje recibidos
  String estadoAgua;
  if (volt_pin >= 2.8) {
    estadoAgua = "LIMPIA";
  } else if (volt_pin < 2.8 && volt_pin >= 1.8) {
    estadoAgua = "MEDIA";
  } else {
    estadoAgua = "TURBIA";
  }

  // Filtro EMA (Media Móvil Exponencial) para atenuación de ruido
  ema_ntu = 0.3 * ntu + 0.7 * ema_ntu;

  // ---------------------------------------------------------
  // 2. GESTIÓN DEL BUFFER
  // ---------------------------------------------------------
  sum_raw += raw;
  sum_ntu += ntu;
  current_buffer++;

  // ---------------------------------------------------------
  // 3. TELEMETRÍA Y HMI LOCAL
  // ---------------------------------------------------------
  Serial.printf("[LECTURA] V_Pin: %.2fV | Est: %s | Turb: %.1f%% | NTU: %.1f | Buff: %d/%d\n", 
                volt_pin, estadoAgua.c_str(), porcentaje, ntu, current_buffer, MAX_BUFFER);

  pantalla.clear();
  pantalla.setFont(ArialMT_Plain_10);
  pantalla.drawString(0, 0,  "Agente Turb | " + estadoAgua);
  pantalla.drawString(0, 12, "Calidad: " + String(porcentaje, 1) + " %");
  pantalla.drawString(0, 24, "Voltaje: " + String(volt_pin, 2) + " V");
  pantalla.drawString(0, 36, "Val NTU: " + String(ntu, 1));
  
  String bufferStr = "Buff [";
  for(int i=0; i<MAX_BUFFER; i++) {
    bufferStr += (i < current_buffer) ? "#" : ".";
  }
  bufferStr += "]";
  pantalla.drawString(0, 48, bufferStr);
  pantalla.display();

  // ---------------------------------------------------------
  // 4. PROTOCOLO DE TRANSMISIÓN RF (Ráfaga cada 25 segundos)
  // ---------------------------------------------------------
  if (current_buffer >= MAX_BUFFER) {
    Serial.println("\n[SISTEMA] Buffer saturado. Ejecutando ráfaga LoRa (Keep-Alive)...");
    
    int prom_raw = sum_raw / MAX_BUFFER;
    float prom_ntu = sum_ntu / MAX_BUFFER;
    float tendencia = prom_ntu - ema_ntu; 

    String payload = String(NODE_ID) + "," +
                     String(prom_ntu, 1) + "," +
                     String(prom_raw) + "," +
                     String(tendencia, 2);

    Serial.println("[TX] Trama original: " + payload);

    // Cifrado AES-128
    unsigned char plain[32] = {0};
    payload.toCharArray((char*)plain, 32);
    unsigned char enc[32]; 
    
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, aes_key, 128); 
    mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, plain, enc); 
    mbedtls_aes_free(&aes);

    int txState = lora->transmit(enc, 32);

    if (txState == RADIOLIB_ERR_NONE) {
      Serial.println("[TX] Estatus: OK (Paquete inyectado en red LoRa)\n");
      pantalla.drawString(90, 36, "TX OK"); 
    } else {
      Serial.printf("[TX] Estatus: FALLO. Error: %d\n\n", txState);
      pantalla.drawString(90, 36, "TX ERR");
    }
    pantalla.display();

    current_buffer = 0;
    sum_raw = 0;
    sum_ntu = 0.0;
  }

  // El delay se ha ajustado a 2500ms. 
  // 10 muestras * 2.5s = Transmisión cada 25 segundos.
  // Esto evita categóricamente que la Powerbank AN996 alcance los 30s de inactividad.
  delay(2500); 
}