# Dependencias y Librerías del Proyecto HELMO

Para garantizar la compilación exitosa y el correcto funcionamiento del ecosistema HELMO (tanto en sus agentes emisores como en el nodo central), el entorno de desarrollo debe contar con las siguientes librerías de software.

## Librerías de Terceros (Requeridas)

* **[RadioLib](https://github.com/jgromes/RadioLib) (v6.0 o superior):** * **Uso:** Modulación física del hardware RF. Configura el chip interno LoRa SX1262 integrado en la Heltec V3. Maneja frecuencias, anchos de banda, factores de dispersión y sincronización de red.
* **[Heltec ESP32 Dev-Boards](https://github.com/Heltec-Aaron-Lee/WiFi_Kit_series) / `HT_SSD1306Wire.h`:**
    * **Uso:** Controlador del HMI (Human Machine Interface). Permite la comunicación por el protocolo I2C con la pequeña pantalla OLED de 0.96 pulgadas para imprimir datos de depuración "in situ".

## Librerías Nativas del Núcleo ESP32
*Estas librerías vienen incluidas al instalar el soporte de la placa ESP32 en Arduino IDE; no requieren descarga externa.*

* **`mbedtls/aes.h`:** * **Uso:** Motor criptográfico de grado militar. Ejecuta el algoritmo Advanced Encryption Standard (AES) en bloques de 128 bits para el cifrado y descifrado de los *payloads* garantizando que el canal de radio no pueda ser interceptado o suplantado.
* **`SPI.h`:** * **Uso:** Bus de Comunicaciones Seriales Periféricas. Utilizado internamente por el ESP32-S3 para enviar los comandos de configuración y los paquetes de datos al chip LoRa SX1262.
* **`Wire.h`:** * **Uso:** Librería base para el bus I2C (Inter-Integrated Circuit), necesaria para el funcionamiento fluido de la pantalla OLED.
* **`esp_sleep.h`:** * **Uso:** Gestión energética (Hardware de ultra bajo consumo). Permite poner a los agentes remotos en estado de *Deep Sleep* entre ciclos de muestreo y transmisión, prolongando la vida de las baterías de litio.