/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    (C)2013 Semtech

Description: Bleeper STM32L151RD microcontroller pins definition

License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainer: Miguel Luis and Gregory Cristian
*/
#ifndef __PIN_NAME_MCU_H__
#define __PIN_NAME_MCU_H__

/*!
 * STM32 Pin Names
 */
#define MCU_PINS \
    MICROPY_HW_SPI1_SCK=5,MICROPY_HW_SPI1_RESET=14,\
    MICROPY_HW_SPI1_NSS=18,MICROPY_HW_SPI1_MISO=19, MICROPY_HW_SPI1_MOSI=27, \
    MICROPY_HW_LORA_DIO0=26, NBR_GP_PINS

#endif // __PIN_NAME_MCU_H__
