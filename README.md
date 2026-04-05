# Proyecto HELMO: Sistema Multi-Agente de Monitoreo Hídrico (PTAP)


**Autor:** Esteban Eduardo Escarraga Tuquerres
**Fecha:** Marzo 2026

## Descripción del Proyecto
El proyecto **HELMO** (Planta de Tratamiento de Agua Potable) es una red inalámbrica de sensores multi-agente basada en la arquitectura ESP32 y comunicaciones LoRa (915 MHz). Está diseñado para el monitoreo descentralizado, seguro y en tiempo real de parámetros de calidad y cantidad de agua, procesando los datos en el borde (Edge Computing) antes de su transmisión.

## Características Principales
* **Topología Multi-Agente:** Nodos distribuidos e independientes para Turbidez, Nivel y pH.
* **Comunicaciones de Largo Alcance:** Implementación del transceptor SX1262 (LoRa) optimizado para entornos de difícil acceso.
* **Criptografía Integrada:** Seguridad de datos mediante cifrado simétrico avanzado (AES-128 ECB) con la librería `mbedtls`.
* **Edge Computing:** Filtrado de señales en los nodos emisores (Promedio Móvil Exponencial - EMA) para reducir el ruido y la carga de transmisión.
* **Interfaz HMI y Web:** Monitoreo local vía pantallas OLED (I2C) y monitoreo remoto a través de un Dashboard Web alojado en el Nodo Central.
=======
- src/emitter_turbidez/emitter_turbidez.ino (Nodo TURB, NODE_ID=0)
- src/emitter_ph/emitter_ph.ino (Nodo pH, NODE_ID=2)
- src/emitter_nivel/emitter_nivel.ino (Nodo NIVEL, NODE_ID=1)
- src/central_node/central_node.ino (Nodo CENTRAL, NODE_ID=3)


## Todos usan los mismos parámetros LoRa: int state = lora->begin(915.0, 125.0, 7, 5, 0x12, 22, 8, 1.6);
