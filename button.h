#pragma once
#include <Arduino.h>
#include <log.h>
#include <OneButton.h>
#include <esp_wifi.h>
#include "display.h"
#include "wifi.h"

OneButton button(0);

void init_button() {
	button.attachClick([]() {
		if (menu == 3) {
			//will add smthing,ig?
		}
		else {
			channel_index = (channel_index + 1) % channels.size();
			esp_wifi_set_channel(channels[channel_index], WIFI_SECOND_CHAN_NONE);
			log1(TFT_WHITE, "Changed channel to %d", channels[channel_index]);
			draw();
		}
	});
	button.attachDoubleClick([]() {menu = (menu == 1) ? 0: 1;draw();});
	button.attachLongPressStart([]() {deauthActive = !deauthActive;});
	button.attachMultiClick([]() {
		int c = button.getNumberClicks();
		if		(c == 3) {menu = (menu == 2) ? 0 : 2;draw();}
		else if (c == 4) {menu = (menu == 4) ? 0 : 4;draw();}
		else if (c == 5) {menu = (menu == 3) ? 0 : 3;draw();}
		else if (c == 6) {for_each_aps ap.STAs.clear();log1(TFT_RED, "Cleared all STAs!");}
		else if (c == 7) {if (fileOpen) fileOpen = false,log1(TFT_RED, "Disabled SD logger!!!");}
		else if (c == 8) {
			log1(TFT_GREEN, "ok!");
			xTaskCreatePinnedToCore([](void* _) {
				while(1) {
					beaconXD();
					delay(10);
				}
			}, "troller", 2000, NULL, 2, NULL, 0);
		}
	});
}