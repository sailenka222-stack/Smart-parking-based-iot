#ifndef DISPLAY_H_
#define DISPLAY_H_

#include "main.h"

void StartupScreen(void);
void HomeScreen(uint8_t availableSlots);
void WelcomeScreen(char *name);
void GateOpenScreen(void);
void GateCountdownScreen(uint8_t sec);
void ThankYouScreen(void);
void FeeScreen(uint16_t fee);
void AccessDeniedScreen(void);
void ParkingFullScreen(void);

#endif
