// #include "config.h"
// #include "system.h"
// #include "FreeRTOSConfig.h"
// #include "stm32f10x_gpio.h"
// #include <stdbool.h>
#include "stm32f10x_conf.h"
// #include "FreeRTOS.h"
#include "led.h"
#include "task.h"

bool isInit = false;
void systemInit()
{
    if(isInit)
    {
        return;
    }
    ledInit();
    isInit = true;
}

void systemTask(void *param) {
  systemInit();

  while (1) {
    ledSetGreen(1);
    vTaskDelay(M2T(20));
    ledSetGreen(0);
    vTaskDelay(M2T(20));
  }

  // Should never reach this point!
  while (1)
    vTaskDelay(portMAX_DELAY);
}

void systemLaunch() {
  xTaskCreate(systemTask, "SYSTEM", configMINIMAL_STACK_SIZE << 1, NULL,
              configMAX_PRIORITIES - 1, NULL);
}

int main() {
  systemLaunch();
  vTaskStartScheduler();
}

/* For now, the stack depth of IDLE has 88 left. if want add func to here,
   you should increase it. */
void vApplicationIdleHook(
    void) { /* ATTENTION: all funcs called within here, must not be blocked */
}
