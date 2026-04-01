/* ============================================================================
   NODO CENTRAL - AGENTE OBJETO (SISTEMA MULTI-AGENTE HELMO)
   ============================================================================
   Autor: Esteban Eduardo Escarraga
   Descripción: Este nodo actúa como el Agente Central (Coordinador)
   dentro de una arquitectura distribuida para el monitoreo de PTAP.
   
   Fase actual (Pruebas Edge): Recepción asíncrona LoRa, desencriptación AES-128,
   demultiplexación de sensores y visualización en tiempo real vía WebServer local
   (Persistencia en base de datos temporalmente desactivada para validación de RF).

   Fecha: 31 de marzo de 2026
   
   Hardware: Heltec ESP32-S3 LoRa32 V3
   ============================================================================ */

#define NODE_ID 3 // Identificador lógico del Nodo Central en la topología de red

// --- INCLUSIÓN DE LIBRERÍAS CORE ---
#include <RadioLib.h>       // Gestión del stack físico de LoRa (Chip SX1262)
#include <WiFi.h>           // Pila TCP/IP para la conexión inalámbrica local
#include <WebServer.h>      // Implementación del servidor HTTP embebido
#include <Wire.h>           // Protocolo de comunicación I2C
#include "HT_SSD1306Wire.h" // Controlador gráfico para la pantalla OLED integrada
#include "mbedtls/aes.h"    // Aceleración criptográfica por hardware (AES)
#include <ESP32Servo.h>     // Control de actuadores PWM (Servomotores)

// ========= CAPA DE SEGURIDAD: LLAVE SIMÉTRICA (AES-128) =========
// Clave privada compartida (PSK) entre todos los nodos para evitar suplantación (Spoofing)
const unsigned char aes_key[16] = {
  'E', 's', 't', 'e', 'b', 'a', 'n', 'L', 'o', 'R', 'a', '2', '0', '2', '6', '!'
};

// ========= MAPEO DE HARDWARE (HELTEC V3 - ESP32-S3) =========
// Interfaz HMI Local
#define SDA_OLED 17         // Línea de datos I2C para OLED
#define SCL_OLED 18         // Línea de reloj I2C para OLED
#define RST_OLED 21         // Pin de reinicio de la pantalla
#define VEXT_PIN 36         // Control de potencia para periféricos (Vext)

// Bus SPI dedicado al Transceptor LoRa SX1262
#define LORA_CS 8           // Chip Select
#define LORA_SCK 9          // Serial Clock
#define LORA_MOSI 10        // Master Out Slave In
#define LORA_MISO 11        // Master In Slave Out
#define LORA_RST 12         // Reset del módulo de radio
#define LORA_BUSY 13        // Pin de estado de ocupado del SX1262
#define LORA_DIO1 14        // Pin de interrupción de hardware (RX Done)

// Asignación de Actuadores (Sistema de Control Reactivo)
#define SERVO_TURBIDEZ 4    // Válvula simulada para derivación de agua
#define LED_NIVEL_BAJO 38   // Indicador visual de alerta de estanque vacío
#define BUZZER_ALERTA 2     // Señal acústica unificada de fallas

// ========= CONFIGURACIÓN DE RED (WIFI) =========
// Credenciales de la red local para el acceso al panel de control web
const char* WIFI_SSID = "Phepe"; 
const char* WIFI_PASS = "bbq7W5ha";

// ========= PARÁMETROS DE EDGE COMPUTING (MACHINE LEARNING) =========
const int ML_VENTANA_TURB = 12;       // Tamaño del buffer circular para promedios móviles
const float ML_HORAS_ADELANTE = 1.0;  // Horizonte predictivo (Delta de tiempo)

// ========= INSTANCIACIÓN DE OBJETOS GLOBALES =========
SSD1306Wire pantalla(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED); // Pantalla
SX1262* lora;                         // Puntero dinámico al módulo de radio
WebServer server(80);                 // Servidor HTTP en el puerto estándar 80
Servo servoTurbidez;                  // Objeto controlador del servo

