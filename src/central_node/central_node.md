# Análisis del Firmware: Agente Central y Servidor Web (Sink Node)

El Agente Central es el cerebro del sistema HELMO. Opera en un ESP32-S3 y tiene la compleja tarea de manejar tareas asíncronas de radiofrecuencia (LoRa) y WiFi de forma paralela.

## 1. Demultiplexor LoRa y Criptografía Inversa
El transceptor SX1262 se mantiene en modo de recepción continua (RX). Cuando se interrumpe el microcontrolador por la llegada de un paquete:
1.  Se captura el *payload* cifrado de 16 bytes.
2.  Se inyecta la llave simétrica AES-128 (`mbedtls_aes_setkey_dec`) y se llama a la función de descifrado.
3.  El *string* en texto plano se segmenta (parseo) utilizando la coma (`,`) como delimitador.
4.  **Demultiplexación Lógica:** Un bloque `switch` o `if-else` lee el índice 0 del CSV (El `NODE_ID`). Dependiendo de si es 0, 1 o 2, actualiza las variables globales en memoria RAM correspondientes a Turbidez, Nivel o pH.

## 2. Servidor Web Asíncrono
El ESP32-S3 se conecta a la red WiFi local y levanta un servidor en el puerto 80.
* **HTML/CSS Dinámico:** El microcontrolador inyecta los valores actualizados de la RAM dentro de las etiquetas HTML antes de despachar la respuesta al cliente (Navegador Web).
* **Refresco:** La página implementa metadatos de refresco o llamadas AJAX/Fetch para actualizar el Dashboard sin recargar toda la interfaz, mostrando un monitoreo en tiempo casi real de la Planta de Tratamiento.