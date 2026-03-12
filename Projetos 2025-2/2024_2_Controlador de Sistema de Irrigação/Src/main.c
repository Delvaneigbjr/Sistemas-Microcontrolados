/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

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

TIM_HandleTypeDef htim2;

/* USER CODE BEGIN PV */
//Variaveis Timer
uint8_t LedsTimer[5] = {LedTimer1_Pin, LedTimer2_Pin, LedTimer3_Pin, LedTimer4_Pin, LedTimer5_Pin,};
uint8_t LedTimerAceso = 0;
uint8_t EstadoBotaoTimer = GPIO_PIN_SET;

//Tempo setado
uint8_t Tempo[5] = {10, 20, 30, 40, 50};
uint8_t TempoSetado;

//Indicador Liga Desliga
uint8_t EstadoSistema = 1;
uint8_t EstadoBotaoOnOff = GPIO_PIN_SET;

float Umidade;
float UmidadeAnterior = 0;//Umidade no Inicio do Ciclo
uint8_t VolumeDeAgua;
uint8_t VariacaoUmidade;

uint8_t ModoAtual = 1;
uint8_t EstadoAnteriorBotaoModo = GPIO_PIN_SET; 

// Variáveis para os modelos matemáticos
float Vazao = 2.5; //Vazao em L/s
float AreaIrrigada = 10.0; //Area Irrigada
float UmidadeDesejada = 50.0; //Umidade alvo

float k; //Constante de Ajuste
float TempoNecessario = 0; //Tempo de Irrigacao
float Capacidade = 200; //Capacidade Hidrica do solo
float AguaDesnecessaria;//Gasto de agua
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM2_Init(void);
/* USER CODE BEGIN PFP */
void ExecutarModo(void);

