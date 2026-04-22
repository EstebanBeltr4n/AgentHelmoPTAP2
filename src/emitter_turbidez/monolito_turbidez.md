# Análisis del Firmware: Agente Emisor de Turbidez (ID: 0)

Este documento detalla el funcionamiento interno del código fuente (`.ino`) del **Agente de Turbidez** del Proyecto HELMO, responsable de medir la dispersión óptica en el agua y transmitir los datos de forma segura.

---

## 1. Principio de Adquisición (Sensor Óptico)
El agente utiliza un sensor fotoeléctrico basado en el principio de **nefelometría**. El dispositivo mide la cantidad de luz infrarroja que atraviesa el fluido:
* **Agua Limpia:** Alta transmitancia, lo que resulta en un voltaje cercano a los **3.3V** (en configuración de lectura directa).
* **Agua Turbia:** Las partículas en suspensión dispersan la luz, reduciendo el voltaje recibido en el receptor óptico (valores cercanos a **1.0V** o menos).

El microcontrolador **ESP32-S3** realiza lecturas analógicas con una resolución de **12 bits (0-4095)**, configurado con una atenuación de **11dB** para mapear linealmente el rango de voltaje del sensor sin necesidad de divisores de tensión externos.

---

## 2. Flujo de Procesamiento en el Borde (Edge Computing)
Para garantizar la precisión y la eficiencia energética, el firmware ejecuta los siguientes pasos:

1. **Muestreo Adaptativo:** Se captura una lectura analógica cada **2.5 segundos**. Este intervalo es crítico para mantener activa la **Powerbank AN996**, evitando su auto-apagado por inactividad.
2. **Filtrado Digital (EMA):** Se aplica un filtro de **Media Móvil Exponencial** ($EMA$) para suavizar la señal:
   $$EMA_{t} = \alpha \cdot NTU_{actual} + (1 - \alpha) \cdot EMA_{t-1}$$
   Donde $\alpha = 0.3$, lo que permite ignorar ruidos transitorios como burbujas de aire o partículas grandes aisladas.
3. **Conversión a NTU:** El voltaje capturado se transforma a **Unidades Nefelométricas de Turbidez (NTU)** en un rango de **0 a 3000**, utilizando una función de mapeo inverso basada en la respuesta del sensor.
4. **Buffer de Acumulación:** Los datos no se envían individualmente. El sistema llena un buffer de **10 muestras** (25 segundos de recolección) antes de promediar y transmitir.

---

## 3. Ensamblaje y Transmisión Segura
Una vez procesado el buffer, el agente prepara el paquete de datos para la red LoRa:

* **Estructura de la Trama (CSV):** `ID,NTU_Promedio,RAW_Promedio,Tendencia`
  * *Ejemplo:* `0,250.5,3100,+1.20`
* **Capa de Seguridad:** La trama se cifra mediante **AES-128 en modo ECB** utilizando la librería `mbedtls`. Esto asegura que los datos ambientales sean ilegibles para receptores no autorizados en la zona.
* **Hardware RF:** La transmisión se realiza mediante el chip **SX1262** integrado, configurado en la banda de **915 MHz** (ISM), optimizado para largo alcance y bajo consumo de energía en entornos rurales o industriales.

---

## 4. Estrategia de Persistencia Energética
El firmware está optimizado para la batería **AN996 (5000mAh)**. Dado que esta fuente de poder pausa el suministro tras 30 segundos sin carga significativa, el ciclo de transmisión cada **25 segundos** actúa como un pulso de "Keep-Alive", garantizando que el sistema HELMO permanezca operativo 24/7 sin intervenciones manuales.