/*
*********************************************************************************************************
*
*	模块名称 : AD7606数据采集模块
*	文件名称 : bsp_ad7606.c
*	版    本 : V1.0
*	说    明 : AD7606挂在STM32的FMC总线上。
*
*			本例子使用了 TIM3 作为硬件定时器，定时启动ADC转换
*
*	修改记录 :
*		版本号  日期        作者     说明
*		V1.0    2015-10-11 armfly  正式发布
*
*	Copyright (C), 2015-2020, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

/*
	STM32-V7开发板 + AD7606模块， 控制采集的IO:
	
	PC6/TIM3_CH1/TIM8_CH1     ----> AD7606_CONVST  (和摄像头复用),  输出PWM方波，作为ADC启动信号
	PE5/DCMI_D6/AD7606_BUSY   <---- AD7606_BUSY    , CPU在BUSY中断服务程序中读取采集结果
*/

#include "bsp.h"

/* CONVST 启动ADC转换的GPIO = PC6 */
#define CONVST_RCC_GPIO_CLK_ENABLE	__HAL_RCC_GPIOC_CLK_ENABLE
#define CONVST_GPIO		GPIOC
#define CONVST_PIN		GPIO_PIN_6
#define CONVST_TIMX		TIM8
#define CONVST_TIMCH	1

/* BUSY 转换完毕信号 = PC13 */
#define BUSY_RCC_GPIO_CLK_ENABLE	__HAL_RCC_GPIOC_CLK_ENABLE
#define BUSY_GPIO		GPIOC
#define BUSY_PIN		GPIO_PIN_13
#define BUSY_IRQn		EXTI15_10_IRQn
#define BUSY_IRQHandler	EXTI15_10_IRQHandler

/* OS0信号 = PA3 */
#define OS0_RCC_GPIO_CLK_ENABLE	__HAL_RCC_GPIOA_CLK_ENABLE
#define OS0_GPIO		GPIOA
#define OS0_PIN		GPIO_PIN_3

/* OS1信号 = PB8 */
#define OS1_RCC_GPIO_CLK_ENABLE	__HAL_RCC_GPIOB_CLK_ENABLE
#define OS1_GPIO		GPIOB
#define OS1_PIN		GPIO_PIN_8

/* OS2信号 = PA5 */
#define OS2_RCC_GPIO_CLK_ENABLE	__HAL_RCC_GPIOA_CLK_ENABLE
#define OS2_GPIO		GPIOA
#define OS2_PIN		GPIO_PIN_5

/* 设置过采样的IO */
#define OS0_1()		HAL_GPIO_WritePin((GPIO_TypeDef *)OS0_GPIO,OS0_PIN, GPIO_PIN_SET)
#define OS0_0()		HAL_GPIO_WritePin((GPIO_TypeDef *)OS0_GPIO,OS0_PIN,GPIO_PIN_RESET)
#define OS1_1()		HAL_GPIO_WritePin((GPIO_TypeDef *)OS1_GPIO,OS1_PIN, GPIO_PIN_SET)
#define OS1_0()		HAL_GPIO_WritePin((GPIO_TypeDef *)OS1_GPIO,OS1_PIN, GPIO_PIN_RESET)
#define OS2_1()		HAL_GPIO_WritePin((GPIO_TypeDef *)OS2_GPIO,OS2_PIN, GPIO_PIN_SET )
#define OS2_0()		HAL_GPIO_WritePin((GPIO_TypeDef *)OS2_GPIO,OS2_PIN, GPIO_PIN_RESET)

/* 启动AD转换的GPIO : PC6 */
#define CONVST_1()	CONVST_GPIO->BSRRL = CONVST_PIN
#define CONVST_0()	CONVST_GPIO->BSRRH = CONVST_PIN


/* 设置输入量程的GPIO, 在扩展的74HC574上 */
#define RANGE_1()	
#define RANGE_0()	

/* AD7606_RESET = PC7 */
#define AD7606_RESET_RCC_GPIO_CLK_ENABLE	__HAL_RCC_GPIOC_CLK_ENABLE
#define AD7606_RESET_GPIO		GPIOC
#define AD7606_RESET_PIN		GPIO_PIN_7

/* AD7606复位口线 */
#define RESET_1()	HAL_GPIO_WritePin((GPIO_TypeDef *)AD7606_RESET_GPIO,AD7606_RESET_PIN,GPIO_PIN_SET)
#define RESET_0()	HAL_GPIO_WritePin((GPIO_TypeDef *)AD7606_RESET_GPIO,AD7606_RESET_PIN,GPIO_PIN_RESET)


