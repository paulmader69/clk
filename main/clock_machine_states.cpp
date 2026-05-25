#include "clock_machine_states.hpp"

//--------------//
//  TIME STATE  //
//--------------//

ClockState& TimeState::getInstance() {
    static TimeState singleton;
    return singleton;
}

void TimeState::enter(ClockMachine* clock) {
    clock->getDisplay()->updateContent(D_E_TIME, &clock->stored_time, D_A_ON);
}

void TimeState::run(ClockMachine* clock) {
    if (clock->is_alarm_set &&
        (clock->stored_time.hour == clock->alarm_time.hour) &&
        (clock->stored_time.minute == clock->alarm_time.minute)) {
        clock_time_t bed_time = clock->getTimeToAlarm(clock->stored_time, clock->alarm_time);
        clock->getDisplay()->updateContent(D_E_BED_TIME, &bed_time, D_A_OFF);
        clock->setState(AlarmState::getInstance());
    }
    else if (clock->is_alarm_set && clock->time_has_changed) {
        clock_time_t bed_time = clock->getTimeToAlarm(clock->stored_time, clock->alarm_time);
        clock->getDisplay()->updateContent(D_E_BED_TIME, &bed_time, D_A_ON);
    }
}

void TimeState::timerExpired(ClockMachine* clock) {
    clock->getDisplay()->setIncreasedBrightness(false);
}

void TimeState::buttonShortPressed(ClockMachine* clock) {
    if (clock->getDisplay()->isDisplayOn()) {
        clock->is_alarm_set = !clock->is_alarm_set;
        display_action_t action = clock->is_alarm_set ? D_A_ON : D_A_OFF;
        clock->getDisplay()->updateContent(D_E_ALARM_TIME, &clock->alarm_time, action);
        clock_time_t bed_time = clock->getTimeToAlarm(clock->stored_time, clock->alarm_time);
        clock->getDisplay()->updateContent(D_E_BED_TIME, &bed_time, action);
    }
    clock->getDisplay()->setIncreasedBrightness(true);
    clock->triggerTimer(3000);
}

void TimeState::buttonLongPressed(ClockMachine* clock) {
    clock->setState(SetAlarmState::getInstance());
}

void TimeState::encoderRotated(ClockMachine* clock, rotary_encoder_pos_t position, rotary_encoder_dir_t direction) {
    if (clock->getWifiTime()->isWifiConnected()) {
        clock->getDisplay()->setIncreasedBrightness(true);
        clock->triggerTimer(3000);
    } else {
        clock->setState(WPSState::getInstance());
    }
}

void TimeState::exit(ClockMachine* clock) {}
TimeState::~TimeState() {}


//-------------//
//  WPS STATE  //
//-------------//

ClockState& WPSState::getInstance() {
    static WPSState singleton;
    return singleton;
}

void WPSState::enter(ClockMachine* clock) {
    clock->getDisplay()->updateContent(D_E_BED_TIME, &(clock->alarm_time), D_A_OFF);
    blink = true;
    clock->getDisplay()->updateContent(D_E_WIFI_SETTING, D_A_ON);
    clock->getDisplay()->setIncreasedBrightness(true);
    clock->getWifiTime()->startWPS();
    clock->triggerTimer(500);
}

void WPSState::run(ClockMachine* clock) {
    if (clock->getWifiTime()->isWifiConnected()) {
        clock->saveWifiCredentialsInNVS();
        clock->setState(TimeState::getInstance());
    }
}

void WPSState::timerExpired(ClockMachine* clock) {
    blink = !blink;
    display_action_t action = blink ? D_A_ON : D_A_OFF;
    clock->getDisplay()->updateContent(D_E_WIFI_SETTING, action);
    clock->triggerTimer(500);
}

void WPSState::buttonShortPressed(ClockMachine* clock) {}
void WPSState::buttonLongPressed(ClockMachine* clock) {}

void WPSState::encoderRotated(ClockMachine* clock, rotary_encoder_pos_t position, rotary_encoder_dir_t direction) {
    clock->getWifiTime()->stopWPS();
    clock->setState(TimeState::getInstance());
}

