#include "i2c_mgr.h"
#include "app_config.h"

SemaphoreHandle_t g_i2cMutex = NULL;

void i2c_mgr_init() {
    // 1. Cria o Mutex de forma segura
    g_i2cMutex = xSemaphoreCreateMutex();
    if (g_i2cMutex == NULL) {
        Serial.println("[I2C_MGR] ERRO FATAL: Falha ao criar o Mutex I2C!");
        while (1) delay(10); // Trava tudo se não conseguir criar a trava
    }

    // 2. Configura e trava os pinos no barramento
    Wire.setPins(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(I2C_FREQ_HZ);

    Serial.println("[I2C_MGR] Barramento inicializado e Mutex criado.");
}