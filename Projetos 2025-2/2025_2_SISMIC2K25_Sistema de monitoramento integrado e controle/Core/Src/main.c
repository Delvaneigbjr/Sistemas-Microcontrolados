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

#include "lcd_i2c.h"
#include <stdio.h> // Essencial para sprintf
#include <math.h>  // Para cálculos

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
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;

/* USER CODE BEGIN PV */

// ============================================================================
// 1. MÁQUINA DE ESTADOS
// ============================================================================
typedef enum {
  BOOT, GRID_MODE, SOLAR_MODE, BATTERY_MODE,
  PROT_SOBRECARGA, PROT_TERMICA, EMERGENCIA, ESTADO_SHUTDOWN
} SistemaEstado_t;

volatile SistemaEstado_t estado_atual = BOOT;
volatile SistemaEstado_t estado_anterior = BOOT;

// ============================================================================
// 2. SENSORES FÍSICOS (5 ADCs)
// ============================================================================
float v_rede = 0.0;        // PA0 - Tensão da Rede (0-300V)
float i_carga = 0.0;       // PA1 - Corrente da Carga (0-10A)
float v_solar = 0.0;       // PA2 - Tensão Solar (0-50V)
float temperatura = 0.0;   // PA3 - Temperatura (0-100°C)
float luminosidade = 0.0;  // PA4 - Sensor LDR (0-100%) ← NOVO!
float v_bateria = 12.6;    // Simulado (poderia ser PA5)

// ============================================================================
// 3. MODELOS MATEMÁTICOS E CÁLCULOS
// ============================================================================
float pot_rede = 0.0;
float pot_solar_w = 0.0;
float pot_total_w = 0.0;
float energia_kwh = 0.0;
float custo_reais = 0.0;
float carbono_kg = 0.0;
float soc_bateria = 100.0;
float autonomia_h = 99.0;
float frequencia = 60.0;
float fator_potencia = 0.92;

// ============================================================================
// 4. INTERFACE
// ============================================================================
LCD_HandleTypeDef lcd;       // ← Novo handle da biblioteca profissional
volatile int tela_lcd = 0;   // 0=Geral, 1=Carga, 2=Solar, 3=Bat, 4=Finan, 5=Tec
char buffer_lcd[32];

// ============================================================================
// 5. CONSTANTES DE PROTEÇÃO
// ============================================================================
const float LIMITE_CORRENTE = 8.0;
const float LIMITE_TEMP = 70.0;
const float LIMITE_BLACKOUT = 180.0;
const float LIMITE_BAT_MIN = 10.5;
const float TARIFA = 0.85;

// ============================================================================
// 6. CONTROLE DE INTERRUPÇÕES
// ============================================================================
volatile uint32_t flag_calculo_1s = 0;  // TIM2: Cálculos financeiros a cada 1s
volatile uint32_t ultimo_debounce = 0;  // Anti-repique de botões
volatile uint8_t flag_limpar_lcd = 0;     // Sinaliza que precisa limpar LCD
volatile uint8_t flag_atualizar_tela = 0; // Sinaliza que mudou de tela
volatile uint32_t tempo_botao_pressionado = 0;  // Contador de tempo do botão
volatile uint8_t flag_reset_sistema = 0;        // Sinaliza reset completo

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM4_Init(void);

/* USER CODE BEGIN PFP */

// Funções de Atuadores
void AtualizarBarraLeds(float pct);
void AtualizarCooler(float temp);


uint32_t LerADC(uint32_t canal);

