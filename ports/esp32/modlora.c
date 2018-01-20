/*
* Reference from pycom
* 
*/
#include "modlora.h"

#include "py/runtime.h"
#include "py/mphal.h"
#include "modmachine.h"


/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
static QueueHandle_t xCmdQueue;
static QueueHandle_t xRxQueue;
static EventGroupHandle_t LoRaEvents;
TaskHandle_t xLoRaTaskHndl;

/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/
static void TASK_LoRa (void *parms) {
    printf("TASK Lora run .... .. \n");
   
#if defined(LORA_MODE_PRIVATE)
    TASK_lora_ping_pong(NULL);
#endif
}



/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/
void modlora_init0(void) {
    xCmdQueue = xQueueCreate(LORA_CMD_QUEUE_SIZE_MAX, sizeof(lora_cmd_data_t));
    xRxQueue = xQueueCreate(LORA_DATA_QUEUE_SIZE_MAX, sizeof(lora_rx_data_t));
    LoRaEvents = xEventGroupCreate();

    // if (!lorawan_nvs_open()) {
    //     printf("Error opening LoRa NVS namespace!\n");
    // }
    printf("LoRaTask stack water mark: %d\n", 
        (unsigned int)uxTaskGetStackHighWaterMark((TaskHandle_t)xLoRaTaskHndl));
    // target board initialisation
    printf("modlora_init0 init start\n");
    BoardInitMcu();
    BoardInitPeriph();
    

    xTaskCreatePinnedToCore(
        TASK_LoRa, "LoRa", 
        LORA_STACK_SIZE / sizeof(StackType_t), NULL, 
        LORA_TASK_PRIORITY, NULL, 0);
    printf("modlora_init0 init done \n");
}
