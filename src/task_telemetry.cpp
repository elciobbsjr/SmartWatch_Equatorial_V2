#include <Arduino.h>

#include "task_telemetry.h"
#include "telemetry.h"
#include "app_config.h"
#include "wifi_mgr.h"
#include "mqtt_mgr.h"
#include "http_mgr.h"

extern SemaphoreHandle_t g_telemetryMutex;


void task_telemetry(void *pvParameters)
{
    Serial.println("[CLOUD] Tarefa de Nuvem e Telemetria iniciada!");

    uint32_t lastPublishTime = 0;
    uint32_t lastHttpTime = 0;

    const uint32_t PUBLISH_INTERVAL_MS = 2000;
    const uint32_t HTTP_INTERVAL_MS = 5000;
    const uint32_t MQTT_RETRY_INTERVAL = 3000;

    while (true)
    {
        //--------------------------------------------------
        // 1. GARANTE CONEXÃO WIFI
        //--------------------------------------------------
        if (!wifi_is_connected())
        {
            Serial.println("[CLOUD] Conectando WiFi...");

            if (!wifi_connect())
            {
                Serial.println("[CLOUD] Falha WiFi");

                vTaskDelay(pdMS_TO_TICKS(3000));

                continue;
            }

            Serial.println("[CLOUD] WiFi OK");
        }

        //--------------------------------------------------
        // 2. GARANTE CONEXÃO MQTT
        //--------------------------------------------------
        if (!mqtt_is_connected())
        {
            Serial.println("[CLOUD] MQTT desconectado...");

            if (mqtt_connect())
            {
                Serial.println("[CLOUD] MQTT conectado com sucesso!");
            }
            else
            {
                Serial.println("[CLOUD] Falha MQTT -> aguardando nova tentativa...");
            }

            vTaskDelay(pdMS_TO_TICKS(MQTT_RETRY_INTERVAL));

            continue;
        }

        //--------------------------------------------------
        // 3. MANTÉM MQTT ATIVO
        //--------------------------------------------------
        mqtt_loop();

        //--------------------------------------------------
        // 4. ENVIA TELEMETRIA
        //--------------------------------------------------
        uint32_t now = millis();

        if (now - lastPublishTime >= PUBLISH_INTERVAL_MS)
        {
            telemetry_data_t copy;

            if (xSemaphoreTake(g_telemetryMutex, pdMS_TO_TICKS(10)))
            {
                copy = g_telemetry;

                xSemaphoreGive(g_telemetryMutex);

                char payload[300];

                snprintf(
                    payload,
                    sizeof(payload),

                    "{\"pitch\":%.1f,"
                    "\"roll\":%.1f,"
                    "\"accMag\":%.2f,"
                    "\"gyroMag\":%.1f,"
                    "\"jerk\":%.1f,"
                    "\"state\":%d,"
                    "\"bpm\":%.1f,"
                    "\"spo2\":%.1f,"
                    "\"temp\":%.2f}",

                    copy.pitch,
                    copy.roll,
                    copy.accMag,
                    copy.gyroMag,
                    copy.jerk,
                    copy.state_beta1,
                    copy.bpm,
                    copy.spo2,
                    copy.body_temp
                );

                //--------------------------------------------------
                // MQTT
                //--------------------------------------------------
                if (mqtt_publish("smartwatch/dados", payload))
                {
                    Serial.println("[MQTT] enviado:");
                    Serial.println(payload);
                }
                else
                {
                    Serial.println("[MQTT] falha envio");
                }

                //--------------------------------------------------
                // GOOGLE SHEETS
                //--------------------------------------------------
                if (now - lastHttpTime >= HTTP_INTERVAL_MS)
                {
                    if (http_send_telemetry(payload))
                    {
                        Serial.println("[HTTP] enviado p/ planilha");
                    }
                    else
                    {
                        Serial.println("[HTTP] erro envio");
                    }

                    lastHttpTime = now;
                }
            }

            lastPublishTime = now;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}