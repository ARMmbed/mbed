#include "mbed_assert.h"
#include "analogin_api.h"
#include "hardware/adc.h"
#include "pinmap.h"
#include "PeripheralPins.h"

static float    const ADC_VREF_VOLTAGE     = 3.3f; /* 3.3V */
static uint16_t const ADC_RESOLUTION_BITS   = 12;
static float    const ADC_CONVERSION_FACTOR = ADC_VREF_VOLTAGE / (1 << 16);

void analogin_init(analogin_t *obj, PinName pin)
{
    /* Make sure pin is an analog pin we can use for ADC */
    MBED_ASSERT((ADCName)pinmap_peripheral(pin, PinMap_ADC) != (ADCName)NC);

    static bool is_adc_initialized = false;
    /* Initialize the ADC the first time it is being used,
     * but don't reinitialize it again afterwards.
     */
    if (!is_adc_initialized) 
    {
        adc_init();
        is_adc_initialized = true;
    }

    /* Lookup the corresponding ADC channel for a given pin. */
    obj->channel = pinmap_find_function(pin, PinMap_ADC);
    /* Make sure GPIO is high-impedance, no pullups etc. */
    adc_gpio_init(pin);
    /* Check if the ADC channel we just configured belongs to the
     * temperature sensor. If that's the case, enable the temperature
     * sensor.
     */
    if (pin == ADC_TEMP)
        adc_set_temp_sensor_enabled(true);
}

float analogin_read(analogin_t *obj)
{
    /* Read the raw 12-Bit value from the ADC. */
    float analog_in_raw = (float)analogin_read_u16(obj);
    /* Convert it to a voltage value. */
    return (analog_in_raw * ADC_CONVERSION_FACTOR);
}

uint16_t analogin_read_u16(analogin_t *obj)
{
    /* Select the desired ADC input channel. */
    adc_select_input(obj->channel);
    /* Read the 16-Bit ADC value. */
    return adc_read() << (16 - ADC_RESOLUTION_BITS);
}

const PinMap *analogin_pinmap()
{
    return PinMap_ADC;
}
