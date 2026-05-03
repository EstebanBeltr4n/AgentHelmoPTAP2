/*
  Heltec WiFi LoRa 32 V3 - Cliente BLE + OLED
  Busca al servidor "Heltec_Turbidez_V3", recibe el JSON
  {"digital":0,"estado":"TURBIA"} y lo muestra en OLED.

  OLED Heltec V3:
    SDA = GPIO17
    SCL = GPIO18
    RST = GPIO21
*/

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BLEDevice.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_SDA 17
#define OLED_SCL 18
#define OLED_RST 21

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);

static BLEUUID serviceUUID("6a5707e0-a337-43aa-b344-70bc1ec6da44");
static BLEUUID charUUID("9c666635-7ea8-4f38-8e63-61dec23ccb77");

static BLEAddress* pServerAddress = nullptr;
static BLERemoteCharacteristic* pRemoteCharacteristic = nullptr;
static BLEAdvertisedDevice* myDevice = nullptr;

bool doConnect = false;
bool connected = false;
bool doScan = false;

String lastData = "Sin datos";
int receivedDigital = -1;
String receivedState = "SIN DATO";

void showDisplay(const String& line1, const String& line2, const String& line3, const String& line4) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(line1);
  display.setCursor(0, 14);
  display.println(line2);
  display.setCursor(0, 28);
  display.println(line3);
  display.setCursor(0, 42);
  display.println(line4);
  display.display();
}

void parseData(String data) {
  int digitalIndex = data.indexOf("\"digital\":");
  int estadoIndex = data.indexOf("\"estado\":\"");

  if (digitalIndex != -1 && estadoIndex != -1) {
    int commaIndex = data.indexOf(",", digitalIndex);
    String digitalStr = data.substring(digitalIndex + 10, commaIndex);
    receivedDigital = digitalStr.toInt();

    int estadoStart = estadoIndex + 10;
    int estadoEnd = data.indexOf("\"", estadoStart);
    receivedState = data.substring(estadoStart, estadoEnd);
  }
}

static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {

  String data = "";
  for (size_t i = 0; i < length; i++) {
    data += (char)pData[i];
  }

  lastData = data;
  parseData(data);

  Serial.print("Notificacion recibida: ");
  Serial.println(lastData);

  showDisplay(
    "Cliente BLE V3",
    "Digital: " + String(receivedDigital),
    "Agua: " + receivedState,
    "Estado: Conectado"
  );
}

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    Serial.println("Conectado al servidor BLE");
  }

  void onDisconnect(BLEClient* pclient) {
    connected = false;
    Serial.println("Desconectado del servidor BLE");
    showDisplay("Cliente BLE V3", "Servidor perdido", "Reintentando...", "");
  }
};

bool connectToServer() {
  Serial.print("Conectando a: ");
  Serial.println(pServerAddress->toString().c_str());

  BLEClient* pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback());

  if (!pClient->connect(*pServerAddress)) {
    Serial.println("Fallo al conectar");
    return false;
  }

  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    Serial.println("No se encontro el servicio");
    pClient->disconnect();
    return false;
  }

  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
  if (pRemoteCharacteristic == nullptr) {
    Serial.println("No se encontro la caracteristica");
    pClient->disconnect();
    return false;
  }

  if (pRemoteCharacteristic->canRead()) {
    String value = pRemoteCharacteristic->readValue();
    lastData = value;
    parseData(lastData);
    Serial.print("Lectura inicial: ");
    Serial.println(lastData);
  }

  if (pRemoteCharacteristic->canNotify()) {
    pRemoteCharacteristic->registerForNotify(notifyCallback);
    Serial.println("Notificaciones activadas");
  }

  connected = true;
  return true;
}

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.haveName() && advertisedDevice.getName() == "Heltec_Turbidez_V3") {
      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      doScan = true;
    }
  }
};

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW);
  delay(50);
  digitalWrite(OLED_RST, HIGH);

  Wire.begin(OLED_SDA, OLED_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("No se encontro OLED");
    while (true) {
      delay(1000);
    }
  }

  showDisplay("Cliente BLE V3", "Iniciando...", "", "");

  BLEDevice::init("");

  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);

  showDisplay("Cliente BLE V3", "Escaneando...", "Buscando server", "");
}

void loop() {
  if (doConnect == true) {
    pServerAddress = new BLEAddress(myDevice->getAddress());

    if (connectToServer()) {
      Serial.println("Conexion BLE exitosa");
      showDisplay(
        "Cliente BLE V3",
        "Digital: " + String(receivedDigital),
        "Agua: " + receivedState,
        "Estado: OK"
      );
    } else {
      Serial.println("No se pudo conectar");
      showDisplay("Cliente BLE V3", "Conexion fallo", "Reescaneando...", "");
    }

    doConnect = false;
  }

  if (!connected && doScan) {
    BLEDevice::getScan()->start(5, false);
    delay(1000);
  }

  delay(1000);
}