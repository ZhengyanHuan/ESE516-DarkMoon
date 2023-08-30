
/**************************************************************************//**
* @file        Adafruit358.c
* @ingroup 	   LCD
* @brief       Basic display driver for ST7735R chip
* @details     Basic display driver for ST7735R chip
*
* @copyright
* @author	   Henry
* @date        May 1st, 2023
* @version	   1.0
*****************************************************************************/

/******************************************************************************
* Includes
******************************************************************************/
#include <asf.h>
#include "Adafruit358.h"
#include "delay.h"
#include "ASCII_LUT.h"
#include "SerialConsole.h"

/******************************************************************************
* Defines
******************************************************************************/

//The following functions bypasses port abstractions to speed up communication
#define DISPLAY_DC_HIGH()  REG_PORT_OUTSET0 =  LCD_DC_PORT
#define DISPLAY_DC_LOW() REG_PORT_OUTCLR0 =  LCD_DC_PORT
#define DISPLAY_NCS_HIGH()  REG_PORT_OUTSET0 = SLAVE_SELECT_PORT
#define DISPLAY_NCS_LOW() REG_PORT_OUTCLR0 =  SLAVE_SELECT_PORT

/******************************************************************************
* Structures and Enumerations
******************************************************************************/
struct spi_module spi_master_instance;
struct spi_slave_inst slave;
struct tcc_module tcc_instance;

volatile dmaDescriptor dmaDescriptorArray[1] __attribute__ ((aligned (16)));
dmaDescriptor dmaDescriptorWritebackArray[1] __attribute__ ((aligned (16)));
#define DMA_BUFFER_LENGTH	1
static volatile uint8_t buffer_tx = 0x00;
/******************************************************************************
* Local Functions
******************************************************************************/

static void sendSPI_DMA(uint8_t beat)
{
	vTaskSuspendAll();
	dmaDescriptorArray[0].srcaddr = &beat + DMA_BUFFER_LENGTH;
	// Start the transfer!
	DMAC->CHCTRLA.reg |= DMAC_CHCTRLA_ENABLE;

	while (DMAC->CHINTFLAG.reg != 2) {};
	DMAC->CHINTFLAG.reg = DMAC_CHINTENCLR_MASK;

	#if SPI_FREQ < 12000000
	delay_cycles_us(1);
	#endif
	
	xTaskResumeAll();
}

static void dma_init()
{
	PM->AHBMASK.bit.DMAC_ = 1;
	PM->APBBMASK.bit.DMAC_ = 1;
	
	DMAC->BASEADDR.reg = (uint32_t)dmaDescriptorArray;
	DMAC->WRBADDR.reg = (uint32_t)dmaDescriptorWritebackArray;
	
	DMAC->CTRL.reg = DMAC_CTRL_DMAENABLE | DMAC_CTRL_LVLEN(0xf);
	
	dmaDescriptorArray[0].btctrl =	(1 << 0) |  // VALID: Descriptor Valid
	(0 << 3) |  // BLOCKACT=NOACT: Block Action
	(1 << 10) | // SRCINC: Source Address Increment Enable
	(0 << 11) | // DSTINC: Destination Address Increment Enable
	(1 << 12) | // STEPSEL=SRC: Step Selection
	(0 << 13);  // STEPSIZE=X1: Address Increment Step Size
	dmaDescriptorArray[0].btcnt = DMA_BUFFER_LENGTH; // beat count
	dmaDescriptorArray[0].dstaddr = (uint32_t)(&spi_master_instance.hw->SPI.DATA.reg);
	dmaDescriptorArray[0].srcaddr = &buffer_tx + DMA_BUFFER_LENGTH;
	dmaDescriptorArray[0].descaddr = 0;
	
	DMAC->CHID.reg = 0; // select channel 0
	DMAC->CHCTRLB.reg = DMAC_CHCTRLB_LVL(0) | DMAC_CHCTRLB_TRIGSRC(SERCOM5_DMAC_ID_TX) | DMAC_CHCTRLB_TRIGACT_BEAT;
	
	DMAC->CHINTENSET.reg = DMAC_CHINTENSET_TCMPL;		//Enable interrupt
}

