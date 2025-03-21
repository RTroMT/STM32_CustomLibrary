#include <SSD1306.h>
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_i2c.h"

extern I2C_HandleTypeDef hi2c1;
uint8_t SSD1306_Buffer[128 * 64 / 8] = {0};

void SSD1306_SendCommand(uint8_t cmd)
{
    uint8_t data[2] = {0x00, cmd};   //0x00 mean Co = 0 and D/C# = 0 follow by six 0's
    HAL_I2C_Master_Transmit(&hi2c1, SSD1306_ADDR, data, 2,  HAL_MAX_DELAY);
}

void SSD1306_WriteData(uint8_t data)
{
    uint8_t buffer[2] = {0x40, data};  //0x00 mean Co = 0 and D/C# = 1 follow by six 0's
    HAL_I2C_Master_Transmit(&hi2c1, SSD1306_ADDR, buffer, 2, HAL_MAX_DELAY);

}

void SSD1306_Init(void)
{
    //Detail software initialization can be found in datasheet page 64
    SSD1306_SendCommand(0xAE); // Display OFF
    SSD1306_SendCommand(0xD5); // Set display clock divide ratio/oscillator frequency
    SSD1306_SendCommand(0x80); // Default clock value

    //Set MUX ratio
    SSD1306_SendCommand(0xA8);
    SSD1306_SendCommand(0x3F); //Set ratio to 1/64

    //Set display offset
    SSD1306_SendCommand(0xD3);
    SSD1306_SendCommand(0x00);

    //Set display start line
    SSD1306_SendCommand(0x40);

    //Set segments (collumns) remap
    SSD1306_SendCommand(0xA0); //A1h if reversed order wanted

    //Set COM pin config
    SSD1306_SendCommand(0xC0); //Set COM output scan direction
                               //C0h mean scan from COM0 -> COM[N-1]
    SSD1306_SendCommand(0xDA); //Set COM pin config
    SSD1306_SendCommand(0x12); //0x12 equal to alternative config

    //Set contrast control
    SSD1306_SendCommand(0x81);
    SSD1306_SendCommand(0xFF); //Medium contrast - used in reset state

    //Set display configuration
    SSD1306_SendCommand(0xA4); // Disable entire display On
    SSD1306_SendCommand(0xA6);

    SSD1306_SendCommand(0x8D);
    SSD1306_SendCommand(0x14);

    //Display On
    SSD1306_SendCommand(0xAF);
}

void SSD1306_UpdateScreen(void)
{
    //Set page address and column offset
    /*F0llowing the datasheet description:
    • Set the page start address of the target display location by command B0h to B7h.
    • Set the lower start column address of pointer by command 00h~0Fh.
    • Set the upper start column address of pointer by command 10h~1F.
    */
    //There are 8 pages, each page have 128 column -> 128 bytes
    for(int i = 0; i < 8; i++)
    {
        SSD1306_SendCommand(0xB0 + i);
        SSD1306_SendCommand(0x00);
        SSD1306_SendCommand(0x10);


        for(int j = 0; j < 128; j++)
        {
            SSD1306_WriteData(SSD1306_Buffer[i * 128 + j]);
        }
    }
}

void SSD1306_DrawPixel(uint8_t x, uint8_t y, uint8_t color)
{
    if(x >= SSD1306_LENGTH || y >= SSD1306_HEIGHT)
    {
        return;
    }

    if(color == White)
    {
        SSD1306_Buffer[x + 128 * (y / 8)] |= 1 << (y % 8);
    }
    else{
        SSD1306_Buffer[x + 128 * (y / 8)] &= ~(1 << (y % 8));
    }
}

void SSD1306_FillWhite(void)
{
    memset(SSD1306_Buffer, 0xFF, sizeof(SSD1306_Buffer));
    SSD1306_UpdateScreen();
}

void SSD1306_Clear(void)
{
    memset(SSD1306_Buffer, 0x00, sizeof(SSD1306_Buffer));
    SSD1306_UpdateScreen();
}


