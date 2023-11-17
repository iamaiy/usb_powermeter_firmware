/**
  ******************************************************************************
  * @file    my_analog.h
  * @author  WI6LABS
  * @version V1.0.0
  * @date    01-August-2016
  * @brief   Header for analog module
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2016 STMicroelectronics</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MY_ANALOG_H
#define __MY_ANALOG_H

/* Includes ------------------------------------------------------------------*/
#include "stm32_def.h"
#include "PeripheralPins.h"
#include "HardwareTimer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Exported functions ------------------------------------------------------- */
#if defined(HAL_ADC_MODULE_ENABLED) && !defined(HAL_ADC_MODULE_ONLY)
void my_adc_setup(PinName pin, uint32_t resolution);
void my_adc_start(void);
void my_adc_clear(void);
void my_adc_callback(void);
uint8_t my_adc_fresh(void);
uint16_t my_adc_read(void);
extern "C" void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef * );

#endif

#ifdef __cplusplus
}
#endif

#endif /* __MY_ANALOG_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