/**************************************************************************//**
* @fn			static void swap_num(short* a,short* b)
* @brief		Swap linker of two short integers
* @note			
*****************************************************************************/
static void swap_num(short* a,short* b)
{
	short t = *a;
	*a = *b;
	*b = t;
}

/**************************************************************************//**
* @fn			static void Adafruit358LCD_spi_select_device(void)
* @brief		Select device by pulling CS pin to GND
* @note
*****************************************************************************/
static void Adafruit358LCD_spi_select_device(void)
{
	DISPLAY_NCS_LOW();		
}

/**************************************************************************//**
* @fn			static void Adafruit358LCD_spi_deselect_device(void)
* @brief		De-select device by returning CS pin to VDD
* @note
*****************************************************************************/
static void Adafruit358LCD_spi_deselect_device(void)
{
	DISPLAY_NCS_HIGH();		
}

/**************************************************************************//**
* @fn			static void Adafruit358LCD_spi_send_stream(uint8_t cmd)
* @brief		Send a command to LCD through SPI without CS
* @note
*****************************************************************************/
static void Adafruit358LCD_spi_send_stream(uint8_t cmd)
{
	sendSPI_DMA(cmd);
}

/**************************************************************************//**
* @fn			static void Adafruit358LCD_spi_send_cmd(uint8_t cmd)
* @brief		Send a command to LCD through SPI
* @note
*****************************************************************************/
static void Adafruit358LCD_spi_send_cmd(uint8_t cmd)
{
	Adafruit358LCD_spi_select_device();
	DISPLAY_DC_LOW();
	sendSPI_DMA(cmd);
	Adafruit358LCD_spi_deselect_device();
}

/**************************************************************************//**
* @fn			static void Adafruit358LCD_spi_send_data(uint8_t data)
* @brief		Send data to LCD through SPI
* @note
*****************************************************************************/
static void Adafruit358LCD_spi_send_data(uint8_t data)
{
	Adafruit358LCD_spi_select_device();
	DISPLAY_DC_HIGH();
	sendSPI_DMA(data);
	Adafruit358LCD_spi_deselect_device();
}

/**************************************************************************//**
* @fn			static void Adafruit358LCD_spi_send_16bit_stream(uint16_t data)
* @brief		Send 16 bit data to LCD through SPI without CS or DC
* @note			Used for color information
*****************************************************************************/
static void Adafruit358LCD_spi_send_16bit_stream(uint16_t data)
{
	uint8_t temp = data >> 8;
	sendSPI_DMA(temp);
	sendSPI_DMA(data);
}

/**************************************************************************//**
* @fn			static void Adafruit358LCD_spi_send_data16(uint16_t data)
* @brief		Send 16 bit data to LCD through SPI
* @note			Used for color information
*****************************************************************************/
static void Adafruit358LCD_spi_send_data16(uint16_t data)
{
	Adafruit358LCD_spi_select_device();
	DISPLAY_DC_HIGH();
	uint8_t temp = data >> 8;
	sendSPI_DMA(temp);
	sendSPI_DMA(data);
	Adafruit358LCD_spi_deselect_device();
}

/**************************************************************************//**
* @fn			static void sendCommands (const uint8_t *cmds, uint8_t length)
* @brief		Parse and send array of commands thru SPI
* @note			
*****************************************************************************/
static void sendCommands (const uint8_t *cmds, uint8_t length)
{

	uint8_t numCommands, numData, waitTime;
	numCommands = length;
	
	Adafruit358LCD_spi_select_device();
	
	while (numCommands--)
	{
		DISPLAY_DC_LOW();
		Adafruit358LCD_spi_send_stream(*cmds++);
		numData = *cmds++;	// # of data bytes to send
		
		DISPLAY_DC_HIGH();
		while (numData--)	// Send each data byte...
		{
			Adafruit358LCD_spi_send_stream(*cmds++);
		}
		
		waitTime = *cmds++;     // Read post-command delay time (ms)
		if (waitTime!=0)
		{
			delay_cycles_ms((waitTime==255 ? 500 : waitTime));
		}
	}
	
	Adafruit358LCD_spi_deselect_device();
}

