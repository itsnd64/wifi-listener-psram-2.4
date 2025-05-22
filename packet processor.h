#pragma once
#include <Arduino.h>
#include "config.h"
#include "esp_wifi.h"
#include "wifi.h"
#include "sd card.h"
#include "TFT_eSPI.h"
#include "eapol.h"
#include "misc.h"
#include "mac cache.h"

QueueHandle_t pktQueue;

struct packet_entry { // bad name ik
    wifi_promiscuous_pkt_t *data;
    wifi_promiscuous_pkt_type_t type;
};

void IRAM_ATTR addPkt(wifi_promiscuous_pkt_t *pkt, wifi_promiscuous_pkt_type_t type) {
    packet_entry packet;
	int totalSize = sizeof(wifi_pkt_rx_ctrl_t) + pkt->rx_ctrl.sig_len;
	packet.data = (wifi_promiscuous_pkt_t*)ps_malloc(totalSize);
	assert(packet.data);
	memcpy(&packet.data->rx_ctrl, &pkt->rx_ctrl, sizeof(wifi_pkt_rx_ctrl_t));
	memcpy(packet.data->payload, pkt->payload, pkt->rx_ctrl.sig_len);

    if (!xQueueSendFromISR(pktQueue, &packet, NULL)) {
		runtimeStats::droppedPackets++;
		free(packet.data);
	}
}

