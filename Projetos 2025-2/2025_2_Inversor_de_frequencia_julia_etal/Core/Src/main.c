/* USER CODE BEGIN Header */
/**
  * @file           : main.c
  * @brief          : Main program body (integrated project)
  ******************************************************************************
  */
#include <stdio.h>
#include <math.h>
#include "main.h"

/* --- Definições do LCD I2C --- */
#define LCD_ADDR (0x27 << 1)

/* Comandos LCD */
#define LCD_CLEARDISPLAY 0x01
#define LCD_RETURNHOME 0x02
#define LCD_ENTRYMODESET 0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_FUNCTIONSET 0x20
#define LCD_SETDDRAMADDR 0x80

/* Bits de Controle LCD */
#define EN 0b00000100
#define RW 0b00000010
#define RS 0b00000001
#define BL 0b00001000

/* Botões do Menu (PA10-PA12) */
#define BTN_UP_PIN GPIO_PIN_10
#define BTN_DOWN_PIN GPIO_PIN_11
#define BTN_SELECT_PIN GPIO_PIN_12

/* --- Variáveis Globais --- */
I2C_HandleTypeDef hi2c1;
ADC_HandleTypeDef hadc1;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim2;

#define ADC_MAX_VALUE 4095.0f
#define FILTER_SIZE 10 // Modelo 5

// Variáveis filtradas
volatile uint32_t adc_values_raw[2];
volatile uint32_t adc_values_filtered[2];

// Variáveis Modelo 5
uint32_t adc_buffer_A[FILTER_SIZE];
uint32_t adc_buffer_B[FILTER_SIZE];
uint8_t buffer_index = 0;

// Variáveis para Filtro Digital - PA2
float adc_rc_filtered = 0.0f;
const float alpha_rc = 0.1f;

uint8_t motorA_on = 0;
uint8_t motorB_on = 0;
uint8_t motorA_dir = 0; // 0: H, 1: AH
uint8_t motorB_dir = 0;

/* Menu control */
uint8_t menu_state = 0;
uint8_t current_selection = 0;

/* Debounce */
uint32_t lastButtonTime[4] = {0,0,0,0};
uint32_t lastMenuButtonTime[3] = {0,0,0};
const uint32_t debounceDelay = 50;

uint32_t lastDirectionButtonTime[2] = {0, 0};

/* Variável para Debounce de Atualização do Display */
uint32_t lastDisplayUpdateTime = 0;
const uint32_t displayRefreshRate = 50;

/*Duty usado para frenagem suave */
uint32_t duty_cycle_atual = 0;
uint16_t torque_relativo = 0;

/* --- MAQUINA DE ESTADOS (MODELO 12) --- */
typedef enum {
    STATE_INIT = 0,
    STATE_READ_A = 1,
    STATE_READ_B = 2,
    STATE_FILT_A = 3,
    STATE_FILT_B = 4,
    STATE_DIAGN_A = 5,
    STATE_DIAGN_B = 6
} StateMachine_TypeDef;

// Variável Global para o Estado Atual
volatile StateMachine_TypeDef current_state = STATE_INIT;

/* --- Protótipos --- */
void SystemClock_Config(void);
void Error_Handler(void);

void MX_I2C1_Init(void);
void MX_GPIO_Init(void);
void MX_ADC1_Init(void);
void MX_TIM3_PWM_Init(void);
void MX_TIM2_Init(void);

void lcd_init(void);
void lcd_send_cmd(char cmd);
void lcd_send_data(char data);
void lcd_send_byte(char data, int type);
void lcd_send_string(char *str);
void lcd_put_cur(uint8_t row, uint8_t col);
void lcd_print_menu(uint8_t sel);
void lcd_handle_input(void);

void loop_delay(uint32_t ms);
void wait_for_button_release(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);

void Read_ADC_Polling(void);
void Read_Motor_Buttons(void);
void MotorA_Update(void);
void MotorB_Update(void);

void Apply_Freagem(void);
void Frenagem_impl(void);
void Sequencia_LEDs(void);
void Rotina_Teste_Direcao(void);

void Controlar_Sentido_Motores(uint16_t btnA_pin, uint16_t btnB_pin, int case_id);

