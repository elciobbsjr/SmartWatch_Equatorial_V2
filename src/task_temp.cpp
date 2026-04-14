#include <Arduino.h>
#include "task_temp.h"
#include "telemetry.h"

extern SemaphoreHandle_t g_telemetryMutex;

static float gerar_temperatura_fake(float bpm)
{
    float temp = 36.5f;

    if (bpm >= 110.0f)
        temp += 0.4f;
    else if (bpm >= 90.0f)
        temp += 0.2f;
    else if (bpm > 0.0f && bpm < 60.0f)
        temp -= 0.1f;

    float ruido = ((float)random(-15, 16)) / 100.0f;
    temp += ruido;

    if (temp < 36.1f) temp = 36.1f;
    if (temp > 37.3f) temp = 37.3f;

    return temp;
}

void task_temp(void *pvParameters)
{
    Serial.println("[TEMP] iniciando gerador de temperatura fake...");

    randomSeed(micros());

    const uint32_t INTERVALO_ENVIO_MS = 5UL * 60UL * 1000UL;
    const uint32_t JANELA_BATIMENTO_MS = 30000UL;

    uint32_t ultimo_envio_temp = 0;
    bool primeira_temp_enviada = false;

    while (true)
    {
        uint32_t agora = millis();
        bool batimento_recente = false;
        float bpm_atual = 0.0f;

        if ((agora - g_lastHeartbeatMs) <= JANELA_BATIMENTO_MS)
        {
            batimento_recente = true;
        }

        if (g_telemetryMutex != NULL)
        {
            if (xSemaphoreTake(g_telemetryMutex, pdMS_TO_TICKS(20)))
            {
                bpm_atual = g_telemetry.bpm;
                xSemaphoreGive(g_telemetryMutex);
            }
        }

        bool deve_atualizar = false;

        if (!primeira_temp_enviada && batimento_recente && bpm_atual > 0.0f)
        {
            deve_atualizar = true;
        }
        else if (primeira_temp_enviada &&
                 batimento_recente &&
                 bpm_atual > 0.0f &&
                 (agora - ultimo_envio_temp >= INTERVALO_ENVIO_MS))
        {
            deve_atualizar = true;
        }

        if (deve_atualizar)
        {
            float temp_fake = gerar_temperatura_fake(bpm_atual);

            if (g_telemetryMutex != NULL)
            {
                if (xSemaphoreTake(g_telemetryMutex, pdMS_TO_TICKS(20)))
                {
                    g_telemetry.body_temp = temp_fake;
                    xSemaphoreGive(g_telemetryMutex);
                }
            }

            ultimo_envio_temp = agora;
            primeira_temp_enviada = true;

            Serial.print("[TEMP] fake atualizada: ");
            Serial.print(temp_fake, 2);
            Serial.print(" C | BPM: ");
            Serial.println(bpm_atual, 1);
        }
        else
        {
            if (!batimento_recente || bpm_atual <= 0.0f)
            {
                Serial.println("[TEMP] sem batimento recente, temperatura nao atualizada");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}