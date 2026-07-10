/***********************************************
公司：轮趣科技(东莞)有限公司
品牌：WHEELTEC
官网：wheeltec.net
淘宝店铺：shop114407458.taobao.com 
速卖通: https://minibalance.aliexpress.com/store/4455017
版本：V1.0
修改时间：2022-09-05

Brand: WHEELTEC
Website: wheeltec.net
Taobao shop: shop114407458.taobao.com 
Aliexpress: https://minibalance.aliexpress.com/store/4455017
Version: V1.0
Update：2022-09-05

All rights reserved
***********************************************/

#include "Lidar.h"
#include <string.h>
#include "sys.h"



float Follow_KP =10,Follow_KD =1,Follow_KI = 0.001;	
float Velocity_KP = 1.1,Velocity_KI = 1;			//增量式PI参数，用于电机速度控制，霍尔编码器参数

PointDataProcessDef PointDataProcess[225] ;//更新225个数据
LiDARFrameTypeDef Pack_Data;
PointDataProcessDef Dataprocess[225];      //用于小车避障、跟随、走直线、ELE雷达避障的雷达数据

/**************************************************************************
Function: LIDAR_USART_Init
Input   : none
Output  : none
函数功能：雷达串口初始化
入口参数: 无 
返回  值：无
**************************************************************************/	 	
//串口5

void LIDAR_USART_Init(void)
{

	GPIO_InitTypeDef GPIO_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	USART_InitTypeDef USART_InitStructure;

	USART_DeInit(UART5);  //复位串口5

	// 打开串口GPIO的时钟
	  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE);
	
	// 打开串口外设的时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART5, ENABLE);
	

	// 将USART Tx的GPIO配置为推挽复用模式
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	// 将USART Rx的GPIO配置为浮空输入模式
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOD, &GPIO_InitStructure);	

	//配置USART为中断源 
	NVIC_InitStructure.NVIC_IRQChannel = UART5_IRQn;
	//抢断优先级
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	//子优先级 
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	//使能中断 
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	//初始化配置NVIC
	NVIC_Init(&NVIC_InitStructure);


	// 配置串口的工作参数
	// 配置波特率
	USART_InitStructure.USART_BaudRate =230400;
	// 配置 针数据字长
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	// 配置停止位
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	// 配置校验位
	USART_InitStructure.USART_Parity = USART_Parity_No ;
	// 配置硬件流控制
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	// 配置工作模式，收发一起
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	// 完成串口的初始化配置
	USART_Init(UART5, &USART_InitStructure);

	// 使能串口接收中断
	USART_ITConfig(UART5, USART_IT_RXNE, ENABLE);	
	//USART_ITConfig(UART5, USART_IT_TXE, ENABLE);

	// 使能串口
	USART_Cmd(UART5, ENABLE);	    

}

