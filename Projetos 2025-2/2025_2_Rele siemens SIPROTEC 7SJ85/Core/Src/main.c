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
#include "liquidcrystal_i2c.h"
#include <stdio.h>
#include <math.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define RESISTENCIA_CARGA_OHMS  270

// Limites de Proteção de Tensão (ANSI 27/59)
#define LIMITE_SUBTENSAO    380 // kV
#define LIMITE_SOBRETENSAO  630 // kV

#define ESTADO_DISJUNTOR_FECHADO 1
#define ESTADO_DISJUNTOR_ABERTO  0

// Máquina de Estados (Telas)
#define TELA_MENU        0
#define TELA_ELETRICA    1
#define TELA_CUBICULO    2
#define TELA_TEMP        3
#define TELA_GAS         4
#define TELA_POTENCIA    5
#define TELA_ALERTA      6
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;

/* USER CODE BEGIN PV */
// --- Variáveis de Leitura Crua (ADC) ---
uint32_t raw_gas = 0;
uint32_t raw_umidade = 0;
uint32_t raw_temp_amb = 0;
uint32_t raw_tensao = 0;

// --- Variáveis Convertidas (Grandezas Físicas) ---
uint32_t nivel_gas = 0;       // 0-100%
uint32_t umidade_cub = 0;     // 0-100%
uint32_t tensao_fase = 0;     // kV
uint32_t tensao_linha = 0;    // kV
uint32_t corrente_linha = 0;  // A
uint32_t temperatura_ambiente = 25; //ºC
uint32_t fator_potencia = 92;
uint32_t temperatura_trans = 0; //ºC

// --- Variáveis de Potência ---
uint32_t potencia_aparente = 0; // kVA
uint32_t potencia_ativa = 0;    // kW
uint32_t potencia_reativa = 0;  // kVAr

// --- Variáveis de Controle e Estado ---
uint32_t temperatura_alvo = 0;
uint32_t intensidade_aquecedor = 0;
uint32_t tempo_anterior_led = 0;

uint8_t estado_reserva = 0; // 0 = Normal, 1 = Reserva (Falha Trafo)
uint8_t timer_rodando = 0;
uint8_t pode_rearmar = 1;
uint8_t status_disjuntor = ESTADO_DISJUNTOR_FECHADO;
uint8_t status_tensao = 0; // 0=OK, 1=Sub, 2=Sobre

// --- Variáveis de Interface (LCD/Botões) ---
char lcd_buffer[16];
char buffer_temp[16]; // Buffer auxiliar para LCD
uint32_t timer_lcd = 0;
uint8_t flag_atualizar_lcd = 0;

int8_t menu_index = 0;
uint8_t tela_atual = TELA_MENU;
uint8_t flag_atualizar_tela = 1;