void Display_Motor_Direction(void);

// interrupção
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin);

/* --- Modelos Matemáticos --- */

// MODELO 5: Filtro de Média Móvel
uint32_t apply_moving_average(uint32_t new_val, uint32_t *buffer, uint8_t size, uint8_t *index) {
    buffer[*index] = new_val;

    uint64_t sum = 0;
    for (int i = 0; i < size; i++) {
        sum += buffer[i];
    }
    return (uint32_t)(sum / size);
}

// MODELO 6 Filtro RC para o PA2
float apply_rc_filter(uint32_t raw_val, float *filtered_val, float alpha) {
    *filtered_val = alpha * ((float)raw_val) + (1.0f - alpha) * (*filtered_val);
    return *filtered_val;
}

// MODELO 10 mapeamento para Torque
uint16_t map_value(uint32_t adc_val) {
    return (uint16_t)((((float)adc_val) / ADC_MAX_VALUE) * 1000.0f);
}

// MODELO 3: Função de Velocidade Não-Linear
uint32_t non_linear_pwm(uint32_t adc_val, uint32_t arr_val) {

    /* MODELO 11: Mapeamento de Deadband*/
    const uint32_t DEAD_BAND_ADC = 100;

    if (adc_val < DEAD_BAND_ADC) {
        return 0;
    }

    float normalized;

    if (ADC_MAX_VALUE > DEAD_BAND_ADC) {
        normalized = ((float)adc_val - DEAD_BAND_ADC) / (ADC_MAX_VALUE - DEAD_BAND_ADC);
    } else {
        normalized = (float)adc_val / ADC_MAX_VALUE;
    }

    if (normalized > 1.0f) normalized = 1.0f;
    if (normalized < 0.0f) normalized = 0.0f;

    // Duty = ARR * (normalized_adc)^2
    return (uint32_t)(powf(normalized, 2.0f) * arr_val);
}

// MODELO 4: Controle Proporcional
uint32_t apply_p_control(uint32_t setpoint_adc, uint32_t feedback_adc, uint32_t arr_val) {
    const float KP = 0.5f;
    float setpoint_norm = (float)setpoint_adc / ADC_MAX_VALUE;
    float feedback_norm = (float)feedback_adc / ADC_MAX_VALUE;

    float error = setpoint_norm - feedback_norm;

    float duty_change = KP * error * arr_val;

    uint32_t current_duty = duty_cycle_atual;
    current_duty += (int32_t)duty_change;

    if (current_duty > arr_val) current_duty = arr_val;
    if (current_duty < 0) current_duty = 0;

    return current_duty;
}

// MODELO 12: Máquina de Estados Sequencial (7 Estados)
void StateMachine_Handler(void)
{
    switch (current_state)
    {
        case STATE_INIT:
            break;
        case STATE_READ_A:
            break;
        case STATE_READ_B:
            break;
        case STATE_FILT_A:
            break;
        case STATE_FILT_B:
            break;
        case STATE_DIAGN_A:
            break;
        case STATE_DIAGN_B:
            break;
        default:
            current_state = STATE_INIT;
            break;
    }

    // Transição para o Próximo Estado
    current_state = (current_state + 1) % 7;
}

// --- sentido dos motores LCD ---
void Display_Motor_Direction(void)
{
    char buf[17];

    lcd_put_cur(0, 0);
    lcd_send_string("INICIALIZACAO    ");

    lcd_put_cur(1, 0);

    snprintf(buf, 17, "A:%s | B:%s   ",
             (motorA_dir == 0) ? "H" : "AH",
             (motorB_dir == 0) ? "H" : "AH");

    lcd_send_string(buf);
}


/* --- interrupção (TIM2) --- */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM2) {
        StateMachine_Handler(); // para a Máquina de Estados
    }
}


/* --- delay--- */
void loop_delay(uint32_t ms) {
  uint32_t start = HAL_GetTick();
  while (HAL_GetTick() - start < ms);
}

