/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : app_freertos.c
  * Description        : FreeRTOS applicative file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "app_freertos.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "string.h"
#include "main.h"
#include "math.h"
#include "stdio.h"
#include "Temp_Sensor.h"
#include "current_data.h"
#include "motor.h"
#include "app_x-cube-ai.h"
#include "vibration_data_spi.h"
#include "motor_model_params.h"
#include "rti.h"
#include <stdbool.h>
// includes for Ethernaet thread
#include "../../Threads/Ethernet/lwip/app/lwip.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
extern RTC_HandleTypeDef hrtc;

extern UART_HandleTypeDef huart1;          /* <-- use your print UART handle */
osMutexId_t UART_mutexHandle;
const osMutexAttr_t UART_mutex_attributes = { .name = "UART_mutex" };
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

extern volatile uint8_t ramp_tag;
extern volatile uint8_t motor_direction;

// for AI buffer filling
#define NUM_FEATURES 14


float temp;
INA260_AvgData ina_data;
uint16_t speed;

LIS3DH_Axis_t vib_data;

extern struct netif gnetif;

float temp_ai;
uint8_t similarity = 0;


float latest_current = 0.0f;
float latest_voltage = 0.0f;

volatile float current_rms = 0.0f;
volatile float voltage_rms = 0.0f;

/*for vibration*/
//volatile uint8_t frame_sync_counter = 0;
volatile uint8_t cur_frame_ready = 0;   /* set by Current_sensor task   */
volatile uint8_t vib_frame_ready = 0;   /* set by Vibration_sensor task */

/* Feature output */
float temp_slope_10s = 0.0f;

/*data processing variable*/
float temp_ai_snapshot;
float temp_slope_snapshot;
uint16_t speed_snapshot;
float current_rms_snapshot;
float voltage_rms_snapshot;
float current_variance_snapshot;
float current_crest_snapshot;
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
float features[NUM_FEATURES];

#define AI_TIME_STEPS        MOTOR_MODEL_WINDOW_LEN
#define AI_FEATURES          MOTOR_MODEL_N_FEATURES
#define AI_STEP_SIZE         10

/* Raw indices within the 14-wide feature vector, matching training's FEATURE_COLUMNS order */
#define VIB_RMS_INDEX           6
#define VIB_VARIANCE_INDEX      12
#define CURRENT_VARIANCE_INDEX  4
#define PWM_FEATURE_INDEX       1

static float ai_live_window[AI_TIME_STEPS][AI_FEATURES];   /* raw-only sliding buffer */
float ai_ready_window[AI_TIME_STEPS][AI_FEATURES];              /* model-ready, 17-wide, consumed by app_x-cube-ai.c */

static uint8_t ai_window_count = 0;
static uint8_t ai_step_count   = 0;
static uint32_t ai_feature_sequence = 0;
//static uint32_t ai_window_sequence = 0;
//_Static_assert(AI_TIME_STEPS <= 255, "Window counter needs a wider type.");