void handle_packet(wifi_promiscuous_pkt_t *pkt, wifi_promiscuous_pkt_type_t type) {
	uint16_t pktlen = (type == WIFI_PKT_MGMT) ? pkt->rx_ctrl.sig_len - 4 : pkt->rx_ctrl.sig_len;
    uint8_t pktType = pkt->payload[0] & 0x0C;
    uint8_t pktSubtype = pkt->payload[0] & 0xF0;
	int rssi = pkt->rx_ctrl.rssi;

	array<uint8_t, 6> macFrom;
	array<uint8_t, 6> macTo;
	array<uint8_t, 6> bssid;
	memcpy(macFrom.data(), pkt->payload + 4, 6);
	memcpy(macTo.data(), pkt->payload + 10, 6);
	memcpy(bssid.data(), pkt->payload + 16, 6);

	const char* macFromName = BSSID2NAME(macFrom, false);
	const char* macToName = BSSID2NAME(macTo, false);

	currentPacketCounts::packetc++;
	currentPacketCounts::rssitol += rssi;
	runtimeStats::packetc++;

	addPktSD(millis() / 1000, micros(), pktlen, pkt->payload);

	for_each_aps if (memcmp(ap.bssid.data(), macTo.data(), 6) == 0) {
		ap.packetc++;
		ap.rssitol += rssi;
		break;
	};

	if (pktType == 0x04) return; // dont parse random pkts
    if (pktType == 0x00) { // management
        if      (pktSubtype == 0x40) {
			currentPacketCounts::probec++;
			runtimeStats::probec++;
			if (isBroadcast(macFrom)) log1(TFT_CYAN, "Probe Broadcast: %s", macToName);
			else log1(TFT_CYAN, "Probe: %s -> %s", macToName, macFromName);
			adduSTA(PKT_PROBE, macFrom, macTo, bssid);
		}
		else if (pktSubtype == 0x50) {
			currentPacketCounts::probec++;
			runtimeStats::probec++;
			log1(TFT_SKYBLUE, "Probe: %s -> %s", macToName, macFromName);
			adduSTA(PKT_PROBE, macFrom, macTo, bssid);
			addAP(pkt, pktlen);
		}
		else if (pktSubtype == 0xB0 || pktSubtype == 0x00 || pktSubtype == 0x10) { // auth,assoc
			//remove sta bc its connecting
			if (!disable_auto_delete_sta) for_each_aps { ap.STAs.erase(remove_if(ap.STAs.begin(), ap.STAs.end(), [&](const STA_Info& sta) { if (macTo == sta.mac) {log1(TFT_DARKGREEN, "~ %s AP: %s", BSSID2NAME(sta.mac), BSSID2NAME(ap.bssid));return true;}return false;}), ap.STAs.end());}
			if (pktSubtype == 0xB0) log1(TFT_ORANGE, "%s Auth: %s -> %s", (pkt->payload[24] == 0x03 && pkt->payload[25] == 0x00) ? "S" : "W", macToName, macFromName);
			else log1(TFT_ORANGE, "Assoc: %s -> %s", macToName, macFromName);
			adduSTA(PKT_AUTH, macFrom, macTo, bssid);
			return; // return to prevent adding sta when its connecting
		}
		else if (pktSubtype == 0x20 || pktSubtype == 0x30) {
			if (!disable_auto_delete_sta) for_each_aps { ap.STAs.erase(remove_if(ap.STAs.begin(), ap.STAs.end(), [&](const STA_Info& sta) { if (macTo == sta.mac) {log1(TFT_DARKGREEN, "~ %s AP: %s", BSSID2NAME(sta.mac), BSSID2NAME(ap.bssid));return true;}return false;}), ap.STAs.end());}
			log1(TFT_ORANGE, "Reassoc: %s -> %s", BSSID2NAME(macTo, false), BSSID2NAME(macFrom, false));
			return; // return to prevent adding sta when its reconnecting
		}
        else if (pktSubtype == 0xC0 || pktSubtype == 0xA0) { //deauth,dissoc, might be bad to parse this(deauther) but oh well
			currentPacketCounts::deauthc++;
			runtimeStats::deauthc++;
			if (isBroadcast(macFrom)) log1(0xF410, "%s Broadcast: %s", ((pktSubtype == 0xC0) ? "Deauth" : "Dissoc"), macToName);
			else log1(0xF410, "%s: %s -> %s", ((pktSubtype == 0xC0) ? "Deauth" : "Dissoc"), macToName, macFromName);
			#if DEAUTH_REMOVE_STA
			if (!disable_auto_delete_sta) removeSTA(macTo);
			#endif
			adduSTA(PKT_DEAUTH, macFrom, macTo, bssid);
		} 
        else if (pktSubtype == 0x80) {  //...beacon
			currentPacketCounts::beaconc++;
			runtimeStats::beaconc++;
			addAP(pkt, pktlen);
		}
    }

	// process data frame
	// Serial.println(pktType == 0x08 ? "Data" : pktType == 0x00 ? "Mgmt" : pktType == 0x04 ? "Ctrl" : "Misc");

	if (pktType != 0x08 || pktlen < 28) return;

    if ((pkt->payload[30] == 0x88 && pkt->payload[31] == 0x8e) || (pkt->payload[32] == 0x88 && pkt->payload[33] == 0x8e)) { // eapol
		currentPacketCounts::eapolc++;
		runtimeStats::eapolc++;
		for_each_aps if ((ap.bssid ==  macTo) || (ap.bssid ==  macFrom)) {
			const char* tmp = eapol_add_frame((data_frame_t*)pkt->payload, ap);
			log1(TFT_GOLD, "%s: %s -> %s", tmp, BSSID2NAME(macFrom, false), BSSID2NAME(macTo, false));
			log2(TFT_GOLD, "%s: %s -> %s", tmp, BSSID2NAME(macFrom, false), BSSID2NAME(macTo, false));
		}
		return; // i dont want it to add sta when its connecting so return
	}

	currentPacketCounts::datac++;
	runtimeStats::datac++;

	if (!(macValid(macFrom) || macValid(macTo))) {
		Serial.printf("Invalid MAC: %s -> %s\n", BSSID2NAME(macFrom, false), BSSID2NAME(macTo, false));
		return;
	};

	// adding sta
	for_each_aps {
		if (bssid == ap.bssid && (ap.bssid == macFrom || ap.bssid == macTo)) {
			array<uint8_t, 6> targetMac = (ap.bssid == macFrom) ? macTo : macFrom;
			for_each_sta(ap) if (targetMac == sta.mac) {
				sta.packetCount++;
				sta.totalPacketCount++;
				(ap.bssid == macFrom ? sta.packetsFromAP : sta.packetsToAP)++;
				sta.rssitol += rssi;
				return;
			}
			addSTA(ap, targetMac);
		}
	}
}

void init_packet_processor() {
    pktQueue = xQueueCreate(PACKET_QUEUE_SIZE, sizeof(packet_entry));
    assert(pktQueue);
    xTaskCreatePinnedToCore([](void* _) {
        packet_entry packet;
        while (1) {
            while (xQueueReceive(pktQueue, &packet, 0) == pdTRUE) {
                handle_packet(packet.data, packet.type);
                free(packet.data);
            }
            delay(1);
        }
    }, "pkt proc", PACKET_PROCESSOR_STACK_SIZE, NULL, 2, NULL, PACKET_PROCESSOR_CORE);
}