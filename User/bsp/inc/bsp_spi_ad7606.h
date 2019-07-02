/*
*********************************************************************************************************
*
*	模块名称 : AD7606数据采集模块
*	文件名称 : bsp_ad7606.h
*	版    本 : V1.0
*
*	Copyright (C), 2013-2014, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#ifndef _BSP_AD7606_H
#define _BSP_AD7606_H

#define SPIx                             SPI1
#define SPIx_CLK_ENABLE()                __HAL_RCC_SPI1_CLK_ENABLE()
#define DMAx_CLK_ENABLE()                __HAL_RCC_DMA2_CLK_ENABLE()
#define SPIx_SCK_GPIO_CLK_ENABLE()       __HAL_RCC_GPIOB_CLK_ENABLE()
#define SPIx_MISO_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOB_CLK_ENABLE()
#define SPIx_MOSI_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOB_CLK_ENABLE()

#define SPIx_FORCE_RESET()               __HAL_RCC_SPI1_FORCE_RESET()
#define SPIx_RELEASE_RESET()             __HAL_RCC_SPI1_RELEASE_RESET()

/* Definition for SPIx Pins */
#define SPIx_SCK_PIN                     GPIO_PIN_3
#define SPIx_SCK_GPIO_PORT               GPIOB
#define SPIx_SCK_AF                      GPIO_AF5_SPI1
#define SPIx_MISO_PIN                    GPIO_PIN_4
#define SPIx_MISO_GPIO_PORT              GPIOB
#define SPIx_MISO_AF                     GPIO_AF5_SPI1

#define SPIx_MOSI_PIN                    
#define SPIx_MOSI_GPIO_PORT              
#define SPIx_MOSI_AF                     

/* Definition for SPIx's DMA */
#define SPIx_TX_DMA_STREAM               DMA2_Stream3
#define SPIx_RX_DMA_STREAM               DMA2_Stream2

#define SPIx_TX_DMA_REQUEST              DMA_REQUEST_SPI1_TX
#define SPIx_RX_DMA_REQUEST              DMA_REQUEST_SPI1_RX

/* Definition for SPIx's NVIC */
#define SPIx_DMA_TX_IRQn                 DMA2_Stream3_IRQn
#define SPIx_DMA_RX_IRQn                 DMA2_Stream2_IRQn

#define SPIx_DMA_TX_IRQHandler           DMA2_Stream3_IRQHandler
#define SPIx_DMA_RX_IRQHandler           DMA2_Stream2_IRQHandler

#define SPIx_IRQn                        SPI1_IRQn
#define SPIx_IRQHandler                  SPI1_IRQHandler

#define BUFFERSIZE 16
/* 过采样倍率 */
typedef enum
{
	AD_OS_NO = 0,
	AD_OS_X2 = 1,
	AD_OS_X4 = 2,
	AD_OS_X8 = 3,
	AD_OS_X16 = 4,
	AD_OS_X32 = 5,
	AD_OS_X64 = 6
}AD7606_OS_E;


/* AD数据采集缓冲区 FIFO */
#define ADC_FIFO_SIZE	(2*1024)	/* 总体样本数 */

typedef struct
{
	uint8_t ucOS;			/* 过采样倍率，0 - 6. 0表示无过采样 */
	uint8_t ucRange;		/* 输入量程，0表示正负5V, 1表示正负10V */
	int16_t sNowAdc[8];		/* 当前ADC值, 有符号数 */
}AD7606_VAR_T;

typedef struct
{
	/* FIFO 结构 */
	uint16_t usRead;		/* 读指针 */
	uint16_t usWrite;		/* 写指针 */

	uint16_t usCount;		/* 新数据个数 */
	uint8_t ucFull;			/* FIFO满标志 */

	int16_t  sBuf[ADC_FIFO_SIZE];
}AD7606_FIFO_T;

void bsp_InitAD7606(void);
void AD7606_SetOS(uint8_t _ucOS);
void AD7606_SetInputRange(uint8_t _ucRange);
void AD7606_Reset(void);
void AD7606_StartConvst(void);
void AD7606_ReadNowAdc(void);

/* 下面的函数用于FIFO操作模式 */
void AD7606_EnterAutoMode(uint32_t _ulFreq);
void AD7606_StartRecord(uint32_t _ulFreq);
void AD7606_StopRecord(void);
uint8_t AD7606_FifoNewData(void);
uint8_t AD7606_ReadFifo(uint16_t *_usReadAdc);
uint8_t AD7606_FifoFull(void);


/* 全局变量 */
extern AD7606_VAR_T g_tAD7606;
extern AD7606_FIFO_T g_tAdcFifo;

#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
