/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Corpo principal do programa (Main program body)
  * Este código controla um sistema IoT com sensores, display LCD,
  * atuadores via PWM (Ventoinha/Lâmpada) e menus interativos.
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
#include "i2c_lcd.h" // Biblioteca personalizada para controle do LCD via I2C

#include <stdio.h>   // Necessário para funções como snprintf (formatação de texto)

#include <string.h>  // Necessário para manipulação de strings
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdlib.h>
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* --- MÁQUINA DE ESTADOS FINITOS (FSM) --- */
/* Define os 6 estados possíveis do sistema para organizar a lógica de controle */
typedef enum {
    STATE_INIT,         // 1. Inicialização: Mensagem de boas-vindas e setup inicial
    STATE_MONITORING,   // 2. Monitoramento: Apenas lê sensores e mostra no LCD (sem ligar cargas)
    STATE_AUTO_CONTROL, // 3. Automático: Atua nos relés/PWM baseado nos sensores (Lógica principal)
    STATE_MANUAL_MODE,  // 4. Manual: Usuário controla (ou sistema fica em espera/manutenção)
    STATE_ALARM,        // 5. Alarme: Estado crítico quando limites são violados (pisca LED, avisa LCD)
    STATE_CONFIG        // 6. Configuração: Menu interativo para alterar limites e modos
} SystemState;

/* --- ESTRUTURA DO MENU --- */
/* Define as opções navegáveis no LCD */
typedef enum {
    MENU_VALUES,        // Opção 1: Ver valores (retorna ao monitoramento)
    MENU_LIMITS,        // Opção 2: Entrar na edição de limites (Temp, Umid, Luz)
    MENU_MODE,          // Opção 3: Alternar Auto/Manual
    MENU_RESET,         // Opção 4: Reset de software do MCU
    MENU_COUNT          // Auxiliar: Conta quantos itens existem (para cálculo do módulo %)
} MenuOption;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1; // Handler do Conversor Analógico-Digital 1

I2C_HandleTypeDef hi2c1; // Handler da comunicação I2C (para o LCD)

TIM_HandleTypeDef htim3; // Timer para temporização de leitura (loop de sensores)
TIM_HandleTypeDef htim4; // Timer para geração de PWM (controle de potência Ventoinha/Lâmpada)

/* USER CODE BEGIN PV */
/* --- Variáveis Globais do Usuário --- */

I2C_LCD_HandleTypeDef lcd1;     // Estrutura de controle do display LCD
SystemState currentState = STATE_INIT; // Estado inicial da máquina
MenuOption currentMenuOption = MENU_VALUES; // Opção inicial do menu
uint8_t menuLevel = 0;          // Nível do menu: 0 = Principal, 1 = Sub-menu (edição)
uint8_t currentLimitToEdit = 0; // Índice do limite a editar: 0=Temp, 1=Umid, 2=Luz
uint8_t isEditingLimit = 0;     // Flag: 0 = Navegando, 1 = Editando valor (seta piscando/valor mudando)

// Variáveis de Sensores (Dados Brutos do ADC: 0 a 4095 para 12 bits)
uint16_t adc_temp_raw = 0;  // Leitura Canal PA0
uint16_t adc_humid_raw = 0; // Leitura Canal PA1
uint16_t adc_light_raw = 0; // Leitura Canal PA2

// Valores Convertidos para Unidades Físicas
float temperature = 0.0; // Em Graus Celsius (0-50°C)
float humidity = 0.0;    // Em Porcentagem (0-100%)
float luminosity = 0.0;  // Em Porcentagem (0-100%)

// Limites de Disparo (Setpoints padrão)
float temp_limit_high = 30.0; // Acima disso, liga ventoinha/alarme
float humid_limit_low = 50.0; // Abaixo disso, alarme
float light_limit_low = 50.0; // Abaixo disso, liga lâmpada/alarme

// Flags de Controle do Sistema
uint8_t isAutoMode = 1;  // 1 = Controle Automático (PWM ativo), 0 = Manual
uint8_t systemOn = 1;    // 1 = Sistema Ligado, 0 = Parada de Emergência
uint8_t alarmActive = 0; // 1 = Condição de alarme detectada

