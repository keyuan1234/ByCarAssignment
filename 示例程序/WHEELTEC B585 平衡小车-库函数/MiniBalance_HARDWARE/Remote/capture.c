/***********************************************
公司：轮趣科技（东莞）有限公司
品牌：WHEELTEC
官网：wheeltec.net
淘宝店铺：shop114407458.taobao.com 
速卖通: https://minibalance.aliexpress.com/store/4455017
版本：V1.0
修改时间：2023-03-02

Brand: WHEELTEC
Website: wheeltec.net
Taobao shop: shop114407458.taobao.com 
Aliexpress: https://minibalance.aliexpress.com/store/4455017
Version: V1.0
Update：2023-03-02

All rights reserved
***********************************************/

//这个模块默认不使用
//4路超声波模块
//或 4路航模遥控
#include "capture.h"

//Variables related to remote control acquisition of model aircraft
//航模遥控采集相关变量
int Remoter_Ch1=1500,Remoter_Ch2=1500,Remoter_Ch3=1500;
//Model aircraft remote control receiver variable
//航模遥控接收变量
int L_Remoter_Ch1=1500,L_Remoter_Ch2=1500,L_Remoter_Ch3=1500;  
u16 TIM2CH2_CAPTURE_STA,TIM2CH2_CAPTURE_VAL;



TIM_ICUserValueTypeDef PWM_TIM2_CH4_ICUserValueStructure = {0,0,0,0};//航模遥控第一路
TIM_ICUserValueTypeDef PWM_TIM2_CH3_ICUserValueStructure = {0,0,0,0};//航模遥控第二路
TIM_ICUserValueTypeDef PWM_TIM1_CH4_ICUserValueStructure = {0,0,0,0};//航模遥控第三路
TIM_ICUserValueTypeDef PWM_TIM1_CH1_ICUserValueStructure = {0,0,0,0};//航模遥控第四路


/**************************************************************************
Function: PWM_Capture_GPIO_Config
Input   : none
Output  : none
函数功能：航模遥控端口初始化
入口参数: 无
返回  值：无
**************************************************************************/	 	
void PWM_Capture_GPIO_Config(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_APB2PeriphClockCmd(PWM_TIM2_CH4_GPIO_CLK,ENABLE);//航模遥控第一路接口
	RCC_APB2PeriphClockCmd(PWM_TIM2_CH3_GPIO_CLK,ENABLE);//航模遥控第二路接口
//	RCC_APB2PeriphClockCmd(PWM_TIM1_CH4_GPIO_CLK,ENABLE);//航模遥控第三路接口
//	RCC_APB2PeriphClockCmd(PWM_TIM1_CH1_GPIO_CLK,ENABLE);//航模遥控第四路接口

	//第一路
	GPIO_InitStructure.GPIO_Pin  = PWM_TIM2_CH4_PIN; 
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD; 				//PA3 输入  
	GPIO_Init(PWM_TIM2_CH4_PORT, &GPIO_InitStructure);


	//第二路
	GPIO_InitStructure.GPIO_Pin  = PWM_TIM2_CH3_PIN; 
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD; 				//PA2 输入  
	GPIO_Init(PWM_TIM2_CH3_PORT, &GPIO_InitStructure);

//	//第三路
//	GPIO_InitStructure.GPIO_Pin  = PWM_TIM1_CH4_PIN; 
//	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD; 				//PA11 输入  
//	GPIO_Init(PWM_TIM1_CH4_PORT, &GPIO_InitStructure);
//	
//	//第四路
//	GPIO_InitStructure.GPIO_Pin  = PWM_TIM1_CH1_PIN; 
//	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD; 				//PA8 输入  
//	GPIO_Init(PWM_TIM1_CH1_PORT, &GPIO_InitStructure);
}

