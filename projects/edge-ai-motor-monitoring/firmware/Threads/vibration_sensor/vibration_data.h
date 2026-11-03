/*
 * vibration_data.h
 *
 *  Created on: Apr 9, 2026
 */

#ifndef VIBRATION_SENSOR_VIBRATION_DATA_H_
#define VIBRATION_SENSOR_VIBRATION_DATA_H_

#include "main.h"
#include "stdio.h"
#include "cmsis_os2.h"


extern I2C_HandleTypeDef hi2c1;
extern osMutexId_t I2C_mutexHandle;

typedef struct
{
	float x;
	float y;
	float z;
}LIS3DH_Axis_t;

void LIS3DH_Init(void);
void LIS3DH_Read_g(float *xg, float *yg, float *zg);
LIS3DH_Axis_t LIS3DH_data(void);

#endif /* VIBRATION_SENSOR_VIBRATION_DATA_H_ */
