/*
*********************************************************************************************************
*
*	模块名称 : 主程序模块
*	文件名称 : main.c
*	版    本 : V1.1
*	说    明 : 
*
*	修改记录 :
*		版本号   日期         作者        说明
*		V1.0    2018-12-12   Eric2013     1. CMSIS软包版本 V5.4.0
*                                     2. HAL库版本 V1.3.0
*
*   V1.1    2019-04-01   suozhang     1. add FreeRTOS V10.20
*
*	Copyright (C), 2018-2030, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/	
#include "bsp.h"			/* 底层硬件驱动 */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "croutine.h"
#include "semphr.h"
#include "event_groups.h"

/**
 * Log default configuration for EasyLogger.
 * NOTE: Must defined before including the <elog.h>
 */
#if !defined(LOG_TAG)
#define LOG_TAG                    "main_test_tag:"
#endif
#undef LOG_LVL
#if defined(XX_LOG_LVL)
    #define LOG_LVL                    XX_LOG_LVL
#endif

#include "elog.h"

#include "lwip/netif.h"
#include "lwip/tcpip.h"
#include "netif_port.h"

#include "tcp_client.h"
#include "app.h"

static void vTaskLED (void *pvParameters);
static void vTaskLwip(void *pvParameters);

 TaskHandle_t xHandleTaskLED  = NULL;
 TaskHandle_t xHandleTaskLwip = NULL;

struct  CONFIG  config={0xAA55, //	uint16_t vaildsign;
	1,//uint8_t baundrate;    
	1,//uint8_t addr; 
	0x000000000000145,//0x6275110032120001,//0x6275110032120003,//0x5955125011120002, 03 yec-test 101
	0, //uint8_t parity;		// =0 : n,8,1   =1: o,8,1  =2: e,8,1	 数据格式
	{0.2f,0.2f,0.2f,1,1,1,1,1,1,1,1,1},
	0, //uint8_t DisplayMode;  // 显示模式　=0　固定　=1 循环
	{TYPE_IEPE,TYPE_PT,TYPE_IEPE,TYPE_IEPE,TYPE_IEPE,TYPE_IEPE,TYPE_IEPE,TYPE_IEPE,TYPE_IEPE,1,1,1}, //uint8_t interface_type[12]; // 输入类型
	{UNIT_M_S2,UNIT_TEMP,UNIT_M_S2,UNIT_M_S2,UNIT_M_S2,UNIT_M_S2,1,1,1,1,1,1},//uint8_t unit[12];  // 单位
	{250,250,250,50,1000,1000,1000,1000,10000,10000,10000,10000},//uint32_t scale[12]; // 转换系数
	{0.0f,0.0f,0.0f,1250.f,1250.f,0,0,0,8192,8192,8192,8192},//uint32_t adjust[12]; // 修正系数
//	{0,1,2,~0,~0,~0,~0,~0,3,4,5,6},//uint16_t interface_addr[12]; // modbus 地址 上传
	{100,100,100,100,100,100},//	float alarmgate[12]; // float报警值
	{1.004f,1.004f,1.004f,1,1,1},
	0,//uint8_t means	;// 均值类型
	1,//uint16_t means_times; // 均值积算周期
	20000,//uint16_t freq;  // 采样频率 Hz
	4096,//uint16_t avr_count;
	2, // uint8_t reflash; // 刷新时间 
	~0, //	uint16_t din_addr;  //  开关量输入寄存器地址
	~0, // uint16_t dout_addr; //  开关量输出寄存器地址
	300, 30, // uint32_t force_gate,force_backlash,   // 应变启动阈值， 回差。
	~0,~0, //	uint16_t max_addr0,max_addr1; 最大值地址
	300,
	300,
	300,
	0x4a,                          //PGA
	60,                          //工作周期
	0x0100,                         //放大倍数
	0x11,                      //  发射功率设置
	0x01,                    //AD采样时间，跟工作周期一个道理
	8192,                     //AD采样频率
	0x58B6A4C3,           //8点00分，aabb的格式，aa代表时间，bb代表分钟，秒默认为0
	0,                //flash起始地址
	PARAMETERMODE,
	0x1F,     //选哪几个通道传输
	1, //DHCP
	"wifi-sensor",//"TP-LINK-SCZZB",//"yec-test",//"wifi-sensor",//"TP-LINK-sczzb2",//"hold-704",//"wifi-test1",//"yec-test",//"wifi-test",//"yec-test",//"zl_sensor",/////"yec-test",//"test3",//"qiangang2", //"qiangang1", //"qiangang1", /////
  "wifi-sensor",//"china-yec",//"",//"wifi-sensor",//"18051061462",//"wifi-test",//"zl_sensor",///"china-yec",//"",////"",//"zl_sensor",/"lft13852578307",//"",//"",//"123456789",//"china-yec.com",// //
  "192.168.99.3",// "192.168.0.112",//服务器端的IP地址  "192.168.0.18", //M
  "8712", //端口号
  "192.168.99.45",  //LocalIP
  "192.168.99.1",  //LocalGATEWAY
  "255.255.255.0",	//LocalMASK
	1,
	1,
	1,
	0x1f,
	1,
	0,  //是否主动发送心跳包
	0,
	0,
};


