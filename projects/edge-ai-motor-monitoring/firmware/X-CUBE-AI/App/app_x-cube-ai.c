
/**
  ******************************************************************************
  * @file    app_x-cube-ai.c
  * @author  X-CUBE-AI C code generator
  * @brief   AI program body
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

 /*
  * Description
  *   v1.0 - Minimum template to show how to use the Embedded Client API
  *          model. Only one input and one output is supported. All
  *          memory resources are allocated statically (AI_NETWORK_XX, defines
  *          are used).
  *          Re-target of the printf function is out-of-scope.
  *   v2.0 - add multiple IO and/or multiple heap support
  *
  *   For more information, see the embeded documentation:
  *
  *       [1] %X_CUBE_AI_DIR%/Documentation/index.html
  *
  *   X_CUBE_AI_DIR indicates the location where the X-CUBE-AI pack is installed
  *   typical : C:\Users\[user_name]\STM32Cube\Repository\STMicroelectronics\X-CUBE-AI\7.1.0
  */

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/

#if defined ( __ICCARM__ )
#elif defined ( __CC_ARM ) || ( __GNUC__ )
#endif

/* System headers */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#include "app_x-cube-ai.h"
#include "main.h"
#include "ai_datatypes_defines.h"
#include "dann_hybrid.h"
#include "dann_hybrid_data.h"

/* USER CODE BEGIN includes */
#include "math.h"
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "task.h"
#include "motor_model_params.h"
#include "rti.h"

extern volatile double g_rti_latest_risk;
extern volatile float mt_remaining_inspection_h;

#define PWM_FEATURE_INDEX       1

#define CLASS_OUT_INDEX         0
#define RECON_OUT_INDEX         1
#define ANOMALY_OUT_INDEX       2


extern float ai_ready_window [MOTOR_MODEL_WINDOW_LEN] [MOTOR_MODEL_N_FEATURES];

extern osMutexId_t AI_Window_mutexHandle;

//static uint32_t last_inference_ms;
_Static_assert(AI_DANN_HYBRID_IN_1_FORMAT == AI_BUFFER_FORMAT_FLOAT,
               "The current inference wrapper requires float32 input.");
_Static_assert(AI_DANN_HYBRID_IN_1_SIZE == MOTOR_MODEL_INPUT_SIZE,
               "Generated model and parameter input sizes differ.");
_Static_assert(AI_DANN_HYBRID_OUT_1_SIZE == MOTOR_MODEL_N_CLASSES &&
               AI_DANN_HYBRID_OUT_2_SIZE == MOTOR_MODEL_INPUT_SIZE &&
               AI_DANN_HYBRID_OUT_3_SIZE == 1,
               "Verify output mapping against the generated model.");
_Static_assert(AI_DANN_HYBRID_OUT_1_FORMAT == AI_BUFFER_FORMAT_FLOAT &&
               AI_DANN_HYBRID_OUT_2_FORMAT == AI_BUFFER_FORMAT_FLOAT &&
               AI_DANN_HYBRID_OUT_3_FORMAT == AI_BUFFER_FORMAT_FLOAT,
               "The current inference wrapper requires float32 outputs.");


AI_ALIGNED(32)
static float normalized_snapshot[MOTOR_MODEL_INPUT_SIZE];

AI_ALIGNED(32)
static float raw_snapshot[MOTOR_MODEL_INPUT_SIZE];

char mt_status[16] = "WAIT";
char mt_state[48] = "waiting";
char mt_class[32] = "none";

float mt_class_conf = 0.0f;
float mt_mse = 0.0f;
float mt_threshold = MOTOR_MODEL_RECON_THRESHOLD;
float mt_aprob = 0.0f;
float mt_severity_ratio = 0.0f;
char mt_severity[16] = "normal";
volatile uint8_t mt_critical_fault = 0;
static uint8_t critical_fault_count = 0;

volatile uint32_t mt_fault_event = 0;
volatile uint8_t mt_fault_pending = 0;
volatile uint8_t mt_fault_class = 255;
volatile uint8_t mt_inspection_action = 0;

static uint8_t accepted_fault_class = 255;
static uint8_t fault_rearmed = 1;
static uint32_t next_fault_reminder_ms = 0;

