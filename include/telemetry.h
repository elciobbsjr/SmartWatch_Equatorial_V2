#pragma once

typedef struct {
    float pitch;
    float roll;
    float accMag;
    float gyroMag;
    float jerk;
    int   state_beta1;
    float bpm;
    float spo2;   
    float body_temp; // <-- NOVO: Temperatura Corporal
} telemetry_data_t;

// Avisa o sistema que a variável g_telemetry existe em algum lugar
extern telemetry_data_t g_telemetry;