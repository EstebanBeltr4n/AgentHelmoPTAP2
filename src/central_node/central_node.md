# Análisis del Firmware: Agente Central, Servidor Web y Persistencia (Sink Node)

El Agente Central es el cerebro del sistema HELMO. Opera en un ESP32-S3 y gestiona un ecosistema híbrido que combina radiofrecuencia (LoRa), protocolos web (HTTP/HTML) y almacenamiento en bases de datos relacionales (MySQL).
## 1. Demultiplexor LoRa y Criptografía Inversa

El transceptor SX1262 se mantiene en modo de recepción continua (RX). Cuando el microcontrolador detecta la llegada de un paquete:

    Captura de Payload: Se extrae el bloque de 16 bytes cifrados.

    Descifrado AES-128: Se utiliza la librería mbedtls con una llave simétrica compartida para retornar el mensaje a texto plano.

    Parseo de Telemetría: El mensaje en formato CSV (ej. 0,450.5,2300) se segmenta para identificar el NODE_ID, el valor procesado y el valor analógico crudo (RAW).

    Actualización de RAM: Se actualizan las variables globales y se alimenta una estructura de datos circular (struct Registro) en la memoria volátil para el despliegue inmediato en el Dashboard.

## 2. Integración XAMPP: Persistencia de Datos (MySQL)

Para garantizar el análisis histórico, el Agente Central actúa como un cliente HTTP que comunica el hardware con el software de servidor:

    Protocolo de Inserción: Tras recibir y validar un paquete LoRa, el ESP32 genera una petición HTTP POST dirigida a una dirección IPv4 local (PC con XAMPP).

    Puente PHP: Un script intermedio (insertar.php) recibe las variables, las sanea y ejecuta una sentencia INSERT INTO en la base de datos helmo_db.

    Sincronización: Esto permite que, mientras el ESP32 muestra datos en tiempo real, el servidor MySQL mantenga un registro histórico de turbidez, pH y niveles para auditorías ambientales.

## 3. Servidor Web HMI (Dashboard Local)

El ESP32-S3 levanta un servidor asíncrono en el puerto 80 para la interfaz hombre-máquina (HMI):

    HTML/CSS Dinámico: El microcontrolador inyecta los valores de la RAM en un diseño de "Modo Oscuro" con tarjetas visuales para cada variable crítica.

    Monitoreo en Tiempo Real: La página implementa un refresco automático cada 3 segundos, permitiendo visualizar las fluctuaciones de la Planta de Tratamiento (PTAP) sin intervención manual.

## 4. Lógica de Actuación y Alertas Físicas (Edge AI)

El sistema no es solo de monitoreo, sino de control reactivo basado en reglas (Edge Computing):

    Control de Flujo (Servo GPIO 4): Si la turbidez supera los 500 NTU, el firmware acciona automáticamente un servomotor a 90° para desviar el agua contaminada.

    Alerta de Nivel (LED GPIO 38): Se activa de forma binaria si la altura del tanque es inferior a 5 cm.

    Alerta Crítica (Buzzer GPIO 2): Se dispara ante cualquier condición fuera de los parámetros operativos óptimos, notificando de forma sonora una falla inminente en la PTAP.

Desarrollado por: Esteban Eduardo Escarraga

* **Proyecto: Sistema Multi-Agente de Monitoreo Hídrico HELMO (V3.1)