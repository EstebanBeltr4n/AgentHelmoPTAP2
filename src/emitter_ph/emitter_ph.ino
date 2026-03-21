// ===== Emisor PH (ID = 1) =====
#define NODE_ID 1
#define NODE_TYPE "PH"

#include <RadioLib.h>
#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include "mbedtls/aes.h"

const unsigned char aes_key[16] = {
  'E','s','t','e','b','a','n','L','o','R','a','2','0','2','6','!'
};

#define SDA_OLED 17
#define SCL_OLED 18
#define RST_OLED 21
#define VEXT_PIN 36

#define LORA_CS   8
#define LORA_SCK  9
#define LORA_MOSI 10
#define LORA_MISO 11
#define LORA_RST  12
#define LORA_BUSY 13
#define LORA_DIO1 14

// PH-4502C: salida analógica PO -> pin ADC
#define SENSOR_PH_PIN 2

SSD1306Wire pantalla(0x3c, 500000, SDA_OLED, SCL_OLED,
                     GEOMETRY_128_64, RST_OLED);
SX1262* lora;

float ema = 7.0;
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
  pantalla.drawString(0, 0, "Nodo PH ID 1");
  pantalla.display();

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  Module* radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY, SPI);
  lora = new SX1262(radio);

  // ✅ BIEN (todos usan 0x12)
  int state = lora->begin(915.0, 125.0, 7, 5, 0x12, 22, 8, 1.6);
  
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("LoRa PH OK");
  } else {
    Serial.print("LoRa error: "); Serial.println(state);
  }

  pinMode(SENSOR_PH_PIN, INPUT);
}

void loop() {
  int raw = analogRead(SENSOR_PH_PIN);
  float volt = raw * 3.3 / 4095.0;

  // Aproximación, deberás calibrar
  float ph = 7.0 + ((2.5 - volt) / 0.18);

  float val = ph;
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

  String payload = String(NODE_ID) + "," +
                   String(val, 2) + "," +
                   String(tendencia, 3);

  unsigned char plain[16] = {0};
  payload.getBytes(plain, 16);

  unsigned char enc[16];
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, aes_key, 128);
  mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, plain, enc);
  mbedtls_aes_free(&aes);

  int txState = lora->transmit(enc, 16);
  Serial.print("TX PH: "); Serial.println(payload);
  Serial.print("TX Estado: "); Serial.println(txState);

  pantalla.clear();
  pantalla.setFont(ArialMT_Plain_10);
  pantalla.drawString(0, 0, "pH:" + String(ph, 2) +
                           " V:" + String(volt, 2));
  pantalla.drawString(0, 14, "EMA:" + String(ema, 2));
  pantalla.drawString(0, 28, "Trend:" + String(tendencia, 3));
  pantalla.display();

  delay(4000);
}
