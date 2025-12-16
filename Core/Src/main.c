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
#include "adc.h"
#include "dma.h"
#include "i2c.h"
#include "tim.h"
#include "gpio.h"

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

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
	/*========================================*/
	/*           Global  Variables            */

	float Vd        	 		    = 1.6e-3f;
	float densidade_ar              = 1.18f;
	float AFR       			    = 14.7f;
	float LHV       			    = 44e6f;
	float eficiencia_termica_motor  = 0.28f;
	float inercia_motor    	 	    = 0.15f;
	float raio_roda   			    = 0.34f;
	float massa      			    = 800.0f;
	float Cd        			    = 0.32f;     // Coef. arrasto aerodinâmico
	float area_frontal			    = 1.5f;
	float Crr       			    = 0.015f;    // Coef. rolamento
	float gravidade         	    = 9.81f;
	float i_fd      			    = 3.9f;      // Relação diferencial
	float eficiencia_transmissao    = 0.95f;
	float dt        			    = 0.01f;
	float gears[6] 				    = {8.0f,5.5f, 3.5f, 2.8f, 2.0f, 1.5f};
	int gear_index 				    = 0;
	//---------------------------------------------------------------------------
	float VE 					    = 0;
	float massa_ar 				    = 0;
	float massa_combustivel 	    = 0;
	float Potencia 				    = 0;
	float velocidade_angular_local  = 0;
	float T 					    = 0;
	float RPM             		    = 0;
	float Friccao_motor 		    = 0;
	float i_gear 				    = 0;
	float velocidade_angular_roda   = 0;
	float velocidade_angular_target = 0;
	float velocidade_angular_diff   = 0;
	float T_embreagem               = 0;
	float alpha_eng                 = 0;
	float T_eng                     = 0;
	float k_embreagem               = 200.0f;
	float velocidade_angular        = 0;
	float T_roda                    = 0;
	float F_ativa                   = 0;
	float F_arrasto                 = 0;
	float F_atrito                  = 0;
	float F_freio  					= 0;
	float F_freio_max 				= 8000.0f;
	float F_resistiva_total         = 0;
	float a                         = 0;
	float velocidade_angular_idle   = 800.0f * 2.0f * M_PI / 60.0f;
	float v                         = 0.0f;
	float vkm_h						= 0.0f;
	float x                         = 0.0f;
	float embreagem                 = 0.0f;
	float accel_pedal               = 0.0f;
	float brake_pedal               = 0.0f;


	float calc_engine_torque(float RPM, float Pedal);
	float calc_VE(float RPM, float Pedal);
	void  principal(void);
	void  BuzzerAlert_Update(float rpm);
	void  led_RPM(float rpm);
	void painel_lcd(void);





	/*========================================*/


	uint16_t adcBuffer[4];
	float Pedal_embreagem=0;
	float Pedal_freio=0;
	float Pedal_acelerador=0;
	float volante = 0;
	uint32_t timer_clk 				= 1000000;
		uint32_t period					=0;
		float duty  					=0;
		float freq						=0;
	uint32_t lastPress4 = 0;
	uint32_t lastPress5 = 0;
	uint32_t t_prev = 0;
	uint32_t t_now =0;
	int timer = 0;

	int painel   = 1;
	int exibicao = 1;



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
  MX_TIM11_Init();
  MX_TIM3_Init();
  MX_I2C1_Init();
  MX_TIM10_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_PWM_Start(&htim11, TIM_CHANNEL_1);
  HAL_TIM_Base_Start_IT(&htim3);
  HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adcBuffer, 4);
  lcd_init();
  HAL_Delay(50);
  lcd_put_cursor(0,0);
  lcd_send_string("Simulador");
  lcd_put_cursor(1, 0);
  lcd_send_string("Corrida");
  HAL_Delay(1000);
  lcd_clear();
  lcd_put_cursor(0, 0);
  lcd_send_string("Selecione");
  lcd_put_cursor(1, 0);
  lcd_send_string("modo");
  HAL_Delay(1000);
  uint32_t now = 0;
  while(now-lastPress4 <= 10000||now-lastPress5<=10000){
	  now = HAL_GetTick();
	  lcd_clear();

	  switch(exibicao){
		  case 1 :
			  lcd_put_cursor(0,0);
			  lcd_send_string("Velocidade");
			  lcd_put_cursor(1, 0);
			  lcd_send_string("RPM");
			  break;
		  case 2:
			  lcd_put_cursor(0,0);
			  lcd_send_string("Forças Ativas");
			  lcd_put_cursor(1, 0);
			  lcd_send_string("Resistivas");
			  break;
		  case 3:
			  lcd_put_cursor(0,0);
			  lcd_send_string("Torque");
			  lcd_put_cursor(1, 0);
			  lcd_send_string("Efi. Volumetrica");
			  break;
		  case 4:
			  lcd_put_cursor(0,0);
			  lcd_send_string("Massa de Ar");
			  lcd_put_cursor(1, 0);
			  lcd_send_string("Massa combustível");
			  break;
		  case 5:
			  lcd_put_cursor(0,0);
			  lcd_send_string("Acelerador");
			  lcd_put_cursor(1, 0);
			  lcd_send_string("Embreagem");
			  break;
		  default:
			  lcd_put_cursor(0,0);
			  lcd_send_string("Erro!");
			  break;
	  }
	  HAL_Delay(1000);
  }
  painel = 0;
  lcd_clear();
  lcd_put_cursor(0,0);
  lcd_send_string("Iniciando...");
  HAL_Delay(2000);
  lcd_clear();
