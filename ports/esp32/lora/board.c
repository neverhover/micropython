/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    (C)2013 Semtech

Description: Target board general functions implementation

License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainer: Miguel Luis and Gregory Cristian
*/
#include "board.h"
#include <esp_system.h>
/*!
 * Flag to indicate if the MCU is Initialized
 */
static bool McuInitialized = false;
static uint8_t BatteryLevel = 255;

void BoardInitPeriph( void )
{

}

void BoardInitMcu( void )
{
    if( McuInitialized == false )
    {
        printf("Spi init\n");
        SpiInit( &SX1276.Spi, RADIO_MOSI, RADIO_MISO, RADIO_SCLK, RADIO_NSS );
        printf("SX1276IoInit\n");
        SX1276IoInit( );
        TimerHwInit();
        printf("Lora version is 0x%x\n",SX1276Read(0x42));
        McuInitialized = true;
    }
}

void BoardDeInitMcu( void )
{
#if defined(LOPY) || defined (FIPY)
    SpiDeInit( &SX1272.Spi );
    SX1272IoDeInit( );
#elif defined(LOPY4)
    SpiDeInit( &SX1276.Spi );
    SX1276IoDeInit( );
#endif

    McuInitialized = false;
}

uint32_t BoardGetRandomSeed( void )
{
    return esp_random();
}

void BoardGetUniqueId( uint8_t *id )
{
    for (int i = 0; i < 8; i++) {
        id[i] = i;
    }
}
uint8_t BoardGetBatteryLevel( void )
{
    return BatteryLevel;
}

void BoardSetBatteryLevel( uint8_t level )
{
    BatteryLevel = level;
}
