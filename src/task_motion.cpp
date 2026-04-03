#include <Arduino.h>
#include <math.h>
#include "task_motion.h"
#include "app_config.h"
#include "telemetry.h" // A sua telemetria volta aqui!

// O Mutex e a variável global da telemetria (que devem estar declarados no seu main ou telemetria.cpp)
extern SemaphoreHandle_t g_telemetryMutex;

// =========================
// CONFIGURAÇÃO DOS LIMITES
// =========================
#define SAMPLE_RATE_MS 20 // 50Hz vindo do Driver

#define FREE_FALL_THRESHOLD_G   0.85
#define IMPACT_THRESHOLD_G      1.4
#define JERK_THRESHOLD          3.0
#define GYRO_THRESHOLD          80.0

#define IMMOBILITY_TIME_MS      3000
#define IMMOBILITY_GYRO_LIMIT   6.0

#define EVENT_BUFFER_SIZE       200

// =========================
// Estrutura de Evento
// =========================
typedef struct {
    unsigned long timestamp;
    float pitch;
    float roll;
    float accMag;
    float gyroMag;
    float jerk;
    int state;
} imu_event_t;

static imu_event_t eventBuffer[EVENT_BUFFER_SIZE];
static int eventIndex = 0;

// =========================
// Variáveis Globais de Motion
// =========================
static float gyro_offset_x = 0, gyro_offset_y = 0, gyro_offset_z = 0;
static float acc_offset_x = 0, acc_offset_y = 0, acc_offset_z = 0;

enum FallState {
    NORMAL,
    FREE_FALL,
    IMPACT,
    IMMOBILITY_CHECK
};

static FallState fallState = NORMAL;
static FallState previousState = NORMAL;
static unsigned long freeFallStart = 0;
static unsigned long immobilityStart = 0;

// =========================
// Função de Armazenar Evento
// =========================
static void store_event(float pitch, float roll, float accMag, float gyroMag, float jerk, int state) {
    eventBuffer[eventIndex].timestamp = millis();
    eventBuffer[eventIndex].pitch = pitch;
    eventBuffer[eventIndex].roll = roll;
    eventBuffer[eventIndex].accMag = accMag;
    eventBuffer[eventIndex].gyroMag = gyroMag;
    eventBuffer[eventIndex].jerk = jerk;
    eventBuffer[eventIndex].state = state;

    eventIndex++;
    if (eventIndex >= EVENT_BUFFER_SIZE) eventIndex = 0;
}

