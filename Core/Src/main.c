#include "main.h"
#include <ssd1306_fonts.h>
#include "ds3231.h"
#include "ssd1306.h"
#include "rc522.h"
#include "servo.h"
#include "we-10.h"
#include "display.h"
#include <stdio.h>
#include <string.h>

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;
SPI_HandleTypeDef hspi2;
TIM_HandleTypeDef htim2;
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_SPI2_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM2_Init(void);

/* USER CODE BEGIN PFP */
/* USER CODE END PFP */


uint8_t totalSlots = 2;
uint8_t slot1 = 0;
uint8_t slot2 = 0;
uint8_t occupiedSlots = 0;
uint8_t availableSlots = 2;
uint8_t previousAvailableSlots = 2;

RTC_TimeTypeDef rtc;
uint8_t TagType[2];
uint8_t UID[5];
uint8_t status;

// Redirect printf to PC Debug UART2
int _write(int file, char *ptr, int len)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)ptr, len, HAL_MAX_DELAY);
    return len;
}

// Entry/exit state now lives on the backend (status + entryTime fields on
// each user record), not in STM32 RAM - so a reset/reboot no longer causes
// the firmware to "forget" who's currently parked. See WE10_CheckUser(),
// current_user_status, and current_user_entry_time in we-10.c.

uint16_t CalculateParkingMinutesBetween(int entry_h, int entry_m, int entry_s,
                                         RTC_TimeTypeDef *exitTime)
{
    long entry_sec = entry_h * 3600L + entry_m * 60L + entry_s;
    long exit_sec  = exitTime->hours * 3600L + exitTime->minutes * 60L + exitTime->seconds;

    long diff_sec = exit_sec - entry_sec;

    // Handle parking that crosses midnight (exit time "wraps" to a smaller
    // value than entry time) by adding a full day's worth of seconds.
    if (diff_sec < 0) {
        diff_sec += 24L * 3600L;
    }

    // Round UP to the nearest whole minute (e.g. 45 seconds parked = 1
    // minute billed), with a 1-minute minimum so a fee is always charged.
    uint16_t minutes = (uint16_t)((diff_sec + 59) / 60);
    if (minutes < 1) minutes = 1;

    return minutes;
}

uint16_t CalculateParkingFee(uint16_t minutes)
{
    return minutes * 5;
}

/**
  * @brief  Shows a warning on the OLED when the wallet balance can't fully
  *         cover the parking fee. The gate still opens (a car shouldn't be
  *         physically trapped over a balance issue) - the shortfall is
  *         recorded as a negative wallet balance, to be settled next
  *         recharge. Uses the raw ssd1306 primitives directly since this
  *         is a one-off screen, not part of display.c's normal screen set.
  */
void LowBalanceWarning(uint16_t fee, int wallet_before)
{
    char line1[24];
    char line2[24];

    ssd1306_Fill(Black);

    ssd1306_SetCursor(2, 2);
    ssd1306_WriteString("LOW BALANCE!", Font_7x10, White);

    snprintf(line1, sizeof(line1), "Fee: Rs %u", fee);
    ssd1306_SetCursor(2, 20);
    ssd1306_WriteString(line1, Font_7x10, White);

    snprintf(line2, sizeof(line2), "Wallet: Rs %d", wallet_before);
    ssd1306_SetCursor(2, 35);
    ssd1306_WriteString(line2, Font_7x10, White);

    ssd1306_SetCursor(2, 50);
    ssd1306_WriteString("Please recharge", Font_7x10, White);

    ssd1306_UpdateScreen();
}

void UpdateParkingSlots(void)
{
    // LOW = Car Present
    slot1 = (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_0) == GPIO_PIN_RESET);
    slot2 = (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_1) == GPIO_PIN_RESET);

    occupiedSlots = 0;
    if(slot1) occupiedSlots++;
    if(slot2) occupiedSlots++;

    availableSlots = totalSlots - occupiedSlots;
}

