#include <Arduino.h>
#include "task_heart.h"
#include "app_config.h"

#define RATE_SIZE 8
static float rates[RATE_SIZE];
static byte rateSpot = 0;
static float smoothedBPM = 0;

static const float alphaFast = 0.3f;
static const float alphaSlow = 0.1f;
// LOWERED: Prevent false resets if the signal drops slightly
static const uint32_t fingerThreshold = 20000; 

// Variáveis do nosso Filtro Digital Avançado
static float w = 0;
static long lastBeat = 0;
static bool beat_armed = false;
static float peak_ac = 0.0f; 

void task_heart(void *pvParameters) {
    Serial.println("[HEART] Motor DSP V5 (Hyper-Aggressive) iniciado!");

    ppg_sample_t sample;
    int validBeatCount = 0;
    float lastDisplayedBPM = 0;
    uint32_t lastDisplayTime = 0;
    float irAvg = 0;

    while (true) {
        if (xQueueReceive(g_ppgHeartQueue, &sample, portMAX_DELAY)) {
            
            float irValue = (float)sample.ir;

            if (irAvg == 0) irAvg = irValue;
            else irAvg = (irAvg * 0.95f) + (irValue * 0.05f);
            
            if (irAvg < fingerThreshold) {
                smoothedBPM = 0;
                validBeatCount = 0;
                w = 0; 
                beat_armed = false;
                lastBeat = 0; 
                peak_ac = 0; 
                continue;
            }

            // =========================================================
            // FILTRO DIGITAL (DSP)
            // =========================================================
            float prev_w = w;
            w = irValue + (0.95f * prev_w);
            float ac_signal = prev_w - w; 

            static float ac_filtered = 0;
            ac_filtered = (ac_filtered * 0.8f) + (ac_signal * 0.2f);

            // --- GATILHO INTELIGENTE V5 ---
            if (ac_filtered > peak_ac) {
                peak_ac = ac_filtered;
            }
            peak_ac *= 0.995f; 

            // HYPER-SENSITIVE: Minimum threshold dropped to 0.1
            float threshold = peak_ac * 0.5f;
            if (threshold < 0.1f) threshold = 0.1f; 

            bool beat_detected = false;
            
            if (ac_filtered > threshold && !beat_armed) {
                beat_armed = true;
            }
            
            if (ac_filtered < 0.0f && beat_armed) {
                beat_armed = false;
                beat_detected = true;
                Serial.println(">>> BATIMENTO DETECTADO! <<<");
            }
            // =========================================================

            if (beat_detected) {
                long now = millis();
                
                if (lastBeat == 0) {
                    lastBeat = now;
                    continue; 
                }

                long delta = now - lastBeat;
                lastBeat = now;

                // WIDENED: Accept 30 BPM (2000ms) to 200 BPM (300ms)
                if (delta < 300 || delta > 2000) {
                     Serial.printf("    -> Delta rejeitado: %ld ms\n", delta);
                     continue; 
                }

                float rawBPM = 60.0f / (delta / 1000.0f);
                
                // WIDENED
                if (rawBPM < 30 || rawBPM > 200) continue;

                if (validBeatCount == 0) {
                    for (byte i = 0; i < RATE_SIZE; i++) rates[i] = rawBPM;
                    smoothedBPM = rawBPM;
                } else {
                    rates[rateSpot++] = rawBPM;
                    rateSpot %= RATE_SIZE;
                }

                float avgBPM = 0;
                for (byte i = 0; i < RATE_SIZE; i++) avgBPM += rates[i];
                avgBPM /= RATE_SIZE;

                float variation = abs(avgBPM - smoothedBPM);
                float alpha = (variation < 5) ? alphaFast : ((variation < 15) ? alphaSlow : 0);
                if (alpha > 0) smoothedBPM = alpha * avgBPM + (1 - alpha) * smoothedBPM;

                validBeatCount++;
                
                // DROPPED STABILITY CHECK: Print immediately
                if (validBeatCount < 1) continue; 

                uint32_t nowPrint = millis();
                // DROPPED DISPLAY THRESHOLD: Print every calculation
                if (true /*abs(smoothedBPM - lastDisplayedBPM) > 1.5f || (nowPrint - lastDisplayTime) > 3000*/) {
                    Serial.printf("[HEART] -> BPM Calculado: %.1f (Raw: %.1f)\n", smoothedBPM, rawBPM);
                    lastDisplayedBPM = smoothedBPM;
                    lastDisplayTime = nowPrint;
                }
            }
        }
    }
}