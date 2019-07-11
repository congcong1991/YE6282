/*
*********************************************************************************************************
*
*	模块名称 : tcp_client
*	文件名称 : tcp_client.c
*	版    本 : V1.0
*	说    明 : TCP 客户端 用于连接后台服务器
*
*	修改记录 :
*						版本号    日期        作者       说明
*						V1.0  2019年04月04日  suozhang   首次发布
*
*********************************************************************************************************
*/


#include "tcp_client.h"


/**
 * Log default configuration for EasyLogger.
 * NOTE: Must defined before including the <elog.h>
 */
#if !defined(LOG_TAG)
#define LOG_TAG                    "tcp_client_tag:"
#endif
#undef LOG_LVL
#if defined(XX_LOG_LVL)
    #define LOG_LVL                    XX_LOG_LVL
#endif

#include "elog.h"

#if LWIP_NETCONN

#include "lwip/sys.h"
#include "lwip/api.h"

#include "lwip/tcp.h"
#include "lwip/ip.h"
#include "app.h"

extern TaskHandle_t xHandleTaskLED;

#define TCP_SERVER_IP   "192.168.0.22"
#define TCP_SERVER_PORT 9527

/* 服务器通信使用权信号量 */
SemaphoreHandle_t xServerCommunicationLockSemaphore = NULL;

uint8_t NetReceiveBuf[NetReceiveBufSize];
uint32_t NetReceiveBufHeadIndex=0;
uint32_t NetReceiveBufTailIndex=0;

uint8_t    RxdTelBuf[RX_BUFFER_SIZE];
uint8_t    RXDCmdBuf[RX_BUFFER_SIZE];
uint8_t    CmdBuf[TX_BUFFER_SIZE];

volatile uint16_t RxdBytes=0;
uint16_t CmdBufLength=0;

struct netconn *tcp_client_server_conn;

void RxdByte(uint8_t c);
	
void tcp_client_conn_server_task( void )
{
  struct netbuf *buf;
  err_t err;
	struct pbuf *q;
	uint8_t *NetPbuf;
	ip_addr_t server_ip;

	
	u16_t server_port = TCP_SERVER_PORT;				     // 服务器端口号初始化

	ip4addr_aton( TCP_SERVER_IP, &server_ip ); 			 // 服务器IP地址初始化

	xServerCommunicationLockSemaphore = xSemaphoreCreateBinary();

	if( NULL == xServerCommunicationLockSemaphore )
	{
			log_e("err:xServerCommunicationLockSemaphore == NULL,while(1).");
			while(1);
	}
	
	for( ;; )
	{
				
		log_i("tcp server connecting %s:%d......", ipaddr_ntoa(&server_ip), server_port );
		
		
		xTaskNotify( xHandleTaskLED, 200, eSetValueWithOverwrite );/* 服务器断开连接状态，LED闪烁为200mS一次. */
		
		/* Create a new connection identifier. */
		tcp_client_server_conn = netconn_new( NETCONN_TCP );
				
		if( tcp_client_server_conn != NULL )
		{		
			//打开TCP 的保活功能 （客户端不默认打开），2018年12月6日10:00:41，SuoZhang
			tcp_client_server_conn->pcb.tcp->so_options |= SOF_KEEPALIVE;

			err = netconn_connect( tcp_client_server_conn, &server_ip, server_port );
					
			if(err == ERR_OK)
			{
				log_i("TCP Server %s:%d connected sucess.", ipaddr_ntoa(&server_ip), server_port );
						
				xSemaphoreGive( xServerCommunicationLockSemaphore ); /* 释放服务器通信使用权 */

				xTaskNotify( xHandleTaskLED, 1000, eSetValueWithOverwrite );/* 服务器连接状态，LED闪烁为1000mS一次. */
				
				for( ;; )
				{
					/* receive data until the other host closes the connection */
					if((err = netconn_recv(tcp_client_server_conn, &buf)) == ERR_OK) 
					{
														 //获取一个指向netbuf 结构中的数据的指针
						for(q=buf->p;q!=NULL;q=q->next)  //遍历完整个pbuf链表
						{
							 __disable_irq(); //关中断
								//判断要拷贝到UDP_DEMO_RX_BUFSIZE中的数据是否大于UDP_DEMO_RX_BUFSIZE的剩余空间，如果大于
								//的话就只拷贝UDP_DEMO_RX_BUFSIZE中剩余长度的数据，否则的话就拷贝所有的数据
							NetPbuf=q->payload;
							for(uint32_t i=0;i<q->len;i++)
							{
								if(!isNetReceiveBufFull())
								{
									NetReceiveBuf[NetReceiveBufHeadIndex]=*NetPbuf;
									NetPbuf++;
									
									IncreaseNetReceiveBuf(NetReceiveBufHeadIndex);
								}
							}	
							 __enable_irq();  //开中断 
						}	
						netbuf_delete(buf);						
					}
					else//if((err = netconn_recv(conn, &buf)) == ERR_OK)
					{
						log_e("err:netconn_recv(conn, &buf):%d.",err);
						netbuf_delete(buf);	
						break; //连接发生错误，退出死等数据的循环，重新建立链接
					}
				 }
			 }
			
			 log_e("err:TCP Server %s:%d connect fail,err:%d.", ipaddr_ntoa(&server_ip), server_port, err );
			 netconn_close  ( tcp_client_server_conn );
			 netconn_delete ( tcp_client_server_conn );		
		   vTaskDelay(1000);
		}
		else//(conn!=NULL)
		{
			log_e("err:Can not create tcp_client_server_conn.");
			vTaskDelay(1000);
		}
	}
}

