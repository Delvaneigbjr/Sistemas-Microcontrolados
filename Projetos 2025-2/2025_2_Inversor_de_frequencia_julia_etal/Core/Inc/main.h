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
#define MA1_Pin GPIO_PIN_4
#define MA1_GPIO_Port GPIOA
#define MA2_Pin GPIO_PIN_5
#define MA2_GPIO_Port GPIOA
#define MB1_Pin GPIO_PIN_6
#define MB1_GPIO_Port GPIOA
#define MB2_Pin GPIO_PIN_7
#define MB2_GPIO_Port GPIOA
#define Led_liga_motor_A_Pin GPIO_PIN_10
#define Led_liga_motor_A_GPIO_Port GPIOB
#define BM4_Pin GPIO_PIN_12
#define BM4_GPIO_Port GPIOB
#define BM3_Pin GPIO_PIN_13
#define BM3_GPIO_Port GPIOB
#define BM2_Pin GPIO_PIN_14
#define BM2_GPIO_Port GPIOB
#define BM1_Pin GPIO_PIN_15
#define BM1_GPIO_Port GPIOB
#define Led_liga_motor_B_Pin GPIO_PIN_8
#define Led_liga_motor_B_GPIO_Port GPIOA
#define Led_troca_sentido_motor_A_Pin GPIO_PIN_9
#define Led_troca_sentido_motor_A_GPIO_Port GPIOA
#define Bmenu_Pin GPIO_PIN_10
#define Bmenu_GPIO_Port GPIOA
#define BmenuA11_Pin GPIO_PIN_11
#define BmenuA11_GPIO_Port GPIOA
#define BmenuA12_Pin GPIO_PIN_12
#define BmenuA12_GPIO_Port GPIOA
#define Led_troca_sentido_motor_B_Pin GPIO_PIN_3
#define Led_troca_sentido_motor_B_GPIO_Port GPIOB
#define Led_acelera_3_Pin GPIO_PIN_5
#define Led_acelera_3_GPIO_Port GPIOB
#define Led_acelera_2_Pin GPIO_PIN_8
#define Led_acelera_2_GPIO_Port GPIOB
#define Led_acelera_1_Pin GPIO_PIN_9
#define Led_acelera_1_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