/* SPI handler declaration */
SPI_HandleTypeDef SpiHandle;
static DMA_HandleTypeDef hdma_tx;
static DMA_HandleTypeDef hdma_rx;

AD7606_VAR_T g_tAD7606;		/* 定义1个全局变量，保存一些参数 */
AD7606_FIFO_T g_tAdcFifo;	/* 定义FIFO结构体变量 */

static void AD7606_CtrlLinesConfig(void);
static void AD7606_SPIConfig(void);

/*
*********************************************************************************************************
*	函 数 名: bsp_InitAD7606
*	功能说明: 配置连接外部SRAM的GPIO和FSMC
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitAD7606(void)
{
	AD7606_CtrlLinesConfig();
	AD7606_SPIConfig();

	AD7606_SetOS(AD_OS_NO);		/* 无过采样 */
	AD7606_SetInputRange(0);	/* 0表示输入量程为正负5V, 1表示正负10V */

	AD7606_Reset();

	CONVST_1();					/* 启动转换的GPIO平时设置为高 */
}

/*
*********************************************************************************************************
*	函 数 名: AD7606_CtrlLinesConfig
*	功能说明: 配置GPIO口线，FMC管脚设置为复用功能
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/

/* 
	控制AD7606参数的其他IO分配在扩展的74HC574上
	X13 - AD7606_OS0
	X14 - AD7606_OS1
	X15 - AD7606_OS2
	X24 - AD7606_RESET
	X25 - AD7606_RAGE	
	
	PE5 - AD7606_BUSY
*/
static void AD7606_CtrlLinesConfig(void)
{
	

	GPIO_InitTypeDef gpio_init_structure;

	/* 使能 GPIO时钟 */
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();


	/* 设置 GPIOD 相关的IO为复用推挽输出 */
	gpio_init_structure.Mode = GPIO_MODE_AF_PP;
	gpio_init_structure.Pull = GPIO_PULLUP;
	gpio_init_structure.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	gpio_init_structure.Alternate = GPIO_AF12_FMC;
	
	/* 配置GPIOA */
	gpio_init_structure.Pin = GPIO_PIN_3 | GPIO_PIN_5 ;
	HAL_GPIO_Init(GPIOA, &gpio_init_structure);

	/* 配置GPIOB */
	gpio_init_structure.Pin = GPIO_PIN_8;
	HAL_GPIO_Init(GPIOB, &gpio_init_structure);

	/* 配置GPIOC */
	gpio_init_structure.Pin = GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_13;
	HAL_GPIO_Init(GPIOC, &gpio_init_structure);
	
}

/*
*********************************************************************************************************
*	函 数 名: AD7606_SPIConfig
*	功能说明: 配置FSMC并口访问时序
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi)
{
  GPIO_InitTypeDef  GPIO_InitStruct;
  
  if (hspi->Instance == SPIx)
  {
    /*##-1- Enable peripherals and GPIO Clocks #################################*/
    /* Enable GPIO TX/RX clock */
    SPIx_SCK_GPIO_CLK_ENABLE();
    SPIx_MISO_GPIO_CLK_ENABLE();
    SPIx_MOSI_GPIO_CLK_ENABLE();
    /* Enable SPI1 clock */
    SPIx_CLK_ENABLE();
    /* Enable DMA clock */
    DMAx_CLK_ENABLE();

    /*##-2- Configure peripheral GPIO ##########################################*/  
    /* SPI SCK GPIO pin configuration  */
    GPIO_InitStruct.Pin       = SPIx_SCK_PIN;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_PULLDOWN;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = SPIx_SCK_AF;
    HAL_GPIO_Init(SPIx_SCK_GPIO_PORT, &GPIO_InitStruct);

    /* SPI MISO GPIO pin configuration  */
    GPIO_InitStruct.Pin = SPIx_MISO_PIN;
    GPIO_InitStruct.Alternate = SPIx_MISO_AF;
    HAL_GPIO_Init(SPIx_MISO_GPIO_PORT, &GPIO_InitStruct);

    /* SPI MOSI GPIO pin configuration  */