// Extracted cloud variables from we-10.c
extern char current_user_name[32];
extern char current_user_vehicle[32];
extern int current_user_wallet;
extern char current_user_status[16];
extern char current_user_entry_time[16];

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  HAL_Init();
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_I2C1_Init();

  HAL_StatusTypeDef ret = HAL_I2C_IsDeviceReady(&hi2c1, 0x3C << 1, 2, 100);
  if(ret == HAL_OK) printf("OLED Found\r\n");
  else printf("OLED Not Found\r\n");

  MX_SPI2_Init();
  MX_USART1_UART_Init();

  char testMsg[] = "USART1 TEST\r\n";
  HAL_UART_Transmit(&huart1, (uint8_t *)testMsg, strlen(testMsg), HAL_MAX_DELAY);
  printf("USART1 Test Sent\r\n");

  MX_TIM2_Init();
  printf("Before Servo_Init\r\n");
  Servo_Init();
  printf("Servo OK\r\n");

  ssd1306_Init();
  StartupScreen();
  HAL_Delay(2000);
  HomeScreen(availableSlots);

  RC522_Init();

  if(DS3231_Init() == HAL_OK) {
      printf("RTC Found\r\n");
      // One-time set: fill in the real current time/date, flash once,
      // then remove these two lines and reflash so it doesn't reset every boot.
      // DS3231_SetTime(14, 30, 0);
      // DS3231_SetDate(4, 7, 2026);
  } else {
      printf("RTC Not Found\r\n");
  }

  printf("Connecting to WiFi...\r\n");
  // At the very beginning of WE10_Init:
  HAL_Delay(500);
  WE10_Init("Haiersmart","ayvan3002");
  printf("WiFi Initialization Complete\r\n");

  printf("\r\n=========================================\r\n");
  printf(" SMART PARKING MANAGEMENT SYSTEM\r\n");
  printf("=========================================\r\n");
  printf("System Ready...\r\n");
  printf("Place RFID Card...\r\n");

  // NOTE: entry/exit state is no longer tracked in a local variable here -
  // it's read fresh from the backend on every scan via current_user_status,
  // so a reset can no longer cause a "stuck inside" card to be treated as a
  // fresh entry.
  uint32_t last_firebase_get_tick = 0;

  /* Infinite loop */
  while (1)
  {
      // CRITICAL ASYNC TASK: Process incoming UART bytes from WE10 continuously
      //Firebase_Process_Incoming_Buffer();

      // 1. Update IR Parking Sensors periodically without blocking the loop
      static uint32_t last_sensor_tick = 0;
      if (HAL_GetTick() - last_sensor_tick > 500) {
          last_sensor_tick = HAL_GetTick();
          UpdateParkingSlots();
          if(previousAvailableSlots != availableSlots) {
              HomeScreen(availableSlots);
              previousAvailableSlots = availableSlots;

              // Push the new slot1/slot2 state to the backend so
              // visualization.html can show which slot has a car in it.
              WE10_SyncSlots(slot1, slot2);
          }
      }

      // 3. Non-blocking RFID scan request
      status = RC522_Request(PICC_REQIDL, TagType);
      if(status == MI_OK)
      {
          if(RC522_AntiColl(UID) == MI_OK)
          {
              DS3231_GetTime(&rtc);
              DS3231_GetDate(&rtc);
              RC522_SelectTag(UID);

              char uidString[32];
              sprintf(uidString, "%02X%02X%02X%02X", UID[0], UID[1], UID[2], UID[3]);
              printf("\r\n=========================================\r\n");
              printf("RFID Card Detected: %s\r\n", uidString);
              printf("Time : %02d:%02d:%02d\r\n", rtc.hours, rtc.minutes, rtc.seconds);

              // Perform the profile check on scanned card via synchronous/blocking logic
              if (WE10_CheckUser(uidString))
              {
                  // Decide entry vs exit using the STATUS THE BACKEND JUST
                  // RETURNED (current_user_status), not a local variable.
                  // This is the fix for the "stuck after reset" bug: even if
                  // the STM32 resets mid-session, the backend still
                  // correctly remembers this card is INSIDE, so the very
                  // next scan is correctly treated as an EXIT, not a
                  // duplicate ENTRY.
                  if (strcmp(current_user_status, "INSIDE") != 0)
                  {
                      // ENTRY
                      if (availableSlots == 0) {
                          ParkingFullScreen();
                          HAL_Delay(2000);
                          HomeScreen(availableSlots);
                          continue;
                      }

                      if (strlen(current_user_name) > 0) {
                          WelcomeScreen(current_user_name);
                      } else {
                          WelcomeScreen("USER");
                      }

                      OpenGate();
                      HomeScreen(availableSlots);
                      printf("ENTRY SUCCESSFUL FOR: %s\r\n", current_user_name);

                      // Push the entry to the backend: flip status to INSIDE
                      // and store the entry timestamp THERE (not just in
                      // local RAM), so it survives a reset. Wallet unchanged
                      // on entry, log the ENTRY event.
                      char entryTimeStr[16];
                      snprintf(entryTimeStr, sizeof(entryTimeStr), "%02d:%02d:%02d", rtc.hours, rtc.minutes, rtc.seconds);
                      WE10_SyncStatus(uidString, "INSIDE", current_user_wallet, entryTimeStr);
                      WE10_LogEvent(uidString, "ENTRY", 0);
                  }
                  else
                  {
                      // EXIT - read the entry time back from the backend
                      // (current_user_entry_time), not from local RAM.
                      int entry_h = 0, entry_m = 0, entry_s = 0;
                      uint16_t minutes = 1; // sensible fallback if parsing fails

                      if (strlen(current_user_entry_time) >= 8) {
                          sscanf(current_user_entry_time, "%d:%d:%d", &entry_h, &entry_m, &entry_s);
                          minutes = CalculateParkingMinutesBetween(entry_h, entry_m, entry_s, &rtc);
                      } else {
                          printf("WARNING: No entryTime on backend for %s, using 1-minute minimum fee.\r\n", uidString);
                      }

                      uint16_t fee = CalculateParkingFee(minutes);
                      int new_wallet = current_user_wallet - fee;

                      printf("Parking Duration : %d Minutes\r\n", minutes);
                      printf("Parking Fee : Rs %d\r\n", fee);

                      if (new_wallet < 0) {
                          printf("WARNING: Insufficient balance! Wallet will go negative.\r\n");
                          LowBalanceWarning(fee, current_user_wallet);
                          HAL_Delay(2500);
                      }

                      FeeScreen(fee);
                      OpenGate();
                      ThankYouScreen();
                      HAL_Delay(2000);
                      HomeScreen(availableSlots);
                      printf("EXIT SUCCESSFUL\r\n");

                      // Push the exit to the backend: flip status back to
                      // OUTSIDE, deduct the fee from wallet, clear entryTime,
                      // and log EXIT with the fee amount so history.html shows it.
                      WE10_SyncStatus(uidString, "OUTSIDE", new_wallet, NULL);
                      WE10_LogEvent(uidString, "EXIT", fee);
                  }
              }
              else
              {
                  printf("Unknown Card Structure\r\n");
                  AccessDeniedScreen();
                  HomeScreen(availableSlots);
              }

              printf("-----------------------------------------\r\n");
              printf("Waiting for Next Card...\r\n");
              RC522_Halt();
              RC522_StopCrypto();
              HAL_Delay(1500); // Debounce card presence
          }
      }
  }
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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_I2C1_Init(void)
{
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_SPI2_Init(void)
{
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_TIM2_Init(void)
{
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 15;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 19999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  HAL_TIM_MspPostInit(&htim2);
}

static void MX_USART1_UART_Init(void)
{
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 38400;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_USART2_UART_Init(void)
{
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1|GPIO_PIN_4|LD2_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_4|LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif /* USE_FULL_ASSERT */