/* ----------------- funçao 3 teste motores ----------------- */
void Rotina_Teste_Direcao(void)
{
    lcd_send_cmd(LCD_CLEARDISPLAY);
    loop_delay(3);
    lcd_put_cur(0, 0);
    lcd_send_string("TESTE DIRECAO    ");

    // PWM teste
    uint32_t test_duty = htim3.Instance->ARR / 2;
    if (test_duty == 0) test_duty = 500;

    motorA_on = 1;
    motorB_on = 1;

    // Motor A Horário
    lcd_put_cur(1, 0);
    lcd_send_string("A: H | B: Parado");
    motorA_dir = 0; // H
    MotorA_Update();
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, test_duty);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, 0); // B Parado
    HAL_Delay(4000);

    //Motor A Anti-Horário
    lcd_put_cur(1, 0);
    lcd_send_string("A: AH | B: Parado");
    motorA_dir = 1; // AH
    MotorA_Update();
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, test_duty);
    HAL_Delay(4000);

    // desliga motor A
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, 0);

    // motor B Horário
    lcd_put_cur(1, 0);
    lcd_send_string("A: Parado | B: H");
    motorB_dir = 0; // H
    MotorB_Update();
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, test_duty);
    HAL_Delay(4000);

    // motor B Anti-Horário
    lcd_put_cur(1, 0);
    lcd_send_string("A: Parado | B: AH");
    motorB_dir = 1; // AH
    MotorB_Update();
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, test_duty);
    HAL_Delay(4000);

    // desliga Motor B
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, 0);

    motorA_on = 0;
    motorB_on = 0;
    MotorA_Update();
    MotorB_Update();

    lcd_send_cmd(LCD_CLEARDISPLAY);
    loop_delay(3);
    lcd_put_cur(0, 0);
    lcd_send_string("motores ok");
    lcd_put_cur(1, 0);
    lcd_send_string("inversao realizavel");
    HAL_Delay(2000);
}


/* ---------------- Frenagem ---------------- */
void Apply_Freagem(void)
{
    Frenagem_impl();
}

void Frenagem_impl(void)
{

    lcd_send_cmd(LCD_CLEARDISPLAY);
    loop_delay(3);
    lcd_put_cur(0, 0);
    lcd_send_string("FRENAGEM         ");

    uint32_t ARR = htim3.Instance->ARR;
    if (ARR == 0) ARR = 1;

    /* duty_cycle_atual com o valor do PWM anterior */
    uint32_t duty = duty_cycle_atual;
    if (duty > ARR) duty = ARR;

    /* MODELO 2 Aceleração */
    uint32_t step = ARR / 100;
    if (step == 0) step = 1;

    /* MODELO 7: Mapeamento de Tempo */
    uint32_t base_delay_ms = 40;

    motorA_on = 1;
    motorB_on = 1;

    MotorA_Update();
    MotorB_Update();

    lcd_put_cur(1, 0);
    lcd_send_string("Veloc:     %      ");

    while (duty > 0)
    {

        if (duty <= step) duty = 0;
        else duty -= step;

        /* para os dois canais dos motores */
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, duty);
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, duty);

        /* MODELO 8: calc porcentagem de velocidade  */
        uint32_t percent = (duty * 100) / ARR;

        /* MODELO 9: mapeamento por nível LED (PB5, PB8, PB9) */
        if (percent == 0) {
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET);
        } else if (percent <= 33) {
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);
        } else if (percent <= 66) {
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);
        } else {
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET);
        }

        char buf[5];
        snprintf(buf, sizeof(buf), "%3lu", (unsigned long)percent);

        lcd_put_cur(1, 7);
        lcd_send_string(buf);

        // cursor na linha 1
        lcd_send_cmd(LCD_DISPLAYCONTROL | 0x0C);
        loop_delay(1);

        duty_cycle_atual = duty;

        //  Modelo 7: Atraso inver prop
        uint32_t delay_val = base_delay_ms;
        if (percent > 0) {
            delay_val = base_delay_ms - (uint32_t)((base_delay_ms * percent) / 100);
            if (delay_val < 5) delay_val = 5;
        }
        HAL_Delay(delay_val);
    }

    // finaliza
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, 0);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, 0);
    duty_cycle_atual = 0;

    motorA_on = 0;
    motorB_on = 0;

    MotorA_Update();
    MotorB_Update();

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2 | GPIO_PIN_3, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8 | GPIO_PIN_9, GPIO_PIN_RESET);

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET);

    lcd_send_cmd(LCD_CLEARDISPLAY);
    loop_delay(3);
    lcd_put_cur(0, 0);
    lcd_send_string("Motores Parados  ");
}

