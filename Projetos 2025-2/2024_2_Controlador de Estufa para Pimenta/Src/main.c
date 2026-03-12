#include "main.h"
#include "stdbool.h"
#include <stdio.h>  // Para printf
#include <math.h>   // Para funções matemáticas (exp, log)
#include "stm32f4xx_hal_tim.h"

// --- Definições dos Pinos ---
// LEDs
#define LED1_PIN GPIO_PIN_1
#define LED2_PIN GPIO_PIN_14
#define LED3_PIN GPIO_PIN_15
#define LED4_PIN GPIO_PIN_4
#define LED5_PIN GPIO_PIN_5
#define LED1_PORT GPIOA
#define LED2_PORT GPIOB  // B14 e B15 no GPIOB
#define LED3_PORT GPIOB
#define LED4_PORT GPIOA
#define LED5_PORT GPIOA

// Botões
#define SELECT_BUTTON_PIN GPIO_PIN_0  // PA0
#define SELECT_BUTTON_PORT GPIOA
#define ENTER_BUTTON_PIN GPIO_PIN_1   // PB1 (não usado)
#define ENTER_BUTTON_PORT GPIOB        // (não usado)
#define EMERGENCY_BUTTON_PIN GPIO_PIN_2 // PB2
#define EMERGENCY_BUTTON_PORT GPIOB

// Atuadores (PWM para Ventoinha)
#define FAN_PWM_PIN     GPIO_PIN_7  // PB7 - Usaremos PWM para a ventoinha
#define HEATER_PIN      GPIO_PIN_6  // PB6
#define ACTUATOR_PORT   GPIOB

// --- Constantes ---
#define DEBOUNCE_TIME_MS 50
#define BLINK_PERIOD_MS 500
#define ADC_MAX_VALUE 4095 // Valor máximo do ADC (12 bits)
#define EMERGENCY_HOLD_TIME_MS 5000
#define ADC_TIMEOUT_MS 10
#define PWM_FREQUENCY_HZ 25000 // Frequência PWM para a ventoinha (exemplo: 25kHz)

// --- Máquina de Estados (LEDs) ---
typedef enum {
    STATE_LED1, STATE_LED2, STATE_LED3, STATE_LED4, STATE_LED5,
} LedState_t;

// --- Estrutura para Configurações de Plantas ---
typedef struct {
    uint16_t temp_min;
    uint16_t temp_max;
    uint16_t humidity_air_min;
    uint16_t humidity_soil_min;
} PlantConfig_t;

// --- Variáveis Globais ---
volatile LedState_t current_state = STATE_LED1;
volatile bool select_button_pressed = false;
volatile uint32_t select_button_press_time = 0;
volatile uint32_t last_blink_time = 0;
volatile bool led_state = false;
volatile bool emergency_button_pressed = false; // Botão de emergência
volatile uint32_t emergency_button_press_time = 0;
volatile bool emergency_active = false; // Flag para indicar se a emergência está ativa
volatile LedState_t previous_led_state; // Armazena o estado do LED antes da emergência
volatile bool enter_button_pressed = false; // Botão enter (não utilizado)
volatile uint32_t enter_button_press_time = 0;// Botão enter (não utilizado)
volatile uint16_t temperature_value = 0;      // Valor bruto do ADC (temperatura)
volatile uint16_t air_humidity_value = 0;     // Valor bruto do ADC (umidade do ar)
volatile uint16_t soil_humidity_value = 0;    // Valor bruto do ADC (umidade do solo)

PlantConfig_t plant_configs[5] = {
    {1024, 2048, 2048, 1024},  // Planta 1 (LED1) - Ajuste!
    {1536, 2560, 1536, 1536},  // Planta 2 (LED2) - Ajuste!
    {512,  1536, 2560, 2048},  // Planta 3 (LED3) - Ajuste!
    {2048, 3072, 1024, 512},   // Planta 4 (LED4) - Ajuste!
    {204,  1024, 1024, 2048}   // Planta 5 (LED5) - Ajuste!
};

ADC_HandleTypeDef hadc1;
UART_HandleTypeDef huart2; // Estrutura para a UART2 (HAL)
TIM_HandleTypeDef htim4;    // Estrutura para o Timer 4 (PWM)

