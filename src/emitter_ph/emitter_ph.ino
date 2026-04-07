/* ============================================================================
   PROYECTO: SISTEMA MULTI-AGENTE DE MONITOREO HÍDRICO HELMO (PTAP)
   NODO: AGENTE EMISOR DE pH (ID: 2)
   ============================================================================
   Autor: Esteban Eduardo Escarraga
   Propósito: Captura pH, aplica filtrado, cifra con AES-128 y transmite por LoRa
   Hardware: Heltec WiFi LoRa 32 V3 (ESP32-S3 + SX1262) + PH-4502C
   Fecha: 07 de abril de 2026
   DESCRIPTOR:
   Este nodo emisor se encarga exclusivamente de adquirir la señal analógica del
   sensor PH-4502C, convertirla a voltaje, estimar el valor de pH mediante una
   ecuación de calibración ajustable, suavizar la medida con un filtro EMA y
   transmitir el dato al nodo central mediante LoRa a 915 MHz con cifrado AES-128.
   ============================================================================
*/

#include <RadioLib.h>           // Librería para control del transceptor LoRa SX1262
#include <Wire.h>               // Librería para comunicación I2C (pantalla OLED)
#include "HT_SSD1306Wire.h"     // Librería OLED propia del ecosistema Heltec
#include <mbedtls/aes.h>        // Librería AES para cifrado simétrico
#include <SPI.h>                // Librería SPI para comunicación con el módulo LoRa

/* ============================================================================
   CONFIGURACIÓN GENERAL DEL NODO
   ============================================================================ */

// Identificador único del agente emisor de pH dentro del sistema multiagente
#define NODE_ID 2

// Número de muestras lógicas antes de forzar una transmisión al nodo central
#define PACKET_SIZE 10

// Pin ADC seleccionado para leer la salida analógica del módulo PH-4502C
// GPIO2 es utilizable como entrada analógica en ESP32-S3, pero debe validarse
// físicamente en tu cableado real sobre la Heltec V3
#define PH_PIN 2

/* ============================================================================
   CONFIGURACIÓN DE PANTALLA OLED EN HELTEC V3
   ============================================================================ */

// Pines I2C de la pantalla integrada en la Heltec WiFi LoRa 32 V3
#define SDA_OLED 17
#define SCL_OLED 18
#define RST_OLED 21

// Pin de habilitación de alimentación auxiliar en Heltec V3
// En esta placa, VEXT se activa llevando el pin a nivel LOW
#define VEXT_PIN 36

/* ============================================================================
   CONFIGURACIÓN DEL MÓDULO LoRa SX1262 EN HELTEC V3
   ============================================================================ */

// Pines SPI y de control del transceptor SX1262 de la Heltec V3
#define LORA_CS   8
#define LORA_SCK  9
#define LORA_MOSI 10
#define LORA_MISO 11
#define LORA_RST  12
#define LORA_BUSY 13
#define LORA_DIO1 14

/* ============================================================================
   PARÁMETROS DE CALIBRACIÓN DEL SENSOR DE pH
   ============================================================================ */

// Resolución ADC del ESP32-S3: 12 bits -> rango 0 a 4095
static const float ADC_RESOLUTION = 4095.0f;

// Voltaje de referencia asumido en el ADC del Heltec
// En ESP32 el valor real puede variar ligeramente entre placas, por eso se
// recomienda luego contrastarlo con multímetro y buffers reales
static const float ADC_VREF = 3.30f;

// Número de lecturas crudas para promediar por cada adquisición
// Promediar reduce ruido de alta frecuencia y fluctuaciones instantáneas
static const int ADC_SAMPLES = 20;

// Factor del filtro EMA (Exponential Moving Average)
// Valores bajos suavizan más; valores altos responden más rápido
static const float EMA_ALPHA = 0.30f;

// Parámetro de calibración central del PH-4502C
// Varias referencias prácticas con PH-4502C en ESP32 usan 2.50 V como referencia
// cercana a pH 7.00 cuando el módulo está ajustado correctamente
static const float PH7_VOLTAGE = 2.50f;

// Pendiente aproximada de conversión del electrodo/módulo
// Modelo práctico ampliamente usado: pH = 7 + ((2.50 - V) / 0.18)
// Este valor debe ajustarse posteriormente con soluciones buffer reales
static const float PH_SLOPE = 0.18f;

// Límites físicos razonables del pH en la aplicación para evitar valores absurdos
static const float PH_MIN = 0.0f;
static const float PH_MAX = 14.0f;

/* ============================================================================
   OBJETOS GLOBALES
   ============================================================================ */

// Objeto de control de la pantalla OLED integrada
SSD1306Wire oled(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);

// Puntero al módulo LoRa, usado así para construir el objeto de forma explícita
SX1262* lora = nullptr;

/* ============================================================================
   VARIABLES DE PROCESAMIENTO
   ============================================================================ */

// Buffer local de valores de pH adquiridos
float buffer_ph[PACKET_SIZE];

// Índice del buffer circular/simple de envío
int buffer_idx = 0;

