/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body (UTFPR - Monitor Ambiental Completo)
  * RA              : [Inserir o seu RA]
  ********************************----------------------------------------------
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c_lcd.h"
#include <stdio.h>

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
I2C_HandleTypeDef hi2c1;
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;

/* USER CODE BEGIN PV */
// =========================================================================
// [REQUISITO] ENTRADAS ANALÓGICAS & DIGITAIS
// =========================================================================
uint32_t valor_sensor = 0;             // Canal 0 (Sensor de Gás)
uint32_t valor_potenciometro = 1;      // Canal 1 (Potenciômetro 1 - Ajuste de Parâmetros)
uint32_t valor_pot_buzzer = 0;         // Canal 2 (Potenciômetro 2 - Ajuste do Limiar de Alarme)
uint32_t valor_temperatura_adc = 0;    // Canal 4 (Sensor de Temperatura LM35)

float porcentagem_gas = 0.0f;
float temperatura_celsius = 0.0f;
float qualidade_ambiente = 100.0f;

// --- VARIÁVEIS DOS NOVOS MODELOS MATEMÁTICOS ---
float temperatura_fahrenheit = 32.0f;
float gas_filtrado = 0.0f;
float tempo_exposicao_segura = 120.0f;
uint32_t milissegundos_ligado = 0;
float consumo_kwh = 0.0f;

// --- VARIÁVEIS DE CONFIGURAÇÃO SALVAS ---
uint32_t sensibilidade = 1;
uint32_t sensibilidade_temp = 1;
uint8_t porcentagem_motor = 70;
uint8_t porcentagem_motor_temp = 70;
uint32_t limiar_alerta_gas = 70;

// --- VARIÁVEIS DE CONTROLE E TRAVAS (PROTEÇÕES) ---
uint32_t valor_pot_anterior = 0;
uint8_t trava_pot = 1;
uint8_t forcar_atualizacao_pot_anterior = 0; // Flag para sincronizar o potenciômetro na troca de telas
I2C_LCD_HandleTypeDef my_lcd;

// --- CONTROLE ASSÍNCRONO DO ALARME ---
uint32_t intervalo_bipe = 1000;
uint8_t som_ligado = 0;

// --- CONTROLE DO BOTÃO DE INCÊNDIO MANUAL ---
uint8_t alarme_manual_ativo = 0;
uint32_t tempo_ultimo_botoes_juntos = 0;

// ⏱️ 🎯 VARIÁVEL DE TESTE DO SLIDE DA UTFPR
volatile uint32_t i = 0;

// =========================================================================
// [REQUISITO] MÁQUINA DE ESTADOS EXPANDIDA (7 ESTADOS)
// =========================================================================
typedef enum {
    ESTADO_MONITORAMENTO  = 0,
    ESTADO_AJUSTE_SENS    = 1,
    ESTADO_DIAGNOSTICO    = 2,
    ESTADO_CONFIG_MOTOR   = 3,
    ESTADO_CONFIG_ALARME  = 4,
    ESTADO_QUALIDADE      = 5,
    ESTADO_ALARME_CRITICO = 6
} EstadoSistema;

volatile EstadoSistema estado_atual = ESTADO_MONITORAMENTO;

// --- CONTROLE DE DEBOUNCE ASSÍNCRONO DOS BOTÕES ---
uint8_t botao_voltar_ultimo = 1;
uint32_t tempo_voltar_debounce = 0;
volatile uint32_t tempo_ultimo_clique_exti = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_ADC1_Init(void);
void MX_TIM2_Init(void);
void MX_TIM3_Init(void);
void MX_I2C1_Init(void);
void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* USER CODE BEGIN PFP */
uint32_t ADC_Ler_Canal(uint32_t canal);
uint32_t obter_sensibilidade(uint32_t valor_pot);
uint8_t obter_motor(uint32_t valor_pot);
float calcular_qualidade_ambiente(float gas_pct, float temp_celsius);
void gerenciar_botao_voltar_polling(void);
void gerenciar_botoes_juntos_incendio(void);
void gerenciar_buzzer_temporizado(void);
void atualizar_display_maquina_estados(void);

