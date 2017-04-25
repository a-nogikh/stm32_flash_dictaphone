#ifndef __RECORDER_H
#define __RECORDER_H

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx.h"
#include "stm32f4_discovery.h"
#include "stm32f4_discovery_audio_codec.h"
#include "stm32f4_discovery_lis302dl.h"
#include <stdio.h>
#include "stm32f4xx_it.h"

void record_init(void);
void record_process(uint32_t write_position, uint32_t max_bytes);
void record_stop(void);
void record_started_callback(void);
void record_stopped_callback(void);
 
#endif /* __RECORDER_H */


