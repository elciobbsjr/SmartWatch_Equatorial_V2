#include <Arduino.h>
#include <Wire.h>
#include "task_temp.h"
#include "app_config.h"
#include "i2c_mgr.h"
#include "telemetry.h"

extern SemaphoreHandle_t g_i2cMutex;
extern SemaphoreHandle_t g_telemetryMutex;

#define AHT10_ADDR 0x38

// ==========================================
// DRIVER AHT10 (Convertido para ESP32-Wire)
// ==========================================

static bool aht10_init() {
    bool sucesso = false;
    
    if (xSemaphoreTake(g_i2cMutex, pdMS_TO_TICKS(50))) {
        Wire.beginTransmission(AHT10_ADDR);
        Wire.write(0xE1); // Comando Init
        Wire.write(0x08);
        Wire.write(0x00);
        
        if (Wire.endTransmission() == 0) {
            sucesso = true;
        }
        xSemaphoreGive(g_i2cMutex);
    }
    
    if (sucesso) {
        vTaskDelay(pdMS_TO_TICKS(20)); // Espera inicialização
    }
    return sucesso;
}

static bool aht10_read_data(float *tempOut, float *humOut) {
    bool sucesso = false;

    // 1. Disparar Medição
    if (xSemaphoreTake(g_i2cMutex, pdMS_TO_TICKS(50))) {
        Wire.beginTransmission(AHT10_ADDR);
        Wire.write(0xAC); // Comando Measure
        Wire.write(0x33);
        Wire.write(0x00);
        Wire.endTransmission();
        xSemaphoreGive(g_i2cMutex);
    } else {
        return false; // MPU6500 ocupando, aborta
    }

    // 2. Esperar conversão
    vTaskDelay(pdMS_TO_TICKS(80));

    // 3. Ler os 6 bytes
    if (xSemaphoreTake(g_i2cMutex, pdMS_TO_TICKS(50))) {
        if (Wire.requestFrom((uint8_t)AHT10_ADDR, (size_t)6) == 6) {
            uint8_t buf[6];
            for (int i = 0; i < 6; i++) {
                buf[i] = Wire.read();
            }

            // 4. Checa status
            if ((buf[0] & 0x88) == 0x08) {
                uint32_t raw_humidity = ((uint32_t)buf[1] << 12) | ((uint32_t)buf[2] << 4) | (buf[3] >> 4);
                *humOut = ((float)raw_humidity / 1048576.0f) * 100.0f;

                uint32_t raw_temp = (((uint32_t)buf[3] & 0x0F) << 16) | ((uint32_t)buf[4] << 8) | buf[5];
                *tempOut = (((float)raw_temp / 1048576.0f) * 200.0f) - 50.0f;

                sucesso = true;
            }
        }
        xSemaphoreGive(g_i2cMutex);
    }

    return sucesso;
}

// ==========================================
// TAREFA FREERTOS (Com trava de proteção)
// ==========================================

void task_temp(void *pvParameters) {
    Serial.println("[TEMP] Tarefa de Temperatura (AHT10) iniciada!");

    bool sensor_ok = aht10_init();

    if (!sensor_ok) {
        Serial.println("[TEMP] ERRO CRITICO: AHT10 nao encontrado no endereco 0x38.");
        Serial.println("[TEMP] Tarefa suspensa para proteger o barramento do MPU6500.");
        
        // Se der erro, a tarefa entra em um loop infinito "vazio" e nunca mais usa o I2C
        while(true) {
            vTaskDelay(pdMS_TO_TICKS(10000));
        }
    } 

    Serial.println("[TEMP] AHT10 inicializado com sucesso!");

    while (true) {
        float temp = 0.0f;
        float hum = 0.0f; 
        
        if (aht10_read_data(&temp, &hum)) {
            if (g_telemetryMutex != NULL) {
                if (xSemaphoreTake(g_telemetryMutex, pdMS_TO_TICKS(10))) {
                    g_telemetry.body_temp = temp;
                    xSemaphoreGive(g_telemetryMutex);
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(2000)); 
    }
}