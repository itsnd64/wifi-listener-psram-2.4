#pragma once
// config file woooooooooooooo

// tasks stack size
#define SETUP_TASK_STACK_SIZE 30000 // hehe ez counter to beacon flood
#define UPDATE_TASK_STACK_SIZE 2500
#define USTA_UPDATE_TASK_STACK_SIZE 3000
#define AP_STA_UPDATE_TASK_STACK_SIZE 3000
#define BUTTON_TASK_STACK_SIZE 3000 // this stupid shit uses so much memory
#define MONITOR_TASK_STACK_SIZE 2000
#define DEAUTH_STACK_SIZE 1500
#define SD_TASK_STACK_SIZE 3000
#define LOG_TASK_STACK_SIZE 2000
#define PROBE_TASK_STACK_SIZE 1200
#define FLUSH_TASK_STACK_SIZE 1500
#define PACKET_PROCESSOR_STACK_SIZE 16000

// tasks core
#define UPDATE_TASK_CORE 1 // calls draw()
#define USTA_UPDATE_TASK_CORE 1
#define AP_STA_UPDATE_TASK_CORE 1
#define SD_TASK_CORE 1
#define LOG_TASK_CORE 1
#define MONITOR_TASK_CORE 1
#define FLUSH_TASK_CORE 1
#define DEAUTH_TASK_CORE 0
#define PROBE_TASK_CORE 0
#define BUTTON_TASK_CORE 1
#define PACKET_PROCESSOR_CORE 1

// intervals
#define LOG_MENU_UPDATE_INTERVAL 25
#define BUTTON_TICK_INTERVAL 3
#define UPDATE_TASK_INTERVAL 500
#define DEAUTH_INTERVAL 50
#define PROBE_INTERVAL 1000
#define FLUSH_INTERVAL 3500

// others
#define WDT_TIMEOUT 2147483647 // s
#define WDT_PANIC false // its finally able to sustain for very long under beacon flood attack(~450 pkt /s)
#define USTA_TIMEOUT 20000 // ms
// #define AP_TIMEOUT 10000 // ms
#define AP_SSID_MAX_LEN 18
#define SCAN_TIME_MIN 150
#define SCAN_TIME_MAX 200
#define MAX_LOG_LEN 64
#define BASIC_FRAMES true // only mgmt and data(prob dont need filter for this), WILL MESS UP BUNCH OF THINGS AND MALFORM PACKETS IF FALSE!
// #define CUSTOM_FILTER // my own filter
#define ENABLE_STATS_IN_LOG_MENU false
#define ENABLE_PROBE_SENDING_NO_SCAN true
#define DEAUTH_REMOVE_STA true // remove sta if received deauth pkt from/to sta
// #define DONT_ADD_AP_NO_SCAN // what the point then? to prevent crashing from beacon spamming TODO: comment this after i finish the testing



#if ENABLE_STATS_IN_LOG_MENU
#define LOG_LIST_MAX_SIZE 29
#else
#define LOG_LIST_MAX_SIZE 30
#endif

// Buffer sizes
#define LOG_QUEUE_SIZE 64
#define LOG_RING_SIZE 30
#define SD_QUEUE_SIZE 500 // a bit big
#define PACKET_QUEUE_SIZE 1000