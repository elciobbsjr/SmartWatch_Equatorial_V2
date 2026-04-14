#pragma once
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// ===== Pinos do ESP32-C3 Mini =====
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9
#define I2C_FREQ_HZ 25000

// ===== Tempos (FreeRTOS) =====
#define OXIMETER_SAMPLE_RATE_MS 20 // 50Hz

// ===== Configurações de Nuvem (Wi-Fi e MQTT) =====
#define WIFI_SSID "Galaxy A36"
#define WIFI_PASS "35246Jr@"
#define WIFI_CONNECT_TIMEOUT_MS 10000

#define MQTT_HOST "87a490b8ab324342ab84c48829e097ce.s1.eu.hivemq.cloud" // ex: broker.hivemq.com ou IP
#define MQTT_PORT 8883             // O seu código antigo usa Porta Segura (TLS/SSL)
#define MQTT_USER "elciobsjr"    // Se não tiver, deixe ""
#define MQTT_PASS "9Fsp2T!PKmKWRPr"      // Se não tiver, deixe ""


// ===== Estrutura de Dados do Oxímetro =====
// É aqui que definimos a "caixa" que vai guardar o IR e o RED juntos
typedef struct {
    uint32_t ir;
    uint32_t red;
    uint32_t timestamp;
} ppg_sample_t;

// ===== Declaração das Filas Globais =====
extern QueueHandle_t g_ppgHeartQueue;

// ===== Estrutura de Dados do MPU6500 =====
typedef struct {
    int16_t ax, ay, az;
    int16_t gx, gy, gz;
    uint32_t timestamp;
} mpu_sample_t;

// ===== Declaração das Filas Globais =====
extern QueueHandle_t g_ppgHeartQueue;
extern QueueHandle_t g_ppgSpo2Queue;
extern QueueHandle_t g_mpuQueue; // <-- NOVA FILA PARA O MPU