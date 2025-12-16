/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body (Medição de Tensão, Corrente e Potência True RMS)
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed for use under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c_lcd.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <math.h> // Necessário para sqrtf, fabsf (True RMS)
#include <stdlib.h> // Necessário para abs()
#include <string.h> // Necessário para comparação de strings (se for usada)
#include <stdio.h> // Necessário para snprintf (formatação de string para o LCD)
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
// Tipos de menu
typedef enum {
   WELCOME,      // Tela inicial (Esperando variação do Pot)
   SELECTION,    // Seleção de menu (Lendo Pot contínuo)
   LOCKED,       // Menu bloqueado (Esperando B3 para voltar)
   TARIFF_CONFIG // Configuração da Tarifa
} MenuState;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// --- Parâmetros ---
// ADC_VALOR_MAXIMO (4095.0f)   // 12-bit ADC - utilizado para traduzir a leitura para corrente e tensão
//V_REF (3.3f)      // Tensão de referência do ADC (VCC a partir do qual ele apresenta os valores)
#define AMOSTRAS_POR_CICLO_DE_MEDICAO  (10000)      // Total de amostras por canal por janela de cálculo (aprox. 60 ciclos em 60Hz)
// --- Parâmetros de Gasto de Energia ---
#define TARIFF_MAX_VALUE    (1.50f)     // Valor máximo da tarifa (R$/kWh)
#define CALC_PERIOD_SECONDS (1.000f)    // Período de cálculo do TIM4 em segundos (~1.0s com a configuração atual)
#define FATOR_DE_CONVERSAO_PARA_KWH (1.0f / 3600000.0f) // Fator de conversão (1 / (3600 * 1000))
#define ADC_MAX_VALUE 4095
#define ADC_THRESHOLD (uint32_t)(ADC_MAX_VALUE * 0.05) // 5% de 4095 ≈ 204
// Definição dos pinos dos botões (PB0 e PB3 com PULL-UP)
#define BTN_SELECT_PIN GPIO_PIN_0 // B0
#define BTN_SELECT_PORT GPIOB
#define BTN_RETURN_PIN GPIO_PIN_3 // B3
#define BTN_RETURN_PORT GPIOB
#define BTN_CONFIG_PIN GPIO_PIN_1 // B1 - Botão de Acesso à Tarifa
#define BTN_CONFIG_PORT GPIOB
#define BLINK_PERIOD_MS 1000 // Período do piscar (1000ms = 1Hz)

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
I2C_LCD_HandleTypeDef lcd1;


