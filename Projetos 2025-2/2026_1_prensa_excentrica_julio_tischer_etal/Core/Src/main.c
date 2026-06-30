/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "../../ECUAL/I2C_LCD/I2C_LCD.h"
#include <stdio.h>
#include <math.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define MyI2C_LCD I2C_LCD_1
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_I2C1_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM4_Init(void);
/* USER CODE BEGIN PFP */
void ativar_Furo();
void alimentar_Barra();
void HMS_Converter();
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
int menu=0;
int peças_Produzidas = 0;
int peças_Seguidas=0;
int falhas;
int timer_Produção;
int sensor_Temp;
int sensor_Oleo;
int poten_Veloc;
int process_Running=0;
int process_Delays;
int flag=0;
int alimentações_Seguidas=0;
int parar_FM=0;
int tempo_Segundos = 0;
int a =1 ;

float peças_Minuto;
float altura_Oleo;

float temperatura_C;
float temperatura_K;
float nivel_Oleo;
float variação_Oleo;
float variação_Temp;

char string_LCD[17];


uint32_t prev_Tick = 0;
uint16_t prev_Oleo = 0;
uint16_t prev_Temp = 0;
uint16_t prev_Veloc = 0;
uint32_t agora;
uint32_t tempo_Decorrido = 0;

uint32_t delta_ms;
uint32_t pwm_spray;
volatile uint32_t GP_Timer;
uint32_t tempo_Timer;