// Variable del filtro EMA inicializada en neutro
float ema = 7.0f;

/* ============================================================================
   CLAVE AES-128
   ============================================================================ */

// Clave simétrica de 16 bytes para AES-128
// Debe coincidir exactamente con la usada por el nodo central receptor
const unsigned char aes_key[16] = {
  'E','s','t','e','b','a','n','L','o','R','a','2','0','2','6','!'
};

/* ============================================================================
   FUNCIÓN: INICIALIZAR PANTALLA
   ============================================================================ */

void initDisplay() {
  // Configura el pin VEXT como salida digital
  pinMode(VEXT_PIN, OUTPUT);

  // Activa la alimentación externa de la Heltec V3
  digitalWrite(VEXT_PIN, LOW);

  // Espera breve para estabilización de alimentación
  delay(100);

  // Inicializa la pantalla OLED
  oled.init();

  // Ajusta orientación de la pantalla si el montaje lo requiere
  oled.flipScreenVertically();

  // Selecciona una fuente base para textos pequeños
  oled.setFont(ArialMT_Plain_10);
}

/* ============================================================================
   FUNCIÓN: INICIALIZAR LoRa
   ============================================================================ */

void initLoRa() {
  // Mensaje de diagnóstico por puerto serie
  Serial.print(F("[LoRa] Iniciando... "));

  // Inicializa el bus SPI con los pines físicos de la Heltec V3
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);

  // Crea el módulo de RadioLib con la topología de la Heltec
  Module* radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY, SPI);

  // Construye el objeto SX1262 sobre el módulo recién creado
  lora = new SX1262(radio);

  // Inicializa el transceptor con parámetros sincronizados con la red HELMO:
  // Frecuencia = 915.0 MHz
  // Ancho de banda = 125 kHz
  // Spreading Factor = 7
  // Coding Rate = 5
  // Sync Word = 0x12 (red privada)
  // Potencia TX = 22 dBm (RadioLib intentará aplicar el máximo admitido)
  // Preamble = 8 símbolos
  // TCXO = 1.6 V (usado por muchas configuraciones SX1262 en Heltec)
  int state = lora->begin(915.0, 125.0, 7, 5, 0x12, 22, 8, 1.6);

  // Verifica si la inicialización fue correcta
  if (state == RADIOLIB_ERR_NONE) {
    // Configura DIO2 para control automático del RF switch, si la placa lo requiere
    if (lora->setDio2AsRfSwitch(true) != RADIOLIB_ERR_NONE) {
      Serial.println(F("Error en RF Switch"));
    }
    Serial.println(F("¡Éxito!"));
  } else {
    // Reporta el código de error si falla el inicio del radio
    Serial.print(F("Fallo, código: "));
    Serial.println(state);

    // Bloquea la ejecución porque el nodo no puede operar sin radio
    while (true) {
      delay(1000);
    }
  }
}

/* ============================================================================
   FUNCIÓN: INICIALIZAR ADC
   ============================================================================ */

void initADC() {
  // Define resolución de lectura ADC en 12 bits
  analogReadResolution(12);

  // Ajusta atenuación del pin para permitir lectura estable en rango ampliado
  // En ESP32 se recomienda configurar atenuación explícitamente
  analogSetPinAttenuation(PH_PIN, ADC_11db);

  // Da una pequeña espera para estabilización del frente analógico
  delay(50);
}

/* ============================================================================
   FUNCIÓN: LEER VOLTAJE PROMEDIO DEL SENSOR
   ============================================================================ */

float readPHVoltage() {
  // Acumulador para promedio de muestras ADC
  long sum = 0;

  // Toma múltiples muestras para reducir el ruido
  for (int i = 0; i < ADC_SAMPLES; i++) {
    sum += analogRead(PH_PIN);
    delay(10);
  }

  // Calcula el valor ADC promedio
  float avg_adc = sum / (float)ADC_SAMPLES;

  // Convierte el promedio ADC a voltaje
  float voltage = avg_adc * (ADC_VREF / ADC_RESOLUTION);

  // Retorna el voltaje medido en la entrada analógica
  return voltage;
}

/* ============================================================================
   FUNCIÓN: CONVERTIR VOLTAJE A pH
   ============================================================================ */

float voltageToPH(float voltage) {
  // Ecuación práctica de referencia para PH-4502C en ESP32:
  // pH = 7 + ((2.50 - V) / 0.18)
  // Donde 2.50 V corresponde aproximadamente al punto neutro pH 7.00
  float ph = 7.0f + ((PH7_VOLTAGE - voltage) / PH_SLOPE);

  // Limita la salida al rango físico razonable del pH
  if (ph < PH_MIN) ph = PH_MIN;
  if (ph > PH_MAX) ph = PH_MAX;

  // Retorna el pH calculado
  return ph;
}

/* ============================================================================
   FUNCIÓN: ADQUISICIÓN COMPLETA DE pH
   ============================================================================ */

float readPH() {
  // Lee el voltaje promedio del módulo de pH
  float voltage = readPHVoltage();

  // Convierte el voltaje a valor de pH
  float ph = voltageToPH(voltage);

  // Retorna el valor calculado
  return ph;
}

