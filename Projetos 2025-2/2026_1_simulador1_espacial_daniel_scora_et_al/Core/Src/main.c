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
#include "math.h"
#include "i2c_lcd.h"
#include "stdio.h"

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
I2C_LCD_HandleTypeDef lcd1;
I2C_LCD_HandleTypeDef lcd2;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM4_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */


/* VARIAVEIS FISICAS */
float fx = 0,
	  fy = 0,
	  ax = 0,
	  ay = 0,
	  modulo_a = 0,
	  vx = 0,
	  vy = 0,
	  modulo_v = 0,
	  sx = 0,
	  sy = 0,
	  modulo_s = 0,
	  massa_foguete = 0,
	  angulo_foguete = 0, //0 a 2pi
	  modulo_empuxo = 0,
	  g = 0,
	  tempo = 0,
	  altura = 0,
	  densidade_do_ar = 0,
	  pressao_atmosferica,
	  resistencia_do_ar = 0,
	  semi_major_axis = 0,
	  apoastro = 0,
	  periastro = 0,
	  excentricidade = 0,
	  periodo_orbital = 0,
	  intensidade_motor = 0;

/* VARIAVEIS ADC */
uint16_t valores_adc[3];

/* CONSTANTES */
float mu = 3.986 * powf(10.0f, 14.0f),
      raio_terra = 6371008,
	  pi = 3.1415926,
	  time_step = 1,
	  g0 = 9.80665;

/* FLAGS */

int no_chao = 1,
	fisica = 0,
	orbita = 0,
	avancar_estagio = 0,
	time_warp = 1,
	estagio_atual = 0,
	pwm_crescendo = 1;

/* VARIAVEIS LCD */
int menu = 0,
	lcd = 0;
char buffer[64];

/* VARIAVEIS BOTAO */
int botao1 = 1,
	botao2 = 1,
	botao3 = 1,
	botao4 = 1,
	botao_pressionado = 1;

/* STRUCTS */

typedef struct {
	float massa_seca,      //massa sem combustivel
		  massa_liquida,   //massa com combustivel
		  empuxo,	  	   //empuxo em newtons, empuxo dinamico
		  empuxo_vac,	   //empuxo em newtons, vacuo
		  isp,			   //empuxo especifico variavel
		  isp_vac,         //empuxo especifico em segundos, vacuo
		  consumo,		   //consumo de combustivel, kg/s ( empuxo / (Isp * g) )
		  draco,           //Coeficiente de atrito
		  area_transversal,//area na secao vertical, usada no atrito
		  area_bico;	   //area do bico do motor para o calculo do empuxo
} Estagio;

//MAQUINA DE ESTADOS

typedef enum {
	PRE_LANCAMENTO,
	ESTAGIO_1,
    ESTAGIO_2,
	ESTAGIO_3,
    EM_ORBITA,
    FALHA_COLISAO
} Estado_Foguete;

Estado_Foguete estado_atual = PRE_LANCAMENTO;
/* FUNCOES */

float modulo(float a, float b) {
	return sqrtf(a*a+b*b);
}

//se pressao = 1 retorna a pressao atmosferica em Pa, se 0 retorna a densidade atmosferica
float densidade_ar(float altura, int pressao) {
    float t, p;
    if(altura>=25000) {
        t = -131.21 + (0.00299 * altura);
        p = 2.488 * powf(((t+273.15)/216.6), -11.388);
    } else if(altura<25000 && altura>11000) {
        t = -56.46;
        p = 22.65 * expf(1.73 - (0.000157 * altura));
    } else if(altura<=11000) {
        t = 15.04 - (0.00649 * altura);
        p = 101.29 * powf(((t+273.15)/288.08), 5.256);
    }

    if(pressao == 0 ) {
    	return p / (0.2869 * (t+273.15));
    } else {
    	return p*1000.0;
    }

}


