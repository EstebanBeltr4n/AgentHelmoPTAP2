# Análisis del Firmware: Agente Emisor de pH (ID: 2)

Este firmware se encarga de la caracterización electroquímica del agua y destaca por su mecanismo de búfer de datos para optimizar el uso de la red LoRa.

## 1. Caracterización Electroquímica
El sensor PH-4502C emite una variación de voltaje basada en la concentración de iones de hidrógeno. El código promedia 20 muestras rápidas de ADC, calcula el voltaje real considerando el ADC de 12 bits y el voltaje de referencia (3.3V), y aplica la ecuación de la recta de calibración:

$$pH = -5.70 \cdot V + 21.34$$

## 2. Filtro Promedio Móvil Exponencial (EMA)
Para contrarrestar el ruido térmico y las fluctuaciones del electrodo, se aplica un filtro recursivo IIR:

$$EMA_{t} = 0.3 \cdot pH_{actual} + 0.7 \cdot EMA_{t-1}$$

Esto genera un vector de tendencia suave que refleja la calidad real del agua a lo largo del tiempo.

## 3. Mecanismo de Buffer y Transmisión
A diferencia de otros nodos que transmiten inmediatamente, el Agente de pH almacena localmente las lecturas en un arreglo `buffer_ph` hasta alcanzar el `PACKET_SIZE` (10 muestras). Una vez lleno, ensambla el payload `2,pH_actual,EMA`, cifra el bloque de 16 bytes con AES-128 y lo inyecta a la red LoRa, ahorrando energía (Duty Cycle) al no saturar el espectro.


## Normativa RAS 2000 Colombia
- **Potable**: 6.5-8.5 pH
- **Tratamiento**: 5.5-9.5 pH  
- **Emergencia**: <5.5 o >9.5 pH
