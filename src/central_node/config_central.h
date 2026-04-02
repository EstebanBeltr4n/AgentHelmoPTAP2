
// ===== CONFIGURACIÓN CENTRAL HELMO LoRa =====
// Archivo: config_central.h
// Autor: Esteban Eduardo Escarraga Tuquerres
// Propósito: Constantes, pines, credenciales seguras para nodo central

#ifndef CONFIG_CENTRAL_H
#define CONFIG_CENTRAL_H

// 📡 IDENTIFICACIÓN DEL NODO
#define NODE_ID 3                           // ID único del nodo central
#define NODE_TYPE "CENTRAL_PTAP"            // Tipo para logs y web

// 🔌 PINES HELTEC V3 ESP32 + ACTUADORES
#define SDA_OLED 17                         // SDA pantalla OLED
#define SCL_OLED 18                         // SCL pantalla OLED  
#define RST_OLED 21                         // Reset OLED
#define VEXT_PIN 36                         // Alimentación externa
#define SERVO_TURBIDEZ 4                    // Servo válvula turbidez
#define LED_NIVEL_BAJO 38                   // LED alerta nivel bajo
#define BUZZER_ALERTA 2                     // Buzzer emergencias

// 🌊 UMBRALES SENSORIALES (para control loop abierto)
#define TURB_MAX 500.0                      // NTU máximo aceptable
#define PH_MIN 6.5                          // pH mínimo potable
#define PH_MAX 7.5                          // pH máximo potable
#define NIVEL_MIN_CM 5.0                    // Altura mínima tanque (cm)
#define ALTURA_TANQUE_CM 25.0               // Altura total tanque

// 🔐 WIFI AP SEGURO (operario PTAP se conecta)
#define WIFI_SSID "LoRaCentral_Seguro"      // Nombre red WiFi
#define WIFI_PASS "ptap2026!"               // Contraseña WiFi

// 🔐 WEB AUTENTICACIÓN (solo operario autorizado)
#define WEB_USER "admin"                    // Usuario web
#define WEB_PASS "ptap2026!"                // Contraseña web

// 🗄️ MYSQL CREDENCIALES (para ML y persistencia)
#define MYSQL_SERVER "127.0.0.1"        // IP servidor MySQL (ajusta)
#define MYSQL_USER "ptap_user"              // Usuario DB
#define MYSQL_PASS "ptap2026!"              // Contraseña DB  
#define MYSQL_DB "ptap_sensores"            // Base de datos

// 🧠 ML EMBEBIDO - VENTANAS TEMPORALES
#define ML_VENTANA_TURB 10                  // Promedio móvil 10 lecturas
#define ML_PREDICCION_HORAS 1               // Horizonte predicción

// 📡 CONFIG LoRa (igual para todos nodos HELMO)
#define LORA_FREQ 915.0                     // Frecuencia MHz (915 Colombia)
#define LORA_SF 7                           // Spreading Factor
#define LORA_BW 125.0                       // Bandwidth kHz
#define LORA_CR 5                           // Coding Rate
#define LORA_SYNC 0x12                      // Sync word
#define LORA_POWER 22                       // Potencia dBm
#define LORA_CURRENT 8                      // Current limit
#define LORA_RSSI 1.6                       // RSSI offset

#endif  