// PROTÓTIPOS DOS NOVOS MODELOS MATEMÁTICOS
float converter_celsius_to_fahrenheit(float c);
float filtro_media_movel_exponencial(float valor_atual, float valor_anterior);
float calcular_tempo_exposicao(float pct_gas);
float calcular_consumo_energetico(uint32_t tempo_ms);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// =========================================================================
// [REQUISITO] 1 INTERRUPÇÃO EXTERNA REAL (Botão Avançar no PA12)
// =========================================================================
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == GPIO_PIN_12) {
        uint32_t tempo_atual = HAL_GetTick();

        // Bloqueia avanço se o botão Voltar (PB10) estiver pressionado
        if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_10) == GPIO_PIN_RESET) {
            return;
        }

        if ((tempo_atual - tempo_ultimo_clique_exti) > 250) {
            if (estado_atual == ESTADO_AJUSTE_SENS)  sensibilidade = sensibilidade_temp;
            if (estado_atual == ESTADO_CONFIG_MOTOR) porcentagem_motor = porcentagem_motor_temp;

            if (estado_atual == ESTADO_QUALIDADE) {
                estado_atual = ESTADO_MONITORAMENTO;
            } else if (estado_atual != ESTADO_ALARME_CRITICO) {
                estado_atual++;
            }

            trava_pot = 1;
            forcar_atualizacao_pot_anterior = 1; // Sinaliza que precisamos reajustar a base do potenciômetro
            tempo_ultimo_clique_exti = tempo_atual;
        }
    }
}

// =========================================================================
// ⏱️ 🔥 [REQUISITO] SEGUNDA INTERRUPÇÃO REAL DE HARDWARE (ESTOURO DO TIM3)
// =========================================================================
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim)
{
    if (htim->Instance == TIM3) {
        if (estado_atual == ESTADO_ALARME_CRITICO) {
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET); // Apaga LED nativo (Lógica Inversa)
        } else {
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET); // Mantém aceso (Heartbeat seguro)
        }

        i++;
        if(i == 100000)
            i = 0;
    }
}

// [REQUISITO] 2 BOTÕES (Botão Voltar em PB10 operando por Polling)
void gerenciar_botao_voltar_polling(void) {
    uint32_t tempo_atual = HAL_GetTick();
    uint8_t estado_voltar = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_10);

    // Bloqueia retorno se o botão Avançar (PA12) estiver pressionado
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_12) == GPIO_PIN_RESET) {
        botao_voltar_ultimo = estado_voltar;
        return;
    }

    if (estado_voltar == 0 && botao_voltar_ultimo == 1) {
        if ((tempo_atual - tempo_voltar_debounce) > 250) {
            if (estado_atual == ESTADO_AJUSTE_SENS)  sensibilidade = sensibilidade_temp;
            if (estado_atual == ESTADO_CONFIG_MOTOR) porcentagem_motor = porcentagem_motor_temp;

            if (estado_atual == ESTADO_MONITORAMENTO) {
                estado_atual = ESTADO_QUALIDADE;
            } else if (estado_atual != ESTADO_ALARME_CRITICO) {
                estado_atual--;
            }

            trava_pot = 1;
            forcar_atualizacao_pot_anterior = 1; // Sinaliza que precisamos reajustar a base do potenciômetro
            tempo_voltar_debounce = tempo_atual;
        }
    }
    botao_voltar_ultimo = estado_voltar;
}

