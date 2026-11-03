/*
 * motor.c
 *
 *  Created on: Apr 8, 2026
 */


#include "motor.h"
#include "stdio.h"
#include <stdlib.h>

/*for interuppt while recording the data*/
volatile uint8_t profile_start = 0;
volatile uint8_t profile_reset = 0;

void motor_init(void)
{
	  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
	  __HAL_TIM_SET_COMPARE(&htim12, TIM_CHANNEL_2, 0);

	  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);    // RPWM
	  HAL_TIM_PWM_Start(&htim12, TIM_CHANNEL_2);   // LPWM

	  // Enable driver
	  HAL_GPIO_WritePin(GPIOG, GPIO_PIN_8, GPIO_PIN_SET); // R_EN
	  HAL_GPIO_WritePin(GPIOG, GPIO_PIN_5, GPIO_PIN_SET); // L_EN
}

/*The below function is to record motor data at different speed ramps*/

volatile uint8_t  ramp_tag        = 0;
volatile uint8_t  motor_direction = 0;

//static const uint16_t PHASE_HOLD_MS[4] = {1000, 500, 200, 100};
//static const uint8_t  PHASE_TAG[4]     = {1, 2, 3, 4};
//
//float motor_control(void)
//{
//    static uint8_t  started   = 0;
//    static uint8_t  done      = 0;
//    static uint8_t  phase     = 0;     /* 0..3 */
//    static int16_t  spd       = 1;     /* 1..100 */
//    static int8_t   step      = +1;    /* +1 ramp up, -1 ramp down */
//    static uint32_t last_tick = 0;
//
//    /*The below if(!profile_start for interuppt only as we receive the command from powercell to start motor*/
//    if (profile_reset)
//    {
//        started = 0;
//        done = 0;
//        phase = 0;
//        spd = 1;
//        step = +1;
//
//        motor_direction = 0;
//        ramp_tag = 0;
//        profile_start = 0;
//        profile_reset = 0;
//
//        __HAL_TIM_SET_COMPARE(&htim1,  TIM_CHANNEL_1, 0);
//        __HAL_TIM_SET_COMPARE(&htim12, TIM_CHANNEL_2, 0);
//
//        return 0.0f;
//    }
//
//    if (!profile_start)
//        {
//            __HAL_TIM_SET_COMPARE(&htim1,  TIM_CHANNEL_1, 0);
//            __HAL_TIM_SET_COMPARE(&htim12, TIM_CHANNEL_2, 0);
//
//            ramp_tag = 0;
//            return 0.0f;
//        }
//
//    if (!started) { last_tick = HAL_GetTick(); started = 1; }
//
//    if (done)
//    {
//        __HAL_TIM_SET_COMPARE(&htim1,  TIM_CHANNEL_1, 0);
//        __HAL_TIM_SET_COMPARE(&htim12, TIM_CHANNEL_2, 0);
//        ramp_tag = 0;
//        return 0.0f;
//    }
//
//    uint32_t now = HAL_GetTick();
//    if ((now - last_tick) >= PHASE_HOLD_MS[phase])
//    {
//        last_tick = now;
//        spd += step;
//
//        if (spd >= 100) { spd = 100; step = -1; }          /* top reached */
//        else if (spd <= 1 && step == -1)                   /* bottom of a sweep */
//        {
//            spd  = 1;
//            step = +1;
//            if (motor_direction == 0)                      /* did dir0, now dir1 */
//            {
//                motor_direction = 1;
//            }
//            else                                           /* finished both dirs */
//            {
//                motor_direction = 0;
//                phase++;
//                if (phase > 3) { done = 1; }
//            }
//        }
//    }
//
//    uint16_t pwm = (spd >= 100) ? 999 : (uint16_t)(spd * 10);
//
//    if (motor_direction == 0) {                            /* right */
//        __HAL_TIM_SET_COMPARE(&htim1,  TIM_CHANNEL_1, pwm);
//        __HAL_TIM_SET_COMPARE(&htim12, TIM_CHANNEL_2, 0);
//    } else {                                               /* left */
//        __HAL_TIM_SET_COMPARE(&htim1,  TIM_CHANNEL_1, 0);
//        __HAL_TIM_SET_COMPARE(&htim12, TIM_CHANNEL_2, pwm);
//    }
//
//    ramp_tag = done ? 0 : PHASE_TAG[phase];
//    return (float)spd;                                     /* 1..100 commanded speed */
//}


/*****************************************************************/
///*The below function is to run motor via Potentiometer command*/

float motor_control(void)
{
	static float filtered = 0.0f;
	    static uint16_t last_percent = 0U;

	    HAL_ADC_Start(&hadc1);

	    if (HAL_ADC_PollForConversion(&hadc1, 5) == HAL_OK)
	    {
	        uint32_t adc_val = HAL_ADC_GetValue(&hadc1);
	        HAL_ADC_Stop(&hadc1);

	        /* Smooth potentiometer readings. */
	        filtered = (0.9f * filtered) + (0.1f * adc_val);

	        /* Measured endpoints: minimum 85–95, maximum 4095. */
	        const float adc_min = 95.0f;
	        const float adc_max = 4095.0f;

	        /* Map calibrated knob travel to 0–100%. */
	        float mapped = 100.0f * (filtered - adc_min) / (adc_max - adc_min);

	        if (mapped < 0.0f)
	        {
	            mapped = 0.0f;
	        }

	        if (mapped > 100.0f)
	        {
	            mapped = 100.0f;
	        }

	        uint16_t percent = (uint16_t)(mapped + 0.5f);
	        last_percent = percent;

	        /* ARR = 999: CCR 0–1000 gives 0–100% duty. */
	        uint16_t pwm = percent * 10U;

	        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0U);
	        __HAL_TIM_SET_COMPARE(&htim12, TIM_CHANNEL_2, pwm);

	        return (float)percent;
	    }

	    HAL_ADC_Stop(&hadc1);
	    return (float)last_percent;
}
/*************************************************************/
