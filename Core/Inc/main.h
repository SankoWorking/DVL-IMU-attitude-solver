/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
typedef enum
{
  MOTOR_OUTPUT_TOP = 0,
  MOTOR_OUTPUT_RIGHT,
  MOTOR_OUTPUT_LEFT,
  MOTOR_OUTPUT_FRONT_ROLLER,
  MOTOR_OUTPUT_COUNT
} MotorOutput_t;

typedef enum
{
  MOTOR_DIR_CW = 0,
  MOTOR_DIR_CCW
} MotorDir_t;

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
void Motor_Init(void);
void Motor_StopAll(void);
void Motor_SetPercent(MotorOutput_t motor, MotorDir_t dir, uint8_t duty_percent);

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define WATER_DO_Pin GPIO_PIN_10
#define WATER_DO_GPIO_Port GPIOF
#define MOTOR_ROLLER_DIR_Pin GPIO_PIN_4
#define MOTOR_ROLLER_DIR_GPIO_Port GPIOA
#define MOTOR2_DIR_Pin GPIO_PIN_7
#define MOTOR2_DIR_GPIO_Port GPIOA
#define MOTOR3_DIR_Pin GPIO_PIN_4
#define MOTOR3_DIR_GPIO_Port GPIOC

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
