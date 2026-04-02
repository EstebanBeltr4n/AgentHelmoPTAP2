/* =================================================================
   PROYECTO: SISTEMA MULTI-AGENTE DE MONITOREO HÍDRICO HELMO (PTAP)
   NODO: AGENTE EMISOR DE NIVEL (ID: 1)
   =================================================================
   AUTOR: Esteban Eduardo Escarraga 
   FECHA: 31 de marzo de 2026
   HARDWARE: Heltec WiFi LoRa 32 V3 (ESP32-S3) + Sensor Ultrasonido HC-SR04
   ================================================================= */

#define NODE_ID 1           
#define NODE_TYPE "NIVEL"    

#include <RadioLib.h>       
#include <Wire.h>           
#include "HT_SSD1306Wire.h" 
#include "mbedtls/aes.h"    

// --- LLAVE SIMÉTRICA AES-128 ---
const unsigned char aes_key[16] = {
  'E', 's', 't', 'e', 'b', 'a', 'n', 'L', 'o', 'R', 'a', '2', '0', '2', '6', '!'
};

// --- DEFINICIÓN DE PINES HELTEC V3 ---
#define SDA_OLED 17
#define SCL_OLED 18
#define RST_OLED 21
#define VEXT_PIN 36 

#define LORA_CS 8
#define LORA_SCK 9
#define LORA_MOSI 10
#define LORA_MISO 11
#define LORA_RST 12
#define LORA_BUSY 13
#define LORA_DIO1 14

#define TRIG_PIN 4  
#define ECHO_PIN 5  

SSD1306Wire pantalla(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);
SX1262* lora;

// --- VARIABLES DE PROCESAMIENTO ---
float ema = 0.0;        
float tendencia = 0.0;  
float hist[10];         
int idxHist = 0;        
int nHist = 0;          

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, LOW); 
  delay(100);

  pantalla.init();
  pantalla.clear();
  pantalla.setFont(ArialMT_Plain_10);
  pantalla.drawString(0, 0, "Nodo NIVEL ID 1"); 
  pantalla.display();

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  Module* radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY, SPI);
  lora = new SX1262(radio);

  int state = lora->begin(915.0, 125.0, 7, 5, 0x12, 22, 8, 1.6);
  
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("LoRa NIVEL OK");
  } else {
    Serial.print("LoRa error: "); Serial.println(state);
    while (1); 
  }

  pinMode(TRIG_PIN, OUTPUT);
  digitalWrite(TRIG_PIN, LOW);
  pinMode(ECHO_PIN, INPUT_PULLDOWN); 
}

void loop() {
  // 1. DISPARO DEL SENSOR
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(5);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(12); 
  digitalWrite(TRIG_PIN, LOW);

  // 2. CÁLCULO DE DISTANCIA
  long dur = pulseIn(ECHO_PIN, HIGH, 50000); 
  float dist_cm = 0;
  if (dur > 100 && dur < 45000) {
    dist_cm = dur * 0.0343 / 2.0; 
  }

  // 3. CALIBRACIÓN DE ALTURA REAL (Referencia: Sensor a 8cm del fondo)
  float altura_real = 8.0 - dist_cm; 
  if (altura_real < 0) altura_real = 0;

  // 4. CLASIFICACIÓN LOGÍSTICA
  String estadoNivel = "FUERA RANGO";
  if (dist_cm > 0 && dist_cm <= 4)       estadoNivel = "NIVEL ALTO";
  else if (dist_cm <= 6.0)               estadoNivel = "NIVEL MEDIO";
  else if (dist_cm <= 8.0)               estadoNivel = "NIVEL BAJO";
  else if (dist_cm > 8.0)                estadoNivel = "VACIO";

  // 5. PROCESAMIENTO ESTADÍSTICO
  ema = 0.3 * altura_real + 0.7 * ema; 
  hist[idxHist] = altura_real;
  idxHist = (idxHist + 1) % 10;
  if (nHist < 10) nHist++;

  if (nHist >= 3) {
    float v0 = hist[(idxHist + 9) % 10];
    float v1 = hist[(idxHist + 8) % 10];
    float v2 = hist[(idxHist + 7) % 10];
    float avg = (v0 + v1 + v2) / 3.0;
    tendencia = avg - ema; 
  }

  // 6. FORMATEO DE TRAMA CSV (Protocolo Multi-Agente)
  char txPacket[16]; // Tamaño exacto para un bloque AES
  memset(txPacket, 0, 16); // Limpiar buffer con ceros
  
  // Construcción de la trama: "1,dist,alt"
  snprintf(txPacket, sizeof(txPacket), "%d,%.1f,%.1f", NODE_ID, dist_cm, altura_real);

  // 7. CAPA DE CIFRADO AES-128 ECB
  unsigned char enc[16];
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, aes_key, 128);
  // Ciframos directamente el contenido de txPacket
  mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, (const unsigned char*)txPacket, enc);
  mbedtls_aes_free(&aes);

  // 8. TRANSMISIÓN INALÁMBRICA LORA
  Serial.print("TX NIVEL (Plano): "); Serial.println(txPacket);
  
  int txState = lora->transmit(enc, 16); // Transmitimos el bloque cifrado

  if (txState == RADIOLIB_ERR_NONE) {
    Serial.println("TX Estado: OK");
  } else {
    Serial.print("TX Estado error: "); Serial.println(txState);
  }

  // 9. ACTUALIZACIÓN DE HMI (OLED)
  pantalla.clear();
  pantalla.setFont(ArialMT_Plain_10);
  pantalla.drawString(0, 0, "Dist:" + String(dist_cm, 1) + "cm");
  pantalla.drawString(0, 12, "Altura:" + String(altura_real, 1) + "cm");
  pantalla.drawString(0, 24, "Estado:" + estadoNivel);
  pantalla.drawString(0, 36, "Tend:" + String(tendencia, 3));
  pantalla.display();

  delay(4000); 
}