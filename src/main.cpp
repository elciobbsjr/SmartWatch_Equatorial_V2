#include <Arduino.h>
#include "i2c_mgr.h"
#include "task_mpu.h"
#include "task_motion.h" // <-- CORREÇÃO 1: Faltou incluir o header do cérebro!
#include "task_telemetry.h" // <-- NOVO: O Mensageiro da Nuvem
#include "app_config.h" 
#include "task_temp.h" // <-- ADICIONE NO TOPO

// Vamos deixar as tasks do oxímetro e heart incluídas, mas não vamos iniciá-las
#include "task_oximeter.h"
#include "task_heart.h"
//#include "task_spo2.h"

// Filas globais
QueueHandle_t g_ppgHeartQueue = NULL;
QueueHandle_t g_ppgSpo2Queue = NULL; 
QueueHandle_t g_mpuQueue = NULL; // <-- CORREÇÃO 2: Faltou declarar a Fila do MPU!

SemaphoreHandle_t g_telemetryMutex = NULL;

void setup() {
    Serial.begin(115200);
    delay(3000); 
    
    Serial.println("\n\n==========================================");
    Serial.println("  BOOT SISTEMA MODULAR - FASE 7 (NUVEM)   "); // Atualizei o nome para a Fase 7
    Serial.println("==========================================");

    // =========================================================
    // INICIALIZAÇÃO DE MEMÓRIA (Filas e Mutex)
    // =========================================================
    g_telemetryMutex = xSemaphoreCreateMutex();
    
    // CORREÇÃO 3: Faltou criar fisicamente a fila na memória! 
    // Sem isso, a placa reseta quando tenta mandar os dados.
    g_mpuQueue = xQueueCreate(32, sizeof(mpu_sample_t)); 

    // =========================================================
    // INICIALIZAÇÃO DE HARDWARE E TAREFAS
    // =========================================================
    i2c_mgr_init();

    // INICIA O SISTEMA DE MOVIMENTO (Fase 6)
    //xTaskCreate(task_mpu, "TaskMPU", 4096, NULL, 1, NULL);       // Driver: Só lê I2C
    //xTaskCreate(task_motion, "TaskMotion", 4096, NULL, 2, NULL); // Cérebro: Calcula quedas
    // NOVO: Monitor de Temperatura (Prioridade 1, pois não tem pressa)
    xTaskCreate(task_temp, "TaskTemp", 4096, NULL, 1, NULL);
    
    // CONEXÃO COM O MUNDO (Fase 7 - Wi-Fi e MQTT)
    // Nota: Como o MQTT Secure (SSL/TLS) consome muita RAM, damos 8192 de memória
    xTaskCreate(task_telemetry, "TaskCloud", 8192, NULL, 1, NULL);
    
    // DESLIGADOS TEMPORARIAMENTE:
    // xTaskCreate(task_oximeter, "TaskOxi", 4096, NULL, 1, NULL);
    // xTaskCreate(task_heart, "TaskHeart", 4096, NULL, 2, NULL); 
    // xTaskCreate(task_spo2, "TaskSpo2", 4096, NULL, 2, NULL);
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(2000));
}