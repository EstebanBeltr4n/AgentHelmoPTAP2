/*
  Heltec WiFi LoRa 32 V3 - Servidor BLE + Sensor de Turbidez (salida digital)
  Envía por BLE el estado digital del módulo de turbidez.
  La pantalla OLED se apaga para ahorrar batería.

  Placa: Heltec WiFi LoRa 32 V3 (ESP32-S3)

  OLED:
    SDA = GPIO17
    SCL = GPIO18
    RST = GPIO21

  Sensor de turbidez con módulo:
    G -> GND
    V -> 3.3V o 5V (según tu módulo)
    D -> GPIO1   <-- pin digital usado en este ejemplo
    A -> no se usa
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
// Sensor de turbidez (DIGITAL)
// ===========================
#define TURBIDITY_DIGITAL_PIN 1   //  pin GPIO digital

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
  void onConnect(BLEServer* pServer) override {
    deviceConnected = true;
  }

  void onDisconnect(BLEServer* pServer) override {
    deviceConnected = false;
  }
};

void setup() {
  Serial.begin(115200);
  delay(1000);

  // ===========================
  // Inicializar OLED y apagarla
  // ===========================
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW);
  delay(50);
  digitalWrite(OLED_RST, HIGH);

  Wire.begin(OLED_SDA, OLED_SCL);

  if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    display.clearDisplay();
    display.display();
    display.ssd1306_command(SSD1306_DISPLAYOFF);  // apagar OLED
    Serial.println("OLED apagada para ahorrar bateria");
  } else {
    Serial.println("No se encontro la OLED");
  }

  // ===========================
  // Configurar pin digital
  // ===========================
  pinMode(TURBIDITY_DIGITAL_PIN, INPUT);

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
  pCharacteristic->setValue("{\"digital\":-1,\"estado\":\"INICIO\"}");
  pService->start();

  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->start();

  Serial.println("Servidor BLE iniciado");
  Serial.println("Leyendo sensor por salida digital...");
}

void loop() {
  int digitalValue = digitalRead(TURBIDITY_DIGITAL_PIN);

  
  // 0 = superó umbral
  // 1 = no superó umbral
  String estado = (digitalValue == 0) ? "TURBIA" : "CLARA";

  String data = "{";
  data += "\"digital\":";
  data += String(digitalValue);
  data += ",\"estado\":\"";
  data += estado;
  data += "\"}";

  pCharacteristic->setValue(data.c_str());

  if (deviceConnected) {
    pCharacteristic->notify();
    Serial.println("Enviado BLE: " + data);
  } else {
    Serial.println("Esperando cliente...");
    Serial.println("Lectura local: " + data);
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