/**************************************************************************//**
* @fn			static void lcd_pin_config(void)
* @brief		Initialize pins for LCD
* @note			
*****************************************************************************/
static void lcd_pin_config(void)
{
	/* Configure LCD control pins as outputs, turn them on */
	struct port_config pin_conf;
	pin_conf.direction  = PORT_PIN_DIR_OUTPUT;
	
	delay_cycles_ms(100);
	port_pin_set_config(LCD_RST, &pin_conf);
	port_pin_set_output_level(LCD_RST, 0);		//LCD Reset
	delay_cycles_ms(50);
	port_pin_set_output_level(LCD_RST, 1);		//LCD Enable
	
	port_pin_set_config(LCD_DC, &pin_conf);
	port_pin_set_output_level(LCD_DC, 1);		//LCD_DC pin
	delay_cycles_ms(100);
}

/**************************************************************************//**
* @fn			static void lcd_spi_init(void)
* @brief		Initialize SPI for LCD
* @note			
*****************************************************************************/
static void lcd_spi_init(void)
{	
	struct spi_config config_spi_master;
	struct spi_slave_inst_config slave_dev_config;
	/* Configure and initialize software device instance of peripheral slave */
	spi_slave_inst_get_config_defaults(&slave_dev_config);
	slave_dev_config.ss_pin = SLAVE_SELECT_PIN;
	spi_attach_slave(&slave, &slave_dev_config);
	/* Configure, initialize and enable SERCOM SPI module */
	spi_get_config_defaults(&config_spi_master);
	config_spi_master.mux_setting = CONF_MASTER_MUX_SETTING;
	config_spi_master.pinmux_pad0 = CONF_MASTER_PINMUX_PAD0;
	config_spi_master.pinmux_pad1 = CONF_MASTER_PINMUX_PAD1;
	config_spi_master.pinmux_pad2 = CONF_MASTER_PINMUX_PAD2;
	config_spi_master.pinmux_pad3 = CONF_MASTER_PINMUX_PAD3;
	config_spi_master.mode_specific.master.baudrate =  SPI_FREQ;
	spi_init(&spi_master_instance, CONF_MASTER_SPI_MODULE, &config_spi_master);
	spi_enable(&spi_master_instance);
	dma_init();
}

/**************************************************************************//**
* @fn			uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue)
* @brief		Convert RGB888 value to RGB565 16-bit color data
* @note
*****************************************************************************/
uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue)
{
	return ((((31*(red+4))/255)<<11) | (((63*(green+2))/255)<<5) | ((31*(blue+4))/255));
}

/**************************************************************************//**
* @fn			void lcd_init(void)
* @brief		Initialize LCD settings
* @note			
*****************************************************************************/
void lcd_init(void)
{
	lcd_pin_config();
	delay_cycles_ms(1000);
	lcd_spi_init();
	delay_cycles_ms(1000);

	
	static uint8_t ST7735_cmds[]  =
	{
		ST7735_SWRESET, 0, 150,       // Software reset. This first one is needed because of the RC reset.
		ST7735_SLPOUT, 0, 255,       // Exit sleep mode
		ST7735_FRMCTR1, 3, 0x01, 0x2C, 0x2D, 0,  // Frame rate control 1
		ST7735_FRMCTR2, 3, 0x01, 0x2C, 0x2D, 0,  // Frame rate control 2
		ST7735_FRMCTR3, 6, 0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D, 0,  // Frame rate control 3
		ST7735_INVCTR, 1, 0x07, 0,	// Display inversion
		ST7735_PWCTR1, 3, 0x0A, 0x02, 0x84, 5, // Power control 1
		ST7735_PWCTR2, 1, 0xC5, 5,       // Power control 2
		ST7735_PWCTR3, 2, 0x0A, 0x00, 5,	// Power control 3
		ST7735_PWCTR4, 2, 0x8A, 0x2A, 5,	// Power control 4
		ST7735_PWCTR5, 2, 0x8A, 0xEE, 5,	// Power control 5
		ST7735_VMCTR1, 1, 0x0E, 0, // Vcom control 1
		ST7735_INVOFF, 0, 0,	//Inversion off
		ST7735_MADCTL, 1, 0xC8, 0,	//Memory Access control
		ST7735_COLMOD, 1, 0x05, 0,	//Interface pixel format
		ST7735_CASET, 4, 0x00, 0x00, 0x00, 0x7F, 0,		//Column
		ST7735_RASET, 4, 0x00, 0x00, 0x00, 0x9F, 0,		//Page
		ST7735_GMCTRP1, 16, 0x02, 0x1C, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2D,
		0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10, 0, // Positive Gamma
		ST7735_GMCTRN1, 16, 0x03, 0x1D, 0x07, 0x06, 0x2E, 0x2C, 0x29,0x2D,
		0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10, 0, // Negative Gamma
		ST7735_NORON, 0, 10,	//Normal display on
		ST7735_DISPON, 0, 100,            // Set display on
		ST7735_MADCTL, 1, MADCTL_MX | MADCTL_MV | MADCTL_RGB, 10		//Default to rotation 3
	};
	
	sendCommands(ST7735_cmds, 22);
}

