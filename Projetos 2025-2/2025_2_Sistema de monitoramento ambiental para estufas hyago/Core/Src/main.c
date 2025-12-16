/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Controle de estufa
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "i2c.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define DEBOUNCE_TIME       200U
#define ADC_MAX_VALUE       4095.0f
#define VREF                3.3f
#define WINDOW_SIZE         10
#define NUM_ADC_CHANNELS    4    // PA0, PA1, PA2, PA3

// LCD I2C
#define LCD_ADDR   (0x27u << 1)
#define LCD_COLS   16
#define LCD_ROWS   2

#define RS_BIT     0x01
#define EN_BIT     0x04
#define BL_ON      0x08
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
Estado_Controlador g_estado_atual = MONITORAMENTO_PRINCIPAL;
Modo_Operacao     g_modo_operacao = MODO_AUTOMATICO;
Controlador_Dados g_dados_controlador = {0};

Estado_Controlador estadoAtual = MENU_MODO; // passa a iniciar no menu (pedido)

// Menu / setpoints
Config_Setpoints_Index config_index_selecionado = SP_TEMP_MAX;

// Botão emergência
volatile bool flag_emergencia = false;

// Debounce dos botões
uint32_t ultimo_press_verde_ms   = 0;
uint32_t ultimo_press_amarelo_ms = 0;
uint32_t ultimo_press_roxo_ms    = 0;

// ADC e filtragem
volatile uint16_t adc_buffer[NUM_ADC_CHANNELS];

float temp_hist[WINDOW_SIZE] = {0};
float umid_hist[WINDOW_SIZE] = {0};
float o2_hist[WINDOW_SIZE]   = {0};
float luz_hist[WINDOW_SIZE]  = {0};
int   hist_idx = 0;

int temp_i = 0, umid_i = 0, o2_i = 0, luz_i = 0;

// ---------- SETPOINTS DE FÁBRICA ----------
static const float sp_temp_max_factory = 25.0f;
static const float sp_temp_min_factory = 18.0f;
static const float sp_umid_max_factory = 85.0f;
static const float sp_umid_min_factory = 60.0f;
static const float sp_o2_max_factory   = 21.0f;
static const float sp_o2_min_factory   = 19.0f;
static const float sp_luz_max_factory  = 45.0f;
static const float sp_luz_min_factory  = 20.0f;

// ---------- SETPOINTS ATUAIS (Para alterar no modo manual) ----------
float sp_temp_max   = 25.0f;
float sp_temp_min   = 18.0f;
float sp_umid_max   = 85.0f;
float sp_umid_min   = 60.0f;
float sp_o2_max     = 21.0f;
float sp_o2_min     = 19.0f;
float sp_luz_max    = 45.0f;
float sp_luz_min    = 20.0f;

// ---------- CONTROLE DO BUZZER POR TEMPORIZADOR (TIM2) ----------
bool     buzzer_temp_alerta   = false;   // se true, TIM2 controla o buzzer
uint32_t buzzer_periodo_ms    = 300;     // 300 ms abaixo do mínimo, 150 ms acima do máximo
uint32_t buzzer_elapsed_ms    = 0;       // acumula tempo desde última troca
bool     buzzer_toggle_state  = false;   // estado atual (ON/OFF) do beep

// ---------- NOVA LÓGICA DE MENU ----------
int menu_index = 0;
bool in_submenu = false;
enum {
    SUB_NONE = 0,
    SUB_CONFIRM_AUTO,
    SUB_AJUSTE_TEMP,
    SUB_AJUSTE_UMI,
    SUB_AJUSTE_O2,
    SUB_AJUSTE_LUZ,
    SUB_MONITORAMENTO_SELECTED
} submenu_type = SUB_NONE;
// Ajuste step (0=max, 1=min)
int ajuste_step = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
// LCD
void LCD_Write4bits(uint8_t data);
void LCD_SendCommand(uint8_t cmd);
void LCD_SendData(uint8_t data);
void LCD_SendString(char *str);
void LCD_SetCursor(uint8_t row, uint8_t col);
void LCD_Clear(void);
void LCD_Init(void);

// App
void Display_Update(void);
void Transicao_FSM(void);
void Maquina_Monitoramento(void);
void Maquina_ModoOperacao(void);
void Maquina_ConfigSetpoints(void);
void Maquina_AlertaCritico(void);

// Util
void Carregar_Setpoints(void);
void Salvar_Setpoints(void);
void Restaurar_Setpoints_Fabrica(void);
void Ligar_Aquecimento(void);
void Desligar_Aquecimento(void);
void Ligar_Resfriamento(void);
void Desligar_Resfriamento(void);