/* ----------------- Inversão de Sentido  ----------------- */
void Controlar_Sentido_Motores(uint16_t btnA_pin, uint16_t btnB_pin, int case_id)
{
    uint32_t now = HAL_GetTick();
    static uint8_t lastA = 1;
    static uint8_t lastB = 1;
    uint8_t display_update_needed = 0;

    // Motor A
    uint8_t leituraA = HAL_GPIO_ReadPin(GPIOB, btnA_pin);
    if (leituraA == GPIO_PIN_RESET && lastA == GPIO_PIN_SET && (now - lastDirectionButtonTime[0] > debounceDelay))
    {
        motorA_dir = !motorA_dir;    // Inverte sentido
        motorA_on = 1;
        MotorA_Update();
        lastDirectionButtonTime[0] = now;
        if (case_id == 2) display_update_needed = 1;
    }
    lastA = leituraA;

    // Motor B
    uint8_t leituraB = HAL_GPIO_ReadPin(GPIOB, btnB_pin);
    if (leituraB == GPIO_PIN_RESET && lastB == GPIO_PIN_SET && (now - lastDirectionButtonTime[1] > debounceDelay))
    {
        motorB_dir = !motorB_dir;    // Inverte sentido
        motorB_on = 1;
        MotorB_Update();
        lastDirectionButtonTime[1] = now;
        if (case_id == 2) display_update_needed = 1;
    }
    lastB = leituraB;

    /* atualiza o display caso esteja no modo 2 */
    if (case_id == 2 && display_update_needed)
    {

        lcd_send_cmd(LCD_CLEARDISPLAY);
        loop_delay(3);
        lcd_put_cur(0, 0);
        lcd_send_string(" DIRECAO MOTORES ");

        lcd_put_cur(1, 0);
        char buf[17];
        if (motorA_dir == 0 && motorB_dir == 0)
            snprintf(buf, 17, "A:H   B:H      ");
        else if (motorA_dir == 0 && motorB_dir == 1)
            snprintf(buf, 17, "A:H   B:AH     ");
        else if (motorA_dir == 1 && motorB_dir == 0)
            snprintf(buf, 17, "A:AH  B:H      ");
        else
            snprintf(buf, 17, "A:AH  B:AH     ");
        lcd_send_string(buf);

        lcd_send_cmd(LCD_DISPLAYCONTROL | 0x0C);
        loop_delay(1);
    }
}

/* ----------------- velocidade para o torque ------------------*/
void Sequencia_LEDs(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    /* CHANNEL2 */
    sConfig.Channel = ADC_CHANNEL_2;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_112CYCLES;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) { Error_Handler(); }

    if (HAL_ADC_Start(&hadc1) != HAL_OK) { Error_Handler(); }
    if (HAL_ADC_PollForConversion(&hadc1, 20) != HAL_OK) {
        HAL_ADC_Stop(&hadc1);
        return;
    }
    uint32_t adc_val_raw = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);

    /* MODELO 6 filtro RC Digital no PA2 */
    apply_rc_filter(adc_val_raw, &adc_rc_filtered, alpha_rc);
    uint32_t adc_val = (uint32_t)adc_rc_filtered;


    /* porcentagem */
    uint32_t percent = (adc_val * 100) / ADC_MAX_VALUE;
    if (percent > 100) percent = 100;

    char buf[7];

    lcd_put_cur(0,0);
    lcd_send_string("ACELERACAO       ");

    /* MODELO 10 conversão de unidade no display */
   torque_relativo = map_value(adc_val);

    lcd_put_cur(1,0);
    lcd_send_string("Torque:    ");

    snprintf(buf, sizeof(buf), "%4u", torque_relativo);
    lcd_put_cur(1, 8);
    lcd_send_string(buf);

    lcd_send_cmd(LCD_DISPLAYCONTROL | 0x0C);
    loop_delay(1);

    /* MODELO 9 mapeamento por nível LEDs (PB5, PB8, PB9) */
    if (percent <= 33) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);
    } else if (percent <= 66) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);
    } else {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET);
    }

    /* acelera motores (CH3 e CH4) */
    uint32_t ARR = htim3.Instance->ARR;
    if (ARR == 0) ARR = 1;

    /* MODELO 1 mapeamento linear do Potenciômetro */
    uint32_t ccr = (percent * ARR) / 100;

    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, ccr);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, ccr);

    duty_cycle_atual = ccr;

    motorA_on = (ccr > 0) ? 1 : 0;
    motorB_on = (ccr > 0) ? 1 : 0;

    MotorA_Update();
    MotorB_Update();

}