// Filtro de tempo contínuo (80ms) para evitar falsos disparos individuais
void gerenciar_botoes_juntos_incendio(void) {
    uint32_t tempo_atual = HAL_GetTick();
    static uint32_t tempo_pressionado_juntos = 0;
    static uint8_t pressionados_no_ciclo_anterior = 0;

    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_12) == GPIO_PIN_RESET &&
        HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_10) == GPIO_PIN_RESET) {

        if (!pressionados_no_ciclo_anterior) {
            tempo_pressionado_juntos = tempo_atual;
            pressionados_no_ciclo_anterior = 1;
        }

        if ((tempo_atual - tempo_pressionado_juntos) >= 80) {
            if ((tempo_atual - tempo_ultimo_botoes_juntos) > 500) {
                if (estado_atual == ESTADO_ALARME_CRITICO) {
                    alarme_manual_ativo = 0;
                    lcd_clear(&my_lcd);
                    estado_atual = ESTADO_MONITORAMENTO;
                } else {
                    alarme_manual_ativo = 1;
                    lcd_clear(&my_lcd);
                    estado_atual = ESTADO_ALARME_CRITICO;
                }
                tempo_ultimo_botoes_juntos = tempo_atual;
                pressionados_no_ciclo_anterior = 0;
            }
        }
    } else {
        pressionados_no_ciclo_anterior = 0;
    }
}

// MODELO MATEMÁTICO 5: Mapeamento Não-Linear Discreto para Sensibilidade
uint32_t obter_sensibilidade(uint32_t valor_pot) {
    if (valor_pot >= 3276)      return 5;
    else if (valor_pot >= 2457) return 4;
    else if (valor_pot >= 1638) return 3;
    else if (valor_pot >= 819)  return 2;
    else                        return 1;
}

// MODELO MATEMÁTICO 6: Interpolação Linear para Ajuste Percentual do Motor
uint8_t obter_motor(uint32_t valor_pot) {
    return (uint8_t)((valor_pot * 100) / 4095);
}

// MODELO MATEMÁTICO 9 e 10: Índice Ponderado de Qualidade de Ar e Penalidade Térmica
float calcular_qualidade_ambiente(float gas_pct, float temp_celsius) {
    float nota_gas = 100.0f - gas_pct;
    if (nota_gas < 0.0f) nota_gas = 0.0f;

    float nota_temp = 0.0f;
    if (temp_celsius >= 20.0f && temp_celsius <= 26.0f) {
        nota_temp = 100.0f;
    } else if (temp_celsius > 26.0f) {
        nota_temp = 100.0f - ((temp_celsius - 26.0f) * 5.26f);
    } else if (temp_celsius < 20.0f) {
        nota_temp = 100.0f - ((20.0f - temp_celsius) * 10.0f);
    }
    if (nota_temp < 0.0f) nota_temp = 0.0f;

    return (nota_gas * 0.70f) + (nota_temp * 0.30f);
}

// MODELO MATEMÁTICO 11: Conversão Linear de Escala Térmica (Fahrenheit)
float converter_celsius_to_fahrenheit(float c) {
    return (c * 1.8f) + 32.0f;
}

// MODELO MATEMÁTICO 12: Filtro Digital Passa-Baixas (Média Móvel Exponencial)
float filtro_media_movel_exponencial(float valor_atual, float valor_anterior) {
    float alpha = 0.2f;
    return (alpha * valor_atual) + ((1.0f - alpha) * valor_anterior);
}

// MODELO MATEMÁTICO 13: Decaimento Linearizado do Tempo de Exposição Segura
float calcular_tempo_exposicao(float pct_gas) {
    if (pct_gas <= 5.0f) return 120.0f;

    if (pct_gas < 25.0f) {
        return 120.0f - (pct_gas * 3.2f);
    } else if (pct_gas < 50.0f) {
        return 40.0f - ((pct_gas - 25.0f) * 1.2f);
    } else {
        return 5.0f;
    }
}

// MODELO MATEMÁTICO 14: Integração Temporal de Potência (Consumo Energético)
float calcular_consumo_energetico(uint32_t tempo_ms) {
    float horas = (float)tempo_ms / 3600000.0f;
    float potencia_kw = 0.0005f;
    return potencia_kw * horas;
}

