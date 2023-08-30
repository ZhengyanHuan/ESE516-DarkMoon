/**************************************************************************/ /**
 * @file      UiHandlerThread.h
 * @brief     File that contains the task code and supporting code for the UI Thread for ESE516 Spring (Online) Edition
 * @author    You! :)
 * @date      2020-04-09

 ******************************************************************************/

#pragma once
#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * Includes
 ******************************************************************************/
#include "WifiHandlerThread/WifiHandler.h"
/******************************************************************************
 * Defines
 ******************************************************************************/
#define UI_TASK_SIZE 700  //<Size of stack to assign to the UI thread. In words
#define UI_TASK_PRIORITY (configMAX_PRIORITIES - 1)
typedef enum uiStateMachine_state {
    //UI_STATE_HANDLE_BUTTONS = 0,  ///< State used to handle buttons
    //UI_STATE_IGNORE_PRESSES,      ///< State to ignore button presses
    //UI_STATE_SHOW_MOVES,          ///< State to show opponent's moves
    //UI_STATE_MAX_STATES           ///< Max number of states
	
	UI_STATE_INIT,
	UI_STATE_HEARTRATE,
	UI_STAY
} uiStateMachine_state;

#define BUTTON_PRESS_TIME_MS	1000 ///<Time, in ms, that a button must be pressed to count as a press.

//! Button state variable definition
typedef enum buttonControlStates
{
	BUTTON_RELEASED, ///<BUTTON IS RELEASED
	BUTTON_PRESSED, ///<BUTTON IS PRESSED
	BUTTON_MAX_STATES ///Max number of states.
}buttonControlStates;
/******************************************************************************
 * Structures and Enumerations
 ******************************************************************************/

/******************************************************************************
 * Global Function Declaration
 ******************************************************************************/
void vUiHandlerTask(void *pvParameters);
bool button_long_press(void);


#ifdef __cplusplus
}
#endif
