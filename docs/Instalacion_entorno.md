# Instalación y Configuración del Entorno de Desarrollo

Para compilar y cargar el firmware a los diferentes agentes del proyecto HELMO, es necesario configurar el entorno en el Arduino IDE.

## 1. Requisitos Previos
* **Software:** Arduino IDE 2.x o superior.
* **Hardware:** Placas Heltec WiFi LoRa 32 V3 y cables USB-C de transferencia de datos.

## 2. Instalación de Tarjetas (Board Manager)
1. Abrir Arduino IDE -> `File` -> `Preferences`.
2. En *Additional Boards Manager URLs*, agregar: 
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
3. Ir a `Tools` -> `Board` -> `Boards Manager`, buscar **esp32** e instalar la versión de Espressif Systems.
4. Seleccionar la placa: `Heltec WiFi LoRa 32(V3) / Wireless shell(V3) / ...`

## 3. Instalación de Librerías Dependientes
Ir a `Sketch` -> `Include Library` -> `Manage Libraries` e instalar:
* **RadioLib:** Para el control y configuración del transceptor SX1262.
* **Heltec ESP32 Dev-Boards:** Incluye la librería `HT_SSD1306Wire.h` para el control de la pantalla OLED integrada.

*Nota: La librería criptográfica `mbedtls/aes.h` y `esp_sleep.h` ya vienen incluidas de forma nativa en el núcleo de ESP32.*

## 4. Parámetros Críticos de Red LoRa
Asegúrese de que todos los nodos compartan estrictamente la siguiente configuración en la inicialización de `RadioLib` para evitar colisiones o aislamientos en la red:

* **Frecuencia:** 915.0 MHz (Banda ISM Américas)
* **Ancho de Banda (BW):** 125.0 kHz
* **Spreading Factor (SF):** 7
* **Coding Rate (CR):** 5
* **SyncWord:** 0x12 (Red Privada)
* **Potencia de Salida:** 22 dBm

## 5. Carga de Código
1. Abra el archivo `.ino` correspondiente al agente deseado.
2. Asegúrese de ajustar los parámetros de calibración si los sensores han sido cambiados.
3. Conecte la placa, seleccione el puerto COM adecuado y haga clic en **Upload**.