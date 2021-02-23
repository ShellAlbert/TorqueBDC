

/**
  ******************************************************************
  * @file	 main.c
  * @author  fire
  * @version V1.0
  * @date	 2020-xx-xx
  * @brief	 直流有刷减速电机-电压电流读取-MOS管搭建板
  ******************************************************************
  * @attention
  *
  * 实验平台:野火 STM32H743 开发板 
  * 论坛	  :http://www.firebbs.cn
  * 淘宝	  :http://firestm32.taobao.com
  *
  ******************************************************************
  */
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include "stm32h7xx.h"
#include "main.h"
#include "./led/bsp_led.h"
#include "./usart/bsp_debug_usart.h"
#include "./key/bsp_key.h"
#include "./tim/bsp_motor_tim.h"
#include "./motor_control/bsp_motor_control.h"
#include "./encoder/bsp_encoder.h"
#include "./adc/bsp_adc.h"

int 			pulse_num = 0;

// ADC1转换的电压值通过MDA方式传到SRAM
extern __IO uint16_t ADC_ConvertedValue;

// 局部变量，用于保存转换计算后的电压值	 
float			ADC_Vol;


//DONOT reference the current displayed on the screen of Agilent U8031A Power Supply
//it's not the real motor current.
//we should write a test app to monitor the real current when motor rotates clockwise and anti-clockwise.
#define CURRENT_OVER	500 //500mA
#define CURRENT_THROWOUT	200/*350*/ //350mA

void Delay(__IO uint32_t nCount) //简单的延时函数

{
	for (; nCount != 0; nCount--)
		;
}


void zsyDelay(__IO uint32_t nCount)
{
	int 			i, j;

	for (i = 0; i < 999999; i++) {
		for (j = 0; j < 999999; j++) {
			for (; nCount != 0; nCount--)
				;
		}
	}
}


/**
  * @brief	主函数
  * @param	无
  * @retval 无
  */
int main(void)
{
	__IO uint16_t	ChannelPulse = PWM_MAX_PERIOD_COUNT * 0.5;
	uint8_t 		i	= 0;
	uint8_t 		flag = 0;

	uint8_t 		bIsMotorEn = 0;
	uint32_t 		iRevEncCnt=0;

	HAL_Init();

	/* 初始化系统时钟为480MHz */
	SystemClock_Config();

	/* 串口初始化 */
	DEBUG_USART_Config();

	/* 初始化按键GPIO */
	Key_GPIO_Config();

	/* 初始化 LED */
	LED_GPIO_Config();

	/* 电机初始化 */
	motor_init();

	/* 编码器接口初始化 */
	Encoder_Init();


	/* ADC 始化 */
	ADC_Init();

	set_motor_speed(ChannelPulse);
	set_motor_disable();
	LED1_TOGGLE;

	//add by zsy begin.
	while (1) {

		if (Key_Scan(KEY1_GPIO_PORT, KEY1_PIN) == KEY_ON) {
			set_motor_direction(MOTOR_FWD); 		//shouxian.
			set_motor_enable();
			bIsMotorEn			= 1;
			iRevEncCnt=0;

			//skip startup big current.
			HAL_Delay(2000);
		}

		if (Key_Scan(KEY2_GPIO_PORT, KEY2_PIN) == KEY_ON) {
			set_motor_disable();
			bIsMotorEn			= 0;
		}


		//read motor current every 50ms.
		if (bIsMotorEn) {
			if (HAL_GetTick() % /*50*/100 == 0 && flag == 0) {
				flag				= 1;


				int32_t Capture_Count = 0;	  // 当前时刻总计数值
				
				/* 当前时刻总计数值 = 计数器值 + 计数溢出次数 * ENCODER_TIM_PERIOD  */
				Capture_Count = __HAL_TIM_GET_COUNTER(&TIM_EncoderHandle) + (Encoder_Overflow_Count * ENCODER_TIM_PERIOD);
				

				int32_t 		iCurrent = get_curr_val();
				float			fVoltage = get_vbus_val();

				//motor over current protection.
				//if current is greater than ?A then stop immediately.
				if (iCurrent > CURRENT_OVER) {
					set_motor_disable();
					bIsMotorEn			= 0;
					printf("OVC:(%.2fV/%dmA,%d)\r\n", fVoltage, iCurrent,Capture_Count);

				}
				else if (iCurrent < CURRENT_THROWOUT) {
					printf("GetBack:(%.2fV/%dmA,%d)\r\n", fVoltage, iCurrent,Capture_Count);
					set_motor_direction(MOTOR_FWD);
					iRevEncCnt=0;

				}
				else{
					iRevEncCnt++;
					printf("ThrowOut:(%.2fV/%dmA,%d/%d)\r\n", fVoltage, iCurrent,Capture_Count,iRevEncCnt);

					//first throw out 3000ms then try to get back.
					set_motor_direction(MOTOR_REV);

					//skip reverse rotate huge current
					//plus 2000ms period of reverse rotate.
					HAL_Delay(3000);
					set_motor_direction(MOTOR_FWD); //shouxian.

					//skip reverse rotate huge current
					HAL_Delay(1000);
				}
			}
			else if (HAL_GetTick() % /*50*/100 != 0 && flag == 1) {
				flag				= 0;
			}
		}
	}

	//add by zsy end.
	while (1) {


		/* 扫描KEY1 */
		if (Key_Scan(KEY1_GPIO_PORT, KEY1_PIN) == KEY_ON) {
			/* 使能电机 */
			set_motor_enable();
		}


		/* 扫描KEY2 */
		if (Key_Scan(KEY2_GPIO_PORT, KEY2_PIN) == KEY_ON) {
			/* 禁用电机 */
			set_motor_disable();
		}

		/* 扫描KEY3 */
		if (Key_Scan(KEY3_GPIO_PORT, KEY3_PIN) == KEY_ON) {
			/* 增大占空比 */
			ChannelPulse		+= PWM_MAX_PERIOD_COUNT / 10;

			if (ChannelPulse > PWM_MAX_PERIOD_COUNT)
				ChannelPulse = PWM_MAX_PERIOD_COUNT;

			set_motor_speed(ChannelPulse);
		}

		/* 扫描KEY4 */
		if (Key_Scan(KEY4_GPIO_PORT, KEY4_PIN) == KEY_ON) {
			if (ChannelPulse < PWM_MAX_PERIOD_COUNT / 10)
				ChannelPulse = 0;
			else 
				ChannelPulse -= PWM_MAX_PERIOD_COUNT / 10;

			set_motor_speed(ChannelPulse);
		}

		/* 扫描KEY5 */
		if (Key_Scan(KEY5_GPIO_PORT, KEY5_PIN) == KEY_ON) {
			/* 转换方向 */
			set_motor_direction((++i % 2) ? MOTOR_FWD: MOTOR_REV);
		}

		if (HAL_GetTick() % 50 == 0 && flag == 0) // 每50毫秒读取一次电流、电压
		{
			flag				= 1;
			int32_t 		current = get_curr_val();

#if 0 //defined(PID_ASSISTANT_EN)

			set_computer_value(SEED_FACT_CMD, CURVES_CH1, &current, 1);

#else

			printf("电源电压：%.2fV，电流：%dmA\r\n", get_vbus_val(), current);
#endif

		}
		else if (HAL_GetTick() % 50 != 0 && flag == 1) {
			flag				= 0;
		}

	}
}


