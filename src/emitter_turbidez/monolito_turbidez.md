# Análisis del Firmware: Agente Emisor de Turbidez (ID: 0)

Este documento detalla el funcionamiento interno del código fuente (`.ino`) del Agente de Turbidez, responsable de medir la dispersión óptica en el agua.

## 1. Principio de Adquisición (Sensor Óptico)
El agente utiliza un sensor fotoeléctrico que mide la cantidad de luz infrarroja transmitida a través del agua frente a la luz dispersada por partículas en suspensión. 
El microcontrolador realiza múltiples lecturas analógicas (RAW) para estabilizar la medida antes de la conversión a Unidades Nefelométricas de Turbidez (NTU).

## 2. Flujo de Procesamiento en el Borde
1.  **Muestreo:** Se capturan ráfagas de datos analógicos en el pin ADC designado.
2.  **Conversión a NTU:** Se aplica una función polinomial o lineal de calibración para mapear el valor RAW (típicamente 0-4095 en el ESP32 de 12 bits) a NTU (0 - 3000 NTU).
3.  **Filtrado:** Al igual que los demás nodos, puede emplear un filtro de paso bajo (EMA) para evitar picos causados por burbujas aisladas.

## 3. Ensamblaje y Transmisión
La trama se formatea estrictamente como `0,NTU,RAW` (donde `0` es el identificador `NODE_ID`). 
El paquete se cifra utilizando la llave precompartida AES-128 en modo ECB mediante `mbedtls` y se envía a través del hardware LoRa (SX1262) configurado a 915 MHz.