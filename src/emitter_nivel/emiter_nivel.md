# Análisis del Firmware: Agente Emisor de Nivel (ID: 1)

Este documento explica el algoritmo de ToF (Time of Flight) y la lógica de estado implementada en el firmware del Agente de Nivel.

## 1. Física de Medición (Ultrasonido)
El microcontrolador genera un pulso de 12 µs en el pin *Trigger* del HC-SR04 y mide el tiempo en alto del pin *Echo* usando la función `pulseIn`.
La distancia física se calcula basándose en la velocidad del sonido en el aire (aprox. 343 m/s), implementando la siguiente ecuación matemática:

$$d = \frac{v \cdot t}{2}$$

Donde $d$ es la distancia, $v$ es la velocidad del sonido (0.0343 cm/µs) y $t$ es el tiempo medido en microsegundos. El factor divisor de 2 compensa el viaje de ida y vuelta del pulso acústico.

## 2. Referenciación y Lógica Logística
* **Altura Real:** El sistema resta la distancia medida de una cota fija (altura total del tanque, ej. 8.0 cm) para obtener el tirante de agua real.
* **Clasificación:** Se evalúa la variable mediante condicionales para asignar un estado de texto (`NIVEL ALTO`, `NIVEL MEDIO`, `NIVEL BAJO`, `VACIO`), lo que permite que el Nodo Central tome decisiones rápidas (ej. encender una bomba) sin tener que procesar la matemática de nuevo.

## 3. Criptografía y LoRa
Los datos de distancia y altura se formatean en CSV (`1,distancia,altura`), se empacan en un arreglo de 16 bytes y se pasan por la función `mbedtls_aes_crypt_ecb` antes de invocar la transmisión de la librería `RadioLib`.