// SERVO
void Servo_SetPWM(uint16_t pulse_us);
void Servo_Para(void);

// BOTÕES
bool BotaoVerde_Pressionado(void);
bool BotaoAmarelo_Pressionado(void);
bool BotaoRoxo_Pressionado(void);

// ADC / conversões
void  Processar_ADC_e_Filtrar(void);
float Converter_ADC_para_Volts(uint16_t raw);

// BUZZER
void Buzzer_Liga(void);
void Buzzer_Desliga(void);
void Buzzer_TimerHandler(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* ===================== LCD I2C ===================== */

void LCD_Write4bits(uint8_t data)
{
    uint8_t buf[1];
    buf[0] = data | BL_ON | EN_BIT;
    HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR, buf, 1, 100);
    HAL_Delay(1);
    buf[0] = data | BL_ON;
    HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR, buf, 1, 100);
}

void LCD_SendCommand(uint8_t cmd)
{
    uint8_t hi = cmd & 0xF0;
    uint8_t lo = (uint8_t)((cmd << 4) & 0xF0);

    LCD_Write4bits(hi);
    LCD_Write4bits(lo);
}

void LCD_SendData(uint8_t data)
{
    uint8_t hi = data & 0xF0;
    uint8_t lo = (uint8_t)((data << 4) & 0xF0);

    LCD_Write4bits(hi | RS_BIT);
    LCD_Write4bits(lo | RS_BIT);
}

void LCD_SendString(char *str)
{
    while (*str)
    {
        LCD_SendData((uint8_t)*str++);
    }
}

void LCD_SetCursor(uint8_t row, uint8_t col)
{
    uint8_t addr;
    switch (row)
    {
        default:
        case 0: addr = 0x80 + col; break;
        case 1: addr = 0xC0 + col; break;
    }
    LCD_SendCommand(addr);
}

void LCD_Clear(void)
{
    LCD_SendCommand(0x01);
    HAL_Delay(2);
}

void LCD_Init(void)
{
    HAL_Delay(20);
    LCD_Write4bits(0x30);
    HAL_Delay(10);
    LCD_Write4bits(0x30);
    LCD_Write4bits(0x20);
    LCD_SendCommand(0x28);
    LCD_SendCommand(0x0C);
    LCD_SendCommand(0x01);
    HAL_Delay(2);
}

/* ===================== Atuadores ===================== */

void Carregar_Setpoints(void)
{
    g_dados_controlador.sp_temp_max     = sp_temp_max;
    g_dados_controlador.sp_temp_min     = sp_temp_min;
    g_dados_controlador.sp_umidade_max  = sp_umid_max;
    g_dados_controlador.sp_umidade_min  = sp_umid_min;
    g_dados_controlador.sp_oxigenio_max = sp_o2_max;
    g_dados_controlador.sp_oxigenio_min = sp_o2_min;
}

void Salvar_Setpoints(void)
{
    Carregar_Setpoints();
}

void Restaurar_Setpoints_Fabrica(void)
{
    sp_temp_max = sp_temp_max_factory;
    sp_temp_min = sp_temp_min_factory;
    sp_umid_max = sp_umid_max_factory;
    sp_umid_min = sp_umid_min_factory;
    sp_o2_max   = sp_o2_max_factory;
    sp_o2_min   = sp_o2_min_factory;
    sp_luz_max  = sp_luz_max_factory;
    sp_luz_min  = sp_luz_min_factory;

    Carregar_Setpoints();
}

void Ligar_Aquecimento(void)
{
    HAL_GPIO_WritePin(AQUECIMENTO_GPIO_Port, AQUECIMENTO_Pin, GPIO_PIN_SET);
}

void Desligar_Aquecimento(void)
{
    HAL_GPIO_WritePin(AQUECIMENTO_GPIO_Port, AQUECIMENTO_Pin, GPIO_PIN_RESET);
}

void Ligar_Resfriamento(void)
{
    HAL_GPIO_WritePin(RESFRIAMENTO_GPIO_Port, RESFRIAMENTO_Pin, GPIO_PIN_SET);
}

void Desligar_Resfriamento(void)
{
    HAL_GPIO_WritePin(RESFRIAMENTO_GPIO_Port, RESFRIAMENTO_Pin, GPIO_PIN_RESET);
}

/* ===================== SERVO ===================== */