// --- Protótipos das Funções ---
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
void MX_USART2_UART_Init(void); // Protótipo da função UART (HAL)
static void MX_TIM4_Init(void);  // Protótipo para inicializar o Timer 4 (PWM)
void Select_Button_Init(void);
void Enter_Button_Init(void);  // (não usada)
void Emergency_Button_Init(void);
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin);
void execute_menu_action(void); // (não usada)
void blink_led(void);
void read_sensors(void);
void control_actuators(PlantConfig_t config);
void handle_emergency(void);
void set_fan_pwm(uint32_t pulse); // Função para controlar o PWM da ventoinha

int main(void)  
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_ADC1_Init();
    MX_USART2_UART_Init(); // Inicializa a UART (HAL)
    MX_TIM4_Init();         // Inicializa o Timer 4 para PWM
    Select_Button_Init();
    Enter_Button_Init();
    Emergency_Button_Init();
    HAL_ADC_Start(&hadc1);

    while (1)
    {
        // --- Lógica do Botão de Emergência ---
        if (emergency_button_pressed)
        {
          if(HAL_GPIO_ReadPin(EMERGENCY_BUTTON_PORT, EMERGENCY_BUTTON_PIN) == GPIO_PIN_RESET) //Botão está pressionado
          {
            if (!emergency_active && (HAL_GetTick() - emergency_button_press_time) > DEBOUNCE_TIME_MS) //Ativa emergência
            {
                  emergency_active = true; //Ativa flag
                  previous_led_state = current_state; // Salva o estado atual do LED
                  emergency_button_pressed = false; //Reseta flag

            }
            else if(emergency_active && (HAL_GetTick() - emergency_button_press_time) > EMERGENCY_HOLD_TIME_MS) //Desativa
            {
              emergency_active = false;  //Desativa
              current_state = previous_led_state; // Restaura o estado do LED
              emergency_button_pressed = false; //Reseta
            }

          }
          else{
              emergency_button_press_time = HAL_GetTick(); //Continua contando tempo
          }
        }


        if (emergency_active) {
            handle_emergency();
            continue; // Volta para o início do loop while(1)
        }

        // --- Lógica do Botão de Seleção (A0) ---
        if (select_button_pressed && (HAL_GetTick() - select_button_press_time) > DEBOUNCE_TIME_MS) {
            select_button_pressed = false;
            current_state = (current_state + 1) % (STATE_LED5 + 1);
            printf("Novo estado: %d\n", current_state);  // Depuração: Imprime o estado atual
            led_state = true;
            last_blink_time = HAL_GetTick();
        }

        blink_led();       // Pisca o LED do estado atual
        read_sensors();    // Lê os sensores
        control_actuators(plant_configs[current_state]); // Controla os atuadores

        // --- EQUAÇÕES (Descomentadas e com valores de exemplo) ---
        // *IMPORTANTE*: Ajuste os coeficientes com base na calibração dos seus sensores!

        // --- Conversão para Graus Celsius (Exemplo Linear) ---
        float m_temp = 0.0732;
        float b_temp = -50.0;
        volatile float temp_celsius = m_temp * temperature_value + b_temp; // **ADD volatile HERE**
        printf("Temperatura (ADC): %u, Temperatura (Celsius): %.2f\n", temperature_value, temp_celsius);

        // --- Conversão para Porcentagem de Umidade do Solo (Exemplo Linear) ---
        float m_humidity = 0.0244; // Inclinação (% de umidade por unidade do ADC) - AJUSTE!
        float b_humidity = -10.0;  // Offset (% de umidade) - AJUSTE!
        float humidity_percent = m_humidity * soil_humidity_value + b_humidity;
        printf("Umidade do Solo (ADC): %u, Umidade do Solo (%%): %.2f\n", soil_humidity_value, humidity_percent);

		// --- Conversão para Porcentagem de Umidade do Ar (Exemplo Linear) ---
        float m_air_humidity = 0.0244; // Inclinação (% de umidade por unidade do ADC) - AJUSTE!
        float b_air_humidity = -10.0;  // Offset (% de umidade) - AJUSTE!
        float air_humidity_percent = m_air_humidity * air_humidity_value + b_air_humidity;
        printf("Umidade do Ar (ADC): %u, Umidade do Ar (%%): %.2f\n", air_humidity_value, air_humidity_percent);

        // --- Cálculo do Déficit de Pressão de Vapor (DPV) ---
        // 1. Calcular a pressão de vapor saturado (es) em kPa (equação de Tetens):
        float es = 0.6108 * expf((17.27 * temp_celsius) / (temp_celsius + 237.3)); // Use expf para float

        // 2. Calcular a pressão de vapor real (ea) em kPa:
        float ea = (air_humidity_percent / 100.0) * es;

        // 3. Calcular o DPV:
        float dpv = es - ea;
        printf("DPV: %.2f kPa\n", dpv);

        // --- Cálculo do Ponto de Orvalho (Td) ---
        // Equação de Magnus-Tetens (aproximada):
        float gamma = (17.27 * temp_celsius) / (237.3 + temp_celsius) + logf(air_humidity_percent / 100.0); // Use logf para float
        float td = (237.3 * gamma) / (17.27 - gamma);
        printf("Ponto de Orvalho: %.2f C\n", td);

        // --- Índice de Conforto Térmico (Humidex) ---
        // Equação simplificada (pode não ser precisa em todas as condições):
        float humidex = temp_celsius + (5.0/9.0) * (ea * 10 - 10); // ea em kPa, ajuste se necessário
        printf("Humidex: %.2f\n", humidex);
    }
}

