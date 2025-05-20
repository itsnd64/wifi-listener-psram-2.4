/*  Menus:
    0: Home menu
    1: Logs menu
    2: File/EAPOL event
    3: FreeRTOS Tasks and sum stats
    4: Not connected STAs list(WIP)
*/

#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "sd card.h"
#include "wifi.h"
#include "cpu mon.h"
#include "setup.h"
#include "misc.h"

using namespace std;

TFT_eSPI tft = TFT_eSPI(240, 320);
TFT_eSprite sprite = TFT_eSprite(&tft);
SemaphoreHandle_t drawSemaphore;
int menu = 0;
vector<pair<pair<float, float>, TaskStatus_t>> taskCpuUsageList;

void init_display() {
	tft.init();
	tft.setRotation(1);
	tft.fillScreen(TFT_BLACK);
    tft.setTextWrap(false);
    sprite.setColorDepth(8); 
	sprite.createSprite(tft.width(), tft.height());
	sprite.fillSprite(TFT_BLACK);
	sprite.setTextColor(0x9FE3);
	sprite.setCursor(0, 0);
	drawSemaphore = xSemaphoreCreateBinary();

    sprite.printf("Setups:\n");
	sprite.printf("No Scan:%s\n", no_scan_mode ? "true" : "false");
    sprite.printf("Disable SD:%s\n", disable_sd ? "true" : "false");
    sprite.printf("Disable Auto Delete STA:%s\n", disable_auto_delete_sta ? "true" : "false");
    sprite.printf("Have SD:%s\n", fileOpen ? "true" : "false");

    sprite.printf("\nCores:\n");
    sprite.printf("Core 0: %s%s%s%s%s%s%s%s%s\n", WIFI_TASK_CORE_ID ? "" : "Wifi ", MONITOR_TASK_CORE ? "" : "Monitor ", DEAUTH_TASK_CORE ? "" : "Deauth ", PROBE_TASK_CORE ? "" : "Probe ", LOG_TASK_CORE ? "" : "Log ", SD_TASK_CORE ? "" : "SD ", AP_STA_UPDATE_TASK_CORE ? "" : "AP+STA Update ", UPDATE_TASK_CORE ? "" : "Update ", FLUSH_TASK_CORE ? "" : "Flush");
    sprite.printf("Core 1: %s%s%s%s%s%s%s%s%s\n", WIFI_TASK_CORE_ID ? "Wifi " : "", MONITOR_TASK_CORE ? "Monitor " : "", DEAUTH_TASK_CORE ? "Deauth " : "", PROBE_TASK_CORE ? "Probe " : "", LOG_TASK_CORE ? "Log " : "", SD_TASK_CORE ? "SD " : "", AP_STA_UPDATE_TASK_CORE ? "AP+STA_Update " : "", UPDATE_TASK_CORE ? "Update " : "", FLUSH_TASK_CORE ? "Flush" : "");

    sprite.printf("\nIntervals:\n");
    sprite.printf("Log Menu Update: %ums => %.1f FPS\n", LOG_MENU_UPDATE_INTERVAL, 1000.0 / LOG_MENU_UPDATE_INTERVAL);
    sprite.printf("Button Tick: %ums => %.1f cps\n", BUTTON_TICK_INTERVAL, 1000.0 / BUTTON_TICK_INTERVAL);
    sprite.printf("Update Task: %ums => %.1f cps\n", UPDATE_TASK_INTERVAL, 1000.0 / UPDATE_TASK_INTERVAL);
    sprite.printf("Flush: %ums => %.1f cps\n", FLUSH_INTERVAL, 1000.0 / FLUSH_INTERVAL);
    sprite.printf("Deauth: %ums =  > %.1f pkt/s\n", DEAUTH_INTERVAL, 1000.0 / DEAUTH_INTERVAL);
    sprite.printf("Probe: %ums => %.1f pkt/s\n", PROBE_INTERVAL, 1000.0 / PROBE_INTERVAL);

    sprite.printf("\nOthers:\n");
    // sprite.printf("AP Timeout: %u => %.1fs\n", AP_TIMEOUT, AP_TIMEOUT / 1000.0);
    sprite.printf("Scan Time Min: %u => %.2fs Each Channel Min\n", SCAN_TIME_MIN, SCAN_TIME_MIN / 1000.0);
    sprite.printf("Scan Time Max: %u => %.2fs Each Channel Max\n", SCAN_TIME_MAX, SCAN_TIME_MAX / 1000.0);
    // sprite.printf("Enable All Frames Including Control: %s\n", ENABLE_ALL_FRAMES ? "true" : "false");
    sprite.printf("Show Stats Bar On Top Of Log Menu: %s\n", ENABLE_STATS_IN_LOG_MENU ? "true" : "false");
    sprite.printf("Send Probe While In No Scan Mode: %s\n", ENABLE_PROBE_SENDING_NO_SCAN ? "true" : "false");

	xSemaphoreGive(drawSemaphore);
}

