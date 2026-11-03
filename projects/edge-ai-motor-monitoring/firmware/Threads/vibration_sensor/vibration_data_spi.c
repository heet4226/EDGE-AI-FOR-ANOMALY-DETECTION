/*
 * vibration_data_spi.c
 *
 *  Created on: Apr 15, 2026
 */

#include "vibration_data_spi.h"
#include "stdio.h"


#define SCALE_2G  (0.001f)	// ~1 mg/LSB after shift


static osSemaphoreId_t spi_dma_sem;


static float vib_rms_frame[VIB_FRAME_SAMPLES];

static uint16_t vib_index = 0;
static uint16_t vib_count = 0;
static uint16_t vib_step_count = 0;

static LIS3DH_FeatureData_t vib_feature_data = {0};

static void insertion_sort(float *arr, uint16_t n);


//Variable for feature values
//static float fx=0, fy=0, fz=0;
//static LIS3DH_Axis_t last_axis = {0};

static void LIS3DH_Write(uint8_t reg, uint8_t data)
{
    uint8_t tx[2] = {reg & 0x3F, data};

    CS_LOW();
    HAL_SPI_Transmit(&hspi5, tx, 2, 10);
    CS_HIGH();
}

static uint8_t LIS3DH_ReadMulti(uint8_t reg, uint8_t *buffer, uint8_t len)
{
	 uint8_t tx = reg | 0xC0;

	CS_LOW();

	if (HAL_SPI_Transmit_DMA(&hspi5, &tx, 1) != HAL_OK)
	{
		CS_HIGH();
		return 0;
	}
	osSemaphoreAcquire(spi_dma_sem, osWaitForever);

	if (HAL_SPI_Receive_DMA(&hspi5, buffer, len) != HAL_OK)
	{
		CS_HIGH();
		return 0;
	}
	osSemaphoreAcquire(spi_dma_sem, osWaitForever);

	CS_HIGH();

	return 1;
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI5)
    {
        osSemaphoreRelease(spi_dma_sem);
    }
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI5)
    {
        osSemaphoreRelease(spi_dma_sem);
    }
}


void LIS3DH_Init(void)
{
	spi_dma_sem = osSemaphoreNew(1, 0, NULL);

    CS_HIGH();
    HAL_Delay(10);

    LIS3DH_Write(0x20, 0x67); //97 = 1.344khz, 77= 400Hz, XYZ for 200 use 0x67
    LIS3DH_Write(0x23, 0x88); // HR + ±2g
}



uint8_t LIS3DH_Read_g(float *x, float *y, float *z)
{
	uint8_t status = 0;
    uint8_t buffer[6];
    int16_t rawX, rawY, rawZ;

    if (!LIS3DH_ReadMulti(0x27, &status, 1))
       {
           return 0;
       }

    /* ZYXDA bit: new X/Y/Z data available */
    if (!(status & 0x08))
       {
           return 0;
       }

    /* Read all XYZ in one burst */
    if (!LIS3DH_ReadMulti(0x28, buffer, 6))
		{
		   return 0;
		}

    rawX = (int16_t)(buffer[1] << 8 | buffer[0]) >> 4;
	rawY = (int16_t)(buffer[3] << 8 | buffer[2]) >> 4;
	rawZ = (int16_t)(buffer[5] << 8 | buffer[4]) >> 4;

    /* Reject only impossible zero-axis glitches */
	if (rawX < -2000 || rawX > 2000 ||
	    rawY < -2000 || rawY > 2000 ||
	    rawZ < -2000 || rawZ > 2000)
	{
	    return 0;
	}
    if ((rawY == 0 && rawX != 0 && rawZ != 0) ||
        (rawZ == 0 && rawX != 0 && rawY != 0))
       {
//    	printf("BAD: %02X %02X %02X %02X %02X %02X\r\n",
//    	       buffer[0], buffer[1],
//    	       buffer[2], buffer[3],
//    	       buffer[4], buffer[5]);
    	return 0;
       }

    *x = rawX * SCALE_2G;
    *y = rawY * SCALE_2G;
    *z = rawZ * SCALE_2G;

    return 1;
}


/*function for raw data to rms*/

LIS3DH_Axis_t LIS3DH_ReadRawSample(void)
{
	static LIS3DH_Axis_t last_axis = {0};
	static uint8_t have_valid = 0;
	LIS3DH_Axis_t axis = last_axis;

	if (!LIS3DH_Read_g(&axis.x, &axis.y, &axis.z))
	        return last_axis;                         /* read failed -> hold last good */

	axis.rms = sqrtf((axis.x * axis.x +
					  axis.y * axis.y +
					  axis.z * axis.z) / 3.0f);

	/* reject NaN / out-of-physical-range */
	if (!(axis.rms > 0.05f && axis.rms < 5.0f))
		return last_axis;

	/* reject impossible jumps (the spike-to-0 events). 0.5 g/sample is
	   far larger than any real change at 200 Hz on a steady motor.      */
	if (have_valid && fabsf(axis.rms - last_axis.rms) > 0.5f)
		return last_axis;

	last_axis  = axis;
	have_valid = 1;
	return axis;
}


