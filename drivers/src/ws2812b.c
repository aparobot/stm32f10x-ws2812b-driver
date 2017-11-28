#include "ws2812b.h"
#include <string.h>

static bool isInited = false;
static uint16_t ws2812bBuff[WS2812B_BUFF_SIZE];   // Array of data to be sent to leds.

static GPIO_InitTypeDef GPIO_InitStruct;
static TIM_TimeBaseInitTypeDef  TIM_TimeBaseStruct;
static TIM_OCInitTypeDef  TIM_OCInitStruct;
static DMA_InitTypeDef DMA_InitStruct;
static NVIC_InitTypeDef NVIC_InitStructure;
    
static void ws2812bSend(void);

void ws2812bInit(void)
{   
	RCC_APB2PeriphClockCmd(WS2812B_GPIO_PERIF, ENABLE);
	RCC_APB2PeriphClockCmd(WS2812B_TIM_PERIF, ENABLE);
	RCC_AHBPeriphClockCmd(WS2812B_DMA, ENABLE);
	
	GPIO_InitStruct.GPIO_Pin = WS2812B_GPIO_PIN;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(WS2812B_GPIO_PORT, &GPIO_InitStruct);
    
    TIM_DeInit(WS2812B_TIM);
    TIM_TimeBaseStruct.TIM_Period = WS2812B_TIM_PERIOD; 
    TIM_TimeBaseStruct.TIM_Prescaler = (uint16_t)(SystemCoreClock / WS2812B_TMI_CLOCK) - 1;
    TIM_TimeBaseStruct.TIM_ClockDivision = 0; 
    TIM_TimeBaseStruct.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStruct);
    
    TIM_OCInitStruct.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStruct.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStruct.TIM_Pulse = 0;
    TIM_OCInitStruct.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OCInitStruct.TIM_OutputNState = TIM_OutputNState_Disable;
    TIM_OCInitStruct.TIM_OCNPolarity = TIM_OCNPolarity_Low;
	TIM_OC1Init(TIM1, &TIM_OCInitStruct);
	TIM_OC1PreloadConfig(WS2812B_TIM, TIM_OCPreload_Enable);
	TIM_CtrlPWMOutputs(WS2812B_TIM, ENABLE);
	TIM_ARRPreloadConfig(WS2812B_TIM, ENABLE);
    
	DMA_DeInit(WS2812B_DMA_CHANNEL);                                           // Deinitialize DAM1 Channel 7 to their default reset values.
	DMA_InitStruct.DMA_PeripheralBaseAddr = WS2812B_DMA_PeripheralBaseAddr     // Specifies Physical address of the peripheral in this case Timer
	DMA_InitStruct.DMA_MemoryBaseAddr = (uint32_t)&ws2812bBuff;                // Specifies the buffer memory address
	DMA_InitStruct.DMA_DIR = DMA_DIR_PeripheralDST;                            // Data transfered from memory to peripheral
	DMA_InitStruct.DMA_BufferSize = WS2812B_BUFF_SIZE;                         // Specifies the buffer size
	DMA_InitStruct.DMA_PeripheralInc = DMA_PeripheralInc_Disable;              // Do not incrament the peripheral address
	DMA_InitStruct.DMA_MemoryInc = DMA_MemoryInc_Enable;                       // Incrament the buffer index
	DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;   // Specifies the peripheral data width 16bit
	DMA_InitStruct.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;           // Specifies the memory data width
	DMA_InitStruct.DMA_Mode = DMA_Mode_Normal;                                 // Specifies the operation mode. Normal or Circular
	DMA_InitStruct.DMA_Priority = DMA_Priority_High;                           // Specifies the software priority
	DMA_InitStruct.DMA_M2M = DMA_M2M_Disable;                                  // memory to memory disable
	DMA_Init(WS2812B_DMA_CHANNEL, &DMA_InitStruct);                            // Initialize DAM1 Channel 2 to values specified in the DMA_InitStruct structure.
    
    NVIC_InitStructure.NVIC_IRQChannel = WS2812B_DMA_NVIC_IRQChannel;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = WS2812B_NVIC_PRI;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
	
    DMA_ITConfig(WS2812B_DMA_CHANNEL, DMA_IT_TC, ENABLE);

    ws2812bClearAll();
    
    isInited = true;
}

bool ws2812bTest(void)
{
    #if(WS2812B_TEST == 1)
    ws2812bSet(0, 0xff0000);
    for(int i = 1000000; i > 0; i--);
    ws2812bClear(0);
    #endif
    return isInited;
}

void DMA1_Channel2_IRQHandler(void)
{
	DMA_ClearITPendingBit(DMA1_IT_TC2);        // clear DMA1 transfer complete interrupt flag
	TIM_Cmd(TIM1, DISABLE);                    // disable the TIM and DMA channels
	DMA_Cmd(WS2812B_DMA_CHANNEL, DISABLE);
	DMA_ClearFlag(DMA1_FLAG_TC2);              // clear DMA1 Channel 2 transfer complete flag
	TIM_DMACmd(TIM1, TIM_DMA_CC1, DISABLE);    // disable the DMA requests
}

void ws2812bClear(const int index)
{
    ws2812bSet(index, 0);
}

void ws2812bClearAll()
{
    int32_t i, j, k = 0;
    
    for (i = 0; i < WS2812B_NUM; i++)
    {
        for (j = 0; j < WS2812B_PULSE_BIT_SIZE; j++)
        {
            ws2812bBuff[k++] = WS2812B_PWM_LOW_PULSE_WIDTH;
        }
    }

    memset(&ws2812bBuff[k], 0, WS2812B_BUFF_SIZE - k);
    ws2812bSend();
}

void ws2812bSet(const int index, const int32_t color)
{
    int32_t i, j;
    int32_t k = 0;
    int32_t clr = color;
    
    if(index >= WS2812B_NUM || (index < 0 && index != WS2812B_ALL_INDEX))
    {
        return;
    }
    
    k = index * WS2812B_PULSE_BIT_SIZE;
    
    if(index != WS2812B_ALL_INDEX)
    {
        WS2812B_CREATE_PULSE_BUFF();
    }
    else
    {
        for(i = 0; i < WS2812B_NUM; i++)
            WS2812B_CREATE_PULSE_BUFF();
    }

    ws2812bSend();
}

void ws2812bSetAll(const int32_t color)
{
    ws2812bSet(WS2812B_ALL_INDEX, color);
}

static void ws2812bSend(void)
{
	DMA_SetCurrDataCounter(WS2812B_DMA_CHANNEL, WS2812B_BUFF_SIZE); 
	DMA_Cmd(WS2812B_DMA_CHANNEL, ENABLE);
	TIM_DMACmd(TIM1, TIM_DMA_CC1, ENABLE);
	TIM_Cmd(TIM1, ENABLE);
}
