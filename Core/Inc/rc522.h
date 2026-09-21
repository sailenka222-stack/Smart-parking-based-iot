#ifndef __RC522_H
#define __RC522_H

#include "main.h"
#include <stdint.h>

/*==========================
        RC522 PINS
==========================*/
#define RC522_CS_PORT GPIOA
#define RC522_CS_PIN  GPIO_PIN_4

#define RC522_RST_PORT GPIOA
#define RC522_RST_PIN  GPIO_PIN_1

extern SPI_HandleTypeDef hspi2;

/*==========================
      RC522 REGISTERS
==========================*/

#define CommandReg         0x01
#define CommIEnReg         0x02
#define DivIEnReg          0x03
#define CommIrqReg         0x04
#define DivIrqReg          0x05
#define ErrorReg           0x06
#define Status1Reg         0x07
#define Status2Reg         0x08
#define FIFODataReg        0x09
#define FIFOLevelReg       0x0A
#define WaterLevelReg      0x0B
#define ControlReg         0x0C
#define BitFramingReg      0x0D
#define CollReg            0x0E

#define ModeReg            0x11
#define TxModeReg          0x12
#define RxModeReg          0x13
#define TxControlReg       0x14
#define TxASKReg           0x15
#define CRCResultRegH      0x21
#define CRCResultRegL      0x22
#define TModeReg           0x2A
#define TPrescalerReg      0x2B
#define TReloadRegH        0x2C
#define TReloadRegL        0x2D
#define VersionReg         0x37

/*==========================
       COMMANDS
==========================*/

#define PCD_IDLE           0x00
#define PCD_MEM            0x01
#define PCD_GENERATE_RANDOM_ID 0x02
#define PCD_CALCCRC        0x03
#define PCD_TRANSMIT       0x04
#define PCD_NOCMDCHANGE    0x07
#define PCD_RECEIVE        0x08
#define PCD_TRANSCEIVE     0x0C
#define PCD_MFAUTHENT      0x0E
#define PCD_SOFTRESET      0x0F

/*==========================
      PICC COMMANDS
==========================*/

#define PICC_REQIDL        0x26
#define PICC_REQALL        0x52
#define PICC_ANTICOLL      0x93
#define PICC_SELECTTAG     0x93
#define PICC_AUTHENT1A     0x60
#define PICC_AUTHENT1B     0x61
#define PICC_READ          0x30
#define PICC_WRITE         0xA0
#define PICC_DECREMENT     0xC0
#define PICC_INCREMENT     0xC1
#define PICC_RESTORE       0xC2
#define PICC_TRANSFER      0xB0
#define PICC_HALT          0x50

/*==========================
       STATUS
==========================*/

#define MI_OK              0
#define MI_NOTAGERR        1
#define MI_ERR             2

/*==========================
     FUNCTION PROTOTYPES
==========================*/

void RC522_Select(void);
void RC522_Unselect(void);
void RC522_Reset(void);

void RC522_WriteRegister(uint8_t reg, uint8_t value);
uint8_t RC522_ReadRegister(uint8_t reg);

void RC522_SetBitMask(uint8_t reg, uint8_t mask);
void RC522_ClearBitMask(uint8_t reg, uint8_t mask);

void RC522_AntennaOn(void);
void RC522_AntennaOff(void);

void RC522_Init(void);

uint8_t RC522_Request(uint8_t reqMode, uint8_t *TagType);
uint8_t RC522_AntiColl(uint8_t *serNum);
uint8_t RC522_ToCard(uint8_t command,
                     uint8_t *sendData,
                     uint8_t sendLen,
                     uint8_t *backData,
                     uint16_t *backLen);
void RC522_CalculateCRC(uint8_t *data, uint8_t len, uint8_t *result);

uint8_t RC522_SelectTag(uint8_t *serNum);

void RC522_Halt(void);

void RC522_StopCrypto(void);
uint8_t RC522_Auth(uint8_t authMode,
                   uint8_t blockAddr,
                   uint8_t *sectorKey,
                   uint8_t *serNum);

uint8_t RC522_Read(uint8_t blockAddr,
                   uint8_t *recvData);

uint8_t RC522_Write(uint8_t blockAddr,
                    uint8_t *writeData);

#endif
