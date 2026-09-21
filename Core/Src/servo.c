#include "servo.h"
#include "ssd1306.h"
#include "display.h"
#include "display.h"

#define BUZZER_ON()  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET)
#define BUZZER_OFF() HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET)

#include "ssd1306_fonts.h"
#include <stdio.h>

extern TIM_HandleTypeDef htim2;

void Servo_Init(void)
{
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);

    // Servo initially closed
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 200);
}

void Servo_OpenSlow(void)
{
    for(int pos=500; pos<=1500; pos+=10)
    {
        __HAL_TIM_SET_COMPARE(&htim2,TIM_CHANNEL_1,pos);

        BUZZER_ON();
        HAL_Delay(50);

        BUZZER_OFF();
        HAL_Delay(50);
    }

    BUZZER_OFF();
}

void Servo_CloseSlow(void)
{
    for(int pos=1500; pos>=500; pos-=10)
    {
        __HAL_TIM_SET_COMPARE(&htim2,TIM_CHANNEL_1,pos);

        BUZZER_ON();
        HAL_Delay(50);

        BUZZER_OFF();
        HAL_Delay(50);
    }

    BUZZER_OFF();
}
void OpenGate(void)
{
    // Display gate opening
    GateOpenScreen();

    // Open the servo
    Servo_OpenSlow();

    HAL_Delay(1000);

    // Countdown
    for(int i = 9; i >= 1; i--)
    {
        GateCountdownScreen(i);
        HAL_Delay(1000);
    }

    // Close the gate
    Servo_CloseSlow();

    HAL_Delay(500);
}