// =========================================================================
// GERAÇÃO DE ÁUDIO DINÂMICA VIA RECORRÊNCIA TEMPORAL REAL
// =========================================================================
void gerenciar_buzzer_temporizado(void) {
    static uint32_t ultimo_tempo_bipe = 0;
    uint32_t tempo_atual = HAL_GetTick();

    if (estado_atual == ESTADO_ALARME_CRITICO) {
        intervalo_bipe = 1500 - (uint32_t)(gas_filtrado * 13.0f);
        if (intervalo_bipe < 80) intervalo_bipe = 80;

        if ((tempo_atual - ultimo_tempo_bipe) >= intervalo_bipe) {
            ultimo_tempo_bipe = tempo_atual;

            if (som_ligado == 0) {
                uint32_t tom_frequencia = 2000 - (uint32_t)(gas_filtrado * 12.0f);
                if (tom_frequencia < 400) tom_frequencia = 400;

                __HAL_TIM_SET_AUTORELOAD(&htim2, tom_frequencia);
                __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, tom_frequencia / 2);
                HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
                som_ligado = 1;
            } else {
                HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
                som_ligado = 0;
            }
        }
    } else {
        HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
        som_ligado = 0;
    }
}

// =========================================================================
// [REQUISITO] MENU COM OPÇÕES DE VISUALIZAÇÃO OPERACIONAIS
// =========================================================================
void atualizar_display_maquina_estados(void) {
    char buffer_linha[21];

    switch (estado_atual) {
        case ESTADO_MONITORAMENTO:
            lcd_gotoxy(&my_lcd, 0, 0);
            sprintf(buffer_linha, "GasF: %.1f %%      ", gas_filtrado);
            lcd_puts(&my_lcd, buffer_linha);

            lcd_gotoxy(&my_lcd, 0, 1);
            sprintf(buffer_linha, "%.1fC | %.1fF     ", temperatura_celsius, temperatura_fahrenheit);
            lcd_puts(&my_lcd, buffer_linha);
            break;

        case ESTADO_AJUSTE_SENS:
            {
                uint32_t tolerancia = (6 - sensibilidade_temp) * 20;
                lcd_gotoxy(&my_lcd, 0, 0);
                sprintf(buffer_linha, "Sensib. Gas: %lu ", sensibilidade_temp);
                lcd_puts(&my_lcd, buffer_linha);

                lcd_gotoxy(&my_lcd, 0, 1);
                sprintf(buffer_linha, "Toler.: %lumV   ", tolerancia);
                lcd_puts(&my_lcd, buffer_linha);
            }
            break;

        case ESTADO_DIAGNOSTICO:
            lcd_gotoxy(&my_lcd, 0, 0);
            sprintf(buffer_linha, "Cons: %.5f kWh  ", consumo_kwh);
            lcd_puts(&my_lcd, buffer_linha);

            lcd_gotoxy(&my_lcd, 0, 1);
            sprintf(buffer_linha, "ADC G:%lu T:%lu", valor_sensor, valor_temperatura_adc);
            lcd_puts(&my_lcd, buffer_linha);
            break;

        case ESTADO_CONFIG_MOTOR:
            lcd_gotoxy(&my_lcd, 0, 0);
            lcd_puts(&my_lcd, "LIMIAR DO MOTOR ");

            lcd_gotoxy(&my_lcd, 0, 1);
            sprintf(buffer_linha, "%% Motor: %lu %%   ", (uint32_t)porcentagem_motor_temp);
            lcd_puts(&my_lcd, buffer_linha);
            break;

        case ESTADO_CONFIG_ALARME:
            lcd_gotoxy(&my_lcd, 0, 0);
            lcd_puts(&my_lcd, "CONFIG. ALARME  ");

            lcd_gotoxy(&my_lcd, 0, 1);
            sprintf(buffer_linha, "Limiar: %lu %%    ", limiar_alerta_gas);
            lcd_puts(&my_lcd, buffer_linha);
            break;

        case ESTADO_QUALIDADE:
            lcd_gotoxy(&my_lcd, 0, 0);
            sprintf(buffer_linha, "IQAI: %.1f %%     ", qualidade_ambiente);
            lcd_puts(&my_lcd, buffer_linha);

            lcd_gotoxy(&my_lcd, 0, 1);
            sprintf(buffer_linha, "T.Seguro: %.0f min ", tempo_exposicao_segura);
            lcd_puts(&my_lcd, buffer_linha);
            break;

        case ESTADO_ALARME_CRITICO:
            lcd_gotoxy(&my_lcd, 0, 0);
            if (alarme_manual_ativo) {
                lcd_puts(&my_lcd, "*BOTAO INCENDIO*");
            } else {
                lcd_puts(&my_lcd, "!!! PERIGO !!!  ");
            }
            lcd_gotoxy(&my_lcd, 0, 1);
            sprintf(buffer_linha, "EVACUE: GAS %.1f%%", gas_filtrado);
            lcd_puts(&my_lcd, buffer_linha);
            break;
    }
}