//    GPIO_InitStruct.Pin = SPIx_MOSI_PIN;
//    GPIO_InitStruct.Alternate = SPIx_MOSI_AF;
//    HAL_GPIO_Init(SPIx_MOSI_GPIO_PORT, &GPIO_InitStruct);

    /*##-3- Configure the DMA ##################################################*/
    /* Configure the DMA handler for Transmission process */
    hdma_tx.Instance                 = SPIx_TX_DMA_STREAM;
    hdma_tx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
    hdma_tx.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
    hdma_tx.Init.MemBurst            = DMA_MBURST_INC4;
    hdma_tx.Init.PeriphBurst         = DMA_PBURST_INC4;
    hdma_tx.Init.Request             = SPIx_TX_DMA_REQUEST;
    hdma_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
    hdma_tx.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_tx.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_tx.Init.Mode                = DMA_NORMAL;
    hdma_tx.Init.Priority            = DMA_PRIORITY_LOW;

    HAL_DMA_Init(&hdma_tx);

    /* Associate the initialized DMA handle to the the SPI handle */
    __HAL_LINKDMA(hspi, hdmatx, hdma_tx);

    /* Configure the DMA handler for Transmission process */
    hdma_rx.Instance                 = SPIx_RX_DMA_STREAM;

    hdma_rx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
    hdma_rx.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
    hdma_rx.Init.MemBurst            = DMA_MBURST_INC4;
    hdma_rx.Init.PeriphBurst         = DMA_PBURST_INC4;
    hdma_rx.Init.Request             = SPIx_RX_DMA_REQUEST;
    hdma_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    hdma_rx.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_rx.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_rx.Init.Mode                = DMA_NORMAL;
    hdma_rx.Init.Priority            = DMA_PRIORITY_HIGH;

    HAL_DMA_Init(&hdma_rx);

    /* Associate the initialized DMA handle to the the SPI handle */
    __HAL_LINKDMA(hspi, hdmarx, hdma_rx);
    
    /*##-4- Configure the NVIC for DMA #########################################*/ 
    /* NVIC configuration for DMA transfer complete interrupt (SPI1_TX) */
    HAL_NVIC_SetPriority(SPIx_DMA_TX_IRQn, 1, 1);
    HAL_NVIC_EnableIRQ(SPIx_DMA_TX_IRQn);
    
    /* NVIC configuration for DMA transfer complete interrupt (SPI1_RX) */
    HAL_NVIC_SetPriority(SPIx_DMA_RX_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(SPIx_DMA_RX_IRQn);

    /*##-5- Configure the NVIC for SPI #########################################*/ 
    /* NVIC configuration for SPI transfer complete interrupt (SPI1) */
    HAL_NVIC_SetPriority(SPIx_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(SPIx_IRQn);
  }
}

static void AD7606_SPIConfig(void)
{
 
  /*##-1- Configure the SPI peripheral #######################################*/
  /* Set the SPI parameters */
  SpiHandle.Instance               = SPIx;
  SpiHandle.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
  SpiHandle.Init.Direction         = SPI_DIRECTION_2LINES;
  SpiHandle.Init.CLKPhase          = SPI_PHASE_1EDGE;
  SpiHandle.Init.CLKPolarity       = SPI_POLARITY_LOW;
  SpiHandle.Init.DataSize          = SPI_DATASIZE_8BIT;
  SpiHandle.Init.FirstBit          = SPI_FIRSTBIT_MSB;
  SpiHandle.Init.TIMode            = SPI_TIMODE_DISABLE;
  SpiHandle.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
  SpiHandle.Init.CRCPolynomial     = 7;
  SpiHandle.Init.CRCLength         = SPI_CRC_LENGTH_8BIT;
  SpiHandle.Init.NSS               = SPI_NSS_SOFT;
  SpiHandle.Init.NSSPMode          = SPI_NSS_PULSE_DISABLE;
  SpiHandle.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_ENABLE;  /* Recommanded setting to avoid glitches */


  SpiHandle.Init.Mode = SPI_MODE_MASTER;


  if(HAL_SPI_Init(&SpiHandle) != HAL_OK)
  {
    /* Initialization Error */
//    Error_Handler();
  }	
}

/*
*********************************************************************************************************
*	函 数 名: AD7606_SetOS
*	功能说明: 配置AD7606数字滤波器，也就设置过采样倍率。
*			 通过设置 AD7606_OS0、OS1、OS2口线的电平组合状态决定过采样倍率。
*			 启动AD转换之后，AD7606内部自动实现剩余样本的采集，然后求平均值输出。
*
*			 过采样倍率越高，转换时间越长。
*			 无过采样时，AD转换时间 4us;
*				2倍过采样时 = 8.7us;
*				4倍过采样时 = 16us
*			 	64倍过采样时 = 286us
*
*	形    参: _ucOS : 过采样倍率, 0 - 6
*	返 回 值: 无
*********************************************************************************************************
*/
void AD7606_SetOS(uint8_t _ucOS)
{
	g_tAD7606.ucOS = _ucOS;
	switch (_ucOS)
	{
		case AD_OS_X2:
			OS2_0();
			OS1_0();
			OS0_1();
			break;

		case AD_OS_X4:
			OS2_0();
			OS1_1();
			OS0_0();
			break;

		case AD_OS_X8:
			OS2_0();
			OS1_1();
			OS0_1();
			break;

		case AD_OS_X16:
			OS2_1();
			OS1_0();
			OS0_0();
			break;

		case AD_OS_X32:
			OS2_1();
			OS1_0();
			OS0_1();
			break;

		case AD_OS_X64:
			OS2_1();
			OS1_1();
			OS0_0();
			break;

		case AD_OS_NO:
		default:
			g_tAD7606.ucOS = AD_OS_NO;
			OS2_0();
			OS1_0();
			OS0_0();
			break;
	}
}