/* ----------------- MAIN ----------------- */
int main(void)
{
  HAL_Init();
  SystemClock_Config();

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  MX_I2C1_Init();
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_TIM3_PWM_Init();
  MX_TIM2_Init();

  /* PA0, PA1, PA2 */
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* PA4-PA7  e led PA8-PA9 */
  GPIO_InitStruct.Pin = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* Menu PA10-PA12 */
  GPIO_InitStruct.Pin = BTN_UP_PIN | BTN_DOWN_PIN | BTN_SELECT_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* Motor botoes PB12-PB15 */
  GPIO_InitStruct.Pin = GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* Ensure LED outputs PB9 PB8 PB5 and NEW LEDs (PB2, PB3) are configured */
  GPIO_InitStruct.Pin = GPIO_PIN_9 | GPIO_PIN_8 | GPIO_PIN_5 | GPIO_PIN_2 | GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* Start PWM for motors (CH3, CH4) */
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4);

  /* Inicia a interrupção de base de tempo do TIM2 (10ms) */
  HAL_TIM_Base_Start_IT(&htim2);

  lcd_init();
  lcd_print_menu(current_selection);

  MotorA_Update();
  MotorB_Update();

  // Array estático para controle do setup inicial
  static uint8_t initial_setup_done[5] = {0, 0, 0, 0, 0};

  while (1)
  {
    /* update PA0/PA1 readings for motors */
    Read_ADC_Polling();

    if (menu_state == 0)
    {
      lcd_handle_input();
      for (int i = 0; i < 5; i++) initial_setup_done[i] = 0;
    }
    else
    {
      if (current_selection == 0) {
        Read_Motor_Buttons();
        Controlar_Sentido_Motores(GPIO_PIN_13, GPIO_PIN_15, 0); // inverte direção
      }

      if (current_selection == 2) {
            Controlar_Sentido_Motores(GPIO_PIN_13, GPIO_PIN_15, 99);
      }

      /* --- ATUALIZAÇÃO DE DISPLAY --- */
      uint32_t now = HAL_GetTick();

      // atualiza apos 50ms
      if (now - lastDisplayUpdateTime >= displayRefreshRate)
      {
          lastDisplayUpdateTime = now; // Reinicia o contador de tempo

          switch (current_selection)
          {
            case 0:
              // INICIALIZACAO
              Display_Motor_Direction();
              lcd_send_cmd(LCD_DISPLAYCONTROL | 0x0C);
              loop_delay(1);
              break;

            case 1: {
              // VELOCIDADE

              if (!initial_setup_done[1]) {
                  lcd_put_cur(0, 0); lcd_send_string("VELOCIDADE       ");
                  lcd_put_cur(1, 0); lcd_send_string("M1:    % M2:    %");
                  initial_setup_done[1] = 1;
              }

              uint32_t pot_A_val = adc_values_filtered[0];
              uint32_t pot_B_val = adc_values_filtered[1];

              uint32_t ARR = htim3.Instance->ARR;
              if (ARR == 0) ARR = 1;

              uint32_t ccr_A = non_linear_pwm(pot_A_val, ARR); //converte o val potenciometro para o ccr
              uint32_t ccr_B = non_linear_pwm(pot_B_val, ARR);

              __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, ccr_A);
              __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, ccr_B);

              duty_cycle_atual = (ccr_A + ccr_B) / 2;

              motorA_on = (ccr_A > 0) ? 1 : 0;
              motorB_on = (ccr_B > 0) ? 1 : 0;

              MotorA_Update();
              MotorB_Update();

              char buffer[5];

              uint8_t speed_A = (ccr_A * 100) / ARR;
              uint8_t speed_B = (ccr_B * 100) / ARR;

              snprintf(buffer, sizeof(buffer), "%3d", speed_A);
              lcd_put_cur(1, 3);
              lcd_send_string(buffer);

              snprintf(buffer, sizeof(buffer), "%3d", speed_B);
              lcd_put_cur(1, 11);
              lcd_send_string(buffer);

              lcd_send_cmd(LCD_DISPLAYCONTROL | 0x0C);
              loop_delay(1);
              break;
            }

            case 2: {
                // teste motor
                if (!initial_setup_done[2]) {
                    Rotina_Teste_Direcao();
                    initial_setup_done[2] = 1;
                }

                lcd_put_cur(0, 0);
                lcd_send_string("motores ok");
                lcd_put_cur(1, 0);
                lcd_send_string("inversao realizavel");

                lcd_send_cmd(LCD_DISPLAYCONTROL | 0x0C);
                loop_delay(1);

                break;
            }

            case 3:
              // frenagem
              lcd_send_string("FREIO MIP            ");
              Apply_Freagem();
              break;

            case 4:
              // aceleração
              Sequencia_LEDs();
              break;
          }
      }

      /* ---------- select para o submenu ---------- */
      if (HAL_GPIO_ReadPin(GPIOA, BTN_SELECT_PIN) == GPIO_PIN_RESET)
      {
        wait_for_button_release(GPIOA, BTN_SELECT_PIN);

        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, 0);
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, 0);

        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5 | GPIO_PIN_8 | GPIO_PIN_9, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2 | GPIO_PIN_3, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8 | GPIO_PIN_9, GPIO_PIN_RESET);


        motorA_on = 0;
        motorB_on = 0;

        MotorA_Update();
        MotorB_Update();

        lcd_send_cmd(LCD_CLEARDISPLAY);
        loop_delay(3);
        lcd_put_cur(0, 0);
        lcd_send_string("Menu Principal:");
        current_selection = 0;
        menu_state = 0;
        lcd_print_menu(current_selection);
      }
    }

    HAL_Delay(1);
  }
}

