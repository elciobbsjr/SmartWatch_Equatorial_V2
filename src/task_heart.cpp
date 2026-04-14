#include <Arduino.h>
#include "task_heart.h"
#include "app_config.h"
#include "telemetry.h"

extern telemetry_data_t g_telemetry;
extern SemaphoreHandle_t g_telemetryMutex;
extern QueueHandle_t g_ppgHeartQueue;

#define RATE_SIZE 8

static float rates[RATE_SIZE];
static byte rateSpot = 0;

static float smoothedBPM = 0;

static const float alphaFast = 0.3f;
static const float alphaSlow = 0.1f;

static const uint32_t fingerThreshold = 20000;

// =====================================================
// variáveis DSP
// =====================================================

static float w = 0;
static long lastBeat = 0;

static bool beat_armed = false;

static float peak_ac = 0.0f;

// =====================================================
// TASK
// =====================================================

void task_heart(void *pvParameters)
{
    Serial.println("[HEART] Motor DSP V5 iniciado!");

    ppg_sample_t sample;

    int validBeatCount = 0;
    float irAvg = 0;

    while (true)
    {
        if (xQueueReceive(g_ppgHeartQueue, &sample, portMAX_DELAY))
        {
            float irValue = (float)sample.ir;

            //--------------------------------------------------
            // média do sinal IR
            //--------------------------------------------------
            if (irAvg == 0)
                irAvg = irValue;
            else
                irAvg = (irAvg * 0.95f) + (irValue * 0.05f);

            //--------------------------------------------------
            // dedo não está no sensor
            //--------------------------------------------------
            if (irAvg < fingerThreshold)
            {
                smoothedBPM = 0;
                validBeatCount = 0;
                w = 0;
                beat_armed = false;
                lastBeat = 0;
                peak_ac = 0;

                if (xSemaphoreTake(g_telemetryMutex, pdMS_TO_TICKS(5)))
                {
                    g_telemetry.bpm = 0;
                    g_telemetry.spo2 = 0;
                    xSemaphoreGive(g_telemetryMutex);
                }

                continue;
            }

            //--------------------------------------------------
            // filtro digital (DSP)
            //--------------------------------------------------
            float prev_w = w;
            w = irValue + (0.95f * prev_w);

            float ac_signal = prev_w - w;

            static float ac_filtered = 0;
            ac_filtered = (ac_filtered * 0.8f) + (ac_signal * 0.2f);

            //--------------------------------------------------
            // detector de pico adaptativo
            //--------------------------------------------------
            if (ac_filtered > peak_ac)
                peak_ac = ac_filtered;

            peak_ac *= 0.995f;

            float threshold = peak_ac * 0.5f;

            if (threshold < 0.1f)
                threshold = 0.1f;

            bool beat_detected = false;

            if (ac_filtered > threshold && !beat_armed)
            {
                beat_armed = true;
            }

            if (ac_filtered < 0.0f && beat_armed)
            {
                beat_armed = false;
                beat_detected = true;
                Serial.println(">>> BATIMENTO DETECTADO <<<");
            }

            //--------------------------------------------------
            // calcula BPM
            //--------------------------------------------------
            if (beat_detected)
            {
                long now = millis();

                if (lastBeat == 0)
                {
                    lastBeat = now;
                    continue;
                }

                long delta = now - lastBeat;
                lastBeat = now;

                if (delta < 300 || delta > 2000)
                {
                    Serial.printf("delta rejeitado: %ld ms\n", delta);
                    continue;
                }

                float rawBPM = 60.0f / (delta / 1000.0f);

                if (rawBPM < 30 || rawBPM > 200)
                    continue;

                //--------------------------------------------------
                // suavização
                //--------------------------------------------------
                if (validBeatCount == 0)
                {
                    for (byte i = 0; i < RATE_SIZE; i++)
                        rates[i] = rawBPM;

                    smoothedBPM = rawBPM;
                }
                else
                {
                    rates[rateSpot++] = rawBPM;
                    rateSpot %= RATE_SIZE;
                }

                float avgBPM = 0;

                for (byte i = 0; i < RATE_SIZE; i++)
                    avgBPM += rates[i];

                avgBPM /= RATE_SIZE;

                float variation = abs(avgBPM - smoothedBPM);

                float alpha =
                    (variation < 5) ? alphaFast :
                    ((variation < 15) ? alphaSlow : 0);

                if (alpha > 0)
                {
                    smoothedBPM =
                        alpha * avgBPM +
                        (1 - alpha) * smoothedBPM;
                }

                validBeatCount++;

                //--------------------------------------------------
                // PRINT DEBUG
                //--------------------------------------------------
                Serial.printf(
                    "[HEART] BPM: %.1f (raw %.1f)\n",
                    smoothedBPM,
                    rawBPM
                );

                //--------------------------------------------------
                // registra último batimento válido
                //--------------------------------------------------
                g_lastHeartbeatMs = now;

                //--------------------------------------------------
                // ATUALIZA TELEMETRIA GLOBAL
                //--------------------------------------------------
                if (xSemaphoreTake(g_telemetryMutex, pdMS_TO_TICKS(5)))
                {
                    g_telemetry.bpm = smoothedBPM;
                    g_telemetry.spo2 = 98;
                    xSemaphoreGive(g_telemetryMutex);
                }
            }
        }
    }
}