/**************************************************************************
Function: PWM_Capture_Mode_Config
Input   : TIM_Period,TIM_Prescaler
Output  : none
函数功能：航模遥控捕获PWM高电平初始化
入口参数: 预装载值和预分频器 
返回  值：无
**************************************************************************/	 	
void PWM_Capture_Mode_Config(u16 arr,u16 psc)
{
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	TIM_ICInitTypeDef  TIM_ICInitStructure;

	//使能定时器时钟。TIM2
	PWM_TIM2_APBxClock_FUN(PWM_TIM2_CLK,ENABLE);				
	//使能定时器时钟。TIM1
//	PWM_TIM1_APBxClock_FUN(PWM_TIM1_CLK,ENABLE);
	
	/*时基定时器初始化*/
	//定时器2
	TIM_TimeBaseStructure.TIM_Period = arr; 					//设定计数器自动重装值 
	TIM_TimeBaseStructure.TIM_Prescaler =psc; 					//预分频器   
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1; 	//设置时钟分割:TDTS = Tck_tim
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; //TIM向上计数模式
	TIM_TimeBaseInit(PWM_TIM2, &TIM_TimeBaseStructure); 		//根据TIM_TimeBaseInitStruct中指定的参数初始化TIMx的时间基数单位

//	//定时器1
//	TIM_TimeBaseStructure.TIM_Period = arr; 					//设定计数器自动重装值 
//	TIM_TimeBaseStructure.TIM_Prescaler =psc; 					//预分频器   
//	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1; 	//设置时钟分割:TDTS = Tck_tim
//	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; //TIM向上计数模式
//	TIM_TimeBaseInit(PWM_TIM1, &TIM_TimeBaseStructure); 		//根据TIM_TimeBaseInitStruct中指定的参数初始化TIMx的时间基数单位

	/*捕获通道初始化*/
	//定时器2CH3，CH4
	TIM_ICInitStructure.TIM_Channel = TIM_Channel_3; 			//通道3
	TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;	//上升沿捕获
	TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
	TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;	 	//配置输入分频,不分频 
	TIM_ICInitStructure.TIM_ICFilter = 0x00;					//配置输入滤波器 不滤波
	TIM_ICInit(PWM_TIM2, &TIM_ICInitStructure);

	TIM_ICInitStructure.TIM_Channel = TIM_Channel_4; 			//通道4
	TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;	//上升沿捕获
	TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
	TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;	 	//配置输入分频,不分频 
	TIM_ICInitStructure.TIM_ICFilter = 0x00;					//配置输入滤波器 不滤波
	TIM_ICInit(PWM_TIM2, &TIM_ICInitStructure);

//	//定时器1CH4
//	TIM_ICInitStructure.TIM_Channel = TIM_Channel_4; 			//通道4
//	TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;	//上升沿捕获
//	TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
//	TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;	 	//配置输入分频,不分频 
//	TIM_ICInitStructure.TIM_ICFilter = 0x00;					//配置输入滤波器 不滤波
//	TIM_ICInit(PWM_TIM1, &TIM_ICInitStructure);

//	TIM_ICInitStructure.TIM_Channel = TIM_Channel_3; 			//通道4
//	TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;	//上升沿捕获
//	TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
//	TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;	 	//配置输入分频,不分频 
//	TIM_ICInitStructure.TIM_ICFilter = 0x00;					//配置输入滤波器 不滤波
//	TIM_ICInit(PWM_TIM1, &TIM_ICInitStructure);

	//使能中断设置
	//TIM2_CH3,CH4
	TIM_ITConfig(PWM_TIM2,TIM_IT_Update|TIM_IT_CC3|TIM_IT_CC4,ENABLE);	//允许更新中断和捕获中断	
	//TIM1_CH3，CH4
//	TIM_ITConfig(PWM_TIM1,TIM_IT_Update|TIM_IT_CC4,ENABLE);	//允许更新中断 ,允许CC4IE捕获中断	

	//定时器使能
	TIM_Cmd(PWM_TIM2,ENABLE); 										//使能定时器2
	
//	TIM_Cmd(PWM_TIM1,ENABLE); 										//使能定时器1
	
}
/**************************************************************************
Function: PWM_Capture_Mode_Config
Input   : none
Output  : none
函数功能：航模遥控捕获PWM高电平初始化
入口参数: 无 
返回  值：无
**************************************************************************/	 	

