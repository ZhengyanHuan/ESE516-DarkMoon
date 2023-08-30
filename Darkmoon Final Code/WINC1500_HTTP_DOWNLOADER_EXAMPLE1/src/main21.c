/**
 * @file      main.c
 * @brief     Main application entry point
 * @author    Eduardo Garcia
 * @date      2022-04-14
 * @copyright Copyright Bresslergroup\n
 *            This file is proprietary to Bresslergroup.
 *            All rights reserved. Reproduction or distribution, in whole
 *            or in part, is forbidden except by express written permission
 *            of Bresslergroup.
 ******************************************************************************/

/****
 * Includes
 ******************************************************************************/
#include <errno.h>

#include "CliThread/CliThread.h"
#include "ControlThread\ControlThread.h"
#include "DistanceDriver\DistanceSensor.h"
#include "FreeRTOS.h"
#include "Seesaw\Seesaw.h"
#include "SerialConsole.h"
#include "UiHandlerThread\UiHandlerThread.h"
#include "WifiHandlerThread/WifiHandler.h"
#include "asf.h"
#include "driver/include/m2m_wifi.h"
#include "main.h"
#include "stdio_serial.h"
#include "LIS2DH12/lis2dh12_reg.h"
#include "HeartClick/driver_max30102_interface.h"
#include "HeartClick/driver_max30102_fifo.h"
#include "LCD\Adafruit358.h"
/****
 * Defines and Types
 ******************************************************************************/
#define APP_TASK_ID 0 /**< @brief ID for the application task */
#define CLI_TASK_ID 1 /**< @brief ID for the command line interface task */

/****
 * Local Function Declaration
 ******************************************************************************/
void vApplicationIdleHook(void);
//!< Initial task used to initialize HW before other tasks are initialized
static void StartTasks(void);
void vApplicationDaemonTaskStartupHook(void);

void vApplicationStackOverflowHook(void);
void vApplicationMallocFailedHook(void);
void vApplicationTickHook(void);

/****
 * Variables
 ******************************************************************************/
static TaskHandle_t cliTaskHandle = NULL;      //!< CLI task handle
static TaskHandle_t daemonTaskHandle = NULL;   //!< Daemon task handle
static TaskHandle_t wifiTaskHandle = NULL;     //!< Wifi task handle
static TaskHandle_t uiTaskHandle = NULL;       //!< UI task handle
static TaskHandle_t controlTaskHandle = NULL;  //!< Control task handle
static TaskHandle_t AccHandle = NULL;  

char bufferPrint[64];  ///< Buffer for daemon task

/**
 * @brief Main application function.
 * Application entry point.
 * @return int
 */
int main(void){
    /* Initialize the board. */
    system_init();

    /* Initialize the UART console. */
    InitializeSerialConsole();

    // Initialize trace capabilities
    vTraceEnable(TRC_START);
    // Start FreeRTOS scheduler
    vTaskStartScheduler();

    return 0;  // Will not get here
}

/**
 * function          vApplicationDaemonTaskStartupHook
 * @brief            Initialization code for all subsystems that require FreeRToS
 * @details			This function is called from the FreeRToS timer task. Any code
 *					here will be called before other tasks are initilized.
 * @param[in]        None
 * @return           None
 */
void vApplicationDaemonTaskStartupHook(void)
{
    SerialConsoleWriteString("\r\n\r\n-----ESE516 Main Program-----\r\n");

    // Initialize HW that needs FreeRTOS Initialization
    SerialConsoleWriteString("\r\n\r\nInitialize HW...\r\n");
    if (I2cInitializeDriver() != STATUS_OK) {
        SerialConsoleWriteString("Error initializing I2C Driver!\r\n");
    } else {
        SerialConsoleWriteString("Initialized I2C Driver!\r\n");
    }


	// init ACC
	/**/
	uint8_t whoamI_acc = 0;
	lis2dh12_device_id_get(GetAccStruct(), &whoamI_acc);
	
	if (whoamI_acc != LIS2DH12_ID) {
		SerialConsoleWriteString("Cannot find ACC!\r\n");
	} else {
		//SerialConsoleWriteString("ACC found!\r\n");
		if (init_LIS2DH12() == 0) {
			SerialConsoleWriteString("ACC initialized!\r\n");
		} else {
			SerialConsoleWriteString("Could not initialize ACC\r\n");
		}
	}
	
	
	// init max
	int error = max30102_init_f();
	error = max30102_fifo_init();
	if (error != 0)
	{
		SerialConsoleWriteString("Max initialized failed!\r\n");
	} else {
		SerialConsoleWriteString("Max initialized!\r\n");
	}
	
	
	// init lcd
	lcd_init();
	SerialConsoleWriteString("LCD initialized! \n\r");
	LCD_setScreen(BOOT_SCREEN_BG_COLOR);
	draw_boot_screen_title("W e l c o m e !");
	draw_boot_screen_subtitle("By ESE516 DarkMoon");
	LCD_setScreen(BG_COLOR);
	draw_border(3, WHITE);
	draw_rotating_squares(SCREEN_WIDTH / 2 + 20, SCREEN_HEIGHT / 2 + 10, 10, 0x07FF);
	draw_moon_icon(SCREEN_WIDTH / 2 - 50, SCREEN_HEIGHT / 2 - 40, 40);

    StartTasks();

    vTaskSuspend(daemonTaskHandle);
}

/**
 * function          StartTasks
 * @brief            Initialize application tasks
 * @details
 * @param[in]        None
 * @return           None
 */
static void StartTasks(void)
{
    snprintf(bufferPrint, 64, "Heap before starting tasks: %d\r\n", xPortGetFreeHeapSize());
    SerialConsoleWriteString(bufferPrint);

    // Initialize Tasks here

    if (xTaskCreate(vCommandConsoleTask, "CLI_TASK", 400, NULL, CLI_PRIORITY, &cliTaskHandle) != pdPASS) {
        SerialConsoleWriteString("ERR: CLI task could not be initialized!\r\n");
    }

    if (xTaskCreate(vWifiTask, "WIFI_TASK", 700, NULL, WIFI_PRIORITY, &wifiTaskHandle) != pdPASS) {
        SerialConsoleWriteString("ERR: WIFI task could not be initialized!\r\n");
    }
	
	if (xTaskCreate(vAccTask, "Acc_TASK", 200, NULL, WIFI_PRIORITY - 1, &AccHandle) != pdPASS) {
		SerialConsoleWriteString("ERR: Acc task could not be initialized!\r\n");
	}
	
    if (xTaskCreate(vUiHandlerTask, "UI Task", UI_TASK_SIZE, NULL, UI_TASK_PRIORITY, &uiTaskHandle) != pdPASS) {
	    SerialConsoleWriteString("ERR: UI task could not be initialized!\r\n");
    }
	
	snprintf(bufferPrint, 64, "Heap after starting UI: %d\r\n", xPortGetFreeHeapSize());
	SerialConsoleWriteString(bufferPrint);
}



void vApplicationMallocFailedHook(void)
{
    SerialConsoleWriteString("Error on memory allocation on FREERTOS!\r\n");
    while (1)
        ;
}

void vApplicationStackOverflowHook(void)
{
    SerialConsoleWriteString("Error on stack overflow on FREERTOS!\r\n");
    while (1)
        ;
}

#include "MCHP_ATWx.h"
void vApplicationTickHook(void)
{
    SysTick_Handler_MQTT();
}