RTI_State g_rti;
volatile double g_rti_latest_risk = 1.0;
volatile float mt_remaining_inspection_h = 0.0f;
volatile uint8_t mt_inspection_due = 0U;
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .priority = (osPriority_t) osPriorityBelowNormal,
  .stack_size = 4096 * 4
};
/* Definitions for Temp_sensor */
osThreadId_t Temp_sensorHandle;
const osThreadAttr_t Temp_sensor_attributes = {
  .name = "Temp_sensor",
  .priority = (osPriority_t) osPriorityAboveNormal,
  .stack_size = 2048 * 4
};
/* Definitions for Motor_driver */
osThreadId_t Motor_driverHandle;
const osThreadAttr_t Motor_driver_attributes = {
  .name = "Motor_driver",
  .priority = (osPriority_t) osPriorityHigh,
  .stack_size = 2048 * 4
};
/* Definitions for Ethernet_Communication */
osThreadId_t Ethernet_CommunicationHandle;
const osThreadAttr_t Ethernet_Communication_attributes = {
  .name = "Ethernet_Communication",
  .priority = (osPriority_t) osPriorityLow,
  .stack_size = 3072 * 4
};
/* Definitions for Data_Processing */
osThreadId_t Data_ProcessingHandle;
const osThreadAttr_t Data_Processing_attributes = {
  .name = "Data_Processing",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 4096 * 4
};
/* Definitions for Current_sensor */
osThreadId_t Current_sensorHandle;
const osThreadAttr_t Current_sensor_attributes = {
  .name = "Current_sensor",
  .priority = (osPriority_t) osPriorityAboveNormal,
  .stack_size = 2048 * 4
};
/* Definitions for Vibration_sensor */
osThreadId_t Vibration_sensorHandle;
const osThreadAttr_t Vibration_sensor_attributes = {
  .name = "Vibration_sensor",
  .priority = (osPriority_t) osPriorityAboveNormal,
  .stack_size = 2048 * 4
};
/* Definitions for RTI_Task */
osThreadId_t RTI_TaskHandle;
const osThreadAttr_t RTI_Task_attributes = {
  .name = "RTI_Task",
  .priority = (osPriority_t) osPriorityLow,
  .stack_size = 1024 * 4
};
/* Definitions for I2C_mutex */
osMutexId_t I2C_mutexHandle;
const osMutexAttr_t I2C_mutex_attributes = {
  .name = "I2C_mutex"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
static void AI_PushFeatureVector(float fv[AI_FEATURES]);

osMutexId_t Feature_mutexHandle;
const osMutexAttr_t Feature_mutex_attributes = { .name = "Feature_mutex" };

osMutexId_t AI_Window_mutexHandle;
const osMutexAttr_t AI_Window_mutex_attributes = { .name = "AI_Window_mutex" };

osSemaphoreId_t AI_Ready_semHandle;
const osSemaphoreAttr_t AI_Ready_sem_attributes = { .name = "AI_Ready_sem" };

void RTI_Service(uint32_t elapsed_ms, bool motor_running);
/* USER CODE END FunctionPrototypes */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */
  /* creation of I2C_mutex */
  I2C_mutexHandle = osMutexNew(&I2C_mutex_attributes);

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  UART_mutexHandle = osMutexNew(&UART_mutex_attributes);
  Feature_mutexHandle = osMutexNew(&Feature_mutex_attributes);

  AI_Window_mutexHandle = osMutexNew(&AI_Window_mutex_attributes);
  AI_Ready_semHandle = osSemaphoreNew(1, 0, &AI_Ready_sem_attributes);
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */

  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of Temp_sensor */
  Temp_sensorHandle = osThreadNew(temp_data, NULL, &Temp_sensor_attributes);

  /* creation of Motor_driver */
  Motor_driverHandle = osThreadNew(motor_driver, NULL, &Motor_driver_attributes);

  /* creation of Ethernet_Communication */
  Ethernet_CommunicationHandle = osThreadNew(Lwip_ETH, NULL, &Ethernet_Communication_attributes);

  /* creation of Data_Processing */
  Data_ProcessingHandle = osThreadNew(Data_Processing, NULL, &Data_Processing_attributes);

  /* creation of Current_sensor */
  Current_sensorHandle = osThreadNew(Ina260_data, NULL, &Current_sensor_attributes);

  /* creation of Vibration_sensor */
  Vibration_sensorHandle = osThreadNew(Lis3dh_data, NULL, &Vibration_sensor_attributes);

  /* creation of RTI_Task */
  RTI_TaskHandle = osThreadNew(RTI_Task, NULL, &RTI_Task_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}
/* USER CODE BEGIN Header_StartDefaultTask */
/**
* @brief Function implementing the defaultTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN defaultTask */

  /* Infinite loop */
  for(;;)
  {
	  /* wait until Data_Processing has prepared one locked 30x12 window */
	   osSemaphoreAcquire(AI_Ready_semHandle, osWaitForever);

	   /* AI runs here, not inside Data_Processing */
	   MX_X_CUBE_AI_Process();
//	   osDelay(5);
  }
  /* USER CODE END defaultTask */
}

