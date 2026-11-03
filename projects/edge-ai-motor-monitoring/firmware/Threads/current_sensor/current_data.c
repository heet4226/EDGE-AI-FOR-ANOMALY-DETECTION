/*
 * current_data.c
 *
 *  Created on: Apr 8, 2026
 */


#include "current_data.h"
#include "math.h"

#define INA260_ADDR 		  (0x40 << 1)
#define INA260_REG_CURRENT 	   0x01
#define INA260_REG_CONFIG      0x00
#define INA260_CONFIG_FAST     0x6007
#define INA260_REG_BUSVOLTAGE  0x02

static osSemaphoreId_t i2c_dma_sem;

static float current_frame[CURRENT_FRAME_SAMPLES];
static float voltage_frame[CURRENT_FRAME_SAMPLES];

static uint8_t frame_index = 0;
static uint16_t frame_count = 0;
static uint16_t step_count = 0;

static INA260_FeatureData_t feature_data = {0};

//static float filtered_current = 0;
////static float filtered_voltage = 0;
//static float alpha = 0.02;


HAL_StatusTypeDef INA260_ReadReg(uint8_t reg, uint16_t *value)
{
	uint8_t data[2];

	osMutexAcquire(I2C_mutexHandle, osWaitForever);

	if (HAL_I2C_Mem_Read_DMA(&hi2c1, INA260_ADDR, reg, I2C_MEMADD_SIZE_8BIT, data, 2) != HAL_OK)
	{
		osMutexRelease(I2C_mutexHandle);
		return HAL_ERROR;
	}

	osSemaphoreAcquire(i2c_dma_sem, osWaitForever);   /* waits for DMA complete */

	osMutexRelease(I2C_mutexHandle);

	*value = ((uint16_t)data[0] << 8) | data[1];
	return HAL_OK;
}


void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C1)
    {
        osSemaphoreRelease(i2c_dma_sem);
    }
}

HAL_StatusTypeDef INA260_WriteReg(uint8_t reg, uint16_t value)
{
    uint8_t data[2];

    data[0] = (value >> 8) & 0xFF;
    data[1] = value & 0xFF;

    osMutexAcquire(I2C_mutexHandle, osWaitForever);

    HAL_StatusTypeDef status = HAL_I2C_Mem_Write(&hi2c1,
                                                  INA260_ADDR,
                                                  reg,
                                                  I2C_MEMADD_SIZE_8BIT,
                                                  data,
                                                  2,
                                                  100);

    osMutexRelease(I2C_mutexHandle);

    return status;
}


void INA260_CheckDevice(void)
{
	 if (HAL_I2C_IsDeviceReady(&hi2c1, INA260_ADDR, 3, 100) != HAL_OK)
	 {
		printf("INA260 NOT FOUND\r\n");
		while (1);
	 }
}


void INA260_InitFast(void)
{
	i2c_dma_sem = osSemaphoreNew(1, 0, NULL);

	    if (INA260_WriteReg(INA260_REG_CONFIG, INA260_CONFIG_FAST) != HAL_OK)
	    {
	        printf("INA260 FAST MODE FAILED\r\n");
	    }
}


INA260_RawData INA260_ReadRaw(void)
{
    INA260_RawData data = {0};

    uint16_t raw_current = 0;
    uint16_t raw_voltage = 0;

    if (INA260_ReadReg(INA260_REG_CURRENT, &raw_current) == HAL_OK &&
        INA260_ReadReg(INA260_REG_BUSVOLTAGE, &raw_voltage) == HAL_OK)
    {
        int16_t signed_current = (int16_t)raw_current;

        data.current = signed_current * 1.25f / 1000.0f;  // A
        data.voltage = raw_voltage * 1.25f / 1000.0f;     // V
    }

    return data;
}

/*calculate rms*/

float INA260_CalculateRMS(float *buf, uint16_t n)
{
    float sum = 0.0f;

    for(uint16_t i = 0; i < n; i++)
    {
        sum += buf[i] * buf[i];
    }

    return sqrtf(sum / n);
}

/* NEW: variance (spread) + crest factor (peak/RMS) over the same frame.
 * Cheap to add: reuses the current_frame buffer already collected for RMS,
 * no extra sensor reads. Targets electrical-fault confusion seen in training. */
void INA260_CalculateExtra(float *buf, uint16_t n, float rms,
                            float *variance_out, float *crest_out)
{
    float mean = 0.0f;
    float var_sum = 0.0f;
    float peak = 0.0f;

    for (uint16_t i = 0; i < n; i++)
    {
        mean += buf[i];
    }
    mean /= n;

    for (uint16_t i = 0; i < n; i++)
    {
        float d = buf[i] - mean;
        var_sum += d * d;

        float mag = fabsf(buf[i]);
        if (mag > peak) peak = mag;
    }

    *variance_out = var_sum / n;
    *crest_out = (rms > 1e-6f) ? (peak / rms) : 0.0f;
}


