/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "main.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "interface_button.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
AnkleController_t ankle_ctrl;

const AnkleBtnPin_t my_button_pins[BTN_COUNT] = {
    [BTN_MODE_TOGGLE]  = {GPIOC, GPIO_PIN_10}, // CN7 Pin 1: Mode 1 / Mode 2
    [BTN_AXIS_TOGGLE]  = {GPIOC, GPIO_PIN_11}, // CN7 Pin 2: Roll / Pitch
    [BTN_ANGLE_INC]    = {GPIOC, GPIO_PIN_12}, // CN7 Pin 3: Angle +
    [BTN_ANGLE_DEC]    = {GPIOD, GPIO_PIN_2},  // CN7 Pin 4: Angle -
    [BTN_STRENGTH_INC] = {GPIOC, GPIO_PIN_8},  // CN10 Pin 2: Strength +
    [BTN_STRENGTH_DEC] = {GPIOC, GPIO_PIN_6},  // CN10 Pin 4: Strength -
    [BTN_START_RESET]  = {GPIOC, GPIO_PIN_5}   // CN10 Pin 6: Start / Reset
};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void AnkleUI_PrintStatus_UART(UART_HandleTypeDef *huart, const AnkleController_t *ctrl) {
    char tx_buf[180];

    const char *state_str = ctrl->is_running ? "STARTED (Force Loaded)" : "RESET (Force Unloaded)";
    const char *mode_str  = (ctrl->mode == TRAINING_MODE_1) ? "Mode 1 (Resist Pedal)" : "Mode 2 (Active Move) ";
    const char *axis_str  = (ctrl->axis == AXIS_ROLL) ? "Roll " : "Pitch";

    int len;
    if (ctrl->mode == TRAINING_MODE_1) {
        len = snprintf(tx_buf, sizeof(tx_buf),
                       "[AnkleUI] Status: %-22s | %s | Axis: %s | Angle: %+3d deg     | Strength: %2d kg\r\n",
                       state_str, mode_str, axis_str, ctrl->angle, ctrl->strength);
    } else {
        // Mode 2 下用户主动发力移动踏板，无需按钮控制角度，显示 [FREE_MOVE]
        len = snprintf(tx_buf, sizeof(tx_buf),
                       "[AnkleUI] Status: %-22s | %s | Axis: %s | Angle: [FREE_MOVE] | Strength: %2d kg\r\n",
                       state_str, mode_str, axis_str, ctrl->strength);
    }

    if (len > 0) {
        HAL_UART_Transmit(huart, (uint8_t *)tx_buf, (uint16_t)len, 100);
    }
}
/* USER CODE END 0 */


/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  /* 初始化 7 个按钮引脚与控制器 (角度步长 = 1°, 力量步长 = 1 kg) */
  AnkleUI_Init(&ankle_ctrl, my_button_pins, 1, 1);

  /* 上电打印欢迎信息与初始状态 */
  const char *banner = "\r\n========================================================\r\n"
                       "   Ankle Trainer 7-Button UI Controller Debug Console   \r\n"
                       "========================================================\r\n";
  HAL_UART_Transmit(&huart2, (uint8_t *)banner, strlen(banner), 100);
  AnkleUI_PrintStatus_UART(&huart2, &ankle_ctrl);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* 非阻塞扫描 7 个按钮：仅在状态发生变化时通过串口打印最新状态 */
    if (AnkleUI_Update(&ankle_ctrl))
    {
      AnkleUI_PrintStatus_UART(&huart2, &ankle_ctrl);
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}


/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
