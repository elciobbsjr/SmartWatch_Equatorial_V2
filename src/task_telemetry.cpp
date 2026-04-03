#include <Arduino.h>
#include "task_telemetry.h"
#include "telemetry.h"
#include "app_config.h"
#include "wifi_mgr.h"
#include "mqtt_mgr.h"

extern SemaphoreHandle_t g_telemetryMutex;

void task_telemetry(void *pvParameters) {
    Serial.println("[CLOUD] Tarefa de Nuvem e Telemetria iniciada!");

    uint32_t lastPublishTime = 0;
    const uint32_t PUBLISH_INTERVAL_MS = 2000; // Envia pro MQTT a cada 2 segundos

    while (true) {
        // 1. GARANTE A CONEXÃO WI-FI (Sem travar o resto do sistema!)
        if (!wifi_is_connected()) {
            Serial.println("[CLOUD] Wi-Fi desconectado. Tentando reconectar...");
            wifi_connect();
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue; // Volta pro início do loop
        }

        // 2. GARANTE A CONEXÃO MQTT
        if (!mqtt_is_connected()) {
            Serial.println("[CLOUD] MQTT desconectado. Tentando reconectar...");
            if (mqtt_connect()) {
                 Serial.println("[CLOUD] MQTT Conectado com Sucesso!");
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue; // Volta pro início
        }

        // 3. MANTÉM A COMUNICAÇÃO VIVA
        mqtt_loop();

        // 4. PUBLICA OS DADOS PERIODICAMENTE
        uint32_t now = millis();
        if (now - lastPublishTime > PUBLISH_INTERVAL_MS) {
            
            telemetry_data_t copy;

            // Pega o Mutex rapidinho só para fazer uma cópia dos dados atuais
            if (xSemaphoreTake(g_telemetryMutex, pdMS_TO_TICKS(10))) {
                copy = g_telemetry;
                xSemaphoreGive(g_telemetryMutex);

                // Monta a string JSON (Leve e rápido)
                char payload[256];
                snprintf(payload, sizeof(payload),
                    "{\"pitch\":%.1f,\"roll\":%.1f,\"accMag\":%.2f,\"gyroMag\":%.1f,\"jerk\":%.1f,\"state\":%d,\"bpm\":%.1f,\"spo2\":%.1f}",
                    copy.pitch, copy.roll, copy.accMag, copy.gyroMag, copy.jerk, copy.state_beta1, copy.bpm, copy.spo2);

                // Envia para o tópico do seu projeto antigo
                if (mqtt_publish("smartwatch/dados", payload)) {
                    //Serial.println("[CLOUD] Pacote enviado para a nuvem!");
                }
            }
            lastPublishTime = now;
        }

        // Pausa curta para deixar o processador respirar e rodar outras coisas
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}