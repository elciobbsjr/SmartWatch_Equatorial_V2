#include <Arduino.h>

#include "i2c_mgr.h"

#include "task_mpu.h"
#include "task_motion.h"

#include "task_oximeter.h"
#include "task_heart.h"

#include "task_temp.h"

#include "task_telemetry.h"

#include "app_config.h"


//====================================================
// FILAS GLOBAIS
//====================================================

QueueHandle_t g_mpuQueue = NULL;

QueueHandle_t g_ppgHeartQueue = NULL;
QueueHandle_t g_ppgSpo2Queue  = NULL;


//====================================================
// MUTEX GLOBAL DA TELEMETRIA
//====================================================

SemaphoreHandle_t g_telemetryMutex = NULL;


//====================================================
// SETUP
//====================================================

void setup()
{
    Serial.begin(115200);

    delay(3000);

    Serial.println("\n==========================================");
    Serial.println(" BOOT SISTEMA MODULAR - FASE 7 (NUVEM) ");
    Serial.println("==========================================");


    //------------------------------------------------
    // 1. CRIAÇÃO DE MEMÓRIA (Filas + Mutex)
    //------------------------------------------------

    Serial.println("[SYS] criando filas...");

    g_telemetryMutex = xSemaphoreCreateMutex();


    // fila do MPU (acelerometro)
    g_mpuQueue = xQueueCreate(
        32,
        sizeof(mpu_sample_t)
    );


    // filas do sensor MAX30102
    g_ppgHeartQueue = xQueueCreate(
        32,
        sizeof(ppg_sample_t)
    );

    g_ppgSpo2Queue = xQueueCreate(
        32,
        sizeof(ppg_sample_t)
    );


    //------------------------------------------------
    // valida se criou corretamente
    //------------------------------------------------

    if(g_mpuQueue == NULL)
        Serial.println("ERRO criando fila MPU");

    if(g_ppgHeartQueue == NULL)
        Serial.println("ERRO criando fila HEART");

    if(g_ppgSpo2Queue == NULL)
        Serial.println("ERRO criando fila SPO2");

    if(g_telemetryMutex == NULL)
        Serial.println("ERRO criando mutex telemetria");


    //------------------------------------------------
    // 2. INICIALIZA HARDWARE I2C
    //------------------------------------------------

    Serial.println("[SYS] iniciando I2C...");

    i2c_mgr_init();


    //------------------------------------------------
    // 3. CRIA TASKS (FreeRTOS)
    //------------------------------------------------

    Serial.println("[SYS] iniciando tasks...");


    //--------------------------------
    // MOVIMENTO
    //--------------------------------

    xTaskCreate(
        task_mpu,
        "TaskMPU",
        4096,
        NULL,
        1,
        NULL
    );

    xTaskCreate(
        task_motion,
        "TaskMotion",
        4096,
        NULL,
        2,
        NULL
    );


    //--------------------------------
    // OXIMETRO
    //--------------------------------

    xTaskCreate(
        task_oximeter,
        "TaskOxi",
        6144,
        NULL,
        1,
        NULL
    );

    xTaskCreate(
        task_heart,
        "TaskHeart",
        6144,
        NULL,
        2,
        NULL
    );


    //--------------------------------
    // TEMPERATURA
    //--------------------------------

    xTaskCreate(
        task_temp,
        "TaskTemp",
        4096,
        NULL,
        1,
        NULL
    );


    //--------------------------------
    // CLOUD (MQTT + GOOGLE SHEETS)
    //--------------------------------

    xTaskCreate(
        task_telemetry,
        "TaskCloud",
        8192,
        NULL,
        1,
        NULL
    );


    Serial.println("[SYS] sistema iniciado");
}


//====================================================
// LOOP VAZIO
//====================================================

void loop()
{
    vTaskDelay(
        pdMS_TO_TICKS(2000)
    );
}