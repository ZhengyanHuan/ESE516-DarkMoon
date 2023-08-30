/**
* @file      UiHandlerThread.c
* @brief     File that contains the task code and supporting code for the UI
Thread for ESE516 Spring (Online) Edition
* @author    XY
* @date      2023-5-3

******************************************************************************/

/******************************************************************************
 * Includes
 ******************************************************************************/
#include "UiHandlerThread/UiHandlerThread.h"
#include <stdbool.h>

#include <errno.h>

#include "SerialConsole.h"
#include "WifiHandlerThread/WifiHandler.h"
#include "asf.h"
#include "main.h"
#include "LCD/Adafruit358.h"
#include "HeartClick/driver_max30102_interface.h"
#include "HeartClick/driver_max30102_fifo.h"
#include "LIS2DH12/lis2dh12_reg.h"

/******************************************************************************
 * Defines
 ******************************************************************************/
#define BUTTON_PRESSES_MAX 16  ///< Number of maximum button presses to analize in one go

/******************************************************************************
 * Variables
 ******************************************************************************/
uiStateMachine_state uiState;         ///< Holds the current state of the UI
static buttonControlStates buttonState;
static uint16_t pressTime; ///<Time button was pressed.
static bool buttonHeldActive;

/******************************************************************************
 * Forward Declarations
 ******************************************************************************/

/******************************************************************************
 * Callback Functions
 ******************************************************************************/

/******************************************************************************
 * Task Function
 ******************************************************************************/

/**
 * @fn		void vUiHandlerTask( void *pvParameters )
 * @brief	STUDENT TO FILL THIS
 * @details 	student to fill this
 * @param[in]	Parameters passed when task is initialized. In this case we can ignore them!
 * @return		Should not return! This is a task defining function.
 * @note
 */
