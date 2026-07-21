/***
	*************************************************************************************************
	*	@file  	main.c
	*	@version V1.0
   *************************************************************************************************
   *  @description
	*
>>>>> 功能说明：
	*
	*	1.点亮LED，使用HAL库自带的延时函数实现闪烁
	*	2.使用QSPI驱动W25Q64，进行简单的读写测试
	*	3.QSPI间接模式，使用库函数直接读写，没有用到DMA和中断
	*	4.默认配置QSPI驱动时钟为125M
	*
>>>>> 串口打印说明：
	*
	*	USART1使用的是PA9/PA10，串口波特率115200
	*	
	************************************************************************************************
***/

#include "main.h"
#include "led.h"
#include "usart.h"
#include "qspi_w25q64.h"
#include <string.h>


/********************************************** 变量定义 *******************************************/

#define W25Qxx_NumByteToTest   	32*1024					// 测试数据的长度，64K

int32_t QSPI_Status ; 		 //检测标志位

uint32_t W25Qxx_TestAddr  =	0	;							// 测试地址
uint8_t  W25Qxx_WriteBuffer[W25Qxx_NumByteToTest];		//	写数据数组
uint8_t  W25Qxx_ReadBuffer[W25Qxx_NumByteToTest];		//	读数据数组


/***************************************************************************************************
*	函 数 名: QSPI_W25Qxx_Test
*	入口参数: 无
*	返 回 值: QSPI_W25Qxx_OK - 测试成功并通过
*	函数功能: 进行简单的读写测试，并计算速度
*	说    明: 无	
***************************************************************************************************/


int8_t QSPI_W25Qxx_Test(void)		//Flash读写测试
{
	uint32_t i = 0;	// 计数变量
	uint32_t ExecutionTime_Begin;		// 开始时间
	uint32_t ExecutionTime_End;		// 结束时间
	uint32_t ExecutionTime;				// 执行时间	
	float    ExecutionSpeed;			// 执行速度

// 擦除 >>>>>>>    
	
	ExecutionTime_Begin 	= HAL_GetTick();	// 获取 systick 当前时间，单位ms
	QSPI_Status 			= QSPI_W25Qxx_BlockErase_32K(W25Qxx_TestAddr);	// 擦除32K字节
	ExecutionTime_End		= HAL_GetTick();	// 获取 systick 当前时间，单位ms
	
	ExecutionTime = ExecutionTime_End - ExecutionTime_Begin; // 计算擦除时间，单位ms
	
	if( QSPI_Status == QSPI_W25Qxx_OK )
	{
		printf ("\r\nW25Q64 擦除成功, 擦除32K字节所需时间: %d ms\r\n",ExecutionTime);		
	}
	else
	{
		printf ("\r\n 擦除失败!!!!!  错误代码:%d\r\n",QSPI_Status);
		while (1);
	}	
	
// 写入 >>>>>>>    

	for(i=0;i<W25Qxx_NumByteToTest;i++)  //先将数据写入数组
	{
		W25Qxx_WriteBuffer[i] = i;
	}
	ExecutionTime_Begin 	= HAL_GetTick();	// 获取 systick 当前时间，单位ms
	QSPI_Status				= QSPI_W25Qxx_WriteBuffer(W25Qxx_WriteBuffer,W25Qxx_TestAddr,W25Qxx_NumByteToTest); // 写入数据
	ExecutionTime_End		= HAL_GetTick();	// 获取 systick 当前时间，单位ms
	
	ExecutionTime  = ExecutionTime_End - ExecutionTime_Begin; 		// 计算擦除时间，单位ms
	ExecutionSpeed = (float)W25Qxx_NumByteToTest / ExecutionTime ; // 计算写入速度，单位 KB/S
	if( QSPI_Status == QSPI_W25Qxx_OK )
	{
		printf ("\r\n写入成功,数据大小：%d KB, 耗时: %d ms, 写入速度：%.2f KB/s\r\n",W25Qxx_NumByteToTest/1024,ExecutionTime,ExecutionSpeed);		
	}
	else
	{
		printf ("\r\n写入错误!!!!!  错误代码:%d\r\n",QSPI_Status);
		while (1);
	}		
	
// 读取 >>>>>>>    
	
	ExecutionTime_Begin 	= HAL_GetTick();	// 获取 systick 当前时间，单位ms	
	QSPI_Status				= QSPI_W25Qxx_ReadBuffer(W25Qxx_ReadBuffer,W25Qxx_TestAddr,W25Qxx_NumByteToTest);	// 读取数据
	ExecutionTime_End		= HAL_GetTick();	// 获取 systick 当前时间，单位ms
	
	ExecutionTime  = ExecutionTime_End - ExecutionTime_Begin; 					// 计算擦除时间，单位ms
	ExecutionSpeed = (float)W25Qxx_NumByteToTest / ExecutionTime / 1024 ; 	// 计算读取速度，单位 MB/S 
	
	if( QSPI_Status == QSPI_W25Qxx_OK )
	{
		printf ("\r\n读取成功,数据大小：%d KB, 耗时: %d ms, 读取速度：%.2f MB/s \r\n",W25Qxx_NumByteToTest/1024,ExecutionTime,ExecutionSpeed);		
	}
	else
	{
		printf ("\r\n读取错误!!!!!  错误代码:%d\r\n",QSPI_Status);
		while (1);
	}			
	

	
// 读取整片Flash的数据，用以测试速度 >>>>>>>  	
	printf ("\r\n*****************************************************************************************************\r\n");		
	printf ("\r\n上面的测试中，读取的数据比较小，耗时很短，加之测量的最小单位为ms，计算出的读取速度误差较大\r\n");		
	printf ("\r\n接下来读取整片flash的数据用以测试速度，这样得出的速度误差比较小\r\n");		
	printf ("\r\n开始读取>>>>\r\n");		
	ExecutionTime_Begin 	= HAL_GetTick();	// 获取 systick 当前时间，单位ms		
	
	for(i=0;i<W25Qxx_FlashSize/(W25Qxx_NumByteToTest);i++)	// 每次读取 W25Qxx_NumByteToTest 字节的数据
	{
		QSPI_Status		 = QSPI_W25Qxx_ReadBuffer(W25Qxx_ReadBuffer,W25Qxx_TestAddr,W25Qxx_NumByteToTest);
		W25Qxx_TestAddr = W25Qxx_TestAddr + W25Qxx_NumByteToTest;		
	}
	ExecutionTime_End		= HAL_GetTick();	// 获取 systick 当前时间，单位ms
	
	ExecutionTime  = ExecutionTime_End - ExecutionTime_Begin; 					// 计算擦除时间，单位ms
	ExecutionSpeed = (float)W25Qxx_FlashSize / ExecutionTime / 1024 ; 	// 计算读取速度，单位 MB/S 

	if( QSPI_Status == QSPI_W25Qxx_OK )
	{
		printf ("\r\n读取成功,数据大小：%d MB, 耗时: %d ms, 读取速度：%.2f MB/s \r\n",W25Qxx_FlashSize/1024/1024,ExecutionTime,ExecutionSpeed);		
	}
	else
	{
		printf ("\r\n读取错误!!!!!  错误代码:%d\r\n",QSPI_Status);
		while (1);
	}	
	
	return QSPI_W25Qxx_OK ;  // 测试通过	
}



