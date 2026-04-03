#include <Arduino.h>
#include <Wire.h>
#include "task_mpu.h"
#include "app_config.h"
#include "i2c_mgr.h"

#define MPU_ADDR        0x68
#define REG_PWR_MGMT_1  0x6B
#define REG_WHO_AM_I    0x75
#define REG_ACCEL_XOUT  0x3B

static bool mpu_write_reg(uint8_t reg, uint8_t val) {
    bool sucesso = false;
    if (xSemaphoreTake(g_i2cMutex, pdMS_TO_TICKS(50))) {
        Wire.beginTransmission(MPU_ADDR);
        Wire.write(reg);
        Wire.write(val);
        sucesso = (Wire.endTransmission(true) == 0);
        xSemaphoreGive(g_i2cMutex);
    }
    return sucesso;
}

static bool mpu_read_bytes(uint8_t reg, uint8_t *buf, size_t len) {
    bool sucesso = false;
    if (xSemaphoreTake(g_i2cMutex, pdMS_TO_TICKS(50))) {
        Wire.beginTransmission(MPU_ADDR);
        Wire.write(reg);
        if (Wire.endTransmission(false) == 0) { 
            if (Wire.requestFrom((uint8_t)MPU_ADDR, len, true) == len) {
                for (size_t i = 0; i < len; i++) {
                    buf[i] = Wire.read();
                }
                sucesso = true;
            }
        }
        xSemaphoreGive(g_i2cMutex);
    }
    return sucesso;
}

void task_mpu(void *pvParameters) {
    Serial.println("[MPU-DRIVER] Tarefa de Aquisicao iniciada...");

    bool iniciado = false;
    for (int i = 0; i < 5; i++) {
        if (mpu_write_reg(REG_PWR_MGMT_1, 0x00)) {
            iniciado = true;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    if (!iniciado) {
        Serial.println("[MPU-DRIVER] ERRO FATAL: MPU6500 nao respondeu.");
        vTaskDelete(NULL);
    }

    Serial.println("[MPU-DRIVER] Sensor MPU6500 pronto! Despachando dados...");

    uint8_t rawData[14];

    while (true) {
        if (mpu_read_bytes(REG_ACCEL_XOUT, rawData, 14)) {
            
            // Empacota os dados
            mpu_sample_t sample;
            sample.ax = (rawData[0] << 8) | rawData[1];
            sample.ay = (rawData[2] << 8) | rawData[3];
            sample.az = (rawData[4] << 8) | rawData[5];

            sample.gx = (rawData[8] << 8) | rawData[9];
            sample.gy = (rawData[10] << 8) | rawData[11];
            sample.gz = (rawData[12] << 8) | rawData[13];
            sample.timestamp = millis();

            // Despacha para a Fila de Processamento
            if (g_mpuQueue != NULL) {
                xQueueSend(g_mpuQueue, &sample, 0);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20)); // Amostragem cravada em 50Hz
    }
}