/*
 * vibration_data_spi.h
 *
 *  Created on: Apr 15, 2026
 */

#ifndef VIBRATION_SENSOR_VIBRATION_DATA_SPI_H_
#define VIBRATION_SENSOR_VIBRATION_DATA_SPI_H_

#include "main.h"
#include "stdio.h"
#include "cmsis_os2.h"
#include "math.h"
#include "FreeRTOS.h"
#include "task.h"

#define VIB_FRAME_SAMPLES 150
#define VIB_STEP_SAMPLES  3

extern SPI_HandleTypeDef hspi5;

typedef struct
{
	float x;
	float y;
	float z;
	float rms;
}LIS3DH_Axis_t;

typedef struct
{
    float rms;
    float min;
    float q1;
    float median;
    float q3;
    float max;
    float variance;
    float kurtosis;
    float skewness;
} LIS3DH_Features_t;

typedef struct
{
    LIS3DH_Axis_t raw;
    LIS3DH_Features_t features;
    uint8_t features_ready;
} LIS3DH_FeatureData_t;


#define LIS3DH_CS_PORT CHIP_SELECT_GPIO_Port
#define LIS3DH_CS_PIN  CHIP_SELECT_Pin

#define CS_LOW()  HAL_GPIO_WritePin(LIS3DH_CS_PORT, LIS3DH_CS_PIN, GPIO_PIN_RESET)
#define CS_HIGH() HAL_GPIO_WritePin(LIS3DH_CS_PORT, LIS3DH_CS_PIN, GPIO_PIN_SET)


void LIS3DH_Init(void);

uint8_t LIS3DH_Read_g(float *x, float *y, float *z);
LIS3DH_Axis_t LIS3DH_ReadRawSample(void);
//LIS3DH_Axis_t LIS3DH_data(void);

void LIS3DH_UpdateFeatures(void);
LIS3DH_FeatureData_t LIS3DH_GetFeatureData(void);

LIS3DH_Features_t LIS3DH_CalculateFeatures(float *buf, uint16_t n);

#endif /* VIBRATION_SENSOR_VIBRATION_DATA_SPI_H_ */
