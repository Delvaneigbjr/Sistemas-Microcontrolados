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
#define LED_BARRA_1_Pin GPIO_PIN_5
#define LED_BARRA_1_GPIO_Port GPIOA
#define LED_BARRA_2_Pin GPIO_PIN_6
#define LED_BARRA_2_GPIO_Port GPIOA
#define LED_BARRA_3_Pin GPIO_PIN_7
#define LED_BARRA_3_GPIO_Port GPIOA
#define LED_BARRA_4_Pin GPIO_PIN_0
#define LED_BARRA_4_GPIO_Port GPIOB
#define LED_BARRA_5_Pin GPIO_PIN_1
#define LED_BARRA_5_GPIO_Port GPIOB
#define BTN_MENU_Pin GPIO_PIN_12
#define BTN_MENU_GPIO_Port GPIOB
#define BTN_MENU_EXTI_IRQn EXTI15_10_IRQn
#define BTN_SOBE_Pin GPIO_PIN_13
#define BTN_SOBE_GPIO_Port GPIOB
#define BTN_SOBE_EXTI_IRQn EXTI15_10_IRQn
#define BTN_DESCE_Pin GPIO_PIN_14
#define BTN_DESCE_GPIO_Port GPIOB
#define BTN_DESCE_EXTI_IRQn EXTI15_10_IRQn
#define BTN_EMERGENCIA_Pin GPIO_PIN_15
#define BTN_EMERGENCIA_GPIO_Port GPIOB
#define BTN_EMERGENCIA_EXTI_IRQn EXTI15_10_IRQn

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
