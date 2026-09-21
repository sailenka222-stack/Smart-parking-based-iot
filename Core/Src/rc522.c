#include "rc522.h"

extern SPI_HandleTypeDef hspi2;

/************************************************
            CHIP SELECT FUNCTIONS
*************************************************/

void RC522_Select(void)
{
    HAL_GPIO_WritePin(RC522_CS_PORT, RC522_CS_PIN, GPIO_PIN_RESET);
}

void RC522_Unselect(void)
{
    HAL_GPIO_WritePin(RC522_CS_PORT, RC522_CS_PIN, GPIO_PIN_SET);
}

/************************************************
                RESET
*************************************************/

void RC522_Reset(void)
{
    HAL_GPIO_WritePin(RC522_RST_PORT, RC522_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(10);

    HAL_GPIO_WritePin(RC522_RST_PORT, RC522_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(10);
}

/************************************************
            WRITE REGISTER
*************************************************/

void RC522_WriteRegister(uint8_t reg, uint8_t value)
{
    uint8_t tx[2];

    tx[0] = (reg << 1) & 0x7E;
    tx[1] = value;

    RC522_Select();

    HAL_SPI_Transmit(&hspi2, tx, 2, HAL_MAX_DELAY);

    RC522_Unselect();
}

/************************************************
            READ REGISTER
*************************************************/

uint8_t RC522_ReadRegister(uint8_t reg)
{
    uint8_t tx[2];
    uint8_t rx[2];

    tx[0] = ((reg << 1) & 0x7E) | 0x80;
    tx[1] = 0x00;

    RC522_Select();

    HAL_SPI_TransmitReceive(&hspi2, tx, rx, 2, HAL_MAX_DELAY);

    RC522_Unselect();

    return rx[1];
}

/************************************************
            SET BIT MASK
*************************************************/

void RC522_SetBitMask(uint8_t reg, uint8_t mask)
{
    uint8_t tmp;

    tmp = RC522_ReadRegister(reg);

    RC522_WriteRegister(reg, tmp | mask);
}

/************************************************
            CLEAR BIT MASK
*************************************************/

void RC522_ClearBitMask(uint8_t reg, uint8_t mask)
{
    uint8_t tmp;

    tmp = RC522_ReadRegister(reg);

    RC522_WriteRegister(reg, tmp & (~mask));
}

/************************************************
            ANTENNA ON
*************************************************/

void RC522_AntennaOn(void)
{
    uint8_t temp;

    temp = RC522_ReadRegister(TxControlReg);

    if(!(temp & 0x03))
    {
        RC522_SetBitMask(TxControlReg,0x03);
    }
}

/************************************************
            ANTENNA OFF
*************************************************/

void RC522_AntennaOff(void)
{
    RC522_ClearBitMask(TxControlReg,0x03);
}

/************************************************
            INITIALIZE RC522
*************************************************/

void RC522_Init(void)
{
    RC522_Reset();

    RC522_WriteRegister(CommandReg, PCD_SOFTRESET);

    HAL_Delay(50);

    RC522_WriteRegister(TModeReg,0x8D);
    RC522_WriteRegister(TPrescalerReg,0x3E);
    RC522_WriteRegister(TReloadRegL,30);
    RC522_WriteRegister(TReloadRegH,0);

    RC522_WriteRegister(TxASKReg,0x40);

    RC522_WriteRegister(ModeReg,0x3D);

    RC522_AntennaOn();
}
/************************************************
        COMMUNICATE WITH CARD
*************************************************/

uint8_t RC522_ToCard(uint8_t command,
                     uint8_t *sendData,
                     uint8_t sendLen,
                     uint8_t *backData,
                     uint16_t *backLen)
{
    uint8_t status = MI_ERR;
    uint8_t irqEn = 0x00;
    uint8_t waitIRq = 0x00;
    uint8_t lastBits;
    uint8_t n;
    uint16_t i;

    if(command == PCD_MFAUTHENT)
    {
        irqEn = 0x12;
        waitIRq = 0x10;
    }
    else if(command == PCD_TRANSCEIVE)
    {
        irqEn = 0x77;
        waitIRq = 0x30;
    }

    RC522_WriteRegister(CommIEnReg, irqEn | 0x80);
    RC522_ClearBitMask(CommIrqReg, 0x80);
    RC522_SetBitMask(FIFOLevelReg, 0x80);

    RC522_WriteRegister(CommandReg, PCD_IDLE);

    for(i=0;i<sendLen;i++)
    {
        RC522_WriteRegister(FIFODataReg, sendData[i]);
    }

    RC522_WriteRegister(CommandReg, command);

    if(command == PCD_TRANSCEIVE)
    {
        RC522_SetBitMask(BitFramingReg,0x80);
    }

    i = 2000;

    do
    {
        n = RC522_ReadRegister(CommIrqReg);
        i--;
    }
    while((i!=0) && !(n&0x01) && !(n&waitIRq));

    RC522_ClearBitMask(BitFramingReg,0x80);

    if(i!=0)
    {
        if((RC522_ReadRegister(ErrorReg)&0x1B)==0x00)
        {
            status = MI_OK;

            if(n & irqEn & 0x01)
            {
                status = MI_NOTAGERR;
            }

            if(command==PCD_TRANSCEIVE)
            {
                n = RC522_ReadRegister(FIFOLevelReg);
                lastBits = RC522_ReadRegister(ControlReg)&0x07;

                if(lastBits)
                    *backLen=(n-1)*8+lastBits;
                else
                    *backLen=n*8;

                if(n==0)
                    n=1;

                if(n>16)
                    n=16;

                for(i=0;i<n;i++)
                {
                    backData[i]=RC522_ReadRegister(FIFODataReg);
                }
            }
        }
    }

    return status;
}
uint8_t RC522_Request(uint8_t reqMode,uint8_t *TagType)
{
    uint8_t status;
    uint16_t backBits;

    RC522_WriteRegister(BitFramingReg,0x07);

    TagType[0]=reqMode;

    status=RC522_ToCard(PCD_TRANSCEIVE,
                        TagType,
                        1,
                        TagType,
                        &backBits);

    if((status!=MI_OK)||(backBits!=0x10))
    {
        status=MI_ERR;
    }

    return status;
}
uint8_t RC522_AntiColl(uint8_t *serNum)
{
    uint8_t status;
    uint8_t i;
    uint8_t serNumCheck=0;
    uint16_t unLen;

    RC522_WriteRegister(BitFramingReg,0x00);

    serNum[0]=PICC_ANTICOLL;
    serNum[1]=0x20;

    status=RC522_ToCard(PCD_TRANSCEIVE,
                        serNum,
                        2,
                        serNum,
                        &unLen);

    if(status==MI_OK)
    {
        for(i=0;i<4;i++)
        {
            serNumCheck ^= serNum[i];
        }

        if(serNumCheck!=serNum[4])
        {
            status=MI_ERR;
        }
    }

    return status;
}
void RC522_CalculateCRC(uint8_t *data, uint8_t len, uint8_t *result)
{
    uint8_t i,n;

    RC522_ClearBitMask(DivIrqReg,0x04);
    RC522_SetBitMask(FIFOLevelReg,0x80);

    for(i=0;i<len;i++)
    {
        RC522_WriteRegister(FIFODataReg,data[i]);
    }

    RC522_WriteRegister(CommandReg,PCD_CALCCRC);

    i=0xFF;

    do
    {
        n=RC522_ReadRegister(DivIrqReg);
        i--;
    }
    while((i!=0) && !(n&0x04));

    result[0]=RC522_ReadRegister(CRCResultRegL);
    result[1]=RC522_ReadRegister(CRCResultRegH);
}
uint8_t RC522_SelectTag(uint8_t *serNum)
{
    uint8_t i;
    uint8_t status;
    uint8_t size;
    uint16_t recvBits;

    uint8_t buffer[9];

    buffer[0]=PICC_SELECTTAG;
    buffer[1]=0x70;

    for(i=0;i<5;i++)
        buffer[i+2]=serNum[i];

    RC522_CalculateCRC(buffer,7,&buffer[7]);

    status=RC522_ToCard(PCD_TRANSCEIVE,
                        buffer,
                        9,
                        buffer,
                        &recvBits);

    if((status==MI_OK)&&(recvBits==0x18))
        size=buffer[0];
    else
        size=0;

    return size;
}
void RC522_Halt(void)
{
    uint16_t unLen;

    uint8_t buffer[4];

    buffer[0]=PICC_HALT;
    buffer[1]=0;

    RC522_CalculateCRC(buffer,2,&buffer[2]);

    RC522_ToCard(PCD_TRANSCEIVE,
                 buffer,
                 4,
                 buffer,
                 &unLen);
}
void RC522_StopCrypto(void)
{
    RC522_ClearBitMask(Status2Reg,0x08);
}
uint8_t RC522_Auth(uint8_t authMode,
                   uint8_t blockAddr,
                   uint8_t *sectorKey,
                   uint8_t *serNum)
{
    uint8_t status;
    uint16_t recvBits;
    uint8_t i;

    uint8_t buffer[12];

    buffer[0] = authMode;
    buffer[1] = blockAddr;

    for(i=0;i<6;i++)
        buffer[i+2]=sectorKey[i];

    for(i=0;i<4;i++)
        buffer[i+8]=serNum[i];

    status = RC522_ToCard(PCD_MFAUTHENT,
                          buffer,
                          12,
                          buffer,
                          &recvBits);

    if((status != MI_OK) ||
       (!(RC522_ReadRegister(Status2Reg)&0x08)))
    {
        status = MI_ERR;
    }

    return status;
}
uint8_t RC522_Read(uint8_t blockAddr,
                   uint8_t *recvData)
{
    uint8_t status;
    uint16_t unLen;

    recvData[0]=PICC_READ;
    recvData[1]=blockAddr;

    RC522_CalculateCRC(recvData,2,&recvData[2]);

    status=RC522_ToCard(PCD_TRANSCEIVE,
                        recvData,
                        4,
                        recvData,
                        &unLen);

    if((status!=MI_OK)||(unLen!=0x90))
        status=MI_ERR;

    return status;
}
uint8_t RC522_Write(uint8_t blockAddr,
                    uint8_t *writeData)
{
    uint8_t status;
    uint16_t recvBits;
    uint8_t i;

    uint8_t buffer[18];

    buffer[0]=PICC_WRITE;
    buffer[1]=blockAddr;

    RC522_CalculateCRC(buffer,2,&buffer[2]);

    status=RC522_ToCard(PCD_TRANSCEIVE,
                        buffer,
                        4,
                        buffer,
                        &recvBits);

    if((status!=MI_OK) ||
       ((recvBits!=4) ||
       ((buffer[0]&0x0F)!=0x0A)))
    {
        status=MI_ERR;
    }

    if(status==MI_OK)
    {
        for(i=0;i<16;i++)
            buffer[i]=writeData[i];

        RC522_CalculateCRC(buffer,16,&buffer[16]);

        status=RC522_ToCard(PCD_TRANSCEIVE,
                            buffer,
                            18,
                            buffer,
                            &recvBits);

        if((status!=MI_OK) ||
           ((recvBits!=4) ||
           ((buffer[0]&0x0F)!=0x0A)))
        {
            status=MI_ERR;
        }
    }

    return status;
}