void PWM_Capture_NVIC_Config(void)
{
	NVIC_InitTypeDef NVIC_InitStructure;

	//中断优先级设置
	//TIM2总中断
	NVIC_InitStructure.NVIC_IRQChannel = PWM_TIM2_IRQ;  		//TIM2中断
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;  	//先占优先级2级
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;  		//从优先级2级
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE; 			//IRQ通道被使能
	NVIC_Init(&NVIC_InitStructure);  							//根据NVIC_InitStruct中指定的参数初始化外设NVIC寄存器 

//	//TIM1更新中断和捕获中断
//	NVIC_InitStructure.NVIC_IRQChannel = PWM_TIM1_CC_IRQn;  	//TIM1捕获中断
//	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;  	//先占优先级2级
//	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;  		//从优先级2级
//	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE; 			//IRQ通道被使能
//	NVIC_Init(&NVIC_InitStructure);  							//根据NVIC_InitStruct中指定的参数初始化外设NVIC寄存器 

//	NVIC_InitStructure.NVIC_IRQChannel = CAPTURE_TIM1_UP_IRQn;  //TIM1更新中断
//	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;  	//先占优先级2级
//	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;  		//从优先级2级
//	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE; 			//IRQ通道被使能
//	NVIC_Init(&NVIC_InitStructure);  							//根据NVIC_InitStruct中指定的参数初始化外设NVIC寄存器 

}




/**************************************************************************
Function: PWM_Capture_Mode_Config
Input   : TIM_Period,TIM_Prescaler
Output  : none
函数功能：航模遥控捕获PWM高电平初始化
入口参数: 预装载值和预分频器 
返回  值：无
**************************************************************************/	 	
void PWM_Cap_Init(u16 arr,u16 psc)
{
	PWM_Capture_GPIO_Config();
	PWM_Capture_NVIC_Config();
	PWM_Capture_Mode_Config(arr,psc);
}