/********************************************** 函数声明 *******************************************/

void SystemClock_Config(void);		// 时钟初始化


/***************************************************************************************************
*	函 数 名: main
*	入口参数: 无
*	返 回 值: 无
*	函数功能: LED闪烁
*	说    明: 无
****************************************************************************************************/

int main(void)
{
	SCB_EnableICache();		// 使能ICache
	SCB_EnableDCache();		// 使能DCache
	HAL_Init();					// 初始化HAL库
	SystemClock_Config();	// 配置系统时钟，主频480MHz
	LED_Init();					// 初始化LED引脚
	USART1_Init();				// USART1初始化	
	
	QSPI_W25Qxx_Init();	   // 初始化W25Q64
	QSPI_W25Qxx_Test();		// Flash读写测试
	

	while (1)
	{
	  LED1_Toggle;			// LED1指示灯翻转
	  HAL_Delay(1000);	// 延时	
	}
}

/****************************************************************************************************/
/**
  * @brief  System Clock Configuration
  *         The system Clock is configured as follow : 
  *            System Clock source            = PLL (HSE)
  *            SYSCLK(Hz)                     = 480000000 (CPU Clock)
  *            HCLK(Hz)                       = 240000000 (AXI and AHBs Clock)
  *            AHB Prescaler                  = 2
  *            D1 APB3 Prescaler              = 2 (APB3 Clock  120MHz)
  *            D2 APB1 Prescaler              = 2 (APB1 Clock  120MHz)
  *            D2 APB2 Prescaler              = 2 (APB2 Clock  120MHz)
  *            D3 APB4 Prescaler              = 2 (APB4 Clock  120MHz)
  *            HSE Frequency(Hz)              = 25000000
  *            PLL_M                          = 5
  *            PLL_N                          = 192
  *            PLL_P                          = 2
  *            PLL_Q                          = 4
  *            PLL_R                          = 2
  *            VDD(V)                         = 3.3
  *            Flash Latency(WS)              = 4
  * @param  None
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  
  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}
  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
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
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
  
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART1|RCC_PERIPHCLK_QSPI; // 选择外设时钟
															

/*************************************** QSPI内核时钟设置 **************************************************/
/*** lxb ***
>>>>> 配置说明：
	*
	*	1.	W25Q64JV 所允许的最高驱动频率为133MHz(必须是后缀JV的器件才能达到133M)
	*	2. 750的QSPI最高驱动频率也是133MHz(参考STM32H750数据手册 第 7.3.18小节： Quad-SPI interface characteristics)
	*	3. QSPI内核时钟可以是 rcc_hclk3 / pll1_q_ck / pll2_r_ck / per_ck 其中一个，注意这里的内核时钟不是实际通信的驱动时钟！
	*	4. 其中 per_ck 的时钟源可以是 hse_ck / hsi_ker_ck / csi_ker_ck ，这三个时钟都只有几十兆，无法满足QSPI的高速通信需求
	*	5. rcc_hclk3 由系统时钟2分频之后产生，当主频为480M时，rcc_hclk3 = 240M
	*	6. pll1_q_ck 由 PLLN倍频之后（960M）再分频产生，最大可以设置为480M
	*	7. 因为QSPI驱动时钟只能由内核时钟进行整数分频，所以当选择 rcc_hclk3 和 pll1_q_ck作为内核时钟时，无法得到133M的最高速度
	*	8. 使用 pll2_r_ck 作为QSPI内核时钟,经过设置得到133M,然后QSPI将分频设置1,最终可以得到133M的驱动时钟
	*	9. QSPI内核时钟最高只允许到250M，详情可以查阅英文版的750参考手册 第8.5.8小节  Kernel clock selection（版本：RM0433 Rev 7 ，February 2020 ）
	*
>>>>> 重要说明：
	*
	*	1.W25QXX的擦除时间是限定的!!! 手册给出的典型参考值为: 4K-45ms, 32K-120ms ,64K-150ms,整片擦除20S
	*
	*	2.W25QXX的写入时间是限定的!!! 手册给出的典型参考值为: 256字节-0.4ms，也就是 1M字节/s （实测大概在600K字节/s左右）
	*
	*	3.如果使用库函数直接读取，那么是否使用DMA、是否开启Cache、编译器的优化等级以及数据存储区的位置(内部 TCM SRAM 或者 AXI SRAM)都会影响读取的速度
	*
	*	4.如果使用内存映射模式，则读取性能只与QSPI的驱动时钟以及是否开启Cache有关
	*
	*	5.使用库函数进行直接读取，keil版本5.30，编译器AC6.14，编译等级Oz image size，读取速度为 7M字节/S ，数据放在TCM SRAM 或者 AXI SRAM  
	*    都是差不多的结果，因为CPU直接访问外设寄存器的效率很低，直接使用HAL库进行读取的话，速度很慢
	*
	*	6.如果使用MDMA进行读取，可以达到 58M字节/S，使用内存映射模式的话，几乎可以达到驱动时钟的全速，62.14M/s
	*
	*  7.W25Q64JV 所允许的最高驱动频率为133MHz，750的QSPI最高驱动频率也是133MHz ，但是对于HAL库函数直接读取而言，驱动时钟超过15M已经不会对性能有提升
	*
	*	8.对于内存映射模式直接读取而言，驱动时钟超过127.5M已经不会对性能有提升，因为QSPI内核时钟最高限定为250M，所以建议实际QSPI驱动时钟不要超过125M，
	*	  具体的时钟配置请参考 SystemClock_Config 函数
	*
	*	9.实际使用中，当数据比较大时，建议使用64K或者32K擦除，擦除时间比4K擦除块	
	*
*** lxb ***/

  
  // 以下代码由cubeMX生成
	PeriphClkInitStruct.PLL2.PLL2M = 25;   // 晶振分频系数25(注：FK750M1-VBT6的晶振为25M)
	PeriphClkInitStruct.PLL2.PLL2N = 500;	// 将经过预分频后的晶振时钟进行500倍倍频，得到500M时钟
	PeriphClkInitStruct.PLL2.PLL2P = 2;		//	这个时钟无关QSPI，用户可自由配置和使用
	PeriphClkInitStruct.PLL2.PLL2Q = 2;		// 这个时钟无关QSPI，用户可自由配置和使用
	PeriphClkInitStruct.PLL2.PLL2R = 2;		// 进行2分频，得到 250M 的 pll2_r_ck 时钟
	PeriphClkInitStruct.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_0;
	PeriphClkInitStruct.PLL2.PLL2VCOSEL = RCC_PLL2VCOWIDE;
	PeriphClkInitStruct.PLL2.PLL2FRACN = 0;
  
	// 将 pll2_r_ck 设置为QSPI的内核时钟，按照上面的PLL2设置，此时 pll2_r_ck 为250MHz
 	PeriphClkInitStruct.QspiClockSelection = RCC_QSPICLKSOURCE_PLL2;	  // QSPI初始化的时候，设置为2分频即可得到125M驱动时钟
  
 	// 将 rcc_hclk3 设置为QSPI的内核时钟，当750主频设置为480M时， rcc_hclk3 为240MHz 
//	PeriphClkInitStruct.QspiClockSelection = RCC_QSPICLKSOURCE_D1HCLK;  // QSPI初始化的时候，设置为2分频即可得到120M驱动时钟

/********************************************************************************************************/     
  
  PeriphClkInitStruct.Usart16ClockSelection = RCC_USART16CLKSOURCE_D2PCLK2;
  
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }  
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