/* USER CODE BEGIN Header_temp_data */
/**
* @brief Function implementing the Temp_sensor thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_temp_data */
void temp_data(void *argument)
{
  /* USER CODE BEGIN Temp_sensor */
	static float temp_start = 0.0f;
	static uint8_t temp_start_set = 0;

	static float temp_10s_old = 0.0f;
	static uint8_t temp_10s_set = 0;
	static uint32_t last_10s_tick = 0;

  /* Infinite loop */
  for(;;)
  {
	  float t = DS18B20_ReadTemp();  // read result

	  if (t > -50.0f && t < 100.0f)
	  {
	      if (!temp_start_set)
	      {
	          temp_start = t;
	          temp_start_set = 1;
	      }

	      taskENTER_CRITICAL();
	      temp = t;
	      temp_ai = t - temp_start;
	      taskEXIT_CRITICAL();

	      if (!temp_10s_set)
	      {
	          temp_10s_old = t;
	          last_10s_tick = HAL_GetTick();
	          temp_10s_set = 1;
	      }

	      if ((HAL_GetTick() - last_10s_tick) >= 10000)
	      {
	          taskENTER_CRITICAL();
	          temp_slope_10s = t - temp_10s_old;
	          taskEXIT_CRITICAL();

	          temp_10s_old = t;
	          last_10s_tick = HAL_GetTick();
	      }
	  }

	  osDelay(50); // ~1.25 Hz total
  }
  /* USER CODE END Temp_sensor */
}

/* USER CODE BEGIN Header_motor_driver */
/**
* @brief Function implementing the Motor_driver thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_motor_driver */
void motor_driver(void *argument)
{
  /* USER CODE BEGIN Motor_driver */
	uint32_t wake = osKernelGetTickCount();
  /* Infinite loop */
  for(;;)
  {

	  uint16_t s = motor_control();   // motor still runs normally

//	  uint16_t s = 0;   // test only

	  taskENTER_CRITICAL();
	  speed = s;
	  taskEXIT_CRITICAL();

	  wake += 5;
	  osDelayUntil(wake);

//	speed = motor_control();
//	osDelay(5); //1.489 khz pwm frewuency(value update at 20hz for speed variable)
  }
  /* USER CODE END Motor_driver */
}

/* USER CODE BEGIN Header_Lwip_ETH */
/**
* @brief Function implementing the Ethernet_Communication thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Lwip_ETH */
void Lwip_ETH(void *argument)
{
  /* USER CODE BEGIN Ethernet_Communication */

  /* Infinite loop */
  for(;;)
  {
	  ethernetif_input(&gnetif);
	  sys_check_timeouts();

	  Ethernet_Link_Periodic_Handle(&gnetif);
	  DHCP_Periodic_Handle(&gnetif);
	  osDelay(100);
  }
  /* USER CODE END Ethernet_Communication */
}