// Funções de Interface
void DesenharTela(void);
void TratarMudancaEstado(void);

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
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  /* USER CODE BEGIN 2 */

  // ============================================================================
    // INICIALIZAÇÃO DO SISTEMA
    // ============================================================================

    // Inicializa LCD (detecta endereço automaticamente)
    if(LCD_Init(&lcd, &hi2c1) != LCD_OK) {
        // LCD falhou! Pisca LED vermelho
        while(1) {
            HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_8);
            HAL_Delay(200);
        }
    }

    // Inicia PWMs
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1); // Cooler (PB6)
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2); // LED Solar (PB7)

    // Inicia Interrupções de Timer
    HAL_TIM_Base_Start_IT(&htim2); // 1 segundo (cálculos financeiros)
    HAL_TIM_Base_Start_IT(&htim3); // 0.5 segundo (piscar LED no nobreak)

    // Tela de Abertura
    LCD_Clear(&lcd);
    LCD_PrintAt(&lcd, 0, 0, "SISMIC 2K25 v2.0");
    LCD_PrintAt(&lcd, 1, 0, "Guilherme Wilke");
    HAL_Delay(2000);

    LCD_Clear(&lcd);
    LCD_PrintAt(&lcd, 0, 0, "Inicializando...");
    LCD_PrintAt(&lcd, 1, 0, "[");

    // Barra de progresso de inicialização
    for(int i = 0; i < 14; i++) {
        LCD_Print(&lcd, "=");
        HAL_Delay(100);
    }
    LCD_Print(&lcd, "]");
    HAL_Delay(500);

    LCD_Clear(&lcd);
    estado_atual = GRID_MODE;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

	  // ========================================================================
	      // 1. LEITURA DE SENSORES (5 ADCs)
	      // ========================================================================
	      v_rede = (float)LerADC(ADC_CHANNEL_0) * 0.0732;      // 0-300V
	      i_carga = (float)LerADC(ADC_CHANNEL_1) * 0.00244;    // 0-10A
	      v_solar = (float)LerADC(ADC_CHANNEL_2) * 0.0122;     // 0-50V
	      temperatura = (float)LerADC(ADC_CHANNEL_3) * 0.0244; // 0-100°C
	      luminosidade = (float)LerADC(ADC_CHANNEL_4) * 0.0244; // 0-100%

	      // ========================================================================
	      // 2. MODELOS MATEMÁTICOS E FÍSICA DO SISTEMA
	      // ========================================================================

	      // Modelo 1: Fator de Potência Dinâmico
	      fator_potencia = (i_carga > 1.0) ? 0.95 : 0.80;

	      // Modelo 2: Potência da Rede (P = V × I × FP)
	      float v_calc = (estado_atual == BATTERY_MODE) ? 220.0 : v_rede;
	      pot_rede = v_calc * i_carga * fator_potencia;

	      // Modelo 3: Potência Solar (ajustada por luminosidade)
	      if(v_solar > 12.0 && luminosidade > 20.0) {
	          pot_solar_w = (v_solar * 3.5) * (luminosidade / 100.0); // Afetada pela luz!
	      } else {
	          pot_solar_w = 0.0;
	      }

	      // Modelo 4: Potência Total
	      pot_total_w = pot_rede + pot_solar_w;

	      // Modelo 5 e 6: Simulação de Bateria (Carga/Descarga)
	      if (estado_atual == GRID_MODE || estado_atual == SOLAR_MODE) {
	          if (v_bateria < 13.8) v_bateria += 0.03; // Carregando
	          frequencia = 60.0;
	      } else if (estado_atual == BATTERY_MODE) {
	          float dreno = 0.02 + (i_carga * 0.01);
	          v_bateria -= dreno; // Descarregando
	          if(v_bateria < 0) v_bateria = 0;
	          frequencia = 59.5;
	      }

	      // Modelo 7: Cálculo de Autonomia (Ah × V / I)
	      if(i_carga > 0.5) {
	          autonomia_h = (50.0 * (v_bateria / 12.0)) / i_carga;
	      } else {
	          autonomia_h = 99.0;
	      }

	      // ========================================================================
	      // 3. MÁQUINA DE ESTADOS (8 Estados)
	      // ========================================================================
	      estado_anterior = estado_atual;

	      switch(estado_atual) {
	          case GRID_MODE:
	              HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, 1); // Verde ON
	              HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, 0);  // Vermelho OFF

	              // Transições
	              if (pot_solar_w > 50 && v_bateria > 13) estado_atual = SOLAR_MODE;
	              if (v_rede < LIMITE_BLACKOUT) estado_atual = BATTERY_MODE;
	              if (i_carga > LIMITE_CORRENTE) estado_atual = PROT_SOBRECARGA;
	              if (temperatura > LIMITE_TEMP) estado_atual = PROT_TERMICA;
	              break;

	          case SOLAR_MODE:
	              HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, 1);
	              HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, 0);

	              if (v_bateria < 12.2 || pot_solar_w < 10) estado_atual = GRID_MODE;
	              if (v_rede < LIMITE_BLACKOUT) estado_atual = BATTERY_MODE;
	              break;

	          case BATTERY_MODE:
	              HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, 0);
	              // LED vermelho pisca automaticamente via TIM3 (interrupção)

	              if (v_rede > (LIMITE_BLACKOUT + 10)) estado_atual = GRID_MODE;
	              if (v_bateria < LIMITE_BAT_MIN) estado_atual = ESTADO_SHUTDOWN;
	              break;

	          case PROT_SOBRECARGA:
	          case PROT_TERMICA:
	          case ESTADO_SHUTDOWN:
	          case EMERGENCIA:
	              HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, 1); // Vermelho FIXO
	              HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, 0);
	              pot_total_w = 0; // Corta carga

	              // Histerese automática (menos emergência)
	              if (i_carga < 1.0 && temperatura < 40.0 && v_rede > 200 && estado_atual != EMERGENCIA) {
	                   estado_atual = GRID_MODE;
	              }
	              break;

	          default:
	              estado_atual = GRID_MODE;
	              break;
	      }

	      // Detecta mudança de estado e atualiza LCD
	      if(estado_atual != estado_anterior) {
	          TratarMudancaEstado();
	      }

	      // ========================================================================
	      // 4. CÁLCULOS FINANCEIROS (executado via interrupção TIM2)
	      // ========================================================================
	      // NOTA: Modelos 8, 9, 10 são calculados em HAL_TIM_PeriodElapsedCallback()

	      if(flag_calculo_1s) {
	          flag_calculo_1s = 0;

	          // Modelo 8: Energia consumida (kWh)
	          energia_kwh += (pot_total_w / 1000.0) / 3600.0;

	          // Modelo 9: Custo financeiro (R$)
	          custo_reais = energia_kwh * TARIFA;

	          // Modelo 10: CO2 evitado (kg)
	          if (estado_atual == SOLAR_MODE) carbono_kg += 0.001;

	          // Modelo 11: State of Charge (%)
	          soc_bateria = ((v_bateria - 10.5) / (13.8 - 10.5)) * 100.0;
	          if(soc_bateria < 0) soc_bateria = 0;
	          if(soc_bateria > 100) soc_bateria = 100;
	      }

	      // ========================================================================
	      // 5. ATUALIZAÇÃO DA INTERFACE LCD
	      // ========================================================================
	      DesenharTela();

	      // ========================================================================
	      // 6. ATUADORES (LEDs e PWM)
	      // ========================================================================
	      if(estado_atual < PROT_SOBRECARGA) {
	          float pct = (pot_total_w * 100.0) / 2200.0;
	          AtualizarBarraLeds(pct);
	          AtualizarCooler(temperatura);

	          // PWM do LED Solar (proporcional à geração e luminosidade)
	          int pwm_solar = (int)(pot_solar_w * 100);
	          if(pwm_solar > 65535) pwm_solar = 65535;
	          __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_2, pwm_solar);
	      } else {
	          AtualizarBarraLeds(0); // Apaga barra em erro
	          __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_2, 0);
	      }

	      // ========================================================================
	      // 7. TRATAMENTO DE BOTÃO RESET LONGO (PB14)
	      // ========================================================================
	      // Verifica se o botão PB14 está sendo segurado
	      if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_14) == GPIO_PIN_RESET && tempo_botao_pressionado > 0) {
	          uint32_t tempo_segurado = HAL_GetTick() - tempo_botao_pressionado;

	          // Se segurou por mais de 2 segundos
	          if(tempo_segurado >= 2000) {
	              flag_reset_sistema = 1;
	              tempo_botao_pressionado = 0; // Reseta contador
	          }
	      }

	      // Quando o botão é SOLTO
	      if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_14) == GPIO_PIN_SET && tempo_botao_pressionado > 0) {
	          uint32_t tempo_segurado = HAL_GetTick() - tempo_botao_pressionado;
	          tempo_botao_pressionado = 0; // Reseta contador

	          // Se foi clique CURTO (menos de 2 segundos) → Reset de contadores
	          if(tempo_segurado < 2000) {
	              energia_kwh = 0.0;
	              custo_reais = 0.0;
	              carbono_kg = 0.0;
	              flag_atualizar_tela = 1;
	          }
	      }

	      // ========================================================================
	      // 8. EXECUÇÃO DO RESET COMPLETO DO SISTEMA
	      // ========================================================================
	      if(flag_reset_sistema) {
	          flag_reset_sistema = 0;

	          // Feedback visual
	          LCD_Clear(&lcd);
	          LCD_PrintAt(&lcd, 0, 0, "RESET SISTEMA");
	          LCD_PrintAt(&lcd, 1, 0, "Reiniciando...");

	          // Desliga todos os LEDs
	          HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
	          HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_RESET);
	          AtualizarBarraLeds(0);

	          HAL_Delay(1500);

	          // RESET COMPLETO DO ESTADO
	          estado_atual = GRID_MODE;
	          estado_anterior = BOOT;

	          // Reseta variáveis críticas
	          v_bateria = 12.6;
	          pot_total_w = 0;

	          // Liga LED verde
	          HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_SET);
	          HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);

	          // Limpa LCD e volta ao normal
	          LCD_Clear(&lcd);
	          LCD_PrintAt(&lcd, 0, 0, "Sistema OK!");
	          LCD_PrintAt(&lcd, 1, 0, "Modo Rede Ativo");
	          HAL_Delay(1500);
	          LCD_Clear(&lcd);
	      }

	      // ========================================================================
	      // 9. RECOVERY AUTOMÁTICO DE SHUTDOWN (Bateria Vazia)
	      // ========================================================================
	      // Se está em SHUTDOWN mas a rede voltou, sai automaticamente
	      if(estado_atual == ESTADO_SHUTDOWN) {
	          if(v_rede > 200.0) {  // Se a rede voltou com tensão boa
	              // Simula que a bateria começou a carregar
	              v_bateria = 11.0;  // Valor acima do mínimo (10.5V)

	              // Volta para GRID_MODE
	              estado_atual = GRID_MODE;

	              // Feedback visual
	              LCD_Clear(&lcd);
	              LCD_PrintAt(&lcd, 0, 0, "REDE RESTAURADA!");
	              LCD_PrintAt(&lcd, 1, 0, "Carregando Bat..");
	              HAL_Delay(2000);
	              LCD_Clear(&lcd);
	          }
	      }

	      // ========================================================================
	      // 10. TRATAMENTO DE FLAGS DE INTERFACE
	      // ========================================================================
	      if(flag_limpar_lcd) {
	          flag_limpar_lcd = 0;
	          LCD_Clear(&lcd);
	      }

	      if(flag_atualizar_tela) {
	          flag_atualizar_tela = 0;

	          if(estado_atual == EMERGENCIA) {
	              LCD_Clear(&lcd);
	              LCD_PrintAt(&lcd, 0, 0, "! EMERGENCIA !");
	              LCD_PrintAt(&lcd, 1, 0, "Segure Reset 2s");  // ← DICA PARA O USUÁRIO!
	              HAL_Delay(2000);
	          } else {
	              LCD_Clear(&lcd);
	              LCD_PrintAt(&lcd, 0, 0, "CONTADORES");
	              LCD_PrintAt(&lcd, 1, 0, "RESETADOS!");
	              HAL_Delay(1000);
	              LCD_Clear(&lcd);
	          }
	      }

	      HAL_Delay(100); // Loop a 10Hz

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
  ADC_InjectionConfTypeDef sConfigInjected = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = ENABLE;
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

  /** Configures for the selected ADC injected channel its corresponding rank in the sequencer and its sample time
  */
  sConfigInjected.InjectedChannel = ADC_CHANNEL_0;
  sConfigInjected.InjectedRank = 1;
  sConfigInjected.InjectedNbrOfConversion = 4;
  sConfigInjected.InjectedSamplingTime = ADC_SAMPLETIME_3CYCLES;
  sConfigInjected.ExternalTrigInjecConvEdge = ADC_EXTERNALTRIGINJECCONVEDGE_NONE;
  sConfigInjected.ExternalTrigInjecConv = ADC_INJECTED_SOFTWARE_START;
  sConfigInjected.AutoInjectedConv = DISABLE;
  sConfigInjected.InjectedDiscontinuousConvMode = DISABLE;
  sConfigInjected.InjectedOffset = 0;
  if (HAL_ADCEx_InjectedConfigChannel(&hadc1, &sConfigInjected) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configures for the selected ADC injected channel its corresponding rank in the sequencer and its sample time
  */
  sConfigInjected.InjectedRank = 2;
  if (HAL_ADCEx_InjectedConfigChannel(&hadc1, &sConfigInjected) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configures for the selected ADC injected channel its corresponding rank in the sequencer and its sample time
  */
  sConfigInjected.InjectedRank = 3;
  if (HAL_ADCEx_InjectedConfigChannel(&hadc1, &sConfigInjected) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configures for the selected ADC injected channel its corresponding rank in the sequencer and its sample time
  */
  sConfigInjected.InjectedRank = 4;
  if (HAL_ADCEx_InjectedConfigChannel(&hadc1, &sConfigInjected) != HAL_OK)
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
  htim2.Init.Prescaler = 41999;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 1999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
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
  htim3.Init.Prescaler = 41999;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 999;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
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
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM4_Init 1 */

  /* USER CODE END TIM4_Init 1 */
  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 0;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 65535;
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
  if (HAL_TIM_PWM_Init(&htim4) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM4_Init 2 */

  /* USER CODE END TIM4_Init 2 */
  HAL_TIM_MspPostInit(&htim4);

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
  HAL_GPIO_WritePin(GPIOA, LED_BARRA_1_Pin|LED_BARRA_2_Pin|LED_BARRA_3_Pin|GPIO_PIN_8, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LED_BARRA_4_Pin|LED_BARRA_5_Pin|GPIO_PIN_10, GPIO_PIN_RESET);

  /*Configure GPIO pins : LED_BARRA_1_Pin LED_BARRA_2_Pin LED_BARRA_3_Pin PA8 */
  GPIO_InitStruct.Pin = LED_BARRA_1_Pin|LED_BARRA_2_Pin|LED_BARRA_3_Pin|GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : LED_BARRA_4_Pin LED_BARRA_5_Pin PB10 */
  GPIO_InitStruct.Pin = LED_BARRA_4_Pin|LED_BARRA_5_Pin|GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : BTN_MENU_Pin BTN_SOBE_Pin BTN_DESCE_Pin BTN_EMERGENCIA_Pin */
  GPIO_InitStruct.Pin = BTN_MENU_Pin|BTN_SOBE_Pin|BTN_DESCE_Pin|BTN_EMERGENCIA_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

// ============================================================================
// FUNÇÕES DE INTERFACE
// ============================================================================

void DesenharTela(void) {
    static uint32_t ultimo_update = 0;

    if((HAL_GetTick() - ultimo_update) < 500) return;
    ultimo_update = HAL_GetTick();

    LCD_SetCursor(&lcd, 0, 0);

    // TELAS DE ERRO - Com instruções de recovery
    if (estado_atual >= PROT_SOBRECARGA) {
        LCD_PrintAt(&lcd, 0, 0, "! FALHA ATIVA !");
        LCD_SetCursor(&lcd, 1, 0);

        switch(estado_atual) {
            case EMERGENCIA:
                LCD_Print(&lcd, "Segure Reset 2s  "); // ← Instrução de reset
                break;
            case PROT_SOBRECARGA:
                LCD_Print(&lcd, "SOBRECORRENTE!  ");
                break;
            case PROT_TERMICA:
                LCD_Print(&lcd, "SOBREAQUECIMENTO");
                break;
            case ESTADO_SHUTDOWN:
                LCD_Print(&lcd, "Aguarde Rede... "); // ← Indica que vai sair automaticamente
                break;
            default:
                LCD_Print(&lcd, "ERRO SISTEMA    ");
                break;
        }
    } else {
        // Telas normais (6 opções) - MANTÉM IGUAL
        int bat_i = (int)v_bateria;
        int bat_d = (int)((v_bateria - bat_i)*10);

        switch(tela_lcd) {
            case 0: // GERAL
                if(estado_atual == GRID_MODE)
                    sprintf(buffer_lcd, "REDE OK  B:%d.%dV", bat_i, bat_d);
                else if(estado_atual == BATTERY_MODE)
                    sprintf(buffer_lcd, "NOBREAK! B:%d.%dV", bat_i, bat_d);
                else
                    sprintf(buffer_lcd, "MODO SOLAR ECO  ");

                LCD_Print(&lcd, buffer_lcd);
                LCD_SetCursor(&lcd, 1, 0);
                sprintf(buffer_lcd, "%3dV %4dW %2dC", (int)v_rede, (int)pot_total_w, (int)temperatura);
                LCD_Print(&lcd, buffer_lcd);
                break;

            case 1: // CARGA
                LCD_Print(&lcd, "MONITOR CARGA:  ");
                LCD_SetCursor(&lcd, 1, 0);
                sprintf(buffer_lcd, "%4dW  %d.%02dA    ", (int)pot_total_w, (int)i_carga, (int)((i_carga-(int)i_carga)*100));
                LCD_Print(&lcd, buffer_lcd);
                break;

            case 2: // SOLAR
                LCD_Print(&lcd, "GERACAO SOLAR:  ");
                LCD_SetCursor(&lcd, 1, 0);
                sprintf(buffer_lcd, "%4dW Luz:%d%%   ", (int)pot_solar_w, (int)luminosidade);
                LCD_Print(&lcd, buffer_lcd);
                break;

            case 3: // BATERIA
                LCD_Print(&lcd, "BANCO BATERIAS: ");
                LCD_SetCursor(&lcd, 1, 0);
                sprintf(buffer_lcd, "%d.%dV Auto:%dh  ", bat_i, bat_d, (int)autonomia_h);
                LCD_Print(&lcd, buffer_lcd);
                break;

            case 4: // FINANCEIRO
                sprintf(buffer_lcd, "Gasto:R$%d.%02d  ", (int)custo_reais, (int)((custo_reais-(int)custo_reais)*100));
                LCD_Print(&lcd, buffer_lcd);
                LCD_SetCursor(&lcd, 1, 0);
                sprintf(buffer_lcd, "CO2 Evit: %dkg  ", (int)carbono_kg);
                LCD_Print(&lcd, buffer_lcd);
                break;

            case 5: // TÉCNICO
                sprintf(buffer_lcd, "T:%2dC Fq:%2dHz  ", (int)temperatura, (int)frequencia);
                LCD_Print(&lcd, buffer_lcd);
                LCD_SetCursor(&lcd, 1, 0);
                LCD_Print(&lcd, "SISMIC v2.0 OK  ");
                break;
        }
    }
}

void TratarMudancaEstado(void) {
    LCD_Clear(&lcd);

    // Feedback visual ao trocar de estado crítico
    if(estado_atual == BATTERY_MODE) {
        LCD_PrintAt(&lcd, 0, 0, "! BLACKOUT !");
        LCD_PrintAt(&lcd, 1, 0, "Modo Bateria...");
        HAL_Delay(1500);
        LCD_Clear(&lcd);
    } else if(estado_atual == SOLAR_MODE) {
        LCD_PrintAt(&lcd, 0, 0, "Modo Solar Ativo");
        LCD_PrintAt(&lcd, 1, 0, "Economia de Rede");
        HAL_Delay(1500);
        LCD_Clear(&lcd);
    }
}

// ============================================================================
// FUNÇÕES DE ATUADORES
// ============================================================================

void AtualizarBarraLeds(float pct) {
    // Apaga todos os LEDs primeiro
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, 0);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, 0);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, 0);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, 0);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, 0);

    // Acende proporcionalmente
    if(pct > 10) HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, 1);
    if(pct > 30) HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, 1);
    if(pct > 50) HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, 1);
    if(pct > 70) HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, 1);
    if(pct > 90) HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, 1);
}