// Leitura do canal isolado por amostragem limpa via software
uint32_t ADC_Ler_Canal(uint32_t canal) {
    ADC_ChannelConfTypeDef sConfig = {0};
    uint32_t resultado = 0;

    sConfig.Channel = canal;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;

    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        return 0;
    }

    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
        resultado = HAL_ADC_GetValue(&hadc1);
    }
    HAL_ADC_Stop(&hadc1);

    return resultado;
}
/* USER CODE END 0 */

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_I2C1_Init();

  /* USER CODE BEGIN 2 */
  HAL_TIM_Base_Start_IT(&htim3);

  // Inicializações preventivas contra estouro artificial do sensor de gás
  gas_filtrado = 0.0f;
  porcentagem_gas = 0.0f;

  my_lcd.hi2c = &hi2c1;
  my_lcd.address = (0x27 << 1);
  lcd_init(&my_lcd);
  lcd_clear(&my_lcd);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
      gerenciar_botoes_juntos_incendio();

      // =========================================================================
      // 1. LEITURAS EM BACKGROUND MULTICANAL (ADC)
      // =========================================================================
      valor_temperatura_adc = ADC_Ler_Canal(ADC_CHANNEL_4);
      valor_sensor = ADC_Ler_Canal(ADC_CHANNEL_0);
      valor_pot_buzzer = ADC_Ler_Canal(ADC_CHANNEL_2);

      // =========================================================================
      // 2. PROCESSAMENTO E EXECUÇÃO DOS MODELOS MATEMÁTICOS
      // =========================================================================
      float valor_limpo = 0.0f;
      if (valor_sensor > 1200) {
          valor_limpo = (float)(valor_sensor - 1200);
      } else {
          valor_limpo = 0.0f;
      }

      porcentagem_gas = (valor_limpo / 2895.0f) * 100.0f * (float)sensibilidade;
      if (porcentagem_gas > 100.0f) porcentagem_gas = 100.0f;

      float milivolts_temp = ((float)valor_temperatura_adc * 3300.0f) / 4095.0f;
      temperatura_celsius = (milivolts_temp / 10.0f) / 4.0f;

      gas_filtrado = filtro_media_movel_exponencial(porcentagem_gas, gas_filtrado);
      qualidade_ambiente = calcular_qualidade_ambiente(gas_filtrado, temperatura_celsius);
      limiar_alerta_gas = 20 + ((valor_pot_buzzer * 60) / 4095);

      temperatura_fahrenheit = converter_celsius_to_fahrenheit(temperatura_celsius);
      tempo_exposicao_segura = calcular_tempo_exposicao(gas_filtrado);

      milissegundos_ligado = HAL_GetTick();
      consumo_kwh = calcular_consumo_energetico(milissegundos_ligado);

      // =========================================================================
      // [REQUISITO] AS 3 PROTEÇÕES DO SISTEMA CRÍTICO
      // =========================================================================
      if (gas_filtrado >= (float)limiar_alerta_gas || alarme_manual_ativo) {
          if (estado_atual != ESTADO_ALARME_CRITICO && (HAL_GetTick() - tempo_ultimo_clique_exti > 2000) && (HAL_GetTick() - tempo_voltar_debounce > 2000)) {
              lcd_clear(&my_lcd);
              estado_atual = ESTADO_ALARME_CRITICO;
          }
      }
      else if (estado_atual == ESTADO_ALARME_CRITICO && gas_filtrado < (float)limiar_alerta_gas && !alarme_manual_ativo) {
          lcd_clear(&my_lcd);
          estado_atual = ESTADO_MONITORAMENTO;
      }

      if (gas_filtrado > (float)porcentagem_motor || temperatura_celsius > 40.0f || alarme_manual_ativo) {
          HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET);
      } else {
          HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);
      }

      gerenciar_botao_voltar_polling();

      // Sincroniza o potenciômetro físico imediatamente após mudar de tela
      if (forcar_atualizacao_pot_anterior) {
          valor_pot_anterior = ADC_Ler_Canal(ADC_CHANNEL_1);
          forcar_atualizacao_pot_anterior = 0;
      }

      switch(estado_atual) {
          case ESTADO_AJUSTE_SENS:
              valor_potenciometro = ADC_Ler_Canal(ADC_CHANNEL_1);
              if (trava_pot) {
                  int32_t diferenca = (int32_t)valor_potenciometro - (int32_t)valor_pot_anterior;
                  if (diferenca > 60 || diferenca < -60) trava_pot = 0;
              }
              if (!trava_pot) sensibilidade_temp = obter_sensibilidade(valor_potenciometro);
              valor_pot_anterior = valor_potenciometro;
              break;

          case ESTADO_CONFIG_MOTOR:
              valor_potenciometro = ADC_Ler_Canal(ADC_CHANNEL_1);
              if (trava_pot) {
                  int32_t diferenca = (int32_t)valor_potenciometro - (int32_t)valor_pot_anterior;
                  if (diferenca > 60 || diferenca < -60) trava_pot = 0;
              }
              if (!trava_pot) porcentagem_motor_temp = obter_motor(valor_potenciometro);
              valor_pot_anterior = valor_potenciometro;
              break;

          default:
              break;
      }

      // =========================================================================
      // 5. ATUADORES DE SAÍDA & GRAFICO DE BARRAS DE LEDS
      // =========================================================================
      gerenciar_buzzer_temporizado();

      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8|GPIO_PIN_9, GPIO_PIN_RESET);
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15, GPIO_PIN_RESET);

      if (gas_filtrado >= 5.0f || alarme_manual_ativo)  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_SET);
      if (gas_filtrado >= 25.0f || alarme_manual_ativo) HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);
      if (gas_filtrado >= 50.0f || alarme_manual_ativo) HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
      if (gas_filtrado >= 75.0f || alarme_manual_ativo) HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9,  GPIO_PIN_SET);
      if (gas_filtrado >= 90.0f || alarme_manual_ativo) HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8,  GPIO_PIN_SET);

      atualizar_display_maquina_estados();
      HAL_Delay(30);
  }
  /* USER CODE END WHILE */
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;

  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON; // CORREÇÃO: Removida linha duplicada aqui
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

void MX_ADC1_Init(void)
{
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE; // CORREÇÃO: Alterado para DISABLE para leitura manual estável por canal
  hadc1.Init.ContinuousConvMode =  DISABLE;
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
}

void MX_TIM2_Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 96 - 1;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 1000 - 1;
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

  HAL_TIM_MspPostInit(&htim2);
}

void MX_TIM3_Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 95;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 9999;
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
}

void MX_I2C1_Init(void)
{
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
}

void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7|GPIO_PIN_8|GPIO_PIN_9, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15|GPIO_PIN_8, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_7|GPIO_PIN_8|GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15|GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}