// Variáveis de Debounce (Anti-repique) e Interrupções
uint32_t lastButton1Press = 0; // Timestamp da última pressão do Botão 1
uint32_t lastButton2Press = 0; // Timestamp da última pressão do Botão 2
uint8_t sensorUpdateFlag = 0;  // Flag levantada pelo TIM3 para avisar hora de ler sensores
uint8_t button1Flag = 0;       // Flag levantada pela interrupção EXTI (PB2) - Navegar
uint8_t button2Flag = 0;       // Flag levantada pela interrupção EXTI (PB10) - Selecionar
uint8_t emergencyFlag = 0;     // Flag levantada pela interrupção EXTI (PA5) - Emergência

// Definições de Pinos dos LEDs (Mapeamento de Hardware)
#define LED_SYSTEM_ON  GPIO_PIN_13 // PC13 - Indica MCU rodando
#define LED_AUTO_MODE  GPIO_PIN_12 // PB12 - Indica Modo Automático
#define LED_ALARM      GPIO_PIN_13 // PB13 - Indica Alarme Crítico
#define LED_FAN        GPIO_PIN_14 // PB14 - Indica Saída PWM Ventoinha ativa
#define LED_LAMP       GPIO_PIN_15 // PB15 - Indica Saída PWM Lâmpada ativa

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM4_Init(void);
/* USER CODE BEGIN PFP */
/* Protótipos das Funções Criadas pelo Usuário */
static void ADC_Read_All(void);          // Lê todos os canais ADC
static void Convert_ADC_Values(void);    // Converte Raw -> Físico
static void State_Handler(void);         // Gerencia a troca de estados
static void Update_LCD_Display(void);    // Atualiza LCD em modo normal
static void Check_Limits_and_Control(void); // Lógica de controle (PWM e Alarmes)
static void Update_LEDs(void);           // Liga/Desliga LEDs físicos

// Funções Específicas do Menu
static void Menu_Display(void);          // Desenha o menu no LCD
static void Menu_Action_Button1(void);   // Lógica do botão "Navegar"
static void Menu_Action_Button2(void);   // Lógica do botão "Selecionar"
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
  * @brief  Realiza a leitura sequencial dos 3 canais do ADC.
  * Configura o canal, inicia a conversão, aguarda e lê o valor.
  */
static void ADC_Read_All(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    // 1. Leitura PA0 (Sensor de Temperatura)
    sConfig.Channel = ADC_CHANNEL_0;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES; // Tempo de amostragem rápido
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) == HAL_OK) {
        HAL_ADC_Start(&hadc1); // Inicia conversão
        HAL_ADC_PollForConversion(&hadc1, 100); // Espera terminar (timeout 100ms)
        adc_temp_raw = HAL_ADC_GetValue(&hadc1); // Salva valor bruto
    }

    // 2. Leitura PA1 (Sensor de Umidade)
    sConfig.Channel = ADC_CHANNEL_1;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) == HAL_OK) {
        HAL_ADC_Start(&hadc1);
        HAL_ADC_PollForConversion(&hadc1, 100);
        adc_humid_raw = HAL_ADC_GetValue(&hadc1);
    }

    // 3. Leitura PA2 (Sensor de Luminosidade / LDR)
    sConfig.Channel = ADC_CHANNEL_2;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) == HAL_OK) {
        HAL_ADC_Start(&hadc1);
        HAL_ADC_PollForConversion(&hadc1, 100);
        adc_light_raw = HAL_ADC_GetValue(&hadc1);
    }
}

/**
  * @brief  Converte valores brutos do ADC (0-4095) para valores físicos reais.
  * Aplica regra de três simples baseada na calibração teórica.
  */