//void INA260_UpdateFeatures(void)
//{
//    INA260_RawData raw = INA260_ReadRaw();
//
//    feature_data.raw_current = raw.current;
//    feature_data.raw_voltage = raw.voltage;
//
//    feature_data.filtered_current = 0.98f * feature_data.filtered_current + 0.02f * raw.current;
//    feature_data.filtered_voltage = 0.98f * feature_data.filtered_voltage + 0.02f * raw.voltage;
//
//    current_frame[frame_index] = raw.current;
//    voltage_frame[frame_index] = raw.voltage;
//
//    frame_index++;
//
//    if (frame_index >= CURRENT_FRAME_SAMPLES)
//    {
//        frame_index = 0;
//    }
//
//    if (frame_count < CURRENT_FRAME_SAMPLES)
//    {
//        frame_count++;
//    }
//
//    feature_data.rms_ready = 0;
//
//    if (frame_count >= CURRENT_FRAME_SAMPLES)
//    {
//        step_count++;
//
//        if (step_count >= CURRENT_STEP_SAMPLES)
//        {
//            step_count = 0;
//
//            feature_data.rms_current = INA260_CalculateRMS(current_frame, CURRENT_FRAME_SAMPLES);
//            feature_data.rms_voltage = INA260_CalculateRMS(voltage_frame, CURRENT_FRAME_SAMPLES);
//
//            /* NEW: 2 extra current-domain features, same frame, same instant */
//            INA260_CalculateExtra(current_frame, CURRENT_FRAME_SAMPLES,
//                                   feature_data.rms_current,
//                                   &feature_data.current_variance,
//                                   &feature_data.current_crest_factor);
//
//            feature_data.rms_ready = 1;
//        }
//    }
//}




void INA260_UpdateFeatures(void)
{
    INA260_RawData raw = INA260_ReadRaw();

    /* Work on a private copy, publish in one step at the end */
    INA260_FeatureData_t work;

    taskENTER_CRITICAL();
    work = feature_data;
    taskEXIT_CRITICAL();

    work.raw_current = raw.current;
    work.raw_voltage = raw.voltage;

    work.filtered_current = 0.98f * work.filtered_current + 0.02f * raw.current;
    work.filtered_voltage = 0.98f * work.filtered_voltage + 0.02f * raw.voltage;

    current_frame[frame_index] = raw.current;
    voltage_frame[frame_index] = raw.voltage;

    frame_index++;
    if (frame_index >= CURRENT_FRAME_SAMPLES)
        frame_index = 0;

    if (frame_count < CURRENT_FRAME_SAMPLES)
        frame_count++;

    work.rms_ready = 0;

    if (frame_count >= CURRENT_FRAME_SAMPLES)
    {
        step_count++;

        if (step_count >= CURRENT_STEP_SAMPLES)
        {
            step_count = 0;

            work.rms_current = INA260_CalculateRMS(current_frame, CURRENT_FRAME_SAMPLES);
            work.rms_voltage = INA260_CalculateRMS(voltage_frame, CURRENT_FRAME_SAMPLES);

            INA260_CalculateExtra(current_frame, CURRENT_FRAME_SAMPLES,
                                  work.rms_current,
                                  &work.current_variance,
                                  &work.current_crest_factor);

            work.rms_ready = 1;
        }
    }

    taskENTER_CRITICAL();
    feature_data = work;
    taskEXIT_CRITICAL();
}



INA260_FeatureData_t INA260_GetFeatureData(void)
{
	INA260_FeatureData_t copy;

	taskENTER_CRITICAL();
	copy = feature_data;
	taskEXIT_CRITICAL();

	return copy;
}


//with low-[ass filter(for without rms project)

//INA260_AvgData INA260_Measurecurrent(void)
//{
//
//    INA260_AvgData data;
//    uint16_t raw_current = 0;
//    uint16_t raw_voltage = 0;
//    uint8_t data_buf[2];
//
//    osMutexAcquire(I2C_mutexHandle, osWaitForever);
//
//    if (HAL_I2C_Mem_Read(&hi2c1, INA260_ADDR, INA260_REG_CURRENT, 1, data_buf, 2, 10) == HAL_OK)
//    {
//        raw_current = (data_buf[0] << 8) | data_buf[1];
//    }
//
//    if (HAL_I2C_Mem_Read(& hi2c1, INA260_ADDR, INA260_REG_BUSVOLTAGE, 1, data_buf, 2, 10) == HAL_OK)
//    {
//        raw_voltage = (data_buf[0] << 8) | data_buf[1];
//    }
//
//    osMutexRelease(I2C_mutexHandle);
//
//    int16_t signed_current = (int16_t)raw_current;
//
//    float current = signed_current * 1.25f / 1000.0f;
//    float voltage = raw_voltage * 1.25f / 1000.0f;
//
//    filtered_current = 0.98f * filtered_current + alpha * current;
////    filtered_voltage = 0.98f * filtered_voltage + alpha * voltage;
//
//
//    data.avg_current = filtered_current;
////    data.avg_voltage = filtered_voltage;
//    data.avg_voltage = voltage;
//
//    return data;
//}
