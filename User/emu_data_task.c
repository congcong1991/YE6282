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
#include "app.h"			/* 底层硬件驱动 */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "croutine.h"
#include "semphr.h"
#include "event_groups.h"
#include "arm_math.h"

extern volatile uint8_t currentblock ;






arm_rfft_fast_instance_f32 S;
uint32_t fftSize,ifftFlag;
void Init_FFT(void)
{
 
 fftSize = config.ADfrequence; //????

 ifftFlag = 0;//???

// arm_rfft_fast_init_f32(&S, fftSize);
}



void updateParameter(uint8_t i)
{

	if(config.interface_type[i]==TYPE_IEPE)
	{
		Parameter.Mean[i]=Parameter.S_sum[i]*Parameter.ReciprocalofEMUnumber;//??
		float tt=Parameter.SS_sum[i]*Parameter.ReciprocalofEMUnumber;  //
		if(Parameter.SS_sum[i]==0) Parameter.SS_sum[i]=0.01f;
		if(tt<0) tt=-tt;
		arm_sqrt_f32(tt,(float32_t *)Parameter.EffectiveValue+i);
#ifdef USE_FLITER
		if(Parameter.EffectiveValue[i]<0.25f)
		{
		Parameter.PeakValue[i]=0;
		Parameter.Inter_Skew[i]=0;
		Parameter.Kurtosis[i]=0; 		
		Parameter.Abs_average[i]=0;
		Parameter.MaxValue[i]=0;			
		Parameter.InterIIMarginIndex[i]=0;
		Parameter.WaveformIndex[i]=0; //????
		Parameter.PeakIndex[i]=0;//????
		Parameter.PulseIndex[i]=0;//????
		Parameter.Skew[i]=0;//????	 
		Parameter.KurtosisIndex[i]=0;//????
		Parameter.MarginIndex[i]=0;
					
		}
		else
#endif
		{		
		//Parameter.ReciprocalofEMUnumber ?????? Parameter.ReciprocalofEMUnumber=1/config.ADfrequence; 
		Parameter.PeakValue[i]=Parameter.InterMAX[i]-Parameter.InterMIN[i];
		Parameter.Inter_Skew[i]=Parameter.SSS_sum[i]*Parameter.ReciprocalofEMUnumber;
		Parameter.Kurtosis[i]=Parameter.SSSS_sum[i]*Parameter.ReciprocalofEMUnumber; //??
		
		Parameter.Abs_average[i]=Parameter.Abs_S_average[i]*Parameter.ReciprocalofEMUnumber;//?????
		
		Parameter.InterIIMarginIndex[i]=(Parameter.Inter_MarginIndex[i]*Parameter.ReciprocalofEMUnumber)*(Parameter.Inter_MarginIndex[i]*Parameter.ReciprocalofEMUnumber);
		if(Parameter.InterMAX[i]<(-Parameter.InterMIN[i])) Parameter.MaxValue[i]=-Parameter.InterMIN[i];
		else Parameter.MaxValue[i]=Parameter.InterMAX[i]; //????
		if(Parameter.Abs_average[i]==0) Parameter.Abs_average[i]=0.01f;
		Parameter.WaveformIndex[i]=Parameter.EffectiveValue[i]/Parameter.Abs_average[i]; //????
		Parameter.PeakIndex[i]=Parameter.MaxValue[i]/Parameter.EffectiveValue[i];//????
		Parameter.PulseIndex[i]=Parameter.MaxValue[i]/Parameter.Abs_average[i];//????
		Parameter.Skew[i]=Parameter.Inter_Skew[i]/(Parameter.EffectiveValue[i]*Parameter.EffectiveValue[i]*Parameter.EffectiveValue[i]);//????	 
		Parameter.KurtosisIndex[i]=Parameter.Kurtosis[i]/(Parameter.EffectiveValue[i]*Parameter.EffectiveValue[i]*Parameter.EffectiveValue[i]*Parameter.EffectiveValue[i]);//????
		Parameter.MarginIndex[i]=Parameter.MaxValue[i]/Parameter.InterIIMarginIndex[i];
		}
	}else 
		{  //
		 Parameter.Abs_average[i]=Parameter.S_sum[i]*Parameter.ReciprocalofEMUnumber;
		}
}



extern void InitIntermediateVariable(uint8_t i) ;

float (*p)(double,uint32_t);

double (*p_highpass)(double,uint32_t);
extern uint32_t SAMPLEblock;

