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
DMA_HandleTypeDef hdma_adc1;

I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;

/* USER CODE BEGIN PV */
I2C_LCD_Handler lcd1;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM4_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
//Variaveis Para Uso De Debug:
float DLux, DUmi, DTemp;
float DLum, DLWatts;
float DUabs, DUmpt, DUpo;
float DTf, DTk, DTst;
uint8_t DAl, DAu, DAt;

//Variaveis Necessárias No Main:
uint8_t AlertaStatus;
uint8_t AtualizaLeds;
uint8_t NovoDutyCycle;
uint16_t ConversorDLimite;

//Limites Padrão:
uint16_t LMinLux = 300;
uint16_t LMaxLux = 500;
uint16_t LMinUmi = 40;
uint16_t LMaxUmi = 80;
uint16_t LMinTemp = 300;
uint16_t LMaxTemp = 500;
uint8_t MudandoMaxMin = 0x00;

//Variaveis Alteradas por DMA || TIMERS:
volatile uint16_t SegundosPadrao;
volatile uint16_t SegundosAlerta;
volatile uint16_t RegistroADC[3];

//Variaveis Para Uso do LCD && Botões:
volatile uint8_t ControleLCD = Nada;
uint8_t TelaMenu = Geral;
const char *menus[7][4] =
{
		// [Menu 0] - Geral
	{//  "01234567890123456789
		"Lux: ",
		"Umidade: ",
		"Temperatura: ",
		"Tempo Alerta: "
	},
		// [Menu 1] - Dados Luminosidade
	{// "01234567890123456789
		"Dados Luminosidade",
		"Lux: ",
		"Lumens em 1.5m: ",
		"WattsPorM2: "
	},

		// [Menu 2] - Dados Umidade
	{//  "01234567890123456789
		"Umidade Rel: ",
		"Umidade Abs: ",
		"Umidade Max: ",
		"PontOrvalho: "
	},
		// [Menu 3] - Dados Temperatura
	{//  "01234567890123456789
		"Temp em C: ",
		"Temp em F: ",
		"Temp em K: ",
		"Sens Termica: "
	},
		// [Menu 4] - Status Alertas:
	{//  "01234567890123456789
		"Conforto Lum: ",
		"Status Gelo: ",
		"Temp Sensor: ",
		"Status Alerta: "
	},
		// [Menu 5] - Dados Tempo
	{// "01234567890123456789
		"Registro De Tempos",
		"Tempo Ligado: ",
		"Tempo Em Alerta: ",
		"ESC Zera TempoAlerta"
	},
		// [Menu 6] - Configurações
	{// "01234567890123456789
		"Alterando Alerta",
		"LLum: ",
		"LUmi: ",
		"LTemp:"
	}
};

void MudaTela(I2C_LCD_Handler *LCD, const char *str[4])
{
	HAL_GPIO_TogglePin(GPIOC, LED_Black_Pin);
	LCD_Clear_All(LCD);
	LCD_Set_Pos_Home(LCD);

	LCD_Write_String(LCD, str[0]);
	LCD_Set_Pos(LCD, 0, 1);
	LCD_Write_String(LCD, str[1]);
	LCD_Set_Pos(LCD, 0, 2);
	LCD_Write_String(LCD, str[2]);
	LCD_Set_Pos(LCD, 0, 3);
	LCD_Write_String(LCD, str[3]);
}

