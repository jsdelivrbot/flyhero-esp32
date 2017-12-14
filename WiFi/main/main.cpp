#include <iostream>
#include <nvs_flash.h>

#include "WiFi_Controller.h"


using namespace flyhero;

extern "C" void app_main(void)
{
    // Initialize NVS
    esp_err_t nvs_status = nvs_flash_init();

    if (nvs_status == ESP_ERR_NVS_NO_FREE_PAGES)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    WiFi_Controller &wifi = WiFi_Controller::Instance();
    LEDs::Init();

    const uint8_t TCP_BUFFER_LENGTH = 50;
    char TCP_buffer[TCP_BUFFER_LENGTH];
    uint8_t received_length = 0;
    bool process_tcp = true;

    wifi.Init();

    ESP_ERROR_CHECK(wifi.TCP_Server_Start());
    ESP_ERROR_CHECK(wifi.TCP_Wait_For_Client());

    while (process_tcp)
    {
        if (wifi.TCP_Receive(TCP_buffer, TCP_BUFFER_LENGTH, &received_length))
        {
            printf("Command: %.*s\n", received_length - 2, TCP_buffer);

            if (strncmp((const char *) TCP_buffer, "start", 5) == 0)
            {
                wifi.TCP_Send("yup", 3);
                process_tcp = false;
            } else if (strncmp((const char *) TCP_buffer, "calibrate", 9) == 0)
            {
                wifi.TCP_Send("yup", 3);
            } else
                wifi.TCP_Send("nah", 3);
        }
    }

    ESP_ERROR_CHECK(wifi.TCP_Server_Stop());
    ESP_ERROR_CHECK(wifi.UDP_Server_Start());

    WiFi_Controller::In_Datagram_Data data;

    while (true)
    {
        if (wifi.UDP_Receive(data))
        {
            std::cout << data.throttle << " "
                      << (uint16_t)data.roll_kp << " " << (uint16_t)data.roll_ki << " "
                      << (uint16_t)data.pitch_kp << " " << (uint16_t)data.pitch_ki << " "
                      << (uint16_t)data.yaw_kp << " " << (uint16_t)data.yaw_ki << std::endl;
        }
    }
}