// --- VARIABLES DE ESTADO Y TELEMETRÍA (RAM) ---
// Variables globales que almacenan el último estado conocido de cada agente
float turb_ntu = 0, turb_raw = 0, turb_trend = 0;
float ph_val = 7.0, ph_trend = 0;
float dist_cm = 0, altura_real = 0;

// Estados lógicos inferidos por el sistema
String estadoGlobal = "SIN DATOS";
String accionRecomendada = "INICIANDO";

// Banderas de control para interrupciones por hardware (ISR)
volatile bool receivedFlag = false; // Flag atómica para indicar llegada de paquete
uint8_t rxBuffer[32];               // Buffer de recepción del payload LoRa cifrado

// Variables para el modelo algorítmico local
float turb_ventana[ML_VENTANA_TURB];
int turb_idx = 0;
float turb_prediccion = 0;

// ========= ESTRUCTURA DE PERSISTENCIA EN MEMORIA VOLÁTIL (RAM) =========
// Al prescindir temporalmente de MySQL, usamos una cola circular en RAM
// para nutrir la tabla del panel web en tiempo real.
const int HIST_MAX = 40; // Número máximo de filas a mostrar en la web

// Estructura de datos (Struct) para estandarizar cada paquete recibido
struct Registro {
  unsigned long ts;   // Marca de tiempo (Timestamp local en milisegundos)
  int sensor_id;      // Origen del dato (0=Turbidez, 1=Nivel, 2=pH)
  float turb_ntu_;    // Valor encapsulado
  float turb_raw_;
  float dist_cm_;
  float altura_;
  float ph_;
};

Registro hist[HIST_MAX]; // Array estructurado (Historial)
int hist_idx = 0;        // Puntero de escritura (Cola circular)
int hist_count = 0;      // Contador absoluto de elementos

// Función que inyecta los datos recién llegados a la estructura en RAM
void addHist(int sensor_id, float turb, float raw, float dist, float alt, float ph) {
  Registro& r = hist[hist_idx]; // Referencia directa a la posición actual
  r.ts = millis();
  r.sensor_id = sensor_id;
  r.turb_ntu_ = turb;
  r.turb_raw_ = raw;
  r.dist_cm_ = dist;
  r.altura_ = alt;
  r.ph_ = ph;

  // Lógica de cola circular: Si llega al límite (40), sobrescribe el más antiguo
  hist_idx = (hist_idx + 1) % HIST_MAX;
  if (hist_count < HIST_MAX) hist_count++;
}

// ========= PROTOTIPOS DE FUNCIONES =========
// Declaración anticipada de rutinas para correcta compilación en C++
void controlarActuadores();
void evaluarEstado();
void actualizarOLED();
void webCompleta();
void actualizarML_Turbidez(float nueva_lectura);
float calcularPrediccion();

// ========= MANEJADOR DE INTERRUPCIONES (ISR) =========
// Esta función se ejecuta a nivel de hardware cuando el pin DIO1 detecta una onda LoRa
#if defined(ESP32)
IRAM_ATTR // Fuerza la carga de la función en la memoria RAM ultrarrápida (IRAM)
#endif
void setFlag(void) {
  receivedFlag = true; // Levanta la bandera sin bloquear el procesador principal
}

