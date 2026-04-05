# 03. Resultados y Experimentos de Validación

Durante la fase de validación en la Planta de Tratamiento de Agua Potable (PTAP), el sistema HELMO fue sometido a diversas pruebas de estrés y estabilidad operativa.

## 1. Validación de Transmisión Cifrada (AES-128)
Se verificó exitosamente la confidencialidad de la información transmitida por el aire. El uso del motor criptográfico por hardware (`mbedtls_aes_crypt_ecb`) demostró no generar latencias perceptibles en el ciclo del ESP32-S3. 
* **Tiempo de cifrado/descifrado:** < 2 ms por paquete de 16 bytes.
* **Integridad:** 100% de los paquetes recibidos en el Agente Central coincidieron con el *payload* original gracias al demultiplexor y la validación de bloque fijo.

## 2. Precisión del Edge Computing (EMA)
El Filtro de Promedio Móvil Exponencial (EMA) demostró ser vital para el sensor de pH (ID 2) y el sensor de Nivel (ID 1), mitigando las lecturas erráticas causadas por el oleaje interno del tanque y el ruido eléctrico del electrodo electroquímico. Se logró una señal limpia que previno falsas alarmas en el umbral de eventos críticos.

## 3. Estabilidad del Enlace RF (SX1262)
Los experimentos demostraron que el uso de la palabra de sincronización privada (`0x12`) aisló efectivamente a la red HELMO de otras redes LoRaWAN locales (como Helium o The Things Network), garantizando que el Nodo Central solo despertara el microcontrolador ante eventos de recepción legítimos provenientes de sus agentes.

## 4. Visualización en Tiempo Real (HMI y Web)
* **Nivel Local (HMI):** Las pantallas OLED nativas de las placas Heltec permitieron la calibración "in situ" sin necesidad de computadoras portátiles.
* **Nivel Remoto (Dashboard Web):** El servidor asíncrono alojado en la IP estática del Nodo Central logró parsear e inyectar el código HTML/CSS dinámicamente, refrescando los valores de Turbidez, pH y Nivel de forma robusta y mostrando el historial en memoria RAM.