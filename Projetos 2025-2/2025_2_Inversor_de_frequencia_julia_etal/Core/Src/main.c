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
 in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <string.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// Endereço do LCD I2C (0x27 ou 0x3F)
#define LCD_ADDR 0x27 << 1

// Comandos do LCD
#define LCD_CLEAR 0x01
#define LCD_HOME 0x02
#define LCD_ENTRY_MODE 0x06
#define LCD_DISPLAY_OFF 0x08
#define LCD_DISPLAY_ON 0x0C
#define LCD_FUNCTION_SET 0x28
#define LCD_SET_CGRAM 0x40
#define LCD_SET_DDRAM 0x80

// Pinos do LCD via I2C
#define LCD_BACKLIGHT 0x08
#define LCD_EN 0x04
#define LCD_RW 0x02
#define LCD_RS 0x01

// Posições das linhas
#define LCD_LINE1 0x00
#define LCD_LINE2 0x40
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;

I2C_HandleTypeDef hi2c1;

/* USER CODE BEGIN PV */
int menup = 1, retorno = 1;
int delayLEDmenu = 340, periodotimer = 7300, periodoset = 3400;
int motor = 0, sentido = 0, start = 0;
int velocidadeAC = 500, velocidadeDC = 500;
int f = 50;
char buffer1[100],buffer2[100];
int i=0;
int32_t adc1_0;
int rpm= 0, w=0, teste_1=0;
float s=0;
float real =0, ligado =0, valore = 0.44;
int32_t adc2_0 = 0, adc3_0 = 0;
int rpm_desejado = 1500;
int menu_sugerido = 1;
double pi = 3.14159265358979323846;
int intensidade_pa1 = 0;
int ciclo_pwm_led = 0;
uint32_t ultimo_update_led = 0;
uint32_t tempo_sem_botao_menu = 0;

int emergencia_ativa = 0;
int pisca_emergencia = 0;
uint32_t tempo_proxima_pisca = 0;
uint32_t tempo_pisca_b7 = 0;
int pisca_b7_estado = 0;

uint32_t ultimo_update_lcd = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM4_Init(void);
static void MX_I2C1_Init(void);
/* USER CODE BEGIN PFP */
void acionarRetorno(void);
void desligarTodosLEDsMenu(void);
void processarParada(void);
void lerTodosADCs(void);
void atualizarLEDsMenu(void);
void atualizarBrilhoLED(void);
void gerenciarEmergencia(void);


void LCD_SendByte(uint8_t data, uint8_t mode);
void LCD_SendCommand(uint8_t cmd);
void LCD_SendData(uint8_t data);
void LCD_Init(void);
void LCD_Clear(void);
void LCD_SetCursor(uint8_t row, uint8_t col);
void LCD_WriteString(char *str);
void atualizarLCD(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// Funções LCD
void LCD_SendByte(uint8_t data, uint8_t mode) {
    uint8_t high_nibble = data & 0xF0;
    uint8_t low_nibble = (data << 4) & 0xF0;
    uint8_t data_t[4];


    data_t[0] = (high_nibble | mode | LCD_BACKLIGHT | LCD_EN);
    data_t[1] = (high_nibble | mode | LCD_BACKLIGHT) & ~LCD_EN;


    data_t[2] = (low_nibble | mode | LCD_BACKLIGHT | LCD_EN);
    data_t[3] = (low_nibble | mode | LCD_BACKLIGHT) & ~LCD_EN;

    HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR, data_t, 4, 100);
}

void LCD_SendCommand(uint8_t cmd) {
    LCD_SendByte(cmd, 0);
    if(cmd == LCD_CLEAR || cmd == LCD_HOME) {
        HAL_Delay(2);
    } else {
        HAL_Delay(1);
    }
}

void LCD_SendData(uint8_t data) {
    LCD_SendByte(data, LCD_RS);
    HAL_Delay(1);
}