//Isso é o vetor que o DMA escreve, são as leituras do ADC
uint16_t ADC_Reads[3];

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
  MX_DMA_Init();
  MX_I2C1_Init();
  MX_ADC1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  /* USER CODE BEGIN 2 */

	// Inicialização do LCD
	HAL_Delay(500);
	I2C_LCD_Init(MyI2C_LCD);
	HAL_Delay(100);
	I2C_LCD_Clear(MyI2C_LCD);
	HAL_Delay(100);

	//Inicialização do DMA dos ADCs
	HAL_ADC_Start_DMA(&hadc1, (uint32_t*)ADC_Reads,3);

	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1)
	{

		//HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12, a);

		//Esse trecho controla os botões cima e baixo
		//Os PB2 e PB3 serão o cima e baixo
		if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_2)==1 && process_Running==0)
		{
			I2C_LCD_Clear(MyI2C_LCD);

			process_Running = 0;

			if (menu>6||menu<-1){menu=0;}
			else {menu++;}
			HAL_Delay(200);
		}
		if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_3)==1 && process_Running==0)
		{
			I2C_LCD_Clear(MyI2C_LCD);

			process_Running = 0;

			if (menu<1||menu>7){menu=7;}
			else {menu--;}
			HAL_Delay(200);
		}

		//O botão AZUL é o OK, ele muda o process_Running para 1. Isso ativa qualquer ação baseada no menu
		if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1)==1 && process_Running==0)
		{
			process_Running = 1;
			parar_FM = 0;				//Isso serve para o furo multiplo
			GP_Timer = HAL_GetTick();
			if (menu==1)
			{
				tempo_Segundos = 0; 			//Reseta tempo
				HAL_TIM_Base_Start_IT(&htim4);  //Inicia o timer do furo multiplo
				peças_Seguidas=0;				//Reseta peças seguidas
			}
			HAL_Delay(200);
		}

		if (process_Running == 1)
		{
			//Furo unico
			if (menu == 0)
			{
				ativar_Furo();
			}

			//Furo multiplo
			else if (menu == 1)
			{
				ativar_Furo();
			}


			//Soltar Pistões
			else if (menu==2)
			{
				HAL_GPIO_WritePin(GPIOA,  GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_8|GPIO_PIN_11, 0);
				process_Running = 0;
			}

			else if (menu ==7){}//Isso é pro else nn ferrar tudo

			else
			{
				process_Running = 0;
			}
		}

		//Leituras ADCS
		//PA0: Velocidade, PA1: Temperatura oleo, PA2: Nivel Oleo
		poten_Veloc = ADC_Reads[0];
		sensor_Temp = ADC_Reads[1];
		sensor_Oleo = ADC_Reads[2];

		//Converções do ADC
		//Temperatura varia de 273 a 473 K
		temperatura_K = (float)(sensor_Temp/4095.0)*200.0 + 273;
		temperatura_C = temperatura_K-273.0;

		//Caso temperatura alta
		if (temperatura_C>100 && menu!=21)
		{
			I2C_LCD_Clear(MyI2C_LCD);
			menu=21;
			HAL_TIM_Base_Stop_IT(&htim4);
		}

		//Nivel do Oleo
		//O sensor medira um container 0,2m * 0,2m, com a altura sendo de 0 a 0,2m
		//Para o sensor, ele mede de 0 a 1 metros, e estara 5 cm de distancia do limite maximo
		//Logo 20cm -> h=0 e 5cm -> h=15cm
		//Tambem multiplicampos por 10³ para converter de m³ em L
		altura_Oleo = (float)((0.20)-(sensor_Oleo/4095.0));
		nivel_Oleo = (float)(altura_Oleo*0.2*0.2*pow(10,3));

		//Caso oleo esteja baixo
		if (nivel_Oleo<1&& menu!=22)
		{
			I2C_LCD_Clear(MyI2C_LCD);
			menu=22;
			HAL_TIM_Base_Stop_IT(&htim4);
		}

		//Velocidade
		//O potenciometro vai determinar o tamanh do delay de operações, de 200ms até 1000ms
		process_Delays = (poten_Veloc/4095.0)*800.0 + 200;

		//Spary
		// Escala o potenciometro para o range do PWM (0–65535)
		pwm_spray = 65535 - ((uint32_t)poten_Veloc * 65535) / 4095;
		if (pwm_spray > 65535) pwm_spray = 65535;          // limita o maximo
		if (pwm_spray < 65535/5) pwm_spray = 65535/5;	//Limita um minimo

		__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pwm_spray);



		// Variação dos potenciômetros (calculada a cada iteração do loop)
		uint32_t agora   = HAL_GetTick();           // tempo atual em ms
		uint32_t delta_ms = agora - prev_Tick;

		if (delta_ms > 0)
		{
			// Converte delta ADC para unidades físicas por minuto
			float delta_oleo_L = ((float)((int)sensor_Oleo - (int)prev_Oleo) / 4095.0f) * 10.0f;
			float delta_temp_C = ((float)((int)sensor_Temp - (int)prev_Temp) / 4095.0f) * 200.0f;

			float minutos = delta_ms / 60000.0f;

			if(process_Running==1 && menu==7)
			{
				variação_Oleo = delta_oleo_L / minutos;
				variação_Temp = delta_temp_C / minutos;
				process_Running = 0;
			}





		}

		prev_Oleo = sensor_Oleo;
		prev_Temp = sensor_Temp;
		prev_Tick = agora;
		prev_Veloc = poten_Veloc;


		//Se der muito erro, parar processos e avisar
		if (alimentações_Seguidas>5)
		{
			menu=-2;
			process_Running=0;
			parar_FM = 1;
			alimentações_Seguidas=0;
			HAL_TIM_Base_Stop_IT(&htim4);
			I2C_LCD_Clear(MyI2C_LCD);
		}


		//Caso tenha alguem erro, ligar led de erro
		if (menu>7 || menu <0)
		{
			HAL_GPIO_WritePin(GPIOA,  GPIO_PIN_12, 1);
		}
		else {HAL_GPIO_WritePin(GPIOA,  GPIO_PIN_12, 0);}

		//MenuHandler -- Seriamente pensando em usar uma função para lidar com tantos menus
		switch(menu)
		{
		case 0:
			I2C_LCD_SetCursor(MyI2C_LCD, 0,0);
			I2C_LCD_WriteString(MyI2C_LCD, "Modo Furo Unico:");
			break;

		case 1:

			//Se nn tiver rodando ele mostra a pergunta
			if (process_Running==0)
			{
				I2C_LCD_SetCursor(MyI2C_LCD, 0,0);
				I2C_LCD_WriteString(MyI2C_LCD, "Modo Furos Multiplos");

			}

			else
			{
				sprintf(string_LCD,"Pecas: %09d",peças_Produzidas);
				I2C_LCD_WriteString(MyI2C_LCD, string_LCD);
				I2C_LCD_SetCursor(MyI2C_LCD, 0,1);


				sprintf(string_LCD,"%02d:%02d:%02d",(tempo_Segundos/3600),(tempo_Segundos%3600)/60,tempo_Segundos%60);
				I2C_LCD_WriteString(MyI2C_LCD, string_LCD);
				I2C_LCD_SetCursor(MyI2C_LCD, 0,0);
			}


			break;


		case 2:
			sprintf(string_LCD,"OK para");
			I2C_LCD_WriteString(MyI2C_LCD, string_LCD);
			I2C_LCD_SetCursor(MyI2C_LCD, 0,1);

			sprintf(string_LCD,"Liberar psts");
			I2C_LCD_WriteString(MyI2C_LCD, string_LCD);
			I2C_LCD_SetCursor(MyI2C_LCD, 0,0);

			break;

			//Talvez inutil
		case 3:

			sprintf(string_LCD,"Pecas: %d",peças_Produzidas);
			I2C_LCD_WriteString(MyI2C_LCD, string_LCD);
			I2C_LCD_SetCursor(MyI2C_LCD, 0,1);

			sprintf(string_LCD,"%02d:%02d:%02d",(tempo_Segundos/3600),(tempo_Segundos%3600)/60,tempo_Segundos%60);
			I2C_LCD_WriteString(MyI2C_LCD, string_LCD);
			I2C_LCD_SetCursor(MyI2C_LCD, 0,0);

			break;

		case 4:

			if (tempo_Segundos!=0)
			{
				peças_Minuto = (float)(1.0*peças_Produzidas/tempo_Segundos)*60.0;
			}


			sprintf(string_LCD,"Pcs Min: %05.2f",peças_Minuto);
			I2C_LCD_WriteString(MyI2C_LCD, string_LCD);
			I2C_LCD_SetCursor(MyI2C_LCD, 0,1);

			sprintf(string_LCD,"Falhas: %d",falhas);
			I2C_LCD_WriteString(MyI2C_LCD, string_LCD);
			I2C_LCD_SetCursor(MyI2C_LCD, 0,0);

			break;

		case 5:
			sprintf(string_LCD,"Temperaturas:");
			I2C_LCD_WriteString(MyI2C_LCD, string_LCD);
			I2C_LCD_SetCursor(MyI2C_LCD, 0,1);

			sprintf(string_LCD,"%04.1f C %04.1f K",temperatura_C,temperatura_K);
			I2C_LCD_WriteString(MyI2C_LCD, string_LCD);
			I2C_LCD_SetCursor(MyI2C_LCD, 0,0);

			break;

		case 6:
			sprintf(string_LCD,"Nivel do oleo:");
			I2C_LCD_WriteString(MyI2C_LCD, string_LCD);
			I2C_LCD_SetCursor(MyI2C_LCD, 0,1);

			sprintf(string_LCD,"%05.2f L",nivel_Oleo);
			I2C_LCD_WriteString(MyI2C_LCD, string_LCD);
			I2C_LCD_SetCursor(MyI2C_LCD, 0,0);

			break;

		case 7:
			sprintf(string_LCD,"vOleo: %04.1f L/m",variação_Oleo);
			I2C_LCD_WriteString(MyI2C_LCD, string_LCD);
			I2C_LCD_SetCursor(MyI2C_LCD, 0,1);

			sprintf(string_LCD,"vTmp: %04.1f C/m",variação_Temp);
			I2C_LCD_WriteString(MyI2C_LCD, string_LCD);
			I2C_LCD_SetCursor(MyI2C_LCD, 0,0);



			break;


			//Casos especiais:
			//tempo de produção (Furo multiplo)
		case 20:
			sprintf(string_LCD,"Minutos:       ");
			I2C_LCD_WriteString(MyI2C_LCD, string_LCD);
			I2C_LCD_SetCursor(MyI2C_LCD, 0,1);

			sprintf(string_LCD,"%03d",timer_Produção);
			I2C_LCD_WriteString(MyI2C_LCD, string_LCD);
			I2C_LCD_SetCursor(MyI2C_LCD, 0,0);

			break;

			//Proteção temp alta
		case 21:
			sprintf(string_LCD,"ALERTA: TEMP");
			I2C_LCD_WriteString(MyI2C_LCD, string_LCD);
			I2C_LCD_SetCursor(MyI2C_LCD, 0,1);

			sprintf(string_LCD,"ALTA");
			I2C_LCD_WriteString(MyI2C_LCD, string_LCD);
			I2C_LCD_SetCursor(MyI2C_LCD, 0,0);

			break;

			//Oleo baixo
		case 22:
			sprintf(string_LCD,"ALERTA: NIVEL");
			I2C_LCD_WriteString(MyI2C_LCD, string_LCD);
			I2C_LCD_SetCursor(MyI2C_LCD, 0,1);

			sprintf(string_LCD,"OLEO BAIXO");
			I2C_LCD_WriteString(MyI2C_LCD, string_LCD);
			I2C_LCD_SetCursor(MyI2C_LCD, 0,0);

			break;

			//Falhas contnuas
		case -2:
			sprintf(string_LCD,"ALERTA: MUITAS");
			I2C_LCD_WriteString(MyI2C_LCD, string_LCD);
			I2C_LCD_SetCursor(MyI2C_LCD, 0,1);

			sprintf(string_LCD,"FALHAS");
			I2C_LCD_WriteString(MyI2C_LCD, string_LCD);
			I2C_LCD_SetCursor(MyI2C_LCD, 0,0);

			break;

			//PARADA EMERGENCIAL
		case -1:
			sprintf(string_LCD,"PARADA DE       ");
			I2C_LCD_WriteString(MyI2C_LCD, string_LCD);
			I2C_LCD_SetCursor(MyI2C_LCD, 0,1);

			sprintf(string_LCD,"EMERGENCIA!     ");
			I2C_LCD_WriteString(MyI2C_LCD, string_LCD);
			I2C_LCD_SetCursor(MyI2C_LCD, 0,0);

			break;
		}





    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
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
  hadc1.Init.ScanConvMode = ENABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 3;
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = 2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_2;
  sConfig.Rank = 3;
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

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 4294967295;
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
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 65535;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */
  HAL_TIM_MspPostInit(&htim3);

}

