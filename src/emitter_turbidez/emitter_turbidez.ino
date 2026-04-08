/* ==============================================================================
   PROYECTO: SISTEMA MULTI-AGENTE DE MONITOREO HÍDRICO HELMO (PTAP)
   NODO: AGENTE EMISOR DE TURBIDEZ (ID: 0)
   ==============================================================================
   Descripción: 
   Adquisición, filtrado, promediado en buffer y transmisión LoRa segura.
   Este nodo está optimizado para funcionar ininterrumpidamente alimentado por 
   una Powerbank comercial (ej. AN-P96, 5V/2.4A, 5000mAh/18.5Wh). El ciclo de 
   trabajo activo previene el auto-apagado de la batería externa, garantizando 
   energía constante para el sensor óptico y el transceptor SX1262.

   HMI Local: Pantalla OLED con telemetría en tiempo real (Estado, %, Voltaje).
   ============================================================================== */

#define NODE_ID 0           // Identificador único en la topología HELMO
#define NODE_TYPE "TURB"    // Clasificación del agente

#include <RadioLib.h>       // Gestión del transceptor físico LoRa
#include <Wire.h>           // Bus I2C para periféricos
#include "HT_SSD1306Wire.h" // Controlador HMI OLED (Heltec V3)
#include "mbedtls/aes.h"    // Aceleración criptográfica por hardware para ESP32

// --- CAPA DE SEGURIDAD: LLAVE AES-128 ---
// Cifrado simétrico para protección de datos ambientales en la red LoRa
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
#define TURB_AO 4             // Canal ADC para lectura analógica pura
const float VOLT_REF = 3.3;   // Voltaje lógico del microcontrolador
const int   ADC_MAX  = 4095;  // Resolución ampliada (12 bits)
const float FACTOR_DIVISOR = 1.50; // Reconstrucción de señal (Divisor 10k/20k)

int RAW_UMBRAL_TURBIO = 500;  // Límite de corte empírico para clasificación

// --- GESTIÓN DE ENERGÍA Y BUFFER DE TRANSMISIÓN ---
// Retiene datos localmente para enviarlos en ráfagas espaciadas, 
// reduciendo la saturación del espectro RF y el consumo pico de antena.
const int MAX_BUFFER = 10;    
int current_buffer = 0;       
long sum_raw = 0;             
float sum_ntu = 0.0;          

// --- OBJETOS DE SISTEMA ---
SSD1306Wire pantalla(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);
SX1262* lora;
float ema_ntu = 0.0;          // Filtro de Media Exponencial

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n========================================");
  Serial.println("  INICIANDO AGENTE HELMO - TURBIDEZ ID 0  ");
  Serial.println("  Hardware: Heltec V3 | PSU: Powerbank 5V ");
  Serial.println("========================================");

  // Habilitar energía para periféricos y pantalla
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, LOW); 
  delay(100);

  // Inicialización HMI OLED
  pantalla.init();
  pantalla.clear();
  pantalla.setFont(ArialMT_Plain_10);
  pantalla.drawString(0, 0, "Helmo - Agente TURB");
  pantalla.drawString(0, 15, "Iniciando hardware...");
  pantalla.display();

  // Configuración de adquisición de datos
  analogReadResolution(12);
  pinMode(TURB_AO, INPUT); 

  // Inicialización del Transceptor LoRa
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  Module* radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY, SPI);
  lora = new SX1262(radio);

  // Parámetros RF para alta montaña: Freq, BW, SF, CR, SyncWord, Power, Preamble, TCXO
  int state = lora->begin(915.0, 125.0, 7, 5, 0x12, 22, 8, 1.6);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("[LoRa] Transceptor listo y calibrado.");
  } else {
    Serial.printf("[LoRa] Fallo crítico de hardware. Error: %d\n", state);
    while (1); // Detener ejecución si el radio falla
  }
}