/**************************************************************************
Function: data_process
Input   : none
Output  : none
函数功能：数据处理函数
入口参数：无
返回  值：无
**************************************************************************/
//完成一帧接收后进行处理
void data_process(void) //数据处理
{
	static u8 data_cnt = 0;
	u8 i,m,n;
	u32 distance_sum[8]={0};//2个点的距离和的数组
	float start_angle = (((u16)Pack_Data.start_angle_h<<8)+Pack_Data.start_angle_l)/100.0;//计算16个点的开始角度
	float end_angle = (((u16)Pack_Data.end_angle_h<<8)+Pack_Data.end_angle_l)/100.0;//计算16个点的结束角度
	float area_angle[8]={0};//一帧数据的8个平均角度
	Lidar_flag=1;
	if((start_angle>350)&&(end_angle<13))//因为一帧数据是13度，避免350到10这段范围相加，最后angle反而变成180范围
		end_angle +=360;
	for(m=0;m<8;m++)
	{
		area_angle[m]=start_angle+(end_angle-start_angle)/8*m;
		if(area_angle[m]>360)  area_angle[m] -=360;
	}
	for(i=0;i<16;i++)
	{
		switch(i)
		{
			case 0:case 1:
				distance_sum[0] +=((u16)Pack_Data.point[i].distance_h<<8)+Pack_Data.point[i].distance_l;//0~1点的距离和
			  break;
			case 2:case 3:
				distance_sum[1] +=((u16)Pack_Data.point[i].distance_h<<8)+Pack_Data.point[i].distance_l;//2~3点的距离和
			  break;
			case 4:case 5:
				distance_sum[2] +=((u16)Pack_Data.point[i].distance_h<<8)+Pack_Data.point[i].distance_l;//4~5点的距离和
			  break;
			case 6:case 7:
				distance_sum[3] +=((u16)Pack_Data.point[i].distance_h<<8)+Pack_Data.point[i].distance_l;//6~7点的距离和
			  break;
			case 8:case 9:
				distance_sum[4] +=((u16)Pack_Data.point[i].distance_h<<8)+Pack_Data.point[i].distance_l;//8~9点的距离和
			  break;
			case 10:case 11:
				distance_sum[5] +=((u16)Pack_Data.point[i].distance_h<<8)+Pack_Data.point[i].distance_l;//10~11点的距离和
			  break;
			case 12:case 13:
				distance_sum[6] +=((u16)Pack_Data.point[i].distance_h<<8)+Pack_Data.point[i].distance_l;//12~13点的距离和
			  break;
			case 14:case 15:
				distance_sum[7] +=((u16)Pack_Data.point[i].distance_h<<8)+Pack_Data.point[i].distance_l;//14~15点的距离和
			  break;
			default:break;
								
		}
		  
	}
	for(n=0;n<8;n++)
	{
		PointDataProcess[data_cnt+n].angle = area_angle[n];
	  PointDataProcess[data_cnt+n].distance = distance_sum[n]/2;
	}
	data_cnt +=8;
	if(data_cnt>=225)
	{
		for(i=0;i<225;i++)
		{
			Dataprocess[i].angle=PointDataProcess[i].angle;
			Dataprocess[i].distance=PointDataProcess[i].distance;
		}
		data_cnt = 0;
	}
		
}

/**************************************************************************
Function: Serial port 5 receives interrupted
Input   : none
Output  : none
函数功能：串口5接收中断
入口参数：无
返回  值：无
**************************************************************************/
void UART5_IRQHandler(void)
{	

	static u8 state = 0;//状态位	
	static u8 crc_sum = 0;//校验和
	static u8 cnt = 0;//用于一帧16个点的计数
	u8 temp_data;
	if(USART_GetITStatus(UART5, USART_IT_RXNE) != RESET) //接收到数据
	{	
		USART_ClearITPendingBit(UART5, USART_IT_RXNE);
		temp_data=USART_ReceiveData(UART5);	
		switch(state)
		{
			case 0:
				if(temp_data == HEADER_0)//头固定
				{
					Pack_Data.header_0= temp_data;
					state++;
					//校验
					crc_sum += temp_data;
				} else state = 0,crc_sum = 0;
				break;
			case 1:
				if(temp_data == HEADER_1)//头固定
				{
					Pack_Data.header_1 = temp_data;
					state++;
					crc_sum += temp_data;
				} else state = 0,crc_sum = 0;
				break;
			case 2:
				if(temp_data == Length_)//字长固定
				{
					Pack_Data.ver_len = temp_data;
					state++;
					crc_sum += temp_data;
				} else state = 0,crc_sum = 0;
				break;
			case 3:
				Pack_Data.speed_h = temp_data;//速度高八位
				state++;
				crc_sum += temp_data;			
				break;
			case 4:
				Pack_Data.speed_l = temp_data;//速度低八位
				state++;
				crc_sum += temp_data;
				break;
			case 5:
				Pack_Data.start_angle_h = temp_data;//开始角度高八位
				state++;
				crc_sum += temp_data;
				break;
			case 6:
				Pack_Data.start_angle_l = temp_data;//开始角度低八位
				state++;
				crc_sum += temp_data;
				break;
			
			case 7:case 10:case 13:case 16:
			case 19:case 22:case 25:case 28:
			case 31:case 34:case 37:case 40:
			case 43:case 46:case 49:case 52:
				Pack_Data.point[cnt].distance_h = temp_data;//16个点的距离数据，高字节
				state++;
				crc_sum += temp_data;
				break;
			
			case 8:case 11:case 14:case 17:
			case 20:case 23:case 26:case 29:
			case 32:case 35:case 38:case 41:
			case 44:case 47:case 50:case 53:
				Pack_Data.point[cnt].distance_l = temp_data;//16个点的距离数据，低字节
				state++;
				crc_sum += temp_data;
				break;
			
			case 9:case 12:case 15:case 18:
			case 21:case 24:case 27:case 30:
			case 33:case 36:case 39:case 42:
			case 45:case 48:case 51:case 54:
				Pack_Data.point[cnt].Strong = temp_data;//16个点的强度数据
				state++;
				crc_sum += temp_data;
				cnt++;
				break;
			
			case 55:
				Pack_Data.end_angle_h = temp_data;//结束角度的高八位
				state++;
				crc_sum += temp_data;			
				break;
			case 56:
				Pack_Data.end_angle_l = temp_data;//结束角度的低八位
				state++;
				crc_sum += temp_data;
				break;
			case 57:
				Pack_Data.crc = temp_data;//校验
				state = 0;
				cnt = 0;
				if(crc_sum == Pack_Data.crc)
				{
					data_process();//数据处理，校验正确不断刷新存储的数据
				}
				else 
				{
					memset(&Pack_Data,0,sizeof(Pack_Data));//清零
				}
				crc_sum = 0;//校验和清零
				break;
			default: break;
		}
	}		

}


