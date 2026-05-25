#include "nvs.h"
#include "clock_machine.hpp"
#include "clock_machine_states.hpp"
#include "esp_log.h"


ClockMachine::ClockMachine(RotaryEncoder* encoder_ref) {
    if (readNVSValues() == ESP_ERR_NVS_NOT_FOUND) {
        writeNVSDefaultValues();
        ESP_ERROR_CHECK(readNVSValues());
    }

    wifi_time.init(&wifi_credentials);
    wifi_time.getTime(&stored_time);

    display.init();

    encoder = encoder_ref;

    display.updateContent(D_E_ALARM_TIME, &alarm_time, D_A_OFF);

    last_wifi_connected_status = !wifi_time.isWifiConnected();

    state = static_cast<ClockState*>(new TimeState());
    state->enter(this);
}

esp_err_t ClockMachine::readNVSValues() {
    nvs_handle_t NVS_handle;

    esp_err_t err = nvs_open(NVS_STORAGE, NVS_READONLY, &NVS_handle);
    if (err != ESP_OK) return err;
    err = nvs_get_u8(NVS_handle, NVS_ALARM_HOUR, &alarm_time.hour);
    if (err != ESP_OK) return err;
    err = nvs_get_u8(NVS_handle, NVS_ALARM_MINUTE, &alarm_time.minute);
    if (err != ESP_OK) return err;
    size_t length = sizeof(wifi_credentials_t);
    err = nvs_get_blob(NVS_handle, NVS_WIFI_CREDENTIALS, &wifi_credentials, &length);

    nvs_close(NVS_handle);
    return err;
}

void ClockMachine::writeNVSDefaultValues() {
    alarm_time.hour = 7;
    alarm_time.minute = 0;
    strcpy((char*)wifi_credentials.ssid, "Dummy");
    strcpy((char*)wifi_credentials.password, "123456");

    saveAlarmTimeInNVS();
    saveWifiCredentialsInNVS();
}

void ClockMachine::saveAlarmTimeInNVS() {
    nvs_handle_t NVS_handle;
    ESP_ERROR_CHECK(nvs_open(NVS_STORAGE, NVS_READWRITE, &NVS_handle));
    ESP_ERROR_CHECK(nvs_set_u8(NVS_handle, NVS_ALARM_HOUR, alarm_time.hour));
    ESP_ERROR_CHECK(nvs_set_u8(NVS_handle, NVS_ALARM_MINUTE, alarm_time.minute));
    nvs_close(NVS_handle);
}

void ClockMachine::saveWifiCredentialsInNVS() {
    nvs_handle_t NVS_handle;
    ESP_ERROR_CHECK(nvs_open(NVS_STORAGE, NVS_READWRITE, &NVS_handle));
    ESP_ERROR_CHECK(nvs_set_blob(NVS_handle, NVS_WIFI_CREDENTIALS, &wifi_credentials, sizeof(wifi_credentials_t)));
    nvs_close(NVS_handle);
}

void ClockMachine::setState(ClockState& newState) {
    active_timer_us = 0;
    state->exit(this);
    state = &newState;
    state->enter(this);
}

void ClockMachine::checkTimeUpdate(void) {
    clock_time_t current_time;
    wifi_time.getTime(&current_time);

    if ((current_time.hour != stored_time.hour) ||
        (current_time.minute != stored_time.minute)) {
        stored_time.hour = current_time.hour;
        stored_time.minute = current_time.minute;
        display.updateContent(D_E_TIME, &stored_time, D_A_ON);
        time_has_changed = true;
    } else {
        time_has_changed = false;
    }
}

clock_time_t ClockMachine::getTimeToAlarm(clock_time_t current_time, clock_time_t alarm_time) {
    clock_time_t time_to_alarm;

    if (current_time.minute > alarm_time.minute) {
        alarm_time.minute += 60;
        current_time.hour++;
    }
    if (current_time.hour > alarm_time.hour)
        alarm_time.hour += 24;

    time_to_alarm.hour = alarm_time.hour - current_time.hour;
    time_to_alarm.minute = alarm_time.minute - current_time.minute;

    return time_to_alarm;
}

WifiTime* ClockMachine::getWifiTime() { return &wifi_time; }
Display* ClockMachine::getDisplay() { return &display; }
RotaryEncoder* ClockMachine::getEncoder() { return encoder; }

void ClockMachine::triggerTimer(uint16_t timer_ms) {
    active_timer_us = timer_ms * 1000;
    trigger_timestamp_us = esp_timer_get_time();
}

void ClockMachine::checkWifiStatus(bool force_update) {
    bool wifi_connected_status = wifi_time.isWifiConnected();
    if ((last_wifi_connected_status != wifi_connected_status) || force_update) {
        last_wifi_connected_status = wifi_connected_status;
        display_action_t wifi_action = wifi_connected_status ? D_A_ON : D_A_OFF;
        display.updateContent(D_E_WIFI_STATUS, wifi_action);
    }
}

void ClockMachine::run() {
    if (active_timer_us > 0 && (esp_timer_get_time() - trigger_timestamp_us) > active_timer_us) {
        active_timer_us = 0;
        state->timerExpired(this);
    } else {
        checkTimeUpdate();
        state->run(this);
    }
    checkWifiStatus(false);
}

void ClockMachine::buttonShortPressed() { state->buttonShortPressed(this); }
void ClockMachine::buttonLongPressed() { state->buttonLongPressed(this); }
void ClockMachine::encoderRotated(rotary_encoder_pos_t position, rotary_encoder_dir_t direction) {
    state->encoderRotated(this, position, direction);
}

