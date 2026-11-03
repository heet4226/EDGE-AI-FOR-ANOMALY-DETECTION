/*
 * Temp_Sensor.h
 *
 *  Created on: Mar 26, 2026
 */

#ifndef TEMP_SENSOR_H_
#define TEMP_SENSOR_H_

#include "main.h"

#define DS18B20_PORT GPIOG
#define DS18B20_PIN  GPIO_PIN_15

#define DS18B20_LOW()  HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_RESET)
#define DS18B20_RELEASE() HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_SET)
#define DS18B20_READ() HAL_GPIO_ReadPin(DS18B20_PORT, DS18B20_PIN)




void DWT_Init(void);
void delay_us(uint32_t us);
uint8_t DS18B20_Reset(void);
void DS18B20_WriteBit(uint8_t bit);
uint8_t DS18B20_ReadBit(void);
void DS18B20_WriteByte(uint8_t data);
uint8_t DS18B20_ReadByte(void);

void DS18B20_StartConversion(void);
float DS18B20_ReadTemp(void);

#endif /* TEMP_SENSOR_H_ */