void WPSState::exit(ClockMachine* clock) {
    clock->getDisplay()->updateContent(D_E_WIFI_SETTING, D_A_OFF);
    display_action_t action = clock->is_alarm_set ? D_A_ON : D_A_OFF;
    clock_time_t bed_time = clock->getTimeToAlarm(clock->stored_time, clock->alarm_time);
    clock->getDisplay()->updateContent(D_E_BED_TIME, &bed_time, action);
    clock->checkWifiStatus(true);
}

WPSState::~WPSState() {}


//---------------//
//  ALARM STATE  //
//---------------//

ClockState& AlarmState::getInstance() {
    static AlarmState singleton;
    return singleton;
}

void AlarmState::enter(ClockMachine* clock) {
    clock->getDisplay()->setMaxBrightness(true);
    clock->triggerTimer(500);
}

void AlarmState::run(ClockMachine* clock) {}

void AlarmState::timerExpired(ClockMachine* clock) {
    display_action_t action = alarm_symbol_direction ? D_A_OFF : D_A_ON;
    alarm_symbol_direction = !alarm_symbol_direction;
    clock->getDisplay()->updateContent(D_E_ALARM_ACTIVE, action);
    clock->triggerTimer(500);
}

void AlarmState::buttonShortPressed(ClockMachine* clock) {
    clock->setState(SnoozeState::getInstance());
}

void AlarmState::buttonLongPressed(ClockMachine* clock) {}
void AlarmState::encoderRotated(ClockMachine* clock, rotary_encoder_pos_t position, rotary_encoder_dir_t direction) {}

void AlarmState::exit(ClockMachine* clock) {
    clock->getDisplay()->setMaxBrightness(false);
    clock->getDisplay()->updateContent(D_E_ALARM_TIME, &clock->alarm_time, D_A_ON);
}

AlarmState::~AlarmState() {}


//----------------//
//  SNOOZE STATE  //
//----------------//

ClockState& SnoozeState::getInstance() {
    static SnoozeState singleton;
    return singleton;
}

void SnoozeState::enter(ClockMachine* clock) {
    snooze_leaving_step = SNOOZE_WAITING;
    clock->getDisplay()->setIncreasedBrightness(true);
    clock->triggerTimer(3000);
    snooze_start_timer_s = (int64_t)(esp_timer_get_time() / 1000000);
    remaining_snooze_time_s = clock->settings.snooze_time_s;
}

void SnoozeState::run(ClockMachine* clock) {
    int64_t now_s = (int64_t)(esp_timer_get_time() / 1000000);
    uint16_t snooze_time_left_s = (uint16_t)(clock->settings.snooze_time_s - now_s + snooze_start_timer_s);
    if (snooze_time_left_s < remaining_snooze_time_s) {
        if (snooze_time_left_s == 0) {
            clock->setState(AlarmState::getInstance());
        } else {
            remaining_snooze_time_s = snooze_time_left_s;
            clock->getDisplay()->updateContent(D_E_SNOOZE_TIME, &snooze_time_left_s, D_A_ON);
        }
    }
}

void SnoozeState::timerExpired(ClockMachine* clock) {
    clock->getDisplay()->setIncreasedBrightness(false);
    snooze_leaving_step = SNOOZE_WAITING;
    clock->getDisplay()->updateContent(D_E_SNOOZE_CANCEL, D_A_OFF);
}

void SnoozeState::buttonShortPressed(ClockMachine* clock) {
    clock->getDisplay()->setIncreasedBrightness(true);
    clock->triggerTimer(3000);
}

void SnoozeState::buttonLongPressed(ClockMachine* clock) {
    if (snooze_leaving_step == SNOOZE_FIRST_ROTATION) {
        snooze_leaving_step = SNOOZE_LONG_PRESS;
        clock->getDisplay()->updateContent(D_E_SNOOZE_CANCEL, D_A_TWO_BARS);
    }
    clock->getDisplay()->setIncreasedBrightness(true);
    clock->triggerTimer(3000);
}