/**************************************************************************//**
* @fn			void LCD_rotate(uint8_t r){
* @brief		Rotate display to another orientation
* @note
*****************************************************************************/
void LCD_rotate(uint8_t r){
	uint8_t madctl = 0;
	uint8_t rotation = r % 4; // can't be higher than 3

	switch (rotation) {
		case 0:
			madctl = MADCTL_MX | MADCTL_MY | MADCTL_RGB;
			break;
		case 1:
			madctl = MADCTL_MY | MADCTL_MV | MADCTL_RGB;
			break;
		case 2:
			madctl = MADCTL_RGB;
			break;
		case 3:
			madctl = MADCTL_MX | MADCTL_MV | MADCTL_RGB;
			break;
		default:
			madctl = MADCTL_MX | MADCTL_MY | MADCTL_RGB;
			break;
	}
	
	uint8_t ST7735_cmds[]  =
	{
		ST7735_MADCTL, 1, madctl, 0
	};
	
	sendCommands(ST7735_cmds, 1);
}

/**************************************************************************//**
* @fn			void LCD_setAddr(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1)
* @brief		Set pixel memory address to write to
* @note
*****************************************************************************/
void LCD_setAddr(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
	uint8_t ST7735_cmds[]  =
	{
		ST7735_CASET, 4, 0x00, x0, 0x00, x1, 0,		
		ST7735_RASET, 4, 0x00, y0, 0x00, y1, 0,		
		ST7735_RAMWR, 0, 5				
	};
	sendCommands(ST7735_cmds, 3);
}

/**************************************************************************//**
* @fn			void LCD_drawPixel(uint8_t x, uint8_t y, uint16_t color)
* @brief		Draw a single pixel to a color
* @note
*****************************************************************************/
void LCD_drawPixel(uint8_t x, uint8_t y, uint16_t color) {
	LCD_setAddr(x,y,x,y);
	Adafruit358LCD_spi_send_data16(color);
}

/**************************************************************************//**
* @fn			void LCD_drawBlock(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1,uint16_t color)
* @brief		Draw a colored block at coordinates
* @note
*****************************************************************************/
void LCD_drawBlock(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1,uint16_t color) {
	int i;
	
	if (x0 > x1) {
		swap_num(&x0, &x1);
	}
	if (y0 > y1) {
		swap_num(&y0, &y1);
	}
	
	LCD_setAddr(x0, y0, x1, y1);
	
	Adafruit358LCD_spi_select_device();		//Manually control CS
	DISPLAY_DC_HIGH();
	for (i = 0; i < ((x1-x0+1)*(y1-y0+1)); i++)
	{
		Adafruit358LCD_spi_send_16bit_stream(color);
	}

	Adafruit358LCD_spi_deselect_device();		//Manually control CS
}

/**************************************************************************//**
* @fn			void LCD_setScreen(uint16_t color)
* @brief		Draw the entire screen to a color
* @note
*****************************************************************************/
void LCD_setScreen(uint16_t color) {
	LCD_setAddr(0, 0, LCD_WIDTH, LCD_HEIGHT);
	
	Adafruit358LCD_spi_select_device();		//Manually control CS
	DISPLAY_DC_HIGH();
	
	for (int i = 0; i < LCD_SIZE; i++){
		Adafruit358LCD_spi_send_16bit_stream(color);
	}
	
	Adafruit358LCD_spi_deselect_device();		//Manually control CS
}