/**
  * @brief	System Clock 配置
  * 		system Clock 配置如下: 
	*			 System Clock source  = PLL (HSE)
	*			 SYSCLK(Hz) 		  = 480000000 (CPU Clock)
	*			 HCLK(Hz)			  = 240000000 (AXI and AHBs Clock)
	*			 AHB Prescaler		  = 2
	*			 D1 APB3 Prescaler	  = 2 (APB3 Clock  120MHz)
	*			 D2 APB1 Prescaler	  = 2 (APB1 Clock  120MHz)
	*			 D2 APB2 Prescaler	  = 2 (APB2 Clock  120MHz)
	*			 D3 APB4 Prescaler	  = 2 (APB4 Clock  120MHz)
	*			 HSE Frequency(Hz)	  = 25000000
	*			 PLL_M				  = 5
	*			 PLL_N				  = 192
	*			 PLL_P				  = 2
	*			 PLL_Q				  = 4
	*			 PLL_R				  = 2
	*			 VDD(V) 			  = 3.3
	*			 Flash Latency(WS)	  = 4
  * @param	None
  * @retval None
  */
/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
	RCC_OscInitTypeDef RCC_OscInitStruct = {
		0
	};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {
		0
	};
	RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {
		0
	};

	/** 启用电源配置更新
	*/
	HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

	/** 配置主内稳压器输出电压
	*/
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

	while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {
	}

	/** 初始化CPU、AHB和APB总线时钟
	*/
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLM = 5;
	RCC_OscInitStruct.PLL.PLLN = 192;
	RCC_OscInitStruct.PLL.PLLP = 2;
	RCC_OscInitStruct.PLL.PLLQ = 2;
	RCC_OscInitStruct.PLL.PLLR = 2;
	RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
	RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
	RCC_OscInitStruct.PLL.PLLFRACN = 0;

	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		while (1)
			;
	}

	/** 初始化CPU、AHB和APB总线时钟
	*/
	/* 选择PLL作为系统时钟源并配置总线时钟分频器 */
	RCC_ClkInitStruct.ClockType =
		 (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_D1PCLK1 | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_D3PCLK1);
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
	RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;
	HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4);

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) {
		while (1) {
			;
		}
	}

	PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_FDCAN;
	PeriphClkInitStruct.FdcanClockSelection = RCC_FDCANCLKSOURCE_PLL;

	if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
		while (1)
			;
	}
}


/****************************END OF FILE***************************/