// --- Variáveis ---
uint16_t adc_buffer[AMOSTRAS_POR_CICLO_DE_MEDICAO * 4]; // 1. Buffer DMA para 4 canais (V, I, PA2/Menu, PA3/Tarifa)
char lcd_line_buffer[17];// Variável de buffer para a linha do LCD (16 caracteres + NULL)
float voltage_rms = 0.0f; // Tensão True RMS (Volts)
float voltage_avg = 0.0f; // Tensão Média Absoluta
float current_rms = 0.0f; // Corrente True RMS (Amperes)
float current_avg = 0.0f; // Corrente Média Absoluta
float apparent_power = 0.0f; // Potência Aparente (S - VA)
float active_power = 0.0f; // Potência Ativa (P - Watts)
float reactive_power = 0.0f; // Potência Reativa (Q - VAR)
float power_factor = 0.0f; // Fator de Potência (FP)
float energy_kwh = 0.0f;    // Energia Total Consumida Acumulada (kWh)
float energy_cost = 0.0f; // Custo Total Acumulado (R$)
float FATOR_DE_CORRECAO_DA_CORRENTE = 1.2;// Fator de escala para correção da corrente (V/ADC_V)
float FATOR_DE_CORRECAO_DA_TENSAO = 240; // Fator de escala para correção da Tensão (V/ADC_V)
float OFFSET_DO_MEDIDOR_DE_TENSAO = 3232.8;   // Ponto zero para o sensor de Tensão
float OFFSET_DA_CORRENTE = 2900.0; // Ponto zero para o sensor de corrente
uint16_t PONTO_ZERO_TENSAO_BRUTO = 0; // Leitura Média Bruta da Tensão (Para Calibrar OFFSET_DO_MEDIDOR_DE_TENSAO)
uint16_t PONTO_ZERO_CORRENTE_BRUTO = 0; // Leitura Média Bruta da Corrente (Para Calibrar OFFSET_DA_CORRENTE)
uint32_t current_menu_adc = 0; // Valor de 0 a 4095 (média do PA2) - Variáveis de Leitura dos Potenciômetros
uint32_t current_tarifa_adc = 0; // NOVO: Valor de 0 a 4095 (média do PA3)- Variáveis de Leitura dos Potenciômetros
// Variáveis do Menu
MenuState currentState = WELCOME;
uint32_t currentADCValue = 0; // Vai ser o 'current_menu_adc'
uint32_t lastADCValue = 0; // Vai ser o 'current_menu_adc' da leitura anterior
const char* selectedMenuText = ""; // Armazena a string selecionada quando B0 é pressionado (estado LOCKED)
const char* lastDisplayedMenuText = NULL;// VARIÁVEL ANTI-FLICKER.
// Variáveis de Tarifa
float tariff_value = 0.8458f; // Valor da tarifa SALVA (R$/kWh) - Inicializado com o valor anterior
float new_tariff_value_for_display = 0.0f; // Valor da tarifa em CONFIG (para ser ajustado)
// Variáveis dos Botões
uint32_t lastBtnSelectState = GPIO_PIN_SET; // B0
uint32_t lastBtnReturnState = GPIO_PIN_SET; // B3
uint32_t lastBtnConfigState = GPIO_PIN_SET; //  B1
uint32_t lastDebounceTime = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM4_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_I2C1_Init(void);
/* USER CODE BEGIN PFP */
uint8_t checkButtons(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/**
 * @brief  Função de debounce e leitura dos botões (B0, B3 e B1).
 * @param  None
 * @retval 1 se BTN_SELECT (B0), 2 se BTN_RETURN (B3), 3 se BTN_CONFIG (B1), 0 caso contrário.
 */
uint8_t checkButtons(void)
{
   // Tempo atual
   uint32_t currentTick = HAL_GetTick();
   // Se o tempo decorrido desde o último debounce é menor que o DELAY 50, retorna 0 (Nenhum evento)
   if ((currentTick - lastDebounceTime) < 50) {
       return 0;
   }
   // 1=SELECT (B0), 2=RETURN (B3), 3=CONFIG (B1), 0=NONE
   uint8_t event = 0;

   // Checagem do Botão SELECT (B0)
   uint32_t currentBtnSelectState = HAL_GPIO_ReadPin(BTN_SELECT_PORT, BTN_SELECT_PIN);
   // B0 é pressionado (LOW)
   if (currentBtnSelectState != lastBtnSelectState) {
       // Se mudou de estado, e o estado atual é LOW (pressionado)
       if (currentBtnSelectState == GPIO_PIN_RESET) {
           // Evento de Pressionamento Detectado
           event = 1;
           lastDebounceTime = currentTick; // Atualiza o tempo para evitar múltiplos disparos
       }
   }
   lastBtnSelectState = currentBtnSelectState;

   // Checagem do Botão RETURN (B3)
   uint32_t currentBtnReturnState = HAL_GPIO_ReadPin(BTN_RETURN_PORT, BTN_RETURN_PIN);
   // B3 é pressionado
   if (currentBtnReturnState != lastBtnReturnState) {
       // Se mudou de estado
       if (currentBtnReturnState == GPIO_PIN_RESET) {
           // Evento de Pressionamento Detectado
           // B3 tem prioridade de "Return" ou "Exit"
           event = 2;
           lastDebounceTime = currentTick; // Atualiza o tempo para evitar múltiplos disparos
       }
   }
   lastBtnReturnState = currentBtnReturnState;

   // Checagem do Botão CONFIG (B1)
   uint32_t currentBtnConfigState = HAL_GPIO_ReadPin(BTN_CONFIG_PORT, BTN_CONFIG_PIN);
   // B1 é pressionado (LOW, pois usa PULL-UP)
   if (currentBtnConfigState != lastBtnConfigState) {
       // Se mudou de estado
       if (currentBtnConfigState == GPIO_PIN_RESET) {
           // Evento de Pressionamento Detectado
           event = 3; // Evento 3 para CONFIG
           lastDebounceTime = currentTick;
       }
   }
   lastBtnConfigState = currentBtnConfigState;

   return event;
}
/*
* Função de Callback do Timer: É chamada quando o TIM4 alcança o período (1.0s)
* para calcular todas as grandezas (RMS, Potência, Energia).
*/
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	// Se a interrupção veio do TIM4 (nosso Timer de Janela de Cálculo)
	if (htim->Instance == TIM4)
	{
		// Acumuladores para Tensão e Corrente (RMS e Média)
		float v_sum_of_squares = 0.0f;
		float v_sum_of_absolute_values = 0.0f;
		float i_sum_of_squares = 0.0f;
		float i_sum_of_absolute_values = 0.0f;
		float p_sum_of_products = 0.0f;// Acumulador para Potência Ativa (P) - Média do produto instantâneo v(t) * i(t)
		uint32_t SOMA_DO_VALOR_BRUTO_DA_CORRENTE = 0; // Para calibração de corrente (DC Offset)
		uint32_t SOMA_DO_VALOR_BRUTO_DA_TENSAO = 0; // Para calibração de tensão (DC Offset)
        uint32_t SOMA_DO_VALOR_BRUTO_DO_MENU = 0; // Para o Potenciômetro (PA2)
        uint32_t SOMA_DO_VALOR_BRUTO_DA_TARIFA = 0; //  Para o Potenciômetro da Tarifa (PA3)

		// O buffer contém: [V0, I0, MENU_POT0, TARIFA_POT0, V1, I1, MENU_POT1, TARIFA_POT1, ...]
		// Iteramos pelo número de amostras POR CANAL (SAMPLES_PER_WINDOW)
		for (uint32_t i = 0; i < AMOSTRAS_POR_CICLO_DE_MEDICAO; i++)
		{
			// Os índices usam i * 4 para pular entre amostras do mesmo canal
			float raw_v_val = (float)adc_buffer[i * 4];        // Posição 0 (PA0 - V)
			float raw_i_val = (float)adc_buffer[i * 4 + 1];    // Posição 1 (PA1 - I)
            uint32_t raw_menu_val = adc_buffer[i * 4 + 2];     // Posição 2 (PA2 - Menu Pot)
            uint32_t raw_tarifa_val = adc_buffer[i * 4 + 3];   // Posição 3 (PA3 - Tarifa Pot) <-- LEITURA DO PA3

			// Acumula o valor bruto dos potenciômetros
			SOMA_DO_VALOR_BRUTO_DO_MENU += raw_menu_val;
            SOMA_DO_VALOR_BRUTO_DA_TARIFA += raw_tarifa_val; // NOVO ACUMULADOR

			// === 1. CENTRAlizando em 0 e ESCALA (Tensão) ===
			SOMA_DO_VALOR_BRUTO_DA_TENSAO += (uint32_t)raw_v_val; // Acumula o valor bruto da Tensão (para debug/calibração)
			float v_adc_centered = (raw_v_val - OFFSET_DO_MEDIDOR_DE_TENSAO) * (3.3 / 4095);// Centralizando em 0
			float v_rede = v_adc_centered * FATOR_DE_CORRECAO_DA_TENSAO;
			v_sum_of_squares += v_rede * v_rede; // Tensão real ao quadrado somada para RMS
			v_sum_of_absolute_values += fabsf(v_rede); // Valor absoluto dessa soma para média retificada

			// === 2. CENTRAlizando em 0 e ESCALA (Corrente) ===
			SOMA_DO_VALOR_BRUTO_DA_CORRENTE += (uint32_t)raw_i_val; // Acumula o valor bruto da Corrente (para debug/calibração)
			float i_adc_centered = (raw_i_val - OFFSET_DA_CORRENTE) * (3.3 / 4095);// Centralizando em 0
			float i_rede = i_adc_centered * FATOR_DE_CORRECAO_DA_CORRENTE;// Correcao para valor medido multimetro
			i_sum_of_squares += i_rede * i_rede;
			i_sum_of_absolute_values += fabsf(i_rede);// média retificada

			// === 3. CÁLCULO DA POTÊNCIA INSTANTÂNEA ===
			// P_instantanea = v(t) * i(t)
			p_sum_of_products += v_rede * i_rede; // para calculo da potência ativa
		}

		// --- CÁLCULOS FINAIS ---
		if (AMOSTRAS_POR_CICLO_DE_MEDICAO > 0)
		{
			// Tensão RMS e Média
			voltage_rms = sqrtf(v_sum_of_squares / AMOSTRAS_POR_CICLO_DE_MEDICAO);
			voltage_avg = v_sum_of_absolute_values / AMOSTRAS_POR_CICLO_DE_MEDICAO;
			// Corrente RMS e Média
			current_rms = sqrtf(i_sum_of_squares / AMOSTRAS_POR_CICLO_DE_MEDICAO);
			current_avg = i_sum_of_absolute_values / AMOSTRAS_POR_CICLO_DE_MEDICAO;

			// Cálculo da Média dos Potenciômetros (PA2 e PA3)
			current_menu_adc = SOMA_DO_VALOR_BRUTO_DO_MENU / AMOSTRAS_POR_CICLO_DE_MEDICAO;
            current_tarifa_adc = SOMA_DO_VALOR_BRUTO_DA_TARIFA / AMOSTRAS_POR_CICLO_DE_MEDICAO; // NOVO CÁLCULO DE MÉDIA

			// Debug do Ponto Zero (Offset Bruto)
			PONTO_ZERO_TENSAO_BRUTO = (uint16_t)(SOMA_DO_VALOR_BRUTO_DA_TENSAO / AMOSTRAS_POR_CICLO_DE_MEDICAO);
			PONTO_ZERO_CORRENTE_BRUTO = (uint16_t)(SOMA_DO_VALOR_BRUTO_DA_CORRENTE / AMOSTRAS_POR_CICLO_DE_MEDICAO);

			// Potência Ativa (P)
			// P = Média da Potência Instantânea
			active_power = p_sum_of_products / AMOSTRAS_POR_CICLO_DE_MEDICAO;
			// Potência Aparente (S)
			// S = V_RMS * I_RMS
			apparent_power = voltage_rms * current_rms;
			// Fator de Potência (FP)
			// FP = P / S. Evita divisão por zero.
			if (apparent_power > 0.001f) {
				// Usa fabsf() pois o FP é geralmente um valor positivo
				power_factor = fabsf(active_power / apparent_power);
			} else {
				power_factor = 0.0f;
			}
			// Potência Reativa (Q)
			// Q = sqrt(S^2 - P^2)
			// Usa fabsf no resultado da subtração para evitar erros de ponto flutuante
			// que poderiam resultar em raiz quadrada de um número negativo.
			float S_squared = apparent_power * apparent_power;
			float P_squared = active_power * active_power;
			reactive_power = sqrtf(fabsf(S_squared - P_squared));

			// === CÁLCULO DE ENERGIA E CUSTO ===
			// 1. Apenas acumula energia se a potência for positiva (consumo)
			if (active_power > 0.0f)
			{
				// E (Watt-segundos/intervalo) = P (Watts) * TEMPO (s)
				float energy_watt_seconds = active_power * CALC_PERIOD_SECONDS;
				// Converte para kWh e acumula
				float energy_kwh_interval = energy_watt_seconds * FATOR_DE_CONVERSAO_PARA_KWH;
				energy_kwh += energy_kwh_interval;
				// 2. Custo Total = Energia Total (kWh) * Preço (R$/kWh)
				energy_cost = energy_kwh * tariff_value; // USANDO VARIÁVEL GLOBAL
			}
		}
	}
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
  MX_TIM4_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */
 // 1. Configura o handle do LCD
   lcd1.hi2c = &hi2c1;
   lcd1.address = 0x4E; // Endereço I2C do módulo PCF8574 (0x27 << 1 = 0x4E)
   // 2. Inicializa o LCD
   lcd_init(&lcd1);
   // 3. Tela de Bem Vindo Inicial (Redesenha apenas aqui)
   lcd_clear(&lcd1);
   lcd_gotoxy(&lcd1, 0, 0);
   lcd_puts(&lcd1, "  BEM VINDO!    ");
   lcd_gotoxy(&lcd1, 0, 1);
   lcd_puts(&lcd1, "   Gire o Pot   ");
 // Define o estado inicial do display para WELCOME (impede redesenho desnecessário no loop)
   lastDisplayedMenuText = "WELCOME";
 // 4. lastADCValue é inicializado para que a primeira variação seja detectada
   lastADCValue = 0;
 // 5. Inicia a aquisição contínua do ADC via DMA (Circular)
 // O tamanho é agora * 4 (Tensão, Corrente, Menu Pot, Tarifa Pot).
 if (HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_buffer, AMOSTRAS_POR_CICLO_DE_MEDICAO * 4) != HAL_OK)
 {
	  Error_Handler(); // Falha ao iniciar o DMA
 }
 // 6. Inicia o Timer de Amostragem (TIM2) - Dispara o ADC a 10kHz
 if (HAL_TIM_Base_Start(&htim2) != HAL_OK)
 {
	  Error_Handler(); // Falha ao iniciar o TIM2
 }
 // 7. Inicia o Timer de Janela de Cálculo (TIM4) com Interrupção (IT)
 // O TIM4 chamará HAL_TIM_PeriodElapsedCallback a cada ~1.0 segundo (configurado em MX_TIM4_Init).
 if (HAL_TIM_Base_Start_IT(&htim4) != HAL_OK)
 {
	  Error_Handler(); // Falha ao iniciar o TIM4
 }
 // 8. Inicia o PWM para o LED (PA6 / TIM3 CH1)
 if (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1) != HAL_OK)
 {
	  Error_Handler(); // Falha ao iniciar o PWM
 }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
 while (1)
 {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	    // --- LÓGICA DE CONTROLE DO PWM (LED PA6) ---
	    // O Period do TIM3 é 15999, então 15999 é 100% de duty cycle.
	    uint32_t pwm_duty = 0;
	    uint32_t current_time = HAL_GetTick();
	    if (voltage_rms > 120.0f)
	    {
	        // CONDIÇÃO 2: Tensão ALTA (> 120V) -> LED LIGADO CONSTANTE (100% Duty)
	        pwm_duty = htim3.Init.Period; // 15999
	    }
	    else if (voltage_rms < 20.0f)
	    {
	        // CONDIÇÃO 1: Tensão BAIXA (< 20V) -> LED PISCANDO (50% ON / 50% OFF)
	        // Verifica se o tempo atual está na metade "LIGADO" do período de BLINK (500ms ON / 500ms OFF)
	        if ((current_time % BLINK_PERIOD_MS) < (BLINK_PERIOD_MS / 2))
	        {
	            pwm_duty = htim3.Init.Period; // LIGA (100% Duty)
	        }
	        else
	        {
	            pwm_duty = 0; // DESLIGA (0% Duty)
	        }
	    }
	    else
	    {
	        // CONDIÇÃO 3: Tensão NORMAL (20V <= V_rms <= 120V) -> LED DESLIGADO CONSTANTE (0% Duty)
	        pwm_duty = 0;
	    }
	    // Aplica o Duty Cycle calculado ao Canal 1 do TIM3 (PA6)
	    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pwm_duty);

	    // --- LÓGICA DO LED DE CARGA (PA7, PA9, PA10, PA11) ---
	    if (current_rms > 0.2)
	        {
	          // LIGA o LED no pino PA7
	          HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET);
	        }
	        else
	        {
	          // DESLIGA o LED no pino PA7
	          HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);
	        }
	    if (current_rms > 0.4 && current_rms < 0.8)
	    	        {
	    	          // LIGA o LED no pino PA10
	    	          HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);
	    	          HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_RESET);
	    	          HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);
	    	        }
	    	        else
	    	        {
	    	          // DESLIGA o LED no pino PA10
	    	          HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET);
	    	        }
	    if (current_rms > 0.8 && current_rms < 1.2)
	    	        {
	    	          // LIGA o LED no pino PA9
	    	          HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);
	    	          HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_RESET);
	    	        }
	    	        else
	    	        {
	    	          // DESLIGA o LED no pino PA9
	    	          HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);
	    	        }
	    if (current_rms > 1.2)
	    	        {
	    	          // LIGA o LED no pino PA9
	    	          HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_SET);
	    	          HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET);
	    	          HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);
	    	        }
	    	        else
	    	        {
	    	          // DESLIGA o LED no pino PA9
	    	          HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_RESET);
	    	        }

	    //LÓGICA DE MENU DO LCD
	  	// currentADCValue recebe o valor médio do PA2 calculado no TIM4
	        currentADCValue = current_menu_adc; // Usa o valor médio do Potenciômetro
	        uint8_t buttonEvent = checkButtons(); // 1=B0 (SELECT), 2=B3 (RETURN), 3=B1 (CONFIG)
	        const char* currentMenuText = NULL;

            if (buttonEvent == 3) { // B1 pressionado
                if (currentState == TARIFF_CONFIG) {
                    // Já está em CONFIG, B1 é um comando de SALVAR e SAIR (Toggle)
                    tariff_value = new_tariff_value_for_display; // Salva o novo valor na variável global
                    lastDisplayedMenuText = "WELCOME";
                    currentState = WELCOME;

                    // Feedback: TARIFA SALVA
                    lcd_clear(&lcd1);
                    lcd_gotoxy(&lcd1, 0, 0);
                    lcd_puts(&lcd1, " TARIFA SALVA!  ");
                    HAL_Delay(1000);
                    // Redesenha a tela de Bem Vindo
                    lcd_clear(&lcd1);
                    lcd_gotoxy(&lcd1, 0, 0);
                    lcd_puts(&lcd1, "  BEM VINDO!    ");
                    lcd_gotoxy(&lcd1, 0, 1);
                    lcd_puts(&lcd1, "   Gire o Pot   ");

                } else {
                    // Não está em CONFIG, B1 é um comando de ENTRAR em CONFIG
                    currentState = TARIFF_CONFIG;
                    lastDisplayedMenuText = NULL; // Força o redesenho da tela de Tarifa
                    HAL_Delay(50);
                }
                buttonEvent = 0;
            }

	        switch (currentState) {
	            case WELCOME:
	                // 1. A tela de boas-vindas é mantida
	                // 2. Transição para SELECTION (se variação > 5%)
	                // Verifica a variação do potenciômetro (usando o valor médio estabilizado)

	                if (abs((int32_t)currentADCValue - (int32_t)lastADCValue) > (int32_t)ADC_THRESHOLD) {
	                    lastADCValue = currentADCValue;
	                    lastDisplayedMenuText = NULL; // Força o primeiro redesenho em SELECTION
	                    currentState = SELECTION;
	                }
	                break;

	            case SELECTION:
	                // 1. Determinação da String do Menu (Lógica de if/else if baseada no valor do ADC)
	                if (currentADCValue <= 409) {
	                    currentMenuText = "Tensao media";
	                } else if (currentADCValue <= 819) {
	                    currentMenuText = "Tensao RMS";
	                } else if (currentADCValue <= 1228) {
	                    currentMenuText = "Corrente RMS";
	                } else if (currentADCValue <= 1638) {
	                    currentMenuText = "Corrente media";
	                } else if (currentADCValue <= 2047) {
	                    currentMenuText = "Potencia ativa";
	                } else if (currentADCValue <= 2457) {
	                    currentMenuText = "Potencia Reativa";
	                } else if (currentADCValue <= 2867) {
	                    currentMenuText = "Potencia aparente";
	                } else if (currentADCValue <= 3276) {
	                    currentMenuText = "FP";
	                } else if (currentADCValue <= 3685) {
	                    currentMenuText = "Consumo";
	                } else {
	                    currentMenuText = "Gasto em Energia";
	                }
	                // 2. **ANTI-FLICKER:** Só redesenha se o texto for diferente do anterior
	                if (currentMenuText != lastDisplayedMenuText) {
	                    lcd_clear(&lcd1);
	                    lcd_gotoxy(&lcd1, 0, 0);
	                    lcd_puts(&lcd1, ">> SELECIONE: <<");
	                    lcd_gotoxy(&lcd1, 0, 1);
	                    // Limpa a linha 1 antes de exibir, para garantir que não sobra lixo
	                    lcd_puts(&lcd1, "                "); // 16 espaços em branco
	                    lcd_gotoxy(&lcd1, 0, 1);
	                    lcd_puts(&lcd1, (char*)currentMenuText);
	                    lastDisplayedMenuText = currentMenuText;
	                }
	                // 3. Transição para LOCKED (se B0 for pressionado)
	                if (buttonEvent == 1) { // Evento B0 (Select)
	                    selectedMenuText = currentMenuText; // Salva a string
	                    lastDisplayedMenuText = NULL; // Força o redesenho único em LOCKED (na próxima iteração)
	                    currentState = LOCKED;
	                }
	                // 4. Transição para WELCOME (se B3 for pressionado)
	                if (buttonEvent == 2) { // Evento B3 (Return)
	                    lastDisplayedMenuText = "WELCOME"; // Marca o texto de WELCOME como exibido
	                    currentState = WELCOME;
	                    // Redesenha a tela de Bem Vindo APENAS UMA VEZ ao voltar do menu
	                    lcd_clear(&lcd1);
	                    lcd_gotoxy(&lcd1, 0, 0);
	                    lcd_puts(&lcd1, "  BEM VINDO!    ");
	                    lcd_gotoxy(&lcd1, 0, 1);
	                    lcd_puts(&lcd1, "   Gire o Pot   ");
	                }
	                // Atualiza lastADCValue para a próxima checagem de 5%
	                lastADCValue = currentADCValue;
	                break;

	            case LOCKED:
	                // 1. ANTI-FLICKER: Desenha o cabeçalho (opção fixada) APENAS uma vez
	                if (lastDisplayedMenuText == NULL) {
	                    lcd_clear(&lcd1);
	                    lcd_gotoxy(&lcd1, 0, 0);
	                    // O selectedMenuText agora vai para a linha 0 como título
	                    lcd_puts(&lcd1, (char*)selectedMenuText);
	                    lastDisplayedMenuText = selectedMenuText; // Marca o título como desenhado
	                }
	                // 2. Formata e exibe o valor da variável (REPETITIVO)
	                // Uso de int e float-to-int para simular %f em snprintf lite.
	                if (strstr(selectedMenuText, "Tensao media") != NULL) {
	                    // %.1f Vavg
	                    int whole = (int)voltage_avg;
	                    int fractional = (int)(fabsf(voltage_avg) * 10.0f) % 10;
	                    snprintf(lcd_line_buffer, sizeof(lcd_line_buffer), "%d.%d V", whole, fractional);
	                } else if (strstr(selectedMenuText, "Tensao RMS") != NULL) {
	                    // %.1f VRMS
	                    int whole = (int)voltage_rms;
	                    int fractional = (int)(fabsf(voltage_rms) * 10.0f) % 10;
	                    snprintf(lcd_line_buffer, sizeof(lcd_line_buffer), "%d.%d VRMS", whole, fractional);
	                } else if (strstr(selectedMenuText, "Corrente RMS") != NULL) {
	                    // %.2f ARMS
	                    int whole = (int)current_rms;
	                    int fractional = (int)(fabsf(current_rms) * 100.0f) % 100;
	                    snprintf(lcd_line_buffer, sizeof(lcd_line_buffer), "%d.%02d ARMS", whole, fractional);
	                } else if (strstr(selectedMenuText, "Corrente media") != NULL) {
	                    // %.2f Aavg
	                    int whole = (int)current_avg;
	                    int fractional = (int)(fabsf(current_avg) * 100.0f) % 100;
	                    snprintf(lcd_line_buffer, sizeof(lcd_line_buffer), "%d.%02d A", whole, fractional);
	                } else if (strstr(selectedMenuText, "Potencia ativa") != NULL) {
	                    // %.1f W (Mantém o sinal no whole se o valor for negativo)
	                    int whole = (int)active_power;
	                    int fractional = (int)(fabsf(active_power) * 10.0f) % 10;
	                    // Ajuste para garantir que o sinal de negativo apareça corretamente se a parte inteira for negativa
	                    if (whole < 0) {
	                        snprintf(lcd_line_buffer, sizeof(lcd_line_buffer), "%d.%d W", whole, fractional);
	                    } else {
	                        snprintf(lcd_line_buffer, sizeof(lcd_line_buffer), "%d.%d W", whole, fractional);
	                    }
	                } else if (strstr(selectedMenuText, "Potencia Reativa") != NULL) {
	                    // %.1f VAR
	                    int whole = (int)reactive_power;
	                    int fractional = (int)(fabsf(reactive_power) * 10.0f) % 10;
	                    snprintf(lcd_line_buffer, sizeof(lcd_line_buffer), "%d.%d VAR", whole, fractional);
	                } else if (strstr(selectedMenuText, "Potencia aparente") != NULL) {
	                    // %.1f VA
	                    int whole = (int)apparent_power;
	                    int fractional = (int)(fabsf(apparent_power) * 10.0f) % 10;
	                    snprintf(lcd_line_buffer, sizeof(lcd_line_buffer), "%d.%d VA", whole, fractional);
	                } else if (strstr(selectedMenuText, "FP") != NULL) {
	                    // FP: 0.XX
	                    int decimal = (int)(power_factor * 100.0f);
	                    snprintf(lcd_line_buffer, sizeof(lcd_line_buffer), "FP: 0.%02d", decimal);
	                } else if (strstr(selectedMenuText, "Consumo") != NULL) {
	                    // %.3f kWh
	                    int whole = (int)energy_kwh;
	                    int fractional = (int)(fabsf(energy_kwh) * 1000.0f) % 1000;
	                    snprintf(lcd_line_buffer, sizeof(lcd_line_buffer), "%d.%03d kWh", whole, fractional);
	                } else if (strstr(selectedMenuText, "Gasto em Energia") != NULL) {
	                    // R$ %.2f
	                    int whole = (int)energy_cost;
	                    int fractional = (int)(fabsf(energy_cost) * 100.0f) % 100;
	                    snprintf(lcd_line_buffer, sizeof(lcd_line_buffer), "R$ %d.%02d", whole, fractional);
	                } else {
	                    // Fallback se a string não for reconhecida
	                    snprintf(lcd_line_buffer, sizeof(lcd_line_buffer), "ERRO MENU");
	                }
	                // Limpa a linha 1 antes de colocar o novo valor (para apagar o que sobrou)
	                lcd_gotoxy(&lcd1, 0, 1);
	                lcd_puts(&lcd1, "                "); // 16 espaços em branco
	                lcd_gotoxy(&lcd1, 0, 1);
	                // Exibe o valor do buffer na linha 1 (índice 1)
	                lcd_puts(&lcd1, lcd_line_buffer);

	                // 3. Transição para SELECTION (se B3 for pressionado)
	                if (buttonEvent == 2) { // Evento B3 (Return)
	                    lastDisplayedMenuText = NULL; // Força o redesenho em SELECTION (na próxima iteração)
	                    currentState = SELECTION;
	                }
	                break;

            case TARIFF_CONFIG:
              // --- TELA DE CONFIGURAÇÃO DE TARIFA ---

              // 1. Mapeamento do Potenciômetro (PA3)
              // PA3 (0 a 4095) mapeia para 0.00 a TARIFF_MAX_VALUE (1.50 R$/kWh)
              new_tariff_value_for_display = (float)current_tarifa_adc * TARIFF_MAX_VALUE / 4095;

              // 2. **ANTI-FLICKER:** Desenha o cabeçalho APENAS uma vez
              if (lastDisplayedMenuText == NULL) {
                  lcd_clear(&lcd1);
                  lcd_gotoxy(&lcd1, 0, 0);
                  lcd_puts(&lcd1, "    TARIFA      ");
                  lastDisplayedMenuText = "TARIFF_CONFIG";
              }

              // 3. Formata e exibe o valor da variável
              // Isso permite que o valor se altere na tela em tempo real com o potenciômetro
              int whole_tar = (int)new_tariff_value_for_display;
              int fractional_tar = (int)(fabsf(new_tariff_value_for_display) * 1000.0f) % 1000;

              lcd_gotoxy(&lcd1, 0, 1);
              lcd_puts(&lcd1, "                "); // Limpa a linha
              lcd_gotoxy(&lcd1, 0, 1);
              // Exibe R$ X.XXX/kWh (3 casas decimais)
              snprintf(lcd_line_buffer, sizeof(lcd_line_buffer), "R$ %d.%03d/kWh", whole_tar, fractional_tar);
              lcd_puts(&lcd1, lcd_line_buffer);

              // 4. Transição de SALVAR/SAIR com B0 (SELECT)
              if (buttonEvent == 1) { // Evento B0 (SELECT) pressionado: SALVA E SAI
                  tariff_value = new_tariff_value_for_display; // Salva o novo valor na variável global

                  // Retorna para a tela inicial WELCOME
                  lastDisplayedMenuText = "WELCOME";
                  currentState = WELCOME;

                  // Feedback: TARIFA SALVA
                  lcd_clear(&lcd1);
                  lcd_gotoxy(&lcd1, 0, 0);
                  lcd_puts(&lcd1, " TARIFA SALVA!  ");
                  HAL_Delay(1000);
                  // Redesenha a tela de Bem Vindo
                  lcd_clear(&lcd1);
                  lcd_gotoxy(&lcd1, 0, 0);
                  lcd_puts(&lcd1, "  BEM VINDO!    ");
                  lcd_gotoxy(&lcd1, 0, 1);
                  lcd_puts(&lcd1, "   Gire o Pot   ");

              } else if (buttonEvent == 2) { // Evento B3 (RETURN) pressionado: SAI SEM SALVAR
                  lastDisplayedMenuText = "WELCOME";
                  currentState = WELCOME;

                  // Redesenha a tela de Bem Vindo
                  lcd_clear(&lcd1);
                  lcd_gotoxy(&lcd1, 0, 0);
                  lcd_puts(&lcd1, "  BEM VINDO!    ");
                  lcd_gotoxy(&lcd1, 0, 1);
                  lcd_puts(&lcd1, "   Gire o Pot   ");
              }
              break;


	            default: // Caso de fallback
	                currentState = WELCOME;
	                break;
	        }
	    HAL_Delay(50); // Atraso para botão
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
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
  hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T2_TRGO;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 4;
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
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
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

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_3;
  sConfig.Rank = 4;
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
  htim2.Init.Period = 1599;
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
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_ENABLE;
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
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 15999;
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
  htim4.Init.Prescaler = 999;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 15999;
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
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
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
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11, GPIO_PIN_RESET);

  /*Configure GPIO pins : PA7 PA9 PA10 PA11 */
  GPIO_InitStruct.Pin = GPIO_PIN_7|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB1 PB3 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PB2 */
  GPIO_InitStruct.Pin = GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

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
	  // Exibe ERRO no LCD se necessário
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
