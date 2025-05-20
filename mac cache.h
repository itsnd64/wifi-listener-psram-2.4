#pragma once
#include <unordered_map>
#include <array>
#include <string>
#include <algorithm>
#include <vector>
#include "wifi.h"

using namespace std;

struct MACHasher {
    size_t operator()(const array<uint8_t, 6>& mac) const noexcept {
        size_t hash = 0;
        for (uint8_t b : mac) hash = (hash * 31) ^ b;
        return hash;
    }
};


unordered_map<array<uint8_t, 6>, string, MACHasher> macNameCache;


const char* BSSID2NAME(const array<uint8_t, 6>& mac, bool checkSTA = true) {
    auto it = macNameCache.find(mac);
    if (it != macNameCache.end()) return it->second.c_str();

    string result;

    for (auto& [channel, aps] : AP_Map) {
        for (auto& ap : aps) {
            if (mac == ap.bssid) {
                result = ap.ssid.c_str();  // If AP_Info.ssid is String, convert to std::string
                macNameCache[mac] = result;
                return macNameCache[mac].c_str();
            }
            if (checkSTA) {
                for (const auto& sta : ap.STAs) {
                    if (mac == sta.mac) {
                        char tmp[18];
                        snprintf(tmp, sizeof(tmp), MACSTR, MAC2STR(mac.data()));
                        result = string(tmp);
                        transform(result.begin(), result.end(), result.begin(), ::toupper);
                        macNameCache[mac] = result;
                        return macNameCache[mac].c_str();
                    }
                }
            }
        }
    }

    // Default fallback to MAC string uppercase
    char tmp[18];
    snprintf(tmp, sizeof(tmp), MACSTR, MAC2STR(mac.data()));
    result = string(tmp);
    transform(result.begin(), result.end(), result.begin(), ::toupper);
    macNameCache[mac] = result;
    return macNameCache[mac].c_str();
}

const char* BSSID2NAME(const char* macStr, bool checkSTA = true) {
    std::array<uint8_t, 6> mac;
    if (sscanf(macStr, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) != 6) return macStr;
    return BSSID2NAME(mac, checkSTA);
}


void addSTA(AP_Info &ap, array<uint8_t, 6> mac) {
	STA_Info newSTA;
	newSTA.mac = mac;
	newSTA.lastPacketUpdate = millis();
	ap.STAs.push_back(newSTA);
    if (macNameCache.find(mac) == macNameCache.end()) {
        char tmp[18];
        snprintf(tmp, sizeof(tmp), MACSTR, MAC2STR(mac.data()));
        std::string result(tmp);
        std::transform(result.begin(), result.end(), result.begin(), ::toupper);
        macNameCache[mac] = result;
    }
	log1(0xB7E0, "+ %s AP: %s", BSSID2NAME(mac, false), ap.ssid.c_str());
}

void removeSTA(const array<uint8_t, 6>& mac) {
    for (auto& [channel, aps] : AP_Map) {
        for (auto& ap : aps) {
            auto& staList = ap.STAs;
            auto it = remove_if(staList.begin(), staList.end(), [&](const STA_Info& sta) {
                return sta.mac == mac;
            });
            if (it != staList.end()) {
                staList.erase(it, staList.end());
                macNameCache.erase(mac);
                return;
            }
        }
    }
}

void removeAP(const array<uint8_t, 6>& bssid) {
    for (auto& [channel, aps] : AP_Map) {
        auto it = remove_if(aps.begin(), aps.end(), [&](const AP_Info& ap) {
            if (ap.bssid == bssid) {
                for (const auto& sta : ap.STAs) macNameCache.erase(sta.mac);
                macNameCache.erase(ap.bssid);
                return true;
            }
            return false;
        });
        if (it != aps.end()) {
            aps.erase(it, aps.end());
            return;
        }
    }
}