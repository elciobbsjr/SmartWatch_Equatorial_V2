#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Acesso global ao Mutex
extern SemaphoreHandle_t g_i2cMutex;

// Funções de inicialização
void i2c_mgr_init();