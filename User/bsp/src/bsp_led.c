/*
*********************************************************************************************************
*
*	模块名称 : LED指示灯驱动模块
*	文件名称 : bsp_led.c
*	版    本 : V1.0
*	说    明 : 驱动LED指示灯
*
*	修改记录 :
*		版本号  日期        作者     说明
*		V1.0    2018-09-05 armfly  正式发布
*
*	Copyright (C), 2015-2030, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

	
/*
	suozhang
	2019-4-1 11:00:24
	STM32H743 Nucleo-144 
	
	led red PB14

*/	
#define GPIO_PORT_LED1  GPIOA
#define GPIO_PIN_LED1		GPIO_PIN_0

#define GPIO_PORT_LED2  GPIOA
#define GPIO_PIN_LED2		GPIO_PIN_6

static void led_config_gpio(void)
{

	GPIO_InitTypeDef gpio_init_structure;

	/* 使能 GPIO时钟 */
	__HAL_RCC_GPIOA_CLK_ENABLE();

	/* 设置 GPIOB 相关的IO为复用推挽输出 */
	gpio_init_structure.Mode = GPIO_MODE_OUTPUT_PP;
	gpio_init_structure.Pull = GPIO_PULLUP;
	gpio_init_structure.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

	
	/* 配置GPIOA */
	gpio_init_structure.Pin = GPIO_PIN_LED1;
	HAL_GPIO_Init(GPIO_PORT_LED1, &gpio_init_structure);
	
	gpio_init_structure.Pin = GPIO_PIN_LED2;
	HAL_GPIO_Init(GPIO_PORT_LED2, &gpio_init_structure);

}

/*
*********************************************************************************************************
*	函 数 名: bsp_InitLed
*	功能说明: 配置LED指示灯相关的GPIO,  该函数被 bsp_Init() 调用。
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitLed(void)
{
	led_config_gpio();
	
//	bsp_LedOff(1);

}

/*
*********************************************************************************************************
*	函 数 名: bsp_LedOn
*	功能说明: 点亮指定的LED指示灯。
*	形    参:  _no : 指示灯序号，范围 1 - 4
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_LedStatue(uint8_t _no,uint8_t nb)
{
	if(nb==1)
	{
		if (_no == 1)
		{
			HAL_GPIO_WritePin(GPIO_PORT_LED1, GPIO_PIN_LED1,GPIO_PIN_SET);
		}else
		{
			HAL_GPIO_WritePin(GPIO_PORT_LED1, GPIO_PIN_LED1,GPIO_PIN_RESET);
		}
	}else
	{
		if (_no == 2)
		{
			HAL_GPIO_WritePin(GPIO_PORT_LED2, GPIO_PIN_LED2,GPIO_PIN_SET);
		}else
		{
			HAL_GPIO_WritePin(GPIO_PORT_LED2, GPIO_PIN_LED2,GPIO_PIN_RESET);
		}
	}

}



/*
*********************************************************************************************************
*	函 数 名: bsp_LedToggle
*	功能说明: 翻转指定的LED指示灯。
*	形    参:  _no : 指示灯序号，范围 1 - 4
*	返 回 值: 按键代码
*********************************************************************************************************
*/
void bsp_LedToggle(uint8_t _no)
{

	if (_no == 1)
	{
		HAL_GPIO_TogglePin(GPIO_PORT_LED1, GPIO_PIN_LED1);
	}else if(_no == 2)
	{
		HAL_GPIO_TogglePin(GPIO_PORT_LED2, GPIO_PIN_LED2);
	}

}

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