void Servo_SetPWM(uint16_t pulse_us)
{
    if (pulse_us < 1000) pulse_us = 1000;
    if (pulse_us > 2000) pulse_us = 2000;

    uint32_t ticks = pulse_us;

    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, ticks);
}

void Servo_Para(void)
{
    Servo_SetPWM(1500);
}

/* ===================== BOTÕES ===================== */

bool BotaoVerde_Pressionado(void)
{
    GPIO_PinState p = HAL_GPIO_ReadPin(BOTAO_VERDE_GPIO_Port, BOTAO_VERDE_Pin);
    static GPIO_PinState last = GPIO_PIN_SET;

    if (p == GPIO_PIN_RESET && last == GPIO_PIN_SET)
    {
        if (HAL_GetTick() - ultimo_press_verde_ms > DEBOUNCE_TIME)
        {
            ultimo_press_verde_ms = HAL_GetTick();
            last = p;
            return true;
        }
    }
    if (p == GPIO_PIN_SET)
        last = p;

    return false;
}

bool BotaoAmarelo_Pressionado(void)
{
    GPIO_PinState p = HAL_GPIO_ReadPin(BOTAO_AMARELO_GPIO_Port, BOTAO_AMARELO_Pin);
    static GPIO_PinState last = GPIO_PIN_SET;

    if (p == GPIO_PIN_RESET && last == GPIO_PIN_SET)
    {
        if (HAL_GetTick() - ultimo_press_amarelo_ms > DEBOUNCE_TIME)
        {
            ultimo_press_amarelo_ms = HAL_GetTick();
            last = p;
            return true;
        }
    }
    if (p == GPIO_PIN_SET)
        last = p;

    return false;
}

bool BotaoRoxo_Pressionado(void)
{
    GPIO_PinState p = HAL_GPIO_ReadPin(BOTAO_ROXO_GPIO_Port, BOTAO_ROXO_Pin);
    static GPIO_PinState last = GPIO_PIN_SET;

    if (p == GPIO_PIN_RESET && last == GPIO_PIN_SET)
    {
        if (HAL_GetTick() - ultimo_press_roxo_ms > DEBOUNCE_TIME)
        {
            ultimo_press_roxo_ms = HAL_GetTick();
            last = p;
            return true;
        }
    }
    if (p == GPIO_PIN_SET)
        last = p;

    return false;
}

/* ===================== BUZZER ===================== */

void Buzzer_Liga(void)
{
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_SET);
}

void Buzzer_Desliga(void)
{
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);
}

void Buzzer_TimerHandler(void)
{
    if (!buzzer_temp_alerta || flag_emergencia || estadoAtual == ALERTA_CRITICO)
        return;

       buzzer_elapsed_ms += 100U;

    if (buzzer_elapsed_ms >= buzzer_periodo_ms)
    {
        buzzer_elapsed_ms = 0;
        buzzer_toggle_state = !buzzer_toggle_state;

        if (buzzer_toggle_state)
            Buzzer_Liga();
        else
            Buzzer_Desliga();
    }
}

/* ===================== ADC / FILTRO ===================== */

float Converter_ADC_para_Volts(uint16_t raw)
{
    return (raw / ADC_MAX_VALUE) * VREF;
}

static float limpa_ruido_adc(uint16_t raw)
{
    if (raw < 50)   raw = 50;
    if (raw > 4050) raw = 4050;
    return raw;
}