//使用航模遥控
#ifdef PWM_Capture
/**************************************************************************
Function: PWM_TIM2_IRQHandler
Input   : none
Output  : none
函数功能：高电平捕获中断函数
入口参数: 无
返回  值：无
**************************************************************************/	 	
void PWM_TIM2_IRQHandler(void)
{
	static u8 ch1_filter_times=0,ch2_filter_times=0;
	u16 tsr;
	tsr=TIM2->SR;
	if((TIM2CH2_CAPTURE_STA&0X80)==0)//还未成功捕获	
	{
		if(tsr&0X01)//定时器溢出
		{	    
			 if(TIM2CH2_CAPTURE_STA&0X40)//已经捕获到高电平了
			 {
				 if((TIM2CH2_CAPTURE_STA&0X3F)==0X3F)//高电平太长了
				 {
					  TIM2CH2_CAPTURE_STA|=0X80;      //标记成功捕获了一次
						TIM2CH2_CAPTURE_VAL=0XFFFF;
				 }else TIM2CH2_CAPTURE_STA++;
			 }	 
		}
		if(tsr&0x04)//捕获2发生捕获事件
		{	
			if(TIM2CH2_CAPTURE_STA&0X40)		  //捕获到一个下降沿 		
			{	  	
         	    
				TIM2CH2_CAPTURE_STA|=0X80;		  //标记成功捕获到一次高电平脉宽
				TIM2CH2_CAPTURE_VAL=TIM2->CCR2;	//获取当前的捕获值.
				TIM2->CCER&=~(1<<5);			      //CC2P=0 设置为上升沿捕获
			}
			else  								     //还未开始,第一次捕获上升沿
			{
				 TIM2CH2_CAPTURE_STA=0;	 //清空
				 TIM2CH2_CAPTURE_VAL=0;
				 TIM2CH2_CAPTURE_STA|=0X40;		//标记捕获到了上升沿
				 TIM2->CNT=0;									//计数器清空
				 TIM2->CCER|=1<<5; 						//CC2P=1 设置为下降沿捕获
			}		    
		}			     	    					   
	}
	//连接航模遥遥控器后，需要推下前进杆，才可以正式航模控制小车
	//After connecting the remote controller of the model aircraft, 
	//you need to push down the forward lever to officially control the car of the model aircraft
  if(Remoter_Ch2>1600&&Remote_ON_Flag==0)
  {
		//Model aircraft remote control mark position 1, other marks position 0
		//航模遥控标志位置1，其它标志位置0
		Remote_ON_Flag=1;
		PS2_ON_Flag=0;
	}
	/*************************************通道3*******************************************/
	if(PWM_TIM2_CH3_ICUserValueStructure.Capture_FinishFlag == 0)//没有完成一次的时候捕获才能进去，防止溢出次数错误
	{
		
		if ( TIM_GetITStatus ( PWM_TIM2, TIM_IT_Update) != RESET )               
			PWM_TIM2_CH3_ICUserValueStructure.Capture_Period ++;
			
		// 捕获中断，第一次是上升沿中断，第二次是下降沿中断
		if ( TIM_GetITStatus (PWM_TIM2, TIM_IT_CC3 ) != RESET)
		{
			// 第一次捕获
			if ( PWM_TIM2_CH3_ICUserValueStructure.Capture_StartFlag == 0 )
			{
				//第一次捕获时，把捕获值储存起来
				PWM_TIM2_CH3_ICUserValueStructure.Capture_CcrValue = TIM_GetCapture3 (PWM_TIM2);
				// 自动重装载寄存器更新标志清0
				PWM_TIM2_CH3_ICUserValueStructure.Capture_Period = 0;
//				// 存捕获比较寄存器的值的变量的值清0			
//				Distance_TIM2_CH2_ICUserValueStructure.Capture_CcrValue = 0;

				// 当第一次捕获到上升沿之后，就把捕获边沿配置为下降沿
				TIM_OC3PolarityConfig(PWM_TIM2, CAPTURE_TIM_END_ICPolarity);
				// 开始捕获标志位置1			
				PWM_TIM2_CH3_ICUserValueStructure.Capture_StartFlag = 1;			
			}
			// 下降沿捕获中断
			else // 第二次捕获
			{
				
				// 获取捕获比较寄存器的值，这个值就是捕获到的高电平的时间的值
				Remoter_Ch1 = TIM_GetCapture3 (PWM_TIM2)-PWM_TIM2_CH3_ICUserValueStructure.Capture_CcrValue;
				if(abs(Remoter_Ch1-L_Remoter_Ch1)>500)
				{
					ch1_filter_times++;
					if(ch1_filter_times<=5) Remoter_Ch1 = L_Remoter_Ch1;
					else ch1_filter_times = 0;
				}
				else
					ch1_filter_times=0;
				L_Remoter_Ch1 = Remoter_Ch1;

				// 当第二次捕获到下降沿之后，就把捕获边沿配置为上升沿，好开启新的一轮捕获
				TIM_OC3PolarityConfig(PWM_TIM2, CAPTURE_TIM_STRAT_ICPolarity);
				// 开始捕获标志清0		
				PWM_TIM2_CH3_ICUserValueStructure.Capture_StartFlag = 0;
				// 捕获完成标志置1			
//				PWM_TIM2_CH3_ICUserValueStructure.Capture_FinishFlag = 1;		
			}
			//清除中断
			TIM_ClearITPendingBit (PWM_TIM2,TIM_IT_CC3);	
		}		
	}
	/*************************************通道4*******************************************/
	if(PWM_TIM2_CH4_ICUserValueStructure.Capture_FinishFlag == 0)//没有完成一次的时候捕获才能进去，防止溢出次数错误
	{
		if ( TIM_GetITStatus ( PWM_TIM2, TIM_IT_Update) != RESET )               
			PWM_TIM2_CH4_ICUserValueStructure.Capture_Period ++;
			
		// 捕获中断，第一次是上升沿中断，第二次是下降沿中断
		if ( TIM_GetITStatus (PWM_TIM2, TIM_IT_CC4 ) != RESET)
		{
			// 第一次捕获
			if ( PWM_TIM2_CH4_ICUserValueStructure.Capture_StartFlag == 0 )
			{
				//第一次捕获时，把捕获值储存起来
				PWM_TIM2_CH4_ICUserValueStructure.Capture_CcrValue = TIM_GetCapture4 (PWM_TIM2);
				// 自动重装载寄存器更新标志清0
				PWM_TIM2_CH4_ICUserValueStructure.Capture_Period = 0;
//				// 存捕获比较寄存器的值的变量的值清0			
//				Distance_TIM2_CH2_ICUserValueStructure.Capture_CcrValue = 0;

				// 当第一次捕获到上升沿之后，就把捕获边沿配置为下降沿
				TIM_OC4PolarityConfig(PWM_TIM2, CAPTURE_TIM_END_ICPolarity);
				// 开始捕获标志位置1			
				PWM_TIM2_CH4_ICUserValueStructure.Capture_StartFlag = 1;			
			}
			// 下降沿捕获中断
			else // 第二次捕获
			{
				// 获取捕获比较寄存器的值，这个值就是捕获到的高电平的时间的值
				Remoter_Ch2 = TIM_GetCapture4(PWM_TIM2)-PWM_TIM2_CH4_ICUserValueStructure.Capture_CcrValue;
                if(abs(Remoter_Ch2-L_Remoter_Ch2)>500)
				{
					ch2_filter_times++;
					if(ch2_filter_times<=5) Remoter_Ch2 = L_Remoter_Ch2;
					else ch2_filter_times = 0;
				}
				else
					ch2_filter_times=0;
				L_Remoter_Ch2 = Remoter_Ch2;
				// 当第二次捕获到下降沿之后，就把捕获边沿配置为上升沿，好开启新的一轮捕获
				TIM_OC4PolarityConfig(PWM_TIM2, CAPTURE_TIM_STRAT_ICPolarity);
				// 开始捕获标志清0		
				PWM_TIM2_CH4_ICUserValueStructure.Capture_StartFlag = 0;
				// 捕获完成标志置1			
//				PWM_TIM2_CH4_ICUserValueStructure.Capture_FinishFlag = 1;		
			}
			//清除中断
			TIM_ClearITPendingBit (PWM_TIM2,TIM_IT_CC4);	
		}		
	}
	TIM_ClearITPendingBit (CAPTURE_TIM2,TIM_IT_Update|TIM_IT_CC2|TIM_IT_CC3|TIM_IT_CC4);	

}