/* ============================================================================
   FUNCIÓN: MOSTRAR DATOS EN OLED
   ============================================================================ */

void printDisplay(float ph, float voltage) {
  // Limpia el búfer de la pantalla
  oled.clear();

  // Título del sistema
  oled.setFont(ArialMT_Plain_10);
  oled.drawString(0, 0, "HELMO PTAP - AGENTE pH");

  // Línea separadora
  oled.drawHorizontalLine(0, 12, 128);

  // Valor principal de pH en fuente grande
  oled.setFont(ArialMT_Plain_16);
  oled.drawString(0, 18, "pH: " + String(ph, 2));

  // Voltaje medido del módulo PH-4502C
  oled.setFont(ArialMT_Plain_10);
  oled.drawString(0, 42, "V: " + String(voltage, 3) + " V");

  // Estado del buffer de transmisión
  oled.drawString(0, 54, "Buffer: " + String(buffer_idx + 1) + "/" + String(PACKET_SIZE));

  // Envía el contenido al display
  oled.display();
}

/* ============================================================================
   FUNCIÓN: CIFRAR BLOQUE AES-128
   ============================================================================ */

void encryptAES128(const char* plainText, unsigned char* cipherText) {
  // Contexto de AES provisto por mbedTLS
  mbedtls_aes_context aes;

  // Inicializa el contexto AES
  mbedtls_aes_init(&aes);

  // Configura la clave de cifrado de 128 bits
  mbedtls_aes_setkey_enc(&aes, aes_key, 128);

  // Cifra exactamente 16 bytes en modo ECB
  mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT,
                        (const unsigned char*)plainText, cipherText);

  // Libera recursos del contexto AES
  mbedtls_aes_free(&aes);
}

/* ============================================================================
   SETUP
   ============================================================================ */

void setup() {
  // Inicializa el puerto serie para depuración
  Serial.begin(115200);

  // Pequeña espera de arranque
  delay(500);

  // Inicializa pantalla OLED
  initDisplay();

  // Inicializa conversor ADC
  initADC();

  // Inicializa el radio LoRa
  initLoRa();

  // Mensaje inicial en pantalla
  oled.clear();
  oled.drawString(0, 0, "Sistema Listo");
  oled.drawString(0, 14, "Nodo emisor pH");
  oled.display();

  // Espera breve antes de entrar al lazo principal
  delay(1500);
}

/* ============================================================================
   LOOP PRINCIPAL
   ============================================================================ */

void loop() {
  // Lee el voltaje actual proveniente del módulo de pH
  float ph_voltage = readPHVoltage();

  // Convierte el voltaje a valor de pH
  float ph_raw = voltageToPH(ph_voltage);

  // Aplica filtrado EMA para suavizar la señal y obtener tendencia local
  ema = (EMA_ALPHA * ph_raw) + ((1.0f - EMA_ALPHA) * ema);

  // Muestra el valor instantáneo en la pantalla OLED
  printDisplay(ph_raw, ph_voltage);

  // Guarda la muestra actual en el buffer local
  buffer_ph[buffer_idx] = ph_raw;

  // Incrementa el índice del buffer
  buffer_idx++;

  // Traza de diagnóstico por serial
  Serial.printf("Voltaje: %.3f V | pH: %.2f | EMA: %.2f | Buffer: %d/%d\n",
                ph_voltage, ph_raw, ema, buffer_idx, PACKET_SIZE);

  // Si el buffer alcanza el tamaño programado, se transmite al nodo central
  if (buffer_idx >= PACKET_SIZE) {
    // Informa el inicio de la fase de transmisión
    Serial.println("Buffer lleno. Iniciando protocolo de transmisión...");

    // Bloque de 16 bytes para AES-128
    char txPacket[16];
    memset(txPacket, 0, sizeof(txPacket));

    // Formato compacto CSV:
    // ID,pH,EMA
    // Se mantiene corto para no exceder el bloque fijo de 16 bytes
    snprintf(txPacket, sizeof(txPacket), "%d,%.2f,%.2f", NODE_ID, ph_raw, ema);

    // Imprime el contenido plano antes del cifrado
    Serial.print("TX pH (Plano): ");
    Serial.println(txPacket);

    // Buffer para el texto cifrado
    unsigned char enc[16];

    // Cifra el bloque en AES-128
    encryptAES128(txPacket, enc);

    // Transmite el paquete cifrado por LoRa
    int txState = lora->transmit(enc, 16);

    // Evalúa resultado de la transmisión
    if (txState == RADIOLIB_ERR_NONE) {
      Serial.println("TX Estado: OK (Paquete entregado a la red)");
    } else {
      Serial.printf("TX Estado error: %d\n", txState);
    }

    // Reinicia el índice del buffer para comenzar un nuevo ciclo
    buffer_idx = 0;

    // Espera antes de la siguiente muestra
    delay(5000);
  } else {
    // Si el buffer aún no se llena, solo espera hasta la próxima adquisición
    delay(5000);
  }
}