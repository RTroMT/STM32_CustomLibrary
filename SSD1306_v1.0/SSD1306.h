#ifndef SSD1306_H
#define SSD1306_H

#include <stdio.h>
#include <stdint.h>

#define SSD1306_ADDR  (0x3C << 1)
#define SSD1306_HEIGHT 64
#define SSD1306_LENGTH 128

extern uint8_t SSD1306_Buffer[128 * 64 / 8];

// Enumeration for screen colors
typedef enum {
    Black = 0x00, // Black color, no pixel
    White = 0x01  // Pixel is set. Color depends on OLED
} SSD1306_COLOR;

void SSD1306_SendCommand(uint8_t cmd);
void SSD1306_WriteData(uint8_t data);

void SSD1306_Init(void);

void SSD1306_UpdateScreen(void);
void SSD1306_DrawPixel(uint8_t x, uint8_t y, uint8_t color);
void SSD1306_FillWhite(void);
void SSD1306_Clear(void);

#endif