//variaveis que vao no LCD VELOCIDADE, RPM e MARCHA




  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1){
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  principal();
	  led_RPM(RPM);
	  painel_lcd();
	  BuzzerAlert_Update(RPM);



  }
}

float calc_VE(float RPM, float Pedal) {
    VE = Pedal * (0.9f * expf(-powf((RPM - 8000.0f) / 5000.0f, 2.0f)) + 0.2f);
    return VE;
}
float calc_engine_torque(float RPM, float Pedal) {
    VE = calc_VE(RPM, Pedal);
    massa_ar = densidade_ar * VE * Vd * RPM / 120.0f;
    massa_combustivel = massa_ar / AFR;
    Potencia = eficiencia_termica_motor  * massa_combustivel * LHV;
    velocidade_angular_local = RPM * 2.0f * M_PI / 60.0f;
    if (velocidade_angular_local < 1.0f) velocidade_angular_local = 1.0f;
    T = Potencia / velocidade_angular_local; // Torque (N·m)
    return T;
}


void principal(void){
	  t_now = HAL_GetTick();
	  float dt = (t_now - t_prev) / 1000.0f; // converte para segundos
	  t_prev = t_now;
	  if (dt<= 0.0f) dt = 0.001f;
	  RPM = velocidade_angular * 60.0f / (2.0f * M_PI);
	  // =========================
	  T_eng = calc_engine_torque(RPM, accel_pedal);

	  Friccao_motor = 7.0f + 0.005f * RPM;
	  if (accel_pedal < 0.05f && RPM < 900.0f)
		  T_eng = 20.0f;

	  // =========================

	  i_gear = gears[gear_index];
	  velocidade_angular_roda = v / raio_roda;
	  velocidade_angular_target = velocidade_angular_roda * i_gear * i_fd;
	  velocidade_angular_diff = velocidade_angular - velocidade_angular_target;


	  T_embreagem = k_embreagem * velocidade_angular_diff * embreagem;


	  if (fabsf(T_embreagem) > fabsf(T_eng)){

		  T_embreagem = T_eng * (T_embreagem > 0 ? 1 : -1);
	  }
	  // =========================
	  alpha_eng = (T_eng - T_embreagem - Friccao_motor) / inercia_motor;
	  velocidade_angular += alpha_eng * dt;
	  if (velocidade_angular < velocidade_angular_idle)velocidade_angular = velocidade_angular_idle;
	  // =========================
	  T_roda = T_embreagem * i_gear * i_fd * eficiencia_transmissao;

	  F_ativa = T_roda / raio_roda;
	  F_arrasto = 0.5f * densidade_ar * Cd * area_frontal * v * v;
	  F_atrito = massa * gravidade * Crr;
	  F_freio = brake_pedal * F_freio_max;
	  F_resistiva_total = F_arrasto + F_atrito + F_freio;
	  // =========================
	  // 5. Aceleração e movimento
	  // =========================
	  a = (F_ativa - F_resistiva_total) / massa;
	  v += a * dt;
	  vkm_h = v *3.6;
	  if (v < 0.0f) v = 0.0f;

	  x += v * dt;
}
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin){
    uint32_t now = HAL_GetTick();
    if(painel == 0){
		if (GPIO_Pin == GPIO_PIN_4 && (now - lastPress4 > 100)) {
			lastPress4 = now;
			if (gear_index > 0) gear_index--;
		}
		else if (GPIO_Pin == GPIO_PIN_5 && (now - lastPress5 > 100)) {
			lastPress5 = now;
			if (gear_index < 5) gear_index++;
		}
    }
    else{
    	if (GPIO_Pin == GPIO_PIN_4 && (now - lastPress4 > 100)) {
    		lastPress4 = now;
    		if (exibicao > 1) exibicao--;
    		else exibicao = 5;

    	}
		else if (GPIO_Pin == GPIO_PIN_5 && (now - lastPress5 > 100)) {
			lastPress5 = now;
			if(exibicao < 5) exibicao++;
			else exibicao=1;
		}

    }
}
void led_RPM(float rpm){
	(rpm >= 900 )  ? HAL_GPIO_WritePin(GPIOA,GPIO_PIN_12,GPIO_PIN_SET) : HAL_GPIO_WritePin(GPIOA,GPIO_PIN_12,GPIO_PIN_RESET);
	(rpm >= 2000)  ? HAL_GPIO_WritePin(GPIOA,GPIO_PIN_11,GPIO_PIN_SET) : HAL_GPIO_WritePin(GPIOA,GPIO_PIN_11,GPIO_PIN_RESET);
	(rpm >= 4000)  ? HAL_GPIO_WritePin(GPIOA,GPIO_PIN_10,GPIO_PIN_SET) : HAL_GPIO_WritePin(GPIOA,GPIO_PIN_10,GPIO_PIN_RESET);
	(rpm >= 5000)  ? HAL_GPIO_WritePin(GPIOA,GPIO_PIN_9,GPIO_PIN_SET)  : HAL_GPIO_WritePin(GPIOA,GPIO_PIN_9,GPIO_PIN_RESET);
	(rpm >= 8000)  ? HAL_GPIO_WritePin(GPIOA,GPIO_PIN_8,GPIO_PIN_SET)  : HAL_GPIO_WritePin(GPIOA,GPIO_PIN_8,GPIO_PIN_RESET);
	(rpm >= 10000) ? HAL_GPIO_WritePin(GPIOB,GPIO_PIN_15,GPIO_PIN_SET) : HAL_GPIO_WritePin(GPIOB,GPIO_PIN_15,GPIO_PIN_RESET);
}
void painel_lcd(void){
	char buffer[20];
	//HAL_Delay(100);
	if(timer==2000){
		lcd_clear();
		 switch(exibicao){
			  case 1 :
				  lcd_put_cursor(0,0);
					snprintf(buffer, sizeof(buffer), "RPM: %.2f", RPM);
					lcd_send_string(buffer);

					lcd_put_cursor(1,0);
					snprintf(buffer, sizeof(buffer), "Vel: %.2f km/h", vkm_h);
					lcd_send_string(buffer);
				  break;
			  case 2:
				  lcd_put_cursor(0,0);
					snprintf(buffer, sizeof(buffer), "F. Ativa: %.2f", F_ativa);
					lcd_send_string(buffer);

					lcd_put_cursor(1,0);
					snprintf(buffer, sizeof(buffer), "F. Resist.: %.2f ", F_resistiva_total);
					lcd_send_string(buffer);
				  break;
			  case 3:
				  lcd_put_cursor(0,0);
					snprintf(buffer, sizeof(buffer), "Torque: %.2f", T_eng);
					lcd_send_string(buffer);

					lcd_put_cursor(1,0);
					snprintf(buffer, sizeof(buffer), "Efic. Vol. %.2f ", VE);
					lcd_send_string(buffer);
				  break;
			  case 4:
				  lcd_put_cursor(0,0);
					snprintf(buffer, sizeof(buffer), "Massa Ar: %.2f g", massa_ar);
					lcd_send_string(buffer);

					lcd_put_cursor(1,0);
					snprintf(buffer, sizeof(buffer), "Massa Comb.: %.2f g", massa_combustivel);
					lcd_send_string(buffer);
				  break;
			  case 5:
				  lcd_put_cursor(0,0);
					snprintf(buffer, sizeof(buffer), "Acelerador: %.2f", accel_pedal);
					lcd_send_string(buffer);

					lcd_put_cursor(1,0);
					snprintf(buffer, sizeof(buffer), "Embreagem: %.2f", embreagem);
					lcd_send_string(buffer);
				  break;
			  default:
				  lcd_put_cursor(0,0);
				  lcd_send_string("Erro!");
			  }
		 timer =0;
		}
	timer++;
}
void BuzzerAlert_Update(float rpm) {
    if (rpm < 5000.0f) {
        HAL_TIM_PWM_Stop(&htim11, TIM_CHANNEL_1);
        return;
    }
    else{
    	freq = 300.0f + (rpm - 5000.0f) * 0.24f;
    	if (freq > 1500.0f) freq = 1500.0f;

    	period = (uint32_t)(timer_clk / freq);
    	htim11.Instance->ARR = period;

    	duty = 0.10f + (rpm - 5000.0f) * (0.30f / 5000.0f);
    	if (duty > 0.40f) duty = 0.40f;
    	htim11.Instance->CCR1 = (uint32_t)(period * duty);
    	HAL_TIM_PWM_Start(&htim11, TIM_CHANNEL_1);
    }

}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim){
			if (htim->Instance == TIM3){
				volante     = roundf((adcBuffer[0] / 4095.0f)*100.0f)/100.0f;
				embreagem   = roundf(((4095-adcBuffer[1]) / 4095.0f)*100.0f)/100.0f;
				brake_pedal = roundf((adcBuffer[2] / 4095.0f)*100.0f)/100.0f;
				accel_pedal = roundf((adcBuffer[3] / 4095.0f)*100.0f)/100.0f;


			}
			if (htim->Instance == TIM10) {
			   BuzzerAlert_Update(RPM);



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