/**************************************************************************//**
* @fn			void LCD_drawLine(short x0,short y0,short x1,short y1,uint16_t c)
* @brief		Draw a line from and to a point with a color
* @note
*****************************************************************************/
void LCD_drawLine(short x0,short y0,short x1,short y1,uint16_t c){
	int16_t steep = abs(y1 - y0) > abs(x1 - x0);
	if (steep) {
		swap_num(&x0, &y0);
		swap_num(&x1, &y1);
	}

	if (x0 > x1) {
		swap_num(&x0, &x1);
		swap_num(&y0, &y1);
	}

	int16_t dx, dy;
	dx = x1 - x0;
	dy = abs(y1 - y0);

	int16_t err = dx / 2;
	int16_t ystep;

	if (y0 < y1) {
		ystep = 1;
		} else {
		ystep = -1;
	}

	for (; x0<=x1; x0++) {
		if (steep) {
			LCD_drawPixel(y0, x0, c);
			} else {
			LCD_drawPixel(x0, y0, c);
		}
		err -= dy;
		if (err < 0) {
			y0 += ystep;
			err += dx;
		}
	}
}

/**************************************************************************//**
* @fn			void LCD_drawYLine(uint8_t y,uint16_t c)
* @brief		Draw a vertical line with a color
* @note
*****************************************************************************/
void LCD_drawYLine(uint8_t y,uint16_t c){
	
	LCD_setAddr(y, 0, y, LCD_HEIGHT);
	Adafruit358LCD_spi_select_device();		//Manually control CS
	DISPLAY_DC_HIGH();

	for (int i = 0; i < LCD_HEIGHT; i++){
		Adafruit358LCD_spi_send_16bit_stream(c);
	}

	Adafruit358LCD_spi_deselect_device();		//Manually control CS
}

/**************************************************************************//**
* @fn			void LCD_drawXLine(uint8_t y,uint16_t c)
* @brief		Draw a horizontal line with a color
* @note
*****************************************************************************/
void LCD_drawXLine(uint8_t x,uint16_t c){
		
	LCD_setAddr(0, x, LCD_WIDTH, x);
	Adafruit358LCD_spi_select_device();		//Manually control CS
	DISPLAY_DC_HIGH();

	for (int i = 0; i < LCD_WIDTH; i++){
		Adafruit358LCD_spi_send_16bit_stream(c);
	}

	Adafruit358LCD_spi_deselect_device();		//Manually control CS
}

/**************************************************************************//**
* @fn			void LCD_drawChar(uint8_t x, uint8_t y, uint16_t character, uint16_t fColor, uint16_t bColor)
* @brief		Draw a character starting at the point with foreground and background colors
* @note
*****************************************************************************/
void LCD_drawChar(uint8_t x, uint8_t y, uint16_t character, uint16_t fColor, uint16_t bColor){
	uint16_t row = character - 0x20;		//Determine row of ASCII table starting at space
	int i, j;
	if ((LCD_WIDTH-x>7)&&(LCD_HEIGHT-y>7)){
		for(i=0;i<5;i++){
			uint8_t pixels = ASCII[row][i]; //Go through the list of pixels
			for(j=0;j<8;j++){
				if ((pixels>>j)&1==1){
					LCD_drawPixel(x+i,y+j,fColor);
				}
				else {
					LCD_drawPixel(x+i,y+j,bColor);
				}
			}
		}
	}
}

/**************************************************************************//**
* @fn			void LCD_drawFastChar(uint8_t x, uint8_t y, uint16_t character, uint16_t fColor, uint16_t bColor)
* @brief		Draw block then character on top
* @note
*****************************************************************************/
void LCD_drawFastChar(uint8_t x, uint8_t y, uint16_t character, uint16_t fColor, uint16_t bColor){
	uint16_t row = character - 0x20;		//Determine row of ASCII table starting at space
	int i, j;
	
	LCD_drawBlock(x,y,x+5,y+9,bColor);
		
	if ((LCD_WIDTH-x>=5)&&(LCD_HEIGHT-y>=8)){
		for(i=0;i<5;i++){
			char pixels = ASCII[row][i]; //Go through the list of pixels
			for(j=0;j<9;j++){
				if ((pixels>>j)&1==1){
					LCD_drawPixel(x+i,y+j+1,fColor);
				}
			}
		}
	}
}