void vUiHandlerTask(void *pvParameters)
{
    // Do initialization code here
    SerialConsoleWriteString("UI Task Started!");
    uiState = UI_STATE_INIT;  // Initial state
	
	// Initialize button
	struct port_config msg_pin;
	port_get_config_defaults(&msg_pin);
	msg_pin.direction = PORT_PIN_DIR_INPUT;
	msg_pin.input_pull = PORT_PIN_PULL_UP;
	port_pin_set_config(PIN_PA20, &msg_pin);
	
	struct port_config pin_conf;
	port_get_config_defaults(&pin_conf);
	pin_conf.direction = PORT_PIN_DIR_OUTPUT;
	port_pin_set_config(PIN_PA06, &pin_conf);

	int flag = 0;
	uint8_t raw_red_data = 0;
			uint8_t raw_ir_data = 0;
			uint8_t len = 1;
			char temp[30];
    while (1) {
		
		if (flag == 0){
			delay_cycles_ms(500);
			draw_main_page();
			flag = 1;
		}
		else if (flag == 1){
			if(!port_pin_get_input_level(PIN_PA20)){
				delay_cycles_ms(1500);
				if(!port_pin_get_input_level(PIN_PA20)){
					SerialConsoleWriteString("Temperature Mode!\r\n");
					flag = 3;
				}
				else{
					SerialConsoleWriteString("Heart Rate Mode!\r\n");
					flag = 2;
				}
			}else{
				flag = 1;
			}
		}
		else if (flag == 2) {
			port_pin_set_output_level(PIN_PA06, 1);
			delay_cycles_ms(1000);
			port_pin_set_output_level(PIN_PA06, 0);
			LCD_setScreen(BLACK);
			draw_border(3, RED);
			LCD_drawString(21,20,"H E A R T  R A T E",RED,BLACK);
			LCD_drawString(21,35,"Entering ... ...",rgb565(255,255,255),BLACK);
			LCD_drawString(21,35,"Entering ... ...",rgb565(255,255,255),BLACK);
			delay_cycles_ms(1000);
			
			LCD_setScreen(WHITE);
			draw_heart(80, 60, 40, RED);
			LCD_drawString(21,101,"Put on your finger, plz", BLACK, WHITE);
			LCD_drawString(21,111,"Starting ... ...       ", BLACK, WHITE);
			
			max30102_fifo_read(&raw_red_data, &raw_ir_data, &len);
			
			if (raw_red_data == 0){
				sprintf(temp, " H R :       FAILED !!  ", raw_red_data);
				LCD_drawString(21,101,temp, BLACK, WHITE);
			} else{
				sprintf(temp, " H R :       %d/min     ", raw_red_data);
				LCD_drawString(21,101,temp, BLACK, WHITE);
			}

			sprintf(temp, " S P O 2 :   %d         ", raw_ir_data);
			LCD_drawString(21,111,temp, BLACK, WHITE);
			delay_cycles_ms(20000);
			flag = 0;
			
		}
		else if (flag == 3) {
			port_pin_set_output_level(PIN_PA06, 1);
			delay_cycles_ms(500);
			port_pin_set_output_level(PIN_PA06, 0);
			delay_cycles_ms(500);
			port_pin_set_output_level(PIN_PA06, 1);
			delay_cycles_ms(500);
			port_pin_set_output_level(PIN_PA06, 0);
			delay_cycles_ms(500);
			LCD_setScreen(BLACK);
			draw_border(3, BLUE);
			LCD_drawString(21,20,"T E M P E R A T U R E",BLUE,BLACK);
			LCD_drawString(21,35,"Entering ... ...",rgb565(255,255,255),BLACK);
			LCD_drawString(21,35,"Entering ... ...",rgb565(255,255,255),BLACK);
			delay_cycles_ms(1000);
			static int16_t data_raw_temperature;
			uint8_t fla = 0;
			static float temperature_degC;
			char temp[20];
				
			stmdev_ctx_t *dev_ctx_lis = GetAccStruct();

				
			lis2dh12_temp_data_ready_get(dev_ctx_lis, &fla);

			if (fla) {
				/* Read temperature data */
				memset(&data_raw_temperature, 0x00, sizeof(int16_t));
				lis2dh12_temperature_raw_get(dev_ctx_lis, &data_raw_temperature);
				temperature_degC = lis2dh12_from_lsb_hr_to_celsius(data_raw_temperature);
				sprintf(temp, "Temperature: %d degC", (int)temperature_degC);
				SerialConsoleWriteString(temp);
				struct ImuDataPacket imuPacket;
					imuPacket.xmg = 0;
					imuPacket.ymg = 0;
					imuPacket.zmg = 0;
					imuPacket.alarm= false;
					imuPacket.temp = (int)temperature_degC;
					WifiAddImuDataToQueue(&imuPacket);
			}
			sprintf(temp, "T E M P:  %d DegC", (int)temperature_degC);
			LCD_setScreen(BLACK);
			LCD_drawString(21,50,temp,rgb565(255,255,255),BLACK);
			
			delay_cycles_ms(20000);
			flag = 0;
		}
		
        vTaskDelay(50);
    }
}

/******************************************************************************
 * Functions
 *****************************************************************************/
bool button_long_press(void){
	buttonState = BUTTON_RELEASED;
	pressTime = 0;
	uint flag = 0;
	
	struct tc_config config_tc;
	struct tc_module instance_tc;
	tc_get_config_defaults(&config_tc);
	
	// set time
	config_tc.clock_prescaler =  TC_CLOCK_PRESCALER_DIV1024;
	config_tc.counter_size = TC_COUNTER_SIZE_16BIT;
	tc_init(&instance_tc, TC3, &config_tc);
	tc_enable(&instance_tc);
	
	switch(buttonState){
				
				case(BUTTON_RELEASED):
				{
					if(port_pin_get_input_level(BUTTON_0_PIN) == false) //Button is logic low active
					{	
						tc_start_counter(&instance_tc);
						pressTime = tc_get_count_value(&instance_tc);
						buttonState = BUTTON_PRESSED;
						buttonHeldActive = false; //Reset flag used on button_press state
						flag = 0;
					} //Button is active low
					
					break;
				}
				
				case(BUTTON_PRESSED):
				{
					if(tc_get_count_value(&instance_tc) - pressTime >= BUTTON_PRESS_TIME_MS*10 && buttonHeldActive == false)
					{
						
						buttonHeldActive = true; //Set flag so message only happens once every press!
						//flag = 1;
					}
					
					
					if(port_pin_get_input_level(BUTTON_0_PIN) == true) //Button is logic low active
					{
						//Button release!
						buttonState = BUTTON_RELEASED;
					} //Button is active low
					
					break;
				}

				
		}
	return buttonHeldActive;
}

	