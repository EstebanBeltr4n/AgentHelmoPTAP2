/*
  Heltec WiFi LoRa 32 V3 - Servidor BLE + OLED + Sensor de Turbidez
  Envía por BLE el valor analógico y el voltaje del sensor.

  Placa: Heltec WiFi LoRa 32 V3 (ESP32-S3)
  OLED:
    SDA = GPIO17
    SCL = GPIO18
    RST = GPIO21

  Sensor de turbidez:
    AO  = GPIO1   <-- puedes cambiar este pin si lo necesitas
*/

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ===========================
// OLED Heltec V3
// ===========================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_SDA 17
#define OLED_SCL 18
#define OLED_RST 21

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);

// ===========================
// Sensor de turbidez
// ===========================
#define TURBIDITY_PIN 1     // Cambia este pin si deseas usar otro ADC
#define ADC_MAX 4095.0
#define VREF 3.3

// ===========================
// BLE UUIDs
// ===========================
#define SERVICE_UUID        "6a5707e0-a337-43aa-b344-70bc1ec6da44"
#define CHARACTERISTIC_UUID "9c666635-7ea8-4f38-8e63-61dec23ccb77"

BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;

bool deviceConnected = false;
bool oldDeviceConnected = false;

// ===========================
// Callbacks BLE
// ===========================
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
  }

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
  }
};

// ===========================
// Función para mostrar datos
// ===========================
void showDisplay(int adcValue, float voltage, const String& bleState) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println("Heltec V3 BLE Server");

  display.setCursor(0, 14);
  display.print("ADC: ");
  display.println(adcValue);

  display.setCursor(0, 28);
  display.print("Voltaje: ");
  display.print(voltage, 2);
  display.println(" V");

  display.setCursor(0, 42);
  display.print("BLE: ");
  display.println(bleState);

  display.display();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // ===========================
  // Inicializar OLED
  // ===========================
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW);
  delay(50);
  digitalWrite(OLED_RST, HIGH);

  Wire.begin(OLED_SDA, OLED_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("No se encontro la OLED");
    while (true) {
      delay(1000);
    }
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Iniciando...");
  display.display();

  // ===========================
  // Config ADC
  // ===========================
  analogReadResolution(12);

  // ===========================
  // Inicializar BLE
  // ===========================
  BLEDevice::init("Heltec_Turbidez_V3");

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService* pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );

  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setValue("Esperando datos...");
  pService->start();

  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->start();

  Serial.println("Servidor BLE iniciado");
  showDisplay(0, 0.0, "Esperando");
}

void loop() {
  int adcValue = analogRead(TURBIDITY_PIN);
  float voltage = (adcValue / ADC_MAX) * VREF;

  String data = "{";
  data += "\"adc\":";
  data += String(adcValue);
  data += ",\"voltaje\":";
  data += String(voltage, 2);
  data += "}";

  pCharacteristic->setValue(data.c_str());

  if (deviceConnected) {
    pCharacteristic->notify();
    Serial.println("Enviado BLE: " + data);
    showDisplay(adcValue, voltage, "Conectado");
  } else {
    Serial.println("Esperando cliente...");
    showDisplay(adcValue, voltage, "Sin cliente");
  }

  // Reiniciar advertising al desconectarse
  if (!deviceConnected && oldDeviceConnected) {
    delay(500);
    pServer->startAdvertising();
    Serial.println("Cliente desconectado, advertising reiniciado");
    oldDeviceConnected = deviceConnected;
  }

  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }

  delay(1000);
}