#define numStages 2 /* 2阶IIR滤波的个数 */
float32_t testOutput[51200]; /* 滤波后的输出 */
float32_t IIRStateF32[4*numStages]; /* 状态缓存，大小numTaps + blockSize - 1*/
/* 巴特沃斯高通滤波器系数 140Hz */
const float32_t IIRCoeffs51200_10HP[5*numStages] = {
1 , -2 , 1 , 1.9990596893924666 , -0.99906119466748244   ,    
1 , -2 , 1 , 1.9977335227271302  , -0.99773502700355399  
};
const float sacle51200_10HP=0.99953022101498723f*0.99886713743267108f;


const float32_t IIRCoeffs25600_10HP[5*numStages] = {
1 , -2 , 1 , 1.9981172534159273 , -0.99812327168873494   ,    
1 , -2 , 1 , 1.9954691714091646  , -0.99547518170602411  
};
const float sacle25600_10HP=0.99773608827879723f*0.99906013127616555f;


const float32_t IIRCoeffs12800_10HP[5*numStages] = {
1 , -2 , 1 , 1.9962260229711992 , -0.99625007345819772   ,    
1 , -2 , 1 , 1.9909468313661285  , -0.99097081824954325  
};
const float sacle12800_10HP=0.99811902410734921f*0.99547941240391791f;

arm_biquad_casd_df1_inst_f32 S_test;

volatile float hp_yy[6],SpeedInter[6];
void EmuData(void)
{ 
	float absyy=0;
  float INTERsqrt=0;
	float yy,vy;
	uint32_t SAMPLEblock;
	if(currentSAMPLEblock==1)SAMPLEblock=0;
	else SAMPLEblock=1;
	float ScaleValue=1;

	
	
	SAMPLEblock=0;
	{
		
		
		
		for(uint32_t j=0;j<Acceleration_ADCHS;j++)
		{  
			if(((config.DataToSendChannel>>j)&0x01)==0)  //?????????
			{
				continue;
			}
			Parameter.vs[j]=0;
			hp_yy[j]=0;
		  /* 初始化 */
			switch(config.ADfrequence)
			{
			case 51200:
				arm_biquad_cascade_df1_init_f32(&S_test, numStages, (float32_t *)&IIRCoeffs51200_10HP[0], (float32_t
			*)&IIRStateF32[0]);
			/* IIR滤波 */
			arm_biquad_cascade_df1_f32(&S_test,(float32_t *)&ReceiveSamplesPeriod[SAMPLEblock][j][0], testOutput, config.ADfrequence*2);
//			arm_biquad_cascade_df1_f32(&S_test,(float32_t *)testOutput1, testOutput, config.ADfrequence*2);
			
			/*放缩系数 */
			ScaleValue = sacle16384_10HP;
				break;
			case 25600:
				arm_biquad_cascade_df1_init_f32(&S_test, numStages, (float32_t *)&IIRCoeffs25600_10HP[0], (float32_t
			*)&IIRStateF32[0]);
			/* IIR滤波 */
			arm_biquad_cascade_df1_f32(&S_test,(float32_t *)&ReceiveSamplesPeriod[SAMPLEblock][j][0], testOutput, config.ADfrequence*2);
			/*放缩系数 */
			 ScaleValue = sacle8192_10HP;
				break;
			case 12800:
				arm_biquad_cascade_df1_init_f32(&S_test, numStages, (float32_t *)&IIRCoeffs12800_10HP[0], (float32_t
			*)&IIRStateF32[0]);
			/* IIR滤波 */
			arm_biquad_cascade_df1_f32(&S_test,(float32_t *)&ReceiveSamplesPeriod[SAMPLEblock][j][0], testOutput, config.ADfrequence*2);
//			arm_biquad_cascade_df1_f32(&S_test,(float32_t *)testOutput1, testOutput, config.ADfrequence*2);
			
			/*放缩系数 */
			ScaleValue = sacle16384_10HP;
				break;
			default:
			arm_biquad_cascade_df1_init_f32(&S_test, numStages, (float32_t *)&IIRCoeffs16384_10HP[0], (float32_t
			*)&IIRStateF32[0]);
			/* IIR滤波 */
			arm_biquad_cascade_df1_f32(&S_test,(float32_t *)&ReceiveSamplesPeriod[SAMPLEblock][j][0], testOutput, config.ADfrequence*2);
			/*放缩系数 */
			 ScaleValue =sacle16384_10HP;
			break;
			}
			
			for(uint32_t i=0;i<2*config.ADfrequence;i++)	 
			{
				hp_yy[j]+=testOutput[i]*ScaleValue*1000*Parameter.ReciprocalofADfrequence;
				hp_yy[j]=hp_yy[j]*0.999f;//泄放直流分量
				if(i>=config.ADfrequence){	
//				ReceiveSamplesPeriod[SAMPLEblock][j][i]=testOutput[i];					
//				hp_yy[j]=testOutput[i]*ScaleValue;//1000*Parameter.ReciprocalofADfrequence;
//				ReceiveSamplesPeriod[SAMPLEblock][j][i]=hp_yy[j];
				Parameter.vs[j]+=hp_yy[j]*hp_yy[j];	
				}
			}
		float tt=Parameter.vs[j]*Parameter.ReciprocalofEMUnumber;//1000000*Parameter.ReciprocalofEMUnumber;
		arm_sqrt_f32(tt,(float32_t *)Parameter.fv+j);
		Parameter.vs[j]=0; //?????0		
		}
		for(uint32_t j=0;j<Acceleration_ADCHS;j++)
		{  
				if(((config.DataToSendChannel>>j)&0x01)==0)  //?????????
				{
					continue;
				}
				 InitIntermediateVariable(j); //???????
				 for(uint32_t i=config.ADfrequence;i<config.ADfrequence*(config.ADtime+1);i++)	 {
				 yy=ReceiveSamplesPeriod[SAMPLEblock][j][i];
				 Parameter.S_average[j]+=yy;
			 }
		}
		for(uint32_t j=0;j<Acceleration_ADCHS;j++)
		{
			Parameter.average[j]=Parameter.S_average[j]*Parameter.ReciprocalofEMUnumber;//?????
			if(0)
			{
				NeedRestartCollect();
			}
		}
		for(uint32_t j=0;j<Acceleration_ADCHS;j++)	 {
		 	if(((config.DataToSendChannel>>j)&0x01)==0)  //?????????
			{
				continue;
			}
      for(uint32_t i=config.ADfrequence;i<config.ADfrequence*(config.ADtime+1);i++)
	   {
      yy=ReceiveSamplesPeriod[SAMPLEblock][j][i]-Parameter.average[j];
			
			if(Parameter.InterMAX[j]<yy) Parameter.InterMAX[j]=yy;
			if(Parameter.InterMIN[j]>yy) Parameter.InterMIN[j]=yy;
			if(yy<0) absyy=-yy;
			 else absyy=yy;
			Parameter.Abs_S_average[j]+=absyy;
			arm_sqrt_f32(absyy,&INTERsqrt);
			Parameter.Inter_MarginIndex[j]+=INTERsqrt;
			Parameter.S_sum[j]+=yy;
			Parameter.SS_sum[j]+=yy*yy;
			Parameter.SSS_sum[j]+=(absyy*absyy*absyy);
			Parameter.SSSS_sum[j]+=(yy*yy*yy*yy);  	 
	   }
			
		}
		for(uint32_t j=0;j<Acceleration_ADCHS;j++)
		{
			updateParameter(j);
//			if(Parameter.EffectiveValue[j]>25)
//			{
//				NeedRestartCollect();
//			}
			InitIntermediateVariable(j); 
		}
	}

}

