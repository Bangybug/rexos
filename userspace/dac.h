/*
    RExOS - embedded RTOS
    Copyright (c) 2011-2015, Alexey Kramarenko
    All rights reserved.
*/

#ifndef DAC_H
#define DAC_H

#include "sys.h"
#include "cc_macro.h"
#include "sys_config.h"
#include "file.h"

typedef enum {
    DAC_SET = HAL_IPC(HAL_DAC),
    DAC_WAVE,
    DAC_IPC_MAX
} DAC_IPCS;

//it is not required to support all modes by driver
typedef enum {
    DAC_MODE_LEVEL = 0,
    DAC_MODE_WAVE,
    DAC_MODE_NOISE,
    DAC_MODE_STREAM
} DAC_MODE;

typedef enum {
    DAC_WAVE_SINE = 0,
    DAC_WAVE_TRIANGLE,
    DAC_WAVE_SQUARE
} DAC_WAVE_TYPE;

__STATIC_INLINE HANDLE dac_open(int channel, DAC_MODE mode, unsigned int samplerate)
{
    return fopen_p(object_get(SYS_OBJ_DAC), HAL_HANDLE(HAL_DAC, channel), mode, (void*)samplerate);
}

__STATIC_INLINE void dac_set(int channel, SAMPLE value)
{
    ack(object_get(SYS_OBJ_DAC), DAC_SET, HAL_HANDLE(HAL_DAC, channel), value, 0);
}

__STATIC_INLINE void dac_wave(int channel, DAC_WAVE_TYPE wave_type, SAMPLE amplitude)
{
    ack(object_get(SYS_OBJ_DAC), DAC_WAVE, HAL_HANDLE(HAL_DAC, channel), wave_type, amplitude);
}

#endif // ADC_H