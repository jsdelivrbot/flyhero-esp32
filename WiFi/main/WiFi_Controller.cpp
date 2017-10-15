/*
 * WiFi_Controller.cpp
 *
 *  Created on: 13. 9. 2017
 *      Author: michp
 */

#include "WiFi_Controller.h"

namespace flyhero {

static esp_err_t event_handler(void *ctx, system_event_t *event) {
	switch (event->event_id) {
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

WiFi_Controller& WiFi_Controller::Instance() {
	static WiFi_Controller instance;

	return instance;
}

WiFi_Controller::WiFi_Controller() {
	this->socket_handle = -1;
	this->client_socket_length = sizeof(this->client);
	this->client_connected = false;
}

void WiFi_Controller::ap_init() {
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
	std::memcpy(wifi_config.ap.password, "dronjede", 9);
	std::memcpy(wifi_config.ap.ssid, "DRON_WIFI", 10);
	wifi_config.ap.ssid_hidden = 0; // TODO
	wifi_config.ap.ssid_len = 0;

	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());
}

esp_err_t WiFi_Controller::udp_server_start() {
	this->socket_handle = socket(PF_INET, SOCK_DGRAM, 0);

	// TODO show error using getsockopt
	if (this->socket_handle < 0)
		return ESP_FAIL;

	sockaddr_in server;
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	server.sin_port = htons(this->UDP_PORT);
	server.sin_family = AF_INET;

	// set socket operations non-blocking
	// TODO show error using getsockopt
	if (fcntl(this->socket_handle, F_SETFL, O_NONBLOCK) < 0) {
		closesocket(this->socket_handle);
	}

	// TODO show error using getsockopt
	if (bind(this->socket_handle, (sockaddr*)&server, sizeof(server)) < 0) {
		closesocket(this->socket_handle);
		return ESP_FAIL;
	}

	return ESP_OK;
}

void WiFi_Controller::Init() {
	this->ap_init();

	ESP_ERROR_CHECK(this->udp_server_start());
}

bool WiFi_Controller::Receive(uint8_t *buffer, uint8_t buffer_length, uint8_t& received_length) {
	int len;

	if ( (len = recvfrom(this->socket_handle, buffer, buffer_length, 0, (sockaddr*)(&this->client), &this->client_socket_length)) < 0)
		return false;

	received_length = len;

	return true;
}

bool WiFi_Controller::Send(uint8_t *data, uint8_t data_length) {
	if (!this->client_connected)
		return false;

	int len;

	if ( (len = sendto(this->socket_handle, data, data_length, 0, (sockaddr*)&this->client, this->client_socket_length)) < 0)
		return false;

	if (len != data_length)
		return false;

	return true;
}

void WiFi_Controller::Client_Connected_Callback() {
	this->client_connected = true;
}

void WiFi_Controller::Client_Disconnected_Callback() {
	this->client_connected = false;
}

} /* namespace flyhero */
