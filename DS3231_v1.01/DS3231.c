#include "DS3231.h"

//BCD Conversion functions
static uint8_t BCDtoDEC(uint8_t bcd)
{
    return ((bcd >> 4) * 10 + (bcd & 0x0f));
    //Or bit 4-7 * 10 + bit 0-3 
}

static uint8_t DECtoBCD(uint8_t dec)
{
    return ((dec / 10) << 4 | (dec % 10));
}


//Initialize the DS3231
void DS3231_Init(DS3231_Handle_t *ds3231, I2C_HandleTypeDef *hi2c)
{
    //Assign HAL handle typedef and address
    ds3231->hi2c = hi2c;
    ds3231->Address = DS3231_ADDR;
}

void DS3231_SetTime(DS3231_Handle_t *ds3231, uint8_t sec, uint8_t min, uint8_t hour)
{
    
}

void DS3231_GetTime(DS3231_Handle_t *ds3231, uint8_t& sec, uint8_t& min, uint8_t& hour)
{}

float DS3231_GetTemperature(DS3231_Handle_t *ds3231)
{}