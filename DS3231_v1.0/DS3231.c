#include "DS3231.h"
#include "stm32f1xx_hal.h"
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

//Functions to write/read regs
static void DS3231_Write_Register(DS3231_Handle_t *ds3231, uint8_t regAddr, uint8_t *pData, uint16_t size)
{
    HAL_I2C_Mem_Write(ds3231->hi2c, ds3231->Address, regAddr, I2C_MEMADD_SIZE_8BIT, pData, size, HAL_MAX_DELAY);
}

static void DS3231_Read_Register(DS3231_Handle_t *ds3231, uint8_t regAddr, uint8_t *pData, uint16_t size)
{
    HAL_I2C_Mem_Read(ds3231->hi2c, ds3231->Address, regAddr, I2C_MEMADD_SIZE_8BIT, pData, size, HAL_MAX_DELAY);
}

//Initialize the DS3231
void DS3231_Init(DS3231_Handle_t *ds3231, I2C_HandleTypeDef *hi2c)
{
    //Assign HAL handle typedef and address
    ds3231->hi2c = hi2c;
    ds3231->Address = DS3231_ADDR;
}

void DS3231_SetTime(DS3231_Handle_t *ds3231, uint8_t sec, uint8_t min, uint8_t hour, uint8_t day, uint8_t date, uint8_t month, uint8_t year)
{
    uint8_t buffer[7];
    //
    buffer[0] = DECtoBCD(sec);
    buffer[1] = DECtoBCD(min);
    buffer[2] = DECtoBCD(hour);
    buffer[3] = DECtoBCD(day);
    buffer[4] = DECtoBCD(date);
    //buffer[5] = DECtoBCD(month);
    buffer[5] = 1 << 7 | DECtoBCD(month);
    buffer[6] = DECtoBCD(year);
    //
    DS3231_Write_Register(ds3231, DS3231_REG_SEC, buffer, (sizeof(buffer) / sizeof(buffer[0])));
}
void DS3231_GetTime(DS3231_Handle_t *ds3231, uint8_t* sec, uint8_t* min, uint8_t* hour, uint8_t* day, uint8_t* date, uint8_t* month, uint8_t* year)
{
    uint8_t buffer[7];
    //
    DS3231_Read_Register(ds3231, DS3231_REG_SEC, buffer, (sizeof(buffer) / sizeof(buffer[0])));
    //
    *sec   =  BCDtoDEC(buffer[0]);
    *min   =  BCDtoDEC(buffer[1]);
    *hour  =  BCDtoDEC(buffer[2]);
    *day   =  BCDtoDEC(buffer[3]);
    *date  =  BCDtoDEC(buffer[4]);
    *month =  BCDtoDEC(buffer[5]) - 80;
    *year  =  BCDtoDEC(buffer[6]);

}

float DS3231_GetTemperature(DS3231_Handle_t *ds3231)
{
    uint8_t tempMSB, tempLSB;
    DS3231_Read_Register(ds3231, 0x11, &tempMSB, 1);
    DS3231_Read_Register(ds3231, 0x12, &tempLSB, 1);
    return ((float)tempMSB + (tempLSB >> 6) * 0.25);
}