/*
*********************************************************************************************************
*	函 数 名: AD7606_SetInputRange
*	功能说明: 配置AD7606模拟信号输入量程。
*	形    参: _ucRange : 0 表示正负5V   1表示正负10V
*	返 回 值: 无
*********************************************************************************************************
*/
void AD7606_SetInputRange(uint8_t _ucRange)
{
	if (_ucRange == 0)
	{
		g_tAD7606.ucRange = 0;
		RANGE_0();	/* 设置为正负5V */
	}
	else
	{
		g_tAD7606.ucRange = 1;
		RANGE_1();	/* 设置为正负10V */
	}
}

/*
*********************************************************************************************************
*	函 数 名: AD7606_Reset
*	功能说明: 硬件复位AD7606。复位之后恢复到正常工作状态。
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void AD7606_Reset(void)
{
	RESET_0();	/* 退出复位状态 */

	RESET_1();	/* 进入复位状态 */
	RESET_1();	/* 仅用于延迟。 RESET复位高电平脉冲宽度最小50ns。 */
	RESET_1();
	RESET_1();

	RESET_0();	/* 退出复位状态 */
}

/*
*********************************************************************************************************
*	函 数 名: AD7606_StartConvst
*	功能说明: 启动1次ADC转换
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void AD7606_StartConvst(void)
{
	/* page 7：  CONVST 高电平脉冲宽度和低电平脉冲宽度最短 25ns */
	/* CONVST平时为高 */
	CONVST_0();
	CONVST_0();
	CONVST_0();

	CONVST_1();
}

/*
*********************************************************************************************************
*	函 数 名: AD7606_ReadNowAdc
*	功能说明: 读取8路采样结果。结果存储在全局变量 g_tAD7606
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
const uint8_t aTxBuffer[] = "help me,help you";
ALIGN_32BYTES(uint8_t aRxBuffer[BUFFERSIZE]);

void AD7606_ReadNowAdc(void)
{
	if(HAL_SPI_TransmitReceive_DMA(&SpiHandle, (uint8_t*)aTxBuffer, (uint8_t *)aRxBuffer, BUFFERSIZE) != HAL_OK)
  {
    /* Transfer error in transmission process */
//    Error_Handler();
  }
}

void SPIx_DMA_RX_IRQHandler(void)
{
	
}
/*
*********************************************************************************************************
*		下面的函数用于定时采集模式。 TIM5硬件定时中断中读取ADC结果，存在全局FIFO
*
*
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*	函 数 名: AD7606_HasNewData
*	功能说明: 判断FIFO中是否有新数据
*	形    参:  _usReadAdc : 存放ADC结果的变量指针
*	返 回 值: 1 表示有，0表示暂无数据
*********************************************************************************************************
*/
uint8_t AD7606_HasNewData(void)
{
	if (g_tAdcFifo.usCount > 0)
	{
		return 1;
	}
	return 0;
}

/*
*********************************************************************************************************
*	函 数 名: AD7606_FifoFull
*	功能说明: 判断FIFO是否满
*	形    参:  _usReadAdc : 存放ADC结果的变量指针
*	返 回 值: 1 表示满，0表示未满
*********************************************************************************************************
*/
uint8_t AD7606_FifoFull(void)
{
	return g_tAdcFifo.ucFull;
}

/*
*********************************************************************************************************
*	函 数 名: AD7606_ReadFifo
*	功能说明: 从FIFO中读取一个ADC值
*	形    参:  _usReadAdc : 存放ADC结果的变量指针
*	返 回 值: 1 表示OK，0表示暂无数据
*********************************************************************************************************
*/
uint8_t AD7606_ReadFifo(uint16_t *_usReadAdc)
{
	if (AD7606_HasNewData())
	{
		*_usReadAdc = g_tAdcFifo.sBuf[g_tAdcFifo.usRead];
		if (++g_tAdcFifo.usRead >= ADC_FIFO_SIZE)
		{
			g_tAdcFifo.usRead = 0;
		}

		DISABLE_INT();
		if (g_tAdcFifo.usCount > 0)
		{
			g_tAdcFifo.usCount--;
		}
		ENABLE_INT();
		return 1;
	}
	return 0;
}

