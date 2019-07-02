    /*
    *********************************************************************************************************
    *        函 数 名: MDMA_SpeedTest
    *        功能说明: MDMA性能测试
    *        形    参: 无
    *        返 回 值: 无
    *********************************************************************************************************
    */
		
#include "bsp.h"			/* 底层硬件驱动 */
#include "app.h"			/* 底层硬件驱动 */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "croutine.h"
#include "semphr.h"
#include "event_groups.h"

MDMA_HandleTypeDef MDMA_Handle;

uint16_t volatile TransferCompleteDetected=0;
void MDMA_IRQHandler(void)
{
	HAL_MDMA_IRQHandler(&MDMA_Handle);
}
static void MDMA_TransferCompleteCallback(MDMA_HandleTypeDef *hmdma)
{
	TransferCompleteDetected = 1;
}
void MDMA_init(void)
{
				/* MDMA配置 **********************************************************************/
	__HAL_RCC_MDMA_CLK_ENABLE();  

	MDMA_Handle.Instance = MDMA_Channel0;  

	MDMA_Handle.Init.Request              = MDMA_REQUEST_SW;         /* 软件触发 */
	MDMA_Handle.Init.TransferTriggerMode  = MDMA_BLOCK_TRANSFER;     /* 块传输 */
	MDMA_Handle.Init.Priority             = MDMA_PRIORITY_HIGH;      /* 优先级高*/
	MDMA_Handle.Init.Endianness           = MDMA_LITTLE_ENDIANNESS_PRESERVE; /* 小端 */
	MDMA_Handle.Init.SourceInc            = MDMA_SRC_INC_DOUBLEWORD;         /* 源地址自增，双字，即8字节 */
	MDMA_Handle.Init.DestinationInc       = MDMA_DEST_INC_DOUBLEWORD;        /* 目的地址自增，双字，即8字节 */
	MDMA_Handle.Init.SourceDataSize       = MDMA_SRC_DATASIZE_DOUBLEWORD;    /* 源地址数据宽度双字，即8字节 */
	MDMA_Handle.Init.DestDataSize         = MDMA_DEST_DATASIZE_DOUBLEWORD;   /* 目的地址数据宽度双字，即8字节 */
	MDMA_Handle.Init.DataAlignment        = MDMA_DATAALIGN_PACKENABLE;       /* 小端，右对齐 */                    
	MDMA_Handle.Init.SourceBurst          = MDMA_SOURCE_BURST_128BEATS;      /* 源数据突发传输，128次 */
	MDMA_Handle.Init.DestBurst            = MDMA_DEST_BURST_128BEATS;        /* 源数据突发传输，128次 */
	
	MDMA_Handle.Init.BufferTransferLength = 128;    /* 每次传输128个字节 */

	MDMA_Handle.Init.SourceBlockAddressOffset  = 0; /* 用于block传输，地址偏移0 */
	MDMA_Handle.Init.DestBlockAddressOffset    = 0; /* 用于block传输，地址偏移0 */

	/* 初始化MDMA */
	if(HAL_MDMA_Init(&MDMA_Handle) != HAL_OK)
	{
					 Error_Handler(__FILE__, __LINE__);
	}

	/* 设置传输完成回调和中断及其优先级配置 */
	HAL_MDMA_RegisterCallback(&MDMA_Handle, HAL_MDMA_XFER_CPLT_CB_ID, MDMA_TransferCompleteCallback);
	HAL_NVIC_SetPriority(MDMA_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(MDMA_IRQn);  

			 

	
}

uint8_t WriteDataToTXDBUF(uint8_t * source,uint16_t length)
{ 
	
	if(xSemaphoreTake(WRITE_ready, portMAX_DELAY) != pdTRUE);//  等信号量，没毛病
	__disable_irq();	   //不应该屏蔽所有中断，先用着
	if(isTxdBufFull()) 
	{
	xSemaphoreGive(WRITE_ready);//释放信号量这块
	__enable_irq();

	return 0;
	} //队列满了就不完
	__enable_irq();

		/* AXI SRAM向SDRAM的64KB数据传输测试 ***********************************************/
	TransferCompleteDetected = 0;
	HAL_MDMA_Start_IT(&MDMA_Handle,
																(uint32_t)source,
																(uint32_t)0xC0000000,
																length/4,
																1);

	while(TransferCompleteDetected == 0) {}
	Increase(HeadIndex);  
	xSemaphoreGive(WRITE_ready);//释放信号量这块
	return 1;
}