// ================= RUTINA DE INICIALIZACIÓN (SETUP) =================
void setup() {
  Serial.begin(115200); // Puerto serie para depuración (Baud rate estándar ESP32)
  delay(1000);
  Serial.println("=== CENTRAL OBJETIVO V2 (Fase Pruebas Edge) ===");

  // 1. Inicialización de la gestión de energía
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, LOW); // Enciende riel de poder para periféricos
  delay(100);

  // 2. Inicialización de Interfaz Local (OLED)
  pantalla.init();
  pantalla.clear();
  pantalla.setFont(ArialMT_Plain_10);
  pantalla.drawString(0, 0, "Central Objetivo V2");
  pantalla.display();

  // 3. Configuración del Transceptor LoRa (SX1262)
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS); // Asigna pines al bus SPI
  Module* radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY, SPI);
  lora = new SX1262(radio);

  // Parámetros RF: 915MHz, 125kHz Bandwidth, SF 7, SyncWord Privado (0x12)
  int state = lora->begin(915.0, 125.0, 7, 5, 0x12, 22, 8, 1.6);
  if (state == RADIOLIB_ERR_NONE) {
    lora->setDio1Action(setFlag); // Vincula la interrupción al pin
    lora->startReceive();         // Pone el radio en modo escucha perpetua (RX Continuo)
    Serial.println("LoRa OK - Escuchando...");
  } else {
    Serial.print("Error Crítico LoRa: "); Serial.println(state);
    while (1); // Bucle infinito de seguridad si el hardware RF falla
  }

  // 4. Configuración de Puertos para Actuadores (Output)
  pinMode(LED_NIVEL_BAJO, OUTPUT);
  pinMode(BUZZER_ALERTA, OUTPUT);
  digitalWrite(LED_NIVEL_BAJO, LOW);
  digitalWrite(BUZZER_ALERTA, LOW);
  servoTurbidez.attach(SERVO_TURBIDEZ); // Vincula el objeto Servo al pin físico
  servoTurbidez.write(0);               // Posición inicial (Válvula cerrada)

  // 5. Conexión a Infraestructura de Red (WiFi)
  WiFi.mode(WIFI_STA); // Modo Estación (Cliente de un router)
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("Conectando WiFi");
  while (WiFi.status() != WL_CONNECTED) { // Espera activa hasta obtener IP
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Conectado!");
  Serial.print("🌐 Accede al panel web en: ");
  Serial.println(WiFi.localIP());

  // 6. Configuración de Rutas del Servidor HTTP Embebido
  server.on("/", []() {
    // Capa de Seguridad de Red: Autenticación básica HTTP (Login)
    if (!server.authenticate("admin", "ptap2026!")) {
      return server.requestAuthentication(); // Despliega el cuadro de usuario/contraseña
    }
    webCompleta(); // Si las credenciales son correctas, despacha el HTML
  });
  server.begin(); // Pone el servidor web online

  // Muestra la IP asignada en la pantalla física para fácil acceso
  IPAddress ip = WiFi.localIP();
  pantalla.clear();
  pantalla.drawString(0, 0, "PTAP Central");
  pantalla.drawString(0, 15, "IP: " + ip.toString());
  pantalla.display();
  
  // Limpieza inicial del buffer de algoritmos predictivos
  for (int i = 0; i < ML_VENTANA_TURB; i++) turb_ventana[i] = 0;
}