void LIS3DH_UpdateFeatures(void)
{
    LIS3DH_Axis_t raw = LIS3DH_ReadRawSample();

    taskENTER_CRITICAL();
    vib_feature_data.raw = raw;
    vib_feature_data.features_ready = 0;
    taskEXIT_CRITICAL();

    /*
     * Store only physically possible RMS.
     * This is not hiding real motor faults; it only blocks impossible data.
     */
    if (raw.rms > 0.2f && raw.rms < 1.5f &&
        raw.x > -2.0f && raw.x < 2.0f &&
        raw.y > -2.0f && raw.y < 2.0f &&
        raw.z > -2.0f && raw.z < 2.0f)
    {
        vib_rms_frame[vib_index] = raw.rms;

        vib_index++;
        if (vib_index >= VIB_FRAME_SAMPLES)
        {
            vib_index = 0;
        }

        if (vib_count < VIB_FRAME_SAMPLES)
        {
            vib_count++;
        }

        if (vib_count >= VIB_FRAME_SAMPLES)
        {
            vib_step_count++;

            if (vib_step_count >= VIB_STEP_SAMPLES)
            {
                vib_step_count = 0;
                float frame_copy[VIB_FRAME_SAMPLES];

                taskENTER_CRITICAL();
                for (uint16_t i = 0; i < VIB_FRAME_SAMPLES; i++)
                {
                    frame_copy[i] = vib_rms_frame[i];
                }
                taskEXIT_CRITICAL();

                LIS3DH_Features_t new_features = LIS3DH_CalculateFeatures(frame_copy, VIB_FRAME_SAMPLES);

                if (!(new_features.min <= new_features.q1 &&
                      new_features.q1 <= new_features.median &&
                      new_features.median <= new_features.q3 &&
                      new_features.q3 <= new_features.max))
                {
                    return;
                }

                if (new_features.min < 0.2f || new_features.max > 1.5f)
                {
                    return;
                }
                LIS3DH_FeatureData_t new_data;

                taskENTER_CRITICAL();
                new_data = vib_feature_data;
                taskEXIT_CRITICAL();

                new_data.raw = raw;
                new_data.features = new_features;
                new_data.features_ready = 1;

                taskENTER_CRITICAL();
                vib_feature_data = new_data;
                taskEXIT_CRITICAL();
            }
        }
    }
}

LIS3DH_FeatureData_t LIS3DH_GetFeatureData(void)
{
	LIS3DH_FeatureData_t copy;

	taskENTER_CRITICAL();
	copy = vib_feature_data;
	taskEXIT_CRITICAL();

	return copy;
}

/*
 * Insertion sort in-place on float array of length n.
 *
 * Replaces bubble sort.
 * On nearly-sorted data (consecutive RMS readings change slowly)
 * insertion sort runs close to O(n). Worst case is still O(n²)
 * but average performance on sensor data is much better than
 * bubble sort's unconditional O(n²).
 *
 * For n=150 this is well within time budget of Data_Processing task.
 */
static void insertion_sort(float *arr, uint16_t n)
{
    for (uint16_t i = 1; i < n; i++)
    {
        float key = arr[i];
        int16_t j = (int16_t)i - 1;

        while (j >= 0 && arr[j] > key)
        {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

LIS3DH_Features_t LIS3DH_CalculateFeatures(float *buf, uint16_t n)
{
    LIS3DH_Features_t f = {0};

    float sum = 0.0f;
    float sum_sq = 0.0f;
    float temp[VIB_FRAME_SAMPLES];

    f.min = buf[0];
    f.max = buf[0];

    for (uint16_t i = 0; i < n; i++)
    {
        float v = buf[i];

        sum += v;
        sum_sq += v * v;

        if (v > f.max) f.max = v;
        if (v < f.min) f.min = v;

        temp[i] = v;
    }

    float mean = sum / n;
    f.rms = sqrtf(sum_sq / n);

    float var_sum = 0.0f;
    float third_sum = 0.0f;
    float fourth_sum = 0.0f;

    for (uint16_t i = 0; i < n; i++)
    {
        float d = buf[i] - mean;
        float d2 = d * d;

        var_sum += d2;
        third_sum += d2 * d;
        fourth_sum += d2 * d2;
    }

    f.variance = var_sum / n;

    if (f.variance > 1e-6f)
       {
           float std = sqrtf(f.variance);
           f.skewness = (third_sum / n) / (std * std * std);

       }
       else
       {
           f.skewness = 0.0f;

       }

    /* Sort temp copy for percentile calculation */
       insertion_sort(temp, n);

       f.q1     = temp[n / 4];
       f.median = (temp[n / 2 - 1] + temp[n / 2]) * 0.5f;
       f.q3     = temp[(3 * n) / 4];


    return f;
}


// feature vib data just a normal axis data with filter
//LIS3DH_Axis_t LIS3DH_data(void)
//{
//    LIS3DH_Axis_t axis = last_axis;
//    uint8_t status = 0;
//
//    uint8_t reg = 0x27 | 0x80;
//
//    CS_LOW();
//    HAL_SPI_Transmit(&hspi5, &reg, 1, HAL_MAX_DELAY);
//    HAL_SPI_Receive(&hspi5, &status, 1, HAL_MAX_DELAY);
//    CS_HIGH();
//
//    if(status & 0x08)
//    {
//        LIS3DH_Read_g(&axis.x, &axis.y, &axis.z);
//
//        /* smoothing */
//        fx = 0.85f*fx + 0.15f*axis.x;
//        fy = 0.85f*fy + 0.15f*axis.y;
//        fz = 0.85f*fz + 0.15f*axis.z;
//
//        axis.x = fx;
//        axis.y = fy;
//        axis.z = fz;
//
//        /* RMS from XYZ */
//	  axis.rms = sqrtf((axis.x * axis.x +
//						axis.y * axis.y +
//						axis.z * axis.z) / 3.0f);
//
//        last_axis = axis;
//    }
//
//    return axis;
//}