// Función auxiliar para mapeo de precisión en coma flotante
float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void loop() {
  // ---------------------------------------------------------
  // 1. ADQUISICIÓN Y PROCESAMIENTO DE SEÑAL
  // ---------------------------------------------------------
  int raw = analogRead(TURB_AO);
  float volt_pin = (raw * VOLT_REF) / ADC_MAX;        
  float volt_sensor = volt_pin * FACTOR_DIVISOR;      

  // Mapeo a unidades nefelométricas (NTU) y porcentaje relativo
  float ntu = mapFloat(volt_sensor, 0.0, 5.0, 0.0, 1000.0);
  if (ntu < 0) ntu = 0;
  if (ntu > 1000) ntu = 1000;
  
  float porcentaje = (ntu / 1000.0) * 100.0;

  // Clasificación de estado
  String estadoAgua = (raw >= RAW_UMBRAL_TURBIO) ? "TURBIA" : "LIMPIA";

  // Suavizado de curva mediante filtro paso bajo (EMA)
  ema_ntu = 0.3 * ntu + 0.7 * ema_ntu;

  // ---------------------------------------------------------
  // 2. GESTIÓN DEL BUFFER
  // ---------------------------------------------------------
  sum_raw += raw;
  sum_ntu += ntu;
  current_buffer++;

  // ---------------------------------------------------------
  // 3. TELEMETRÍA Y HMI LOCAL (Monitor Serie + Pantalla OLED)
  // ---------------------------------------------------------
  // Salida por monitor serial para depuración
  Serial.printf("[LECTURA] V_Pin: %.2fV | Est: %s | Turb: %.1f%% | NTU: %.1f | Buff: %d/%d\n", 
                volt_pin, estadoAgua.c_str(), porcentaje, ntu, current_buffer, MAX_BUFFER);

  // Actualización de la interfaz OLED de cara al operario
  pantalla.clear();
  pantalla.setFont(ArialMT_Plain_10);
  pantalla.drawString(0, 0,  "Estado: " + estadoAgua);
  pantalla.drawString(0, 12, "Calidad: " + String(porcentaje, 1) + " %");
  pantalla.drawString(0, 24, "V. Entrada: " + String(volt_pin, 2) + " V");
  pantalla.drawString(0, 36, "Val. NTU: " + String(ntu, 1));
  
  // Barra de progreso visual para el buffer
  String bufferStr = "Buffer [";
  for(int i=0; i<MAX_BUFFER; i++) {
    if(i < current_buffer) bufferStr += "#";
    else bufferStr += ".";
  }
  bufferStr += "]";
  pantalla.drawString(0, 48, bufferStr);
  pantalla.display();

  // ---------------------------------------------------------
  // 4. PROTOCOLO DE TRANSMISIÓN RF (Al llenar el buffer)
  // ---------------------------------------------------------
  if (current_buffer >= MAX_BUFFER) {
    Serial.println("\n[SISTEMA] Buffer saturado. Iniciando empaquetado LoRa...");
    
    int prom_raw = sum_raw / MAX_BUFFER;
    float prom_ntu = sum_ntu / MAX_BUFFER;
    float tendencia = prom_ntu - ema_ntu; 

    // Estructura de trama separada por comas (CSV ligero)
    String payload = String(NODE_ID) + "," +
                     String(prom_ntu, 1) + "," +
                     String(prom_raw) + "," +
                     String(tendencia, 2);

    Serial.println("[TX] Trama original: " + payload);

    // Cifrado de datos en bloque (AES-128 ECB)
    unsigned char plain[32] = {0};
    payload.toCharArray((char*)plain, 32);
    unsigned char enc[32]; 
    
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, aes_key, 128); 
    mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, plain, enc); 
    mbedtls_aes_free(&aes);

    // Ejecución de la transmisión física
    int txState = lora->transmit(enc, 32);

    if (txState == RADIOLIB_ERR_NONE) {
      Serial.println("[TX] Estatus: COMPLETADO. Paquete inyectado en red.\n");
      pantalla.drawString(100, 36, "TX OK"); // Indicador visual rápido de éxito
    } else {
      Serial.printf("[TX] Estatus: FALLO. Código de error: %d\n\n", txState);
      pantalla.drawString(90, 36, "TX ERR");
    }
    pantalla.display();

    // Vaciado de variables para el siguiente bloque de adquisición
    current_buffer = 0;
    sum_raw = 0;
    sum_ntu = 0.0;
  }

  // Intervalo de retención activo. El uso de delay() mantiene el consumo
  // de corriente en el umbral necesario para evitar que la Powerbank apague
  // el suministro energético del sistema.
  delay(5000); 
}