/**************************************************************************//**
* @fn			void LCD_drawString(uint8_t x, uint8_t y, char* str, uint16_t fg, uint16_t bg)
* @brief		Draw a string starting at the point with foreground and background colors
* @note
*****************************************************************************/
void LCD_drawString(uint8_t x, uint8_t y, char* str, uint16_t fg, uint16_t bg){
	int i = 0;
	while(str[i]){
		LCD_drawFastChar(x+5*i,y,str[i],fg,bg);
		i++;
	}
}

/**************************************************************************//**
* @fn			void draw_circle_arc(int x0, int y0, int r, bool down
* @brief		draw arc
* @note
*****************************************************************************/
void lcd_draw_circle_arc(int x0, int y0, int r, bool down, uint16_t HEART_COLOR) {
	int x = -r;
	int y = 0;
	int err = 2 - 2 * r;
	int x_start, x_end;

	do {
		x_start = x0 - x;
		x_end = x0 + x;

		if (down) {
			LCD_drawLine(x_start, y0 + y, x_end, y0 + y, HEART_COLOR);
			} else {
			LCD_drawLine(x_start, y0 - y, x_end, y0 - y, HEART_COLOR);
		}

		r = err;
		if (r <= y) {
			err += ++y * 2 + 1;
		}
		if (r > x || err > y) {
			err += ++x * 2 + 1;
		}
	} while (x < 0);
}



/**************************************************************************//**
* @fn			void draw_heart(int x, int y, int r, uint16 HEART_COLOR)
* @brief		draw heart
* @note
*****************************************************************************/
void draw_heart(int x, int y, int r, uint16_t HEART_COLOR) {
	lcd_draw_circle_arc(x - r / 2, y - r / 3, r / 2, false, HEART_COLOR);
	lcd_draw_circle_arc(x + r / 2, y - r / 3, r / 2, false, HEART_COLOR);

	for (int i = 0; i < r; i++) {
		LCD_drawLine(x - r + i, y + i - 26 / 2, x + r - i, y + i - 26 / 2, HEART_COLOR);
	}
}

/**************************************************************************//**
* @fn			void draw_title(const char *title)
* @brief		draw title
* @note
*****************************************************************************/
void draw_title(const char *title) {
	int x = (SCREEN_WIDTH - strlen(title) * 6) / 2;
	int y = 10;

	for (size_t i = 0; i < strlen(title); i++) {
		LCD_drawChar(x + i * 6, y, title[i], TITLE_COLOR, BG_COLOR);
	}
}


/**************************************************************************//**
* @fn			void draw_title(const char *title)
* @brief		draw title
* @note
*****************************************************************************/
void draw_border(int thickness, uint16_t color) {
	for (int i = 0; i < thickness; i++) {
		LCD_drawXLine(i, color);
		LCD_drawYLine(i, color);
		LCD_drawXLine(SCREEN_HEIGHT - 1 - i, color);
		LCD_drawYLine(SCREEN_WIDTH - 1 - i, color);
	}
}

void draw_icon(int x, int y, int size) {
	for (int i = x; i < x + size; i++) {
		for (int j = y; j < y + size; j++) {
			LCD_drawPixel(i, j, ICON_COLOR);
		}
	}
}


/**************************************************************************//**
* @fn			void draw_title(const char *title)
* @brief		draw title
* @note
*****************************************************************************/
void draw_filled_circle(int x0, int y0, int r, uint16_t color) {
	int x = -r;
	int y = 0;
	int err = 2 - 2 * r;

	do {
		LCD_drawLine(x0 - x, y0 - y, x0 - x, y0 + y, color);
		LCD_drawLine(x0 + x, y0 - y, x0 + x, y0 + y, color);

		r = err;
		if (r <= y) {
			err += ++y * 2 + 1;
		}
		if (r > x || err > y) {
			err += ++x * 2 + 1;
		}
	} while (x < 1);
}


/**************************************************************************//**
* @fn			void draw_moon_icon(int x, int y, int size)
* @note
*****************************************************************************/
void draw_moon_icon(int x, int y, int size) {
	int r_outer = size / 2;
	int r_inner = r_outer * 3 / 4;
	int offset = r_outer - r_inner;

	draw_filled_circle(x + r_outer , y + r_outer, r_outer, YELLOW);
	draw_filled_circle(x + r_outer + offset , y + r_outer, r_inner, BG_COLOR);
}