// =========================
// TASK PRINCIPAL (O Cérebro)
// =========================
void task_motion(void *pvParameters) {
    Serial.println("[MOTION] Tarefa de Processamento (Algoritmo Original) iniciada!");

    mpu_sample_t sample;
    
    // ==========================================
    // 1. FASE DE CALIBRAÇÃO (Consumindo a Fila)
    // ==========================================
    Serial.println("=== CALIBRANDO MPU (Aguarde 5 seg) ===");
    const int amostrasCalibracao = 250; // 250 amostras a 50Hz = 5 segundos
    int ok = 0;
    float sum_gx=0, sum_gy=0, sum_gz=0;
    float sum_ax=0, sum_ay=0, sum_az=0;

    while (ok < amostrasCalibracao) {
        if (xQueueReceive(g_mpuQueue, &sample, portMAX_DELAY)) {
            // Converte pra float (física real)
            float ax = sample.ax / 16384.0f;
            float ay = sample.ay / 16384.0f;
            float az = sample.az / 16384.0f;
            float gx = sample.gx / 131.0f;
            float gy = sample.gy / 131.0f;
            float gz = sample.gz / 131.0f;

            sum_ax += ax; sum_ay += ay; sum_az += az;
            sum_gx += gx; sum_gy += gy; sum_gz += gz;
            ok++;
        }
    }

    gyro_offset_x = sum_gx / amostrasCalibracao;
    gyro_offset_y = sum_gy / amostrasCalibracao;
    gyro_offset_z = sum_gz / amostrasCalibracao;
    acc_offset_x = sum_ax / amostrasCalibracao;
    acc_offset_y = sum_ay / amostrasCalibracao;
    acc_offset_z = (sum_az / amostrasCalibracao) - 1.0f; // Tira 1G do eixo Z (Gravidade)

    Serial.println("[MOTION] Calibracao concluida com sucesso!");
    // g_systemReady = true; // (Descomente se tiver essa flag no seu config)

    // ==========================================
    // 2. LOOP PRINCIPAL DE QUEDA LIVRE
    // ==========================================
    float pitch = 0, roll = 0, prevAccMag = 1.0;
    unsigned long lastTime = millis();

    while (true) {
        // Pega os dados mais novos na fila
        if (xQueueReceive(g_mpuQueue, &sample, portMAX_DELAY)) {
            
            // Converte e aplica Offsets de calibração
            float d_ax = (sample.ax / 16384.0f) - acc_offset_x;
            float d_ay = (sample.ay / 16384.0f) - acc_offset_y;
            float d_az = (sample.az / 16384.0f) - acc_offset_z;
            
            float d_gx = (sample.gx / 131.0f) - gyro_offset_x;
            float d_gy = (sample.gy / 131.0f) - gyro_offset_y;
            float d_gz = (sample.gz / 131.0f) - gyro_offset_z;

            // Delta Tempo
            unsigned long now = sample.timestamp; // Usa o tempo real em que a amostra foi lida
            float dt = (now - lastTime) / 1000.0f;
            if (dt <= 0.001f) dt = 0.02f; // ~50Hz padrão
            lastTime = now;

            // Magnitudes
            float accMag = sqrt(d_ax*d_ax + d_ay*d_ay + d_az*d_az);
            float gyroMag = sqrt(d_gx*d_gx + d_gy*d_gy + d_gz*d_gz);

            // Jerk
            float jerk = (accMag - prevAccMag) / dt;
            prevAccMag = accMag;

            // Filtro Complementar (estável)
            float accPitch = atan2(d_ay, sqrt(d_ax*d_ax + d_az*d_az)) * 180.0 / PI;
            float accRoll  = atan2(-d_ax, d_az) * 180.0 / PI;

            pitch = 0.96 * (pitch + d_gx * dt) + 0.04 * accPitch;
            roll  = 0.96 * (roll  + d_gy * dt) + 0.04 * accRoll;

            if (pitch > 180) pitch -= 360; if (pitch < -180) pitch += 360;
            if (roll > 180) roll -= 360; if (roll < -180) roll += 360;

            // =============================
            // MÁQUINA DE ESTADOS DE QUEDA
            // =============================
            switch (fallState) {
                case NORMAL:
                    if (accMag < FREE_FALL_THRESHOLD_G) {
                        fallState = FREE_FALL;
                        freeFallStart = now;
                    }
                    break;

                case FREE_FALL:
                    if (accMag > IMPACT_THRESHOLD_G) {
                        fallState = IMPACT;
                        store_event(pitch, roll, accMag, gyroMag, jerk, fallState);
                    } else if (now - freeFallStart > 1000) {
                        fallState = NORMAL;
                    }
                    break;

                case IMPACT:
                    fallState = IMMOBILITY_CHECK;
                    immobilityStart = now;
                    break;

                case IMMOBILITY_CHECK:
                    if (gyroMag < IMMOBILITY_GYRO_LIMIT && accMag > 0.85 && accMag < 1.15) {
                        if (now - immobilityStart > IMMOBILITY_TIME_MS) {
                            //Serial.println("ALERTA: QUEDA CONFIRMADA!");
                            store_event(pitch, roll, accMag, gyroMag, jerk, fallState);
                            fallState = NORMAL;
                        }
                    } else {
                        fallState = NORMAL;
                    }
                    break;
            }

            // Impressões de Troca de Estado
            if (fallState != previousState) {
                switch (fallState) {
                    case FREE_FALL: Serial.println("[MOTION] -> POSSIVEL QUEDA DETECTADA (Free Fall)"); break;
                    case IMPACT: Serial.println("[MOTION] -> IMPACTO DETECTADO!"); break;
                    case IMMOBILITY_CHECK: Serial.println("[MOTION] -> VERIFICANDO IMOBILIDADE..."); break;
                    default: break;
                }
                previousState = fallState;
            }

            // Eventos estilo Apple (Jerk + Giro)
            if (fabs(jerk) > JERK_THRESHOLD) store_event(pitch, roll, accMag, gyroMag, jerk, fallState);
            if (gyroMag > GYRO_THRESHOLD) store_event(pitch, roll, accMag, gyroMag, jerk, fallState);

            // =============================
            // ATUALIZAR TELEMETRIA (Para o MQTT)
            // =============================
            if (g_telemetryMutex != NULL) {
                if (xSemaphoreTake(g_telemetryMutex, pdMS_TO_TICKS(5))) {
                    g_telemetry.pitch = pitch;
                    g_telemetry.roll = roll;
                    g_telemetry.accMag = accMag;
                    g_telemetry.gyroMag = gyroMag;
                    g_telemetry.jerk = jerk;
                    g_telemetry.state_beta1 = fallState;
                    xSemaphoreGive(g_telemetryMutex);
                }
            }
        }
    }
}