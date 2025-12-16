/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include <stdio.h>
#include <string.h>

// Definição dos LEDs (Ajustado para o hardware configurado PA0-PA4)
#define LED_TEMP_ALTA    GPIO_PIN_0
#define LED_TEMP_BAIXA   GPIO_PIN_1
#define LED_COMPRESSOR   GPIO_PIN_2
#define LED_NORMAL       GPIO_PIN_3
#define LED_ERRO         GPIO_PIN_4

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

typedef enum {
    ST_EMESPERA, ST_LENDO, ST_PROCESSANDO, ST_CONTROLE,
    ST_DISPLAY, ST_PROTECAO, ST_MENU, ST_EXIBIR_CELSIUS,
    ST_EXIBIR_FAHRENHEIT, ST_EXIBIR_KELVIN, ST_ZERAR_TEMP, ST_ERRO
} state_t;

state_t estado_atual = ST_EMESPERA;

float tensao1, tensao2, tensao3;
float temperatura_realc, temperatura_idealc, gap_temperaturac;
float temperaturaidealk, temperatura_idealf;
float offset_temperatura = 0.0;

uint32_t adc1, adc2, adc3;
uint8_t opcao_menu = 0;
uint8_t tela_atual = 0;
uint8_t compressor_ligado = 0;
uint16_t pwm_compressor = 0;
uint8_t unidade_temperatura = 0;  // 0=Celsius, 1=Fahrenheit, 2=Kelvin

uint32_t timer1 = 0;
uint32_t timer2 = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM2_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_I2C1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// Redireciona o printf para a porta serial (UART)
int __io_putchar(int ch) {
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 10);
    return ch;
}

// ========== FUNÇÕES DO LCD ==========
void lcd_send_cmd(char cmd)
{
    char data_u, data_l;
    uint8_t data_t[4];
    data_u = (cmd & 0xf0);
    data_l = ((cmd << 4) & 0xf0);
    data_t[0] = data_u | 0x0C;
    data_t[1] = data_u | 0x08;
    data_t[2] = data_l | 0x0C;
    data_t[3] = data_l | 0x08;
    HAL_I2C_Master_Transmit(&hi2c1, 0x4E, (uint8_t *)data_t, 4, 100);
}

void lcd_send_data(char data)
{
    char data_u, data_l;
    uint8_t data_t[4];
    data_u = (data & 0xf0);
    data_l = ((data << 4) & 0xf0);
    data_t[0] = data_u | 0x0D;
    data_t[1] = data_u | 0x09;
    data_t[2] = data_l | 0x0D;
    data_t[3] = data_l | 0x09;
    HAL_I2C_Master_Transmit(&hi2c1, 0x4E, (uint8_t *)data_t, 4, 100);
}

void lcd_init(void)
{
    HAL_Delay(50);
    lcd_send_cmd(0x30);
    HAL_Delay(5);
    lcd_send_cmd(0x30);
    HAL_Delay(1);
    lcd_send_cmd(0x30);
    HAL_Delay(10);
    lcd_send_cmd(0x20);
    HAL_Delay(10);
    lcd_send_cmd(0x28);
    HAL_Delay(1);
    lcd_send_cmd(0x08);
    HAL_Delay(1);
    lcd_send_cmd(0x01);
    HAL_Delay(1);
    HAL_Delay(1);
    lcd_send_cmd(0x06);
    HAL_Delay(1);
    lcd_send_cmd(0x0C);
}

void lcd_send_string(char *str)
{
    while (*str)
        lcd_send_data(*str++);
}

void lcd_clear(void)
{
    lcd_send_cmd(0x01);
    HAL_Delay(2);
}