void drawStatsBar() {
    sprite.setTextColor(TFT_GOLD);
    sprite.printf("%.1f/%.1f%% %u/%u/%u+%u/%u/%uKB %s%is\n", cpuUsage0, cpuUsage1, ESP.getMaxAllocHeap() / 1000, ESP.getFreeHeap() / 1000, ESP.getHeapSize() / 1000, ESP.getMaxAllocPsram() / 1000, ESP.getFreePsram() / 1000, ESP.getPsramSize() / 1000, fileOpen ? "SD " : "", (int)millis() / 1000);
}

void draw() {
    if (xSemaphoreTake(drawSemaphore, 5000) == pdTRUE) {
        do { 
            sprite.fillRect(0, 0, sprite.width(), sprite.height(), TFT_BLACK);
            sprite.setCursor(0, 0);
            
            if (menu == 4) {
                for (const auto& sta: Unconnected_STA_Map) {
                    unsigned int packetc = sta.authc + sta.probec + sta.deauthc;
                    unsigned long eTime = millis() - sta.lastPacketUpdate;
                    unsigned long eTimeFromStart = millis() - sta.foundTime;
                    uint16_t bgcolor = sta.connected ? 0xd5e6 : (eTime > USTA_TIMEOUT ? 0xc000 : (eTimeFromStart < 1000 ? 0x04c0 : 0x0000));
                    sprite.setTextColor(TFT_WHITE, bgcolor); // green 0x04c0 red 0xc000 gold 0xd5e6
                    sprite.printf("%s|Pkt:%-2i|Pb:%-2i|Au:%-2i|De:%-2i|%i/%is\n", formatMAC(sta.mac), packetc, sta.probec, sta.authc, sta.deauthc, eTime / 1000, eTimeFromStart / 1000);
                }
                sprite.pushSprite(0, 0);
            }
            else if (menu == 3) {
                //TODO: clean this up
                static std::map<String, uint32_t> prevTaskRuntime;
                static uint32_t prevTotalRunTime = 0;
                const UBaseType_t taskCount = uxTaskGetNumberOfTasks();
                uint32_t totalRunTime = 0;
                TaskStatus_t* taskStatusArray = (TaskStatus_t*)pvPortMalloc(taskCount * sizeof(TaskStatus_t));

                if (taskStatusArray) {
                    taskCpuUsageList.clear();
                    totalRunTime = portGET_RUN_TIME_COUNTER_VALUE();
                    uxTaskGetSystemState(taskStatusArray, taskCount, &totalRunTime);
                    uint32_t elapsedTotalRunTime = totalRunTime - prevTotalRunTime;
                    prevTotalRunTime = totalRunTime;

                    for (UBaseType_t i = 0; i < taskCount; i++) {
                        String taskName(taskStatusArray[i].pcTaskName);
                        uint32_t prevRuntime = prevTaskRuntime[taskName];
                        uint32_t taskElapsedRuntime = taskStatusArray[i].ulRunTimeCounter - prevRuntime;
                        prevTaskRuntime[taskName] = taskStatusArray[i].ulRunTimeCounter;
                        float taskCpuUsage = (elapsedTotalRunTime > 0) ? ((float)taskElapsedRuntime / (float)elapsedTotalRunTime) * 100.0 : 0;
                        float taskCpuUsageAvg = (totalRunTime > 0) ? ((float)taskStatusArray[i].ulRunTimeCounter / (float)totalRunTime) * 100.0 : 0;
                        if (strcmp(taskStatusArray[i].pcTaskName, "IDLE0") == 0) cpuUsage0 = abs(100.0f - taskCpuUsage);
                        if (strcmp(taskStatusArray[i].pcTaskName, "IDLE1") == 0) cpuUsage1 = abs(100.0f - taskCpuUsage);
                        taskCpuUsageList.push_back({{taskCpuUsage, taskCpuUsageAvg}, taskStatusArray[i]});
                    }
                    sort(taskCpuUsageList.begin(), taskCpuUsageList.end(), [](const pair<pair<float, float>, TaskStatus_t>& a, const pair<pair<float, float>, TaskStatus_t>& b) {return a.first.first > b.first.first;});
                }

                drawStatsBar();
                sprite.setTextColor(TFT_CYAN);
                sprite.println("Name          Stats   CPU   Avg   Runtime    Heap   #");
                sprite.setTextColor(0xC7DE);

                for (const auto& task : taskCpuUsageList) {
                    sprite.printf("%-13s %-7s %4s%% %4s%% %-10u %-6u %-1u\n",
                        task.second.pcTaskName,
                        (task.second.eCurrentState == eRunning)   ? "Running" :
                        (task.second.eCurrentState == eReady)     ? "Ready" :
                        (task.second.eCurrentState == eBlocked)   ? "Blocked" :
                        (task.second.eCurrentState == eSuspended) ? "Paused" :
                        (task.second.eCurrentState == eDeleted)   ? "Deleting" : "idk",
                        (task.first.first > 99.9) ? " 100" : (task.first.first < 10.0) ? String(task.first.first, 2).c_str() : String(task.first.first, 1).c_str(),
                        (task.first.second > 99.9) ? " 100" : (task.first.second < 10.0) ? String(task.first.second, 2).c_str() : String(task.first.second, 1).c_str(),
                        task.second.ulRunTimeCounter,
                        task.second.usStackHighWaterMark,
                        task.second.xCoreID);
                }
	            sprite.setTextColor(0x9FE3);
			    sprite.printf("Current Channel AP Count: %u\n", AP_Map[channels[channel_index]].size());
                sprite.setTextColor(0xD69A);
                sprite.printf("Total Received Packets: %i\n", runtimeStats::packetc);
                sprite.setTextColor(0xA7FF);
                sprite.printf("Be:%-8i|", runtimeStats::beaconc);
                sprite.setTextColor(0xF410);
                sprite.printf("De:%-5i|", runtimeStats::deauthc);
                sprite.setTextColor(TFT_CYAN);
                sprite.printf("Pb:%-6i|", runtimeStats::probec);
                sprite.setTextColor(TFT_GREENYELLOW);
                sprite.printf("Da:%-10i|", runtimeStats::datac);
                sprite.setTextColor(TFT_YELLOW);
                sprite.printf("EP:%-4i\n", runtimeStats::eapolc);
                sprite.setTextColor(0xFA29);
                sprite.printf("Dropped Packets: %i\n", runtimeStats::droppedPackets);
                sprite.pushSprite(0, 0);
				free(taskStatusArray);
                break;
            }
            else if (menu == 2) {
                for (int i = 0; i < logListSize2; i++) {
                    int idx = (logListHead2 + i) % LOG_RING_SIZE;
                    sprite.setTextColor(logList2[idx].color);
                    sprite.println(logList2[idx].message);
                }
                sprite.pushSprite(0, 0);
                break;
            }
            else if (menu == 1) {
                #if ENABLE_STATS_IN_LOG_MENU
                drawStatsBar();
                #endif
                for (int i = 0; i < logListSize; i++) {
                    int idx = (logListHead + i) % LOG_RING_SIZE;
                    sprite.setTextColor(logList[idx].color);
                    sprite.println(logList[idx].message);
                }
                sprite.pushSprite(0, 0);
                break;
            }
            else if (menu == 0) {
                int rssi = (currentPacketCounts::packetc == 0) ? 0 : currentPacketCounts::rssitol / (int)currentPacketCounts::packetc;
                drawStatsBar();
                sprite.setTextColor(TFT_WHITE);
                sprite.printf("Ch:%-2i|", channels[channel_index]);
                sprite.setTextColor(0xD69A);
                sprite.printf("Pkt:%-5i|", currentPacketCounts::packetc);
                sprite.setTextColor(0xA7FF);
                sprite.printf("Be:%-2i|", currentPacketCounts::beaconc);
                sprite.setTextColor(0xF410);
                sprite.printf("De:%-2i|", currentPacketCounts::deauthc);
                sprite.setTextColor(TFT_CYAN);
                sprite.printf("Pb:%-2i|", currentPacketCounts::probec);
                sprite.setTextColor(TFT_GREENYELLOW);
                sprite.printf("Da:%-4i|", currentPacketCounts::datac);
                sprite.setTextColor(TFT_YELLOW);
                sprite.printf("EP:%-3i|", currentPacketCounts::eapolc);
                sprite.setTextColor((rssi > -60) ? 0xFFB6C1 : (rssi > -70) ? TFT_YELLOW : (rssi > -85) ? TFT_ORANGE : TFT_RED);
                sprite.printf("%-2i\n", rssi);
                sprite.setTextColor(TFT_WHITE);
                for (auto& ap : AP_Map[channels[channel_index]]) {
                    sprite.printf("%s|%i STA|", ap.ssid.c_str(), ap.STAs.size());
                    sprite.setTextColor((ap.rssi > -60) ? 0xFFB6C1 : (ap.rssi > -70) ? TFT_YELLOW : (ap.rssi > -85) ? TFT_ORANGE : TFT_RED);
                    sprite.printf("%-2i\n", ap.rssi); 
                    sprite.setTextColor(TFT_WHITE);
                    for (const auto& sta : ap.STAs) {
                        sprite.printf(" %02X:%02X:%02X:%02X:%02X:%02X ",MAC2STR(sta.mac));
                        sprite.setTextColor((sta.rssi > -60) ? 0xFFB6C1 : (sta.rssi > -70) ? TFT_YELLOW : (sta.rssi > -85) ? TFT_ORANGE : TFT_RED);
                        sprite.printf("%-2i", sta.rssi);
                        sprite.setTextColor(TFT_WHITE);
                        sprite.printf(" %-i/s %iR %iS %i\n", sta.packetCount, sta.packetsFromAP, sta.packetsToAP, sta.totalPacketCount);
                    }
                }
            }
            else menu = 0;
            sprite.pushSprite(0, 0);
        } while (0);
        xSemaphoreGive(drawSemaphore);
    } else Serial.println("nuh uh");
}
