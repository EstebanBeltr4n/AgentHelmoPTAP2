## 1. Descripción General
El Agente Emisor de pH (Nodo ID: 2) es un componente de borde (Edge) encargado exclusivamente de adquirir, procesar y transmitir el nivel de acidez/alcalinidad del agua. Utiliza un sensor analógico **PH-4502C** y aplica procesamiento local mediante un filtro de Media Móvil Exponencial (EMA) para suavizar las lecturas. 

Para optimizar el uso del espectro y la energía, el nodo implementa un sistema de almacenamiento local (Buffer) que agrupa 10 lecturas antes de empaquetarlas, cifrarlas con **AES-128 ECB** y transmitirlas al nodo central vía **LoRa**.

## 2. Especificaciones de Hardware
* **Microcontrolador:** Heltec WiFi LoRa 32 V3 (ESP32-S3 + SX1262)
* **Sensor:** Módulo de pH PH-4502C con sonda BNC.
* **Pantalla:** OLED SSD1306 de 0.96" integrada (Control vía bus VEXT).
* **Frecuencia LoRa:** 915.0 MHz (Banda ISM).

## 3. Asignación de Pines (Pinout)
| Componente | Pin Heltec V3 | Función |
| :--- | :--- | :--- |
| **PH-4502C** | `GPIO 2` | Entrada Analógica (ADC_11db, 12-bit) |
| **OLED SDA** | `GPIO 17` | Datos I2C |
| **OLED SCL** | `GPIO 18` | Reloj I2C |
| **VEXT_PIN** | `GPIO 36` | Alimentación de periféricos (Activo en LOW) |
| **LoRa SPI** | `8, 9, 10, 11` | CS, SCK, MOSI, MISO |
| **LoRa Control** | `12, 13, 14` | RST, BUSY, DIO1 |

## 4. Calibración y Modelado Matemático
El sistema utiliza una ecuación lineal de primer orden para convertir el voltaje medido en la escala de pH. Los parámetros base definidos en el código (ajustables empíricamente con soluciones buffer) son:

* **Punto de Neutralidad ($pH = 7.0$):** 2.50 V
* **Pendiente de conversión ($m$):** 0.18 V/pH

La ecuación de calibración implementada en el firmware es:
$$pH = 7.0 + \left( \frac{2.50 - V_{sensor}}{0.18} \right)$$

## 5. Lógica de Adquisición y Transmisión
1. **Sobremuestreo (Oversampling):** Se toman 20 muestras analógicas consecutivas (separadas por 10ms) para calcular un promedio y mitigar el ruido eléctrico del ADC.
2. **Filtrado EMA:** Se aplica un filtro de media exponencial (Factor $\alpha = 0.30$) para rastrear la tendencia, evitando falsos positivos por picos transitorios.
3. **Buffering:** Los valores se almacenan en un arreglo local. Al alcanzar `PACKET_SIZE = 10`, se dispara el evento de transmisión.
4. **Criptografía:** El payload (formato `ID,pH,EMA`) se cifra utilizando la librería `mbedtls` bajo el estándar simétrico AES-128.
5. **Telemetría RF:** Se transmite un paquete fijo de 16 bytes a través del transceptor SX1262 utilizando Spreading Factor 7 y Ancho de Banda de 125 kHz.

---