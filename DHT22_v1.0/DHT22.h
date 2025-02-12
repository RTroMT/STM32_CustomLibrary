#ifndef __DHT22_H
#define __DHT22_H


#include "stm32f1xx_hal.h"
//Define pin for DHT22
#define DHT22_GPIO_PORT GPIOB
#define DHT22_GPIO_PIN GPIO_PIN_14

typedef struct 
{
    float temperature;
    float humidity;
} DHT22_DATA;

void DHT22_Init();
uint8_t DHT22_Read(DHT22_DATA* data);

#endif