/**
  * @brief TIM4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM4_Init(void)
{

  /* USER CODE BEGIN TIM4_Init 0 */

  /* USER CODE END TIM4_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM4_Init 1 */

  /* USER CODE END TIM4_Init 1 */
  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 9599;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 9999;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim4) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim4, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM4_Init 2 */

  /* USER CODE END TIM4_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA2_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);

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
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11
                          |GPIO_PIN_12, GPIO_PIN_RESET);

  /*Configure GPIO pins : PB0 PB5 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PB1 PB3 PB4 */
  GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_3|GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PB2 */
  GPIO_InitStruct.Pin = GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PA8 PA9 PA10 PA11
                           PA12 */
  GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11
                          |GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

//Interru´çoes
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    // Verifica se a interrupção veio do TIM4
    if (htim->Instance == TIM4)
    {
        tempo_Segundos++; //"porque não usar o hal_getTick?" Simples, menos variaveis, e menos confuso
        peças_Seguidas++;
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	// Verifica se a interrupção veio do pino 5
	if (GPIO_Pin == GPIO_PIN_0)
	{
		// Ativa flag para parar o furo multiplo e para o timer 4; Isto é feito pra utilizar com o furo multiplo
		parar_FM = 1;
		 HAL_TIM_Base_Stop_IT(&htim4);
	}

	else if (GPIO_Pin == GPIO_PIN_5)
	{
		//Mais divertido do que o anterior, esse mata qualquer processo forçando process_Running como 0
		//Infelizmente, não divertido é colocar a maquina em operação denovo
		//Como o codigo roda sem interrupções, mudar essa flag IMEDIATAMENTE para qualquer processo

		HAL_TIM_Base_Stop_IT(&htim4);		//Isso é para a parada de emergencia congelar o tiemr

		menu = -1;
		process_Running = 0;
		parar_FM = 1;
	}
}