void LCD_Init(void) {
    HAL_Delay(50);

    uint8_t init = 0x30 | LCD_BACKLIGHT | LCD_EN;
    HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR, &init, 1, 100);
    HAL_Delay(5);
    init &= ~LCD_EN;
    HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR, &init, 1, 100);
    HAL_Delay(1);

    init = 0x30 | LCD_BACKLIGHT | LCD_EN;
    HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR, &init, 1, 100);
    HAL_Delay(1);
    init &= ~LCD_EN;
    HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR, &init, 1, 100);
    HAL_Delay(1);

    init = 0x20 | LCD_BACKLIGHT | LCD_EN;
    HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR, &init, 1, 100);
    HAL_Delay(1);
    init &= ~LCD_EN;
    HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR, &init, 1, 100);

    LCD_SendCommand(LCD_FUNCTION_SET);
    LCD_SendCommand(LCD_DISPLAY_OFF);
    LCD_SendCommand(LCD_CLEAR);
    LCD_SendCommand(LCD_ENTRY_MODE);
    LCD_SendCommand(LCD_DISPLAY_ON);
    HAL_Delay(10);
}

void LCD_Clear(void) {
    LCD_SendCommand(LCD_CLEAR);
    HAL_Delay(2);
}

void LCD_SetCursor(uint8_t row, uint8_t col) {
    uint8_t address;
    if(row == 0)
        address = LCD_LINE1 + col;
    else
        address = LCD_LINE2 + col;

    LCD_SendCommand(LCD_SET_DDRAM | address);
}

void LCD_WriteString(char *str) {
    while(*str) {
        LCD_SendData(*str++);
    }
}

void atualizarLCD(void) {
    uint32_t agora = HAL_GetTick();

    if (agora - ultimo_update_lcd < 500) {
        return;
    }
    ultimo_update_lcd = agora;

    char lcd_buffer[17];


       LCD_SetCursor(0, 0);
       if (emergencia_ativa) {
           LCD_WriteString("EMERGENCIA!      ");
       } else if (start == 1) {
           snprintf(lcd_buffer, sizeof(lcd_buffer), "RPM: %4d        ", rpm);
           LCD_WriteString(lcd_buffer);
       } else {
           snprintf(lcd_buffer, sizeof(lcd_buffer), "RPM: %4d (OFF) ", rpm);
           LCD_WriteString(lcd_buffer);
       }


       LCD_SetCursor(1, 0);
       if (emergencia_ativa) {
           LCD_WriteString("Em manutencao...");
       } else {
           snprintf(lcd_buffer, sizeof(lcd_buffer), "Menu: %d        ", menup);
           LCD_WriteString(lcd_buffer);
       }
   }

void lerTodosADCs(void) {
    ADC_ChannelConfTypeDef sConfig = {0};
    static uint8_t canal_atual = 0;
    static uint32_t ultimo_tempo = 0;
    uint32_t tempo_atual = HAL_GetTick();

    if (tempo_atual - ultimo_tempo < 30) {
        return;
    }

    switch(canal_atual) {
        case 0:
            sConfig.Channel = ADC_CHANNEL_1;
            sConfig.Rank = 1;
            sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;
            HAL_ADC_ConfigChannel(&hadc1, &sConfig);

            HAL_ADC_Start(&hadc1);
            if (HAL_ADC_PollForConversion(&hadc1, 20) == HAL_OK) {
                adc1_0 = HAL_ADC_GetValue(&hadc1);
                if (adc1_0 < 100) adc1_0 = 100;
                intensidade_pa1 = (adc1_0 * 100) / 4095;
            }
            HAL_ADC_Stop(&hadc1);
            break;

        case 1:
            sConfig.Channel = ADC_CHANNEL_2;
            sConfig.Rank = 1;
            sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;
            HAL_ADC_ConfigChannel(&hadc1, &sConfig);

            HAL_ADC_Start(&hadc1);
            if (HAL_ADC_PollForConversion(&hadc1, 20) == HAL_OK) {
                adc2_0 = HAL_ADC_GetValue(&hadc1);
                rpm_desejado = (adc2_0 * 3000) / 4095;
                if (rpm_desejado < 100) rpm_desejado = 100;
                if (rpm_desejado > 3000) rpm_desejado = 3000;
            }
            HAL_ADC_Stop(&hadc1);
            break;

        case 2:
            sConfig.Channel = ADC_CHANNEL_4;
            sConfig.Rank = 1;
            sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;
            HAL_ADC_ConfigChannel(&hadc1, &sConfig);

            HAL_ADC_Start(&hadc1);
            if (HAL_ADC_PollForConversion(&hadc1, 20) == HAL_OK) {
                adc3_0 = HAL_ADC_GetValue(&hadc1);
                menu_sugerido = 1 + (adc3_0 * 4) / 4095;
                if (menu_sugerido > 4) menu_sugerido = 4;
                if (menu_sugerido < 1) menu_sugerido = 1;
            }
            HAL_ADC_Stop(&hadc1);
            break;
    }

    canal_atual = (canal_atual + 1) % 3;
    ultimo_tempo = tempo_atual;

    sConfig.Channel = ADC_CHANNEL_1;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
}