void receive_server_data_task( void )
{
	for(;;)
	{
		if(!isNetReceiveBufEmpty()) //
		{
			RxdByte(NetReceiveBuf[NetReceiveBufTailIndex]);
			IncreaseNetReceiveBuf(NetReceiveBufTailIndex);
		} else
		{
		vTaskDelay(2);

		}
	}
}

uint8_t command_start()
{ 
	//EnableIEPEPower();
//	 for(uint32_t i=0;i<add_data;i++)
//		xx[i]+=i;
	 {
		for(uint8_t i=0;i<CmdBufLength;i++)
		CmdBuf[i]=RXDCmdBuf[i];
	  CmdBufLength=CmdBufLength;
	 }
	 
	 return 1;
}


uint8_t AnalyCmd(uint16_t length)
{ 

	switch( RXDCmdBuf[1] ){     //看命令行
//		case COMMAND_ID:     //0x02
//			   command_id();
//			break;
//		case COMMAND_CHANNELKIND:     //0x03 通道类型设置
//			   command_channelkind();
//			break;
//		case COMMAND_REPLYIP:     //0x03 通道类型设置
//			   command_replyip();
//			break;
//		case COMMAND_STOP:
//			   command_stop();
//			break;
//		case COMMAND_RECEIVE_BEACON:
//				command_receive_beacon();
//			break;
//		case COMMAND_SETIP:   //设置IP地址
//			   command_setip();
//			break;
//		case COMMAND_SET_RUNMODE:   //设置IP地址
//			   command_setrunmode();
//			break;
		case COMMAND_START:
			   command_start();
			break;
//		case COMMAND_REQUIRE_PERIODWAVE:
//			   command_require_periodwave();
//			break;
//		case COMMAND_SET_AP:
//			   command_set_ap();
//			break;
//		case COMMAND_RECEIVE_ACTIVE_BEACON:
//					command_receive_active_beacon();
//			break;
//		case COMMAND_SET_FACTORY_PARAMETER:
//					command_set_factory_parameter();
//			break;
//		case COMMAND_SET_CHANNEL_CONDITION:
//			   command_set_channel_condition();
//			break;
//		case COMMAND_REPLY_CHANNEL_CONDITION:
//			   command_reply_channel_condition();
//			break;
//		case COMMAND_REPLY_RATE:    //设置采样参数
//			   command_reply_SampleParameter();
//		  break;
//		case COMMAND_SAMPLE_RATE:    //设置采样参数
//			   command_set_SampleParameter();
//			break;
//		case COMMAND_SET_TCPSERVE:   //设置tcp server的目标地址类似
//			   command_set_tcpserve();
//		  break;
//		case COMMAND_REPLYTCPSERVE:   //返回tcp server的目标地址类似
//			   command_replytcpserve();
//			break; 
//		case COMMAND_SET_TIME:           //返回ap值
//			   command_set_time();
//      break;		
//		case COMMAND_REPLYAP:
//			   command_replyap();  //返回ap值
//			break;
//    case COMMAND_APPLYNETSET:
//			   command_applynetset();  //应用网络设置
//			break;
//		 case 0x40:
//			   command_counter();  //应用网络设置
//			break;
//		case COMMAND_REPLY_RUNMODE:
//			   command_reply_runmode();
//		  break;
//		case COMMAND_ADJUST_TEMP:
//			   command_adjust_temp();
//		  break;
//		case COMMAND_ADJUST_ADC:
//			   command_adjust_adc();
//		  break;
//		case COMMAND_SET_SCALE:
//			   command_set_scale();
//			break;
//		case COMMAND_REPLY_SCALE:
//			   command_reply_scale();
//			break;
//    case COMMAND_SET_SNNUMBER:
//			   command_set_snnumber();
//			break;
//		case COMMAND_IAP_STATUE:
//			   command_iap_statue();
//			break;
//		case COMMAND_REPLY_SW_VERSION:
//			   command_reply_sw_version();
//			break;
//		case COMMAND_IAP_DATA:
//			   command_iap_data();
//			break;
//		case COMMAND_RESET_SYSTEM:
//			   command_reset_system();
			break;
		default:
			return 1;
	}
 return 1;
	
}


