#pragma once
#include <Arduino.h>
#include "config.h"

struct LogEntry {char message[MAX_LOG_LEN]; uint16_t color;};

QueueHandle_t logQueue1;
LogEntry logList[LOG_RING_SIZE];
int logListHead = 0, logListSize = 0;

void init_log1() {
    logQueue1 = xQueueCreate(LOG_QUEUE_SIZE, sizeof(LogEntry));
    if (!logQueue1) while (1);
    
    xTaskCreatePinnedToCore([](void*){
        char lastMessage[MAX_LOG_LEN] = "";
        int repeatCount = 1;

        while (1) {
            LogEntry entry;
            if (xQueueReceive(logQueue1, &entry, portMAX_DELAY) == pdTRUE) {
                if (strncmp(entry.message, lastMessage, MAX_LOG_LEN) == 0) {
                    repeatCount++;
                    snprintf(logList[(logListHead + logListSize - 1) % LOG_RING_SIZE].message, MAX_LOG_LEN, "%s x%d", lastMessage, repeatCount);
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

                            logList[(logListHead + logListSize) % LOG_RING_SIZE] = LogEntry{ "", entry.color };
                            strncpy(logList[(logListHead + logListSize) % LOG_RING_SIZE].message, part, MAX_LOG_LEN);
                            if (logListSize < LOG_RING_SIZE) logListSize++;
                            else logListHead = (logListHead + 1) % LOG_RING_SIZE;
                        }
                    } else {
                        logList[(logListHead + logListSize) % LOG_RING_SIZE] = entry;
                        if (logListSize < LOG_RING_SIZE) logListSize++;
                        else logListHead = (logListHead + 1) % LOG_RING_SIZE;
                    }
                }
            }
        }
    }, "log1", LOG_TASK_STACK_SIZE, NULL, 3, NULL, LOG_TASK_CORE);
}

void log1(uint16_t color, const char* fmt, ...) {
    LogEntry entry;
    entry.color = color;
    va_list args;
    va_start(args, fmt);
    vsnprintf(entry.message, MAX_LOG_LEN, fmt, args);
    va_end(args);
    xQueueSend(logQueue1, &entry, 0); // don't block
}

template<typename T>
void log1(uint16_t, const T&) {
    static_assert(sizeof(T) == 0, "ONLY CHAR* ALLOWED");
}