void lcd_put_cur(int row, int col)
{
    switch (row)
    {
        case 0:
            col |= 0x80;
            break;
        case 1:
            col |= 0xC0;
            break;
    }
    lcd_send_cmd(col);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */


  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_TIM2_Init();
  MX_USART1_UART_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */

  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
  printf("Sistema Iniciado...\n");

  lcd_init();
  lcd_clear();
  printf("LCD Inicializado!\n");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    // Incrementa temporizadores
    timer1++;
    timer2++;

    switch (estado_atual)
    {
        case ST_EMESPERA:
            estado_atual = ST_LENDO;
            break;

        case ST_LENDO:
            if (timer1 >= 50) {
                timer1 = 0;

                // --- Canal 6 (PA6) ---
                ADC_ChannelConfTypeDef sConfig = {0};
                sConfig.Rank = 1;
                sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES; // Tempo maior!
                sConfig.Channel = ADC_CHANNEL_6;
                HAL_ADC_ConfigChannel(&hadc1, &sConfig);

                HAL_ADC_Start(&hadc1);
                HAL_ADC_PollForConversion(&hadc1, 100);
                adc1 = HAL_ADC_GetValue(&hadc1);
                HAL_ADC_Stop(&hadc1);

                // --- Canal 7 (PA7) ---
                sConfig.Channel = ADC_CHANNEL_7;
                HAL_ADC_ConfigChannel(&hadc1, &sConfig); // Reconfigura para o próximo canal

                HAL_ADC_Start(&hadc1);
                HAL_ADC_PollForConversion(&hadc1, 100);
                adc2 = HAL_ADC_GetValue(&hadc1);
                HAL_ADC_Stop(&hadc1);

                // --- Canal 8 (PB0) ---
                sConfig.Channel = ADC_CHANNEL_8;
                HAL_ADC_ConfigChannel(&hadc1, &sConfig);

                HAL_ADC_Start(&hadc1);
                HAL_ADC_PollForConversion(&hadc1, 100);
                adc3 = HAL_ADC_GetValue(&hadc1);
                HAL_ADC_Stop(&hadc1);

                tensao1 = (float)adc1;
                tensao2 = (float)adc2;
                tensao3 = (float)adc3;
            }
            estado_atual = ST_PROCESSANDO;
            break;

        case ST_PROCESSANDO:
            // Conversão com lógica de calibração
            temperatura_realc   = (tensao1 / 4095.0) * 100.0 - offset_temperatura;
            temperatura_idealc  = (tensao2 / 4095.0) * 100.0;
            gap_temperaturac    = (tensao3 / 4095.0) * 100.0;

            temperatura_idealf  = (temperatura_idealc * 9.0/5.0) + 32.0;
            temperaturaidealk   = temperatura_idealc + 273.15;

            estado_atual = ST_CONTROLE;
            break;

        case ST_CONTROLE:
            HAL_GPIO_WritePin(GPIOA, LED_TEMP_ALTA, 0);
            HAL_GPIO_WritePin(GPIOA, LED_TEMP_BAIXA, 0);
            HAL_GPIO_WritePin(GPIOA, LED_COMPRESSOR, 0);
            HAL_GPIO_WritePin(GPIOA, LED_NORMAL, 0);
            HAL_GPIO_WritePin(GPIOA, LED_ERRO, 0);

            if (timer2 >= 50) // Loop de controle lento
            {
                timer2 = 0;
                if (temperatura_realc > temperatura_idealc + gap_temperaturac){
                    HAL_GPIO_WritePin(GPIOA, LED_TEMP_ALTA, 1);
                    HAL_GPIO_WritePin(GPIOA, LED_COMPRESSOR, 1);
                    pwm_compressor = 100;
                    compressor_ligado = 1;
                }
                else if (temperatura_realc < temperatura_idealc - gap_temperaturac){
                    HAL_GPIO_WritePin(GPIOA, LED_TEMP_BAIXA, 1);
                    pwm_compressor = 0;
                    compressor_ligado = 0;
                }
                else {
                    HAL_GPIO_WritePin(GPIOA, LED_NORMAL, 1);
                    float diferenca = temperatura_realc - temperatura_idealc;
                    if(diferenca < 0) diferenca = 0; // Proteção matemática
                    pwm_compressor = (uint16_t)((diferenca / (gap_temperaturac + 0.1)) * 100);
                    if (pwm_compressor > 100) pwm_compressor = 100;
                    compressor_ligado = (pwm_compressor > 10);
                }

                HAL_GPIO_WritePin(GPIOA, LED_COMPRESSOR, compressor_ligado ? 1 : 0);
                __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, (pwm_compressor * 999) / 100);
            }
            estado_atual = ST_MENU;
            HAL_Delay(1);
            break;

        case ST_MENU:
            if (estado_atual == ST_MENU) estado_atual = ST_EXIBIR_CELSIUS;
            HAL_Delay(1);

            break;

        case ST_EXIBIR_CELSIUS:
        {
            // Converte floats para inteiros
            int temp_real_int, temp_ideal_int, gap_int, pwm_int;
            char lcd_buffer[16];
            char unidade_char[2] = "C";  // Padrão Celsius

            pwm_int = (int)pwm_compressor;

            // Seleciona a unidade e converte
            if (unidade_temperatura == 0) {
                // CELSIUS
                temp_real_int = (int)(temperatura_realc * 100);
                temp_ideal_int = (int)(temperatura_idealc * 100);
                gap_int = (int)(gap_temperaturac * 100);
                unidade_char[0] = 'C';
            }
            else if (unidade_temperatura == 1) {
                // FAHRENHEIT
                float temp_real_f = (temperatura_realc * 9.0/5.0) + 32.0;
                float temp_ideal_f = (temperatura_idealc * 9.0/5.0) + 32.0;
                float gap_f = (gap_temperaturac * 9.0/5.0);

                temp_real_int = (int)(temp_real_f * 100);
                temp_ideal_int = (int)(temp_ideal_f * 100);
                gap_int = (int)(gap_f * 100);
                unidade_char[0] = 'F';
            }
            else {  // unidade_temperatura == 2
                // KELVIN
                float temp_real_k = temperatura_realc + 273.15;
                float temp_ideal_k = temperatura_idealc + 273.15;
                float gap_k = gap_temperaturac;

                temp_real_int = (int)(temp_real_k * 100);
                temp_ideal_int = (int)(temp_ideal_k * 100);
                gap_int = (int)(gap_k * 100);
                unidade_char[0] = 'K';
            }

            // Exibe no LCD conforme a tela selecionada (tela_atual)
            if(tela_atual == 0) {
                lcd_clear();
                lcd_put_cur(0, 0);
                sprintf(lcd_buffer, "Real:%d.%02d%c", temp_real_int/100, temp_real_int%100, unidade_char[0]);
                lcd_send_string(lcd_buffer);

                lcd_put_cur(1, 0);
                sprintf(lcd_buffer, "Ideal:%d.%02d%c", temp_ideal_int/100, temp_ideal_int%100, unidade_char[0]);
                lcd_send_string(lcd_buffer);
            }
            else if(tela_atual == 1) {
                lcd_clear();
                lcd_put_cur(0, 0);
                sprintf(lcd_buffer, "Gap:%d.%02d%c", gap_int/100, gap_int%100, unidade_char[0]);
                lcd_send_string(lcd_buffer);

                lcd_put_cur(1, 0);
                lcd_send_string("Press botoes");
            }
            else if(tela_atual == 2) {
                lcd_clear();
                lcd_put_cur(0, 0);
                sprintf(lcd_buffer, "PWM:%d%%", pwm_int);
                lcd_send_string(lcd_buffer);

                lcd_put_cur(1, 0);
                if (unidade_temperatura == 0) lcd_send_string("Celsius");
                else if (unidade_temperatura == 1) lcd_send_string("Fahrenheit");
                else lcd_send_string("Kelvin");
            }

            // Também imprime no serial para debug
            printf("Temp Real: %d.%02d%c | Ideal: %d.%02d%c\r\n",
                   temp_real_int/100, temp_real_int%100, unidade_char[0],
                   temp_ideal_int/100, temp_ideal_int%100, unidade_char[0]);

            HAL_Delay(50);
            estado_atual = ST_PROTECAO;
            break;
        }

        case ST_PROTECAO:
            if (temperatura_realc > 50.0 || temperatura_realc < -10.0) estado_atual = ST_ERRO;
            else estado_atual = ST_EMESPERA;
            break;

        case ST_ERRO:
        {
            HAL_GPIO_WritePin(GPIOA, LED_ERRO, 1);
            __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0);

            // Converte temperatura para inteiro
            int temp_erro_int = (int)(temperatura_realc * 100);
            printf("termometro funcionando");

            if (temperatura_realc > temperatura_idealc + gap_temperaturac){
                                HAL_GPIO_WritePin(GPIOA, LED_TEMP_ALTA, 1);
                                HAL_GPIO_WritePin(GPIOA, LED_COMPRESSOR, 1);
                                pwm_compressor = 100;
                                compressor_ligado = 1;
                                lcd_clear();
                                lcd_put_cur(0, 0);
                                lcd_send_string("codigo");
                                lcd_put_cur(1, 0);
                                lcd_send_string("acima da temperatura");
                                HAL_Delay(500);
                            }
            else if (temperatura_realc < temperatura_idealc - gap_temperaturac){
                                HAL_GPIO_WritePin(GPIOA, LED_TEMP_BAIXA, 1);
                                pwm_compressor = 0;
                                compressor_ligado = 0;
                                lcd_clear();
                                lcd_put_cur(0, 0);
                                lcd_send_string("codigo");
                                lcd_put_cur(1, 0);
                                lcd_send_string("abaixo da temperatura");
                                HAL_Delay(500);
                            }


            HAL_Delay(1000);
            estado_atual = ST_EMESPERA;
            break;
        }

        default:
            estado_atual = ST_EMESPERA;
            break;
    }

  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = ENABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 3;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_6;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_15CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_7;
  sConfig.Rank = 2;
  sConfig.SamplingTime = ADC_SAMPLETIME_15CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_8;
  sConfig.Rank = 3;
  sConfig.SamplingTime = ADC_SAMPLETIME_15CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 83;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3
                          |GPIO_PIN_4, GPIO_PIN_RESET);

  /*Configure GPIO pins : PA0 PA1 PA2 PA3
                           PA4 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3
                          |GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PA11 PA12 */
  GPIO_InitStruct.Pin = GPIO_PIN_11|GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_11)
    {
        // Acende todos os LEDs
        HAL_GPIO_WritePin(GPIOA,
                          GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 |
                          GPIO_PIN_3 | GPIO_PIN_4,
                          GPIO_PIN_SET);

        lcd_clear();
        HAL_Delay(500);

        // ➤ Travamento proposital: loop infinito
        while (1)
        {
            // travado
        }
    }

    if (GPIO_Pin == GPIO_PIN_12)
    {
        unidade_temperatura++;
        if (unidade_temperatura > 2)
            unidade_temperatura = 0;
    }
}



/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