// --- Funções de Inicialização (Botões) ---
void Select_Button_Init(void) {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = SELECT_BUTTON_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(SELECT_BUTTON_PORT, &GPIO_InitStruct);
    HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI0_IRQn);
}

void Enter_Button_Init(void) {  // Não usada
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = ENTER_BUTTON_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(ENTER_BUTTON_PORT, &GPIO_InitStruct);
    HAL_NVIC_SetPriority(EXTI1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI1_IRQn);
}

void Emergency_Button_Init(void) {
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = EMERGENCY_BUTTON_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(EMERGENCY_BUTTON_PORT, &GPIO_InitStruct);
    HAL_NVIC_SetPriority(EXTI2_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI2_IRQn);
}

// --- Função de Callback das Interrupções ---
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == SELECT_BUTTON_PIN) {
        if (HAL_GPIO_ReadPin(SELECT_BUTTON_PORT, SELECT_BUTTON_PIN) == GPIO_PIN_SET) {
            select_button_pressed = true;
            select_button_press_time = HAL_GetTick();
            printf("Botao SELECT pressionado!\n");
        }
    }
     else if (GPIO_Pin == ENTER_BUTTON_PIN) { //Não usado
        if (HAL_GPIO_ReadPin(ENTER_BUTTON_PORT, ENTER_BUTTON_PIN) == GPIO_PIN_SET) {
            enter_button_pressed = true;
            enter_button_press_time = HAL_GetTick();
        }
    }
    else if (GPIO_Pin == EMERGENCY_BUTTON_PIN) {
      emergency_button_pressed = true;
      emergency_button_press_time = HAL_GetTick();
    }
}

// --- Outras Funções ---

void blink_led(void)
{
    if ((HAL_GetTick() - last_blink_time) > BLINK_PERIOD_MS) {
        led_state = !led_state;
        last_blink_time = HAL_GetTick();

        // Desliga todos os LEDs antes de ligar o correto
        HAL_GPIO_WritePin(LED1_PORT, LED1_PIN , GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED2_PORT, LED2_PIN , GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED3_PORT, LED3_PIN , GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED4_PORT, LED4_PIN , GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED5_PORT, LED5_PIN , GPIO_PIN_RESET);

        switch (current_state) {
            case STATE_LED1:
                HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, led_state ? GPIO_PIN_SET : GPIO_PIN_RESET);
                break;
            case STATE_LED2:
                HAL_GPIO_WritePin(LED2_PORT, LED2_PIN, led_state ? GPIO_PIN_SET : GPIO_PIN_RESET);
                break;
            case STATE_LED3:
                HAL_GPIO_WritePin(LED3_PORT, LED3_PIN, led_state ? GPIO_PIN_SET : GPIO_PIN_RESET);
                break;
            case STATE_LED4:
                HAL_GPIO_WritePin(LED4_PORT, LED4_PIN, led_state ? GPIO_PIN_SET : GPIO_PIN_RESET);
                break;
            case STATE_LED5:
                HAL_GPIO_WritePin(LED5_PORT, LED5_PIN, led_state ? GPIO_PIN_SET : GPIO_PIN_RESET);
                break;
        }
    }
}

