/*
 * vibration_data.c
 *
 *  Created on: Apr 9, 2026
 */

#include "vibration_data.h"
#include "app_freertos.h"

#define SCALE_2G  (0.001f)   // ~1 mg/LSB after shift
#define LIS3DH_ADDR  (0x18 << 1)   // or 0x19<<1 depending on SDO




void LIS3DH_Init()
{
    uint8_t data;

    // CTRL_REG1 (0x20): 100Hz, enable X/Y/Z
//    data = 0x57;   // 0b01010111	100hz
    data = 0x77;   // 0b01010111	400hz
    HAL_I2C_Mem_Write(&hi2c1, LIS3DH_ADDR, 0x20, 1, &data, 1, 100);

    // CTRL_REG4 (0x23): ±2g, high resolution
    data = 0x88;   // BDU=1 + HR=1 + ±2g
    HAL_I2C_Mem_Write(&hi2c1, LIS3DH_ADDR, 0x23, 1, &data, 1, 100);

//	 uint8_t data;
//
//	    // 400 Hz, XYZ enable
//	    data = 0x77;
//	    HAL_I2C_Mem_Write(&hi2c1, LIS3DH_ADDR, 0x20, 1, &data, 1, 100);
//
//	    // High-pass filter
//	    data = 0x01;
//	    HAL_I2C_Mem_Write(&hi2c1, LIS3DH_ADDR, 0x21, 1, &data, 1, 100);
//
//	    // ±2g, high resolution
//	    data = 0x88;
//	    HAL_I2C_Mem_Write(&hi2c1, LIS3DH_ADDR, 0x23, 1, &data, 1, 100);
}

void LIS3DH_Read_g(float *xg, float *yg, float *zg)
{
    int16_t x, y, z;
    uint8_t buffer[6];

//    osMutexAcquire(I2C_mutexHandle, osWaitForever);
    HAL_I2C_Mem_Read(&hi2c1, LIS3DH_ADDR, 0x28 | 0x80, 1, buffer, 6, 100);
//    osMutexRelease(I2C_mutexHandle);

    x = (int16_t)(buffer[1] << 8 | buffer[0]) >> 4;
    y = (int16_t)(buffer[3] << 8 | buffer[2]) >> 4;
    z = (int16_t)(buffer[5] << 8 | buffer[4]) >> 4;

    *xg = x * SCALE_2G;
    *yg = y * SCALE_2G;
    *zg = z * SCALE_2G;
}

LIS3DH_Axis_t LIS3DH_data(void){

	LIS3DH_Axis_t axis = {0};
	    uint8_t presence, status;

	    osMutexAcquire(I2C_mutexHandle, osWaitForever);

	    if (HAL_I2C_Mem_Read(&hi2c1, LIS3DH_ADDR, 0x0F, 1, &presence, 1, 100) != HAL_OK)
	    {
	        osMutexRelease(I2C_mutexHandle);
	        printf("I2C Error\r\n");
	        return axis;
	    }

	    if (presence != 0x33)
	    {
	        osMutexRelease(I2C_mutexHandle);
	        printf("Sensor NOT detected\r\n");
	        return axis;
	    }

	    if (HAL_I2C_Mem_Read(&hi2c1, LIS3DH_ADDR, 0x27, 1, &status, 1, 100) != HAL_OK)
	    {
	        osMutexRelease(I2C_mutexHandle);
	        printf("Status read error\r\n");
	        return axis;
	    }


	        LIS3DH_Read_g(&axis.x, &axis.y, &axis.z);


	    osMutexRelease(I2C_mutexHandle);

	    return axis;

}