float resistencia_ar(float densidade, float area, float draco, float velocidade) {
	return 0.5 * densidade * area * draco * powf(velocidade, 2);
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
  MX_TIM3_Init();
  MX_TIM4_Init();
  MX_I2C1_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_Base_Start_IT(&htim3);
  HAL_TIM_Base_Start_IT(&htim4);
  HAL_ADC_Start_DMA(&hadc1, (uint32_t*)valores_adc, 3);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
  htim2.Instance->CCR1 = 0;
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);
  htim2.Instance->CCR3 = 0;
  sy = raio_terra+10;


  lcd1.hi2c = &hi2c1;
  lcd1.address = 0x27<<1;
  lcd_init(&lcd1);
  lcd2.hi2c = &hi2c1;
  lcd2.address = 0x26<<1;
  lcd_init(&lcd2);


  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, 1); //indica o primeiro estagio ligado

  //declarando as especificacoes do foguete

  Estagio foguete[3];
  foguete[0].massa_seca = 137000;
  foguete[0].massa_liquida = 2077000;
  foguete[0].empuxo = 0;
  foguete[0].empuxo_vac = 38850000;
  foguete[0].isp = 0;
  foguete[0].isp_vac = 304;
  foguete[0].consumo = 0;
  foguete[0].draco = 0.4;
  foguete[0].area_transversal = 78.8;
  foguete[0].area_bico = 56.5;
  foguete[1].massa_seca = 43000;
  foguete[1].massa_liquida = 427000;
  foguete[1].empuxo = 0;
  foguete[1].empuxo_vac = 5165500;
  foguete[1].isp = 0;
  foguete[1].isp_vac = 421;
  foguete[1].consumo = 0;
  foguete[1].draco = 0.4;
  foguete[1].area_transversal = 78.8;
  foguete[1].area_bico = 15.1;
  foguete[2].massa_seca = 15200;
  foguete[2].massa_liquida = 105300;
  foguete[2].empuxo = 0;
  foguete[2].empuxo_vac = 1033100;
  foguete[2].isp = 0;
  foguete[2].isp_vac = 421;
  foguete[2].consumo = 0;
  foguete[2].draco = 0.4;
  foguete[2].area_transversal = 34.2;
  foguete[2	].area_bico = 56.5;



  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {



	  if(fisica) {

	  fisica = 0;


	  //colocamos as leituras ADC e dos botoes dentro da flag de fisica para
	  // - pegar as leituras do ADC 60 vezes por segundo
	  // - e so atualizar os botoes a cada 16 ms pra previnir debouncing

	  HAL_ADC_Start_DMA(&hadc1, (uint32_t*)valores_adc, 3);

	  botao1 = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7);
	  botao2 = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6);
	  botao3 = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_5);
	  botao4 = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4);

	  switch(estado_atual) {
	  case PRE_LANCAMENTO:
		  no_chao = 1;
		  intensidade_motor = 0;
		  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, 0);
		  if(botao3 == 0 && botao_pressionado == 1) {
	      no_chao = 0;
	      estagio_atual = 0;
	      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, 1); // LED estagio 1
	      estado_atual = ESTAGIO_1;
	      botao3 = 1; //reseta pra nao conflitar com o check la embaixo
	      botao_pressionado = 0;
	      }
	  break;
	  case ESTAGIO_1:
		  // se acabou o combustivel e apertou o botao 2 passa pro proximo estagio
	      if(botao2 == 0 && botao_pressionado == 1) {
	      estagio_atual = 1;
	      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, 1); // LED estagio 2
	      estado_atual = ESTAGIO_2;
	      }
	   break;
	  case ESTAGIO_2:
		  // passa pro proximo estagio
		  if(botao2 == 0 && botao_pressionado == 1) {
	      estagio_atual = 2;
	      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, 1); // LED estagio 3
	      estado_atual = ESTAGIO_3;
	      }
	  break;
	  case ESTAGIO_3:
	      //se o periastro tiver acima de 100km ele ta em orbita
		  if(periastro > 100000.0f && altura > 60000.0f) {
			  estado_atual = EM_ORBITA;
	      }
	  break;
	  case EM_ORBITA:
	  //acende um led de orbita
	      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, 1);
	      if(periastro < 100000.0f) {
	      			  estado_atual = ESTAGIO_3;
	      	      }
	  break;
	  case FALHA_COLISAO:
		  intensidade_motor = 0; // desliga o motor e zera a velocidade alem de colocar no chao (parar a fisica)
		  no_chao = 1;
		  vx = 0; vy = 0; ax = 0; ay = 0;
		  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, 0);
	   break;
	  }

	  time_step = (1.0 / 60.0) * time_warp;
	  fx = 0;
	  fy = 0;

	  modulo_a = modulo(ax, ay);
	  modulo_v = modulo(vx, vy);
	  modulo_s = modulo(sx, sy);
	  modulo_empuxo = foguete[estagio_atual].empuxo;

	  massa_foguete = 0;
	  for(int i = estagio_atual; i<3; i++) {
		  massa_foguete += foguete[i].massa_liquida;
		  massa_foguete += foguete[i].massa_seca;
	  }

	  altura = modulo_s - raio_terra;

	  densidade_do_ar = densidade_ar(altura, 0);
	  pressao_atmosferica = densidade_ar(altura, 1);
	  resistencia_do_ar = resistencia_ar(densidade_do_ar, foguete[estagio_atual].area_transversal, foguete[estagio_atual].draco, modulo_v);


	  /* Calculo da gravidade */

	  g = (mu * massa_foguete) / powf((modulo_s), 2);

	  //protecao 1
	  //se a altura for menor que 0 entao ele caiu, aciona o estado de falha
	  if( altura < 0 ) {
		  estado_atual = FALHA_COLISAO;
	  }

	  //protecao 2
	  //pressao aerodinamica maxima, se a resistencia do ar passar de 800 kN antes dos 15km de altitude
	  //o foguete desintegra pelas forcas extremas, aciona o estado de falha
		  // pwm que indica o quao perto do limite o foguete esta
		  // i.e. quanto mais forte o LED mais extrema a forca da resistencia do ar
		  htim2.Instance->CCR3 = (htim2.Instance->ARR) * resistencia_do_ar / 800000.0f;
		  if(htim2.Instance->CCR3 > htim2.Instance->ARR) htim2.Instance->CCR3 = htim2.Instance->ARR;
	  if(resistencia_do_ar > 800000.0f && altura < 15000.0f) {
		  estado_atual = FALHA_COLISAO;
		  lcd_clear(&lcd1);
		  menu = 4;
	  }

	  //protecao 3
	  //se acabar o combustivel piscar o led de alerta 1
	  if (foguete[estagio_atual].massa_liquida <= 0.0f)
      {

		  if(pwm_crescendo) {
			  //aumenta o brilho do LED
			  htim2.Instance->CCR1 += (htim2.Instance->ARR)/10;

			  if(htim2.Instance->CCR1 > htim2.Instance->ARR) {
				  htim2.Instance->CCR1 = htim2.Instance->ARR;
				  pwm_crescendo = 0;
			  }

		  } else {
			  //diminui o brilho do LED
			  htim2.Instance->CCR1 -= (htim2.Instance->ARR)/10;

			  if( htim2.Instance->CCR1 <= (htim2.Instance->ARR)/10 ) {
				  htim2.Instance->CCR1 = 0;
				  pwm_crescendo = 1;
			  }

		  }
	  } else {
		  htim2.Instance->CCR1 = 0;
		  pwm_crescendo = 1;
	  }

	  //LEDs de intensidade do motor, A8-A12
	  if(intensidade_motor > 0 && intensidade_motor <= 0.2) {
		  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, 1);
		  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12, 0);
	  } else if(intensidade_motor > 0.2 && intensidade_motor <= 0.4) {
		  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8 | GPIO_PIN_9, 1);
		  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12, 0);
	  } else if(intensidade_motor > 0.4 && intensidade_motor <= 0.6) {
		  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10, 1);
		  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11 | GPIO_PIN_12, 0);
	  } else if(intensidade_motor > 0.6 && intensidade_motor <= 0.8) {
		  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11, 1);
		  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, 0);
	  } else if(intensidade_motor > 0.8) {
		  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12, 1);
	  } else {
		  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12, 0);
	  }

	  //fisica so atualiza nos estados em que ele se move
	  if( estado_atual != PRE_LANCAMENTO && estado_atual != FALHA_COLISAO && no_chao == 0) {
      //ainda deixa o no_chao no if pra quando o usuario congelar o tempo

	  /* Calculo do vetor unitario do sentido da gravidade */

	  fx +=  g * (-sx/modulo_s) + resistencia_do_ar * (-vx/(modulo_v+0.00001)) + foguete[estagio_atual].empuxo * sin(angulo_foguete);
	  fy +=  g * (-sy/modulo_s) + resistencia_do_ar * (-vy/(modulo_v+0.00001)) + foguete[estagio_atual].empuxo * cos(angulo_foguete);

	  ax = fx/massa_foguete;
	  ay = fy/massa_foguete;

	  vx += ax * time_step;
	  vy += ay * time_step;

	  sx += vx * time_step;
	  sy += vy * time_step;

	  //avanco do foguete

	  // Avanço do foguete (Cálculo do empuxo atual com perda atmosférica)
	  	foguete[estagio_atual].empuxo = foguete[estagio_atual].empuxo_vac - (foguete[estagio_atual].area_bico * pressao_atmosferica);
	  	foguete[estagio_atual].empuxo = foguete[estagio_atual].empuxo * intensidade_motor; // Este é o F_total


	  	// Proteção para não dividir por zero quando o motor estiver desligado (intensidade_motor == 0)
	  	if (foguete[estagio_atual].empuxo > 0.0f)
	  	{
	  		foguete[estagio_atual].isp = foguete[estagio_atual].isp_vac * (foguete[estagio_atual].empuxo / (foguete[estagio_atual].empuxo_vac * intensidade_motor));
	  	}
	  	else
	  	{
	  		foguete[estagio_atual].isp = 0.0f;
	  	}

	  	// Consumo de combustível e queima
	  	if (foguete[estagio_atual].empuxo > 0.0f && foguete[estagio_atual].isp > 0.0f)
	  	{
	  		foguete[estagio_atual].consumo = foguete[estagio_atual].empuxo / (foguete[estagio_atual].isp * g0);
	  		foguete[estagio_atual].massa_liquida -= foguete[estagio_atual].consumo * time_step;

	  		// Garante que o combustível não fique negativo
	  		if (foguete[estagio_atual].massa_liquida < 0.0f)
	  		{
	  			foguete[estagio_atual].massa_liquida = 0.0f;
	  		}
	  	} else {
	  		foguete[estagio_atual].consumo = 0.0f;
	  	}
        // precisa zerar o empuxo quando o combustível acabar
        if (foguete[estagio_atual].massa_liquida <= 0.0f)
        {
            foguete[estagio_atual].empuxo = 0.0f;
        }

	  tempo += time_step;

	  }

	  }

		uint16_t valor1 = valores_adc[0]; // valor utilizado para a intensidade to motor
	    uint16_t valor2 = valores_adc[1]; // valor utilizado para angulo do foguete
	    uint16_t valor3 = valores_adc[2]; // valor utilizado para o timewarp

	    intensidade_motor = valor1 / 4095.0f;
	    if(intensidade_motor < 0.1) intensidade_motor = 0;
	    angulo_foguete = valor2 / 4095.0f;
	    angulo_foguete = angulo_foguete * pi * 2;
	    valor3 /= 409.5f;

	    if(valor3 <= 2) {
	    	time_warp = 1;
	    } else if(valor3 > 2 && valor3 <= 4) {
	    	time_warp = 2;
	    } else if(valor3 > 4 && valor3 <= 6) {
	    	time_warp = 3;
	    } else if(valor3 > 6 && valor3 <= 8) {
	    	time_warp = 4;
	    } else if(valor3 > 8) {
	    	time_warp = 5;
	    } else {
	    	time_warp = 1;
	    }
	    if(angulo_foguete < 0.1) angulo_foguete = 0;


	  //MENU DISPLAY LCD
	  if(lcd) {
		  lcd = 0;
		  switch(menu) {
		  case 0:
			  //deslocamento e motores
			  //lcd 1
			  lcd_gotoxy(&lcd1, 0, 0);
		  	  sprintf(buffer, "Alt: %.2f m     ", altura);
		  	  lcd_puts(&lcd1,  buffer);
		  	  lcd_gotoxy(&lcd1, 0, 1);
		  	  sprintf(buffer, "Vel: %.2f m/s      ", modulo_v);
		  	  lcd_puts(&lcd1,  buffer);
			  lcd_gotoxy(&lcd1, 0, 2);
		  	  sprintf(buffer, "Fuel: %.2f kT     ", foguete[estagio_atual].massa_liquida/1000000.0f);
		  	  lcd_puts(&lcd1,  buffer);
		  	  lcd_gotoxy(&lcd1, 0, 3);
		  	  sprintf(buffer, "Emp: %.2f kN     ", foguete[estagio_atual].empuxo/1000.0f);
		  	  lcd_puts(&lcd1,  buffer);
		  	  //lcd 2
		  	  lcd_gotoxy(&lcd2, 0, 0);
		  	  sprintf(buffer, "A: %.2f m     ", apoastro);
		  	  lcd_puts(&lcd2,  buffer);
		  	  lcd_gotoxy(&lcd2, 0, 1);
		  	  sprintf(buffer, "P: %.2f m     ", periastro);
		  	  lcd_puts(&lcd2,  buffer);
			  break;
		  case 1:
			  //atmosfera
			  //lcd 1
			  lcd_gotoxy(&lcd1, 0, 0);
			  sprintf(buffer, "P: %.2f kPa  ", pressao_atmosferica/1000.0f);
		  	  lcd_puts(&lcd1,  buffer);
		  	  lcd_gotoxy(&lcd1, 0, 1);
		  	  sprintf(buffer, "Ra: %.2f N", resistencia_do_ar);
		  	  lcd_puts(&lcd1,  buffer);
			  lcd_gotoxy(&lcd1, 0, 2);
			  sprintf(buffer, "D: %.2f kg/m3 ", densidade_do_ar);
		  	  lcd_puts(&lcd1,  buffer);
		  	  lcd_gotoxy(&lcd1, 0, 3);
		  	  sprintf(buffer, "Alt: %.2f m     ", altura);
		  	  lcd_puts(&lcd1,  buffer);
		  	//lcd 2
		  	  lcd_gotoxy(&lcd2, 0, 0);
		  	  sprintf(buffer, "Ac: %.2f m/s2     ", modulo_a);
		  	  lcd_puts(&lcd2,  buffer);
		  	  lcd_gotoxy(&lcd2, 0, 1);
		  	sprintf(buffer, "Vel: %.2f m/s      ", modulo_v);
		  	  lcd_puts(&lcd2,  buffer);
			  break;
		  case 2:
			  //perifericos/extras
			  //lcd 1
			  lcd_gotoxy(&lcd1, 0, 0);
		  	  sprintf(buffer, "T+: %.0f  ", tempo);
		  	  lcd_puts(&lcd1,  buffer);
		  	  lcd_gotoxy(&lcd1, 0, 1);
		  	  sprintf(buffer, "TW: %dx", time_warp);
		  	  lcd_puts(&lcd1,  buffer);
			  lcd_gotoxy(&lcd1, 0, 2);
		  	  sprintf(buffer, "Estagio: %d", estagio_atual+1);
		  	  lcd_puts(&lcd1,  buffer);
		  	  lcd_gotoxy(&lcd1, 0, 3);
		  	  sprintf(buffer, "Consumo: %.0f kg/s     ", foguete[estagio_atual].consumo);
		  	  lcd_puts(&lcd1,  buffer);
		  	//lcd 2
		  	  lcd_gotoxy(&lcd2, 0, 0);
		  	  sprintf(buffer, "Massa: %.1f T     ", massa_foguete/1000.0f);
		  	  lcd_puts(&lcd2,  buffer);
		  	  lcd_gotoxy(&lcd2, 0, 1);
		  	  sprintf(buffer, "ISP: %.1f s     ", foguete[estagio_atual].isp);
		  	  lcd_puts(&lcd2,  buffer);
			  break;
		  case 3:
			  //informacoes orbita
			  //lcd 1
			  lcd_gotoxy(&lcd1, 0, 0);
			  sprintf(buffer, "Po: %.2fh  ", periodo_orbital);
		  	  lcd_puts(&lcd1,  buffer);
		  	  lcd_gotoxy(&lcd1, 0, 1);
		  	sprintf(buffer, "SMA: %.2f m     ", semi_major_axis);
		  	  lcd_puts(&lcd1,  buffer);
			  lcd_gotoxy(&lcd1, 0, 2);
		  	  sprintf(buffer, "A: %.2f m     ", apoastro);
		  	  lcd_puts(&lcd1,  buffer);
		  	  lcd_gotoxy(&lcd1, 0, 3);
		  	  sprintf(buffer, "P: %.2f m    ", periastro);
		  	  lcd_puts(&lcd1,  buffer);
		  	//lcd 2
			  lcd_gotoxy(&lcd2, 0, 0);
		  	  sprintf(buffer, "Alt: %.2f m     ", altura);
		  	  lcd_puts(&lcd2,  buffer);
		  	  lcd_gotoxy(&lcd2, 0, 1);
		  	  sprintf(buffer, "Vel: %.2f m/s      ", modulo_v);
		  	  lcd_puts(&lcd2,  buffer);
		  	  break;
		  	  //e.g. falhas, colisoes,
		  case 4:
			  //alerta de falha critica/inacessivel pelo botao
			  //lcd 1
			  lcd_gotoxy(&lcd1, 0, 0);
			  sprintf(buffer, "FALHA CRITICA");
			  lcd_puts(&lcd1,  buffer);
			  break;
		  }
	  }


	  if(orbita) {
		  orbita = 0;

    	  semi_major_axis = (mu * modulo_s) / ( (2 * mu) - (modulo_s * modulo_v * modulo_v) );

    	  float h = (sx * vy) - (sy * vx);
    	  float sqrt_argument = 1.0 - ((h * h) / (mu * semi_major_axis));

    	  if (sqrt_argument < 0.0) sqrt_argument = 0.0;
    		   //PROTECAO RAIZ NEGATIVA

    	  excentricidade = sqrtf(sqrt_argument);

    	  apoastro = semi_major_axis * (1 + excentricidade) - raio_terra;
    	  periastro = semi_major_axis * (1 - excentricidade) - raio_terra;
    	  if(periastro<0) periastro = 0;

    	  periodo_orbital = 2 * pi * sqrtf( pow(semi_major_axis, 3) / (mu));
    	  periodo_orbital /= 60;
    	  periodo_orbital /= 60; //em horas

	  }

	  //logica dos botoes

	  if(botao1 == 0 && botao_pressionado == 1) {
		  lcd_clear(&lcd1);
		  lcd_clear(&lcd2);
		  menu++;
		  if(menu > 3) menu = 0;
	  }

	  if(botao3 == 0 && botao_pressionado == 1) {
		  if(no_chao == 1) {
			  no_chao = 0;
		  } else {
			  no_chao = 1;
		  }
	  }


	  if(botao4 == 0 && botao_pressionado == 1) {
		  //reseta para as condicoes iniciais
		  estado_atual = PRE_LANCAMENTO;
		  lcd_clear(&lcd1);
		  lcd_clear(&lcd2);
		  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14 | GPIO_PIN_13 | GPIO_PIN_12, 0);
		  menu = 0;
		  sx = 0; sy = raio_terra+10;
		  vx = 0; vy = 0; ax = 0; ay = 0;
		  estagio_atual = 0;
		  tempo = 0;
		  no_chao = 1;
		  foguete[0].massa_liquida = 2077000;
		  foguete[1].massa_liquida = 427000;
		  foguete[2].massa_liquida = 105300;
	  }

	  if((botao1 == 0) || (botao2 == 0) || (botao3 == 0) || (botao4 == 0)) {
		  botao_pressionado = 0;
	  } else {
		  botao_pressionado = 1;
	  }

    /* USER CODE END WHILE */
  }
}
    /* USER CODE BEGIN 3 */

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3)  //confirma que eh o timer correto
    {
  	  //RODA A CADA 0.25 S
    	orbita = 1;
    	lcd = 1;

    }
    if (htim->Instance == TIM4)  //confirma que eh o timer correto
    {
    	fisica = 1;

    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == GPIO_PIN_0) {
    	// se aperta o botao B0 aborta a missao
        // altera o estado para falha
        estado_atual = FALHA_COLISAO;
        menu = 4; //liga a a tela de falha
        lcd_clear(&lcd1);
		lcd_clear(&lcd2);

    }
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
  hadc1.Init.ScanConvMode = ENABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
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
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_15CYCLES;
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

	//TIM2 gera um PWM com frequencia de 10kHz
	//usado pra piscar (oscilar) os leds em emergencias/falhas

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 9599;
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
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
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
  htim3.Init.Prescaler = 9599;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 2499;
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
  htim4.Init.Prescaler = 511;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 3124;
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
  HAL_GPIO_WritePin(KIT_LED_GPIO_Port, KIT_LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11
                          |GPIO_PIN_12, GPIO_PIN_RESET);

  /*Configure GPIO pin : KIT_LED_Pin */
  GPIO_InitStruct.Pin = KIT_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(KIT_LED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PB0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PB12 PB13 PB14 PB15 */
  GPIO_InitStruct.Pin = GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PA8 PA9 PA10 PA11
                           PA12 */
  GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11
                          |GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : BOTAO4_Pin BOTAO3_Pin BOTAO2_Pin BOTAO1_Pin */
  GPIO_InitStruct.Pin = BOTAO4_Pin|BOTAO3_Pin|BOTAO2_Pin|BOTAO1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

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