void Processar_ADC_e_Filtrar(void)
{
    const uint16_t ADC_MIN = 50;
    const uint16_t ADC_MAX = 4050;
    const float span = (float)(ADC_MAX - ADC_MIN);

    float temp_raw = (adc_buffer[0] < ADC_MIN) ? ADC_MIN :
                  (adc_buffer[0] > ADC_MAX ? ADC_MAX : adc_buffer[0]);

    float umi_raw = (adc_buffer[1] < ADC_MIN) ? ADC_MIN :
                  (adc_buffer[1] > ADC_MAX ? ADC_MAX : adc_buffer[1]);

    float oxi_raw = (adc_buffer[2] < ADC_MIN) ? ADC_MIN :
                  (adc_buffer[2] > ADC_MAX ? ADC_MAX : adc_buffer[2]);

    float luz_raw = (adc_buffer[3] < ADC_MIN) ? ADC_MIN :
                  (adc_buffer[3] > ADC_MAX ? ADC_MAX : adc_buffer[3]);

    float temp_n = (temp_raw - ADC_MIN) / span;
    float umi_n  = (umi_raw  - ADC_MIN) / span;
    float oxi_n  = (oxi_raw  - ADC_MIN) / span;
    float luz_n  = (luz_raw  - ADC_MIN) / span;

    float temp = 10.0f + temp_n * (35.0f - 10.0f);   // 10 a 35 °C
    float umi  = 40.0f + umi_n  * (99.0f - 40.0f);   // 40 a 99 %
    float oxi  = 15.0f + oxi_n  * (25.0f - 15.0f);   // 15 a 25 %
    float lux  = luz_n;

    temp_hist[hist_idx] = temp;
    umid_hist[hist_idx] = umi;
    o2_hist[hist_idx]   = oxi;
    luz_hist[hist_idx]  = lux;

    hist_idx = (hist_idx + 1) % WINDOW_SIZE;

    float stemp = 0, sumi = 0, soxi = 0, sluz = 0;
    for (int i = 0; i < WINDOW_SIZE; i++) {
        stemp += temp_hist[i];
        sumi  += umid_hist[i];
        soxi  += o2_hist[i];
        sluz  += luz_hist[i];
    }

    float temp_media = stemp / WINDOW_SIZE;   // 10–35
    float umid_media = sumi  / WINDOW_SIZE;   // 40–99
    float o2_media   = soxi  / WINDOW_SIZE;   // 15–25
    float luz_media  = sluz  / WINDOW_SIZE;   // 0–1

    temp_i = (int)(temp_media + 0.5f);
    umid_i = (int)(umid_media + 0.5f);
    o2_i   = (int)(o2_media   + 0.5f);

    int luz_calc = (int)(luz_media * 99.0f + 0.5f);
    if (luz_calc < 0)   luz_calc = 0;
    if (luz_calc > 99)  luz_calc = 99;
    luz_i = luz_calc;

    g_dados_controlador.temperatura = temp_media;
    g_dados_controlador.umidade     = umid_media;
    g_dados_controlador.oxigenio    = o2_media;
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1)
    {
        Processar_ADC_e_Filtrar();
        HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_buffer, NUM_ADC_CHANNELS);
    }
}

/* ===================== DISPLAY ===================== */

void Display_Update(void)
{
    char buf[17];

       if (estadoAtual == MENU_MODO && !in_submenu)
    {
                LCD_SetCursor(0,0);
        switch (menu_index)
        {
            case 0: LCD_SendString("1:Monitoramento "); break;
            case 1: LCD_SendString("2:Modo Automatico"); break;
            case 2: LCD_SendString("3:Ajuste Temp   "); break;
            case 3: LCD_SendString("4:Ajuste Umid   "); break;
            case 4: LCD_SendString("5:Ajuste O2     "); break;
            case 5: LCD_SendString("6:Ajuste Luz    "); break;
            default: LCD_SendString("                "); break;
        }
        LCD_SetCursor(1,0);
        LCD_SendString("ROXO-> Next  VER->OK");
        return;
    }

        if (estadoAtual == MENU_MODO && in_submenu)
    {
        switch (submenu_type)
        {
            case SUB_MONITORAMENTO_SELECTED:
                LCD_SetCursor(0,0);
                snprintf(buf,sizeof(buf),"Temp:%02dC Umi:%02d%%",temp_i,umid_i);
                LCD_SendString(buf);
                LCD_SetCursor(1,0);
                snprintf(buf,sizeof(buf),"Oxig:%02d%% Luz:%02d%%",o2_i,luz_i);
                LCD_SendString(buf);
                LCD_SetCursor(1,14);
                return;

            case SUB_CONFIRM_AUTO:
                LCD_SetCursor(0,0);
                LCD_SendString("Confirma AUTO ?  ");
                LCD_SetCursor(1,0);
                LCD_SendString("VER->Sim  ROXO->Nao");
                return;

            case SUB_AJUSTE_TEMP:
                LCD_SetCursor(0,0);
                if (ajuste_step == 0)
                    snprintf(buf,sizeof(buf),"Tmax:%02dC        ", (int)sp_temp_max);
                else
                    snprintf(buf,sizeof(buf),"Tmin:%02dC        ", (int)sp_temp_min);
                LCD_SendString(buf);
                LCD_SetCursor(1,0);
                LCD_SendString("VER+ AMAR- ROXO->>");
                return;

            case SUB_AJUSTE_UMI:
                LCD_SetCursor(0,0);
                if (ajuste_step == 0)
                    snprintf(buf,sizeof(buf),"Umax:%02d%%       ", (int)sp_umid_max);
                else
                    snprintf(buf,sizeof(buf),"Umin:%02d%%       ", (int)sp_umid_min);
                LCD_SendString(buf);
                LCD_SetCursor(1,0);
                LCD_SendString("VER+ AMAR- ROXO->>");
                return;

            case SUB_AJUSTE_O2:
                LCD_SetCursor(0,0);
                if (ajuste_step == 0)
                    snprintf(buf,sizeof(buf),"O2Max:%02d%%      ", (int)sp_o2_max);
                else
                    snprintf(buf,sizeof(buf),"O2Min:%02d%%      ", (int)sp_o2_min);
                LCD_SendString(buf);
                LCD_SetCursor(1,0);
                LCD_SendString("VER+ AMAR- ROXO->>");
                return;

            case SUB_AJUSTE_LUZ:
                LCD_SetCursor(0,0);
                if (ajuste_step == 0)
                    snprintf(buf,sizeof(buf),"LuzMax:%02d%%     ", (int)sp_luz_max);
                else
                    snprintf(buf,sizeof(buf),"LuzMin:%02d%%     ", (int)sp_luz_min);
                LCD_SendString(buf);
                LCD_SetCursor(1,0);
                LCD_SendString("VER+ AMAR- ROXO->>");
                return;

            default:
                LCD_Clear();
                return;
        }
    }

    switch (estadoAtual)
    {
    case MONITORAMENTO_PRINCIPAL:
        LCD_SetCursor(0,0);
        snprintf(buf,sizeof(buf),"Temp:%02dC Umi:%02d%%",temp_i,umid_i);
        LCD_SendString(buf);
        LCD_SetCursor(1,0);
        snprintf(buf,sizeof(buf),"Oxig:%02d%% Luz:%02d%%",o2_i,luz_i);
        LCD_SendString(buf);
        break;

    case ALERTA_CRITICO:
        break;

    default:
        break;
    }
}

