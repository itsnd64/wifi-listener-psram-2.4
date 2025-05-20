#pragma once
#include <Arduino.h>
#include "display.h"

using namespace std;

uint32_t idleRunTime0 = 0, idleRunTime1 = 0;
float cpuUsage0 = 0, cpuUsage1 = 0;
extern int menu;

void init_monitor() {
    xTaskCreatePinnedToCore([](void* _) {
        while (1) {
            delay(1000);
			if (menu != 2) {
				static std::map<String, uint32_t> prevTaskRuntime;
				static uint32_t prevTotalRunTime = 0;
				const UBaseType_t taskCount = uxTaskGetNumberOfTasks();
				uint32_t totalRunTime = 0;
				totalRunTime = portGET_RUN_TIME_COUNTER_VALUE();
				TaskStatus_t* taskStatusArray = (TaskStatus_t*)pvPortMalloc(taskCount * sizeof(TaskStatus_t));
				if (taskStatusArray) {
					uxTaskGetSystemState(taskStatusArray, taskCount, &totalRunTime);
					uint32_t elapsedTotalRunTime = totalRunTime - prevTotalRunTime;
					prevTotalRunTime = totalRunTime;

					for (UBaseType_t i = 0; i < taskCount; i++) {
                        String taskName(taskStatusArray[i].pcTaskName);
                        uint32_t prevRuntime = prevTaskRuntime[taskName];
                        uint32_t taskElapsedRuntime = taskStatusArray[i].ulRunTimeCounter - prevRuntime;
                        prevTaskRuntime[taskName] = taskStatusArray[i].ulRunTimeCounter;
                        float taskCpuUsage = (elapsedTotalRunTime > 0) ? ((float)taskElapsedRuntime / (float)elapsedTotalRunTime) * 100.0 : 0;
                        if (strcmp(taskStatusArray[i].pcTaskName, "IDLE0") == 0) cpuUsage0 = abs(100.0f - taskCpuUsage);
                        if (strcmp(taskStatusArray[i].pcTaskName, "IDLE1") == 0) cpuUsage1 = abs(100.0f - taskCpuUsage);
					}
					vPortFree(taskStatusArray);
				}
			}
        }
    }, "mon dude", MONITOR_TASK_STACK_SIZE, NULL, 3, NULL, MONITOR_TASK_CORE);
}