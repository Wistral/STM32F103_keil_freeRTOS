#ifndef __LED_H__
#define __LED_H__

#include "stm32f10x_conf.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "FreeRTOS.h"
// #include "FreeRTOSConfig.h"

//Hardware configuration
#define LED_GPIO_PERIF         RCC_APB2Periph_GPIOC
#define LED_GPIO_GREEN_PORT    GPIOC
#define LED_GPIO_GREEN         GPIO_Pin_13
#define LED_ON_BOARD         GPIO_Pin_13
#define LED_NUM                1
#define ledSetGreen(VALUE)     ledSet(LED_GREEN, VALUE)
#define INTERVAL               100 // ms

typedef enum {LED_GREEN=0} led_t;

void ledInit(void);
bool ledTest(void);
void ledSet(led_t led, bool value);


#endif