//Função para ler o valor do ADC
uint32_t Read_ADC_Value(void)
{
  HAL_ADC_Start(&hadc1); // Inicia a conversão
  HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY); // Aguarda a conversão
  return HAL_ADC_GetValue(&hadc1); // Retorna o valor lido
}

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */

  HAL_GPIO_WritePin(GPIOA, LedsTimer[LedTimerAceso], GPIO_PIN_SET);
  TempoSetado = Tempo[LedTimerAceso];

  HAL_GPIO_WritePin(KIT_LED_GPIO_Port, KIT_LED_Pin, GPIO_PIN_SET);

  HAL_GPIO_WritePin(Modo1_GPIO_Port, Modo1_Pin, GPIO_PIN_SET);

  HAL_TIM_Base_Start_IT(&htim2);

  HAL_ADC_Start(&hadc1);  // Inicia a conversão contínua do ADC

  HAL_TIM_Base_Start_IT(&htim2);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
 //Leitura do Potenciometro Umidade -------------------------------------------------------------------------------------------------------------------------------------------
     // Lê o valor do ADC
     uint32_t adc_value = Read_ADC_Value();
 
     // Converte o valor do ADC para Umidade (0% a 100%)
     Umidade = (float)((adc_value * 100) / 4095);
 //----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
     //modelos matemáticos
     VolumeDeAgua = Vazao * TempoSetado; //Volume gasto por ciclo
 
     VariacaoUmidade = (Umidade - UmidadeAnterior); //Variação de umidade
     
     k = TempoSetado / (VariacaoUmidade * 0.01);//aumentou tantos % a cada tantos minutos
 
     TempoNecessario = k * (UmidadeDesejada - Umidade);;//Tempo ate chegar a umidade desejada 
 
     AguaDesnecessaria = VolumeDeAgua - (((UmidadeDesejada - Umidade)/100)*Capacidade*AreaIrrigada); //Calculo do desperdicio
 
 
 //Configuracao do Timer-------------------------------------------------------------------------------------------------------------------------------------------------------
    // Lê o estado atual do botão
    uint8_t EstadoAnteriorBotaoTimer = HAL_GPIO_ReadPin(BotaoTimer_GPIO_Port, BotaoTimer_Pin);
  
    if (EstadoBotaoTimer == GPIO_PIN_SET && EstadoAnteriorBotaoTimer == GPIO_PIN_RESET && EstadoSistema == 1)
    {
     HAL_Delay(100);
      HAL_GPIO_WritePin(GPIOA, LedsTimer[LedTimerAceso], GPIO_PIN_RESET);
  
      LedTimerAceso = (LedTimerAceso + 1) % 5;
  
      HAL_GPIO_WritePin(GPIOA, LedsTimer[LedTimerAceso], GPIO_PIN_SET);
      HAL_Delay(50);
    }
  
    EstadoBotaoTimer = EstadoAnteriorBotaoTimer;
    TempoSetado = Tempo[LedTimerAceso];
 //----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 //Configuracao Liga-Desliga---------------------------------------------------------------------------------------------------------------------------------------------------
     // Configuração Liga-Desliga
     uint8_t EstadoAnteriorBotaoOnOff = HAL_GPIO_ReadPin(BotaoOnOff_GPIO_Port, BotaoOnOff_Pin);
     if (EstadoBotaoOnOff == GPIO_PIN_SET && EstadoAnteriorBotaoOnOff == GPIO_PIN_RESET) {
         HAL_Delay(100);
         EstadoSistema = !EstadoSistema;
     }
     EstadoBotaoOnOff = EstadoAnteriorBotaoOnOff;
 
     HAL_GPIO_WritePin(KIT_LED_GPIO_Port, KIT_LED_Pin, EstadoSistema == 0 ? GPIO_PIN_RESET : GPIO_PIN_SET);
 
     if (EstadoSistema == 0) {
         UmidadeAnterior = Umidade;
         ExecutarModo();
     }else{
       HAL_GPIO_WritePin(GPIOA, LedOnOff_Pin, GPIO_PIN_RESET);
     }
 //----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 //Configuracao Configuração Modos---------------------------------------------------------------------------------------------------------------------------------------------
 // Leitura do estado atual do botão Modo
 uint8_t EstadoAtualBotaoModo = HAL_GPIO_ReadPin(BotaoModo_GPIO_Port, BotaoModo_Pin);
 
   if (EstadoAnteriorBotaoModo == GPIO_PIN_SET && EstadoAtualBotaoModo == GPIO_PIN_RESET && EstadoSistema == 1) {
       HAL_Delay(100);
       ModoAtual = (ModoAtual % 3) + 1;
 
       HAL_GPIO_WritePin(Modo1_GPIO_Port, Modo1_Pin, (ModoAtual == 1) ? GPIO_PIN_SET : GPIO_PIN_RESET);
       HAL_GPIO_WritePin(Modo2_GPIO_Port, Modo2_Pin, (ModoAtual == 2) ? GPIO_PIN_SET : GPIO_PIN_RESET);
       HAL_GPIO_WritePin(Modo3_GPIO_Port, Modo3_Pin, (ModoAtual == 3) ? GPIO_PIN_SET : GPIO_PIN_RESET);
 }
   EstadoAnteriorBotaoModo = EstadoAtualBotaoModo;
 
 //----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 }
 }
 /* USER CODE END 3 */
 
  void ExecutarModo(void) {
   static uint8_t Modo1Executado = 0;
 
   static uint32_t TempoAnteriorModo2 = 0;
   static uint8_t LedOnOffStateModo2 = 0;
 
   switch (ModoAtual) {
       case 1: //Modo 1, Manual
           if (!Modo1Executado) {
               //Liga
               HAL_GPIO_WritePin(GPIOA, LedOnOff_Pin, GPIO_PIN_SET);
 
               HAL_Delay(TempoSetado * 1000);
               //Desliga
               HAL_GPIO_WritePin(GPIOA, LedOnOff_Pin, GPIO_PIN_RESET);
 
               EstadoSistema = 1; //Sistema desligado
               Modo1Executado = 1;
 }
           break;
 
       case 2: {//Modo 2, Automatizado
             uint32_t TempoAtual = HAL_GetTick();
             //Verifica se o tempo definido no timer ja passou
             if (TempoAtual - TempoAnteriorModo2 >= (TempoSetado * 1000)) {
                 TempoAnteriorModo2 = TempoAtual;
 
                 LedOnOffStateModo2 = !LedOnOffStateModo2;
                 HAL_GPIO_WritePin(GPIOA, LedOnOff_Pin, LedOnOffStateModo2 ? GPIO_PIN_SET : GPIO_PIN_RESET);
             }
 }
           break;
 
       case 3: { //Modo 3, Controlador de Umidade
            float umidadeMinima = 35; 
            float umidadeDesejada = 50;
 
             //Verifica a umidade atual
            if (Umidade < umidadeMinima) {
                HAL_GPIO_WritePin(GPIOA, LedOnOff_Pin, GPIO_PIN_SET);
            } else if (Umidade >= umidadeDesejada) {
                 HAL_GPIO_WritePin(GPIOA, LedOnOff_Pin, GPIO_PIN_RESET);
     }
 
     HAL_Delay(10);
 }
           break;
 
       default:
           break;
   }
 
   if (EstadoSistema == 1) {
       Modo1Executado = 0;
       LedOnOffStateModo2 = 0;
   }
 }

 void TIM2_IRQHandler(void)
 {
     // Verifica se a interrupção foi gerada pelo timer
     if (__HAL_TIM_GET_FLAG(&htim2, TIM_FLAG_UPDATE) != RESET)
     {
         __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);
 
         if (Umidade > 65.0 || Umidade < 25.0)
         {
          HAL_GPIO_WritePin(GPIOB, Buzzer_Pin, GPIO_PIN_SET);  //Liga o buzzer
          EstadoSistema = 1;
         }
         else
         {
             HAL_GPIO_WritePin(GPIOB, Buzzer_Pin, GPIO_PIN_RESET); //Desliga o buzzer (nível baixo)
         }
     }
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
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
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_8;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

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

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 83;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 999999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

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
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(KIT_LED_GPIO_Port, KIT_LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LedTimer1_Pin|LedTimer2_Pin|LedTimer3_Pin|LedTimer4_Pin
                          |LedTimer5_Pin|LedOnOff_Pin|Modo1_Pin|Modo2_Pin
                          |Modo3_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(Buzzer_GPIO_Port, Buzzer_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : KIT_LED_Pin */
  GPIO_InitStruct.Pin = KIT_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(KIT_LED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LedTimer1_Pin LedTimer2_Pin LedTimer3_Pin LedTimer4_Pin
                           LedTimer5_Pin LedOnOff_Pin Modo1_Pin Modo2_Pin
                           Modo3_Pin */
  GPIO_InitStruct.Pin = LedTimer1_Pin|LedTimer2_Pin|LedTimer3_Pin|LedTimer4_Pin
                          |LedTimer5_Pin|LedOnOff_Pin|Modo1_Pin|Modo2_Pin
                          |Modo3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : Buzzer_Pin */
  GPIO_InitStruct.Pin = Buzzer_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(Buzzer_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : BotaoModo_Pin BotaoTimer_Pin BotaoOnOff_Pin */
  GPIO_InitStruct.Pin = BotaoModo_Pin|BotaoTimer_Pin|BotaoOnOff_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
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

#ifdef  USE_FULL_ASSERT
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