/**************************************************************************
Function: PWM_TIM1_CC_IRQHandler
Input   : none
Output  : none
函数功能：高电平捕获中断函数
入口参数: 无
返回  值：无
**************************************************************************/	 	

void PWM_TIM1_CC_IRQHandler(void)
{
	static u8 ch3_filter_times=0;
	/*************************************通道4*******************************************/
	if(PWM_TIM1_CH4_ICUserValueStructure.Capture_FinishFlag == 0)//没有完成一次的时候捕获才能进去，防止溢出次数错误
	{
		// 捕获中断，第一次是上升沿中断，第二次是下降沿中断
		if ( TIM_GetITStatus (PWM_TIM1, TIM_IT_CC4 ) != RESET)
		{
			// 第一次捕获
			if ( PWM_TIM1_CH4_ICUserValueStructure.Capture_StartFlag == 0 )
			{
				//第一次捕获时，把捕获值储存起来
				PWM_TIM1_CH4_ICUserValueStructure.Capture_CcrValue = 
				TIM_GetCapture4 (PWM_TIM1);
				// 自动重装载寄存器更新标志清0
				PWM_TIM1_CH4_ICUserValueStructure.Capture_Period = 0;
//				// 存捕获比较寄存器的值的变量的值清0			
//				Distance_TIM2_CH2_ICUserValueStructure.Capture_CcrValue = 0;

				// 当第一次捕获到上升沿之后，就把捕获边沿配置为下降沿
				TIM_OC4PolarityConfig(PWM_TIM1, CAPTURE_TIM_END_ICPolarity);
				// 开始捕获标志位置1			
				PWM_TIM1_CH4_ICUserValueStructure.Capture_StartFlag = 1;			
			}
			// 下降沿捕获中断
			else // 第二次捕获
			{
				// 获取捕获比较寄存器的值，这个值就是捕获到的高电平的时间的值
				Remoter_Ch3 = TIM_GetCapture4(PWM_TIM1)-PWM_TIM1_CH4_ICUserValueStructure.Capture_CcrValue;
                if(abs(Remoter_Ch3-L_Remoter_Ch3)>500)
				{
					ch3_filter_times++;
					if(ch3_filter_times<=5) Remoter_Ch3 = L_Remoter_Ch3;
					else ch3_filter_times = 0;
				}
				else
					ch3_filter_times=0;
				L_Remoter_Ch3 = Remoter_Ch3;
				// 当第二次捕获到下降沿之后，就把捕获边沿配置为上升沿，好开启新的一轮捕获
				TIM_OC4PolarityConfig(PWM_TIM1, CAPTURE_TIM_STRAT_ICPolarity);
				// 开始捕获标志清0		
				PWM_TIM1_CH4_ICUserValueStructure.Capture_StartFlag = 0;
				PWM_TIM1_CH4_ICUserValueStructure.Capture_FinishFlag = 0;
				// 捕获完成标志置1			
			}
			//清除中断
			TIM_ClearITPendingBit (PWM_TIM1,TIM_IT_CC4);	
		}		
	}

	TIM_ClearITPendingBit (CAPTURE_TIM1,TIM_IT_CC4);	
}

/**************************************************************************
Function: PWM_TIM1_UP_IRQHandler
Input   : none
Output  : none
函数功能：高电平捕获中断函数
入口参数: 无
返回  值：无
**************************************************************************/	 	

