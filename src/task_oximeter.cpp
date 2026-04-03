#include <Arduino.h>
#include <Wire.h>
#include "MAX30105.h" 
#include "task_oximeter.h"
#include "app_config.h"
#include "i2c_mgr.h"

static MAX30105 sensor;

void task_oximeter(void *pvParameters) {
    Serial.println("[OXI] Tarefa de Aquisicao iniciada. Configurando sensor...");

    bool sensorLigado = false;

    // Inicializa o sensor protegendo o I2C
    if (xSemaphoreTake(g_i2cMutex, pdMS_TO_TICKS(1000))) {
        if (sensor.begin(Wire, I2C_SPEED_STANDARD)) {
            sensor.setup(
                80,   // ledBrightness: Abaixamos mais! 120 ainda bateu no teto. Vamos tentar 80.
                8,    // sampleAverage: Média de 8 amostras (fundamental para o braço)
                2,    // mode: 2 = RED + IR
                200,  // sampleRate: 200 Hz
                411,  // pulseWidth: 411
                16384 // adcRange: 16384
            );
            sensor.clearFIFO();
            sensorLigado = true;
            Serial.println("[OXI] Sensor MAX30105 pronto (Lendo IR e RED)!");
        } else {
            Serial.println("[OXI] ERRO: MAX30105 nao encontrado.");
        }
        xSemaphoreGive(g_i2cMutex);
    }

    if (!sensorLigado) {
        vTaskDelete(NULL); 
    }

    uint32_t contadorDebug = 0;

    // Loop de Aquisição Contínua
    while (true) {
        if (xSemaphoreTake(g_i2cMutex, pdMS_TO_TICKS(10))) {
            
            sensor.check(); // Puxa dados novos para o buffer
            
            while (sensor.available()) {
                // 1. LÊ OS DADOS BRUTOS (IR e RED)
                ppg_sample_t sample;
                sample.ir = sensor.getIR();
                sample.red = sensor.getRed();
                sample.timestamp = millis();
                
                sensor.nextSample(); 

                // 2. ENVIA PARA A FILA (Para a futura task de processamento)
                if (g_ppgHeartQueue != NULL) {
                    xQueueSend(g_ppgHeartQueue, &sample, 0);
                }

                // 3. DEBUG: Mostra os dois valores na tela a cada ~1 segundo
                contadorDebug++;
                if (contadorDebug >= 50) {
                    Serial.printf("[OXI-RAW] IR: %u | RED: %u\n", sample.ir, sample.red);
                    contadorDebug = 0;
                }
            }
            xSemaphoreGive(g_i2cMutex);
        }

        // Taxa de amostragem cravada em 50Hz (20ms)
        vTaskDelay(pdMS_TO_TICKS(OXIMETER_SAMPLE_RATE_MS));
    }
}