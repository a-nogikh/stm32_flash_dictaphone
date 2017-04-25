#include "recorder.h"
#include "pdm_filter.h"

#define INTERNAL_BUFF_SIZE 64

#define SPI_SCK_PIN                       GPIO_Pin_10
#define SPI_SCK_GPIO_PORT                 GPIOB
#define SPI_SCK_GPIO_CLK                  RCC_AHB1Periph_GPIOB
#define SPI_SCK_SOURCE                    GPIO_PinSource10
#define SPI_SCK_AF                        GPIO_AF_SPI2

#define SPI_MOSI_PIN                      GPIO_Pin_3
#define SPI_MOSI_GPIO_PORT                GPIOC
#define SPI_MOSI_GPIO_CLK                 RCC_AHB1Periph_GPIOC
#define SPI_MOSI_SOURCE                   GPIO_PinSource3
#define SPI_MOSI_AF                       GPIO_AF_SPI2

static volatile int bufs_swapped = 0;
static PDMFilter_InitStruct Filter;
volatile uint8_t need_to_stop = 0;

static uint16_t internal_buffer[INTERNAL_BUFF_SIZE];
static uint32_t internal_buffer_size = 0;

static volatile uint16_t out_buffer[16], out_buffer2[16];
static volatile uint16_t *curr_buffer, *to_process;

static void record_gpio_init(void);
static void record_spi_init(void);
static void record_nvic_init(void);

void record_init(void)
{
  Filter.LP_HZ = 8000.0;
  Filter.HP_HZ = 50.0;
  Filter.Fs = 16000;
  Filter.Out_MicChannels = 1;
  Filter.In_MicChannels = 1;
   
  CRC->DR = 0xFFFFFFFF;
  CRC->CR |= CRC_CR_RESET;
	
  PDM_Filter_Init((PDMFilter_InitStruct *)&Filter);

  record_gpio_init();
  record_spi_init();
  record_nvic_init();	
}

void record_process(uint32_t write_position, uint32_t max_bytes)
{
  int32_t bytes_saved = 0, k;
  uint8_t start_callback_fired = 0;
  volatile uint16_t *curr;
	
  need_to_stop = 0;
	
  SPI_I2S_ITConfig(SPI2, SPI_I2S_IT_RXNE, ENABLE);
  I2S_Cmd(SPI2, ENABLE); 
	
  curr_buffer = out_buffer;
  to_process = out_buffer2;
	
  while (need_to_stop == 0 && (bytes_saved + 32) <= max_bytes){
    while (bufs_swapped == 0 && need_to_stop == 0);
    if (!start_callback_fired) 
      record_started_callback();
	  
    if (need_to_stop != 0)
      break;
    bufs_swapped = 0;
		
    curr = to_process;
    
    for (k = 0;k < 16;k++)
    {
      FLASH_ProgramWord(write_position, 65536*curr[k] + curr[k]);
      write_position += 4;
      bytes_saved += 4;		
    }
		
  }	
  
  SPI_I2S_ITConfig(SPI2, SPI_I2S_IT_RXNE, DISABLE);
  I2S_Cmd(SPI2, DISABLE);
	
  record_stopped_callback();
}

static void swap(volatile uint16_t **a, volatile uint16_t **b){
	volatile uint16_t *tmp = *a;
	*a = *b;
	*b = tmp;
}

void SPI2_IRQHandler(void)
{  
  u16 app;
  uint16_t arr[16];
  int k;
	
  /* Check if data are available in SPI Data register */
  if (SPI_GetITStatus(SPI2, SPI_I2S_IT_RXNE) != RESET)
  {
    app = SPI_I2S_ReceiveData(SPI2);
    internal_buffer[internal_buffer_size++] = HTONS(app);

    /* Check to prevent overflow condition */
    if (internal_buffer_size >= INTERNAL_BUFF_SIZE)
    {
      internal_buffer_size = 0;
      PDM_Filter_64_LSB((uint8_t *)internal_buffer, arr, 50 , (PDMFilter_InitStruct *)&Filter);
      
      for (k = 0; k < 16; k++)
        curr_buffer[k] = arr[k];
		
      swap(&curr_buffer, &to_process);
      bufs_swapped = 1;
    }
  }
}


static void record_nvic_init(void)
{
  NVIC_InitTypeDef NVIC_InitStructure;

  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_3); 
  /* Configure the SPI interrupt priority */
  NVIC_InitStructure.NVIC_IRQChannel = SPI2_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);
}

static void record_spi_init(void)
{
  I2S_InitTypeDef I2S_InitStructure;

  /* Enable the SPI clock */
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2,ENABLE);
  
  /* SPI configuration */
  SPI_I2S_DeInit(SPI2);
  I2S_InitStructure.I2S_AudioFreq = 32000;
  I2S_InitStructure.I2S_Standard = I2S_Standard_LSB;
  I2S_InitStructure.I2S_DataFormat = I2S_DataFormat_16b;
  I2S_InitStructure.I2S_CPOL = I2S_CPOL_High;
  I2S_InitStructure.I2S_Mode = I2S_Mode_MasterRx;
  I2S_InitStructure.I2S_MCLKOutput = I2S_MCLKOutput_Disable;
  /* Initialize the I2S peripheral with the structure above */
  I2S_Init(SPI2, &I2S_InitStructure);

  /* Enable the Rx buffer not empty interrupt */
  SPI_I2S_ITConfig(SPI2, SPI_I2S_IT_RXNE, ENABLE);	
}

/**
  * @brief  Initialize GPIO for wave recorder.
  * @param  None
  * @retval None
  */
static void record_gpio_init(void)
{  
  GPIO_InitTypeDef GPIO_InitStructure;

  /* Enable GPIO clocks */
  RCC_AHB1PeriphClockCmd(SPI_SCK_GPIO_CLK | SPI_MOSI_GPIO_CLK, ENABLE);

  /* Enable GPIO clocks */
  RCC_AHB1PeriphClockCmd(SPI_SCK_GPIO_CLK | SPI_MOSI_GPIO_CLK, ENABLE);

  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

  /* SPI SCK pin configuration */
  GPIO_InitStructure.GPIO_Pin = SPI_SCK_PIN;
  GPIO_Init(SPI_SCK_GPIO_PORT, &GPIO_InitStructure);
  
  /* Connect SPI pins to AF5 */  
  GPIO_PinAFConfig(SPI_SCK_GPIO_PORT, SPI_SCK_SOURCE, SPI_SCK_AF);
  
  /* SPI MOSI pin configuration */
  GPIO_InitStructure.GPIO_Pin =  SPI_MOSI_PIN;
  GPIO_Init(SPI_MOSI_GPIO_PORT, &GPIO_InitStructure);
  GPIO_PinAFConfig(SPI_MOSI_GPIO_PORT, SPI_MOSI_SOURCE, SPI_MOSI_AF);
}

void record_stop(void){
  need_to_stop = 1;
}

