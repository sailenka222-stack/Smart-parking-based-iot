#include "display.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include <stdio.h>

void HomeScreen(uint8_t availableSlots)
{
    char buf[10];

    ssd1306_Fill(Black);

    ssd1306_SetCursor(8,0);
    ssd1306_WriteString("SMART PARKING", Font_7x10, White);

    ssd1306_SetCursor(15,18);
    ssd1306_WriteString("Scan RFID Card", Font_7x10, White);

    sprintf(buf,"%d/2",availableSlots);

    ssd1306_SetCursor(5,45);
    ssd1306_WriteString("Available :", Font_7x10, White);

    ssd1306_SetCursor(90,45);
    ssd1306_WriteString(buf, Font_7x10, White);

    ssd1306_UpdateScreen();
}
void WelcomeScreen(char *name)
{
    ssd1306_Fill(Black);

    ssd1306_SetCursor(20,5);
    ssd1306_WriteString("WELCOME", Font_11x18, White);

    ssd1306_SetCursor(25,35);
    ssd1306_WriteString(name, Font_7x10, White);

    ssd1306_UpdateScreen();

    HAL_Delay(2000);
}
void StartupScreen(void)
{
    ssd1306_Fill(Black);

    ssd1306_SetCursor(12,12);
    ssd1306_WriteString("SMART PARKING", Font_11x18, White);

    ssd1306_SetCursor(22,40);
    ssd1306_WriteString("System Ready", Font_7x10, White);

    ssd1306_UpdateScreen();
}


void AccessDeniedScreen(void)
{
    ssd1306_Fill(Black);

    ssd1306_SetCursor(15,20);
    ssd1306_WriteString("ACCESS DENIED", Font_7x10, White);

    ssd1306_UpdateScreen();

    HAL_Delay(2000);
}

void ParkingFullScreen(void)
{
    ssd1306_Fill(Black);

    ssd1306_SetCursor(20,15);
    ssd1306_WriteString("PARKING FULL", Font_7x10, White);

    ssd1306_SetCursor(30,35);
    ssd1306_WriteString("NO SLOTS", Font_7x10, White);

    ssd1306_UpdateScreen();

    HAL_Delay(2000);
}

void GateOpenScreen(void)
{
    ssd1306_Fill(Black);

    ssd1306_SetCursor(18,20);
    ssd1306_WriteString("GATE OPENING", Font_7x10, White);

    ssd1306_UpdateScreen();
}
void GateCountdownScreen(uint8_t sec)
{
    char str[3];

    ssd1306_Fill(Black);

    ssd1306_SetCursor(10,5);
    ssd1306_WriteString("GATE CLOSES IN", Font_7x10, White);

    sprintf(str,"%d",sec);

    ssd1306_SetCursor(55,30);
    ssd1306_WriteString(str, Font_16x26, White);

    ssd1306_UpdateScreen();
}
void ThankYouScreen(void)
{
    ssd1306_Fill(Black);

    ssd1306_SetCursor(25,10);
    ssd1306_WriteString("THANK YOU", Font_11x18, White);

    ssd1306_SetCursor(20,40);
    ssd1306_WriteString("Visit Again", Font_7x10, White);

    ssd1306_UpdateScreen();

    HAL_Delay(2000);
}
void FeeScreen(uint16_t fee)
{
    char buf[20];

    ssd1306_Fill(Black);

    ssd1306_SetCursor(20,5);
    ssd1306_WriteString("Parking Fee", Font_7x10, White);

    sprintf(buf,"Rs %d",fee);

    ssd1306_SetCursor(35,30);
    ssd1306_WriteString(buf, Font_11x18, White);

    ssd1306_UpdateScreen();

    HAL_Delay(2000);
}
