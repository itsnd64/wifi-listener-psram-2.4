// copied from log.h LOL
#pragma once
#include <Arduino.h>
#include "config.h"
#include "log.h" // suspend "error", it doesnt even prevent compilation

QueueHandle_t logQueue2;
LogEntry logList2[LOG_RING_SIZE];
int logListHead2 = 0, logListSize2 = 0;


void init_log2() {
    logQueue2 = xQueueCreate(LOG_QUEUE_SIZE, sizeof(LogEntry));
    if (!logQueue2) while (1);
    
    xTaskCreatePinnedToCore([](void*){
        char lastMessage[MAX_LOG_LEN] = "";
        int repeatCount = 1;

        while (1) {
            LogEntry entry;
            if (xQueueReceive(logQueue2, &entry, portMAX_DELAY) == pdTRUE) {
                if (strncmp(entry.message, lastMessage, MAX_LOG_LEN) == 0) {
                    repeatCount++;
                    snprintf(logList2[(logListHead2 + logListSize2 - 1) % LOG_RING_SIZE].message, MAX_LOG_LEN, "%s x%d", lastMessage, repeatCount);
                } else {
                    repeatCount = 1;
                    strncpy(lastMessage, entry.message, MAX_LOG_LEN);

                    if (strchr(entry.message, '\n')) {
                        const char* p = entry.message;
                        while (*p) {
                            char part[MAX_LOG_LEN];
                            int i = 0;
                            while (i < MAX_LOG_LEN - 1 && p[i] && p[i] != '\n') part[i++] = *p++;
                            part[i] = '\0';
                            if (*p == '\n') p++;

                            logList2[(logListHead2 + logListSize2) % LOG_RING_SIZE] = LogEntry{ "", entry.color };
                            strncpy(logList2[(logListHead2 + logListSize2) % LOG_RING_SIZE].message, part, MAX_LOG_LEN);
                            if (logListSize2 < LOG_RING_SIZE) logListSize2++;
                            else logListHead2 = (logListHead2 + 1) % LOG_RING_SIZE;
                        }
                    } else {
                        logList2[(logListHead2 + logListSize2) % LOG_RING_SIZE] = entry;
                        if (logListSize2 < LOG_RING_SIZE) logListSize2++;
                        else logListHead2 = (logListHead2 + 1) % LOG_RING_SIZE;
                    }
                }
            }
        }
    }, "log2", LOG_TASK_STACK_SIZE, NULL, 3, NULL, LOG_TASK_CORE);
}

void log2(uint16_t color, const char* fmt, ...) {
    LogEntry entry;
    entry.color = color;
    va_list args;
    va_start(args, fmt);
    vsnprintf(entry.message, MAX_LOG_LEN, fmt, args);
    va_end(args);
    xQueueSend(logQueue2, &entry, 0); // don't block
}

template<typename T>
void log2(uint16_t, const T&) {
    static_assert(sizeof(T) == 0, "ONLY CHAR* ALLOWED");
}


typedef struct __attribute__((__packed__)) {
    uint32_t signature;
    uint32_t version;
    uint8_t message_pair;
    uint8_t essid_len;
    uint8_t essid[32];
    uint8_t keyver;
    uint8_t keymic[16];
    uint8_t mac_ap[6];
    uint8_t nonce_ap[32];
    uint8_t mac_sta[6];
    uint8_t nonce_sta[32];
    uint16_t eapol_len;
    uint8_t eapol[256];
} hccapx_t;

