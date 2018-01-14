/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    (C)2013 Semtech

Description: Bleeper board SPI driver implementation

License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainer: Miguel Luis and Gregory Cristian
*/
#include "board.h"
#include "spi-board.h"
#include "gpio-board.h"
#include "driver/spi_master.h"
/*!
 * MCU SPI peripherals enumeration
 */

static uint32_t baudrate_to_delay_half(uint32_t baudrate) {
    #ifdef MICROPY_PY_MACHINE_SPI_MIN_DELAY
    if (baudrate >= MICROPY_PY_MACHINE_SPI_MAX_BAUDRATE) {
        return MICROPY_PY_MACHINE_SPI_MIN_DELAY;
    } else
    #endif
    {
        uint32_t delay_half = 500000 / baudrate;
        // round delay_half up so that: actual_baudrate <= requested_baudrate
        if (500000 % baudrate != 0) {
            delay_half += 1;
        }
        return delay_half;
    }
}

void SpiInit( Spi_t *obj, PinNames mosi, PinNames miso, PinNames sclk, PinNames nss )
{
    // // assign the pins
    obj->Miso.port = miso;
    obj->Mosi.port = mosi;
    obj->Sclk.port = sclk;
    obj->Nss.port = nss;
    obj->delay_half = baudrate_to_delay_half(10000000);
    obj->polarity = 0;
    obj->phase = 0;

    // configure pins
    GpioMcuWrite(&obj->Sclk, obj->polarity);
    // mp_hal_pin_output(self->sck);
    gpio_pad_select_gpio(obj->Sclk.port);
    gpio_set_direction(obj->Sclk.port, GPIO_MODE_INPUT_OUTPUT);
    
    // mp_hal_pin_output(self->mosi);
    gpio_pad_select_gpio(obj->Mosi.port);
    gpio_set_direction(obj->Mosi.port, GPIO_MODE_INPUT_OUTPUT);
    // mp_hal_pin_input(self->miso);
    gpio_pad_select_gpio(obj->Miso.port);
    gpio_set_direction(obj->Miso.port, GPIO_MODE_INPUT);
    // configure nss


}

void SpiDeInit( Spi_t *obj )
{
    // disable the peripheral
}

void SpiFormat( Spi_t *obj, int8_t bits, int8_t cpol, int8_t cpha, int8_t slave )
{
    // configure the interface (only master mode supported)
}

/*!
 * \brief Sets the SPI speed
 *
 * \param [IN] obj SPI object
 * \param [IN] hz  SPI clock frequency in hz
 */
void SpiFrequency( Spi_t *obj, uint32_t hz ) {
    // configure the interface (only master mode supported)
}

uint16_t SpiInOut( Spi_t *obj, uint16_t outData )
{
    uint32_t delay_half = obj->delay_half;
    GpioMcuWrite(&obj->Nss, 0);
    
    uint8_t data_out = outData & 0xff;
    uint8_t data_in = 0;
    for (int j = 0; j < 8; ++j, data_out <<= 1) {
        GpioMcuWrite(&obj->Mosi, (data_out >> 7) & 1);
        if (obj->phase == 0) {
            ets_delay_us(delay_half);
            GpioMcuWrite(&obj->Sclk, 1 - obj->polarity);
        } else {
            GpioMcuWrite(&obj->Sclk, 1 - obj->polarity);
            ets_delay_us(delay_half);
        }
        data_in = (data_in << 1) | GpioMcuRead(&obj->Miso);
        if (obj->phase == 0) {
            ets_delay_us(delay_half);
            GpioMcuWrite(&obj->Sclk, obj->polarity);
        } else {
            GpioMcuWrite(&obj->Sclk, obj->polarity);
            ets_delay_us(delay_half);
        }
    }

    GpioMcuWrite(&obj->Nss, 1);
    if (outData == 0) {
        return data_in;
    }

    return 0;
}

