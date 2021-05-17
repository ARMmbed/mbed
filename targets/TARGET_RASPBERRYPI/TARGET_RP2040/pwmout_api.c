/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice, this list
 *      of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form, except as embedded into a Nordic Semiconductor ASA
 *      integrated circuit in a product or a software update for such product, must reproduce
 *      the above copyright notice, this list of conditions and the following disclaimer in
 *      the documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of Nordic Semiconductor ASA nor the names of its contributors may be
 *      used to endorse or promote products derived from this software without specific prior
 *      written permission.
 *
 *   4. This software, with or without modification, must only be used with a
 *      Nordic Semiconductor ASA integrated circuit.
 *
 *   5. Any software provided in binary or object form under this license must not be reverse
 *      engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#if DEVICE_PWMOUT

#include "hal/pwmout_api.h"
#include "PeripheralPins.h"
#include "pinmap.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "mbed_assert.h"

const uint count_top = 1000;

/** Initialize the pwm out peripheral and configure the pin
 *
 * Parameter obj The pwmout object to initialize
 * Parameter pin The pwmout pin to initialize
 */
void pwmout_init(pwmout_t *obj, PinName pin)
{
    MBED_ASSERT(obj != NULL);

    /* Populate PWM object with default values. */
    obj->slice = pwm_gpio_to_slice_num(pin);
    obj->channel = pwm_gpio_to_channel(pin);
    obj->pin = pin;
    obj->period = 0;
    obj->percent = 0.5f;

    obj->cfg = pwm_get_default_config();
    pwm_config_set_wrap(&(obj->cfg), count_top);

    pwm_init(obj->slice, &(obj->cfg), false);
    gpio_set_function(pin, GPIO_FUNC_PWM);
}

/** Deinitialize the pwmout object
 *
 * Parameter obj The pwmout object
 */
void pwmout_free(pwmout_t *obj)
{
    MBED_ASSERT(obj != NULL);
    pwm_set_enabled(obj->slice, false);
}

/** Set the output duty-cycle in range <0.0f, 1.0f>
 *
 * pulse 0.0f represents 0 percentage, 1.0f represents 100 percent.
 * Parameter obj     The pwmout object
 * Parameter percent The floating-point percentage number
 */
void pwmout_write(pwmout_t *obj, float percent)
{
    obj->percent = percent;
    pwm_set_gpio_level(obj->pin, percent * (count_top + 1));
    pwm_set_enabled(obj->slice, true);
}

/** Read the current float-point output duty-cycle
 *
 * Parameter obj The pwmout object
 * Return A floating-point output duty-cycle
 */
float pwmout_read(pwmout_t *obj)
{
    /* Return percentage stored in object instead of calculating the value.
     * This prevents floating point rounding errors.
     */
    return obj->percent;
}

/** Set the PWM period specified in seconds, keeping the duty cycle the same
 *
 * Periods smaller than microseconds (the lowest resolution) are set to zero.
 * Parameter obj     The pwmout object
 * Parameter seconds The floating-point seconds period
 */
void pwmout_period(pwmout_t *obj, float period)
{
    /* Set new period. */
    pwmout_period_us(obj, period * 1000000);
}

/** Set the PWM period specified in miliseconds, keeping the duty cycle the same
 *
 * Parameter obj The pwmout object
 * Parameter ms  The milisecond period
 */
void pwmout_period_ms(pwmout_t *obj, int period)
{
    /* Set new period. */
    pwmout_period_us(obj, period * 1000);
}

/** Set the PWM period specified in microseconds, keeping the duty cycle the same
 *
 * Parameter obj The pwmout object
 * Parameter us  The microsecond period
 */
void pwmout_period_us(pwmout_t *obj, int period)
{
    obj->period = period;

    // min_period should be 8us
    uint32_t min_period = 1000000 * count_top / clock_get_hz(clk_sys);

    pwm_config_set_clkdiv(&(obj->cfg), (float)period / (float)min_period);
    pwm_init(obj->slice, &(obj->cfg), false);
}

int pwmout_read_period_us(pwmout_t *obj)
{
    return obj->period;
}

/** Set the PWM pulsewidth specified in seconds, keeping the period the same.
 *
 * Parameter obj     The pwmout object
 * Parameter seconds The floating-point pulsewidth in seconds
 */
void pwmout_pulsewidth(pwmout_t *obj, float pulse)
{
    pwmout_pulsewidth_us(obj, pulse * 1000000);
}

/** Set the PWM pulsewidth specified in miliseconds, keeping the period the same.
 *
 * Parameter obj The pwmout object
 * Parameter ms  The floating-point pulsewidth in miliseconds
 */
void pwmout_pulsewidth_ms(pwmout_t *obj, int pulse)
{
    pwmout_pulsewidth_us(obj, pulse * 1000);
}

/** Set the PWM pulsewidth specified in microseconds, keeping the period the same.
 *
 * Parameter obj The pwmout object
 * Parameter us  The floating-point pulsewidth in microseconds
 */
void pwmout_pulsewidth_us(pwmout_t *obj, int pulse)
{
    /* Cap pulsewidth to period. */
    if (pulse > obj->period) {
        pulse = obj->period;
    }

    obj->percent = (float) pulse / (float) obj->period;

    /* Restart instance with new values. */
    pwmout_write(obj, obj->percent);
}

int pwmout_read_pulsewidth_us(pwmout_t *obj) {
    return (obj->period) * (obj->percent);
}

const PinMap *pwmout_pinmap()
{
    return PinMap_PWM_OUT;
}

#endif // DEVICE_PWMOUT
