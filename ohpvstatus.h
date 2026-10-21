#ifndef __OHPVSTATUS_H
#define __OHPVSTATUS_H

#include <vdr/plugin.h>
#include <cstring>

enum eUnitMode { umAuto = 0, umW = 1, umKW = 2 };

struct cOhPvStatusConfig {
    // Connection - set via command line args
    char openhabHost[256];
    int  openhabPort;
    bool useSSL;
    bool sslVerify;

    // Standard items
    char itemPV[128];
    char itemHouse[128];
    char itemSoC[128];

    // Grid: either single item (with sign) or separate in/out
    char itemGrid[128];       // single item, positive=import negative=export
    char itemGridIn[128];     // separate: grid import (always positive)
    char itemGridOut[128];    // separate: grid export (always positive)

    // Battery power: either single item (with sign) or separate in/out
    char itemBattery[128];    // single item, positive=charging negative=discharging
    char itemBatIn[128];      // separate: battery charging (always positive)
    char itemBatOut[128];     // separate: battery discharging (always positive)

    // Display options - stored in setup.conf
    int  updateInterval;
    int  invertPV;
    int  invertGrid;
    int  invertBattery;
    int  invertHouse;
    int  unitMode;

    cOhPvStatusConfig() {
        strncpy(openhabHost, "127.0.0.1", sizeof(openhabHost));
        openhabPort = 8080;
        useSSL      = false;
        sslVerify   = true;

        strncpy(itemPV,      "PVPower",      sizeof(itemPV));
        strncpy(itemHouse,   "HousePower",   sizeof(itemHouse));
        strncpy(itemSoC,     "BatterySoC",   sizeof(itemSoC));
        strncpy(itemGrid,    "GridPower",    sizeof(itemGrid));
        itemGridIn[0]  = '\0';
        itemGridOut[0] = '\0';
        strncpy(itemBattery, "BatteryPower", sizeof(itemBattery));
        itemBatIn[0]   = '\0';
        itemBatOut[0]  = '\0';

        updateInterval = 10;
        invertPV       = 0;
        invertGrid     = 0;
        invertBattery  = 0;
        invertHouse    = 0;
        unitMode       = umAuto;
    }

    // True if separate grid items are configured
    bool hasGridSplit() const { return itemGridIn[0] != '\0' && itemGridOut[0] != '\0'; }
    // True if separate battery items are configured
    bool hasBatSplit()  const { return itemBatIn[0]  != '\0' && itemBatOut[0]  != '\0'; }
};

extern cOhPvStatusConfig OhPvStatusConfig;

#endif //__OHPVSTATUS_H