void ativar_Furo()
{
	//PA8 - Pistão avanco
	//PA9 - Pistao trava
	//PA10 - Furar
	//PA11 - Ejetar Peça

	//Devemos primeiro travar o pistão trava
	if (flag!=1)
	{
		HAL_GPIO_WritePin(GPIOA,  GPIO_PIN_9, 1);
	}


	//Furar após dalay
	if ((HAL_GetTick()-GP_Timer > process_Delays) && flag!=1)
	{
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, 1);
	}

	//Levantar Furar após delay
	if (HAL_GetTick()-GP_Timer > 2*process_Delays && flag!=1)
	{
		HAL_GPIO_WritePin(GPIOA,  GPIO_PIN_10, 0);
	}

	//Ejetar peça
	if (HAL_GetTick()-GP_Timer > (2*process_Delays + 100) && flag!=1)
		{
			HAL_GPIO_WritePin(GPIOA,  GPIO_PIN_11, 1);
		}



	//Soltar Travas e retrair ejetor
	if (HAL_GetTick()-GP_Timer > 3*process_Delays && flag!=1)
	{
		HAL_GPIO_WritePin(GPIOA,  GPIO_PIN_9, 0);
		HAL_GPIO_WritePin(GPIOA,  GPIO_PIN_11, 0);
	}

	//Liberar Processos
	if (HAL_GetTick()-GP_Timer > 4*process_Delays || flag==1)
	{
		//Se for furo unico, encerra processo e adiciona contador
		if (menu==0){process_Running=0; peças_Produzidas++;}

		//Se for furo multiplo, alimentar maquina
		else if (menu==1 )
		{

			//Se não tiver barra (PB4 apertado), alimentar
			if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4)==1 || flag==1)
			{
				if (flag ==0)
				{
					GP_Timer = HAL_GetTick();
				}
				flag=1;
				alimentar_Barra();
			}



		}
	}

}

