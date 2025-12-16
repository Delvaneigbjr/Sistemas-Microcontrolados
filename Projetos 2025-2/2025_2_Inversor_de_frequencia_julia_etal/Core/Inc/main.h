/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LED_KIT_Pin GPIO_PIN_13
#define LED_KIT_GPIO_Port GPIOC
#define PROTECAO_Pin GPIO_PIN_1
#define PROTECAO_GPIO_Port GPIOB
#define PROTECAO_EXTI_IRQn EXTI1_IRQn
#define BT_RETURN_Pin GPIO_PIN_12
#define BT_RETURN_GPIO_Port GPIOB
#define BT_UP_Pin GPIO_PIN_13
#define BT_UP_GPIO_Port GPIOB
#define BT_DOWN_Pin GPIO_PIN_14
#define BT_DOWN_GPIO_Port GPIOB
#define BT_OK_Pin GPIO_PIN_15
#define BT_OK_GPIO_Port GPIOB
#define LED_MENU1_Pin GPIO_PIN_8
#define LED_MENU1_GPIO_Port GPIOA
#define LED_MENU2_Pin GPIO_PIN_9
#define LED_MENU2_GPIO_Port GPIOA
#define LED_MENU3_Pin GPIO_PIN_10
#define LED_MENU3_GPIO_Port GPIOA
#define LED_MENU4_Pin GPIO_PIN_11
#define LED_MENU4_GPIO_Port GPIOA
#define STATUS1_Pin GPIO_PIN_12
#define STATUS1_GPIO_Port GPIOA
#define STATUS2_Pin GPIO_PIN_15
#define STATUS2_GPIO_Port GPIOA
#define BT_START_Pin GPIO_PIN_3
#define BT_START_GPIO_Port GPIOB
#define BT_SENTIDO_Pin GPIO_PIN_4
#define BT_SENTIDO_GPIO_Port GPIOB
#define BT_STOP_Pin GPIO_PIN_5
#define BT_STOP_GPIO_Port GPIOB
#define STATUS3_Pin GPIO_PIN_8
#define STATUS3_GPIO_Port GPIOB
#define STATUS4_Pin GPIO_PIN_9
#define STATUS4_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