/* USER CODE END includes */

/* IO buffers ----------------------------------------------------------------*/

#if !defined(AI_DANN_HYBRID_INPUTS_IN_ACTIVATIONS)
AI_ALIGNED(4) ai_i8 data_in_1[AI_DANN_HYBRID_IN_1_SIZE_BYTES];
ai_i8* data_ins[AI_DANN_HYBRID_IN_NUM] = {
data_in_1
};
#else
ai_i8* data_ins[AI_DANN_HYBRID_IN_NUM] = {
NULL
};
#endif

#if !defined(AI_DANN_HYBRID_OUTPUTS_IN_ACTIVATIONS)
AI_ALIGNED(4) ai_i8 data_out_1[AI_DANN_HYBRID_OUT_1_SIZE_BYTES];
AI_ALIGNED(4) ai_i8 data_out_2[AI_DANN_HYBRID_OUT_2_SIZE_BYTES];
AI_ALIGNED(4) ai_i8 data_out_3[AI_DANN_HYBRID_OUT_3_SIZE_BYTES];
ai_i8* data_outs[AI_DANN_HYBRID_OUT_NUM] = {
data_out_1,
data_out_2,
data_out_3
};
#else
ai_i8* data_outs[AI_DANN_HYBRID_OUT_NUM] = {
NULL,
NULL,
NULL
};
#endif

/* Activations buffers -------------------------------------------------------*/

AI_ALIGNED(32)
static uint8_t pool0[AI_DANN_HYBRID_DATA_ACTIVATION_1_SIZE];

ai_handle data_activations0[] = {pool0};

/* AI objects ----------------------------------------------------------------*/

static ai_handle dann_hybrid = AI_HANDLE_NULL;

static ai_buffer* ai_input;
static ai_buffer* ai_output;

static void ai_log_err(const ai_error err, const char *fct)
{
  /* USER CODE BEGIN log */
  if (fct)
    printf("TEMPLATE - Error (%s) - type=0x%02x code=0x%02x\r\n", fct,
        err.type, err.code);
  else
    printf("TEMPLATE - Error - type=0x%02x code=0x%02x\r\n", err.type, err.code);

  do {} while (1);
  /* USER CODE END log */
}

static int ai_boostrap(ai_handle *act_addr)
{
  ai_error err;

  /* Create and initialize an instance of the model */
  err = ai_dann_hybrid_create_and_init(&dann_hybrid, act_addr, NULL);
  if (err.type != AI_ERROR_NONE) {
    ai_log_err(err, "ai_dann_hybrid_create_and_init");
    return -1;
  }

  ai_input = ai_dann_hybrid_inputs_get(dann_hybrid, NULL);
  ai_output = ai_dann_hybrid_outputs_get(dann_hybrid, NULL);

#if defined(AI_DANN_HYBRID_INPUTS_IN_ACTIVATIONS)
  /*  In the case where "--allocate-inputs" option is used, memory buffer can be
   *  used from the activations buffer. This is not mandatory.
   */
  for (int idx=0; idx < AI_DANN_HYBRID_IN_NUM; idx++) {
	data_ins[idx] = ai_input[idx].data;
  }
#else
  for (int idx=0; idx < AI_DANN_HYBRID_IN_NUM; idx++) {
	  ai_input[idx].data = data_ins[idx];
  }
#endif

#if defined(AI_DANN_HYBRID_OUTPUTS_IN_ACTIVATIONS)
  /*  In the case where "--allocate-outputs" option is used, memory buffer can be
   *  used from the activations buffer. This is no mandatory.
   */
  for (int idx=0; idx < AI_DANN_HYBRID_OUT_NUM; idx++) {
	data_outs[idx] = ai_output[idx].data;
  }
#else
  for (int idx=0; idx < AI_DANN_HYBRID_OUT_NUM; idx++) {
	ai_output[idx].data = data_outs[idx];
  }
#endif

  return 0;
}

static int ai_run(void)
{
  ai_i32 batch;

  batch = ai_dann_hybrid_run(dann_hybrid, ai_input, ai_output);
  if (batch != 1) {
    ai_log_err(ai_dann_hybrid_get_error(dann_hybrid),
        "ai_dann_hybrid_run");
    return -1;
  }

  return 0;
}

