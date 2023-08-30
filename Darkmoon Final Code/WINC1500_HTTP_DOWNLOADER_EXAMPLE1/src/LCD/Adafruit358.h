
/**************************************************************************//**
* @file        Adafruit358.h
* @ingroup 	   LCD
* @brief       Basic display driver for ST7735R chip
* @details     Basic display driver for ST7735R chip
*
* @copyright
* @author	   Henry
* @date        May 1st, 2023
* @version	   1.0
*****************************************************************************/
#include <asf.h>

#ifndef ADAFRUIT358_H_
#define ADAFRUIT358_H_

#define LCD_DC			PIN_PA10
#define LCD_DC_PORT		PORT_PA10
#define LCD_RST			PIN_PB22
#define LCD_RST_PORT	PORT_PB22
#define SLAVE_SELECT_PIN			PIN_PA11
#define SLAVE_SELECT_PORT			PORT_PA11

#define LCD_WIDTH 160
#define LCD_HEIGHT 128
#define LCD_SIZE  LCD_WIDTH * LCD_HEIGHT

#define SPI_FREQ 24000000		 //24MHz  = 200ns per signal 

#define CONF_MASTER_SPI_MODULE		SERCOM5
#define CONF_MASTER_MUX_SETTING		SPI_SIGNAL_MUX_SETTING_N
#define CONF_MASTER_PINMUX_PAD0     PINMUX_PB02D_SERCOM5_PAD0  //mosi
#define CONF_MASTER_PINMUX_PAD1     PINMUX_PB03D_SERCOM5_PAD1  //miso
#define CONF_MASTER_PINMUX_PAD2     PINMUX_UNUSED
#define CONF_MASTER_PINMUX_PAD3     PINMUX_PA21C_SERCOM5_PAD3  //sck


// ST7735 registers
#define ST7735_NOP     0x00
#define ST7735_SWRESET 0x01
#define ST7735_RDDID   0x04
#define ST7735_RDDST   0x09
#define ST7735_SLPIN   0x10
#define ST7735_SLPOUT  0x11
#define ST7735_PTLON   0x12
#define ST7735_NORON   0x13
#define ST7735_INVOFF  0x20
#define ST7735_INVON   0x21
#define ST7735_DISPOFF 0x28
#define ST7735_DISPON  0x29
#define ST7735_CASET   0x2A
#define ST7735_RASET   0x2B
#define ST7735_RAMWR   0x2C
#define ST7735_RAMRD   0x2E
#define ST7735_PTLAR   0x30
#define ST7735_COLMOD  0x3A
#define ST7735_MADCTL  0x36
#define ST7735_FRMCTR1 0xB1
#define ST7735_FRMCTR2 0xB2
#define ST7735_FRMCTR3 0xB3
#define ST7735_INVCTR  0xB4
#define ST7735_DISSET5 0xB6
#define ST7735_PWCTR1  0xC0
#define ST7735_PWCTR2  0xC1
#define ST7735_PWCTR3  0xC2
#define ST7735_PWCTR4  0xC3
#define ST7735_PWCTR5  0xC4
#define ST7735_VMCTR1  0xC5
#define ST7735_RDID1   0xDA
#define ST7735_RDID2   0xDB
#define ST7735_RDID3   0xDC
#define ST7735_RDID4   0xDD
#define ST7735_PWCTR6  0xFC
#define ST7735_GMCTRP1 0xE0
#define ST7735_GMCTRN1 0xE1

#define MADCTL_MY  0x80
#define MADCTL_MX  0x40
#define MADCTL_MV  0x20
#define MADCTL_ML  0x10
#define MADCTL_RGB 0x00
#define MADCTL_BGR 0x08
#define MADCTL_MH  0x04

// colors
#define	BLACK     0x0000
#define WHITE     0xFFFF
#define	BLUE      0x001F
#define	RED       0xF800
#define	GREEN     0x07E0
#define CYAN      0x07FF
#define MAGENTA   0xF81F
#define YELLOW    0xFFE0

#define SCREEN_WIDTH  160
#define SCREEN_HEIGHT 128

#define BG_COLOR      0x0000  
#define TITLE_COLOR   0xFFE0  
#define BORDER_COLOR  0x07E0  
#define ICON_COLOR    0x07FF  

#define BOOT_SCREEN_BG_COLOR 0x0000 
#define BOOT_SCREEN_TITLE_COLOR 0xFFE0 
#define BOOT_SCREEN_SUBTITLE_COLOR 0x07E0  

typedef struct {
	uint16_t btctrl;
	uint16_t btcnt;
	uint32_t srcaddr;
	uint32_t dstaddr;
	uint32_t descaddr;
} dmaDescriptor;

void lcd_init(void);
uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue);
void LCD_setAddr(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1);
void LCD_drawBlock(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1,uint16_t color);
void LCD_setScreen(uint16_t color);
void LCD_drawPixel(uint8_t x, uint8_t y, uint16_t color);
void LCD_rotate(uint8_t r);
void LCD_drawLine(short x0,short y0,short x1,short y1,uint16_t c);
void LCD_drawYLine(uint8_t y,uint16_t c);
void LCD_drawXLine(uint8_t x,uint16_t c);
void LCD_drawChar(uint8_t x, uint8_t y, uint16_t character, uint16_t fColor, uint16_t bColor);
void LCD_drawFastChar(uint8_t x, uint8_t y, uint16_t character, uint16_t fColor, uint16_t bColor);
void LCD_drawString(uint8_t x, uint8_t y, char* str, uint16_t fg, uint16_t bg);
void lcd_draw_circle_arc(int x0, int y0, int r, bool down, uint16_t HEART_COLOR);
void draw_heart(int x, int y, int r, uint16_t HEART_COLOR);
void draw_title(const char *title);
void draw_border(int thickness, uint16_t color);
void draw_icon(int x, int y, int size);
void draw_filled_circle(int x0, int y0, int r, uint16_t color);
void draw_moon_icon(int x, int y, int size);
void draw_rotating_squares(int centerX, int centerY, int num_squares, uint16_t color);
void draw_boot_screen_title(const char *title);
void draw_boot_screen_subtitle(const char *subtitle);
void draw_thick_border(int thickness, uint16_t color1, uint16_t color2, uint16_t color3);
void draw_main_page(void);
void LCD_fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color);

#endif /* ADAFRUIT358_H_ */

