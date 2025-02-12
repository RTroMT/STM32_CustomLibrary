#ifndef DS3231_H
#define DS3231_H

//ADDRESS DEFINE
#ifndef DS3231_ADDR
//The address of DS3231 is defined as 7-bit
//We need to shift left 1 bit inorder to fit 8-bit  
#define DS3231_ADDR 0x68 << 1
#endif 

//Adjust based on your STM32 processor
#include "stm32f1xx_hal.h"

//DS3231 REGISTERS
#define DS3231_REG_SEC   0x00
#define DS3231_REG_MIN   0x01
#define DS3231_REG_HOUR  0x02
#define DS3231_REG_DAY   0x03  //range 1-7 using 3 bit starting from LSB
#define DS3231_REG_DATE  0x04
#define DS3231_REG_MONTH 0x05
#define DS3231_REG_YEAR  0x06

//Lower bits indicate the fractional portion
//Resolution of 0.25 (deg Celsius) per bit 
#define DS3231_REG_TEMP_LO   0x12 
//Higher bits indicate the integer portion                                                              
#define DS3231_REG_TEMP_HI   0x11 


//CUSTOM DEFINE
#define TIMEOUT_1S 1000


//Private variables
typedef struct
{
    I2C_HandleTypeDef *hi2c;
    uint8_t Address;
} DS3231_Handle_t;

//Functions
void DS3231_Init(DS3231_Handle_t *ds3231, I2C_HandleTypeDef *hi2c);
void DS3231_SetTime(DS3231_Handle_t *ds3231, uint8_t sec, uint8_t min, uint8_t hour, uint8_t day, uint8_t date, uint8_t month, uint8_t year);
void DS3231_GetTime(DS3231_Handle_t *ds3231, uint8_t* sec, uint8_t* min, uint8_t* hour, uint8_t* day, uint8_t* date, uint8_t* month, uint8_t* year);
float DS3231_GetTemperature(DS3231_Handle_t *ds3231);



#endif