/* USER CODE BEGIN 2 */

static float normalize_feature(float value, int feature)
{
    float variance = MOTOR_MODEL_NORMALIZATION_VARIANCE[feature];
    if (variance < 1.0e-12f) variance = 1.0e-12f;
    return (value - MOTOR_MODEL_NORMALIZATION_MEAN[feature]) / sqrtf(variance + 1.0e-7f);
}

/* Reconstruction threshold: per operating state, and per PWM bin for running motor */
static float get_recon_threshold(int class_id, float pwm)
{
    if (class_id == 0) return MOTOR_MODEL_MOTOR_OFF_RECON_THRESHOLD;
    if (class_id == 1) return MOTOR_MODEL_TURNING_OFF_RECON_THRESHOLD;
    if (class_id == 2) return MOTOR_MODEL_TURNING_ON_RECON_THRESHOLD;
    return MOTOR_MODEL_NORMAL_THRESHOLDS[motor_model_get_pwm_bin(pwm)];
}

static float get_local_baseline(float pwm)
{
    return MOTOR_MODEL_LOCAL_NORMAL_BASELINES[motor_model_get_pwm_bin(pwm)];
}

static const char *get_severity_label(float severity_ratio)
{
    if (severity_ratio < MOTOR_MODEL_SEVERITY_MILD_MULTIPLIER)     return "normal";
    if (severity_ratio < MOTOR_MODEL_SEVERITY_MODERATE_MULTIPLIER) return "mild";
    if (severity_ratio < MOTOR_MODEL_SEVERITY_SEVERE_MULTIPLIER)   return "moderate";
    return "severe";
}

int acquire_and_process_data(ai_i8 *data[])
{
    if ((data == NULL) || (data[0] == NULL))
        return -1;

    float *input = (float *)data[0];

    if (osMutexAcquire(AI_Window_mutexHandle, osWaitForever) != osOK)
        return -2;

    int k = 0;
    for (int t = 0; t < MOTOR_MODEL_WINDOW_LEN; t++)
    {
        for (int f = 0; f < MOTOR_MODEL_N_FEATURES; f++)
        {
            float raw = ai_ready_window[t][f];
            if (!isfinite(raw))
                raw = MOTOR_MODEL_NORMALIZATION_MEAN[f];

            raw_snapshot[k]        = raw;
            normalized_snapshot[k] = normalize_feature(raw, f);  /* reference for recon MSE */
            input[k]               = raw;                        /* model normalizes internally */
            k++;
        }
    }

    osMutexRelease(AI_Window_mutexHandle);
    return 0;
}