uint16_t getTelLength(void)  //求的是TLV中V的长度
{
					
	return(((uint16_t)RxdTelBuf[3]<<8)+RxdTelBuf[2]);
  
}
uint8_t  isVaildTel(void)
{
	if(RxdBytes>=1)if(RxdTelBuf[0]!=0x7e)return(0);
	if(RxdBytes>=2)if((RxdTelBuf[1]!=COMMAND_START)&&(RxdTelBuf[1]!=COMMAND_STOP)&&(RxdTelBuf[1]!=COMMAND_ID)
		                  &&(RxdTelBuf[1]!=COMMAND_CHANNELKIND)&&(RxdTelBuf[1]!=COMMAND_REPLYIP)&&(RxdTelBuf[1]!=COMMAND_SETIP)
						  &&(RxdTelBuf[1]!=COMMAND_REPLY_RATE)&&(RxdTelBuf[1]!=COMMAND_SAMPLE_RATE)&&(RxdTelBuf[1]!=COMMAND_ADJUST_TEMP)
						  &&(RxdTelBuf[1]!=COMMAND_REPLY_SCALE)&&(RxdTelBuf[1]!=COMMAND_SET_SCALE)&&(RxdTelBuf[1]!=COMMAND_ADJUST_ADC)
						  &&(RxdTelBuf[1]!=COMMAND_SET_SNNUMBER)&&(RxdTelBuf[1]!=COMMAND_REQUIRE_PERIODWAVE)&&(RxdTelBuf[1]!=COMMAND_SET_CHANNEL_CONDITION)
							&&(RxdTelBuf[1]!=COMMAND_SET_RUNMODE)&&(RxdTelBuf[1]!=COMMAND_SET_TIME)&&(RxdTelBuf[1]!=COMMAND_SET_AP)
						  &&(RxdTelBuf[1]!=COMMAND_RECEIVE_BEACON)&&(RxdTelBuf[1]!=COMMAND_SET_TCPSERVE)&&(RxdTelBuf[1]!=COMMAND_REPLYAP)
						  &&(RxdTelBuf[1]!=COMMAND_REPLYTCPSERVE)&&(RxdTelBuf[1]!=COMMAND_APPLYNETSET)&&(RxdTelBuf[1]!=COMMAND_REPLY_RUNMODE)
							&&(RxdTelBuf[1]!=COMMAND_REPLY_CHANNEL_CONDITION)&&(RxdTelBuf[1]!=COMMAND_RECEIVE_ACTIVE_BEACON)&&(RxdTelBuf[1]!=COMMAND_SET_FACTORY_PARAMETER)
							&&(RxdTelBuf[1]!=COMMAND_IAP_DATA)&&(RxdTelBuf[1]!=COMMAND_IAP_STATUE)&&(RxdTelBuf[1]!=COMMAND_REPLY_SW_VERSION)&&(RxdTelBuf[1]!=COMMAND_RESET_SYSTEM))
				return(0);  //命令行最大ID
	
  if(RxdBytes>=4) { 
		uint16_t length=getTelLength();
		if((length>200)) return(0);  //限制最大的为1000
	}
	return(1);				 // 合法的
}
uint8_t sumofRxdBuf(uint16_t l)  //求和的报文，不包含起始标识,	l	的长度包括起始标识
{ 
	uint8_t sum=0;
	if(l<2) return (0);
	for(uint16_t i=1;i<l-2;i++)
	 sum=sum+RxdTelBuf[i];
	return (sum);
}
uint8_t isTelComplete(void)	   // =0 不完整  =1 sum Error =2 正确
{
	uint32_t  temp8;
	uint32_t   dat_len;

	if(RxdBytes<4)return(0);
  ////////////////
	dat_len=getTelLength()+6;	//
	if(dat_len==0)return(0);
	if(RxdBytes<(dat_len))return(0);

	temp8=sumofRxdBuf(dat_len);
 
  if (RxdTelBuf[dat_len-1]==0x7e)
		return(2); 
	if (RxdTelBuf[dat_len-2]==temp8)
		return(2); 
	else{
		return(1);
	}	
}						 

