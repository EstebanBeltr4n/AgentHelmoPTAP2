/* ============================================================================
 PROYECTO: SISTEMA MULTI-AGENTE DE MONITOREO HÍDRICO HELMO (PTAP)
 NODO: AGENTE EMISOR DE PH (ID: 2)
 ============================================================================
 Autor: Esteban Eduardo Escarraga 
 Propósito: Captura pH, Cifrado AES-128, LoRa 915MHz, Deep Sleep
 Hardware: Heltec LoRa32 V3 + PH-4502C
 Fecha: 31 de marzo de 2026
 DESCRIPTOR: Medición electroquímica de pH con filtrado EMA y cifrado simétrico.
 ============================================================================*/

#include <RadioLib.h>
#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include <mbedtls/aes.h>
#include <esp_sleep.h>
#include <SPI.h>

// ------------------- CONFIGURACIÓN HARDWARE V3 -------------------
#define NODE_ID 2
#define PACKET_SIZE 10
#define PH_PIN 2

// Pines específicos Heltec LoRa32 V3
#define SDA_OLED 17
#define SCL_OLED 18
#define RST_OLED 21
#define VEXT_PIN 36 // Control de energía (Pantalla/Sensores)

#define LORA_CS   8
#define LORA_SCK  9
#define LORA_MOSI 10
#define LORA_MISO 11
#define LORA_RST  12
#define LORA_BUSY 13
#define LORA_DIO1 14

// ------------------- OBJETOS Y VARIABLES -------------------
SSD1306Wire oled(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);

// Corrección para la instanciación segura de RadioLib (Punteros)
SX1262* lora;

float buffer_ph[PACKET_SIZE];
int buffer_idx = 0;
float ema = 7.0;

// Clave AES (16 bytes) - Debe coincidir exactamente con el Central
const unsigned char aes_key[16] = {'E','s','t','e','b','a','n','L','o','R','a','2','0','2','6','!'};

// ------------------- FUNCIONES DE APOYO -------------------

void initDisplay() {
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, LOW); // LOW activa la energía en la Heltec V3
  delay(100);
  oled.init();
  oled.flipScreenVertically();
  oled.setFont(ArialMT_Plain_10);
}

void initLoRa() {
  Serial.print(F("[LoRa] Iniciando... "));
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  
  Module* radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY, SPI);
  lora = new SX1262(radio);

  // CORRECCIÓN VITAL: Sincronización de parámetros con la red HELMO
  // Frecuencia: 915MHz, BW: 125kHz, SF: 7, CR: 5, SyncWord: 0x12 (Privada), Pwr: 22dBm
  int state = lora->begin(915.0, 125.0, 7, 5, 0x12, 22, 8, 1.6);

  if (state == RADIOLIB_ERR_NONE) {
    if (lora->setDio2AsRfSwitch(true) != RADIOLIB_ERR_NONE) {
      Serial.println(F("Error en RF Switch"));
    }
    Serial.println(F("¡Éxito!"));
  } else {
    Serial.print(F("Fallo, código: "));
    Serial.println(state);
    while (true);
  }
}

float readPH() {
  long sum = 0;
  for(int i = 0; i < 20; i++) {
    sum += analogRead(PH_PIN);
    delay(10);
  }
  float voltage = (sum / 20.0) * (3.3 / 4095.0);
  // Ecuación de calibración (ajustar según pruebas reales con soluciones buffer)
  return (-5.70 * voltage) + 21.34; 
}

void printDisplay(float ph) {
  oled.clear();
  oled.setFont(ArialMT_Plain_10);
  oled.drawString(0, 0, "MONITOREO AGUA V2.1");
  oled.drawHorizontalLine(0, 12, 128);
  
  oled.setFont(ArialMT_Plain_16);
  oled.drawString(0, 20, "pH: " + String(ph, 2));
  
  oled.setFont(ArialMT_Plain_10);
  oled.drawString(0, 45, "Buffer: " + String(buffer_idx + 1) + "/" + String(PACKET_SIZE));
  oled.display();
}

// ------------------- SETUP & LOOP -------------------

void setup() {
  Serial.begin(115200);
  initDisplay();
  initLoRa();
  
  oled.clear();
  oled.drawString(0, 0, "Sistema Listo");
  oled.display();
  delay(2000);
}

void loop() {
  float ph_raw = readPH();
  ema = 0.3 * ph_raw + 0.7 * ema;
  
  printDisplay(ph_raw);
  
  buffer_ph[buffer_idx] = ph_raw;
  buffer_idx++;

  Serial.printf("pH: %.2f | EMA: %.2f | Buffer: %d/%d\n", ph_raw, ema, buffer_idx, PACKET_SIZE);

  if (buffer_idx >= PACKET_SIZE) {
    Serial.println("Buffer lleno. Iniciando protocolo de transmisión...");
    
    // --- NUEVA LÓGICA DE TRANSMISIÓN IMPLEMENTADA ---
    
    char txPacket[16]; // Buffer exacto para AES-128
    memset(txPacket, 0, 16); 
    
    // Formateo CSV: ID, pH_actual, Tendencia_EMA
    snprintf(txPacket, sizeof(txPacket), "%d,%.2f,%.2f", NODE_ID, ph_raw, ema);
    Serial.print("TX pH (Plano): "); Serial.println(txPacket);

    // Cifrado AES-128 ECB
    unsigned char enc[16];
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, aes_key, 128);
    mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, (const unsigned char*)txPacket, enc);
    mbedtls_aes_free(&aes);

    // Transmisión LoRa
    int txState = lora->transmit(enc, 16);
    
    if (txState == RADIOLIB_ERR_NONE) {
      Serial.println("TX Estado: OK (Paquete entregado a la red)");
    } else {
      Serial.printf("TX Estado error: %d\n", txState);
    }

    buffer_idx = 0; 
    
    
    
    delay(5000); 
  } else {
    delay(5000); // Muestreo cada 5 segundos
  }
}