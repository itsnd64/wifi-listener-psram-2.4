#pragma once
#include <Arduino.h>

namespace runtimeStats {
    unsigned long packetc = 0, beaconc = 0, deauthc = 0, probec = 0, datac = 0, eapolc = 0, droppedPackets = 0;
    unsigned int staAddedCount, staRemovedCount;
}