/* USER CODE BEGIN Header_Data_Processing */
/**
* @brief Function implementing the Data_Processing thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Data_Processing */
void Data_Processing(void *argument)
{
  /* USER CODE BEGIN Data_Processing */
	uint32_t wake = osKernelGetTickCount();
	/* Infinite loop */
	for (;;)
	{
		 /* 1. Wait until both sensor frames are fresh */
		    taskENTER_CRITICAL();
//		    uint8_t ready = (frame_sync_counter >= 2);
//		    if (ready)
//		    {
//		      frame_sync_counter   = 0;
		    uint8_t ready = cur_frame_ready && vib_frame_ready;
		    if (ready)
		    {
		      cur_frame_ready      = 0;
		      vib_frame_ready      = 0;

		      temp_slope_snapshot  = temp_slope_10s;
		      speed_snapshot       = speed;
//		      current_rms_snapshot = current_rms;
//		      voltage_rms_snapshot = voltage_rms;
		    }
		    taskEXIT_CRITICAL();

		    if (!ready)
		    {
		      osDelay(1);
		      wake = osKernelGetTickCount();          /* FIX: no burst after waiting */
		      continue;
		    }

		    /* 2. Snapshot derived features */
		    INA260_FeatureData_t ina = INA260_GetFeatureData();
		    LIS3DH_Features_t    vib = LIS3DH_GetFeatureData().features;
//
		    current_rms_snapshot      = ina.rms_current;
		    voltage_rms_snapshot      = ina.rms_voltage;
//
		    current_variance_snapshot = ina.current_variance;
		    current_crest_snapshot    = ina.current_crest_factor;

		    uint8_t powered = (voltage_rms_snapshot > 1.0f) || (current_rms_snapshot > 0.05f);
		    float   pwm     = powered ? (float)speed_snapshot / 99.0f : 0.0f;
		    if (pwm > 1.0f) pwm = 1.0f;                 /* FIX: same clip as training */

		    /* 3. Build the 14-feature row (training column order) */
		    float ai_vec[AI_FEATURES] = {
		      temp_slope_snapshot,       /*  0 T_slope_10s          */
		      pwm,                       /*  1 PWM                  */
		      current_rms_snapshot,      /*  2 Irms                 */
		      voltage_rms_snapshot,      /*  3 Vrms                 */
		      current_variance_snapshot, /*  4 Current_Variance     */
		      current_crest_snapshot,    /*  5 Current_Crest_Factor */
		      vib.rms,                   /*  6 Vib_RMS              */
		      vib.min,                   /*  7 Vib_Min              */
		      vib.q1,                    /*  8 Vib_Q1               */
		      vib.median,                /*  9 Vib_Median           */
		      vib.q3,                    /* 10 Vib_Q3               */
		      vib.max,                   /* 11 Vib_Max              */
		      vib.variance,              /* 12 Vib_Variance         */
		      vib.skewness               /* 13 Vib_Skewness         */
		    };

		    /* 4. Publish to dashboard and AI window */
		    osMutexAcquire(Feature_mutexHandle, osWaitForever);
		    memcpy(features, ai_vec, sizeof(ai_vec));
		    osMutexRelease(Feature_mutexHandle);

		    AI_PushFeatureVector(ai_vec);

		    /* 5. Fixed 15 ms row period */
		    wake += 15;
		    osDelayUntil(wake);
		  }
  /* USER CODE END Data_Processing */
}

/* USER CODE BEGIN Header_Ina260_data */
/**
* @brief Function implementing the Current_sensor thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Ina260_data */
void Ina260_data(void *argument)
{
  /* USER CODE BEGIN Current_sensor */
	INA260_CheckDevice();
	INA260_InitFast();

	uint32_t wake = osKernelGetTickCount();
  /* Infinite loop */
  for(;;)
  {
	  INA260_UpdateFeatures();

	  INA260_FeatureData_t f = INA260_GetFeatureData();

	  latest_current = f.raw_current;
	  latest_voltage = f.raw_voltage;

	  ina_data.avg_current = f.filtered_current;
	  ina_data.avg_voltage = f.filtered_voltage;

	  current_rms = f.rms_current;
	  voltage_rms = f.rms_voltage;

	  if (f.rms_ready)
	  {
//		 if (frame_sync_counter < 2)  frame_sync_counter++;
		  cur_frame_ready = 1;
	  }

	  wake += 5;
	  osDelayUntil(wake);
  }// ~200 Hz total
  /* USER CODE END Current_sensor */
}