/* --- liga e desliga (PB12/PB14) --- */
void Read_Motor_Buttons(void)
{
    uint32_t now = HAL_GetTick();

    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_12) == GPIO_PIN_RESET && (now - lastButtonTime[0] > debounceDelay))
    {
        motorA_on = !motorA_on;
        MotorA_Update();
        lastButtonTime[0] = now;
    }

    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_14) == GPIO_PIN_RESET && (now - lastButtonTime[2] > debounceDelay))
    {
        motorB_on = !motorB_on;
        MotorB_Update();
        lastButtonTime[2] = now;
    }
}

/* --- direção mot a --- */
void MotorA_Update(void)
{
    if (motorA_on)
    {
        if (motorA_dir == 0) { // H
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
        } else { // AH
        	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
        }
    }
    else
    {
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, 0);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 | GPIO_PIN_5, GPIO_PIN_RESET);
    }

    // --- led ---
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, (motorA_on == 1) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    if (motorA_on && motorA_dir == 1) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_RESET);
    }
}

/* --- direção mot b --- */
void MotorB_Update(void)
{

    if (motorB_on)
    {
        if (motorB_dir == 0) { // H
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);
        } else { // AH
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET);
        }
    }
    else
    {
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, 0);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_RESET);
    }

    // --- leds b ---
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, (motorB_on == 1) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    if (motorB_on && motorB_dir == 1) {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);
    }
}

