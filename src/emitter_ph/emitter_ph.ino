/* ============================================================================
 PROYECTO: SISTEMA MULTI-AGENTE DE MONITOREO HÍDRICO HELMO (PTAP)
 NODO: AGENTE EMISOR DE PH (ID: 2)
 ============================================================================
 Autor: Esteban Eduardo Escarraga Tuquerres
 Propósito: Captura pH, Cifrado AES-128, LoRa 915MHz, Deep Sleep
 Hardware: Heltec LoRa32 V3 + PH-4502C
 Fecha 31 de marzo de 2026
 DESCRIPTOR: 
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
SX1262 lora = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);

float buffer_ph[PACKET_SIZE];
int buffer_idx = 0;
float ema = 7.0;

// Clave AES (16 bytes)
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
  // Configuración de pines SPI para el SX1262 interno
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  
  // begin(frecuencia) - 915.0 MHz para nuestra región
  int state = lora.begin(915.0);

  if (state == RADIOLIB_ERR_NONE) {
    // Configuración vital para que el chip LoRa de la Heltec V3 funcione:
    // DIO2 se usa como switch de antena interno
    if (lora.setDio2AsRfSwitch(true) != RADIOLIB_ERR_NONE) {
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
  // Ecuación de calibración (ajustar según pruebas reales)
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
    Serial.println("Buffer lleno. Transmitiendo...");
    // Aquí iría tu lógica de transmitPacket() y Deep Sleep
    buffer_idx = 0; 
    delay(5000); 
  } else {
    delay(5000); // Muestreo cada 5 segundos para pruebas
  }
}