/*
 * WiFi_Controller.cpp
 *
 *  Created on: 13. 9. 2017
 *      Author: michp
 */

#include "WiFi_Controller.h"


namespace flyhero
{

static esp_err_t event_handler(void * ctx, system_event_t * event)
{
    switch (event->event_id)
    {
        case SYSTEM_EVENT_AP_STACONNECTED:
            WiFi_Controller::Instance().Client_Connected_Callback();

            LEDs::Turn_On(LEDs::WARNING);
            vTaskDelay(250 / portTICK_RATE_MS);
            LEDs::Turn_Off(LEDs::WARNING);
            break;
        case SYSTEM_EVENT_AP_STADISCONNECTED:
            WiFi_Controller::Instance().Client_Disconnected_Callback();

            LEDs::Turn_On(LEDs::WARNING);
            vTaskDelay(1000 / portTICK_RATE_MS);
            LEDs::Turn_Off(LEDs::WARNING);
            break;
        default:
            break;
    }

    return ESP_OK;
}

WiFi_Controller & WiFi_Controller::Instance()
{
    static WiFi_Controller instance;

    return instance;
}

WiFi_Controller::WiFi_Controller()
{
    this->udp_server_fd = -1;
    this->udp_client_socket_length = sizeof(this->udp_client_socket);

    this->tcp_server_fd = -1;
    this->tcp_client_fd = -1;
    this->tcp_client_address_length = sizeof(this->tcp_client_address);

    this->client_connected = false;
}

void WiFi_Controller::ap_init()
{
    tcpip_adapter_init();

    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&config));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    wifi_config_t wifi_config;
    wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    wifi_config.ap.beacon_interval = 100;
    wifi_config.ap.channel = 7;
    wifi_config.ap.max_connection = 1;

    assert(strlen(CONFIG_FLYHERO_WIFI_SSID) >= 1);
    assert(strlen(CONFIG_FLYHERO_WIFI_WPA2_PASSPHRASE) >= 8);

    std::strcpy((char *) wifi_config.ap.password, CONFIG_FLYHERO_WIFI_WPA2_PASSPHRASE);
    std::strcpy((char *) wifi_config.ap.ssid, CONFIG_FLYHERO_WIFI_SSID);
    wifi_config.ap.ssid_hidden = !CONFIG_FLYHERO_WIFI_SSID_VISIBLE;
    wifi_config.ap.ssid_len = 0;

    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void WiFi_Controller::Init()
{
    this->ap_init();
}