void PWM_TIM1_UP_IRQHandler(void)
{
	//通道4
	if(PWM_TIM1_CH4_ICUserValueStructure.Capture_FinishFlag == 0)//没有完成一次的时候捕获才能进去，防止溢出次数错误
	{
		if ( TIM_GetITStatus ( PWM_TIM1, TIM_IT_Update) != RESET )               
			PWM_TIM1_CH4_ICUserValueStructure.Capture_Period ++;
	}
	//通道1
	if(PWM_TIM1_CH1_ICUserValueStructure.Capture_FinishFlag == 0)//没有完成一次的时候捕获才能进去，防止溢出次数错误
	{
		if ( TIM_GetITStatus ( PWM_TIM1, TIM_IT_Update) != RESET )               
			PWM_TIM1_CH1_ICUserValueStructure.Capture_Period ++;
	}

	TIM_ClearITPendingBit (PWM_TIM1,TIM_IT_Update);	

}


#endif

#ifdef Distance_Capture
/**************************************************************************
Function: Timer 2 channel 2 input capture initialization
Input   : arr：Auto reload value； psc： Clock prescaled frequency
Output  : none
函数功能：定时器2通道2输入捕获初始化
入口参数: arr：自动重装值； psc：时钟预分频数 
返回  值：无
**************************************************************************/	 		
TIM_ICInitTypeDef  TIM2_ICInitStructure;
void Distance_Cap_Init(u16 arr,u16 psc)	
{	 
	GPIO_InitTypeDef GPIO_InitStructure;
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
 	NVIC_InitTypeDef NVIC_InitStructure;
    
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);	//使能TIM2时钟
 	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA|RCC_APB2Periph_GPIOC, ENABLE);  //使能GPIOA时钟
	
	GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_1; 
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD; //PA1 输入  
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;     //50M
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_15;     
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;     //PA3输出 
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;     //50M
	GPIO_Init(GPIOC, &GPIO_InitStructure);
	
	
	//初始化定时器2 TIM2	 
	TIM_TimeBaseStructure.TIM_Period = arr; //设定计数器自动重装值 
	TIM_TimeBaseStructure.TIM_Prescaler =psc; 	//预分频器   
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1; //设置时钟分割:TDTS = Tck_tim
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;  //TIM向上计数模式
	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure); //根据TIM_TimeBaseInitStruct中指定的参数初始化TIMx的时间基数单位	
	//初始化TIM2输入捕获参数
	TIM2_ICInitStructure.TIM_Channel = TIM_Channel_2; //CC1S=02 	选择输入端 IC2映射到TI1上
    TIM2_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;	//上升沿捕获
    TIM2_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
    TIM2_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;	 //配置输入分频,不分频 
    TIM2_ICInitStructure.TIM_ICFilter = 0x00;//配置输入滤波器 不滤波
    TIM_ICInit(TIM2, &TIM2_ICInitStructure);
	
	//中断分组初始化
	NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;  //TIM2中断
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;  //先占优先级1级
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;  //从优先级1级
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE; //IRQ通道被使能
	NVIC_Init(&NVIC_InitStructure);  //根据NVIC_InitStruct中指定的参数初始化外设NVIC寄存器 	
	TIM_ITConfig(TIM2,TIM_IT_Update|TIM_IT_CC2,ENABLE);//允许更新中断 ,允许CC2IE捕获中断	
    TIM_Cmd(TIM2,ENABLE ); 	//使能定时器2
}
/**************************************************************************
Function: Ultrasonic receiving echo function
Input   : none
Output  : none
函数功能：超声波接收回波函数
入口参数: 无 
返回  值：无
**************************************************************************/	 	
void Read_Distane(void)        
{   
	 PCout(15)=1;         
	 delay_us(15);  
	 PCout(15)=0;	
	 if(TIM2CH2_CAPTURE_STA&0X80)//成功捕获到了一次高电平
	 {
		 Distance=TIM2CH2_CAPTURE_STA&0X3F; 
		 Distance*=65536;					        //溢出时间总和
		 Distance+=TIM2CH2_CAPTURE_VAL;		//得到总的高电平时间
		 Distance=Distance*170/1000;      //时间*声速/2（来回） 一个计数0.001ms
		 TIM2CH2_CAPTURE_STA=0;			//开启下一次捕获
	 }				
}

#endif

