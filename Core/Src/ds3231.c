#include "ds3231.h"

extern I2C_HandleTypeDef hi2c1;

static uint8_t DecToBCD(uint8_t value)
{
    return ((value / 10) << 4) | (value % 10);
}

static uint8_t BCDToDec(uint8_t value)
{
    return ((value >> 4) * 10) + (value & 0x0F);
}

HAL_StatusTypeDef DS3231_Init(void)
{
    return HAL_I2C_IsDeviceReady(&hi2c1,
                                 DS3231_ADDR,
                                 5,
                                 1000);
}

HAL_StatusTypeDef DS3231_SetTime(uint8_t hour,
                                 uint8_t minute,
                                 uint8_t second)
{
    uint8_t data[4];

    data[0] = 0x00;
    data[1] = DecToBCD(second);
    data[2] = DecToBCD(minute);
    data[3] = DecToBCD(hour);

    return HAL_I2C_Master_Transmit(&hi2c1,
                                   DS3231_ADDR,
                                   data,
                                   4,
                                   1000);
}

HAL_StatusTypeDef DS3231_GetTime(RTC_TimeTypeDef *rtc)
{
    uint8_t reg = 0x00;
    uint8_t data[3];
    HAL_StatusTypeDef status;

    status = HAL_I2C_Master_Transmit(&hi2c1, DS3231_ADDR, &reg, 1, 1000);
    if (status != HAL_OK) return status;

    status = HAL_I2C_Master_Receive(&hi2c1, DS3231_ADDR, data, 3, 1000);
    if (status != HAL_OK) return status;

    // Bit 7 of the seconds register is the Clock Halt (CH) bit, not part of
    // the actual seconds value. If it's set, the oscillator has stopped -
    // usually because the backup battery is dead/missing/miswired. Mask it
    // off so a halted clock decodes as 0, not a garbage value like 80.
    uint8_t ch_bit = data[0] & 0x80;
    rtc->seconds = BCDToDec(data[0] & 0x7F);
    rtc->minutes = BCDToDec(data[1]);
    rtc->hours   = BCDToDec(data[2] & 0x3F);

    if (ch_bit) {
        // Oscillator is halted - restart it by writing seconds back with
        // CH cleared. Time will still be wrong until DS3231_SetTime() is
        // called with a real value, but at least it will start counting.
        uint8_t clear_ch[2] = { 0x00, data[0] & 0x7F };
        HAL_I2C_Master_Transmit(&hi2c1, DS3231_ADDR, clear_ch, 2, 1000);
    }

    return HAL_OK;
}

HAL_StatusTypeDef DS3231_SetDate(uint8_t date,
                                 uint8_t month,
                                 uint16_t year)
{
    uint8_t data[4];

    data[0] = 0x04;
    data[1] = DecToBCD(date);
    data[2] = DecToBCD(month);
    data[3] = DecToBCD(year % 100);

    return HAL_I2C_Master_Transmit(&hi2c1,
                                   DS3231_ADDR,
                                   data,
                                   4,
                                   1000);
}

HAL_StatusTypeDef DS3231_GetDate(RTC_TimeTypeDef *rtc)
{
    uint8_t reg = 0x04;
    uint8_t data[3];
    HAL_StatusTypeDef status;

    status = HAL_I2C_Master_Transmit(&hi2c1, DS3231_ADDR, &reg, 1, 1000);
    if (status != HAL_OK) return status;

    status = HAL_I2C_Master_Receive(&hi2c1, DS3231_ADDR, data, 3, 1000);
    if (status != HAL_OK) return status;

    rtc->date = BCDToDec(data[0]);
    rtc->month = BCDToDec(data[1] & 0x1F);
    rtc->year = 2000 + BCDToDec(data[2]);

    return HAL_OK;
}