/* ===================== FSM ===================== */

void Maquina_Monitoramento(void)
{
    // ======== CONTROLE TEMPERATURA ========
    if (temp_i > (int)sp_temp_max)
    {
        Desligar_Aquecimento();
        Ligar_Resfriamento();
    }
    else if (temp_i < (int)sp_temp_min)
    {
        Ligar_Aquecimento();
        Desligar_Resfriamento();
    }
    else
    {
        Desligar_Aquecimento();
        Desligar_Resfriamento();
    }

    // Controle do buzzer baseado na temperatura (TIM2)
    if (g_modo_operacao == MODO_AUTOMATICO && !flag_emergencia)
    {
        if (temp_i < (int)sp_temp_min)
        {
            buzzer_temp_alerta = true;
            buzzer_periodo_ms  = 300;
        }
        else if (temp_i > (int)sp_temp_max)
        {
            buzzer_temp_alerta = true;
            buzzer_periodo_ms  = 150;
        }
        else
        {
            buzzer_temp_alerta = false;
            buzzer_elapsed_ms  = 0;
            buzzer_toggle_state = false;
            Buzzer_Desliga();
        }
    }
    else
    {
        buzzer_temp_alerta = false;
        buzzer_elapsed_ms  = 0;
        buzzer_toggle_state = false;
        Buzzer_Desliga();
    }

    // ========= CONTROLE UMIDADE → LED RGB =========
    if (umid_i < (int)sp_umid_min)
    {
        HAL_GPIO_WritePin(LED_RGB_VERMELHO_GPIO_Port, LED_RGB_VERMELHO_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(LED_RGB_VERDE_GPIO_Port,    LED_RGB_VERDE_Pin,    GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED_RGB_AZUL_GPIO_Port,     LED_RGB_AZUL_Pin,     GPIO_PIN_RESET);
    }
    else if (umid_i <= (int)sp_umid_max)
    {
        HAL_GPIO_WritePin(LED_RGB_VERMELHO_GPIO_Port, LED_RGB_VERMELHO_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED_RGB_VERDE_GPIO_Port,    LED_RGB_VERDE_Pin,    GPIO_PIN_SET);
        HAL_GPIO_WritePin(LED_RGB_AZUL_GPIO_Port,     LED_RGB_AZUL_Pin,     GPIO_PIN_RESET);
    }
    else
    {
        HAL_GPIO_WritePin(LED_RGB_VERMELHO_GPIO_Port, LED_RGB_VERMELHO_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED_RGB_VERDE_GPIO_Port,    LED_RGB_VERDE_Pin,    GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED_RGB_AZUL_GPIO_Port,     LED_RGB_AZUL_Pin,     GPIO_PIN_SET);
    }

    // ======== CONTROLE OXIGÊNIO → SERVO =========
    if (o2_i < (int)sp_o2_min)
    {
        float falta = sp_o2_min - o2_i;
        float ganho = falta / 15.0f;
        if (ganho > 1.0f) ganho = 1.0f;

        uint16_t pwm = 1500 + (uint16_t)(ganho * 500);
        Servo_SetPWM(pwm);
    }
    else if (o2_i > (int)sp_o2_max)
    {
        float excesso = o2_i - sp_o2_max;
        float ganho   = excesso / 10.0f;
        if (ganho > 1.0f) ganho = 1.0f;

        uint16_t pwm = 1500 - (uint16_t)(ganho * 500);
        Servo_SetPWM(pwm);
    }
    else
    {
        Servo_Para();
    }

    // ======== CONTROLE LUMINOSIDADE → LEDs PA4/PA5 =========
    if (luz_i < (int)sp_luz_min)
    {
        HAL_GPIO_WritePin(POUCA_LUZ_GPIO_Port, POUCA_LUZ_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(MUITA_LUZ_GPIO_Port, MUITA_LUZ_Pin, GPIO_PIN_RESET);
    }
    else if (luz_i > (int)sp_luz_max)
    {
        HAL_GPIO_WritePin(POUCA_LUZ_GPIO_Port, POUCA_LUZ_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(MUITA_LUZ_GPIO_Port, MUITA_LUZ_Pin, GPIO_PIN_SET);
    }
    else
    {
        HAL_GPIO_WritePin(POUCA_LUZ_GPIO_Port, POUCA_LUZ_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(MUITA_LUZ_GPIO_Port, MUITA_LUZ_Pin, GPIO_PIN_RESET);
    }

       if (BotaoRoxo_Pressionado())
    {
        estadoAtual = MENU_MODO;
        in_submenu = false;
        submenu_type = SUB_NONE;
        menu_index = 0;
        LCD_Clear();
    }
}

void Maquina_ModoOperacao(void)
{

    if (!in_submenu)
    {
        if (BotaoRoxo_Pressionado())
        {
            menu_index = (menu_index + 1) % 6; // 6 opções
            LCD_Clear();
            return;
        }
        // VERDE: entrar/selecionar a opção atual
        if (BotaoVerde_Pressionado())
        {
            in_submenu = true;
            ajuste_step = 0; // reset passo de ajuste
            switch (menu_index)
            {
                case 0: // Monitoramento
                    submenu_type = SUB_MONITORAMENTO_SELECTED;
                    // permanece em estado MENU_MODO mas mostra a tela de monitoramento até ROXO voltar
                    LCD_Clear();
                    break;
                case 1: // Modo Automático -> confirmar
                    submenu_type = SUB_CONFIRM_AUTO;
                    LCD_Clear();
                    break;
                case 2: // Ajuste Temperatura
                    submenu_type = SUB_AJUSTE_TEMP;
                    ajuste_step = 0;
                    LCD_Clear();
                    break;
                case 3: // Ajuste Umidade
                    submenu_type = SUB_AJUSTE_UMI;
                    ajuste_step = 0;
                    LCD_Clear();
                    break;
                case 4: // Ajuste Oxigênio
                    submenu_type = SUB_AJUSTE_O2;
                    ajuste_step = 0;
                    LCD_Clear();
                    break;
                case 5: // Ajuste Luminosidade
                    submenu_type = SUB_AJUSTE_LUZ;
                    ajuste_step = 0;
                    LCD_Clear();
                    break;
                default:
                    submenu_type = SUB_NONE;
                    in_submenu = false;
                    break;
            }
            return;
        }
    }
    else
    {
        // Estamos dentro de um submenu; tratar ações específicas
        switch (submenu_type)
        {
            case SUB_MONITORAMENTO_SELECTED:
                // Dentro desta tela: ROXO volta pro menu (sai do submenu)
                if (BotaoRoxo_Pressionado())
                {
                    in_submenu = false;
                    submenu_type = SUB_NONE;
                    LCD_Clear();
                }
                break;

            case SUB_CONFIRM_AUTO:
                // VERDE = confirmar, ROXO = cancelar (voltar ao menu)
                if (BotaoVerde_Pressionado())
                {
                    // confirma modo automático e restaura presets
                    g_modo_operacao = MODO_AUTOMATICO;
                    Restaurar_Setpoints_Fabrica();
                    // volta para tela de monitoramento principal (requisito interpretado)
                    estadoAtual = MONITORAMENTO_PRINCIPAL;
                    in_submenu = false;
                    submenu_type = SUB_NONE;
                    LCD_Clear();
                }
                else if (BotaoRoxo_Pressionado())
                {
                    // cancelar: volta ao menu principal, próximo item (ou manter ítem)
                    in_submenu = false;
                    submenu_type = SUB_NONE;
                    LCD_Clear();
                }
                break;

            case SUB_AJUSTE_TEMP:
            case SUB_AJUSTE_UMI:
            case SUB_AJUSTE_O2:
            case SUB_AJUSTE_LUZ:
            {
                // VERDE aumenta, AMARELO diminui, ROXO avança passo/fecha
                if (BotaoVerde_Pressionado())
                {
                    if (submenu_type == SUB_AJUSTE_TEMP)
                    {
                        if (ajuste_step == 0) { sp_temp_max += 1.0f; if (sp_temp_max > 35.0f) sp_temp_max = 35.0f; if (sp_temp_max < sp_temp_min) sp_temp_max = sp_temp_min; }
                        else { sp_temp_min += 1.0f; if (sp_temp_min > sp_temp_max) sp_temp_min = sp_temp_max; if (sp_temp_min > 35.0f) sp_temp_min = 35.0f; }
                    }
                    else if (submenu_type == SUB_AJUSTE_UMI)
                    {
                        if (ajuste_step == 0) { sp_umid_max += 1.0f; if (sp_umid_max > 99.0f) sp_umid_max = 99.0f; if (sp_umid_max < sp_umid_min) sp_umid_max = sp_umid_min; }
                        else { sp_umid_min += 1.0f; if (sp_umid_min > sp_umid_max) sp_umid_min = sp_umid_max; if (sp_umid_min > 99.0f) sp_umid_min = 99.0f; }
                    }
                    else if (submenu_type == SUB_AJUSTE_O2)
                    {
                        if (ajuste_step == 0) { sp_o2_max += 1.0f; if (sp_o2_max > 25.0f) sp_o2_max = 25.0f; if (sp_o2_max < sp_o2_min) sp_o2_max = sp_o2_min; }
                        else { sp_o2_min += 1.0f; if (sp_o2_min > sp_o2_max) sp_o2_min = sp_o2_max; if (sp_o2_min > 25.0f) sp_o2_min = 25.0f; }
                    }
                    else if (submenu_type == SUB_AJUSTE_LUZ)
                    {
                        if (ajuste_step == 0) { sp_luz_max += 1.0f; if (sp_luz_max > 99.0f) sp_luz_max = 99.0f; if (sp_luz_max < sp_luz_min) sp_luz_max = sp_luz_min; }
                        else { sp_luz_min += 1.0f; if (sp_luz_min > sp_luz_max) sp_luz_min = sp_luz_max; if (sp_luz_min > 99.0f) sp_luz_min = 99.0f; }
                    }
                    LCD_Clear();
                }
                if (BotaoAmarelo_Pressionado())
                {
                    if (submenu_type == SUB_AJUSTE_TEMP)
                    {
                        if (ajuste_step == 0) { sp_temp_max -= 1.0f; if (sp_temp_max < sp_temp_min) sp_temp_max = sp_temp_min; if (sp_temp_max < 10.0f) sp_temp_max = 10.0f; }
                        else { sp_temp_min -= 1.0f; if (sp_temp_min < 10.0f) sp_temp_min = 10.0f; if (sp_temp_min > sp_temp_max) sp_temp_min = sp_temp_max; }
                    }
                    else if (submenu_type == SUB_AJUSTE_UMI)
                    {
                        if (ajuste_step == 0) { sp_umid_max -= 1.0f; if (sp_umid_max < sp_umid_min) sp_umid_max = sp_umid_min; if (sp_umid_max < 40.0f) sp_umid_max = 40.0f; }
                        else { sp_umid_min -= 1.0f; if (sp_umid_min < 40.0f) sp_umid_min = 40.0f; if (sp_umid_min > sp_umid_max) sp_umid_min = sp_umid_max; }
                    }
                    else if (submenu_type == SUB_AJUSTE_O2)
                    {
                        if (ajuste_step == 0) { sp_o2_max -= 1.0f; if (sp_o2_max < sp_o2_min) sp_o2_max = sp_o2_min; if (sp_o2_max < 15.0f) sp_o2_max = 15.0f; }
                        else { sp_o2_min -= 1.0f; if (sp_o2_min < 15.0f) sp_o2_min = 15.0f; if (sp_o2_min > sp_o2_max) sp_o2_min = sp_o2_max; }
                    }
                    else if (submenu_type == SUB_AJUSTE_LUZ)
                    {
                        if (ajuste_step == 0) { sp_luz_max -= 1.0f; if (sp_luz_max < sp_luz_min) sp_luz_max = sp_luz_min; if (sp_luz_max < 0.0f) sp_luz_max = 0.0f; }
                        else { sp_luz_min -= 1.0f; if (sp_luz_min < 0.0f) sp_luz_min = 0.0f; if (sp_luz_min > sp_luz_max) sp_luz_min = sp_luz_max; }
                    }
                    LCD_Clear();
                }
                if (BotaoRoxo_Pressionado())
                {
                    if (ajuste_step == 0)
                    {
                        // vai para ajuste do mínimo
                        ajuste_step = 1;
                        LCD_Clear();
                    }
                    else
                    {
                        // terminou ajuste: salva (espelha) e volta ao menu principal posicao 0
                        Salvar_Setpoints();
                        in_submenu = false;
                        submenu_type = SUB_NONE;
                        ajuste_step = 0;
                        menu_index = 0; // voltar ao inicio do menu
                        LCD_Clear();
                    }
                }
            }
            break;

            default:
                // desconhecido: sair
                if (BotaoRoxo_Pressionado())
                {
                    in_submenu = false;
                    submenu_type = SUB_NONE;
                    LCD_Clear();
                }
                break;
        }
    }
}

void Maquina_AlertaCritico(void)
{
    static bool     inicializado = false;
    static uint32_t t0 = 0;

    if (!inicializado)
    {
        inicializado = true;
        t0 = HAL_GetTick();

        buzzer_temp_alerta  = false;
        buzzer_elapsed_ms   = 0;
        buzzer_toggle_state = false;

        Desligar_Aquecimento();
        Desligar_Resfriamento();
        Servo_Para();

        LCD_Clear();
        LCD_SetCursor(0,0); LCD_SendString("ERRO CRITICO   ");
        LCD_SetCursor(1,0); LCD_SendString("REINICIE SIST  ");

        Buzzer_Liga();
    }

    if (HAL_GetTick() - t0 >= 3000)
    {
        Buzzer_Desliga();
    }
}

/* FSM Dispatcher */
void Transicao_FSM(void)
{
    g_estado_atual = estadoAtual;

    if (flag_emergencia)
    {
        estadoAtual = ALERTA_CRITICO;
    }

    switch (estadoAtual)
    {
    case MONITORAMENTO_PRINCIPAL:
        Maquina_Monitoramento();
        break;
    case MENU_MODO:
        Maquina_ModoOperacao();
        break;
    case AJUSTE_MANUAL:
        Maquina_ConfigSetpoints();
        break;
    case ALERTA_CRITICO:
        Maquina_AlertaCritico();
        break;
    default:
        estadoAtual = MENU_MODO;
        break;
    }
}

/* ===================== EXTI ===================== */

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == BOTAO_VERMELHO_Pin)
    {
        // emergência direta (botão vermelho físico)
        flag_emergencia = true;
    }
}

/* ===================== TIM CALLBACK ===================== */

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2)
    {
        Buzzer_TimerHandler();
    }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  HAL_Init();

  SystemClock_Config();

  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
  HAL_TIM_Base_Start_IT(&htim2);

#if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
  SCB->CPACR |= ((3UL << 20U) | (3UL << 22U));
#endif

  Restaurar_Setpoints_Fabrica();

  Desligar_Aquecimento();
  Desligar_Resfriamento();
  Servo_Para();
  Buzzer_Desliga();

  LCD_Init();
  LCD_SendString("Inicializando...");
  LCD_SetCursor(1, 0);
  LCD_SendString("Estufa STM32");
  HAL_Delay(1000);
  LCD_Clear();

  // ADC + DMA
  if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_buffer, NUM_ADC_CHANNELS) != HAL_OK)
  {
      Error_Handler();
  }

  // Inicial: menu principal visível (conforme pedido)
  estadoAtual      = MENU_MODO;
  g_modo_operacao  = MODO_AUTOMATICO;
  flag_emergencia  = false;
  menu_index = 0;
  in_submenu = false;
  submenu_type = SUB_NONE;

  while (1)
  {
    Transicao_FSM();
    Display_Update();
    HAL_Delay(50);
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

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 12;
  RCC_OscInitStruct.PLL.PLLN = 96;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  (void)file;
  (void)line;
}
#endif
