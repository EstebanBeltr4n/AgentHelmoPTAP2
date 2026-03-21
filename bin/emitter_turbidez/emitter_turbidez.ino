// ===== Emisor TURBIDEZ (ID = 0) =====
// 18 de marco de 2026
/*
Agente EMisor sensor de turbidez  HELMO PTAP SMART fincional calibrado por 
salida analogica
*/
#define NODE_ID 0
#define NODE_TYPE "TURB"

#include <RadioLib.h>
#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include "mbedtls/aes.h"

// Clave AES 128 bits
const unsigned char aes_key[16] = {
  'E', 's', 't', 'e', 'b', 'a', 'n', 'L', 'o', 'R', 'a', '2', '0', '2', '6', '!'
};

// Pines Heltec V3
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


#define SENSOR_TURBIDEZ_PIN 1  // Sensor turbidez (salida ANALÓGICA AO del módulo)

// Opcional: salida DIGITAL DO si quieres solo umbral}
//#define SENSOR_TURBIDEZ_DO  3

SSD1306Wire pantalla(0x3c, 500000, SDA_OLED, SCL_OLED,
                     GEOMETRY_128_64, RST_OLED);
SX1262* lora;

// Modelo local
float ema = 0.0;
float tendencia = 0.0;
float hist[10];
int idxHist = 0;
int nHist = 0;

// === Valores de calibración (AJUSTA ESTO) ===
// Lee el valor RAW en Serial con agua muy limpia y muy turbia
const int RAW_LIMPIO = 1000;  //4000 casi sin turbidez
const int RAW_SUCIO = 800;    //800 agua muy turbia

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, LOW);
  delay(100);

  pantalla.init();
  pantalla.clear();
  pantalla.setFont(ArialMT_Plain_10);
  pantalla.drawString(0, 0, "Nodo TURB ID 0");
  pantalla.display();

  // LoRa
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  Module* radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY, SPI);
  lora = new SX1262(radio);

 
  // ✅ BIEN (todos usan 0x12)
  int state = lora->begin(915.0, 125.0, 7, 5, 0x12, 22, 8, 1.6);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("LoRa TURB OK");
  } else {
    Serial.print("LoRa error: ");
    Serial.println(state);
  }

  pinMode(SENSOR_TURBIDEZ_PIN, INPUT);  //PARA SALIDA ANALOGICA
  //pinMode(SENSOR_TURBIDEZ_DO, INPUT);  // si quieres usar la salida digital
}

void loop() {
  // 1) Medición analógica
  int raw = analogRead(SENSOR_TURBIDEZ_PIN);
  float volt = raw * 3.3 / 4095.0;

  // NTU relativo (0–1000) usando los valores calibrados
  int ntu = map(raw, RAW_LIMPIO, RAW_SUCIO, 0, 1000);
  ntu = constrain(ntu, 0, 1000);

  // Porcentaje de turbidez (0% limpia – 100% muy turbia)
  float turbPorc = (ntu / 1000.0) * 100.0;

  // Clasificación cualitativa
  String estadoAgua;
  if (turbPorc < 30) estadoAgua = "AGUA LIMPIA";
  else if (turbPorc < 70) estadoAgua = "TURBIDEZ MEDIA";
  else estadoAgua = "TURBIDEZ ALTA";

  // 2) Modelo local EMA + tendencia sobre NTU
  float val = ntu;
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

  // 3) Empaquetar: "ID,valor,tendencia" (usa NTU)
  String payload = String(NODE_ID) + "," + String(val, 1) + "," + String(tendencia, 2);

  unsigned char plain[16] = { 0 };
  payload.getBytes(plain, 16);

  unsigned char enc[16];
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, aes_key, 128);
  mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, plain, enc);
  mbedtls_aes_free(&aes);

  int txState = lora->transmit(enc, 16);

  // 4) Serial – información completa
  Serial.println("----------- TURBIDEZ -----------");
  Serial.print("RAW: ");
  Serial.println(raw);
  Serial.print("Volt: ");
  Serial.println(volt, 3);
  Serial.print("NTU aprox: ");
  Serial.println(ntu);
  Serial.print("Turbidez %: ");
  Serial.print(turbPorc, 1);
  Serial.println("%");
  Serial.print("Estado: ");
  Serial.println(estadoAgua);
  Serial.print("EMA: ");
  Serial.println(ema, 1);
  Serial.print("Tendencia: ");
  Serial.println(tendencia, 2);
  Serial.print("TX payload: ");
  Serial.println(payload);
  Serial.print("TX estado LoRa: ");
  Serial.println(txState);

  // 5) OLED – porcentaje + estado
  pantalla.clear();
  pantalla.setFont(ArialMT_Plain_10);
  pantalla.drawString(0, 0,
                      "RAW:" + String(raw) + " V:" + String(volt, 2));
  pantalla.drawString(0, 14,
                      "NTU:" + String(ntu) + " (" + String(turbPorc, 0) + "%)");
  pantalla.drawString(0, 28,
                      estadoAgua);  // LIMPIA / MEDIA / ALTA
  pantalla.drawString(0, 42,
                      "EMA:" + String(ema, 1) + " Tr:" + String(tendencia, 2));
  pantalla.display();

  delay(4000);
}
