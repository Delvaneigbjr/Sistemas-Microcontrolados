/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2022 STMicroelectronics.
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

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define KIT_LED_Pin GPIO_PIN_13
#define KIT_LED_GPIO_Port GPIOC
#define LedTimer1_Pin GPIO_PIN_0
#define LedTimer1_GPIO_Port GPIOA
#define LedTimer2_Pin GPIO_PIN_1
#define LedTimer2_GPIO_Port GPIOA
#define LedTimer3_Pin GPIO_PIN_2
#define LedTimer3_GPIO_Port GPIOA
#define LedTimer4_Pin GPIO_PIN_3
#define LedTimer4_GPIO_Port GPIOA
#define LedTimer5_Pin GPIO_PIN_4
#define LedTimer5_GPIO_Port GPIOA
#define LedOnOff_Pin GPIO_PIN_5
#define LedOnOff_GPIO_Port GPIOA
#define Modo1_Pin GPIO_PIN_6
#define Modo1_GPIO_Port GPIOA
#define Modo2_Pin GPIO_PIN_7
#define Modo2_GPIO_Port GPIOA
#define PotUmidade_Pin GPIO_PIN_0
#define PotUmidade_GPIO_Port GPIOB
#define Buzzer_Pin GPIO_PIN_15
#define Buzzer_GPIO_Port GPIOB
#define Modo3_Pin GPIO_PIN_8
#define Modo3_GPIO_Port GPIOA
#define BotaoModo_Pin GPIO_PIN_9
#define BotaoModo_GPIO_Port GPIOA
#define BotaoTimer_Pin GPIO_PIN_10
#define BotaoTimer_GPIO_Port GPIOA
#define BotaoOnOff_Pin GPIO_PIN_11
#define BotaoOnOff_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