void AtualizarCooler(float temp) {
    int pwm = 0;

    if (temp > 70) {
        pwm = 65535;
    } else if (temp > 30) {

        pwm = (int)((temp - 30) * (65535.0 / 40.0));
    }

    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, pwm);
}

// ============================================================================
// FUNÇÃO DE LEITURA ADC
// ============================================================================

uint32_t LerADC(uint32_t canal) {
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = canal;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_15CYCLES;

    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);
    uint32_t valor = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);

    return valor;
}

// ============================================================================
// INTERRUPÇÃO 1: BOTÕES VIA EXTI (Resposta Instantânea)
// ============================================================================
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    uint32_t agora = HAL_GetTick();

    // Debounce de 200ms (exceto para PB14 que precisa detectar pressionamento longo)
    if(GPIO_Pin != GPIO_PIN_14 && (agora - ultimo_debounce) < 200) return;

    switch(GPIO_Pin) {
        case GPIO_PIN_12: // MENU (avança tela)
            if((agora - ultimo_debounce) < 200) return;
            ultimo_debounce = agora;

            tela_lcd++;
            if(tela_lcd > 5) tela_lcd = 0;
            flag_limpar_lcd = 1;
            break;

        case GPIO_PIN_13: // SOBE (volta tela)
            if((agora - ultimo_debounce) < 200) return;
            ultimo_debounce = agora;

            tela_lcd--;
            if(tela_lcd < 0) tela_lcd = 5;
            flag_limpar_lcd = 1;
            break;

        case GPIO_PIN_14: // RESET/OK (dupla função: curto=reset contadores, longo=reset sistema)
            // Detecta quando o botão é PRESSIONADO (borda de descida)
            if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_14) == GPIO_PIN_RESET) {
                tempo_botao_pressionado = agora; // Marca o tempo inicial
            }
            break;

        case GPIO_PIN_15: // EMERGÊNCIA
            if((agora - ultimo_debounce) < 200) return;
            ultimo_debounce = agora;

            estado_atual = EMERGENCIA;
            flag_atualizar_tela = 1;
            break;
    }
}

// ============================================================================
// INTERRUPÇÃO 2 e 3: TIMERS (TIM2 = 1s, TIM3 = 0.5s)
// ============================================================================
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    // TIM2: Cálculos financeiros a cada 1 segundo
    if(htim->Instance == TIM2) {
        flag_calculo_1s = 1;
    }

    // TIM3: Piscar LED vermelho no modo BATTERY (0.5s)
    if(htim->Instance == TIM3) {
        if(estado_atual == BATTERY_MODE) {
            HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_8);
        }
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