int post_process(ai_i8 *data[])
{
    if (data == NULL)
        return -1;

    float *class_output   = (float *)data[CLASS_OUT_INDEX];
    float *recon_output   = (float *)data[RECON_OUT_INDEX];
    float *anomaly_output = (float *)data[ANOMALY_OUT_INDEX];

    if ((class_output == NULL) ||
        (recon_output == NULL) ||
        (anomaly_output == NULL))
        return -2;

    /* 1. Classification */
    int class_id = 0;
    float class_conf = -1.0f;

    for (int i = 0; i < MOTOR_MODEL_N_CLASSES; i++)
    {
        float p = class_output[i];

        if (!isfinite(p))
            p = 0.0f;

        if (p > class_conf)
        {
            class_conf = p;
            class_id = i;
        }
    }

    if (class_conf < 0.0f)
        class_conf = 0.0f;

    if (class_conf > 1.0f)
        class_conf = 1.0f;

    /* 2. Binary anomaly probability */
//    float anomaly_probability = anomaly_output[0];

    float anomaly_logit = anomaly_output[0];

    if (!isfinite(anomaly_logit))
        return -3;

    float anomaly_probability =
        1.0f / (1.0f + expf(-anomaly_logit));

    if (!isfinite(anomaly_probability)) anomaly_probability = 0.0f;
    if (anomaly_probability < 0.0f) anomaly_probability = 0.0f;
    if (anomaly_probability > 1.0f) anomaly_probability = 1.0f;

    /* 3. Reconstruction MSE */
    float mse = 0.0f;

    for (int i = 0; i < MOTOR_MODEL_INPUT_SIZE; i++)
    {
        float recon = recon_output[i];

        if (!isfinite(recon))
            recon = 0.0f;

        float err = normalized_snapshot[i] - recon;

        mse += err * err;
    }

    mse /= (float)MOTOR_MODEL_INPUT_SIZE;

    /* 4. Mean PWM */
    float mean_pwm = 0.0f;

    for (int t = 0; t < MOTOR_MODEL_WINDOW_LEN; t++)
    {
        mean_pwm += raw_snapshot[
            t * MOTOR_MODEL_N_FEATURES + PWM_FEATURE_INDEX
        ];
    }

    mean_pwm /= (float)MOTOR_MODEL_WINDOW_LEN;

    float threshold = get_recon_threshold(class_id, mean_pwm);

    /* 5. Severity */
    float severity_ratio;
    const char *severity_label;

    if (class_id <= 2)
    {
        severity_ratio = 0.0f;
        severity_label = "n/a";
    }
    else
    {
        float baseline = get_local_baseline(mean_pwm);

        severity_ratio = (baseline > 0.0f)
                       ? (mse / baseline)
                       : 0.0f;

        severity_label = get_severity_label(severity_ratio);
    }

    /* 6. Final decision */
    int binary_flag = (
        anomaly_probability >= MOTOR_MODEL_ANOMALY_THRESHOLD
    );

    int recon_flag = (mse > threshold);

    int confident = (
        class_conf >= MOTOR_MODEL_CLASS_CONF_THRESHOLD
    );

    const char *class_name = MOTOR_MODEL_LABELS[class_id];
    const char *status;
    const char *state;

    if ((class_id >= 4) && confident)
    {
        status = "ANOMALY";
        state = class_name;
    }
    else if (binary_flag || recon_flag)
    {
        status = "ANOMALY";
        state = "unknown_fault";
    }
    else if ((class_id <= 3) && confident)
    {
        status = "NORMAL";
        state = class_name;
    }
    else
    {
        status = "NORMAL";
        state = "uncertain_safe";
    }

    /* 7. Publish result */
    snprintf(mt_status, sizeof(mt_status), "%s", status);
    snprintf(mt_state, sizeof(mt_state), "%s", state);
    snprintf(mt_class, sizeof(mt_class), "%s", class_name);
    snprintf(mt_severity, sizeof(mt_severity), "%s", severity_label);

    mt_class_conf = class_conf * 100.0f;
    mt_mse = mse;
    mt_threshold = threshold;
    mt_aprob = anomaly_probability * 100.0f;
    mt_severity_ratio = severity_ratio;

    /* RTI: calculate risk from existing DANN outputs */

    int is_anomaly = (strcmp(mt_status, "ANOMALY") == 0);

    int known_fault =
        (class_id >= 4) &&
        (class_id <= 6) &&
        (class_conf >= MOTOR_MODEL_CLASS_CONF_THRESHOLD);

    double new_risk;

    if (class_id <= 2 && !is_anomaly)
    {
        new_risk = 0.0;
    }
    else
    {
        new_risk = RTI_RiskFromPrediction(
            (double)mt_aprob,
            (double)mt_mse,
            (double)mt_threshold,
            (double)mt_severity_ratio,
            (double)mt_class_conf,
            known_fault,
            is_anomaly
        );
    }

    taskENTER_CRITICAL();
    g_rti_latest_risk = new_risk;
    taskEXIT_CRITICAL();

    /* Fast inspection warning.
     * This block does NOT modify RTI, risk, or AI output.
     */
    uint32_t now_ms = HAL_GetTick();

    /* Process operator decision */
    uint8_t action = mt_inspection_action;
    mt_inspection_action = 0U;

    if (action == 1U)
    {
        /* Operator reports repair completed.
           Require the critical condition to clear before rearming. */
        mt_fault_pending = 0U;
        accepted_fault_class = 255U;
        fault_rearmed = 0U;
    }
    else if (action == 2U)
    {
        /* Suppress repeat warnings for this accepted class only */
        accepted_fault_class = mt_fault_class;
        mt_fault_pending = 0U;
        fault_rearmed = 1U;
    }
    else if (action == 3U)
    {
        /* No inspection: remind after 30 seconds */
        mt_fault_pending = 0U;
        fault_rearmed = 1U;
        next_fault_reminder_ms = now_ms + 30000U;
    }

    int critical_prediction =
        known_fault &&
        (class_conf >= 0.85f) &&
        (
            (anomaly_probability >= 0.80f) ||
            (severity_ratio >= 4.0f)
        );

    if (critical_prediction)
    {
        if (critical_fault_count < 5U)
            critical_fault_count++;
    }
    else
    {
        critical_fault_count = 0U;

        /* Condition has cleared: enable future detection */
        fault_rearmed = 1U;
    }

    /* Five consecutive qualifying predictions */
    if (critical_fault_count >= 5U)
    {
        mt_critical_fault = 1U;  /* Existing historical flag */

        if (fault_rearmed &&
            !mt_fault_pending &&
            accepted_fault_class != (uint8_t)class_id &&
            (int32_t)(now_ms - next_fault_reminder_ms) >= 0)
        {
            mt_fault_class = (uint8_t)class_id;
            mt_fault_event++;
            mt_fault_pending = 1U;
            fault_rearmed = 0U;
        }
    }



//    /* =====================================================
//     * 8. PuTTY CSV logging
//     * Latest 10 feature vectors from the 30-vector window
//     * Prediction only in the final column of the 10th row
//     * ===================================================== */
//    {
//        static uint8_t csv_header_printed = 0;
//
//        if (!csv_header_printed)
//        {
//            printf(
//                "T_slope_10s,PWM,Irms,Vrms,"
//                "Current_Variance,Current_Crest_Factor,"
//                "Vib_RMS,Vib_Min,Vib_Q1,Vib_Median,"
//                "Vib_Q3,Vib_Max,Vib_Variance,"
//                "Vib_Skewness,Prediction\r\n"
//            );
//
//            csv_header_printed = 1;
//        }
//
//        for (int t = MOTOR_MODEL_WINDOW_LEN - 10;
//             t < MOTOR_MODEL_WINDOW_LEN;
//             t++)
//        {
//            const int row = t * MOTOR_MODEL_N_FEATURES;
//
//            for (int f = 0; f < MOTOR_MODEL_N_FEATURES; f++)
//            {
//                float value = raw_snapshot[row + f];
//
//                const char *sign = signbit(value) ? "-" : "";
//                float magnitude = fabsf(value);
//
//                long scaled = (long)(
//                    magnitude * 100000.0f + 0.5f
//                );
//
//                printf(
//                    "%s%ld.%05ld,",
//                    sign,
//                    scaled / 100000L,
//                    scaled % 100000L
//                );
//            }
//
//            if (t == MOTOR_MODEL_WINDOW_LEN - 1)
//            {
//                printf("%s", state);
//            }
//
//            printf("\r\n");
//        }
//    }

    return 0;
}


/* USER CODE END 2 */

/* Entry points --------------------------------------------------------------*/

void MX_X_CUBE_AI_Init(void)
{
    /* USER CODE BEGIN 5 */
  printf("\r\nDANN_HYBRID 3-head FLOAT initialization\r\n");

  ai_boostrap(data_activations0);

    /* USER CODE END 5 */
}

void MX_X_CUBE_AI_Process(void)
{
    /* USER CODE BEGIN 6 */
//	  osThreadId_t self;
//	    osPriority_t original;
//
//	    self = osThreadGetId();
//	    original = osThreadGetPriority(self);

	int res = acquire_and_process_data(data_ins);

		if (res == 0){
//			  osThreadSetPriority(self, osPriorityRealtime);
//			  uint32_t t0 = HAL_GetTick();
			  res = ai_run();
//			  uint32_t t1 = HAL_GetTick();
//			  last_inference_ms = t1 - t0;
//			  osThreadSetPriority(self, original);
		}

		if (res == 0)
		    res = post_process(data_outs);
    /* USER CODE END 6 */
}
#ifdef __cplusplus
}
#endif