// Textos do Menu Principal
char textos_menu[5][17] = {
    "1. Eletrica     ",
    "2. Cubiculo     ",
    "3. Temperatura  ",
    "4. Gas          ",
    "5. Potencias     "
};

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM4_Init(void);
/* USER CODE BEGIN PFP */
uint32_t Ler_ADC_Canal(uint32_t canal);
uint32_t Calcular_Corrente_Estrela(uint32_t tensao_kV);
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
  MX_I2C1_Init();
  MX_TIM1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  /* USER CODE BEGIN 2 */
  // Inicialização de Pinos de Estado
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_SET);   // A11 Ligado (Normal)
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET); // A10 Desligado (Reserva)
    estado_reserva = 0;

    // Inicialização do Disjuntor (PA9)
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET); // LED Aceso = Fechado
    status_disjuntor = ESTADO_DISJUNTOR_FECHADO;

    // Inicialização de Timers
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_Base_Start_IT(&htim3);
    MX_TIM4_Init();
    HAL_TIM_Base_Start_IT(&htim4);

    // Inicialização do LCD
    HD44780_Init(2);
    HD44780_Clear();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  // BLOCO 1: LEITURA DE SENSORES (ADC)
	        // Leitura de Gás
	        raw_gas = Ler_ADC_Canal(ADC_CHANNEL_0);
	        if (raw_gas < 30) raw_gas = 30;

	        // Leitura de Umidade
	        raw_umidade = Ler_ADC_Canal(ADC_CHANNEL_1);
	        if (raw_umidade < 30) raw_umidade = 30;

	        // Leitura de Fator de Potência (Simulado)
	        raw_temp_amb = Ler_ADC_Canal(ADC_CHANNEL_2);
	        if (raw_temp_amb < 30) raw_temp_amb = 30;

	        // Leitura de Tensão (Simulação de TP)
	        raw_tensao = Ler_ADC_Canal(ADC_CHANNEL_3);
	        if (raw_tensao < 30) raw_tensao = 30;

	        HAL_Delay(10); // Estabilização do ADC

	  // BLOCO 2: CONVERSÃO DE GRANDEZAS E CÁLCULOS MATEMÁTICOS
	        // Conversões Lineares (Reta)
	        nivel_gas = ((raw_gas - 30) * 100) / 4030;
	        umidade_cub = ((raw_umidade - 30) * 100) / 4030;
	        temperatura_ambiente = (((raw_temp_amb - 30) * 55) / 4030) + 10;

	        // Cálculos do Sistema Elétrico
	        tensao_fase = (((raw_tensao - 30) * 362) / 4030) + 80; // 80 a 442 kV
	        tensao_linha = (tensao_fase * 1732) / 1000;            // kV
	        corrente_linha = Calcular_Corrente_Estrela(tensao_fase); // A

	        // Modelo Térmico (Imagem Térmica do Trafo)
	        temperatura_alvo = (temperatura_ambiente * 2/3) + (corrente_linha/20);
	        if (temperatura_alvo > 150) temperatura_alvo = 150;

	        // Triângulo de Potências
	        uint64_t s_calc = (uint64_t)tensao_linha * corrente_linha * 1732;
	        potencia_aparente = (uint32_t)(s_calc / 1000000); // MVA

	        potencia_ativa = (potencia_aparente * fator_potencia) / 100; // MW

	        double s_sq = (double)potencia_aparente * (double)potencia_aparente;
	        double p_sq = (double)potencia_ativa * (double)potencia_ativa;
	        if (p_sq > s_sq) p_sq = s_sq;
	        potencia_reativa = (uint32_t)sqrt(s_sq - p_sq); // MVAr

	        // Cálculo de Controle do Aquecedor (PWM)
	        if (umidade_cub <= 30) intensidade_aquecedor = 0;
	        else intensidade_aquecedor = (((umidade_cub - 30) * 100) / 70);

	  // BLOCO 3: LÓGICA DE PROTEÇÃO (MONITORAMENTO)

	        // Monitoramento de Tensão (ANSI 27/59)
	        if (tensao_linha < LIMITE_SUBTENSAO) status_tensao = 1;      // Subtensão
	        else if (tensao_linha > LIMITE_SOBRETENSAO) status_tensao = 2; // Sobretensão
	        else status_tensao = 0;                                      // Normal

	        // Lógica de Falha no Transformador (Gerenciada pelo TIM2 e Interrupt)
	        if (estado_reserva == 0)
	        {
	        if (temperatura_trans > 93 || status_tensao != 0) {
	               if (timer_rodando == 0) {
	                      HAL_TIM_Base_Start_IT(&htim2);
	                      timer_rodando = 1;
	                  }
	               } else {
	               if (timer_rodando == 1) {
	                     HAL_TIM_Base_Stop_IT(&htim2);
	                   __HAL_TIM_SET_COUNTER(&htim2, 0);
	                     timer_rodando = 0;
	                }
	              }
	            }
	              else // Estado Reserva
	              {
	            if (temperatura_trans < 93 && status_tensao == 0) {
	                if (pode_rearmar == 0 && timer_rodando == 0) {
	                HAL_TIM_Base_Start_IT(&htim2);
	                timer_rodando = 1;
	                 }
	               } else {
	               if (timer_rodando == 1) {
	               HAL_TIM_Base_Stop_IT(&htim2);
	             __HAL_TIM_SET_COUNTER(&htim2, 0);
	               timer_rodando = 0;
	               }
	               pode_rearmar = 0;
	             }
	        }

	        //Alarme de alerta para subcorrente ou sobrecorrente
	        if(corrente_linha < 812 || corrente_linha > 1347){
	        	uint32_t tempo_atual = HAL_GetTick();
	        	if ((tempo_atual - tempo_anterior_led) >= 250)
	        	{
	        		HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_12);
	        		tempo_anterior_led = tempo_atual;
	        	}
	        	}else if(corrente_linha < 962 || corrente_linha > 1176){
	        		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_SET);
	        }else
	          {
	           HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);
	        }

	  // BLOCO 4: CONTROLE DO DISJUNTOR (ANSI 52)

	      // Lógica de Trip (Abertura)
	        if (estado_reserva == 1)
	              {
	                  if (status_disjuntor == ESTADO_DISJUNTOR_FECHADO)
	                  {
	                      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);
	                      status_disjuntor = ESTADO_DISJUNTOR_ABERTO;
	                      // Zera as correntes e potências (Corte de energia)
	                      corrente_linha = 0;
	                      potencia_aparente = 0;
	                      potencia_ativa = 0;
	                      potencia_reativa = 0;
	                  }}
	      // Lógica de Close (Fechamento/Rearme)
	        if (pode_rearmar == 1 && status_tensao == 0)
	              {
	                   // Verifica Botão de Rearme (PB3)
	                   if (!HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_3))
	                   {
	                        estado_reserva = 0;
	                        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET);
	                        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_SET);
	                        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);
	                        status_disjuntor = ESTADO_DISJUNTOR_FECHADO;
	                        HAL_Delay(500);
	                   }}

	      // Força medidas a zero se disjuntor estiver aberto
	       if (status_disjuntor == ESTADO_DISJUNTOR_ABERTO)
	      {
	      corrente_linha = 0;
	      }

	  // BLOCO 5: INTERFACE HOMEM-MÁQUINA (IHM/BOTÕES)

	      // Transição Automática de Tela em Caso de Emergência
	      if (estado_reserva == 1 && tela_atual != TELA_ALERTA){
	       tela_atual = TELA_ALERTA;
	       flag_atualizar_tela = 1;	      }
	       else if (estado_reserva == 0 && tela_atual == TELA_ALERTA){
	       tela_atual = TELA_MENU;
	       flag_atualizar_tela = 1;	      }

	       // Navegação Manual (B4 - Próximo)
	       if (!HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4)){
	       if (tela_atual == TELA_MENU){
	        menu_index++;
	        if (menu_index > 4) menu_index = 0;
	        flag_atualizar_tela = 1;	        }
	         HAL_Delay(200);	        }

	       // Confirmação (B8 - Entrar)
	       if (!HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_8)){
	       if (tela_atual == TELA_MENU){
	       tela_atual = menu_index + 1;
	       flag_atualizar_tela = 1;	       }
	       HAL_Delay(200);	       }

	       // Retorno (B9 - Voltar)
	       if (!HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_9)){
	       if (tela_atual != TELA_MENU && tela_atual != TELA_ALERTA){
	        tela_atual = TELA_MENU;
	        flag_atualizar_tela = 1;	       }
	       HAL_Delay(200);	      }

	  // BLOCO 6: ATUALIZAÇÃO DO DISPLAY LCD

	       if (flag_atualizar_tela == 1 || (flag_atualizar_lcd == 1 && tela_atual != TELA_MENU))
	             {
	                 if (flag_atualizar_tela == 1) HD44780_Clear();

	                 switch (tela_atual)
	                 {
	                     // --- TELA 0: MENU PRINCIPAL ---
	                     case TELA_MENU:
	                          HD44780_SetCursor(0,0);
	                          HD44780_PrintStr("MENU PRINCIPAL: ");
	                          HD44780_SetCursor(0,1);
	                          HD44780_PrintStr(textos_menu[menu_index]);
	                          break;

	                     // --- TELA 1: ELÉTRICA (V e I) ---
	                     case TELA_ELETRICA:
	                          // Prioridade 1: Disjuntor Aberto (TRIP)
	                          if (status_disjuntor == ESTADO_DISJUNTOR_ABERTO)
	                          {
	                              HD44780_SetCursor(0,0);
	                              HD44780_PrintStr("STATUS: TRIP !!!");
	                              HD44780_SetCursor(0,1);
	                              HD44780_PrintStr("DISJUNTOR ABERTO");
	                          }
	                          // Prioridade 2: Erro de Tensão (Sub/Sobretensão)
	                          else if (status_tensao != 0)
	                          {
	                              HD44780_SetCursor(0,0);
	                              HD44780_PrintStr("FALHA TENSAO !  ");
	                              HD44780_SetCursor(0,1);
	                              if(status_tensao == 1) HD44780_PrintStr("SUB-TENSAO      ");
	                              else HD44780_PrintStr("SOBRE-TENSAO    ");
	                          }
	                          // Prioridade 3: Sistema Normal (Mostra V e I fixos)
	                          else
	                          {
	                              sprintf(lcd_buffer, "V:%lukV I:%luA  ", tensao_linha, corrente_linha);
	                              HD44780_SetCursor(0,0);
	                              HD44780_PrintStr("MONIT. ELETRICA ");
	                              HD44780_SetCursor(0,1);
	                              HD44780_PrintStr(lcd_buffer);
	                          }
	                          break;

	                     // --- TELA 2: CUBÍCULO (Umidade + Aquecedor) ---
	                     case TELA_CUBICULO:
	                          sprintf(lcd_buffer, "Umid:%lu%% Pot:%lu%%", umidade_cub, intensidade_aquecedor);
	                          HD44780_SetCursor(0,0);
	                          HD44780_PrintStr("MONIT. CUBICULO ");
	                          HD44780_SetCursor(0,1);
	                          HD44780_PrintStr(lcd_buffer);
	                          break;

	                     // --- TELA 3: TEMPERATURA ---
	                     case TELA_TEMP:
	                          sprintf(lcd_buffer, "Temp T: %lu C    ", temperatura_trans);
	                          HD44780_SetCursor(0,0);
	                          HD44780_PrintStr(lcd_buffer);
	                          HD44780_SetCursor(0,1);
	                          if(estado_reserva == 0) HD44780_PrintStr("Status: NORMAL  ");
	                          else HD44780_PrintStr("Status: RESERVA ");
	                          break;

	                     // --- TELA 4: GÁS SF6 ---
	                     case TELA_GAS:
	                          sprintf(lcd_buffer, "Nivel: %lu %%     ", nivel_gas);
	                          HD44780_SetCursor(0,0);
	                          HD44780_PrintStr("MONIT. GAS SF6  ");
	                          HD44780_SetCursor(0,1);
	                          HD44780_PrintStr(lcd_buffer);
	                          break;

	                     // --- TELA 5: POTÊNCIAS ---
	                     case TELA_POTENCIA:
	                          sprintf(lcd_buffer, "P:%luM Q:%luM   ", potencia_ativa, potencia_reativa);
	                          HD44780_SetCursor(0,0);
	                          HD44780_PrintStr(lcd_buffer);

	                          sprintf(lcd_buffer, "S:%luMVA FP:%lu ", potencia_aparente, fator_potencia);
	                          HD44780_SetCursor(0,1);
	                          HD44780_PrintStr(lcd_buffer);
	                          break;

	                     // --- TELA 6: ALERTA CRÍTICO ---
	                     case TELA_ALERTA:
	                          HD44780_SetCursor(0,0);
	                          HD44780_PrintStr("! FALHA TRAFO ! ");
	                          HD44780_SetCursor(0,1);
	                          HD44780_PrintStr("OPERANDO RESERVA");
	                          break;
	                 }

	                 flag_atualizar_tela = 0;
	                 flag_atualizar_lcd = 0;
	             }

	 // BLOCO 7: ATUAÇÃO DE SAÍDAS AUXILIARES

	      // Controle de LEDs de Alerta de Gás
	      if(nivel_gas < 10)
	      {
	      uint32_t tempo_atual = HAL_GetTick();
	      if ((tempo_atual - tempo_anterior_led) >= 250)
	      {
	      HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_15);
	      tempo_anterior_led = tempo_atual;
	      }
	      } else if(nivel_gas < 25)
	      {
	      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET);
	      } else
	             {
	                 HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);
	             }

	       // Controle do Aquecedor (PWM)
	       if(umidade_cub > 30)
	       {
	        htim1.Instance->CCR1 = (htim1.Instance->ARR*intensidade_aquecedor)/100;
	        }
	        else
	       {
	        htim1.Instance->CCR1 = 0;
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
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
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
  sConfig.Channel = ADC_CHANNEL_0;
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
  hi2c1.Init.ClockSpeed = 400000;
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
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 4570;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 1830;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

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
  htim2.Init.Prescaler = 9600-1;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 20000-1;
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

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 9600-1;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 5000-1;
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
  htim4.Init.Prescaler = 9600-1;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 1000-1;
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
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12
                          |GPIO_PIN_15, GPIO_PIN_RESET);

  /*Configure GPIO pin : PB15 */
  GPIO_InitStruct.Pin = GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PA9 PA10 PA11 PA12
                           PA15 */
  GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12
                          |GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB3 PB4 PB8 PB9 */
  GPIO_InitStruct.Pin = GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_8|GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