// ================= BUCLE PRINCIPAL (MÁQUINA DE ESTADOS) =================
void loop() {
  // 1. Escucha activa de peticiones web HTTP (Clientes entrando al dashboard)
  server.handleClient();

  // 2. Procesamiento de Telemetría (Disparado por Interrupción LoRa)
  if (receivedFlag) {
    receivedFlag = false; // Baja la bandera para permitir futuras interrupciones
    Serial.println("\n📡 Evento de Recepción (RX) Detectado");

    // Copia los datos del chip LoRa al microcontrolador ESP32
    int state = lora->readData(rxBuffer, sizeof(rxBuffer));
    if (state == RADIOLIB_ERR_NONE) {

      // --- CAPA CRIPTOGRÁFICA: Descifrado AES-128 (Modo ECB) ---
      unsigned char dec[32] = { 0 }; // Buffer para texto en claro
      mbedtls_aes_context aes;       // Estructura de contexto del motor criptográfico
      mbedtls_aes_init(&aes);        // Inicialización de hardware
      mbedtls_aes_setkey_dec(&aes, aes_key, 128); // Carga de la llave maestra
      
      // Transformación: Texto Cifrado (rxBuffer) -> Texto Claro (dec)
      mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, (const unsigned char*)rxBuffer, dec);
      mbedtls_aes_free(&aes);        // Liberación de memoria

      String msg = (char*)dec; // Conversión a String para fácil manipulación
      Serial.print("📦 Payload Seguro (Desencriptado): ");
      Serial.println(msg);

      // --- PARSER DE PROTOCOLO: Extracción de CSV (Valores Separados por Comas) ---
      // Busca la posición de cada coma en la cadena de texto
      int c1 = msg.indexOf(',');
      int c2 = msg.indexOf(',', c1 + 1);
      int c3 = msg.indexOf(',', c2 + 1);
      
      // Validación básica de estructura de trama (Debe tener al menos una coma)
      if (c1 > 0) { 
        // Extracción y conversión de Sub-Strings a Tipos de Datos Primitivos
        int id = msg.substring(0, c1).toInt();
        float val = msg.substring(c1 + 1, c2).toFloat();
        //float extra1 = (c2 > 0) ? msg.substring(c2 + 1, c3).toFloat() : 0;
        //float extra2 = (c3 > 0) ? msg.substring(c3 + 1).toFloat() : 0;

        // CORRECCIÓN: Manejo dinámico según la cantidad de comas en el CSV
        float extra1 = 0;
        float extra2 = 0;

        if (c3 > 0) {
          // Si hay 3 comas (ej. Turbidez)
          extra1 = msg.substring(c2 + 1, c3).toFloat();
          extra2 = msg.substring(c3 + 1).toFloat();
        } else if (c2 > 0) {
          // Si solo hay 2 comas (ej. Nivel o pH)
          extra1 = msg.substring(c2 + 1).toFloat(); // Va hasta el final del String
        }

        // --- ENRUTADOR (DEMULTIPLEXOR MULTI-AGENTE) ---
        // Clasifica la información según la identidad del Agente Transmisor
        if (id == 0) {  // Agente 0: Sensor Óptico de Turbidez
          turb_ntu = val;
          turb_raw = extra1;
          turb_trend = extra2;
          Serial.printf("Agente Turbidez -> NTU: %.1f\n", turb_ntu);
          
          actualizarML_Turbidez(turb_ntu); // Alimenta el modelo predictivo local
          addHist(0, turb_ntu, turb_raw, dist_cm, altura_real, ph_val); // Guarda en RAM

        } else if (id == 1) {  // Agente 1: Sensor Ultrasónico de Nivel ToF
          dist_cm = val;
          altura_real = extra1;
          Serial.printf("Agente Nivel -> Altura: %.1fcm\n", altura_real);
          
          addHist(1, turb_ntu, turb_raw, dist_cm, altura_real, ph_val); // Guarda en RAM

        } else if (id == 2) {  // Agente 2: Potenciómetro de Hidrógeno (pH)
          ph_val = val;
          ph_trend = extra1; 
          Serial.printf("Agente pH -> Valor: %.2f | Tr: %.2f\n", ph_val, ph_trend);

          addHist(2, turb_ntu, turb_raw, dist_cm, altura_real, ph_val); // Guarda en RAM
        }

        // --- ETAPA DE TOMA DE DECISIONES AUTÓNOMAS ---
        controlarActuadores(); // Envía señales de pulso/PWM a motores y alarmas
        evaluarEstado();       // Recalcula el nivel de riesgo de la planta
      }
    }
    // Finalizada la transacción, ordena al chip LoRa volver a escuchar el espectro
    lora->startReceive();
  }

  // 3. Actualización continua del Display Físico Local
  actualizarOLED();
  delay(10); // Pausa estratégica para evitar que el Watchdog Timer reinicie el procesador
}

// ============ REGLAS DE NEGOCIO (SISTEMA REACTIVO) ============
// Función que traduce datos crudos en acciones mecánicas de mitigación
void controlarActuadores() {
  // Regla Condicional 1: Derivación de agua por alta turbidez
  // Si NTU > 500, abre la válvula a 90 grados; caso contrario, se mantiene en 0
  servoTurbidez.write(turb_ntu > 500 ? 90 : 0);
  
  // Regla Condicional 2: Protección de bombas de succión por bajo nivel
  digitalWrite(LED_NIVEL_BAJO, altura_real < 5.0 ? HIGH : LOW);
  
  // Regla Condicional 3: Alarma de pánico para operadores en sitio
  bool alerta = (turb_ntu > 500 || altura_real < 5.0);
  digitalWrite(BUZZER_ALERTA, alerta ? HIGH : LOW);
}

