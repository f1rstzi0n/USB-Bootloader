/* 独立裸机 Boot；GPIO 安全初态和时钟沿用目标 12MHz 工程。 */
#include "main.h"
/* USER CODE BEGIN Includes */
#include "board.h"
#include "usb_io.h"
#include "usb_proto.h"
#include "fw_platform.h"
/* USER CODE END Includes */
IWDG_HandleTypeDef hiwdg;
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_IWDG_Init(void);
int main(void)
{
  HAL_Init(); SystemClock_Config(); MX_GPIO_Init(); MX_IWDG_Init();
  /* USER CODE BEGIN 2 */
  usb_io_init(); fw_platform_boot_start();
  if (!usb_io_stack_init()) Error_Handler();
  /* USER CODE END 2 */
  /* USER CODE BEGIN WHILE */
  while (1) {
    /* USER CODE END WHILE */
    /* USER CODE BEGIN 3 */
    usb_io_task(); usb_proto_task(); fw_platform_task(); board_watchdog_refresh();
  }
  /* USER CODE END 3 */
}
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 6;
  RCC_OscInitStruct.PLL.PLLN = 180;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 3;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  HAL_GPIO_WritePin(GPIOE, SPI2_CS_FLASH_Pin | SPI2_CS_MOTOR_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, SPI1_CS_DAC_Pin|LED1_Pin|PFO_O_Pin
                          |DATAOUT_Pin|LED2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, O_LOCK_INTERFACE_OPEN_Pin|DAC_LDAC_Pin|DISPLAY_STCP_Pin|O_LOCK_POWER_OPEN_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, KEY_Pin|SPI3_CS_ADC_Pin|LOCK_POWER_CLOSE_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, CONTROL_Pin|CLKS_Pin|SPI4_CS_EEPROM_Pin|I_DEVICETYPE_3_Pin
                          |I_DEVICETYPE_2_Pin|ARISWITCH_Pin|SYSTEM_LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, O_FAN_Pin|DISPLAY_DS_Pin|LED3_Pin|LED4_Pin
                          |USART1_CTRL_Pin|O_LOCK_INTERFACE_CLOSE_Pin|USART2_CTRL_Pin|SPI1_CS_ECAT_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : SPI1_CS_DAC_Pin O_USB_DP_CTRL_Pin LED1_Pin PFO_O_Pin
                           DATAOUT_Pin SPI2_CS_FLASH_Pin LED2_Pin SPI2_CS_MOTOR_Pin */
  GPIO_InitStruct.Pin = SPI1_CS_DAC_Pin|LED1_Pin|PFO_O_Pin
                          |DATAOUT_Pin|LED2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = O_USB_DP_CTRL_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
  GPIO_InitStruct.Pin = SPI2_CS_FLASH_Pin | SPI2_CS_MOTOR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : MOTOR_EN_SIGNAL_IN_Pin PFO_I_Pin EEPROM_Loaded_Pin */
  GPIO_InitStruct.Pin = MOTOR_EN_SIGNAL_IN_Pin|PFO_I_Pin|EEPROM_Loaded_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : I_LOCK_POWER_OPEN_Pin I_LOCK_POWER_CLOSE_Pin I_LOCK_INTERFACE_CLOSE_Pin I_LOCK_INTERFACE_OPEN_Pin */
  GPIO_InitStruct.Pin = I_LOCK_POWER_OPEN_Pin|I_LOCK_POWER_CLOSE_Pin|I_LOCK_INTERFACE_CLOSE_Pin|I_LOCK_INTERFACE_OPEN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : O_LOCK_INTERFACE_OPEN_Pin DAC_LDAC_Pin DISPLAY_STCP_Pin O_LOCK_POWER_OPEN_Pin */
  GPIO_InitStruct.Pin = O_LOCK_INTERFACE_OPEN_Pin|DAC_LDAC_Pin|DISPLAY_STCP_Pin|O_LOCK_POWER_OPEN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : KEY_Pin SPI3_CS_ADC_Pin LOCK_POWER_CLOSE_Pin */
  GPIO_InitStruct.Pin = KEY_Pin|SPI3_CS_ADC_Pin|LOCK_POWER_CLOSE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : DISPLAY_SHCP_Pin AD_READY_INT_Pin */
  GPIO_InitStruct.Pin = DISPLAY_SHCP_Pin|AD_READY_INT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : CONTROL_Pin CLKS_Pin SPI4_CS_EEPROM_Pin I_DEVICETYPE_3_Pin
                           I_DEVICETYPE_2_Pin ARISWITCH_Pin SYSTEM_LED_Pin */
  GPIO_InitStruct.Pin = CONTROL_Pin|CLKS_Pin|SPI4_CS_EEPROM_Pin|I_DEVICETYPE_3_Pin
                          |I_DEVICETYPE_2_Pin|ARISWITCH_Pin|SYSTEM_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : DATAIN_Pin */
  GPIO_InitStruct.Pin = DATAIN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(DATAIN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : O_FAN_Pin DISPLAY_DS_Pin LED3_Pin LED4_Pin
                           USART1_CTRL_Pin USART2_CTRL_Pin SPI1_CS_ECAT_Pin */
  GPIO_InitStruct.Pin = O_FAN_Pin|DISPLAY_DS_Pin|LED3_Pin|LED4_Pin
                          |USART1_CTRL_Pin|USART2_CTRL_Pin|SPI1_CS_ECAT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin : I_DEVICETYPE_1_Pin */
  GPIO_InitStruct.Pin = I_DEVICETYPE_1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(I_DEVICETYPE_1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : O_LOCK_INTERFACE_CLOSE_Pin */
  GPIO_InitStruct.Pin = O_LOCK_INTERFACE_CLOSE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(O_LOCK_INTERFACE_CLOSE_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : ECAT_SYNCO_INT_Pin */
  GPIO_InitStruct.Pin = ECAT_SYNCO_INT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(ECAT_SYNCO_INT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : INT_ESC_Pin */
  GPIO_InitStruct.Pin = INT_ESC_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(INT_ESC_GPIO_Port, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}
static void MX_IWDG_Init(void)
{

  /* USER CODE BEGIN IWDG_Init 0 */

  /* USER CODE END IWDG_Init 0 */

  /* USER CODE BEGIN IWDG_Init 1 */

  /* USER CODE END IWDG_Init 1 */
  hiwdg.Instance = IWDG;
  hiwdg.Init.Prescaler = IWDG_PRESCALER_32;
  hiwdg.Init.Reload = 4095;
  if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN IWDG_Init 2 */

  /* USER CODE END IWDG_Init 2 */

}
/* USER CODE BEGIN 4 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *timer)
{
  if (timer->Instance == TIM7) HAL_IncTick();
}
/* USER CODE END 4 */
void Error_Handler(void) { __disable_irq(); for (;;) {} }
