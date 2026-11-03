/*
 * motor.h
 *
 *  Created on: Apr 8, 2026
 */

#ifndef MOTOR_MOTOR_H_
#define MOTOR_MOTOR_H_

#include "main.h"
#include "stdlib.h"

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim12;

extern ADC_HandleTypeDef hadc1;

void motor_init(void);
float motor_control(void);

#endif /* MOTOR_MOTOR_H_ */