/**************************************************************************//**
* @fn			void draw_boot_screen_title(const char *title)
* @brief		draw title
* @note
*****************************************************************************/
void draw_boot_screen_title(const char *title) {
	int x = (SCREEN_WIDTH - strlen(title) * 6) / 2; 
	int y = 30;

	for (size_t i = 0; i < strlen(title); i++) {
		LCD_drawChar(x + i * 6, y, title[i], BOOT_SCREEN_TITLE_COLOR, BOOT_SCREEN_BG_COLOR);
	}
}


/**************************************************************************//**
* @fn			void draw_boot_screen_subtitle(const char *subtitle)
* @brief		draw title
* @note
*****************************************************************************/
void draw_boot_screen_subtitle(const char *subtitle) {
	int x = (SCREEN_WIDTH - strlen(subtitle) * 6) / 2; 
	int y = 60;

	for (size_t i = 0; i < strlen(subtitle); i++) {
		LCD_drawChar(x + i * 6, y, subtitle[i], BOOT_SCREEN_SUBTITLE_COLOR, BOOT_SCREEN_BG_COLOR);
	}
}


/**************************************************************************//**
* @fn			void draw_rotating_squares(int centerX, int centerY, int num_squares, uint16_t color)
* @brief		draw title
* @note
*****************************************************************************/
void draw_rotating_squares(int centerX, int centerY, int num_squares, uint16_t color) {
	for (int i = 0; i < num_squares; i++) {
		int side_length = i * 4;
		int x = centerX - side_length / 2;
		int y = centerY - side_length / 2;

		LCD_drawLine(x, y, x + side_length, y, color);
		LCD_drawLine(x + side_length, y, x + side_length, y + side_length, color);
		LCD_drawLine(x + side_length, y + side_length, x, y + side_length, color);
		LCD_drawLine(x, y + side_length, x, y, color);

		centerX += 2;
		centerY += 2;
	}
}


/**************************************************************************//**
* @fn			void draw_thick_border(int thickness, uint16_t color1, uint16_t color2, uint16_t color3)
* @brief		draw title
* @note
*****************************************************************************/
void draw_thick_border(int thickness, uint16_t color1, uint16_t color2, uint16_t color3) {
	for (int i = 0; i < thickness; i++) {
		uint16_t color = (i % 3 == 0) ? color1 : ((i % 3 == 1) ? color2 : color3);

		LCD_drawLine(i, i, SCREEN_WIDTH - 1 - i, i, color);

		LCD_drawLine(SCREEN_WIDTH - 1 - i, i, SCREEN_WIDTH - 1 - i, SCREEN_HEIGHT - 1 - i, color);

		LCD_drawLine(SCREEN_WIDTH - 1 - i, SCREEN_HEIGHT - 1 - i, i, SCREEN_HEIGHT - 1 - i, color);

		LCD_drawLine(i, SCREEN_HEIGHT - 1 - i, i, i, color);
	}
}


/**************************************************************************//**
* @fn			void draw_main_page(void)
* @brief		draw title
* @note
*****************************************************************************/
void draw_main_page(void){
	
		LCD_setScreen(BG_COLOR);
		
		draw_border(3, WHITE);
		
		draw_title("IOT Watch V1.0");
		
		draw_moon_icon(SCREEN_WIDTH / 2 - 70, SCREEN_HEIGHT / 2 - 60, 20);
		
		LCD_drawString(25, 40, "M S G :", YELLOW, BLACK);
		
		LCD_drawString(25, 80, "T I M E :", YELLOW, BLACK);
		
		LCD_fillCircle(30, 65, 3, YELLOW);
		
		LCD_fillCircle(30, 105, 3, YELLOW);
		
}


/**************************************************************************//**
* @fn			void LCD_fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color
* @brief		draw circle
* @note
*****************************************************************************/
void LCD_fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
	int16_t x = -r;
	int16_t y = 0;
	int16_t err = 2 - 2 * r;

	do {
		LCD_drawLine(x0 - x, y0 + y, x0 + x, y0 + y, color);
		LCD_drawLine(x0 - x, y0 - y, x0 + x, y0 - y, color);

		int32_t e2 = err;

		if (e2 <= y) {
			err += ++y * 2 + 1;

			if (-x == y && e2 <= x)
			e2 = 0;
		}

		if (e2 > x)
		err += ++x * 2 + 1;

	} while (x <= 0);
}