const char* VerificarStatusAlerta(uint8_t Status)
{
    if (Status > 0) return "On";

    return "Off";
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

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  /* USER CODE BEGIN 2 */
  //Inicia Medição Potenciometros E Armazena No Registro:
  HAL_ADC_Start_DMA(&hadc1, (uint32_t*)RegistroADC, 3);

  //Inicia Os Timers:
  HAL_TIM_Base_Start(&htim2); 		//Responsável Pelos PWM
  HAL_TIM_Base_Start_IT(&htim3); 	//Responsável Pela Contagem De Tempo Em Que O Dispositivo Está Ligado
  HAL_TIM_Base_Start_IT(&htim4);	//Responsável Pela Contagem De Tempo Em Que O Alerta Esteve Ligado

  //Inicia Os PWMs:
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);

  //Inicializa o LCD:
  LCD_Default_Init(&lcd1, &hi2c1, 20, 4);

  //Coloca A Tela De Medições Gerais Como Inicio:
  MudaTela(&lcd1, menus[TelaMenu]);

  //Apaga Os LEDs Do PWM (Ascenderão No Decorrer Do Programa)
 __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0);
 __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 0);
 __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, 0);
 __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_4, 0);

 //Inicializa Filtros Para Cada Medição
 FiltroExp Lum;
 FiltroExp Umi;
 FiltroExp Temp;
 FiltroInit(&Lum, 0.9f);
 FiltroInit(&Umi, 0.9f);
 FiltroInit(&Temp, 0.68f);

 //Inicialização Concluida Com Sucesso, Ascende o Pino Indicador De "Ligado"
 HAL_GPIO_WritePin(GPIOA, Ligado_Pin, 1);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1)
	{
		//Seção Responsável Por Alterar Tela No Menu Do LCD:
		if (ControleLCD != Nada)
		{
			switch (ControleLCD)
			{
				case Esquerda:
					if (TelaMenu <= Luminosidade){TelaMenu = Tempos; MudaTela(&lcd1, menus[TelaMenu]);}
					else if (TelaMenu != Configuracoes)  MudaTela(&lcd1, menus[--TelaMenu]);
					else if (TelaMenu == Configuracoes) MudandoMaxMin &= ~0X01;
					break;

				case Enter:
					if (TelaMenu != Configuracoes){TelaMenu = Configuracoes; MudaTela(&lcd1, menus[TelaMenu]);}
					else if (TelaMenu == Configuracoes) MudandoMaxMin ^= 0x02;
					break;

				case Direita:
					if (TelaMenu == Tempos){TelaMenu = Luminosidade; MudaTela(&lcd1, menus[TelaMenu]);}
					else if (TelaMenu != Configuracoes) MudaTela(&lcd1, menus[++TelaMenu]);
					else if (TelaMenu == Configuracoes) MudandoMaxMin |= 0X01;
					break;

				case Esc:
					if (TelaMenu != Geral){MudandoMaxMin = 0x00; TelaMenu = Geral; MudaTela(&lcd1, menus[TelaMenu]);}
					SegundosAlerta = 0;
					break;

				default: break;
			}
			ControleLCD = Nada;
			HAL_GPIO_TogglePin(GPIOC, LED_Black_Pin);
		}

		//Seção Responsável Pela Atualização Dos LEDs:
		AlertaStatus = 0x00;
		AtualizaLeds = 0;
		NovoDutyCycle = 0;
		ConversorDLimite = 0;

		while (AtualizaLeds < 3)
		{
			switch (AtualizaLeds)
			{
				case (Luminosidade-1):
					ConversorDLimite = (uint16_t)(RegistroADC[AtualizaLeds]>>2);// [0;~1024]

					if (ConversorDLimite <= (uint16_t)LMinLux || ConversorDLimite >= (uint16_t)LMaxLux)
					{
						NovoDutyCycle += (ConversorDLimite <= LMinLux) ?
							(LMinLux - ConversorDLimite)
								:
							(ConversorDLimite - LMaxLux);
						AlertaStatus = (AlertaStatus & 0x07) | 0x01 << AtualizaLeds;
					}

					FiltroAplicar(&Lum, ConversorDLimite);

					break;

				case (Umidade-1):
					ConversorDLimite = (uint16_t)(RegistroADC[AtualizaLeds]*101/4095); //"0" + 100 == 101

					if (ConversorDLimite <= (uint16_t)LMinUmi || ConversorDLimite >= (uint16_t)LMaxUmi)
					{
						NovoDutyCycle += (ConversorDLimite <= LMinUmi) ?
							(LMinUmi - ConversorDLimite)
								:
							(ConversorDLimite - LMaxUmi);
						AlertaStatus = (AlertaStatus & 0x07) | 0x01 << AtualizaLeds;
					}

					__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, ConversorDLimite);

					FiltroAplicar(&Umi, ConversorDLimite);
					break;

				case (Temperatura-1):
					ConversorDLimite = (uint16_t)(RegistroADC[AtualizaLeds]*801/4095); //80 valores * 10 casas decimais + 0

					if (ConversorDLimite <= (uint16_t)LMinTemp || ConversorDLimite >= (uint16_t)LMaxTemp)
					{
						NovoDutyCycle += (ConversorDLimite <= LMinTemp) ?
							(LMinTemp - ConversorDLimite)
								:
							(ConversorDLimite - LMaxTemp);
						AlertaStatus = (AlertaStatus & 0x07) | 0x01 << AtualizaLeds;
					}

					FiltroAplicar(&Temp, (ConversorDLimite - 200)/10); //Faz Intervalo Temperatura começar em -20 [-20;60]

					if (GetValorAntFiltro(&Temp) < 0.0f)
					{__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, (ConversorDLimite/801));__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, (801-(ConversorDLimite/801)));}
					else if (GetValorAntFiltro(&Temp) > 0.0f)
					{__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, (ConversorDLimite/801));__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, (801-(ConversorDLimite/801)));}
					else if (GetValorAntFiltro(&Temp) == 0.0f) {__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 0);__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0);}

					break;
				default: break;
			}

			AtualizaLeds++;
		}
		__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_4, NovoDutyCycle<<2);

		if (AlertaStatus > 0x00) __HAL_TIM_ENABLE_IT(&htim4, TIM_IT_UPDATE);
		else __HAL_TIM_DISABLE_IT(&htim4, TIM_IT_UPDATE);

		//Atualiza Valores De Variaveis de Debug:
		DLux = GetValorFiltro(&Lum);
		DUmi = GetValorFiltro(&Umi);
		DTemp = GetValorFiltro(&Temp);

		DLum = LuxToLumens(DLux, 1.5);
		DLWatts = LuxToWattPorM2(DLux, Fluororescente);

		DUabs = UmidadeAbsoluta(DUmi, DTemp);
		DUmpt = UmidadeMaxPorTemp(DTemp);
		DUpo = PontOrvalho(DTemp, DUmi);

		DTf = CelsiusToFahrenheit(DTemp);
		DTk = CelsiusToKelvin(DTemp);
		DTst = SensacaoTermica(DTemp, DUmi);

		DAl = (AlertaStatus&(0x01<<0));
		DAu = (AlertaStatus&(0x01<<1));
		DAt = (AlertaStatus&(0x01<<2));

		//Seção Responsável Pela Exibição Dos Valores Medidos De Acordo Com O Tipo De Tela Selecionada:
		switch (TelaMenu)
		{
			case Geral:
				LCD_Clear_Partial_Line(&lcd1, 0, (uint8_t) strlen(menus[TelaMenu][0]));
				LCD_Write_Number(&lcd1, DLux, 2);

				LCD_Clear_Partial_Line(&lcd1, 1, (uint8_t) strlen(menus[TelaMenu][1]));
				LCD_Write_Number(&lcd1, DUmi, 2);

				LCD_Clear_Partial_Line(&lcd1, 2, (uint8_t) strlen(menus[TelaMenu][2]));
				LCD_Write_Number(&lcd1, DTemp, 2);

				LCD_Clear_Partial_Line(&lcd1, 3, (uint8_t) strlen(menus[TelaMenu][3]));
				LCD_Write_Number(&lcd1, SegundosAlerta, 0);
				break;

			case Luminosidade:
				LCD_Clear_Partial_Line(&lcd1, 1, (uint8_t) strlen(menus[TelaMenu][1]));
				LCD_Write_Number(&lcd1, DLux, 2);

				LCD_Clear_Partial_Line(&lcd1, 2, (uint8_t) strlen(menus[TelaMenu][2]));
				LCD_Write_Number(&lcd1, DLum, 2);

				LCD_Clear_Partial_Line(&lcd1, 3, (uint8_t) strlen(menus[TelaMenu][3]));
				LCD_Write_Number(&lcd1, DLWatts, 2);
				break;

			case Umidade:
				LCD_Clear_Partial_Line(&lcd1, 0, (uint8_t) strlen(menus[TelaMenu][0]));
				LCD_Write_Number(&lcd1, DUmi, 2);

				LCD_Clear_Partial_Line(&lcd1, 1, (uint8_t) strlen(menus[TelaMenu][1]));
				LCD_Write_Number(&lcd1, DUabs, 2);

				LCD_Clear_Partial_Line(&lcd1, 2, (uint8_t) strlen(menus[TelaMenu][2]));
				LCD_Write_Number(&lcd1, DUmpt, 2);

				LCD_Clear_Partial_Line(&lcd1, 3, (uint8_t) strlen(menus[TelaMenu][3]));
				LCD_Write_Number(&lcd1, DUpo, 2);
				break;

			case Temperatura:
				LCD_Clear_Partial_Line(&lcd1, 0, (uint8_t) strlen(menus[TelaMenu][0]));
				LCD_Write_Number(&lcd1, DTemp, 2);

				LCD_Clear_Partial_Line(&lcd1, 1, (uint8_t) strlen(menus[TelaMenu][1]));
				LCD_Write_Number(&lcd1, DTf, 2);

				LCD_Clear_Partial_Line(&lcd1, 2, (uint8_t) strlen(menus[TelaMenu][2]));
				LCD_Write_Number(&lcd1, DTk, 2);

				LCD_Clear_Partial_Line(&lcd1, 3, (uint8_t) strlen(menus[TelaMenu][3]));
				LCD_Write_Number(&lcd1, DTst, 2);
				break;

			case Alertas:
				LCD_Clear_Partial_Line(&lcd1, 0, (uint8_t) strlen(menus[TelaMenu][0]));
				LCD_Write_String(&lcd1, AvaliarConfortoLuminoso(DLux, AmbienteTrabalho));

				LCD_Clear_Partial_Line(&lcd1, 1, (uint8_t) strlen(menus[TelaMenu][1]));
				LCD_Write_String(&lcd1, VerificarAlertaGelo(DTemp, DUmi));

				LCD_Clear_Partial_Line(&lcd1, 2, (uint8_t) strlen(menus[TelaMenu][2]));
				LCD_Write_String(&lcd1, InteracaoLuzTemperatura(DTemp));

				LCD_Clear_Partial_Line(&lcd1, 3, (uint8_t) strlen(menus[TelaMenu][3]));
				LCD_Write_String(&lcd1, VerificarStatusAlerta(AlertaStatus));
				break;

			case Tempos:
				LCD_Set_Pos(&lcd1, (uint8_t) strlen(menus[TelaMenu][1]), 1);
				LCD_Write_Number(&lcd1, SegundosPadrao, 0);

				LCD_Clear_Partial_Line(&lcd1, 2, (uint8_t) strlen(menus[TelaMenu][2]));
				LCD_Write_Number(&lcd1, SegundosAlerta, 0);
				break;

			case Configuracoes:
				if((MudandoMaxMin & 0x02) == 0x02)
				{
					if (MudandoMaxMin == 0x03)
					{
						LMaxLux = DLux;
						LMaxUmi = DUmi;
						LMaxTemp = (DTemp*10)+200;

						LCD_Clear_Partial_Line(&lcd1, 0, (uint8_t) strlen(menus[TelaMenu][0]));
						LCD_Write_String(&lcd1, "Max");

						LCD_Clear_Partial_Line(&lcd1, 1, (uint8_t) strlen(menus[TelaMenu][1]));
						LCD_Write_Number(&lcd1, LMaxLux, 2);

						LCD_Clear_Partial_Line(&lcd1, 2, (uint8_t) strlen(menus[TelaMenu][2]));
						LCD_Write_Number(&lcd1, LMaxUmi, 2);

						LCD_Clear_Partial_Line(&lcd1, 3, (uint8_t) strlen(menus[TelaMenu][3]));
						LCD_Write_Number(&lcd1, (LMaxTemp-200)/10, 2);
					}
					else if (MudandoMaxMin == 0x02)
					{
						LMinLux = DLux;
						LMinUmi = DUmi;
						LMinTemp = (DTemp*10)+200;

						LCD_Clear_Partial_Line(&lcd1, 0, (uint8_t) strlen(menus[TelaMenu][0]));
						LCD_Write_String(&lcd1, "Min");

						LCD_Clear_Partial_Line(&lcd1, 1, (uint8_t) strlen(menus[TelaMenu][1]));
						LCD_Write_Number(&lcd1, LMinLux, 2);

						LCD_Clear_Partial_Line(&lcd1, 2, (uint8_t) strlen(menus[TelaMenu][2]));
						LCD_Write_Number(&lcd1, LMinUmi, 2);

						LCD_Clear_Partial_Line(&lcd1, 3, (uint8_t) strlen(menus[TelaMenu][3]));
						LCD_Write_Number(&lcd1, (LMinTemp-200)/10, 2);
					}
				}
				else
				{
					LCD_Clear_Partial_Line(&lcd1, 1, (uint8_t) strlen(menus[TelaMenu][1]));
					LCD_Write_String(&lcd1, "Pressione");

					LCD_Clear_Partial_Line(&lcd1, 2, (uint8_t) strlen(menus[TelaMenu][2]));
					LCD_Write_String(&lcd1, "Enter");

					LCD_Clear_Partial_Line(&lcd1, 3, (uint8_t) strlen(menus[TelaMenu][3]));
					LCD_Write_String(&lcd1, "Para Mudar");
				}

				break;

			default: break;
		}
	}
    /* USER CODE END WHILE */
}
    /* USER CODE BEGIN 3 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) //Interrupção Por Botões (Usada Para Mudar Tela Do LCD):
{
	switch (GPIO_Pin)
	{
		case Esquerda_Pin: ControleLCD = Esquerda; break;

		case Enter_Pin: ControleLCD = Enter; break;

		case Direita_Pin: ControleLCD = Direita; break;

		case Esc_Pin: ControleLCD = Esc; break;

		default: break;
	}
}
void HAL_TIM_PeriodElapsedCallback (TIM_HandleTypeDef * Timer) //Interrupção Por Timers A Cada 1s, Para Contar A Passagem Do Tempo:
{
	if (Timer->Instance == TIM3) SegundosPadrao++;
	if (Timer->Instance == TIM4) SegundosAlerta++;
}
  /* USER CODE END 3 */

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
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 80;
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
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV8;
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
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_5;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_6;
  sConfig.Rank = 2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_7;
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
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 100;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 101;
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
  sConfigOC.Pulse = 100;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

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

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 1000-1;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 10000-1;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

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
  htim4.Init.Prescaler = 1000-1;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 10000-1;
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
  HAL_GPIO_WritePin(LED_Black_GPIO_Port, LED_Black_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(Ligado_GPIO_Port, Ligado_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : LED_Black_Pin */
  GPIO_InitStruct.Pin = LED_Black_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_Black_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : Ligado_Pin */
  GPIO_InitStruct.Pin = Ligado_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(Ligado_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : Esquerda_Pin Enter_Pin Direita_Pin Esc_Pin */
  GPIO_InitStruct.Pin = Esquerda_Pin|Enter_Pin|Direita_Pin|Esc_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PB12 PB13 PB14 PB15
                           PB3 PB4 PB5 PB8
                           PB9 */
  GPIO_InitStruct.Pin = GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15
                          |GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_8
                          |GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PA8 PA9 PA10 PA11
                           PA12 PA15 */
  GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11
                          |GPIO_PIN_12|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  HAL_NVIC_SetPriority(EXTI1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);

  HAL_NVIC_SetPriority(EXTI2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI2_IRQn);

  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

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