/* USER CODE BEGIN Header_Lis3dh_data */
/**
* @brief Function implementing the Vibration_sensor thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Lis3dh_data */
void Lis3dh_data(void *argument)
{
  /* USER CODE BEGIN Vibration_sensor */
  LIS3DH_Init();
	uint32_t wake = osKernelGetTickCount();
  /* Infinite loop */
  for(;;)
  {
//	  // inside the vibration task's for(;;) loop, right at the top
//	  static uint32_t last_tick = 0;
//	  uint32_t now = HAL_GetTick();
//	  if (last_tick != 0) {
//	      uint32_t delta = now - last_tick;
//	      if (delta > 6) {   // should be ~5ms; flag anything meaningfully over
//	          printf("VIB_TASK_LATE delta=%lu\r\n", delta);
//	      }
//	  }
//	  last_tick = now;
	 LIS3DH_UpdateFeatures();

	 LIS3DH_FeatureData_t f = LIS3DH_GetFeatureData();

	 taskENTER_CRITICAL();
	 vib_data = f.raw;
	 taskEXIT_CRITICAL();

	 if (f.features_ready)
	 {
//		 if (frame_sync_counter < 2) frame_sync_counter++;
		 vib_frame_ready = 1;
	 }

	 wake += 5;
	 osDelayUntil(wake);
  }// ~200 Hz total
  /* USER CODE END Vibration_sensor */
}

/* USER CODE BEGIN Header_RTI_Task */
/**
* @brief Function implementing the RTI_Task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_RTI_Task */
void RTI_Task(void *argument)
{
  /* USER CODE BEGIN RTI_Task */
	RTI_NewCycle(&g_rti);
	uint32_t previous_tick = HAL_GetTick();
  /* Infinite loop */
  for(;;)
  {
	  uint32_t now = HAL_GetTick();
	  uint32_t elapsed_ms = now - previous_tick;
	  previous_tick = now;

	  bool motor_running;

	  taskENTER_CRITICAL();
	  motor_running = (speed > 0U);
	  taskEXIT_CRITICAL();

	  RTI_Service(elapsed_ms, motor_running);
//	  uint32_t total_seconds =
//	      (uint32_t)(mt_remaining_inspection_h * 3600.0f);
//
//	  uint32_t hh = total_seconds / 3600U;
//	  uint32_t mm = (total_seconds % 3600U) / 60U;
//	  uint32_t ss = total_seconds % 60U;
//
//	  printf("RTI,%02lu:%02lu:%02lu\r\n",
//	         (unsigned long)hh,
//	         (unsigned long)mm,
//	         (unsigned long)ss);

	  osDelay(1000);
  }
  /* USER CODE END RTI_Task */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

void RTI_Service(uint32_t elapsed_ms, bool motor_running)
{
    if (!g_rti.initialized)
    {
        mt_remaining_inspection_h = 0.0f;
        return;
    }

    if (motor_running)
    {
        double risk;

        taskENTER_CRITICAL();
        risk = g_rti_latest_risk;
        taskEXIT_CRITICAL();

        RTI_Update(&g_rti,
                   (double)elapsed_ms / 3600000.0,
                   risk);
    }

    mt_remaining_inspection_h = (float)RTI_Remaining(&g_rti);
    mt_inspection_due = g_rti.inspection_due;
}

static void AI_PushFeatureVector(float fv[AI_FEATURES])
{
    ++ai_feature_sequence;

    /* shift 30-sample sliding window */
    for (int t = 0; t < AI_TIME_STEPS - 1; t++)
    {
        memcpy(ai_live_window[t], ai_live_window[t + 1], sizeof(ai_live_window[t]));
    }
    memcpy(ai_live_window[AI_TIME_STEPS - 1], fv, sizeof(ai_live_window[0]));

    if (ai_window_count < AI_TIME_STEPS)
    {
        ai_window_count++;
        if (ai_window_count < AI_TIME_STEPS)
            return;
        ai_step_count = 0;
    }
    else
    {
        ai_step_count++;
        if (ai_step_count < AI_STEP_SIZE)
            return;
        ai_step_count = 0;
    }

    osMutexAcquire(AI_Window_mutexHandle, osWaitForever);

    memcpy(ai_ready_window, ai_live_window, sizeof(ai_ready_window));

    osMutexRelease(AI_Window_mutexHandle);

    osSemaphoreRelease(AI_Ready_semHandle);
}
/* USER CODE END Application */