static void Convert_ADC_Values(void)
{
    // Proteção: Ignora leituras absurdas (acima de 12 bits)
    if (adc_temp_raw > 4095 || adc_humid_raw > 4095 || adc_light_raw > 4095) {
        // Poderia adicionar flag de erro de sensor aqui
        return;
    }

    // Conversão Matemática:
    // Temperatura: Mapeia 0-4095 para 0-50°C
    temperature = (float)adc_temp_raw * 50.0f / 4095.0f;

    // Umidade: Mapeia 0-4095 para 0-100%
    humidity = (float)adc_humid_raw * 100.0f / 4095.0f;

    // Luminosidade: Mapeia 0-4095 para 0-100%
    luminosity = (float)adc_light_raw * 100.0f / 4095.0f;
}

/**
  * @brief  Coração do controle automático. Verifica limites e ajusta PWM.
  */
static void Check_Limits_and_Control(void)
{
    // 1. Verificação de Limites para acionar ALARME
    // Se Temp alta, Umidade baixa ou Luz baixa -> Ativa Alarme
    if (temperature > temp_limit_high ||
        humidity < humid_limit_low ||
        luminosity < light_limit_low)
    {
        alarmActive = 1;
        // Se não estiver configurando o menu, força ir para tela de alarme
        if (currentState != STATE_CONFIG) {
             currentState = STATE_ALARM;
        }
    }
    // Lógica de Histerese (Zona Morta) para desligar o alarme
    else if (alarmActive)
    {
        // Só desliga o alarme se os valores voltarem com uma margem de segurança
        // (ex: Temp cair 1°C abaixo do limite, Umid subir 2% acima do limite)
        if (temperature < temp_limit_high - 1.0f &&
            humidity > humid_limit_low + 2.0f &&
            luminosity > light_limit_low + 2.0f)
        {
            alarmActive = 0;
            // Retorna ao estado anterior (Auto ou Monitoramento)
            if (currentState == STATE_ALARM) {
                 currentState = isAutoMode ? STATE_AUTO_CONTROL : STATE_MONITORING;
                 lcd_clear(&lcd1); // Limpa tela para tirar mensagem de erro
            }
        }
    }

    // 2. Controle Proporcional (PWM) dos Atuadores
    // Só atua se estiver em modo AUTOMÁTICO e Sistema LIGADO
    if (isAutoMode && systemOn)
    {
        // --- Controle da Ventoinha (Rampa Linear) ---
        uint16_t fan_pwm_pulse = 0;
        float start_temp = 20.0f; // Temperatura onde a ventoinha começa a girar

        if (temperature < start_temp) {
            // Se menor que 20°C -> Ventoinha parada
            fan_pwm_pulse = 0;
        }
        else if (temperature >= temp_limit_high) {
            // Se maior que o Limite -> Velocidade Máxima
            fan_pwm_pulse = htim4.Init.Period;
        }
        else {
            // Rampa Linear entre 20°C e o Limite (ex: 30°C)
            // Calcula regra de três para o Duty Cycle
            float range = temp_limit_high - start_temp;
            if (range <= 0) range = 1.0f; // Evita divisão por zero

            fan_pwm_pulse = (uint16_t)(((temperature - start_temp) / range) * htim4.Init.Period);
        }

        // Aplica o valor ao registrador do Timer 4 Canal 3 (Pino PB8/PB14 dependendo do hardware)
        htim4.Instance->CCR3 = fan_pwm_pulse;

        // --- Controle da Lâmpada (On/Off via PWM Max/Min) ---
        uint16_t lamp_pwm_pulse = 0;
        // Se estiver muito escuro, liga a lâmpada no máximo
        if (luminosity < light_limit_low)
        {
            lamp_pwm_pulse = htim4.Init.Period;
        }
        htim4.Instance->CCR4 = lamp_pwm_pulse; // Atualiza Timer 4 Canal 4

    } else {
        // Se estiver em MANUAL ou DESLIGADO, corta as saídas PWM
        htim4.Instance->CCR3 = 0;
        htim4.Instance->CCR4 = 0;
    }
}

/**
  * @brief  Atualiza o estado físico dos LEDs baseados nas variáveis globais.
  */
