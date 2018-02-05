/*
 * HC_SR04.h
 *
 *  Created on: 19. 9. 2017
 *      Author: michp
 */

#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_timer.h>


namespace flyhero
{

class HC_SR04
{
private:
    const gpio_num_t trigg_pin;
    const gpio_num_t echo_pin;
    const gpio_isr_t echo_handler;

    int64_t start;
    bool level_high;
    float distance;
    SemaphoreHandle_t distance_semaphore;

    esp_err_t trigg_init();

    esp_err_t echo_init();

public:
    HC_SR04(gpio_num_t trigg, gpio_num_t echo, gpio_isr_t isr_handler);

    void Init();

    void Trigger();

    float Get_Distance();

    void Echo_Callback();
};

} /* namespace flyhero */