void atualizarBrilhoLED(void) {
    uint32_t agora = HAL_GetTick();

    if (agora - ultimo_update_led < 2) {
        return;
    }
    ultimo_update_led = agora;

    ciclo_pwm_led = (ciclo_pwm_led + 1) % 100;

    if (ciclo_pwm_led < intensidade_pa1) {
        HAL_GPIO_WritePin(LED_KIT_GPIO_Port, LED_KIT_Pin, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(LED_KIT_GPIO_Port, LED_KIT_Pin, GPIO_PIN_RESET);
    }
}


void atualizarLEDsMenu(void) {

    if (emergencia_ativa) {
        return;
    }

    HAL_GPIO_WritePin(LED_MENU1_GPIO_Port, LED_MENU1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_MENU2_GPIO_Port, LED_MENU2_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_MENU3_GPIO_Port, LED_MENU3_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_MENU4_GPIO_Port, LED_MENU4_Pin, GPIO_PIN_RESET);

    switch(menup) {
        case 1: HAL_GPIO_WritePin(LED_MENU1_GPIO_Port, LED_MENU1_Pin, GPIO_PIN_SET); break;
        case 2: HAL_GPIO_WritePin(LED_MENU2_GPIO_Port, LED_MENU2_Pin, GPIO_PIN_SET); break;
        case 3: HAL_GPIO_WritePin(LED_MENU3_GPIO_Port, LED_MENU3_Pin, GPIO_PIN_SET); break;
        case 4: HAL_GPIO_WritePin(LED_MENU4_GPIO_Port, LED_MENU4_Pin, GPIO_PIN_SET); break;
    }
}


void gerenciarEmergencia(void) {
    if (emergencia_ativa == 0) {
        return;
    }

    uint32_t agora = HAL_GetTick();


    if (agora >= tempo_proxima_pisca) {
        pisca_emergencia = !pisca_emergencia;

        if (pisca_emergencia == 1) {

            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET);
        } else {

            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);
        }

        tempo_proxima_pisca = agora + 500;
    }


    if (pisca_emergencia == 0) {
        if (agora >= tempo_pisca_b7) {
            pisca_b7_estado = !pisca_b7_estado;
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15,
                pisca_b7_estado ? GPIO_PIN_SET : GPIO_PIN_RESET);
            tempo_pisca_b7 = agora + 125;
        }
    }
}

void desligarTodosLEDsMenu(void) {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);
}

void processarParada(void) {
    if (start == 1) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);
        periodotimer = periodoset;

        while(start == 1){
            periodotimer = periodotimer + velocidadeDC;

            if(periodotimer >= 7300) {
                start = 0;
                periodotimer = periodoset;

                htim2.Instance->CCR1 = 0;
                htim2.Instance->CCR4 = 0;
                htim3.Instance->CCR1 = 0;
                htim3.Instance->CCR2 = 0;
                htim3.Instance->CCR3 = 0;
            }
            i = i - 500;

            if(i <= 0) {
                i = 0;
            }
            __HAL_TIM_SET_AUTORELOAD(&htim4, periodotimer);
            HAL_Delay(100);
        }


        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);
    }
}

