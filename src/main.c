#include "main.h"
#include "recorder.h"

#define FLASH_AUDIO_POSISION 0x080C0000
#define FLASH_MAX_BYTES 120000*2

/* Program states */
#define STATE_NOTHING 0
#define STATE_PLAYING 1
#define STATE_RECORDING 2
#define STATE_RECORDING_STOPPED 3


volatile uint8_t state = STATE_NOTHING, blinking_now = 0;
volatile uint8_t blink_passed_ms = 0;
volatile uint32_t since_button_pressed = 0, since_button_released = 0;
volatile uint8_t press_signaled = 0, release_signaled = 1;
volatile uint64_t last_button_pressed = 0;
volatile uint64_t time_elapsed_ms = 0;


static void erase_flash(void);
static void setup_timer(void);
static void hande_button_change(uint8_t new_state);


int main(void)
{ 
  /* Init GPIO */
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,ENABLE);
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
	
  /* Initialize LEDS */
  STM_EVAL_LEDInit(LED4);
  STM_EVAL_LEDInit(LED3);
  STM_EVAL_LEDInit(LED5);
	
  STM_EVAL_PBInit(BUTTON_USER, BUTTON_MODE_GPIO);
	
  RCC->AHB1ENR |= RCC_AHB1ENR_CRCEN;

  setup_timer();
  record_init();
	  
  while(1)
  {
    if (state == STATE_NOTHING)
	{
    	STM_EVAL_LEDOff(LED3);
		blinking_now = 0;
		
		while (state == STATE_NOTHING);
	}
    else if (state == STATE_RECORDING)
	{
		STM_EVAL_LEDToggle(LED5);
		
		erase_flash();
		record_process(FLASH_AUDIO_POSISION, FLASH_MAX_BYTES);
		STM_EVAL_LEDOff(LED5);
		
		// Wait until button is released
		while (state == STATE_RECORDING_STOPPED); 
	}
    else if(state == STATE_PLAYING)
	{
		blinking_now = 0;
		STM_EVAL_LEDOn(LED3);
		
		EVAL_AUDIO_SetAudioInterface(AUDIO_INTERFACE_I2S);
 		EVAL_AUDIO_Init(OUTPUT_DEVICE_HEADPHONE, 60, 16000);
		EVAL_AUDIO_Play(( uint16_t*)FLASH_AUDIO_POSISION, FLASH_MAX_BYTES);
		
		while (state == STATE_PLAYING);
		
		EVAL_AUDIO_DeInit();
	}
  }
}

static void erase_flash(void)
{
	FLASH_Unlock();
	FLASH_EraseSector(FLASH_Sector_10, VoltageRange_3);
	FLASH_EraseSector(FLASH_Sector_11, VoltageRange_3);
}

static void setup_timer(void)
{
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM6,ENABLE);
  TIM6->PSC = 42000 - 1; 
  TIM6->ARR = 2; // 1ms
  TIM6->DIER |= TIM_DIER_UIE; 
  TIM6->CR1 |= TIM_CR1_CEN; 
  NVIC_EnableIRQ(TIM6_DAC_IRQn); 
 
}

static void hande_button_change(uint8_t new_state)
{
  uint64_t ms_since_press;
	
  if (new_state == 0)
  {
	// time elapsed since button pressed  
    ms_since_press = (time_elapsed_ms - last_button_pressed);	
	    
    if (state == STATE_NOTHING && ms_since_press < 500){
      // Short button press => start playing	
      state = STATE_PLAYING;
    }
    else if (state == STATE_PLAYING || state == STATE_RECORDING_STOPPED)
    {
      state = STATE_NOTHING;
    }
    else if (state == STATE_RECORDING)
	{
      // Button released while recording => stop recording
      record_stop();
      state = STATE_NOTHING;STM_EVAL_LEDOn(LED4);
    }
  }
}


void TIM6_DAC_IRQHandler(void)
{
  uint64_t ms_since_press;
  
  TIM6->SR &= ~TIM_SR_UIF; 
  time_elapsed_ms += 1;
	
  if (GPIOA->IDR & GPIO_Pin_0) // button pressed
  {
    since_button_pressed += 1;
	since_button_released = 0;
  }
  else
  {
  	since_button_released += 1;
    since_button_pressed = 0;
  }
	
  if (since_button_pressed > 50 && press_signaled == 0 && release_signaled == 1)
  {
    press_signaled = 1;
    release_signaled = 0;
    hande_button_change(1);
    last_button_pressed = time_elapsed_ms;
  }
  else if (since_button_released > 50 && release_signaled == 0 && press_signaled == 1)
  {
    press_signaled = 0;
    release_signaled = 1;
    hande_button_change(0);
  }
	
  /* User holding button longer than 500 ms => start recording */	
  ms_since_press = (time_elapsed_ms - last_button_pressed);
  if (state == STATE_NOTHING && press_signaled == 1 && release_signaled == 0 && ms_since_press > 500)
  {
    state = STATE_RECORDING;  
  }
	
	
  if (blinking_now == 1){
    blink_passed_ms += 1;
    if (blink_passed_ms > 100){
      blink_passed_ms = 0;
      STM_EVAL_LEDToggle(LED3);
    }
  }
}


void record_started_callback(void){
  blinking_now = 1;
}

/* Callback from recorder.c */
void record_stopped_callback(void)
{
  if (state == STATE_RECORDING)
    state = STATE_RECORDING_STOPPED;
}


/* Callback from EVAL_AUDIO */
void EVAL_AUDIO_TransferComplete_CallBack(uint32_t *pBuffer, uint32_t Size)
{
	state = STATE_NOTHING;
}

