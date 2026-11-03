/*
 * current_data.h
 *
 *  Created on: Apr 8, 2026
 */

#ifndef CURRENT_SENSOR_CURRENT_DATA_H_
#define CURRENT_SENSOR_CURRENT_DATA_H_

#include "main.h"
#include "stdio.h"
#include "string.h"
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "task.h"

#define CURRENT_FRAME_SAMPLES   40
#define CURRENT_STEP_SAMPLES    3

extern I2C_HandleTypeDef hi2c1;
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim12;

extern osMutexId_t I2C_mutexHandle;

/* for rms*/
typedef struct
{
    float current;
    float voltage;
} INA260_RawData;

/* This structure variable are used in to print main current in webpage and all*/
typedef struct {
	float avg_current;
	float avg_voltage;
}INA260_AvgData;

/* New clean feature structure for Ai input */
typedef struct
{
    float raw_current;
    float raw_voltage;

    float filtered_current;
    float filtered_voltage;

    float rms_current;
    float rms_voltage;

    float current_variance;      /* NEW: spread of current over the frame */
    float current_crest_factor;	 /* NEW: peak/RMS, flags spiky electrical faults */

    uint8_t rms_ready;
} INA260_FeatureData_t;


/* Low-level functions */
HAL_StatusTypeDef INA260_ReadReg(uint8_t reg, uint16_t *value);
void INA260_CheckDevice(void);
void INA260_InitFast(void);
INA260_RawData INA260_ReadRaw(void);

/* Feature functions */
void INA260_UpdateFeatures(void);
INA260_FeatureData_t INA260_GetFeatureData(void);

/* Math helper */
float INA260_CalculateRMS(float *buf, uint16_t n);

/* feature extraction for the 14-feature vector */
void INA260_CalculateExtra(float *buf, uint16_t n, float rms,
                            float *variance_out, float *crest_out);
#endif /* CURRENT_SENSOR_CURRENT_DATA_H_ */
