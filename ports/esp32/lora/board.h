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
#ifndef __BOARD_H__
#define __BOARD_H__

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "utilities.h"
#include "timer.h"
#include "delay.h"
#include "gpio.h"
#include "spi.h"
#include "radio.h"
#include "sx1276/sx1276.h"
#include "timer-board.h"
#include "sx1276-board.h"
#include "mpconfigport.h"
#include "driver/gpio.h"
#include "LoRaMac.h"

#define REMOVED_BY_LEYIZHANG 0
/*!
 * Generic definition
 */
#ifndef SUCCESS
#define SUCCESS                                     1
#endif

#ifndef FAIL
#define FAIL                                        0
#endif


/*!
 * Board MCU pins definitions
 */

#define RADIO_RESET                                 MICROPY_HW_SPI1_RESET

#define RADIO_MOSI                                  MICROPY_HW_SPI1_MOSI
#define RADIO_MISO                                  MICROPY_HW_SPI1_MISO
#define RADIO_SCLK                                  MICROPY_HW_SPI1_SCK
#define RADIO_NSS                                   MICROPY_HW_SPI1_NSS

#define RADIO_DIO_0                                 MICROPY_HW_LORA_DIO0
#define RADIO_DIO_1                                 MICROPY_HW_LORA_DIO1
#define RADIO_DIO_2                                 MICROPY_HW_LORA_DIO2
#define RADIO_DIO_3                                 MICROPY_HW_LORA_DIO3
#define RADIO_DIO_4                                 MICROPY_HW_LORA_DIO4
#define RADIO_DIO_5                                 MICROPY_HW_LORA_DIO5

void BoardInitPeriph( void );

/*!
 * \brief Initializes the target board peripherals.
 */
void BoardInitMcu( void );
/*!
 * \brief De-initializes the target board peripherals to decrease power
 *        consumption.
 */
void BoardDeInitMcu( void );

/*!
 * \brief Gets the board 64 bits unique ID
 *
 * \param [IN] id Pointer to an array that will contain the Unique ID
 */
void BoardGetUniqueId( uint8_t *id );

uint8_t BoardGetBatteryLevel( void );

void BoardSetBatteryLevel( uint8_t level );

#endif // __BOARD_H__
