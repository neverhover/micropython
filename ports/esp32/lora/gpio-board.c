/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    (C)2013 Semtech

Description: Bleeper board GPIO driver implementation

License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainer: Miguel Luis and Gregory Cristian
*/
#include "board.h"
#include "gpio-board.h"

void GpioMcuInit( Gpio_t *obj, PinNames pin, PinModes mode, PinConfigs config, PinTypes type, uint32_t value )
{

    gpio_pad_select_gpio(obj->port);
    gpio_set_level(obj->port, value);
    // configure mode
    // configure pull
    gpio_config_t gpioConfig;
    gpioConfig.pin_bit_mask = 1 << obj->port;
    gpioConfig.mode = mode;
    gpioConfig.pull_up_en = (type  == PIN_PULL_UP) ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
    gpioConfig.pull_down_en = (type == PIN_PULL_DOWN) ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE;
    gpioConfig.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&gpioConfig);
}

void GpioMcuSetInterrupt( Gpio_t *obj, IrqModes irqMode, IrqPriorities irqPriority, GpioIrqHandler *irqHandler )
{
    gpio_isr_handler_remove(obj->port);
    gpio_set_intr_type(obj->port, irqMode);
    gpio_isr_handler_add(obj->port, (void*)irqHandler, (void*)obj->port);
}

void GpioMcuRemoveInterrupt( Gpio_t *obj )
{
	gpio_intr_disable(obj->port);
    gpio_isr_handler_remove(obj->port);
}

void GpioMcuWrite( Gpio_t *obj, uint32_t value )
{
    gpio_set_level(obj->port, value);
}

void GpioMcuToggle( Gpio_t *obj )
{
    uint32_t val = gpio_get_level(obj->port);
    GpioMcuWrite(obj, !val);
}

uint32_t GpioMcuRead( Gpio_t *obj )
{
    return gpio_get_level(obj->port);
}
