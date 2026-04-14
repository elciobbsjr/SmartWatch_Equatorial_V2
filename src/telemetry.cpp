#include "telemetry.h"

telemetry_data_t g_telemetry = {0};

volatile uint32_t g_lastHeartbeatMs = 0;