static void Update_LEDs(void)
{
    // LED1 (PC13): System On (Aceso se systemOn = 1)
    HAL_GPIO_WritePin(GPIOC, LED_SYSTEM_ON, systemOn ? GPIO_PIN_SET : GPIO_PIN_RESET);

    // LED2 (PB12): Auto Mode (Aceso se Auto = 1 e System = 1)
    HAL_GPIO_WritePin(GPIOB, LED_AUTO_MODE, isAutoMode && systemOn ? GPIO_PIN_SET : GPIO_PIN_RESET);

    // LED3 (PB13): Alarme
    // Se houver alarme, o Timer 3 fará ele piscar. Se não, garante apagado aqui.
    if (alarmActive && systemOn) {
        // O controle de piscar está no Callback do Timer 3
    } else {
        HAL_GPIO_WritePin(GPIOB, LED_ALARM, GPIO_PIN_RESET);
    }

    // LED4 (PB14): Fan Active (Aceso se PWM da ventoinha > 0)
    HAL_GPIO_WritePin(GPIOB, LED_FAN, (htim4.Instance->CCR3 > 0 && systemOn) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    // LED5 (PB15): Lamp Active (Aceso se PWM da lâmpada > 0)
    HAL_GPIO_WritePin(GPIOB, LED_LAMP, (htim4.Instance->CCR4 > 0 && systemOn) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/**
  * @brief  Gerenciador principal da Lógica (Máquina de Estados).
  * Executado ciclicamente dentro do While(1).
  */
static void State_Handler(void)
{
    // *** Transição Prioritária 1: Entrar no Menu ***
    // Se algum botão foi apertado, força ida para CONFIG, a menos que seja emergência
    if (button1Flag || button2Flag) {
        if (currentState != STATE_CONFIG && systemOn) {
            currentState = STATE_CONFIG;
            menuLevel = 0;      // Reseta para o topo do menu
            isEditingLimit = 0; // Garante que não entra editando
            lcd_clear(&lcd1);   // Prepara o display
        }
        // Nota: As flags não são limpas aqui, são tratadas dentro do case STATE_CONFIG
    }

    // *** Transição Prioritária 2: Emergência (Sistema Desligado) ***
    if (!systemOn) {
        lcd_clear(&lcd1);
        lcd_gotoxy(&lcd1, 0, 0);
        lcd_puts(&lcd1, "EMERGENCIA!");
        lcd_gotoxy(&lcd1, 0, 1);
        lcd_puts(&lcd1, "SISTEMA OFFLINE");

        // Segurança: Desliga tudo
        htim4.Instance->CCR3 = 0;
        htim4.Instance->CCR4 = 0;
        HAL_GPIO_WritePin(GPIOB, LED_AUTO_MODE | LED_ALARM | LED_FAN | LED_LAMP, GPIO_PIN_RESET);
        return; // Sai da função, não executa o switch abaixo
    }

    // *** Transição Prioritária 3: Alarme Crítico ***
    // Se detectou alarme e não estamos configurando, vai para tela de erro
    if (alarmActive && currentState != STATE_ALARM && currentState != STATE_CONFIG) {
        currentState = STATE_ALARM;
        lcd_clear(&lcd1);
    }

    // Switch Case Principal: Executa a lógica baseada no estado atual
    switch (currentState) {
        case STATE_INIT:
            // Tela de Boot
            lcd_clear(&lcd1);
            lcd_gotoxy(&lcd1, 0, 0);
            lcd_puts(&lcd1, "  Sistema IOT ");
            lcd_gotoxy(&lcd1, 0, 1);
            lcd_puts(&lcd1, "  Iniciando...");
            HAL_Delay(1000); // Aguarda 1s para leitura do usuário
            // Define o estado inicial real
            currentState = isAutoMode ? STATE_AUTO_CONTROL : STATE_MONITORING;
            break;

        case STATE_AUTO_CONTROL:
            // Modo Principal: Lê sensores periodicamente e atua
            if (sensorUpdateFlag) { // Flag setada pelo Timer a cada ~2s
                ADC_Read_All();
                Convert_ADC_Values();
                Check_Limits_and_Control(); // Calcula PWM e verifica Alarmes
                sensorUpdateFlag = 0;       // Reseta flag
            }
            Update_LCD_Display(); // Mostra T, H, L no LCD
            break;

        case STATE_MANUAL_MODE:
             // Modo Manual: Lê sensores mas NÃO atua automaticamente (controle PWM é zero)
             // (Ou poderia ser implementado controle manual via potenciômetro aqui)
             if (sensorUpdateFlag) {
                 ADC_Read_All();
                 Convert_ADC_Values();
                 sensorUpdateFlag = 0;
             }
             Update_LCD_Display();
             break;

        case STATE_ALARM:
            // Modo de Erro: Mostra qual sensor falhou
            if (sensorUpdateFlag) {
                ADC_Read_All();
                Convert_ADC_Values();
                Check_Limits_and_Control(); // Verifica se condições voltaram ao normal
                sensorUpdateFlag = 0;
            }

            // Garante tela limpa na primeira entrada
            static SystemState lastState = STATE_INIT;
            if (lastState != STATE_ALARM) {
                lcd_clear(&lcd1);
            }
            lastState = STATE_ALARM;

            // Exibe mensagem de erro formatada
            lcd_gotoxy(&lcd1, 0, 0);
            lcd_puts(&lcd1, "!!!! ALARME !!!!");

            char msg[17];
            // Identifica a causa do alarme para mostrar na linha 2
            if (temperature > temp_limit_high) snprintf(msg, 17, "T:%.1f > %.0f", temperature, temp_limit_high);
            else if (humidity < humid_limit_low) snprintf(msg, 17, "H:%.1f < %.0f", humidity, humid_limit_low);
            else if (luminosity < light_limit_low) snprintf(msg, 17, "L:%.1f < %.0f", luminosity, light_limit_low);
            else snprintf(msg, 17, "Recuperando..."); // Histerese atuando

            lcd_gotoxy(&lcd1, 0, 1);
            lcd_puts(&lcd1, msg);
            break;

        case STATE_CONFIG:
            // Modo Menu: Processa botões para navegar/editar
            if (button1Flag) {
                Menu_Action_Button1(); // Ação: Próximo / Incrementar
                button1Flag = 0;       // Limpa flag (evento consumido)
            }
            if (button2Flag) {
                Menu_Action_Button2(); // Ação: Entrar / Salvar / Sair
                button2Flag = 0;       // Limpa flag
            }

            Menu_Display(); // Renderiza o menu no LCD
            break;
    }

    Update_LEDs(); // Atualiza LEDs físicos em todo ciclo
    HAL_Delay(10); // Pequeno delay para estabilidade do loop principal
}

/**
  * @brief  Formata e imprime os valores dos sensores no LCD (Modo Normal).
  */
static void Update_LCD_Display(void)
{
    char line1[17];
    char line2[17];

    if (currentState == STATE_MONITORING || currentState == STATE_AUTO_CONTROL) {
        // Linha 1: "T:25.0C H:60.0%"
        snprintf(line1, 17, "T:%.1fC H:%.1f%%", temperature, humidity);
        // Linha 2: "L:80.0% [AUTO]"
        snprintf(line2, 17, "L:%.1f%% %s", luminosity, isAutoMode ? "[AUTO]" : "[MANUAL]");

        lcd_gotoxy(&lcd1, 0, 0);
        lcd_puts(&lcd1, line1);
        lcd_gotoxy(&lcd1, 0, 1);
        lcd_puts(&lcd1, line2);
    }
}

/**
  * @brief  Gerencia o desenho do Menu e Sub-menus no LCD.
  */
static void Menu_Display(void)
{
    // Nível 0: Menu Principal (Lista de opções)
    if (menuLevel == 0) {
        lcd_gotoxy(&lcd1, 0, 0);
        lcd_puts(&lcd1, ">> Menu Principal");
        lcd_gotoxy(&lcd1, 0, 1);

        // Mostra a opção selecionada com base em `currentMenuOption`
        switch (currentMenuOption) {
            case MENU_VALUES:
                lcd_puts(&lcd1, ">1. Valores Atuais");
                break;
            case MENU_LIMITS:
                lcd_puts(&lcd1, ">2. Limites");
                break;
            case MENU_MODE:
                lcd_puts(&lcd1, ">3. Modo: [ ]");
                lcd_gotoxy(&lcd1, 12, 1);
                lcd_puts(&lcd1, isAutoMode ? "AUTO" : "MANUAL"); // Mostra estado atual
                break;
            case MENU_RESET:
                lcd_puts(&lcd1, ">4. Resetar");
                break;
            default:
                lcd_puts(&lcd1, ">Menu Invalido");
                break;
        }
    }
    // Nível 1: Tela de Edição de Limites
    else if (menuLevel == 1 && currentMenuOption == MENU_LIMITS) {
        char line1[17];
        char line2[17];
        // Arrays auxiliares para iterar sobre os limites
        const char *lim_names[] = {"T MAX", "H MIN", "L MIN"};
        float *limits[] = {&temp_limit_high, &humid_limit_low, &light_limit_low};

        // Verifica se estamos editando um dos 3 limites ou na opção "Voltar"
        if (currentLimitToEdit <= 2) {
            // Mostra Nome do Limite e Valor Atual
            snprintf(line1, 17, "%s: %.1f%s", lim_names[currentLimitToEdit], *limits[currentLimitToEdit], currentLimitToEdit == 0 ? "C" : "%");
            lcd_gotoxy(&lcd1, 0, 0);
            lcd_puts(&lcd1, line1);

            if (isEditingLimit) {
                // Modo Edição Ativa: B1 soma valor, B2 salva
                snprintf(line2, 17, "B1:+1.0 | B2:SALVAR");
            } else {
                // Modo Navegação: B1 vai pro próximo, B2 edita este
                snprintf(line2, 17, ">B1:PROX | B2:EDITAR");
            }
        } else {
            // Opção 4: Sair/Voltar
            lcd_gotoxy(&lcd1, 0, 0);
            lcd_puts(&lcd1, " Limites Config.");
            snprintf(line2, 17, "> B1:PROX | B2:VOLTAR");
        }

        lcd_gotoxy(&lcd1, 0, 1);
        lcd_puts(&lcd1, line2);
    }
}

/**
  * @brief  Lógica do Botão 1 (Navegação/Incremento).
  */
static void Menu_Action_Button1(void)
{
    if (menuLevel == 0) {
        // No Menu Principal: Desce para a próxima opção (ciclo infinito)
        currentMenuOption = (currentMenuOption + 1) % MENU_COUNT;
    }
    else if (menuLevel == 1 && currentMenuOption == MENU_LIMITS) {
        // No Sub-menu de Limites
        float *limits[] = {&temp_limit_high, &humid_limit_low, &light_limit_low};

        if (isEditingLimit) {
            // Se editando: Soma +1.0 ao valor
            *limits[currentLimitToEdit] += 1.0f;

            // Clamping: Impede valores absurdos
            if (currentLimitToEdit == 0 && *limits[0] > 50.0f) *limits[0] = 50.0f; // Max Temp 50
            if (currentLimitToEdit > 0 && *limits[currentLimitToEdit] > 100.0f) *limits[currentLimitToEdit] = 100.0f; // Max % 100
        } else {
            // Se navegando: Vai para o próximo limite
            currentLimitToEdit++;
            if (currentLimitToEdit > 3) { // 0, 1, 2, 3(Voltar)
                currentLimitToEdit = 0;
            }
        }
    }
}

/**
  * @brief  Lógica do Botão 2 (Seleção/Edição/Confirmação).
  */
static void Menu_Action_Button2(void)
{
    // Nível 0: Executa a opção selecionada no Menu Principal
    if (menuLevel == 0) {
        switch (currentMenuOption) {
            case MENU_VALUES:
                // Sai do menu e volta a mostrar valores
                currentState = isAutoMode ? STATE_AUTO_CONTROL : STATE_MONITORING;
                lcd_clear(&lcd1);
                menuLevel = 0;
                break;
            case MENU_LIMITS:
                // Entra no sub-menu de limites
                menuLevel = 1;
                currentLimitToEdit = 0;
                isEditingLimit = 0;
                lcd_clear(&lcd1);
                break;
            case MENU_MODE:
                // Alterna flag Auto/Manual imediatamente
                isAutoMode = !isAutoMode;
                currentState = isAutoMode ? STATE_AUTO_CONTROL : STATE_MONITORING;
                lcd_clear(&lcd1);
                break;
            case MENU_RESET:
                // Reseta o microcontrolador via software
                NVIC_SystemReset();
                break;
            default:
                break;
        }
    }
    // Nível 1: Lógica dentro do Sub-menu Limites
    else if (menuLevel == 1 && currentMenuOption == MENU_LIMITS) {
        if (currentLimitToEdit <= 2) {
            // Se estiver sobre um valor (Temp/Umid/Luz)
            if (isEditingLimit) {
                // Se já estava editando, SALVA (para de editar)
                isEditingLimit = 0;
            } else {
                // Se não estava, COMEÇA a editar
                isEditingLimit = 1;
            }
        } else { // Opção "Voltar"
            // Retorna ao Menu Principal
            menuLevel = 0;
            currentLimitToEdit = 0;
            isEditingLimit = 0;
            lcd_clear(&lcd1);
        }
    }
}

/**
  * @brief  Callback do Timer. Executado quando TIM3 estoura.
  * @param  htim: Ponteiro para o timer que gerou a interrupção.
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)

{

if (htim->Instance == TIM3)

{

// Timer 3: Controle de tempo para leitura de sensores
// Configurado para gerar interrupção periódica.

static uint8_t count2s = 0; // Contador auxiliar para gerar base de tempo maior

if (count2s++ >= 5) // Exemplo: Se timer base é 100ms, 5x = 500ms (ajustar conforme prescaler)
{
    sensorUpdateFlag = 1; // Avisa o loop principal para ler ADC
    count2s = 0;
}

// Controle do Piscar do LED de Alarme (Toggle)
if (alarmActive && systemOn)
{
    HAL_GPIO_TogglePin(GPIOB, LED_ALARM); // Inverte estado do pino
}

}

}


/**
  * @brief  Callback de Interrupção Externa (Botões e Emergência).
  * Chamado quando há mudança de estado nos pinos configurados como EXTI.
  */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    // Debounce: Evita múltiplas leituras falsas causadas pela mecânica do botão
    uint32_t currentTick = HAL_GetTick();

    if (GPIO_Pin == GPIO_PIN_2) // Botão 1 (PB2) - Navegar
    {
        if (currentTick - lastButton1Press > 200) // Ignora se pressionado em menos de 200ms
        {
            button1Flag = 1; // Seta flag para o State_Handler processar
            lastButton1Press = currentTick;
        }
    }
    else if (GPIO_Pin == GPIO_PIN_10) // Botão 2 (PB10) - Selecionar
        {
            // Debounce maior (400ms) para evitar cliques duplos acidentais no menu
            if (currentTick - lastButton2Press > 400)
            {
                button2Flag = 1;
                lastButton2Press = currentTick;
            }
        }
    else if (GPIO_Pin == GPIO_PIN_5) // Botão de Emergência (PA5)
    {
        // Prioridade máxima: Desliga sistema instantaneamente via hardware/flag
        systemOn = 0;
        emergencyFlag = 1;
    }
}


/**
  * @brief System Clock Configuration
  * @retval None
  */

/* USER CODE END 0 */

/**
  * @brief  Ponto de entrada do programa (Main).
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
  // Inicializações de baixo nível (antes do HAL)
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset de todos periféricos e Init da Flash/Systick */
  HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  /* Configura o Clock do sistema (PLL, Prescalers) */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */

  /* Inicializa os periféricos configurados no CubeMX */
  MX_GPIO_Init();  // GPIOs (LEDs, Botões)
  MX_ADC1_Init();  // ADC (Sensores)
  MX_I2C1_Init();  // I2C (LCD)
  MX_TIM3_Init();  // Timer Leitura/Pisca
  MX_TIM4_Init();  // Timer PWM
  /* USER CODE BEGIN 2 */

  // Configuração e Inicialização do Driver do LCD
  lcd1.hi2c = &hi2c1; // Associa handler I2C
  lcd1.address = 0x27 << 1; // Endereço I2C do módulo (0x27 deslocado)
  lcd_init(&lcd1); // Sequência de init do display

  // Inicia Timer 3 em modo Interrupção (base de tempo)
  HAL_TIM_Base_Start_IT(&htim3);

  // Inicia canais de PWM do Timer 4
  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_3); // Ventoinha
  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_4); // Lâmpada

  // Garante que PWM comece zerado
  htim4.Instance->CCR3 = 0;
  htim4.Instance->CCR4 = 0;

  // Define estado inicial da FSM
  currentState = STATE_INIT;


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  // Loop Principal do programa
  while (1)
  {
      // Chama o gerenciador de estados continuamente
	  State_Handler();

	  // Pequeno atraso para evitar consumo excessivo de CPU e dar tempo para ADC/Display
	  HAL_Delay(10);



    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * Configura o clock da CPU e barramentos.
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
  // Configura oscilador interno (HSI) e PLL para atingir clock desejado
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 192; // Configuração de multiplicadores do PLL
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  // Distribui clocks para os barramentos
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
  * Configura hardware do ADC (Resolução, Alinhamento, Clock).
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
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4; // Divisor de clock
  hadc1.Init.Resolution = ADC_RESOLUTION_12B; // Resolução de 12 bits (0-4095)
  hadc1.Init.ScanConvMode = DISABLE; // Modo Single Channel (gerenciado manualmente)
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START; // Gatilho via Software
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
  // Configuração inicial padrão para o canal 0
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
  * Configura comunicação serial I2C para o Display.
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
  hi2c1.Init.ClockSpeed = 100000; // Velocidade padrão 100kHz
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
  * @brief TIM3 Initialization Function
  * Configura Timer 3 para temporização geral (1Hz/2s).
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
  htim3.Init.Prescaler = 7200; // Reduz o clock alto da CPU
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 1380; // Valor de estouro (ARR) para gerar ~10Hz
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
 * Configura Timer 4 para geração de sinais PWM (Ventoinha/Lâmpada).
 */
static void MX_TIM4_Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  htim4.Instance = TIM4;

  // *** ALTERAÇÃO CRÍTICA PARA ELIMINAR O PISCAR ***
  // Configuração para frequência de 1 kHz
  // Frequência base estimada: 96MHz (conforme SystemClock_Config)
  // Prescaler 95 -> Clock do Timer = 1 MHz (1 us de precisão)
  // Period 999   -> Frequência PWM = 1 kHz (1000 Hz) - Totalmente linear e sem piscar
  htim4.Init.Prescaler = 95;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 999; // Resolução do PWM: 0 a 999 (1000 passos)
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

  // Configuração dos Canais PWM
  sConfigOC.OCMode = TIM_OCMODE_PWM1; // PWM Modo 1 (Contagem Up, High quando < CCR)
  sConfigOC.Pulse = 0; // Duty Cycle inicial 0%
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

  if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_TIM_MspPostInit(&htim4); // Configuração dos pinos físicos para PWM
}



/**
  * @brief GPIO Initialization Function
  * Configura pinos de I/O digitais (LEDs, Botões).
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* Habilita clocks das portas GPIO usadas */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* Configura estado inicial dos pinos de Saída (RESET = 0V) */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15, GPIO_PIN_RESET);

  /* Configura PC13 (LED System On) como Saída Push-Pull */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* Configura Botões (PB2 e PB10) como Entrada com Interrupção (Borda de Subida) */
  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING; // Dispara IRQ quando aperta
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;       // Resistor interno de Pull-Down
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* Configura LEDs PB12-PB15 como Saída Push-Pull */
  GPIO_InitStruct.Pin = GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
  // Configuração das prioridades de Interrupção no NVIC
  // Linha 2 do EXTI (para PB2)
    HAL_NVIC_SetPriority(EXTI2_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI2_IRQn);

    // Linhas 15-10 do EXTI (para PB10)
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  Esta função é chamada em caso de erro crítico de hardware/config.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* O usuário pode adicionar implementação para reportar erro (ex: piscar LED rápido) */
  __disable_irq(); // Desabilita interrupções para travar o sistema
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reporta nome do arquivo e linha onde ocorreu erro de assert.
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