/* --- Menu LCD handling --- */
void lcd_handle_input(void)
{
    uint32_t now = HAL_GetTick();

    if (HAL_GPIO_ReadPin(GPIOA, BTN_UP_PIN) == GPIO_PIN_RESET && now - lastMenuButtonTime[0] > debounceDelay)
    {
        if (current_selection > 0)
        {
            current_selection--;
            lcd_print_menu(current_selection);
        }
        lastMenuButtonTime[0] = now;
    }

    if (HAL_GPIO_ReadPin(GPIOA, BTN_DOWN_PIN) == GPIO_PIN_RESET && now - lastMenuButtonTime[1] > debounceDelay)
    {
        if (current_selection < 4)
        {
            current_selection++;
            lcd_print_menu(current_selection);
        }
        lastMenuButtonTime[1] = now;
    }

    if (HAL_GPIO_ReadPin(GPIOA, BTN_SELECT_PIN) == GPIO_PIN_RESET && now - lastMenuButtonTime[2] > debounceDelay)
    {
        wait_for_button_release(GPIOA, BTN_SELECT_PIN);
        menu_state = 1;
        lastMenuButtonTime[2] = now;
    }
}

void wait_for_button_release(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin) {
    while(HAL_GPIO_ReadPin(GPIOx, GPIO_Pin) == GPIO_PIN_RESET);
    loop_delay(50);
}

void lcd_print_menu(uint8_t sel) {

    char *menu_items[] = {
        "1. INICIALIZACAO",
        "2. VELOCIDADE",
        "3. TESTES MOTOR",
        "4. FREAGEM",
        "5. ACELERACAO"
    };

    lcd_put_cur(1, 0);
    lcd_send_string("                ");
    lcd_put_cur(1, 0);

    char output_str[17];
    snprintf(output_str, 17, "> %s", menu_items[sel]);
    lcd_send_string(output_str);
}

/* --- LCD  implementação--- */
void MX_I2C1_Init(void)
{
  __HAL_RCC_I2C1_CLK_ENABLE();

  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK) { Error_Handler(); }

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  __HAL_RCC_GPIOB_CLK_ENABLE();

  GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

void lcd_send_byte(char data, int type)
{
    char data_u, data_l;
    uint8_t data_t[4];

    data_u = data & 0xf0;
    data_l = (data << 4) & 0xf0;

    char control = (type == 1) ? (RS | BL) : BL;

    data_t[0] = data_u | EN | control; // EN = 1 alto
    data_t[1] = data_u | control;       // EN = 0 baixo

    data_t[2] = data_l | EN | control; // EN = 1
    data_t[3] = data_l | control;       // EN = 0

    loop_delay(5);
    HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR, (uint8_t *) data_t, 4, 500);

    loop_delay(5);
}

void lcd_send_cmd(char cmd) { lcd_send_byte(cmd, 0); }
void lcd_send_data(char data) { lcd_send_byte(data, 1); }


void lcd_init(void)
{
    loop_delay(50);

    lcd_send_cmd(0x30);
    loop_delay(5);

    lcd_send_cmd(0x30);
    loop_delay(1);

    lcd_send_cmd(0x30);
    loop_delay(10);

    lcd_send_cmd(0x20); // 4-bit
    loop_delay(1);

    //4-bit, 2 linhas
    lcd_send_cmd(LCD_FUNCTIONSET | 0x08);
    loop_delay(1);

    // Display desliga
    lcd_send_cmd(LCD_DISPLAYCONTROL | 0x00);
    loop_delay(1);

    // limpa Display
    lcd_send_cmd(LCD_CLEARDISPLAY);
    loop_delay(3);

    // incremento
    lcd_send_cmd(LCD_ENTRYMODESET | 0x02);
    loop_delay(1);

    // Display liga, cursor nn
    lcd_send_cmd(LCD_DISPLAYCONTROL | 0x0C);
    loop_delay(1);

    lcd_put_cur(0, 0);
    lcd_send_string("Menu Principal:");
}

void lcd_send_string(char *str)
{
    while (*str) lcd_send_data(*str++);
}

void lcd_put_cur(uint8_t row, uint8_t col)
{
    uint8_t address;
    switch (row)
    {
        case 0: address = LCD_SETDDRAMADDR | (0x00 + col); break;
        case 1: address = LCD_SETDDRAMADDR | (0x40 + col); break;
        default: address = LCD_SETDDRAMADDR | (0x00 + col); break;
    }
    lcd_send_cmd(address);
}

