// ===== MYSQL + ML EMBEBIDO HELMO =====
// Archivo: mysql_helpers.cpp  
// Propósito: Guardar datos MySQL + predicciones básicas ML para control loop abierto

#include <Arduino.h>
#include <MySQL_Connection.h>
#include <MySQL_Cursor.h>
#include "config_central.h"

// 📊 Variables globales para ML (accesibles desde .ino)
float turb_ventana[ML_VENTANA_TURB];           // Buffer ventana móvil turbidez
int turb_idx = 0;                              // Índice actual ventana
float turb_prediccion = 0;                     // Predicción próxima hora
bool mysql_conectado = false;                  // Estado conexión DB

// Objetos MySQL
WiFiClient client_mysql;
MySQL_Connection conn((Client *)&client_mysql);
MySQL_Cursor *cursor;

// 🗄️ CONECTAR MYSQL (usa credenciales de config_central.h)
bool conectarMySQL() {
  Serial.println("🗄️ Conectando MySQL...");
  
  // Configurar conexión con credenciales seguras
  conn.begin(MYSQL_SERVER, 3306);              // Puerto MySQL estándar
  conn.connect(MYSQL_USER, MYSQL_PASS, MYSQL_DB);
  
  if (conn.connected()) {
    Serial.println("✓ MySQL conectado");
    mysql_conectado = true;
    
    // Crear tabla si no existe (automático)
    cursor = new MySQL_Cursor(&conn);
    cursor->execute("CREATE TABLE IF NOT EXISTS sensores ("
                    "id INT AUTO_INCREMENT PRIMARY KEY, "
                    "sensor_id INT, valor FLOAT, raw_val FLOAT, "
                    "dist_cm FLOAT, altura_real FLOAT, tendencia FLOAT, "
                    "prediccion FLOAT, estado VARCHAR(20), "
                    "fecha DATETIME DEFAULT CURRENT_TIMESTAMP)");
    delete cursor;
    
    return true;
  } else {
    Serial.println("✗ MySQL falló");
    return false;
  }
}

// 💾 GUARDAR LECTURA EN MYSQL (todos sensores)
// Reemplaza función guardarSensor() en mysql_helpers.cpp:
void guardarSensor(int sensor_id, float valor, float raw_val, 
                   float dist_cm, float altura_real, float tendencia) {
  if (!mysql_conectado) return;
  
  cursor = new MySQL_Cursor(&conn);
  
  // 🧠 Calcular ML
  actualizarML_Turbidez(valor);
  float prediccion = calcularPrediccion();
  
  // 📡 INSERT COMPLETO HELMO (todas columnas)
  char query[512];
  sprintf(query, 
    "INSERT INTO sensores (sensor_id, turbidez_ntu, turbidez_raw, "
    "nivel_dist_cm, nivel_altura_real, ph_valor, ml_prediccion_ntu, "
    "estado_sistema, accion_recomendada, rssi) "
    "VALUES (%d, %f, %f, %f, %f, %f, %f, '%s', '%s', %d)",
    sensor_id, valor, raw_val, dist_cm, altura_real, ph_val, 
    prediccion, estadoGlobal.c_str(), accionRecomendada.c_str(), 0);
  
  cursor->execute(query);
  delete cursor;
  Serial.println("💾 HELMO datos + ML guardados!");
}


// 🧠 ACTUALIZAR VENTANA MÓVIL TURBIDEZ (ML embebido)
void actualizarML_Turbidez(float nueva_lectura) {
  turb_ventana[turb_idx] = nueva_lectura;        // Agregar nueva lectura
  turb_idx = (turb_idx + 1) % ML_VENTANA_TURB;   // Circular buffer
}

// 🧠 CALCULAR PREDICCIÓN (promedio móvil + tendencia)
float calcularPrediccion() {
  float suma = 0;
  int muestras_validas = 0;
  
  // Sumar ventana móvil
  for (int i = 0; i < ML_VENTANA_TURB; i++) {
    if (turb_ventana[i] > 0) {                 // Solo valores válidos
      suma += turb_ventana[i];
      muestras_validas++;
    }
  }
  
  if (muestras_validas == 0) return 0;
  
  float promedio = suma / muestras_validas;      // Promedio base
  
  // Predicción simple: promedio + tendencia * horizonte
  float tendencia = (turb_ventana[turb_idx] - turb_ventana[(turb_idx+5)%ML_VENTANA_TURB]) / 5.0;
  turb_prediccion = promedio + (tendencia * ML_PREDICCION_HORAS);
  
  return turb_prediccion;
}

// 🔄 DESCONECTAR MYSQL (liberar recursos)
void desconectarMySQL() {
  if (mysql_conectado) {
    conn.close();
    mysql_conectado = false;
    Serial.println("🔌 MySQL desconectado");
  }
}
