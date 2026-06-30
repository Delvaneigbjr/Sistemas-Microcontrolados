/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include "LCD_I2C.h"
#include "Modelitos.h"
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
#define LED_Black_Pin GPIO_PIN_13
#define LED_Black_GPIO_Port GPIOC
#define Esquentando_Pin GPIO_PIN_0
#define Esquentando_GPIO_Port GPIOA
#define Esfriando_Pin GPIO_PIN_1
#define Esfriando_GPIO_Port GPIOA
#define Irrigando_Pin GPIO_PIN_2
#define Irrigando_GPIO_Port GPIOA
#define Alerta_Pin GPIO_PIN_3
#define Alerta_GPIO_Port GPIOA
#define Ligado_Pin GPIO_PIN_4
#define Ligado_GPIO_Port GPIOA
#define Luminosidade_Pin GPIO_PIN_5
#define Luminosidade_GPIO_Port GPIOA
#define Umidade_Pin GPIO_PIN_6
#define Umidade_GPIO_Port GPIOA
#define Temperatura_Pin GPIO_PIN_7
#define Temperatura_GPIO_Port GPIOA
#define Esquerda_Pin GPIO_PIN_0
#define Esquerda_GPIO_Port GPIOB
#define Esquerda_EXTI_IRQn EXTI0_IRQn
#define Enter_Pin GPIO_PIN_1
#define Enter_GPIO_Port GPIOB
#define Enter_EXTI_IRQn EXTI1_IRQn
#define Direita_Pin GPIO_PIN_2
#define Direita_GPIO_Port GPIOB
#define Direita_EXTI_IRQn EXTI2_IRQn
#define Esc_Pin GPIO_PIN_10
#define Esc_GPIO_Port GPIOB
#define Esc_EXTI_IRQn EXTI15_10_IRQn

/* USER CODE BEGIN Private defines */
typedef enum
{
	Esquerda,
	Enter,
	Direita,
	Esc,
	Nada
} Botoes;

typedef enum
{
	Geral,
	Luminosidade,
	Umidade,
	Temperatura,
	Alertas,
	Tempos,
	Configuracoes
}TipoTela;
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
