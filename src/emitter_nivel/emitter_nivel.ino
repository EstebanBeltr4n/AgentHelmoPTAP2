// ===== Emisor NIVEL (ID = 2) =====
/*
feCHA: 18 DE MARZO DE 2026
Agente sensor de nivel agua calibrado HELMO
*/
#define NODE_ID 2
#define NODE_TYPE "NIVEL"

#include <RadioLib.h>
#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include "mbedtls/aes.h"

const unsigned char aes_key[16] = {
  'E', 's', 't', 'e', 'b', 'a', 'n', 'L', 'o', 'R', 'a', '2', '0', '2', '6', '!'
};

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

SSD1306Wire pantalla(0x3c, 500000, SDA_OLED, SCL_OLED,
                     GEOMETRY_128_64, RST_OLED);
SX1262* lora;

float ema = 0.0;  // altura en cm
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
  pantalla.drawString(0, 0, "Nodo NIVEL ID 2");
  pantalla.display();

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  Module* radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY, SPI);
  lora = new SX1262(radio);

  // ✅ BIEN (todos usan 0x12)
  int state = lora->begin(915.0, 125.0, 7, 5, 0x12, 22, 8, 1.6);
 
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("LoRa NIVEL OK");
  } else {
    Serial.print("LoRa error: ");
    Serial.println(state);
  }

  pinMode(TRIG_PIN, OUTPUT);
  digitalWrite(TRIG_PIN, LOW);
  pinMode(ECHO_PIN, INPUT_PULLDOWN);
}

void loop() {
  // Lectura ultrasónico
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(5);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(12);
  digitalWrite(TRIG_PIN, LOW);

  long dur = pulseIn(ECHO_PIN, HIGH, 50000);
  float dist_cm = 0;
  if (dur > 100 && dur < 45000) {
    dist_cm = dur * 0.034 / 2.0;
  }

  // Altura de agua (suponiendo 4 cm máximos, sensor arriba)
  float altura = 4.0 - dist_cm;  // puedes ajustar
  if (altura < 0) altura = 0;

  // Estado nivel
  String estadoNivel = "VACIO";
  if (dist_cm > 0 && dist_cm <= 4) estadoNivel = "NIVEL ALTO";
  else if (dist_cm <= 6.0) estadoNivel = "NIVEL MEDIO";
  else if (dist_cm <= 8.0) estadoNivel = "NIVEL BAJO";

  // Modelo local
  float val = altura;
  ema = 0.3 * val + 0.7 * ema;

  hist[idxHist] = val;
  idxHist = (idxHist + 1) % 10;
  if (nHist < 10) nHist++;

  if (nHist >= 3) {
    float v0 = hist[(idxHist + 9) % 10];
    float v1 = hist[(idxHist + 8) % 10];
    float v2 = hist[(idxHist + 7) % 10];
    float avg = (v0 + v1 + v2) / 3.0;
    tendencia = avg - ema;
  }

  String payload = String(NODE_ID) + "," + String(val, 2) + "," + String(tendencia, 3);

  unsigned char plain[16] = { 0 };
  payload.getBytes(plain, 16);

  unsigned char enc[16];
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, aes_key, 128);
  mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, plain, enc);
  mbedtls_aes_free(&aes);

  int txState = lora->transmit(enc, 16);
  Serial.print("TX NIVEL: ");
  Serial.println(payload);
  Serial.print("TX Estado: ");
  Serial.println(txState);

  // OLED
  pantalla.clear();
  pantalla.setFont(ArialMT_Plain_10);
  pantalla.drawString(0, 0, "Dist:" + String(dist_cm, 1) + "cm");
  pantalla.drawString(0, 12, "Altura:" + String(altura, 2) + "cm");
  pantalla.drawString(0, 24, estadoNivel);
  pantalla.drawString(0, 36, "EMA:" + String(ema, 2));
  pantalla.drawString(0, 48, "Trend:" + String(tendencia, 3));
  pantalla.display();

  delay(4000);
}
