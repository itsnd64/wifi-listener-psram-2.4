#pragma once
#include <Arduino.h>
#include <esp_wifi.h>
#include "wifi.h"
#include "hccapx.h"
#include "mac cache.h"

typedef struct {
	uint8_t version;
	uint8_t packet_type;
	uint16_t packet_body_length;
	uint8_t packet_body[];
} eapol_packet_t;
typedef struct __attribute__((__packed__)) {
    uint8_t a;uint8_t b[2];uint16_t c;uint8_t d[8];
    uint8_t key_nonce[32];uint8_t e[16];uint8_t f[8];uint8_t g[8];
    uint8_t key_mic[16];uint16_t h;uint8_t i[];
} eapol_key_packet_t;
typedef struct {
    uint8_t a:2;uint8_t b:2;
    uint8_t subtype:4;uint8_t c:1;uint8_t d:1;uint8_t e:1;
    uint8_t retry:1;uint8_t f:1;uint8_t g:1;uint8_t h:1;uint8_t i:1;
} frame_control_t;
typedef struct {
    frame_control_t frame_control;uint16_t a;
    uint8_t addr1[6];
    uint8_t addr2[6];
    uint8_t addr3[6];uint16_t b;
    uint8_t body[];
} data_frame_t;

static bool is_array_zero(uint8_t *array, unsigned size) {
    for(unsigned i = 0; i < size; i++) if (array[i] != 0) return false;
    return true;
}

static void save_eapol(HCCAPX_Entry *entry, eapol_packet_t *eapol_packet, eapol_key_packet_t *eapol_key_packet) {
    unsigned eapol_len = 4 + ntohs(eapol_packet->packet_body_length);
    if(eapol_len > 256) {
        Serial.printf("EAPoL is too long (%u/%u)", eapol_len, 256);
        Serial.println("Error saving EAPoL packet.");
        log1(0xF800, "Error saving EAPoL packet.");
        log2(0xF800, "Error saving EAPoL packet.");
        return;
    }
    entry->hccapx.eapol_len = eapol_len;
    memcpy(entry->hccapx.eapol, eapol_packet, entry->hccapx.eapol_len);
    memcpy(entry->hccapx.keymic, eapol_key_packet->key_mic, 16);
    memset(&entry->hccapx.eapol[81], 0x0, 16);
}

static bool sta_message(HCCAPX_Entry *entry, data_frame_t *frame, eapol_packet_t* eapol_packet, eapol_key_packet_t* eapol_key_packet) {
    if (!is_array_zero(eapol_key_packet->key_nonce, 16)) {
        Serial.println("From STA M2");
        

        if (entry && entry->message_sta == 0) {
            memcpy(entry->hccapx.nonce_sta, eapol_key_packet->key_nonce, 32);
            save_eapol(entry, eapol_packet, eapol_key_packet);
            entry->message_sta = 2;
            log2(0x07E0, "M2 saved for AP: %s", BSSID2NAME((char*)(frame->addr3)));
        }

        if (entry && entry->message_ap == 1) entry->hccapx.message_pair = 0;  // M1 + M2
        return true;
    } else {
        Serial.println("From STA M4");
        return false;
    }
}

static bool ap_message(HCCAPX_Entry *entry, data_frame_t *frame, eapol_packet_t* eapol_packet, eapol_key_packet_t *eapol_key_packet) {
    if (entry && entry->message_ap == 0) memcpy(entry->hccapx.mac_ap, frame->addr2, 6);
    if (is_array_zero(eapol_key_packet->key_mic, 16)) {
        Serial.println("From AP M1");
        if (entry && entry->message_ap == 0) {
            memcpy(entry->hccapx.nonce_ap, eapol_key_packet->key_nonce, 32);
            entry->message_ap = 1;
            log2(0x07E0, "M1 saved for AP: %s", BSSID2NAME((char*)(frame->addr3)));
        }
        return true;
    }
    else { // no need to save m3
        Serial.println("From AP M3");
        return false;
    }
}

const char* eapol_add_frame(data_frame_t *frame, AP_Info &ap) {
    eapol_packet_t *eapol_packet = (eapol_packet_t*)(frame->body + 10);
    eapol_key_packet_t *eapol_key_packet = (eapol_key_packet_t*)eapol_packet->packet_body;

    uint8_t sta_mac[6];
    if (memcmp(frame->addr2, ap.bssid.data(), 6) == 0) memcpy(sta_mac, frame->addr1, 6);
    else memcpy(sta_mac, frame->addr2, 6);

    HCCAPX_Entry *entry_ptr = NULL;

    if (!ap.hccapx_saved) {
        for (auto &entry : ap.hccapx_list) {
            if (memcmp(entry.hccapx.mac_sta, sta_mac, 6) == 0) {
                entry_ptr = &entry;
                break;
            }
        }

        if (!entry_ptr) { // add hccapx entry
            HCCAPX_Entry entry = {};
            entry.hccapx.signature = 0x58504348;
            entry.hccapx.version = 4;
            entry.hccapx.message_pair = 255;
            entry.hccapx.keyver = 2;
            entry.hccapx.essid_len = ap.ssid.length();
            memcpy(entry.hccapx.essid, ap.ssid.c_str(), entry.hccapx.essid_len);
            memcpy(entry.hccapx.mac_sta, sta_mac, 6);
            memcpy(entry.hccapx.mac_ap, ap.bssid.data(), 6);
            
            ap.hccapx_list.push_back(std::move(entry));

            log2(0x07E0, "New hccapx entry for STA: %s", BSSID2NAME((char*)sta_mac, false));
            entry_ptr = &ap.hccapx_list.back();
        }
        if (!entry_ptr) { //FIXME: shouldnt be reached
            Serial.println("No hccapx entry for STA");
            log2(0xF800, "No hccapx entry for STA");
        }
    }

    Serial.println(entry_ptr == NULL ? "No entry found" : "Entry found");
    if (memcmp(frame->addr2, frame->addr3, 6) == 0) {
        if (ap_message(entry_ptr, frame, eapol_packet, eapol_key_packet)) {
            ap.m1++;
            return "M1";
        } else {
            ap.m3++;
            return "M3";
        }
    } else if (memcmp(frame->addr1, frame->addr3, 6) == 0) {
        if (sta_message(entry_ptr, frame, eapol_packet, eapol_key_packet)) {
            ap.m2++;
            return "M2";
        } else {
            ap.m4++;
            return "M4";
        }
    } else {
        Serial.println("Unknown frame format. BSSID is not source nor destination.");
        log1(0xF800, "Invalid EAPOL!");
        log2(0xF800, "Invalid EAPOL!");
        return "M?";
    }
}