//FUNÇÃO DE INTERRUPÇÃO

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_15)
    {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);

        status_disjuntor = ESTADO_DISJUNTOR_ABERTO;
        estado_reserva = 1;
        pode_rearmar = 0;
        corrente_linha = 0;
    }
}

//Função para leitura das variáveis analógicas

uint32_t Ler_ADC_Canal(uint32_t canal)
{
  ADC_ChannelConfTypeDef sConfig = {0};
  uint32_t adc_valor = 0;

  // 1. Configura o Canal do ADC para a leitura atual
  sConfig.Channel = canal;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_15CYCLES; //Tempo de amostragem

  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_ADC_Start(&hadc1);                     //Start do conversor AD
  if (HAL_ADC_PollForConversion(&hadc1, 100) == HAL_OK)
  {
    adc_valor = HAL_ADC_GetValue(&hadc1);    //Realiza a leitura do conversor AD
  }
  HAL_ADC_Stop(&hadc1);                      //Stop do conversor AD

  return adc_valor;
}

//TEMPORIZADORES

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	//FUNÇÃO DE MUDANÇA PARA TRANSFORMADOR RESERVA
    if (htim->Instance == TIM2)
    {
        if (estado_reserva == 0){
            estado_reserva = 1;
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);
            pode_rearmar = 0;
        }
        // CASO B: Estava na Reserva e a temperatura ficou baixa por 2s
        else{
            pode_rearmar = 1;
        }
        HAL_TIM_Base_Stop_IT(htim);
        timer_rodando = 0;
    }

    //FUNÇÃO DE ATUALIZAÇÃO LCD
    if (htim->Instance == TIM3){
            flag_atualizar_lcd = 1;
        }

    //SIMULAÇÃO FISICA TÉRMICA
    if (htim->Instance == TIM4){
         if (temperatura_trans < temperatura_alvo){
        temperatura_trans++; // Aquecendo
        }
        else if (temperatura_trans > temperatura_alvo){
        temperatura_trans--; // Resfriando (Retornando ao normal)
       }}
}

//Função para cálculo da corrente de linha

uint32_t Calcular_Corrente_Estrela(uint32_t tensao_fase_kV)
{
    // Proteção contra divisão por zero
    if (RESISTENCIA_CARGA_OHMS == 0) return 0;
    // Se a tensão for 0, corrente é 0
    if (tensao_fase_kV == 0) return 0;

    // 1. Converter kV para Volts
    uint32_t tensao_volts = tensao_fase_kV * 1000;

    // 2. Lei de Ohm: I = V_fase / R
    return (tensao_volts / RESISTENCIA_CARGA_OHMS);
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
