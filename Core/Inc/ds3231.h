#ifndef __DS3231_H__
#define __DS3231_H__

#include "main.h"

#define DS3231_ADDR    (0x68 << 1)

typedef struct
{
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;

    uint8_t date;
    uint8_t month;
    uint16_t year;

} RTC_TimeTypeDef;

HAL_StatusTypeDef DS3231_Init(void);

HAL_StatusTypeDef DS3231_SetTime(uint8_t hour,
                                 uint8_t minute,
                                 uint8_t second);

HAL_StatusTypeDef DS3231_GetTime(RTC_TimeTypeDef *rtc);

HAL_StatusTypeDef DS3231_SetDate(uint8_t date,
                                 uint8_t month,
                                 uint16_t year);

HAL_StatusTypeDef DS3231_GetDate(RTC_TimeTypeDef *rtc);

#endif
