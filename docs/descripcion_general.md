# Descripción General del Sistema Arquitectónico

El sistema HELMO opera bajo un paradigma de red en estrella, donde múltiples agentes emisores capturan, procesan, cifran y envían datos a un agente central que actúa como sumidero (Sink) y servidor web.

## Arquitectura de Hardware
La plataforma base para todos los nodos es la placa **Heltec WiFi LoRa 32 V3**, impulsada por un microcontrolador ESP32-S3. Esta selección permite manejar simultáneamente conectividad WiFi (Nodo Central), comunicaciones LoRa (SX1262), interfaces OLED (I2C) y procesamiento criptográfico acelerado por hardware.

## Descripción de los Agentes

### 1. Agente Emisor de Turbidez (ID: 0)
* **Sensor:** Óptico infrarrojo.
* **Función:** Clasificación de calidad mediante análisis de dispersión de luz en el agua.

### 2. Agente Emisor de Nivel (ID: 1)
* **Sensor:** Ultrasonido HC-SR04 (Tiempo de Vuelo - ToF).
* **Procesamiento:** Calcula la distancia y la altura real respecto a la base del tanque. Implementa una evaluación logística de criticidad (Nivel Alto, Medio, Bajo, Vacío).

### 3. Agente Emisor de pH (ID: 2)
* **Sensor:** Sonda electroquímica PH-4502C.
* **Procesamiento:** Convierte el diferencial de voltaje en unidades de pH utilizando una curva de calibración predefinida.

### 4. Agente Central (Receptor y Dashboard)
* **Función:** Actúa como *Gateway*. Escucha la frecuencia de 915 MHz, desencripta los paquetes entrantes mediante AES-128 y utiliza un demultiplexor lógico (basado en el ID del nodo) para clasificar los datos. Levanta un servidor web asíncrono para visualización en tiempo real.

## Filtrado en el Borde (Edge Computing)
Para asegurar la fiabilidad de las lecturas, los nodos implementan un Filtro de Promedio Móvil Exponencial (EMA). La ecuación matemática implementada en el firmware es:

$$EMA_{t} = \alpha \cdot x_{t} + (1 - \alpha) \cdot EMA_{t-1}$$

Donde $\alpha$ es el factor de suavizado (típicamente 0.3 en este proyecto) y $x_{t}$ es la lectura actual del sensor.

## Protocolo de Trama
Los paquetes desencriptados tienen una estructura en formato CSV para fácil parseo computacional:
`ID_NODO,DATO_1,DATO_2`
*(Ejemplo Nivel: `1,25.4,3.2` -> ID 1, Distancia 25.4cm, Altura 3.2cm)*