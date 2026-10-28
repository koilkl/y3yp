/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : 2-Button Force + 3-State Toggle Switch + Status LED + 6-LED Strip
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
// 配置 4 路输入引脚：
// 保持 PC12 (加力) 和 PD2 (减力) 不变；新增 PC10 (拨动开关左档) 和 PC11 (拨动开关右档)
const AnkleBtnPin_t my_pins[INPUT_COUNT] = {
    [BTN_UP]       = PC12,
    [BTN_DOWN]     = PD2,
    [SW_POS_LEFT]  = PC10,
    [SW_POS_RIGHT] = PC11
};

AnkleCtrl_t ankle_ctrl = {
    .raw_value          = 0,
    .min_raw            = 0,
    .max_raw            = 20,
    .display_percentage = 0,
    .active_leds        = 0,
    .sw_state           = SW_STATE_CENTER_OFF,
    .btn_conflict       = false,
    .sw_fault           = false,
    .ui_updated         = false
};

static bool last_conflict_printed = false;
static bool last_fault_printed    = false;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void UART_PrintStatus(const AnkleCtrl_t* ctrl);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static const char* Get_SwitchStateName(ToggleSwitchState_t state) {
    switch (state) {
        case SW_STATE_LEFT_ON:    return "LEFT_ON  (Mode 1)";
        case SW_STATE_RIGHT_ON:   return "RIGHT_ON (Mode 2)";
        case SW_STATE_CENTER_OFF:
        default:                  return "CENTER_OFF (Idle)";
    }
}

static void UART_PrintStatus(const AnkleCtrl_t* ctrl) {
    char buf[160];
    int len = snprintf(buf, sizeof(buf),
                       "[ANKLE UI] Switch: %-17s | Force: %2d.%d kg | Strip: %3d%% (%d/6 LEDs)\r\n",
                       Get_SwitchStateName(ctrl->sw_state),
                       ctrl->raw_value / 2,
                       (ctrl->raw_value % 2) * 5,
                       ctrl->display_percentage,
                       ctrl->active_leds);
    if (len > 0) {
        HAL_UART_Transmit(&huart2, (uint8_t*)buf, (uint16_t)len, 50);
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
  HAL_Init();
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();

  /* USER CODE BEGIN 2 */
  AnkleUI_Init(my_pins);

  const char* banner =
      "\r\n======================================================================\r\n"
      "  Ankle Trainer Controller (Buttons + 3-Way Switch + Status LED + Strip)\r\n"
      "  [Force UP/DOWN] : PC12 / PD2 (Active-Low to GND, 0.0~10.0kg, Step 0.5)\r\n"
      "  [3-Way Switch]  : Left=PC10 | Right=PC11 | Center=OFF (Common to GND)\r\n"
      "  [LED Outputs]   : Power=PA5(LD2) | ForceStatus=PA7(D11) | Strip=PA6(D12)\r\n"
      "======================================================================\r\n";
  HAL_UART_Transmit(&huart2, (uint8_t*)banner, (uint16_t)strlen(banner), 100);

  // 开机立即同步一次拨动开关位置并打印
  AnkleUI_Update(&ankle_ctrl);
  UART_PrintStatus(&ankle_ctrl);
  ankle_ctrl.ui_updated = false;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    AnkleUI_Update(&ankle_ctrl);

    // 防呆报警 1：加减力按钮同时按下
    if (ankle_ctrl.btn_conflict && !last_conflict_printed) {
        const char* msg = "[POKA-YOKE] WARNING: UP & DOWN pressed together! Force locked.\r\n";
        HAL_UART_Transmit(&huart2, (uint8_t*)msg, (uint16_t)strlen(msg), 50);
        last_conflict_printed = true;
    } else if (!ankle_ctrl.btn_conflict) {
        last_conflict_printed = false;
    }

    // 防呆报警 2：三态拨动开关左右两脚同时短路
    if (ankle_ctrl.sw_fault && !last_fault_printed) {
        const char* msg = "[POKA-YOKE] FAULT: Switch LEFT & RIGHT shorted! Fallback to OFF.\r\n";
        HAL_UART_Transmit(&huart2, (uint8_t*)msg, (uint16_t)strlen(msg), 50);
        last_fault_printed = true;
    } else if (!ankle_ctrl.sw_fault) {
        last_fault_printed = false;
    }

    // 无论是拨动了三态开关，还是按按钮改变了力，都实时打印最新一行状态
    if (ankle_ctrl.ui_updated) {
        UART_PrintStatus(&ankle_ctrl);
        ankle_ctrl.ui_updated = false;
    }

    HAL_Delay(1);
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

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

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

void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif /* USE_FULL_ASSERT */
