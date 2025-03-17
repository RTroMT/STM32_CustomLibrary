#ifndef SSD1306__H
#define SSD1306__H

#include <stdio.h>

#define SSD1306_ADR  0x3C<<1

uint8_t SSD1306_Buffer[128 * 64 / 8];

void SSD1306_SendCommand(uint8_t cmd);
void SSD1306_WriteData(uint8_t data);

void SSD1306_Init(void);

void SSD1306_UpdateScreen(void);
void SSD1306_DrawPixel(uint8_t x, uint8_t y, uint8_t color);
void SSD1306_Clear(void);

#endif