/*
*********************************************************************************************************
*	函 数 名: AD7606_StartRecord
*	功能说明: 开始采集
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void AD7606_StartRecord(uint32_t _ulFreq)
{
	AD7606_StopRecord();

	AD7606_Reset();					/* 复位硬件 */
	AD7606_StartConvst();			/* 启动采样，避免第1组数据全0的问题 */

	g_tAdcFifo.usRead = 0;			/* 必须在开启TIM2之前清0 */
	g_tAdcFifo.usWrite = 0;
	g_tAdcFifo.usCount = 0;
	g_tAdcFifo.ucFull = 0;

	AD7606_EnterAutoMode(_ulFreq);
}


/*
*********************************************************************************************************
*	函 数 名: AD7606_EnterAutoMode
*	功能说明: 配置硬件工作在自动采集模式，结果存储在FIFO缓冲区。
*	形    参:  _ulFreq : 采样频率，单位Hz，	1k，2k，5k，10k，20K，50k，100k，200k
*	返 回 值: 无
*********************************************************************************************************
*/
void AD7606_EnterAutoMode(uint32_t _ulFreq)
{
	/* 配置PC6为TIM1_CH1功能，输出占空比50%的方波 */
	bsp_SetTIMOutPWM(CONVST_GPIO, CONVST_PIN, CONVST_TIMX,  CONVST_TIMCH, _ulFreq, 5000);
	
	/* 配置PE5, BUSY 作为中断输入口，下降沿触发 */
	{
		GPIO_InitTypeDef   GPIO_InitStructure;
		
		CONVST_RCC_GPIO_CLK_ENABLE();	/* 打开GPIO时钟 */
		__HAL_RCC_SYSCFG_CLK_ENABLE();

		GPIO_InitStructure.Mode = GPIO_MODE_IT_FALLING;	/* 中断下降沿触发 */
		GPIO_InitStructure.Pull = GPIO_NOPULL;
		GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_LOW;		
		GPIO_InitStructure.Pin = BUSY_PIN;
		HAL_GPIO_Init(BUSY_GPIO, &GPIO_InitStructure);	

		HAL_NVIC_SetPriority(BUSY_IRQn, 2, 0);
		HAL_NVIC_EnableIRQ(BUSY_IRQn);		
	}		
}

/*
*********************************************************************************************************
*	函 数 名: AD7606_StopRecord
*	功能说明: 停止采集定时器
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void AD7606_StopRecord(void)
{
	/* 配置PC6 输出低电平，关闭TIM */
	bsp_SetTIMOutPWM(CONVST_GPIO, CONVST_PIN, CONVST_TIMX,  CONVST_TIMCH, 1000, 10000);
	CONVST_1();					/* 启动转换的GPIO平时设置为高 */	

	
	/* 配置 BUSY 作为中断输入口，下降沿触发 */
	{
		GPIO_InitTypeDef   GPIO_InitStructure;
		
		CONVST_RCC_GPIO_CLK_ENABLE();		/* 打开GPIO时钟 */

		/* Configure PC.13 pin as input floating */
		GPIO_InitStructure.Mode = GPIO_MODE_INPUT;
		GPIO_InitStructure.Pull = GPIO_NOPULL;
		GPIO_InitStructure.Pin = CONVST_PIN;
		HAL_GPIO_Init(CONVST_GPIO, &GPIO_InitStructure);	

		HAL_NVIC_DisableIRQ(EXTI9_5_IRQn);		
	}
}

/*
*********************************************************************************************************
*	函 数 名: AD7606_ISR
*	功能说明: 定时采集中断服务程序
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void AD7606_ISR(void)
{


	AD7606_ReadNowAdc();

	
}

/*
*********************************************************************************************************
*	函 数 名: EXTI9_5_IRQHandler
*	功能说明: 外部中断服务程序入口。PE5 / AD7606_BUSY 下降沿中断触发
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/

void BUSY_IRQHandler(void)
{
	HAL_GPIO_EXTI_IRQHandler(BUSY_PIN);
}

/*
*********************************************************************************************************
*	函 数 名: EXTI15_10_IRQHandler
*	功能说明: 外部中断服务程序入口。 AD7606_BUSY 下降沿中断触发
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if (GPIO_Pin == BUSY_PIN)
	{
		AD7606_ISR();
	}
}



/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