/*
*********************************************************************************************************
*	函 数 名: main
*	功能说明: c程序入口
*	形    参: 无
*	返 回 值: 错误代码(无需处理)
*********************************************************************************************************
*/
int main(void)
{

	bsp_Init();		/* 硬件初始化 */
	
	/* initialize EasyLogger */
	if (elog_init() == ELOG_NO_ERR)
	{
			/* set enabled format */
			elog_set_fmt(ELOG_LVL_ASSERT, ELOG_FMT_ALL & ~ELOG_FMT_P_INFO);
			elog_set_fmt(ELOG_LVL_ERROR, ELOG_FMT_ALL );
			elog_set_fmt(ELOG_LVL_WARN, ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME);
			elog_set_fmt(ELOG_LVL_INFO, ELOG_FMT_TAG | ELOG_FMT_TIME);
			elog_set_fmt(ELOG_LVL_DEBUG, ELOG_FMT_ALL & ~(ELOG_FMT_FUNC | ELOG_FMT_P_INFO));
			elog_set_fmt(ELOG_LVL_VERBOSE, ELOG_FMT_ALL & ~(ELOG_FMT_FUNC | ELOG_FMT_P_INFO));
		
			elog_set_text_color_enabled( true );
		
			elog_buf_enabled( false );
		
			/* start EasyLogger */
			elog_start();
	}
	
	xTaskCreate( vTaskLED, "vTaskLED", 512, NULL, 3, &xHandleTaskLED );
	xTaskCreate( vTaskLwip,"Lwip"     ,512, NULL, 2, &xHandleTaskLwip );
	
	/* 启动调度，开始执行任务 */
	vTaskStartScheduler();
}


struct netif gnetif; /* network interface structure */

static void netif_config(void)
{
  ip_addr_t ipaddr;
  ip_addr_t netmask;
  ip_addr_t gw;

  IP_ADDR4(&ipaddr,192,168,0,11);
  IP_ADDR4(&netmask,255,255,255,0);
  IP_ADDR4(&gw,192,168,0,1);

  /* add the network interface */ 
  netif_add(&gnetif, &ipaddr, &netmask, &gw, NULL, &ethernetif_init, &tcpip_input);
  
  /*  Registers the default network interface. */
  netif_set_default(&gnetif);
	
	/* Set the link callback function, this function is called on change of link status*/
  netif_set_link_callback(&gnetif, eth_link_callback);

}

/*
*********************************************************************************************************
*	函 数 名: vTaskLwip
*	功能说明: 初始化 ETH,MAC,DMA,GPIO,LWIP,并创建线程用于处理以太网消息
*	形    参: pvParameters 是在创建该任务时传递的形参
*	返 回 值: 无
* 优 先 级: 2  
*********************************************************************************************************
*/
static void vTaskLwip(void *pvParameters)
{

  /* Create tcp_ip stack thread */
  tcpip_init(NULL, NULL);

  /* Initilaize the netif */
  netif_config();

	for(;;)
	{
		
		tcp_client_conn_server_task();
		
		vTaskDelay( 2000 / portTICK_PERIOD_MS );
	}
}

/*
*********************************************************************************************************
*	函 数 名: vTaskLED
*	功能说明: KED闪烁	
*	形    参: pvParameters 是在创建该任务时传递的形参
*	返 回 值: 无
* 优 先 级: 2  
*********************************************************************************************************
*/
static void vTaskLED(void *pvParameters)
{
	
	uint32_t ulNotifiedValue     = 0;
	uint32_t ledToggleIntervalMs = 1000;

	for(;;)
	{
		
		/*
			* 参数 0x00      表示使用通知前不清除任务的通知值位，
			* 参数 ULONG_MAX 表示函数xTaskNotifyWait()退出前将任务通知值设置为0
			*/
	 if( pdPASS == xTaskNotifyWait( 0x00, 0xffffffffUL, &ulNotifiedValue, ledToggleIntervalMs ) )
	 {
		 if( ulNotifiedValue < 2000 )
			ledToggleIntervalMs  = ulNotifiedValue;
		 else
			 ledToggleIntervalMs = 1000 / portTICK_PERIOD_MS;
	 }

		bsp_LedToggle(1);

//		log_i( "SystemCoreClock:%u,system heap:%u.", SystemCoreClock,xPortGetFreeHeapSize() );

	}
}

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
