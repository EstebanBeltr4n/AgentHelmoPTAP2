# Proyecto HELMO: Sistema Multi-Agente de Monitoreo Hídrico (PTAP)
## Agente Emisor de Turbidez (Nodo ID: 0)

Este repositorio contiene el firmware oficial para el **Agente de Turbidez**, parte del ecosistema HELMO diseñado para el monitoreo de Plantas de Tratamiento de Agua Potable (PTAP) en entornos de alta montaña. 

### 1. Resumen Técnico
El nodo realiza la adquisición de datos de turbidez mediante sensores ópticos, aplica filtrado digital avanzado (EMA), cifra los datos mediante **AES-128** y los transmite vía **LoRa** de forma eficiente.

### 2. Características Principales del "Monolito"
* **Gestión de Energía "Keep-Alive":** Optimizado para Powerbanks comerciales. El ciclo de trabajo evita el auto-apagado de la fuente (5V/2.4A) al mantener un consumo constante mediante procesos activos y telemetría OLED.
* **Seguridad de Grado Industrial:** Implementación de cifrado avanzado mediante hardware (mbedtls) para proteger la integridad de los datos hídricos en la red LoRa.
* **Filtrado Digital EMA:** Utiliza un filtro de Media Móvil Exponencial ($EMA$) para suavizar picos de ruido en la lectura analógica:
  $$EMA_{actual} = 0.3 \cdot NTU_{inst} + 0.7 \cdot EMA_{prev}$$
* **Transmisión por Buffer:** Acumula 10 muestras antes de realizar el envío RF, optimizando el espectro y reduciendo el consumo pico de antena.

### 3. Especificaciones de Hardware
| Componente | Detalle |
| :--- | :--- |
| **Microcontrolador** | Heltec WiFi LoRa 32 V3 (ESP32-S3) |
| **Radio** | Semtech SX1262 (LoRa 915 MHz) |
| **Sensor** | Sensor Óptico de Turbidez (Salida Analógica) |
| **Pantalla** | OLED SSD1306 (128x64 px) |
| **Alimentación** | Powerbank AN-P96 (5000mAh / 18.5Wh) |

### 4. Configuración de Pines (Mapping)
* **ADC Turbidez:** GPIO 4 (12-bit)
* **Bus I2C (OLED):** SDA (17), SCL (18)
* **Bus SPI (LoRa):** SCK (9), MISO (11), MOSI (10), CS (8)

---

### 5. Firmware del Agente (Monolito)

