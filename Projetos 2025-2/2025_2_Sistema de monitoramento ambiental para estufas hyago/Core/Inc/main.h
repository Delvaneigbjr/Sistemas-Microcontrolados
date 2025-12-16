/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header file for main.c (corrigido)
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* USER CODE BEGIN Includes */
#include <stdbool.h>
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* ===============================
      ESTADOS DO SISTEMA
   =============================== */
typedef enum {
    MONITORAMENTO_PRINCIPAL = 0,
    MENU_MODO,
    AJUSTE_MANUAL,
    ALERTA_CRITICO
} Estado_Controlador;

/* ===============================
        MODO DE OPERAÇÃO
   =============================== */
typedef enum {
    MODO_MANUAL = 0,
    MODO_AUTOMATICO
} Modo_Operacao;

/* ===============================
     ÍNDICES DE SETPOINTS
   =============================== */
typedef enum {
    SP_TEMP_MAX = 0,
    SP_TEMP_MIN,
    SP_UMIDADE_MAX,
    SP_UMIDADE_MIN,
    SP_OXIGENIO_MAX,
    SP_OXIGENIO_MIN,
    SP_LUZ_MAX,
    SP_LUZ_MIN,
    SP_SAIR
} Config_Setpoints_Index;

/* ===============================
      DADOS DO CONTROLADOR
   =============================== */
typedef struct {
    float temperatura;
    float umidade;
    float oxigenio;

    float sp_temp_max;
    float sp_temp_min;
    float sp_umidade_max;
    float sp_umidade_min;
    float sp_oxigenio_max;
    float sp_oxigenio_min;

    bool aquecimento_ativo;
    bool resfriamento_ativo;
    bool led_alerta_ativo;
    bool led_teste_ativo;
} Controlador_Dados;

/* Variáveis globais declaradas no main.c */
extern Estado_Controlador g_estado_atual;
extern Modo_Operacao g_modo_operacao;
extern Controlador_Dados g_dados_controlador;
extern bool buzzer_temp_alerta;

/* USER CODE END ET */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
void Transicao_FSM(void);
void Buzzer_TimerHandler(void);

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
/* USER CODE BEGIN Private defines */

/* =========== DEFINIÇÃO DOS PINOS =========== */

#define SERVO_Pin GPIO_PIN_6
#define SERVO_GPIO_Port GPIOA

#define POUCA_LUZ_Pin GPIO_PIN_4
#define POUCA_LUZ_GPIO_Port GPIOA

#define MUITA_LUZ_Pin GPIO_PIN_5
#define MUITA_LUZ_GPIO_Port GPIOA

#define RESFRIAMENTO_Pin GPIO_PIN_8
#define RESFRIAMENTO_GPIO_Port GPIOA

#define AQUECIMENTO_Pin GPIO_PIN_9
#define AQUECIMENTO_GPIO_Port GPIOA

#define LED_RGB_VERMELHO_Pin GPIO_PIN_10
#define LED_RGB_VERMELHO_GPIO_Port GPIOA

#define LED_RGB_VERDE_Pin GPIO_PIN_11
#define LED_RGB_VERDE_GPIO_Port GPIOA

#define LED_RGB_AZUL_Pin GPIO_PIN_12
#define LED_RGB_AZUL_GPIO_Port GPIOA

#define BUZZER_Pin GPIO_PIN_15
#define BUZZER_GPIO_Port GPIOB

#define BOTAO_VERDE_Pin GPIO_PIN_3
#define BOTAO_VERDE_GPIO_Port GPIOB

#define BOTAO_AMARELO_Pin GPIO_PIN_4
#define BOTAO_AMARELO_GPIO_Port GPIOB

#define BOTAO_ROXO_Pin GPIO_PIN_0
#define BOTAO_ROXO_GPIO_Port GPIOB

#define BOTAO_VERMELHO_Pin GPIO_PIN_5
#define BOTAO_VERMELHO_GPIO_Port GPIOB

#define LCD_COLUMNS 16
#define LCD_ROWS    2

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