/**************************************************************************
Function: Distance_Adjust_PID
Input   : Current_Distance;Target_Distance
Output  : OutPut
函数功能：雷达距离pid
入口参数: 当前距离和目标距离
返回  值：电机目标速度
**************************************************************************/	 	
//雷达距离调整pid
float Distance_Adjust_PID(float Current_Distance,float Target_Distance)//距离调整PID
{
	static float Bias,OutPut,Last_Bias;
	Bias=Target_Distance-Current_Distance;                          	//计算偏差
	OutPut=-Distance_KP*Bias/100-Distance_KD*(Bias-Last_Bias)/100;//位置式PID控制器  //
	Last_Bias=Bias;                                       		 			//保存上一次偏差
	return OutPut;                                          	
}

/**************************************************************************
Function: Distance_Adjust_PID
Input   : Current_Distance;Target_Distance
Output  : OutPut
函数功能：雷达跟随距离pid
入口参数: 当前距离和目标距离
返回  值：电机目标速度
**************************************************************************/	 	

float Lidar_follow_PID(float Current_Distance,float Target_Distance)//距离调整PID
{
	static float Bias,OutPut,Last_Bias;
	Bias=Target_Distance-Current_Distance;                          	//计算偏差
	OutPut=-1.5*Bias-1*(Bias-Last_Bias);//位置式PID控制器  //
	Last_Bias=Bias;                                       		 			//保存上一次偏差
	return OutPut;                                          	
}

/**************************************************************************
Function: Follow_Turn_PID
Input   : Current_Angle;Target_Angle
Output  : OutPut
函数功能：雷达转向pid
入口参数: 当前角度和目标角度
返回  值：电机转向速度
**************************************************************************/	 	
//雷达转向pid
float Follow_Turn_PID(float Current_Angle,float Target_Angle)		                                 				 //求出偏差的积分
{
	static float Bias,OutPut,Integral_bias,Last_Bias;
	Bias=Target_Angle-Current_Angle;                         				 //计算偏差
	if(Integral_bias>1000) Integral_bias=1000;
	else if(Integral_bias<-1000) Integral_bias=-1000;
	OutPut=-Follow_KP*Bias-Follow_KI*Integral_bias-Follow_KD*(Bias-Last_Bias);	//位置式PID控制器
	Last_Bias=Bias;                                       					 		//保存上一次偏差
	if(Turn_Off(Angle_Balance,Voltage)== 1)								//电机关闭，此时积分清零
		Integral_bias = 0;
	return OutPut;                                           					 	//输出
	
}







