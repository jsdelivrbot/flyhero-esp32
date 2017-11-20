/*
 * PID.cpp
 *
 *  Created on: 14. 7. 2017
 *      Author: michp
 */

#include <cstdlib>
#include "PID.h"


namespace flyhero
{

PID::PID(float update_rate, float i_max, float Kp, float Ki, float Kd)
        : d_term_lpf(Biquad_Filter::FILTER_LOW_PASS, update_rate, 20)
{
    this->last_t.tv_sec = 0;
    this->last_t.tv_usec = 0;
    this->integrator = 0;
    this->Kp = Kp;
    this->Ki = Ki;
    this->Kd = Kd;
    this->i_max = i_max;
    this->last_d = NAN;
    this->last_error = NAN;
}

float PID::Get_PID(float error)
{
    timeval now;
    gettimeofday(&now, NULL);

    float delta_t = now.tv_sec - this->last_t.tv_sec
               + (now.tv_usec - this->last_t.tv_usec) * 0.000001f;
    float output = 0;

    if (this->last_t.tv_sec == 0 || delta_t > 1)
    {
        this->integrator = 0;
        delta_t = 0;
    }

    this->last_t = now;

    // proportional component
    output += error * this->Kp;

    // integral component
    if (this->Ki != 0 && delta_t > 0)
    {
        this->integrator += error * this->Ki * delta_t;

        if (this->integrator < -this->i_max)
            this->integrator = -this->i_max;
        if (this->integrator > this->i_max)
            this->integrator = this->i_max;

        output += this->integrator;
    }

    // derivative component
    if (this->Kd != 0 && delta_t > 0)
    {
        float derivative;

        if (std::isnan(this->last_d))
        {
            derivative = 0;
            this->last_d = 0;
        } else
            derivative = (error - this->last_error) / delta_t;

        // apply 20 Hz biquad LPF
        derivative = this->d_term_lpf.Apply_Filter(derivative);

        this->last_error = error;
        this->last_d = derivative;

        output += derivative * this->Kd;
    }

    return output;
}

void PID::Set_Kp(float Kp)
{
    this->Kp = Kp;
}

void PID::Set_Ki(float Ki)
{
    this->Ki = Ki;
}

void PID::Set_Kd(float Kd)
{
    this->Kd = Kd;
}

void PID::Set_I_Max(float i_max)
{
    this->i_max = i_max;
}

} /* namespace flyhero */