esp_err_t WiFi_Controller::UDP_Server_Start()
{
    this->udp_server_fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (this->udp_server_fd < 0)
        return ESP_FAIL;

    sockaddr_in udp_server_address;
    udp_server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    udp_server_address.sin_port = htons(this->UDP_PORT);
    udp_server_address.sin_family = AF_INET;

    if (fcntl(this->udp_server_fd, F_SETFL, O_NONBLOCK) != 0)
    {
        closesocket(this->udp_server_fd);
        this->udp_server_fd = -1;
        return ESP_FAIL;
    }

    if (bind(this->udp_server_fd, (sockaddr *) &udp_server_address, sizeof(udp_server_address)) < 0)
    {
        closesocket(this->udp_server_fd);
        this->udp_server_fd = -1;
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t WiFi_Controller::UDP_Server_Stop()
{
    if (this->udp_server_fd < 0)
        return ESP_FAIL;

    closesocket(this->udp_server_fd);
    this->udp_server_fd = -1;
    return ESP_OK;
}

esp_err_t WiFi_Controller::TCP_Server_Start()
{
    this->tcp_server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (this->tcp_server_fd < 0)
        return ESP_FAIL;

    sockaddr_in tcp_server_address;
    tcp_server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    tcp_server_address.sin_port = htons(this->TCP_PORT);
    tcp_server_address.sin_family = AF_INET;

    if (fcntl(this->tcp_server_fd, F_SETFL, O_NONBLOCK) != 0)
    {
        closesocket(this->tcp_server_fd);
        this->tcp_server_fd = -1;
        return ESP_FAIL;
    }

    if (bind(this->tcp_server_fd, (sockaddr *) &tcp_server_address, sizeof(tcp_server_address)) < 0)
    {
        closesocket(this->tcp_server_fd);
        this->tcp_server_fd = -1;
        return ESP_FAIL;
    }

    if (listen(this->tcp_server_fd, 1) < 0)
    {
        closesocket(this->tcp_server_fd);
        this->tcp_server_fd = -1;
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t WiFi_Controller::TCP_Server_Stop()
{
    if (this->tcp_server_fd < 0)
        return ESP_FAIL;

    closesocket(this->tcp_server_fd);
    this->tcp_server_fd = -1;
    return ESP_OK;
}

esp_err_t WiFi_Controller::TCP_Wait_For_Client()
{
    if (this->tcp_server_fd < 0)
        return ESP_FAIL;

    do
    {
        this->tcp_client_fd =
                accept(this->tcp_server_fd,
                       (sockaddr *) &this->tcp_client_address,
                       &this->tcp_client_address_length);

    } while (this->tcp_client_fd < 0 && (errno == EAGAIN || errno == EWOULDBLOCK));

    if (this->tcp_client_fd < 0)
    {
        closesocket(this->tcp_server_fd);
        this->tcp_server_fd = -1;

        return ESP_FAIL;
    }

    return ESP_OK;
}

bool WiFi_Controller::UDP_Receive(In_Datagram_Data & datagram_data)
{
    in_datagram datagram;

    if (recvfrom(this->udp_server_fd, datagram.raw_data, IN_DATAGRAM_LENGTH, 0,
                 (sockaddr *) (&this->udp_client_socket), &this->udp_client_socket_length) != IN_DATAGRAM_LENGTH)
        return false;

    if (datagram.data.datagram_length != IN_DATAGRAM_LENGTH)
        return false;

    if (datagram.data.crc !=
        CRC::CRC16(datagram.raw_data, IN_DATAGRAM_LENGTH - 2))
        return false;

    datagram_data = datagram.data.datagram_contents;

    return true;
}

bool WiFi_Controller::UDP_Send(const Out_Datagram_Data & datagram_data)
{
    if (!this->client_connected)
        return false;

    out_datagram datagram;

    datagram.data.datagram_length = OUT_DATAGRAM_LENGTH;
    datagram.data.datagram_contents = datagram_data;
    datagram.data.crc = CRC::CRC16(datagram.raw_data, OUT_DATAGRAM_LENGTH - 2);

    return sendto(this->udp_server_fd, datagram.raw_data, OUT_DATAGRAM_LENGTH, 0,
                  (sockaddr *) &this->udp_client_socket, this->udp_client_socket_length) ==
           datagram.data.datagram_length;

}

bool WiFi_Controller::TCP_Receive(const char *& data, size_t & received_length)
{
    // in case that client reconnected
    int tmp_fd = accept(this->tcp_server_fd,
                        (sockaddr *) &this->tcp_client_address,
                        &this->tcp_client_address_length);
    if (tmp_fd >= 0)
        this->tcp_client_fd = tmp_fd;

    ssize_t len;
    if ((len = recv(this->tcp_client_fd, this->tcp_buffer, this->TCP_BUFFER_LENGTH, MSG_DONTWAIT)) <= 0)
    {
        received_length = 0;
        return false;
    }

    data = (const char *) this->tcp_buffer;
    received_length = (size_t) len;

    return true;
}

bool WiFi_Controller::TCP_Receive(const uint8_t *& data, size_t & received_length)
{
    // in case that client reconnected
    int tmp_fd = accept(this->tcp_server_fd,
                        (sockaddr *) &this->tcp_client_address,
                        &this->tcp_client_address_length);
    if (tmp_fd >= 0)
        this->tcp_client_fd = tmp_fd;

    ssize_t len;
    if ((len = recv(this->tcp_client_fd, this->tcp_buffer, this->TCP_BUFFER_LENGTH, MSG_DONTWAIT)) <= 0)
    {
        received_length = 0;
        return false;
    }

    data = this->tcp_buffer;
    received_length = (size_t) len;

    return true;
}

bool WiFi_Controller::TCP_Send(const char * data, size_t data_length)
{
    if (!this->client_connected)
        return false;

    return send(this->tcp_client_fd, data, data_length, 0) == data_length;
}

bool WiFi_Controller::TCP_Send(const uint8_t * data, size_t data_length)
{
    if (!this->client_connected)
        return false;

    return send(this->tcp_client_fd, data, data_length, 0) == data_length;
}

void WiFi_Controller::Client_Connected_Callback()
{
    this->client_connected = true;
}

void WiFi_Controller::Client_Disconnected_Callback()
{
    this->client_connected = false;
}

} /* namespace flyhero */