uint8_t leftRxdTel(void)		//数组左移一位
{
	uint32_t i;
	if(RxdBytes<1)return(0);     // 无法左移
	for	(i=1;i<RxdBytes;i++)
	{
		RxdTelBuf[i-1]=RxdTelBuf[i];		
	}
	RxdBytes--;
	return(1);					 // 丢弃一个字节成功

}

 void RxdByte(uint8_t c)
{	
	uint32_t 	i;
	RxdTelBuf[RxdBytes]=c;
	RxdBytes++;

	switch(RxdBytes)
	{
		case 0:	break;
		case 3:	break;
		case 1:
		case 2:
		case 4:while(!isVaildTel())	//如果不合法			 
				{
					if(!leftRxdTel())break;	  // 丢弃首字节
				}
				break;
			
		
		default:		
				i=isTelComplete();
				if(i==2)
				{
					//do some thing
					for(uint16_t j=0;j<RxdBytes;j++)
					RXDCmdBuf[j]=RxdTelBuf[j];
					CmdBufLength=RxdBytes;
					AnalyCmd(CmdBufLength);
					
					RxdBytes=0;
					
				}
				else if(i==1)	 // CRC error
				{
					leftRxdTel();
					while(!isVaildTel())	//如果不合法			 
					{
						if(!leftRxdTel())break;
					}	
				}
				else if(i==0) //没收完继续收
				{
				
				}
				else
				{
				}
				break;
			
		}
	
}
int send_server_data( uint8_t *data, uint16_t len )
{
												 
	if( tcp_client_server_conn )
	{
		return netconn_write( tcp_client_server_conn, data, len, NETCONN_COPY);
	}
	else
		
	
		return ERR_CONN; 

}

#endif /* LWIP_NETCONN */
