// Función algorítmica lógica para clasificar la salud de la red hídrica
void evaluarEstado() {
  // Se evalúan booleanos con los límites permisibles de potabilidad básica
  bool okTurb = turb_ntu <= 500;
  bool okPh = ph_val >= 6.5 && ph_val <= 7.5;
  bool okNivel = altura_real >= 5.0;

  // Si todas las variables están en rango seguro
  if (okTurb && okPh && okNivel) {
    estadoGlobal = "OPTIMA";
    accionRecomendada = "Mantener Operacion";
  } else {
    // En caso de fallas, se priorizan las alertas según criticidad
    estadoGlobal = "ALERTA CRITICA";
    if (!okTurb) accionRecomendada = "Purgar Drenajes (Turbidez)";
    else if (!okNivel) accionRecomendada = "Activar Llenado (Nivel)";
    else accionRecomendada = "Corregir pH Químicamente";
  }
}

// ============ INTERFAZ LOCAL HARDWARE (OLED) ============
void actualizarOLED() {
  pantalla.clear();
  pantalla.setFont(ArialMT_Plain_10);
  // Dibujado de cadenas alfanuméricas concatenadas para visión del operario
  pantalla.drawString(0, 0, "T:" + String(turb_ntu, 0) + " RAW:" + String(turb_raw, 0));
  pantalla.drawString(0, 12, "pH:" + String(ph_val, 2));
  pantalla.drawString(0, 24, "H:" + String(altura_real, 1) + " D:" + String(dist_cm, 1));
  pantalla.drawString(0, 36, "ESTADO: " + estadoGlobal);
  pantalla.drawString(0, 48, accionRecomendada);
  pantalla.display(); // Refresca la matriz de píxeles
}