void alimentar_Barra()
{

	//PA8 - Pistão avanco
	//PA9 - Pistao trava
	//PA10 - Furar

	//Travar pistão
	//Avançar Barra
	//Soltar pistão
	//Retornar Avanço

	//Travar
	HAL_GPIO_WritePin(GPIOA,  GPIO_PIN_9, 1);

	//Avançar
	if (HAL_GetTick()-GP_Timer > process_Delays)
	{
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, 1);
	}

	//Soltar
	if (HAL_GetTick()-GP_Timer > 2*process_Delays)
	{
		HAL_GPIO_WritePin(GPIOA,  GPIO_PIN_9, 0);
	}

	//Retornar
	if (HAL_GetTick()-GP_Timer > 3*process_Delays)
	{
		HAL_GPIO_WritePin(GPIOA,  GPIO_PIN_8, 0);
	}

	//Liberar Processos
	if (HAL_GetTick()-GP_Timer > 4*process_Delays)
	{
		//Se ele chegar aqui, e PB4 não estiver apertado (com barra), reseta o alimentações seguidas e soma uma peça
		if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4)==0)
		{
			flag = 0;		//So pode liberar pra furar se der certo
			alimentações_Seguidas=0;
			peças_Produzidas++;
		}

		else
		{
			alimentações_Seguidas++;	//Isso conta quantas alimentações ele fez, caso uma seja bem sucedida, isso volta pro 0

			falhas++; 					//Isso conta as falhas, não reseta porque falhas são imperdoaveis
		}
		GP_Timer = HAL_GetTick();

	}

	if (parar_FM == 1)
	{
		process_Running=0;		//Após finalizar um ciclo, encerra processo
		flag = 0;
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
