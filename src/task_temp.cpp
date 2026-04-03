#include <Arduino.h>
#include <Wire.h>
#include "task_temp.h"
#include "app_config.h"
#include "i2c_mgr.h"
#include "telemetry.h"

extern SemaphoreHandle_t g_i2cMutex;
extern SemaphoreHandle_t g_telemetryMutex;

#define AHT10_ADDR 0x38

static const uint8_t CMD_MEASURE[] = {0xAC, 0x33, 0x00};


// leitura universal AHT10/AHT20
static bool aht_read(float *temperature, float *humidity)
{
    bool ok = false;

    // envia comando de medição
    if (xSemaphoreTake(g_i2cMutex, pdMS_TO_TICKS(200)))
    {
        Wire.beginTransmission(AHT10_ADDR);

        Wire.write(CMD_MEASURE, sizeof(CMD_MEASURE));

        ok = (Wire.endTransmission() == 0);

        xSemaphoreGive(g_i2cMutex);
    }

    if (!ok)
        return false;

    vTaskDelay(pdMS_TO_TICKS(90));

    uint8_t buf[6];

    if (xSemaphoreTake(g_i2cMutex, pdMS_TO_TICKS(200)))
    {
        int n = Wire.requestFrom(AHT10_ADDR, 6);

        if (n == 6)
        {
            for (int i = 0; i < 6; i++)
                buf[i] = Wire.read();

            // verifica se sensor não está ocupado
            if ((buf[0] & 0x80) == 0)
            {
                uint32_t raw_humidity =
                    ((uint32_t)buf[1] << 12) |
                    ((uint32_t)buf[2] << 4) |
                    (buf[3] >> 4);

                uint32_t raw_temp =
                    (((uint32_t)buf[3] & 0x0F) << 16) |
                    ((uint32_t)buf[4] << 8) |
                    buf[5];

                *humidity =
                    ((float)raw_humidity / 1048576.0f) * 100.0f;

                *temperature =
                    ((float)raw_temp / 1048576.0f) * 200.0f - 50.0f;

                ok = true;
            }
        }

        xSemaphoreGive(g_i2cMutex);
    }

    return ok;
}



void task_temp(void *pvParameters)
{
    Serial.println("[TEMP] iniciando sensor AHT...");

    // espera sensor estabilizar
    vTaskDelay(pdMS_TO_TICKS(1500));

    Serial.println("[TEMP] pronto para leitura");



    while (true)
    {
        float temp = 0;
        float hum  = 0;

        bool ok = aht_read(&temp, &hum);

        if (ok)
        {
            if (g_telemetryMutex != NULL)
            {
                if (xSemaphoreTake(g_telemetryMutex, pdMS_TO_TICKS(20)))
                {
                    g_telemetry.body_temp = temp;

                    xSemaphoreGive(g_telemetryMutex);
                }
            }

            Serial.print("[TEMP] ");
            Serial.print(temp);
            Serial.print(" C | ");
            Serial.print(hum);
            Serial.println(" %");
        }
        else
        {
            Serial.println("[TEMP] aguardando dados...");
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}