// ============ INTERFAZ DIGITAL HMI (SERVIDOR WEB) ============
// Renderiza el HTML, CSS inyectado y datos en tiempo real de forma dinámica
void webCompleta() {
  String html = "<!DOCTYPE html><html><head>";
  // Auto-refresco de la página cada 3 segundos (Comportamiento de Dashboard)
  html += "<meta charset='UTF-8'><meta http-equiv='refresh' content='3'>";
  html += "<title>PTAP Central</title>";
  
  // Estilos CSS (Modo Oscuro moderno según el diseño aprobado)
  html += "<style>";
  html += "body{font-family:Arial;text-align:center;background:#1e293b;color:white;margin:0;padding:10px}";
  html += ".card{display:inline-block;width:280px;margin:10px;padding:12px;background:#0f172a;border-radius:12px;box-shadow: 0 4px 6px rgba(0,0,0,0.3);}";
  html += "#hist{max-height:260px;overflow-y:auto;margin:10px auto;width:95%; border-radius: 8px;}";
  html += "table{width:100%;border-collapse:collapse;font-size:12px;background:#0f172a;}";
  html += "th,td{border-bottom:1px solid #334155;padding:8px}";
  html += "th{position:sticky;top:0;background:#020617; color:#38bdf8;}";
  html += "</style></head><body><h1>Central PTAP (Monitoreo HELMO)</h1>";

  // Inyección dinámica de variables globales en el HTML (Cajas de Mando)
  html += "<div class='card'><h3>🌊 Turbidez</h3>";
  html += "NTU: <b>" + String(turb_ntu, 1) + "</b><br>";
  html += "RAW: <span style='color:gray;'>" + String(turb_raw, 0) + "</span></div>";

  html += "<div class='card'><h3>🧪 pH</h3>";
  html += "<b style='font-size:24px;'>" + String(ph_val, 2) + "</b></div>";

  html += "<div class='card'><h3>📏 Nivel</h3>";
  html += "Altura: <b>" + String(altura_real, 1) + " cm</b><br>";
  html += "<span style='color:gray;'>Distancia Sensor: " + String(dist_cm, 1) + " cm</span></div>";

  // Bloque de estado general del sistema (Output de la lógica condicional)
  html += "<h2 style='color:" + String(estadoGlobal == "OPTIMA" ? "#4ade80" : "#f87171") + ";'>" + estadoGlobal + "</h2>";
  html += "<p style='font-size:18px;'>" + accionRecomendada + "</p>";

  // Estado de los actuadores físicos
  html += "<p style='color:#94a3b8;'>Estatus Hardware -> Servo: " + String(servoTurbidez.read()) + "° | LED Alarma: ";
  html += digitalRead(LED_NIVEL_BAJO) ? "<span style='color:red;'>ON</span>" : "OFF";
  html += "</p>";

  // Generación dinámica de la tabla consultando la memoria RAM (Structs)
  html += "<h3>Últimos paquetes procesados en red LoRa</h3>";
  html += "<div id='hist'><table>";
  html += "<tr><th>Iteración</th><th>Agente Origen</th><th>NTU</th><th>RAW</th><th>Alt(cm)</th><th>Dist(cm)</th><th>pH</th></tr>";

  // Bucle de renderizado inverso para mostrar el dato más nuevo en la parte superior
  int printed = 0;
  for (int i = 0; i < hist_count; i++) {
    int idx = (hist_idx - 1 - i + HIST_MAX) % HIST_MAX; // Navegación de la cola circular
    const Registro& r = hist[idx];

    html += "<tr>";
    html += "<td>#" + String(hist_count - i) + "</td>"; // Conteo progresivo inverso
    
    // Traducción legible del ID numérico del Agente
    if (r.sensor_id == 0) html += "<td style='color:#38bdf8;'>Turbidez</td>";
    else if (r.sensor_id == 1) html += "<td style='color:#a78bfa;'>Nivel</td>";
    else if (r.sensor_id == 2) html += "<td style='color:#4ade80;'>pH</td>";
    else html += "<td>Desconocido</td>";

    // Vaciado de celdas de datos con precisión controlada
    html += "<td>" + String(r.turb_ntu_, 1) + "</td>";
    html += "<td>" + String(r.turb_raw_, 0) + "</td>";
    html += "<td>" + String(r.altura_, 1) + "</td>";
    html += "<td>" + String(r.dist_cm_, 1) + "</td>";
    html += "<td>" + String(r.ph_, 2) + "</td>";
    html += "</tr>";

    printed++;
    if (printed >= 40) break; // Límite de seguridad visual
  }

  html += "</table></div>";
  html += "</body></html>";

  // Despacho del protocolo HTTP Code 200 OK con el HTML empaquetado
  server.send(200, "text/html", html);
}

// ============ ALGORITMOS DE EDGE COMPUTING (Ventana Móvil) ============
// Incorpora un nuevo dato al array histórico para el modelo de ML local
void actualizarML_Turbidez(float nueva_lectura) {
  turb_ventana[turb_idx] = nueva_lectura;
  turb_idx = (turb_idx + 1) % ML_VENTANA_TURB;
}

// Lógica de predicción rudimentaria basada en tasas de cambio
float calcularPrediccion() {
  float suma = 0;
  int validas = 0;
  
  // Limpieza de nulos
  for (int i = 0; i < ML_VENTANA_TURB; i++) {
    if (turb_ventana[i] > 0) {
      suma += turb_ventana[i];
      validas++;
    }
  }
  if (validas == 0) return turb_ntu; // Si no hay data suficiente, retorna el valor actual

  // Cálculo del promedio histórico
  float prom = suma / validas;
  
  // Cálculo diferencial entre el instante actual (idx) y 5 ventanas atrás
  int idx_pasado = (turb_idx - 5 + ML_VENTANA_TURB) % ML_VENTANA_TURB;
  float tendencia = (turb_ventana[turb_idx] - turb_ventana[idx_pasado]) / 5.0;

  // Extrapolación lineal hacia el futuro
  return prom + tendencia * ML_HORAS_ADELANTE;
}