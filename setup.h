#pragma once
#include <Arduino.h>

bool no_scan_mode = false, disable_sd = false, disable_auto_delete_sta = false;

void setup_init() {
    pinMode(25, INPUT_PULLDOWN);   //no scan mode
    pinMode(33, INPUT_PULLDOWN);  //disable sd
    pinMode(32, INPUT_PULLDOWN);  //disable auto delete sta
    no_scan_mode = digitalRead(25);
    disable_sd = digitalRead(33);
    disable_auto_delete_sta = digitalRead(32);
}