void read_sensors(void)
{
    HAL_ADC_Start(&hadc1);

    HAL_ADC_PollForConversion(&hadc1, ADC_TIMEOUT_MS);
    temperature_value = HAL_ADC_GetValue(&hadc1);

    HAL_ADC_PollForConversion(&hadc1, ADC_TIMEOUT_MS);
    air_humidity_value = HAL_ADC_GetValue(&hadc1);

    HAL_ADC_PollForConversion(&hadc1, ADC_TIMEOUT_MS);
    soil_humidity_value = HAL_ADC_GetValue(&hadc1);
}

void control_actuators(PlantConfig_t config)
{
    float fan_speed_percentage = 0;

   if (temperature_value > config.temp_max) {
        // Ventoinha PWM controlada pela temperatura acima do máximo
        fan_speed_percentage = (float)(temperature_value - config.temp_max) / (ADC_MAX_VALUE - config.temp_max); // Exemplo: Ajuste conforme necessário
        if (fan_speed_percentage > 1.0f) fan_speed_percentage = 1.0f; // Limita a 100%
        if (fan_speed_percentage < 0.0f) fan_speed_percentage = 0.0f; // Limita a 0%
        set_fan_pwm((uint32_t)(fan_speed_percentage * __HAL_TIM_GET_AUTORELOAD(&htim4))); // Ajusta o PWM da ventoinha
        HAL_GPIO_WritePin(ACTUATOR_PORT, HEATER_PIN, GPIO_PIN_RESET); // Desliga aquecedor
    } else if (temperature_value < config.temp_min) {
        set_fan_pwm(0); // Desliga ventoinha (PWM 0%)
        HAL_GPIO_WritePin(ACTUATOR_PORT, HEATER_PIN, GPIO_PIN_SET); // Liga aquecedor
    } else {
        set_fan_pwm(0); // Desliga ventoinha (PWM 0%)
        HAL_GPIO_WritePin(ACTUATOR_PORT, HEATER_PIN, GPIO_PIN_RESET); // Desliga aquecedor
    }

    if (air_humidity_value < config.humidity_air_min && soil_humidity_value < config.humidity_soil_min) {
        // Como não temos mais bomba PWM, podemos manter como liga/desliga no HEATER_PIN ou remover essa lógica se não for mais necessária.
        // Neste exemplo, vamos REMOVER a lógica da bomba, pois o foco é PWM na ventoinha.
        // Se você quiser usar o HEATER_PIN para outra coisa, ajuste aqui.
        // HAL_GPIO_WritePin(ACTUATOR_PORT, HEATER_PIN, GPIO_PIN_SET); // Exemplo: Ligar aquecedor se ambas umidades baixas
    } else {
        // HAL_GPIO_WritePin(ACTUATOR_PORT, HEATER_PIN, GPIO_PIN_RESET); // Desligar aquecedor se umidades ok
    }
}

void execute_menu_action(void) { }  // Função vazia (não usada)

void handle_emergency(void)
{
    // Desliga todos os atuadores
    set_fan_pwm(0); // Desliga ventoinha PWM
    HAL_GPIO_WritePin(ACTUATOR_PORT, HEATER_PIN, GPIO_PIN_RESET); // Desliga aquecedor

    // Pisca todos os LEDs
    if ((HAL_GetTick() - last_blink_time) > BLINK_PERIOD_MS) {
        led_state = !led_state;
        last_blink_time = HAL_GetTick();
        HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, led_state ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED2_PORT, LED2_PIN, led_state ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED3_PORT, LED3_PIN, led_state ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED4_PORT, LED4_PIN, led_state ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED5_PORT, LED5_PIN, led_state ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
}

// --- Inicialização do Timer 4 para PWM ---
static void MX_TIM4_Init(void)
{
    TIM_ClockConfigTypeDef sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    TIM_OC_InitTypeDef sConfigOC = {0};

    htim4.Instance = TIM4;
    htim4.Init.Prescaler = 100; // Ajuste o prescaler para obter a frequência PWM desejada
    htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim4.Init.Period = (SystemCoreClock / (htim4.Init.Prescaler + 1) / PWM_FREQUENCY_HZ) - 1; // Calcula o Period para a frequência desejada
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
    sConfigOC.Pulse = 0; // Inicialmente desligado (duty cycle 0%)
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_2) != HAL_OK) // Canal 2 do TIM4 (verifique o datasheet para PB7)
    {
      Error_Handler();
    }
}