uint8_t BoardParameter_withtime(void)  //发送特征值
{
	uint8_t sendbuf[512];
	uint32_t empty_Acceleration_ADCHS=0;   //未发送通道
	sendbuf[0]=0x7e;
	sendbuf[1]=0x42;
	uint32_t iii=0;
	rtc_datetime_t currentime;
	RTC_GetDatetime(RTC,&currentime);
//	sendbuf[2]=0x32;
//	sendbuf[3]=0x00;
	sendbuf[4]=Sample_Time.year;
	sendbuf[5]=Sample_Time.year>>8;
	sendbuf[6]=Sample_Time.month;
	sendbuf[7]=Sample_Time.day; //时间用32位表示
	sendbuf[8]=Sample_Time.hour;
	sendbuf[9]=Sample_Time.minute;
	sendbuf[10]=Sample_Time.second; //时间用32位表示		
	for(uint32_t ii=0;ii<Acceleration_ADCHS;ii++)
	{
	if(((config.DataToSendChannel>>ii)&0x01)==0)  //未使能的通道不发送
	{
	 empty_Acceleration_ADCHS++;
	 continue;
	}

	sendbuf[11+50*iii]=ii+1;
	sendbuf[12+50*iii]=0x0c;
	

	uint8_t * floatdata=(uint8_t *)&Parameter.PeakValue[ii];//[0];
	sendbuf[13+50*iii]=*floatdata;
	sendbuf[14+50*iii]=*(floatdata+1);
	sendbuf[15+50*iii]=*(floatdata+2);
	sendbuf[16+50*iii]=*(floatdata+3);
	floatdata=(uint8_t *)&Parameter.MaxValue[ii];
	sendbuf[17+50*iii]=*floatdata;
	sendbuf[18+50*iii]=*(floatdata+1);
	sendbuf[19+50*iii]=*(floatdata+2);
	sendbuf[20+50*iii]=*(floatdata+3);
	floatdata=(uint8_t *)&Parameter.Abs_average[ii];
	sendbuf[21+50*iii]=*floatdata;
	sendbuf[22+50*iii]=*(floatdata+1);
	sendbuf[23+50*iii]=*(floatdata+2);
	sendbuf[24+50*iii]=*(floatdata+3);
	floatdata=(uint8_t *)&Parameter.EffectiveValue[ii];
	sendbuf[25+50*iii]=*floatdata;
	sendbuf[26+50*iii]=*(floatdata+1);
	sendbuf[27+50*iii]=*(floatdata+2);
	sendbuf[28+50*iii]=*(floatdata+3);
	floatdata=(uint8_t *)&Parameter.Kurtosis[ii];
	sendbuf[29+50*iii]=*floatdata;
	sendbuf[30+50*iii]=*(floatdata+1);
	sendbuf[31+50*iii]=*(floatdata+2);
	sendbuf[32+50*iii]=*(floatdata+3);
	floatdata=(uint8_t *)&Parameter.WaveformIndex[ii];
	sendbuf[33+50*iii]=*floatdata;
	sendbuf[34+50*iii]=*(floatdata+1);
	sendbuf[35+50*iii]=*(floatdata+2);
	sendbuf[36+50*iii]=*(floatdata+3);
	floatdata=(uint8_t *)&Parameter.PeakIndex[ii];
	sendbuf[37+50*iii]=*floatdata;
	sendbuf[38+50*iii]=*(floatdata+1);
	sendbuf[39+50*iii]=*(floatdata+2);
	sendbuf[40+50*iii]=*(floatdata+3);
	floatdata=(uint8_t *)&Parameter.PulseIndex[ii];
	sendbuf[41+50*iii]=*floatdata;
	sendbuf[42+50*iii]=*(floatdata+1);
	sendbuf[43+50*iii]=*(floatdata+2);
	sendbuf[44+50*iii]=*(floatdata+3);
	floatdata=(uint8_t *)&Parameter.MarginIndex[ii];
	sendbuf[45+50*iii]=*floatdata;
	sendbuf[46+50*iii]=*(floatdata+1);
	sendbuf[47+50*iii]=*(floatdata+2);
	sendbuf[48+50*iii]=*(floatdata+3);
	floatdata=(uint8_t *)&Parameter.KurtosisIndex[ii];
	sendbuf[49+50*iii]=*floatdata;
	sendbuf[50+50*iii]=*(floatdata+1);
	sendbuf[51+50*iii]=*(floatdata+2);
	sendbuf[52+50*iii]=*(floatdata+3);
	floatdata=(uint8_t *)&Parameter.Skew[ii];
	sendbuf[53+50*iii]=*floatdata;
	sendbuf[54+50*iii]=*(floatdata+1);
	sendbuf[55+50*iii]=*(floatdata+2);
	sendbuf[56+50*iii]=*(floatdata+3);
  floatdata=(uint8_t *)&Parameter.fv[ii];
	sendbuf[57+50*iii]=*floatdata;
	sendbuf[58+50*iii]=*(floatdata+1);
	sendbuf[59+50*iii]=*(floatdata+2);
	sendbuf[60+50*iii]=*(floatdata+3);
	iii++;
} 
	iii--;
  uint32_t length=50*(Acceleration_ADCHS-empty_Acceleration_ADCHS)+7;
  sendbuf[2]=(uint8_t)length;
	sendbuf[3]=(uint8_t)(length>>8);
	sendbuf[61+50*iii]=0;
	for(uint8_t i=1;i<(61+50*iii);i++)
	sendbuf[61+50*iii]+=sendbuf[i];
	sendbuf[62+50*iii]=0x7e;

	WriteDataToTXDBUF(sendbuf,(63+50*iii));
	//发送加速度跟速度，上面两个，下面发送速度
  
	return 1;
}


void FFTDATA_PROCESS_TASK ( void *pvParameters )
{ 
  volatile uint8_t firsttime=0;

 while(1)
 {
  while(!(xSemaphoreTake(SAMPLEDATA_ready, portMAX_DELAY) == pdTRUE))
			{};                // 		 
		if((config.DataToBoardMode==PARAMETERMODE)&&(config.ParameterTransimissonStatus==true))
			{
				NotNeedRestartCollect();
				EmuData();				
				BoardParameter_withtime();
			 
   	
  }
 }
/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
