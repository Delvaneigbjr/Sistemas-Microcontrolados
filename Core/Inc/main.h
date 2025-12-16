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

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define Embreagem_Pin GPIO_PIN_1
#define Embreagem_GPIO_Port GPIOA
#define Freio_Pin GPIO_PIN_2
#define Freio_GPIO_Port GPIOA
#define Acelerador_Pin GPIO_PIN_3
#define Acelerador_GPIO_Port GPIOA
#define Dim__Marcha_Pin GPIO_PIN_4
#define Dim__Marcha_GPIO_Port GPIOA
#define Dim__Marcha_EXTI_IRQn EXTI4_IRQn
#define Aum__Marcha_Pin GPIO_PIN_5
#define Aum__Marcha_GPIO_Port GPIOA
#define Aum__Marcha_EXTI_IRQn EXTI9_5_IRQn
#define RPM_Alto_2_Pin GPIO_PIN_15
#define RPM_Alto_2_GPIO_Port GPIOB
#define RPM_Alto_1_Pin GPIO_PIN_8
#define RPM_Alto_1_GPIO_Port GPIOA
#define RPM_M_dio_2_Pin GPIO_PIN_9
#define RPM_M_dio_2_GPIO_Port GPIOA
#define RPM_M_dio_1_Pin GPIO_PIN_10
#define RPM_M_dio_1_GPIO_Port GPIOA
#define RPM_Leve_2_Pin GPIO_PIN_11
#define RPM_Leve_2_GPIO_Port GPIOA
#define RPM_Leve_1_Pin GPIO_PIN_12
#define RPM_Leve_1_GPIO_Port GPIOA
#define Alerta_de_RPM_Pin GPIO_PIN_9
#define Alerta_de_RPM_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