void printHCCAPX(const hccapx_t &h) {
    Serial.println("======HCCAPX======");
    Serial.printf("Signature: 0x%08X\n", h.signature);
    Serial.printf("Version: %u\n", h.version);
    Serial.printf("Message Pair: %u\n", h.message_pair);
    Serial.printf("ESSID Length: %u\n", h.essid_len);
    Serial.printf("ESSID: %.*s\n", h.essid_len, h.essid);

    Serial.printf("Key Version: %u\n", h.keyver);
    Serial.printf("Key MIC: %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
    h.keymic[0], h.keymic[1], h.keymic[2], h.keymic[3],
    h.keymic[4], h.keymic[5], h.keymic[6], h.keymic[7],
    h.keymic[8], h.keymic[9], h.keymic[10], h.keymic[11],
    h.keymic[12], h.keymic[13], h.keymic[14], h.keymic[15]);

    Serial.printf("AP MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
    h.mac_ap[0], h.mac_ap[1], h.mac_ap[2],
    h.mac_ap[3], h.mac_ap[4], h.mac_ap[5]);

    Serial.printf("AP Nonce: "
    "%02X%02X%02X%02X%02X%02X%02X%02X"
    "%02X%02X%02X%02X%02X%02X%02X%02X"
    "%02X%02X%02X%02X%02X%02X%02X%02X"
    "%02X%02X%02X%02X%02X%02X%02X%02X\n",
    h.nonce_ap[0], h.nonce_ap[1], h.nonce_ap[2], h.nonce_ap[3],
    h.nonce_ap[4], h.nonce_ap[5], h.nonce_ap[6], h.nonce_ap[7],
    h.nonce_ap[8], h.nonce_ap[9], h.nonce_ap[10], h.nonce_ap[11],
    h.nonce_ap[12], h.nonce_ap[13], h.nonce_ap[14], h.nonce_ap[15],
    h.nonce_ap[16], h.nonce_ap[17], h.nonce_ap[18], h.nonce_ap[19],
    h.nonce_ap[20], h.nonce_ap[21], h.nonce_ap[22], h.nonce_ap[23],
    h.nonce_ap[24], h.nonce_ap[25], h.nonce_ap[26], h.nonce_ap[27],
    h.nonce_ap[28], h.nonce_ap[29], h.nonce_ap[30], h.nonce_ap[31]);

    Serial.printf("STA MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
    h.mac_sta[0], h.mac_sta[1], h.mac_sta[2],
    h.mac_sta[3], h.mac_sta[4], h.mac_sta[5]);

    Serial.printf("STA Nonce: "
    "%02X%02X%02X%02X%02X%02X%02X%02X"
    "%02X%02X%02X%02X%02X%02X%02X%02X"
    "%02X%02X%02X%02X%02X%02X%02X%02X"
    "%02X%02X%02X%02X%02X%02X%02X%02X\n",
    h.nonce_sta[0], h.nonce_sta[1], h.nonce_sta[2], h.nonce_sta[3],
    h.nonce_sta[4], h.nonce_sta[5], h.nonce_sta[6], h.nonce_sta[7],
    h.nonce_sta[8], h.nonce_sta[9], h.nonce_sta[10], h.nonce_sta[11],
    h.nonce_sta[12], h.nonce_sta[13], h.nonce_sta[14], h.nonce_sta[15],
    h.nonce_sta[16], h.nonce_sta[17], h.nonce_sta[18], h.nonce_sta[19],
    h.nonce_sta[20], h.nonce_sta[21], h.nonce_sta[22], h.nonce_sta[23],
    h.nonce_sta[24], h.nonce_sta[25], h.nonce_sta[26], h.nonce_sta[27],
    h.nonce_sta[28], h.nonce_sta[29], h.nonce_sta[30], h.nonce_sta[31]);

    Serial.printf("EAPOL Length: %u\n", h.eapol_len);

    Serial.printf( // a bit big i think
        "EAPOL: "
        "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X"
        "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X"
        "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X"
        "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X"
        "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X"
        "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X"
        "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X"
        "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X"
        "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X"
        "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X"
        "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X"
        "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X"
        "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X"
        "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X"
        "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X"
        "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
        h.eapol[0],  h.eapol[1],  h.eapol[2],  h.eapol[3],  h.eapol[4],  h.eapol[5],  h.eapol[6],  h.eapol[7],
        h.eapol[8],  h.eapol[9],  h.eapol[10], h.eapol[11], h.eapol[12], h.eapol[13], h.eapol[14], h.eapol[15],
        h.eapol[16], h.eapol[17], h.eapol[18], h.eapol[19], h.eapol[20], h.eapol[21], h.eapol[22], h.eapol[23],
        h.eapol[24], h.eapol[25], h.eapol[26], h.eapol[27], h.eapol[28], h.eapol[29], h.eapol[30], h.eapol[31],
        h.eapol[32], h.eapol[33], h.eapol[34], h.eapol[35], h.eapol[36], h.eapol[37], h.eapol[38], h.eapol[39],
        h.eapol[40], h.eapol[41], h.eapol[42], h.eapol[43], h.eapol[44], h.eapol[45], h.eapol[46], h.eapol[47],
        h.eapol[48], h.eapol[49], h.eapol[50], h.eapol[51], h.eapol[52], h.eapol[53], h.eapol[54], h.eapol[55],
        h.eapol[56], h.eapol[57], h.eapol[58], h.eapol[59], h.eapol[60], h.eapol[61], h.eapol[62], h.eapol[63],
        h.eapol[64], h.eapol[65], h.eapol[66], h.eapol[67], h.eapol[68], h.eapol[69], h.eapol[70], h.eapol[71],
        h.eapol[72], h.eapol[73], h.eapol[74], h.eapol[75], h.eapol[76], h.eapol[77], h.eapol[78], h.eapol[79],
        h.eapol[80], h.eapol[81], h.eapol[82], h.eapol[83], h.eapol[84], h.eapol[85], h.eapol[86], h.eapol[87],
        h.eapol[88], h.eapol[89], h.eapol[90], h.eapol[91], h.eapol[92], h.eapol[93], h.eapol[94], h.eapol[95],
        h.eapol[96], h.eapol[97], h.eapol[98], h.eapol[99], h.eapol[100], h.eapol[101], h.eapol[102], h.eapol[103],
        h.eapol[104], h.eapol[105], h.eapol[106], h.eapol[107], h.eapol[108], h.eapol[109], h.eapol[110], h.eapol[111],
        h.eapol[112], h.eapol[113], h.eapol[114], h.eapol[115], h.eapol[116], h.eapol[117], h.eapol[118], h.eapol[119],
        h.eapol[120], h.eapol[121], h.eapol[122], h.eapol[123], h.eapol[124], h.eapol[125], h.eapol[126], h.eapol[127],
        h.eapol[128], h.eapol[129], h.eapol[130], h.eapol[131], h.eapol[132], h.eapol[133], h.eapol[134], h.eapol[135],
        h.eapol[136], h.eapol[137], h.eapol[138], h.eapol[139], h.eapol[140], h.eapol[141], h.eapol[142], h.eapol[143],
        h.eapol[144], h.eapol[145], h.eapol[146], h.eapol[147], h.eapol[148], h.eapol[149], h.eapol[150], h.eapol[151],
        h.eapol[152], h.eapol[153], h.eapol[154], h.eapol[155], h.eapol[156], h.eapol[157], h.eapol[158], h.eapol[159],
        h.eapol[160], h.eapol[161], h.eapol[162], h.eapol[163], h.eapol[164], h.eapol[165], h.eapol[166], h.eapol[167],
        h.eapol[168], h.eapol[169], h.eapol[170], h.eapol[171], h.eapol[172], h.eapol[173], h.eapol[174], h.eapol[175],
        h.eapol[176], h.eapol[177], h.eapol[178], h.eapol[179], h.eapol[180], h.eapol[181], h.eapol[182], h.eapol[183],
        h.eapol[184], h.eapol[185], h.eapol[186], h.eapol[187], h.eapol[188], h.eapol[189], h.eapol[190], h.eapol[191],
        h.eapol[192], h.eapol[193], h.eapol[194], h.eapol[195], h.eapol[196], h.eapol[197], h.eapol[198], h.eapol[199],
        h.eapol[200], h.eapol[201], h.eapol[202], h.eapol[203], h.eapol[204], h.eapol[205], h.eapol[206], h.eapol[207],
        h.eapol[208], h.eapol[209], h.eapol[210], h.eapol[211], h.eapol[212], h.eapol[213], h.eapol[214], h.eapol[215],
        h.eapol[216], h.eapol[217], h.eapol[218], h.eapol[219], h.eapol[220], h.eapol[221], h.eapol[222], h.eapol[223],
        h.eapol[224], h.eapol[225], h.eapol[226], h.eapol[227], h.eapol[228], h.eapol[229], h.eapol[230], h.eapol[231],
        h.eapol[232], h.eapol[233], h.eapol[234], h.eapol[235], h.eapol[236], h.eapol[237], h.eapol[238], h.eapol[239],
        h.eapol[240], h.eapol[241], h.eapol[242], h.eapol[243], h.eapol[244], h.eapol[245], h.eapol[246], h.eapol[247],
        h.eapol[248], h.eapol[249], h.eapol[250], h.eapol[251], h.eapol[252], h.eapol[253], h.eapol[254], h.eapol[255]);
}