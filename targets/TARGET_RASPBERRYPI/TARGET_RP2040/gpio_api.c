#include "mbed_assert.h"
#include "hal/gpio_api.h"
#include "gpio_irq_api.h"
#include "pinmap.h"
#include "mbed_error.h"

#define GPIO_PIN_COUNT 30

static gpio_irq_handler m_irq_handler;
static uint32_t m_channel_ids[GPIO_PIN_COUNT] = {0};
static uint32_t m_pico_events[GPIO_PIN_COUNT] = {0};

void gpio_write(gpio_t *obj, int value)
{
    gpio_put(obj->pin, value);
}

int gpio_read(gpio_t *obj)
{
    return gpio_get(obj->pin);
}

void gpio_init(gpio_t *obj, PinName pin)
{
    obj->pin = pin;

    if (pin == (PinName)NC) {
        return;
    }

    _gpio_init(obj->pin);
}

static uint32_t gpio_convert_event(gpio_irq_event event)
{
    uint32_t irq_event = 0;

    if (event == IRQ_RISE) {
        irq_event = GPIO_IRQ_EDGE_RISE;
    } else if (event == IRQ_FALL) {
        irq_event = GPIO_IRQ_EDGE_FALL;
    }

    return irq_event;
}

void gpio_mode(gpio_t *obj, PinMode mode)
{
    MBED_ASSERT(obj->pin != (PinName)NC);

    gpio_set_pulls(obj->pin, mode == PullUp, mode == PullDown);
}

void gpio_dir(gpio_t *obj, PinDirection direction)
{
    MBED_ASSERT(obj->pin != (PinName)NC);
    obj->direction = direction;
    if (direction == PIN_OUTPUT) {
        gpio_set_dir(obj->pin, GPIO_OUT);
    }
    if (direction == PIN_INPUT) {
        gpio_set_dir(obj->pin, GPIO_IN);
    }
}

int gpio_is_connected(const gpio_t *obj) {
    return (obj->pin == NC ? 0 : 1);
}


/***********
  GPIO IRQ
***********/

#if DEVICE_INTERRUPTIN

static void _gpio_irq(uint gpio, uint32_t events)
{
    gpio_irq_event ev;
    if (events == GPIO_IRQ_EDGE_RISE) {
        ev = IRQ_RISE;
    } else if (events == GPIO_IRQ_EDGE_FALL) {
        ev = IRQ_FALL;
    } else {
        ev = IRQ_NONE;
    }
    m_irq_handler(m_channel_ids[gpio], ev);
}

int gpio_irq_init(gpio_irq_t *obj, PinName pin, gpio_irq_handler handler, uint32_t id)
{
    if (pin == NC) {
        return -1;
    }
    MBED_ASSERT((uint32_t)pin < GPIO_PIN_COUNT);

    m_channel_ids[pin]          = id;
    m_irq_handler               = handler;

    obj->irq_n = IO_IRQ_BANK0;
    obj->pin = pin;
    obj->irq_index = id;

    return 0;
}


void gpio_irq_free(gpio_irq_t *obj)
{
    gpio_irq_disable(obj);
    obj->irq_n = 0;
    obj->pin = 0;
    obj->irq_index = 0;
}


void gpio_irq_set(gpio_irq_t *obj, gpio_irq_event event, uint32_t enable)
{
    uint32_t irq_event = gpio_convert_event(event);

    if (enable) {
        m_pico_events[obj->pin] |= irq_event;
        obj->event = irq_event;
        gpio_irq_enable(obj);
    }
}


void gpio_irq_enable(gpio_irq_t *obj)
{
    gpio_set_irq_enabled_with_callback(obj->pin, obj->event, true, _gpio_irq);
}


void gpio_irq_disable(gpio_irq_t *obj)
{
    gpio_set_irq_enabled(obj->pin, m_pico_events[obj->pin], false);

    obj->event = 0;
    m_pico_events[obj->pin] = 0;
}

#endif
