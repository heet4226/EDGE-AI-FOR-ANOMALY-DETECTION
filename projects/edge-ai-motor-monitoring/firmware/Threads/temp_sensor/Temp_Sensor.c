/*
 * Temp_Sensor.c
 *
 *  Created on: Mar 26, 2026
 */


#include "Temp_Sensor.h"
#include "main.h"
#include "stdio.h"
#include "string.h"
#include "cmsis_os2.h"
#include <math.h>

//fix
#include "FreeRTOS.h"
#include "task.h"

uint8_t presence;

uint8_t temp_l, temp_h;

void DWT_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000);

    while ((DWT->CYCCNT - start) < ticks);
}



//uint8_t DS18B20_Reset(void)
//{
//    uint8_t presence;
//
//    __disable_irq();
//
//    DS18B20_RELEASE();
//    delay_us(5);
//
//    DS18B20_LOW();
//    delay_us(480);
//
//    DS18B20_RELEASE();
//    delay_us(70);
//
//    presence = DS18B20_READ();
//
//    delay_us(410);
//
//    __enable_irq();
//
//    return (presence == 0);
//}

uint8_t DS18B20_Reset(void)
{
    uint8_t presence;

    DS18B20_LOW();
    osDelay(2);                 /* >= 1 ms low (spec: >= 480 us), CPU free */

    __disable_irq();            /* ~70 us: release -> sample presence */
    DS18B20_RELEASE();
    delay_us(70);
    presence = DS18B20_READ();
    __enable_irq();

    osDelay(2);                 /* >= 1 ms recovery (spec: >= 410 us), CPU free */

    return (presence == 0);
}



//void DS18B20_WriteBit(uint8_t bit)
//{
//    __disable_irq();
//
//    DS18B20_LOW();
//
//    if (bit)
//    {
//        delay_us(5);        // very short low
//        DS18B20_RELEASE();
//        delay_us(60);
//    }
//    else
//    {
//        delay_us(60);       // long low
//        DS18B20_RELEASE();
//        delay_us(5);
//    }
//
//    __enable_irq();
//}

void DS18B20_WriteBit(uint8_t bit)
{
    vTaskSuspendAll();          /* no task switch inside the slot, ISRs still run */

    if (bit)
    {
        __disable_irq();        /* ~5 us: low pulse of a '1' must stay < 15 us */
        DS18B20_LOW();
        delay_us(5);
        DS18B20_RELEASE();
        __enable_irq();
        delay_us(60);
    }
    else
    {
        DS18B20_LOW();          /* '0' low pulse may be 60..120 us */
        delay_us(60);
        DS18B20_RELEASE();
        delay_us(5);
    }

    (void)xTaskResumeAll();
}

//uint8_t DS18B20_ReadBit(void)
//{
//    uint8_t bit;
//
//    __disable_irq();
//
//    DS18B20_LOW();
//    delay_us(3);            // very short
//
//    DS18B20_RELEASE();      // release line
//    delay_us(15);           // wait longer (safe window)
//
//    bit = DS18B20_READ();
//
//    __enable_irq();
//
//    delay_us(50);           // complete slot
//
//    return bit;
//}

uint8_t DS18B20_ReadBit(void)
{
    uint8_t bit;

    vTaskSuspendAll();

    __disable_irq();            /* ~18 us: start slot and sample the bit */
    DS18B20_LOW();
    delay_us(3);
    DS18B20_RELEASE();
    delay_us(15);
    bit = DS18B20_READ();
    __enable_irq();

    delay_us(50);               /* complete slot, interrupts enabled */

    (void)xTaskResumeAll();

    return bit;
}

void DS18B20_WriteByte(uint8_t data)
{
    for (int i = 0; i < 8; i++)
    {
        DS18B20_WriteBit(data & 0x01);
        data >>= 1;
    }
}

uint8_t DS18B20_ReadByte(void)
{
    uint8_t value = 0;

    for (int i = 0; i < 8; i++)
    {
        value >>= 1;
        if (DS18B20_ReadBit())
        {
            value |= 0x80;
        }
    }

    return value;
}


float DS18B20_ReadTemp(void)
{

	static float last_temp = 0.0f;
	static uint8_t has_valid_temp = 0;

    int16_t temp_init;
    float temperature;


    DS18B20_Reset();
    DS18B20_WriteByte(0xCC); // Skip ROM
    DS18B20_WriteByte(0x44); // Convert T

    osDelay(750); // conversion time
//    while (!DS18B20_ReadBit());
    if (!DS18B20_Reset())
    {
        printf("No sensor\r\n");
        return -1000;
    }

    DS18B20_WriteByte(0xCC);
    DS18B20_WriteByte(0xBE); // Read Scratchpad

    temp_l = DS18B20_ReadByte();
    temp_h = DS18B20_ReadByte();

    temp_init = (temp_h << 8) | temp_l;

    temperature = (float)temp_init/16.0f;

    if(temperature < 10.0f || temperature > 50.0f)
    {
    	return has_valid_temp ? last_temp : -1000.0f;
    }

    if (has_valid_temp && fabsf(temperature - last_temp) > 2.0f)
	{
		return last_temp;
	}

    last_temp = temperature;
    has_valid_temp = 1;

    return temperature;
}