/* --- ADC para o polling PA0/PA1  --- */
void MX_ADC1_Init(void) {
    __HAL_RCC_ADC1_CLK_ENABLE();

    hadc1.Instance = ADC1;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode = DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;

    if (HAL_ADC_Init(&hadc1) != HAL_OK) { Error_Handler(); }
}

/* Função de leitura por polling: PA0 e PA1 */
void Read_ADC_Polling(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    uint32_t raw_val;

    /* PA0 -> ADCcanal0 */
    sConfig.Channel = ADC_CHANNEL_0;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_112CYCLES;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) { Error_Handler(); }

    if (HAL_ADC_Start(&hadc1) != HAL_OK) { Error_Handler(); }
    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)
    {
        raw_val = HAL_ADC_GetValue(&hadc1);
        adc_values_raw[0] = raw_val;

        /* MODELO 5: Aplica Filtro de Média Móvel no PA0 */
        adc_values_filtered[0] = apply_moving_average(raw_val, adc_buffer_A, FILTER_SIZE, &buffer_index);
    }
    HAL_ADC_Stop(&hadc1);

    loop_delay(1);

    /* PA1 -> ADCcanal1 */
    sConfig.Channel = ADC_CHANNEL_1;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_112CYCLES;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) { Error_Handler(); }

    if (HAL_ADC_Start(&hadc1) != HAL_OK) { Error_Handler(); }
    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)
    {
        raw_val = HAL_ADC_GetValue(&hadc1);
        adc_values_raw[1] = raw_val;

        /* MODELO 5: Aplica Filtro de Média Móvel no PA1 */
        adc_values_filtered[1] = apply_moving_average(raw_val, adc_buffer_B, FILTER_SIZE, &buffer_index);
    }
    HAL_ADC_Stop(&hadc1);

    buffer_index = (buffer_index + 1) % FILTER_SIZE;
}

/* --- PWM TIM3 - CH3, CH4 --- */
void MX_TIM3_PWM_Init(void) {
    TIM_OC_InitTypeDef sConfigOC = {0};

    __HAL_RCC_TIM3_CLK_ENABLE();

    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 84 - 1;
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 1000 - 1;
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;

    if (HAL_TIM_PWM_Init(&htim3) != HAL_OK) { Error_Handler(); }

    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 0;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;

    if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_3) != HAL_OK) { Error_Handler(); }
    if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_4) != HAL_OK) { Error_Handler(); }

    if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK) {  }
}

/* --- TIM2 para Máquina de Estados --- */
void MX_TIM2_Init(void) {
    __HAL_RCC_TIM2_CLK_ENABLE(); // Habilita o clock do TIM2

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 420 - 1;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 1000 - 1;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;

    if (HAL_TIM_Base_Init(&htim2) != HAL_OK) { Error_Handler(); }
}

/* --- GPIO inicialização --- */
void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* PA0, PA1, PA2 */
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* Motores PA4-PA7 */
    GPIO_InitStruct.Pin = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* botoes menu PA10-PA12 */
    GPIO_InitStruct.Pin = BTN_UP_PIN | BTN_DOWN_PIN | BTN_SELECT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* botoes motor PB12-PB15 */
    GPIO_InitStruct.Pin = GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* LEDs (PB5, PB8, PB9, PB2, PB3, PA8, PA9) */
    GPIO_InitStruct.Pin = GPIO_PIN_9 | GPIO_PIN_8 | GPIO_PIN_5 | GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // LEDs em GPIOA: PA8 | PA9
    GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // Desliga LEDs
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2 | GPIO_PIN_3, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8 | GPIO_PIN_9, GPIO_PIN_RESET);

    /* botão interrupção PA15 (EXTI) */
    GPIO_InitStruct.Pin = GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // habilitar o NVIC
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

//Interrupção no PA15
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{

    if (GPIO_Pin == GPIO_PIN_15)
    {
        motorA_on = !motorA_on;
        MotorA_Update();

        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5);

    }
  }

// clock
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;

  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) { Error_Handler(); }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK) { Error_Handler(); }
}

/* --- Error Handler --- */
void Error_Handler(void)
{
  __disable_irq();
  while (1);
}
