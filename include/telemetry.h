
#pragma once
#include <stdint.h>

typedef struct
{
    float pitch;
    float roll;

    float accMag;
    float gyroMag;

    float jerk;

    int state_beta1;

    float bpm;

    float spo2;

    float body_temp;   // <<< ADICIONAR

} telemetry_data_t;

extern telemetry_data_t g_telemetry;

extern volatile uint32_t g_lastHeartbeatMs;