void SnoozeState::encoderRotated(ClockMachine* clock, rotary_encoder_pos_t position, rotary_encoder_dir_t direction) {
    if (snooze_leaving_step == SNOOZE_WAITING) {
        snooze_leaving_step = SNOOZE_FIRST_ROTATION;
        first_rotation_dir = direction;
        clock->getDisplay()->updateContent(D_E_SNOOZE_CANCEL, D_A_ONE_BAR);
    } else if (snooze_leaving_step == SNOOZE_LONG_PRESS && direction != first_rotation_dir) {
        clock->getDisplay()->updateContent(D_E_SNOOZE_CANCEL, D_A_OFF);
        clock->is_alarm_set = false;
        clock->getDisplay()->updateContent(D_E_ALARM_TIME, &clock->alarm_time, D_A_OFF);
        clock->setState(TimeState::getInstance());
    }
    clock->getDisplay()->setIncreasedBrightness(true);
    clock->triggerTimer(3000);
}

void SnoozeState::exit(ClockMachine* clock) {
    clock->getDisplay()->updateContent(D_E_SNOOZE_TIME, NULL, D_A_OFF);
}

SnoozeState::~SnoozeState() {}


//-------------------//
//  SET ALARM STATE  //
//-------------------//

ClockState& SetAlarmState::getInstance() {
    static SetAlarmState singleton;
    return singleton;
}

void SetAlarmState::enter(ClockMachine* clock) {
    clock->getDisplay()->setIncreasedBrightness(true);
    clock->getDisplay()->updateContent(D_E_ALARM_TIME, &clock->alarm_time, D_A_HIDE_HOURS);
    hours_hidden = true;
    setting_minutes = false;
    clock->triggerTimer(500);
    clock->getEncoder()->setRange(0, 23, true, true);
    clock->getEncoder()->setPosition(clock->alarm_time.hour);
    original_alarm_time.hour = clock->alarm_time.hour;
    original_alarm_time.minute = clock->alarm_time.minute;
}

void SetAlarmState::run(ClockMachine* clock) {}

void SetAlarmState::timerExpired(ClockMachine* clock) {
    display_action_t action;
    if (!setting_minutes) {
        action = hours_hidden ? D_A_ON : D_A_HIDE_HOURS;
        hours_hidden = !hours_hidden;
    } else {
        action = minutes_hidden ? D_A_ON : D_A_HIDE_MINUTES;
        minutes_hidden = !minutes_hidden;
    }
    clock->getDisplay()->updateContent(D_E_ALARM_TIME, &clock->alarm_time, action);
    clock->triggerTimer(500);
}

void SetAlarmState::buttonShortPressed(ClockMachine* clock) {
    if (!setting_minutes) {
        clock->getDisplay()->updateContent(D_E_ALARM_TIME, &clock->alarm_time, D_A_HIDE_MINUTES);
        minutes_hidden = true;
        setting_minutes = true;
        clock->triggerTimer(500);
        clock->getEncoder()->setRange(0, 59, true, true);
        clock->getEncoder()->setPosition(clock->alarm_time.minute);
    } else {
        clock->setState(TimeState::getInstance());
    }
}

void SetAlarmState::buttonLongPressed(ClockMachine* clock) {
    clock->alarm_time.hour = original_alarm_time.hour;
    clock->alarm_time.minute = original_alarm_time.minute;
    clock->setState(TimeState::getInstance());
}

void SetAlarmState::encoderRotated(ClockMachine* clock, rotary_encoder_pos_t position, rotary_encoder_dir_t direction) {
    if (!setting_minutes) {
        clock->alarm_time.hour = clock->getEncoder()->getPosition();
        hours_hidden = false;
    } else {
        clock->alarm_time.minute = clock->getEncoder()->getPosition();
        minutes_hidden = false;
    }
    clock->getDisplay()->updateContent(D_E_ALARM_TIME, &clock->alarm_time, D_A_ON);
    clock_time_t bed_time = clock->getTimeToAlarm(clock->stored_time, clock->alarm_time);
    clock->getDisplay()->updateContent(D_E_BED_TIME, &bed_time, D_A_ON);
    clock->triggerTimer(500);
}

void SetAlarmState::exit(ClockMachine* clock) {
    clock->getDisplay()->updateContent(D_E_ALARM_TIME, &clock->alarm_time, D_A_ON);
    clock->is_alarm_set = true;
    clock->saveAlarmTimeInNVS();
    clock->triggerTimer(3000);
}

SetAlarmState::~SetAlarmState() {}