void acionarRetorno (){
    if (HAL_GPIO_ReadPin(BT_RETURN_GPIO_Port, BT_RETURN_Pin) == 0){
        HAL_Delay(50);
        if (HAL_GPIO_ReadPin(BT_RETURN_GPIO_Port, BT_RETURN_Pin) == 0){
            retorno = 0;
            while(HAL_GPIO_ReadPin(BT_RETURN_GPIO_Port, BT_RETURN_Pin) == 0);
        }
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim){
    if(htim->Instance == TIM4){
        motor++;

        if (start == 1 && sentido == 0 ){
            if(motor==1){
                htim2.Instance->CCR1 = i;
                htim2.Instance->CCR4 = 0;
                htim3.Instance->CCR1 = 0;
                htim3.Instance->CCR2 = 0;
                htim3.Instance->CCR3 = 0;
            } else if (motor==2) {
                htim2.Instance->CCR1 = 0;
                htim2.Instance->CCR4 = i;
                htim3.Instance->CCR1 = 0;
                htim3.Instance->CCR2 = 0;
                htim3.Instance->CCR3 = 0;
            } else if (motor==3) {
                htim2.Instance->CCR1 = 0;
                htim2.Instance->CCR4 = 0;
                htim3.Instance->CCR1 = i;
                htim3.Instance->CCR2 = 0;
                htim3.Instance->CCR3 = 0;
            } else if (motor==4) {
                htim2.Instance->CCR1 = 0;
                htim2.Instance->CCR4 = 0;
                htim3.Instance->CCR1 = 0;
                htim3.Instance->CCR2 = i;
                htim3.Instance->CCR3 = 0;
            } else if (motor==5) {
                htim2.Instance->CCR1 = 0;
                htim2.Instance->CCR4 = 0;
                htim3.Instance->CCR1 = 0;
                htim3.Instance->CCR2 = 0;
                htim3.Instance->CCR3 = i;
                motor = 0;
            }
        }

        if (start == 1 && sentido == 1 ){
            if(motor==1){
                htim2.Instance->CCR1 = 0;
                htim2.Instance->CCR4 = 0;
                htim3.Instance->CCR1 = 0;
                htim3.Instance->CCR2 = 0;
                htim3.Instance->CCR3 = i;
            } else if (motor==2) {
                htim2.Instance->CCR1 = 0;
                htim2.Instance->CCR4 = 0;
                htim3.Instance->CCR1 = 0;
                htim3.Instance->CCR2 = i;
                htim3.Instance->CCR3 = 0;
            } else if (motor==3) {
                htim2.Instance->CCR1 = 0;
                htim2.Instance->CCR4 = 0;
                htim3.Instance->CCR1 = i;
                htim3.Instance->CCR2 = 0;
                htim3.Instance->CCR3 = 0;
            } else if (motor==4) {
                htim2.Instance->CCR1 = 0;
                htim2.Instance->CCR4 = i;
                htim3.Instance->CCR1 = 0;
                htim3.Instance->CCR2 = 0;
                htim3.Instance->CCR3 = 0;
            } else if (motor==5) {
                htim2.Instance->CCR1 = i;
                htim2.Instance->CCR4 = 0;
                htim3.Instance->CCR1 = 0;
                htim3.Instance->CCR2 = 0;
                htim3.Instance->CCR3 = 0;
                motor = 0;
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
  MX_ADC1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */
    // inicia o PWM
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);  // PA0
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);  // PA3
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);  // PA6 (agora I2C SCL)
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);  // PA7 (agora I2C SDA)
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);  // PB0

    // inicia timer base com interrupção
    HAL_TIM_Base_Start_IT(&htim4);

    HAL_NVIC_SetPriority(TIM4_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(TIM4_IRQn);

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // configurar PA8, PA9, PA10, PA11, PA12, PA15 como saídas
    GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 |
                          GPIO_PIN_12 | GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // configurar PB8, PB9 como saída
    GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    desligarTodosLEDsMenu();

    // estado inicial dos LEDs do menu principal
    HAL_GPIO_WritePin(LED_MENU1_GPIO_Port, LED_MENU1_Pin, 1);
    HAL_GPIO_WritePin(LED_MENU2_GPIO_Port, LED_MENU2_Pin, 0);
    HAL_GPIO_WritePin(LED_MENU3_GPIO_Port, LED_MENU3_Pin, 0);
    HAL_GPIO_WritePin(LED_MENU4_GPIO_Port, LED_MENU4_Pin, 0);


    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);

    HAL_GPIO_WritePin(LED_KIT_GPIO_Port, LED_KIT_Pin, GPIO_PIN_RESET);


    LCD_Init();
    LCD_Clear();
    LCD_SetCursor(0, 0);
    LCD_WriteString("Sistema Motor");
    LCD_SetCursor(1, 0);
    LCD_WriteString("Inicializando...");
    HAL_Delay(1000);
    LCD_Clear();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
    while (1)
    {
        if (emergencia_ativa) {
            gerenciarEmergencia();
            atualizarLCD();
            HAL_Delay(10);
            continue;
        }

        teste_1  = HAL_GPIO_ReadPin(PROTECAO_GPIO_Port, PROTECAO_Pin);

        rpm = (120*f)/6; // rotações por minuto
        w = (2* pi * rpm)/60; // velocidade angular
        s= ((3000.0-rpm)/3000.0)*100.0; // saúde modo motor; detectar sobrecarga;

        // botão B3 - Start
        if (HAL_GPIO_ReadPin(BT_START_GPIO_Port, BT_START_Pin) == 0) {
            HAL_Delay(50);
            if (HAL_GPIO_ReadPin(BT_START_GPIO_Port, BT_START_Pin) == 0) {
                if (start == 0) {
                    start = 1;
                    motor = 0;
                    periodotimer = 7300;

                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET);

                    desligarTodosLEDsMenu();

                    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);
                    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET);

                    while( periodotimer > periodoset) {
                        periodotimer = periodotimer-velocidadeAC;
                        if(periodotimer <= periodoset) {
                            periodotimer = periodoset;
                        }

                        i = i+500;

                        __HAL_TIM_SET_AUTORELOAD(&htim4, periodotimer);
                        HAL_Delay(1000);
                    }

                    HAL_Delay (100);
                }
                while(HAL_GPIO_ReadPin(BT_START_GPIO_Port, BT_START_Pin) == 0);
            }
        }

        // botão B5 parada
        if (HAL_GPIO_ReadPin(BT_STOP_GPIO_Port, BT_STOP_Pin) == 0) {
            HAL_Delay(50);
            if (HAL_GPIO_ReadPin(BT_STOP_GPIO_Port, BT_STOP_Pin) == 0) {
                if (start == 1) {

                    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
                    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);
                    periodotimer = periodoset;

                    while(start == 1){
                        periodotimer = periodotimer + velocidadeDC;

                        if(periodotimer >= 7300) {
                            start = 0;
                            periodotimer = periodoset;
                            // desliga todos os PWM
                            htim2.Instance->CCR1 = 0;
                            htim2.Instance->CCR4 = 0;
                            htim3.Instance->CCR1 = 0;
                            htim3.Instance->CCR2 = 0;
                            htim3.Instance->CCR3 = 0;
                        }
                        i = i - 500;

                        if(i <= 0) {
                            i = 0;
                        }
                        __HAL_TIM_SET_AUTORELOAD(&htim4, periodotimer);
                        HAL_Delay(100);
                    }


                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);
                }
                while(HAL_GPIO_ReadPin(BT_STOP_GPIO_Port, BT_STOP_Pin) == 0);
            }
        }

        lerTodosADCs();

        if (start == 1) {
            periodotimer = 7300 - ((rpm_desejado * (7300 - 280)) / 3000);
            if (periodotimer < 280) periodotimer = 280;
            if (periodotimer > 7300) periodotimer = 7300;

            __HAL_TIM_SET_AUTORELOAD(&htim4, periodotimer);

            delayLEDmenu = 28 + ((rpm_desejado * (340 - 28)) / 3000);
            f = 50 + ((rpm_desejado * (134 - 50)) / 3000);
            periodoset = periodotimer;

            ligado = ligado + 1;
        }

        if (start == 1) {
            i = (adc1_0 * 5000) / 4095;
            if (i < 100) i = 100;
            if (i > 5000) i = 5000;
        }

        tempo_sem_botao_menu++;

        if (tempo_sem_botao_menu > 100 && retorno == 0) {
            if (menu_sugerido != menup) {
                menup = menu_sugerido;
                atualizarLEDsMenu();
            }
        }

        real = ((735*ligado)*valore)/60; // energia produzida pelo motor

        atualizarBrilhoLED();


        if (HAL_GPIO_ReadPin(BT_SENTIDO_GPIO_Port, BT_SENTIDO_Pin) == 0) {
            HAL_Delay(50);
            if (HAL_GPIO_ReadPin(BT_SENTIDO_GPIO_Port, BT_SENTIDO_Pin) == 0) {
                sentido = !sentido;

                HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_12);
                while(HAL_GPIO_ReadPin(BT_SENTIDO_GPIO_Port, BT_SENTIDO_Pin) == 0);
                HAL_Delay(300);
            }
        }

        if (HAL_GPIO_ReadPin(BT_UP_GPIO_Port, BT_UP_Pin) == 0) {
            HAL_Delay(50);
            if (HAL_GPIO_ReadPin(BT_UP_GPIO_Port, BT_UP_Pin) == 0) {
                tempo_sem_botao_menu = 0;
                menup = menup+1;
                if(menup > 4){
                    menup = 1;
                }
                atualizarLEDsMenu();
                while(HAL_GPIO_ReadPin(BT_UP_GPIO_Port, BT_UP_Pin) == 0);
                HAL_Delay(300);
            }
        }

        if (HAL_GPIO_ReadPin(BT_DOWN_GPIO_Port, BT_DOWN_Pin) == 0) {
            HAL_Delay(50);
            if (HAL_GPIO_ReadPin(BT_DOWN_GPIO_Port, BT_DOWN_Pin) == 0) {
                tempo_sem_botao_menu = 0;
                menup = menup-1;
                if (menup < 1){
                    menup = 4;
                }
                atualizarLEDsMenu();
                while(HAL_GPIO_ReadPin(BT_DOWN_GPIO_Port, BT_DOWN_Pin) == 0);
                HAL_Delay(300);
            }
        }

        if (HAL_GPIO_ReadPin(BT_OK_GPIO_Port, BT_OK_Pin) == 0) {
            HAL_Delay(50);
            if (HAL_GPIO_ReadPin(BT_OK_GPIO_Port, BT_OK_Pin) == 0) {
                tempo_sem_botao_menu = 0;
                retorno = 1;
                while(HAL_GPIO_ReadPin(BT_OK_GPIO_Port, BT_OK_Pin) == 0);

                if (menup == 1){
                    while(retorno == 1){
                        if (HAL_GPIO_ReadPin(BT_UP_GPIO_Port, BT_UP_Pin) == 0) {
                            HAL_Delay(50);
                            if (HAL_GPIO_ReadPin(BT_UP_GPIO_Port, BT_UP_Pin) == 0) {
                                delayLEDmenu = delayLEDmenu + 52;
                                periodotimer = periodotimer + 520;

                                f = f - 14;

                                if (f <= 50){
                                    delayLEDmenu = 340;
                                    periodotimer = 3400;
                                    f = 50;
                                }
                                periodoset = periodotimer;
                                __HAL_TIM_SET_AUTORELOAD(&htim4, periodotimer);
                                while(HAL_GPIO_ReadPin(BT_UP_GPIO_Port, BT_UP_Pin) == 0);
                                HAL_Delay(300);
                            }
                        }

                        if (HAL_GPIO_ReadPin(BT_DOWN_GPIO_Port, BT_DOWN_Pin) == 0) {
                            HAL_Delay(50);
                            if (HAL_GPIO_ReadPin(BT_DOWN_GPIO_Port, BT_DOWN_Pin) == 0) {
                                delayLEDmenu = delayLEDmenu - 52;
                                periodotimer = periodotimer - 520;
                                f = f + 14;

                                if (f >= 134){
                                    delayLEDmenu = 28;
                                    periodotimer = 280;
                                    f = 134;
                                }
                                periodoset = periodotimer;
                                __HAL_TIM_SET_AUTORELOAD(&htim4, periodotimer);
                                while(HAL_GPIO_ReadPin(BT_DOWN_GPIO_Port, BT_DOWN_Pin) == 0);
                                HAL_Delay(300);
                            }
                        }

                        HAL_GPIO_TogglePin(LED_MENU1_GPIO_Port, LED_MENU1_Pin);
                        HAL_Delay(delayLEDmenu);

                        atualizarBrilhoLED();
                        atualizarLCD();
                        acionarRetorno();
                    }
                }

                if (menup == 2){
                    while(retorno == 1){
                        if (HAL_GPIO_ReadPin(BT_UP_GPIO_Port, BT_UP_Pin) == 0) {
                            HAL_Delay(50);
                            if (HAL_GPIO_ReadPin(BT_UP_GPIO_Port, BT_UP_Pin) == 0) {
                                velocidadeAC = velocidadeAC + 260;
                                if (velocidadeAC > 1300) {
                                    velocidadeAC = 1300;
                                }
                                while(HAL_GPIO_ReadPin(BT_UP_GPIO_Port, BT_UP_Pin) == 0);
                                HAL_Delay(200);
                            }
                        }

                        if (HAL_GPIO_ReadPin(BT_DOWN_GPIO_Port, BT_DOWN_Pin) == 0) {
                            HAL_Delay(50);
                            if (HAL_GPIO_ReadPin(BT_DOWN_GPIO_Port, BT_DOWN_Pin) == 0) {
                                velocidadeAC = velocidadeAC - 260;
                                if(velocidadeAC < 130){
                                    velocidadeAC = 130;
                                }
                                while(HAL_GPIO_ReadPin(BT_DOWN_GPIO_Port, BT_DOWN_Pin) == 0);
                                HAL_Delay(200);
                            }
                        }

                        HAL_GPIO_TogglePin(LED_MENU2_GPIO_Port, LED_MENU2_Pin);
                        HAL_Delay(300);

                        atualizarBrilhoLED();
                        atualizarLCD();
                        acionarRetorno();
                    }
                }

                if (menup == 3){
                    while(retorno == 1){
                        if (HAL_GPIO_ReadPin(BT_UP_GPIO_Port, BT_UP_Pin) == 0) {
                            HAL_Delay(50);
                            if (HAL_GPIO_ReadPin(BT_UP_GPIO_Port, BT_UP_Pin) == 0) {
                                velocidadeDC = velocidadeDC + 260;
                                if (velocidadeDC > 1300) {
                                    velocidadeDC = 1300;
                                }
                                while(HAL_GPIO_ReadPin(BT_UP_GPIO_Port, BT_UP_Pin) == 0);
                                HAL_Delay(200);
                            }
                        }

                        if (HAL_GPIO_ReadPin(BT_DOWN_GPIO_Port, BT_DOWN_Pin) == 0) {
                            HAL_Delay(50);
                            if (HAL_GPIO_ReadPin(BT_DOWN_GPIO_Port, BT_DOWN_Pin) == 0) {
                                velocidadeDC = velocidadeDC - 260;
                                if(velocidadeDC < 130){
                                    velocidadeDC = 130;
                                }
                                while(HAL_GPIO_ReadPin(BT_DOWN_GPIO_Port, BT_DOWN_Pin) == 0);
                                HAL_Delay(200);
                            }
                        }

                        HAL_GPIO_TogglePin(LED_MENU3_GPIO_Port, LED_MENU3_Pin);
                        HAL_Delay(300);

                        atualizarBrilhoLED();
                        atualizarLCD();
                        acionarRetorno();
                    }
                }

                if (menup == 4){
                    while(retorno == 1){
                        HAL_GPIO_TogglePin(LED_MENU4_GPIO_Port, LED_MENU4_Pin);
                        HAL_Delay(100);

                        atualizarBrilhoLED();
                        atualizarLCD();
                        acionarRetorno();
                    }
                }
            }
        }

        if (emergencia_ativa == 1 && HAL_GPIO_ReadPin(BT_RETURN_GPIO_Port, BT_RETURN_Pin) == 0) {
            HAL_Delay(50);
            if (HAL_GPIO_ReadPin(BT_RETURN_GPIO_Port, BT_RETURN_Pin) == 0) {
                emergencia_ativa = 0;
                desligarTodosLEDsMenu();

                menup = 1;
                atualizarLEDsMenu();


                HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);

                while(HAL_GPIO_ReadPin(BT_RETURN_GPIO_Port, BT_RETURN_Pin) == 0);
                HAL_Delay(300);
            }
        }

        // UP
        if (HAL_GPIO_ReadPin(BT_UP_GPIO_Port, BT_UP_Pin) == 0) {
            HAL_Delay(50);
            if (HAL_GPIO_ReadPin(BT_UP_GPIO_Port, BT_UP_Pin) == 0) {
                if (start == 1) {
                    periodotimer -= 520;
                    if (periodotimer < 280) periodotimer = 280;
                    __HAL_TIM_SET_AUTORELOAD(&htim4, periodotimer);

                    rpm_desejado = 3000 - ((periodotimer - 280) * 3000 / (7300 - 280));
                    adc2_0 = (rpm_desejado * 4095) / 3000;
                }
                while(HAL_GPIO_ReadPin(BT_UP_GPIO_Port, BT_UP_Pin) == 0);
            }
        }

        // DOWN
        if (HAL_GPIO_ReadPin(BT_DOWN_GPIO_Port, BT_DOWN_Pin) == 0) {
            HAL_Delay(50);
            if (HAL_GPIO_ReadPin(BT_DOWN_GPIO_Port, BT_DOWN_Pin) == 0) {
                if (start == 1) {
                    periodotimer += 520;
                    if (periodotimer > 7300) periodotimer = 7300;
                    __HAL_TIM_SET_AUTORELOAD(&htim4, periodotimer);

                    rpm_desejado = 3000 - ((periodotimer - 280) * 3000 / (7300 - 280));
                    adc2_0 = (rpm_desejado * 4095) / 3000;
                }
                while(HAL_GPIO_ReadPin(BT_DOWN_GPIO_Port, BT_DOWN_Pin) == 0);
            }
        }

        static int ultimo_menu = 0;
        if (ultimo_menu != menup) {
            atualizarLEDsMenu();
            ultimo_menu = menup;
        }

        atualizarBrilhoLED();
        atualizarLCD();
        gerenciarEmergencia();
        HAL_Delay(10);
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
  sConfig.Channel = ADC_CHANNEL_1;
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
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 14400;
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
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
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

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 14400;
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
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
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
  htim4.Init.Prescaler = 7200;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 4000;
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
  HAL_GPIO_WritePin(LED_KIT_GPIO_Port, LED_KIT_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LED_MENU1_Pin|LED_MENU2_Pin|LED_MENU3_Pin|LED_MENU4_Pin
                          |STATUS1_Pin|STATUS2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, STATUS3_Pin|STATUS4_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : LED_KIT_Pin */
  GPIO_InitStruct.Pin = LED_KIT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_KIT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PROTECAO_Pin */
  GPIO_InitStruct.Pin = PROTECAO_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(PROTECAO_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : BT_RETURN_Pin BT_UP_Pin BT_DOWN_Pin BT_OK_Pin
                           BT_START_Pin BT_SENTIDO_Pin BT_STOP_Pin */
  GPIO_InitStruct.Pin = BT_RETURN_Pin|BT_UP_Pin|BT_DOWN_Pin|BT_OK_Pin
                          |BT_START_Pin|BT_SENTIDO_Pin|BT_STOP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : LED_MENU1_Pin LED_MENU2_Pin LED_MENU3_Pin LED_MENU4_Pin
                           STATUS1_Pin STATUS2_Pin */
  GPIO_InitStruct.Pin = LED_MENU1_Pin|LED_MENU2_Pin|LED_MENU3_Pin|LED_MENU4_Pin
                          |STATUS1_Pin|STATUS2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : STATUS3_Pin STATUS4_Pin */
  GPIO_InitStruct.Pin = STATUS3_Pin|STATUS4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  // Configurar PA1, PA2, PA4 como analógicos para ADC
  GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  // Configurar PA6 (SCL) e PA7 (SDA) para I2C - MODIFICAÇÃO IMPORTANTE!
  GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if(GPIO_Pin == PROTECAO_Pin)
    {
        emergencia_ativa = 1;
        start = 0;
        retorno = 1;
        i = 0;

        velocidadeAC = velocidadeAC - (velocidadeAC * 0.1);
        if (velocidadeAC < 130) velocidadeAC = 130;

        htim2.Instance->CCR1 = 0;
        htim2.Instance->CCR4 = 0;
        htim3.Instance->CCR1 = 0;
        htim3.Instance->CCR2 = 0;
        htim3.Instance->CCR3 = 0;

        pisca_emergencia = 1;
        tempo_proxima_pisca = HAL_GetTick() + 500;
        tempo_pisca_b7 = HAL_GetTick() + 250;
        pisca_b7_estado = 0;

        HAL_GPIO_WritePin(GPIOB, STATUS3_Pin, GPIO_PIN_RESET);


        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET);
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
