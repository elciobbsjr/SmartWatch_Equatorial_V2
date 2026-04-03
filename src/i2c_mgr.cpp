#include <Arduino.h>
#include <Wire.h>
#include "i2c_mgr.h"
#include "app_config.h"

SemaphoreHandle_t g_i2cMutex = NULL;

void i2c_mgr_init()
{
    Serial.println("[I2C_MGR] Inicializando barramento I2C...");

    // inicializa barramento I2C nos pinos definidos
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

    // AHT10 funciona melhor em 100kHz
    Wire.setClock(I2C_FREQ_HZ);

    // pequeno delay para estabilizar barramento
    delay(50);

    // cria mutex de proteção para múltiplas tasks
    g_i2cMutex = xSemaphoreCreateMutex();

    if (g_i2cMutex == NULL)
    {
        Serial.println("[I2C_MGR] ERRO FATAL: nao foi possivel criar mutex I2C");

        while (true)
        {
            delay(100);
        }
    }

    Serial.print("[I2C_MGR] SDA: ");
    Serial.println(I2C_SDA_PIN);

    Serial.print("[I2C_MGR] SCL: ");
    Serial.println(I2C_SCL_PIN);

    Serial.print("[I2C_MGR] Clock: ");
    Serial.println(I2C_FREQ_HZ);

    Serial.println("[I2C_MGR] Barramento inicializado com sucesso!");
}