// --- Função para ajustar o PWM da ventoinha ---
void set_fan_pwm(uint32_t pulse) {
    if (pulse > __HAL_TIM_GET_AUTORELOAD(&htim4)) {
        pulse = __HAL_TIM_GET_AUTORELOAD(&htim4); // Limita ao período máximo
    }
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_2, pulse); // Canal 2 do TIM4
}


void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /** Configure the main internal regulator output voltage
    */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2); // Ou SCALE1, dependendo da frequência

    /** Initializes the RCC Oscillators according to the specified parameters
    * in the RCC_OscInitTypeDef structure.
    */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = 16; // HSI é 16MHz
    RCC_OscInitStruct.PLL.PLLN = 192; //  96MHz
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2; // 96MHz
    RCC_OscInitStruct.PLL.PLLQ = 4; //
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

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK) // Ajuste FLASH_LATENCY para sua frequência!
    {
      Error_Handler();
    }
}

// --- Inicialização dos GPIOs (LEDs e Atuadores) ---
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE(); // Se você tiver LEDs no GPIOC (ex: PC13)
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_TIM4_CLK_ENABLE(); // Habilita o clock do TIM4

    //Antes de configurar, desliga tudo.
    HAL_GPIO_WritePin(GPIOA, LED1_PIN | LED4_PIN | LED5_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, LED2_PIN | LED3_PIN | HEATER_PIN, GPIO_PIN_RESET); // Desliga atuadores no início
    HAL_GPIO_WritePin(ACTUATOR_PORT, FAN_PWM_PIN, GPIO_PIN_RESET); // Garante que a ventoinha PWM inicia desligada

    // Configuração dos pinos dos LEDs (A1, B14, B15, A4, A5) como saídas
    GPIO_InitStruct.Pin = LED1_PIN; //A1
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct); //Configura A1

    GPIO_InitStruct.Pin = LED2_PIN; //B14
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct); //Configura B14

    GPIO_InitStruct.Pin = LED3_PIN; //B15
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct); //Configura B15

    GPIO_InitStruct.Pin = LED4_PIN; //A4
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct); //Configura A4

    GPIO_InitStruct.Pin = LED5_PIN; //A5
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct); //Configura A5

    // Configuração do pino do aquecedor (PB6) como saída
    GPIO_InitStruct.Pin = HEATER_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct); // Usando GPIOB para o aquecedor

    // Configuração do pino PWM da ventoinha (PB7) como saída Alternate Function para TIM4_CH2 (ou o canal correto)
    GPIO_InitStruct.Pin = FAN_PWM_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP; // Alternate Function Push Pull
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM4; // AF2 para TIM4_CH2 no PB7 (verifique o datasheet do seu STM32)
    HAL_GPIO_Init(ACTUATOR_PORT, &GPIO_InitStruct);
}

// --- Inicialização do ADC ---
static void MX_ADC1_Init(void) {
    ADC_ChannelConfTypeDef sConfig = {0};

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
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) {
        Error_Handler();
    }
	//Temp
    sConfig.Channel = ADC_CHANNEL_6;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        Error_Handler();
    }
	//Ar
    sConfig.Channel = ADC_CHANNEL_7;
    sConfig.Rank = 2;
     if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        Error_Handler();
    }
	//Solo
    sConfig.Channel = ADC_CHANNEL_8;
    sConfig.Rank = 3;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        Error_Handler();
    }
}
// --- Inicialização da UART (USART2) - Usando HAL ---
void MX_USART2_UART_Init(void)
{

//Nesse caso, PA2 = Tx e PA3 = Rx
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /**USART2 GPIO Configuration
    PA2     ------> USART2_TX
    PA3     ------> USART2_RX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK)
    {
    Error_Handler();
    }
}

// Função para enviar um caractere pela UART (para printf)
int __io_putchar(int ch) {
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}

void Error_Handler(void) {
    __disable_irq();
    while (1) { }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t* file, uint32_t line) {}
#endif