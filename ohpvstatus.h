/*
 * ohpvstatus.h: configuration of the ohpvstatus plugin
 *
 * See the README file for copyright information and how to reach the author.
 */

#ifndef __OHPVSTATUS_H
#define __OHPVSTATUS_H

#include <vdr/tools.h>

enum eUnitMode { umAuto = 0, umW = 1, umKW = 2 };

struct cOhPvStatusConfig {
    // connection, set on the command line
    char openhabHost[256];
    int  openhabPort;
    bool useSSL;
    bool sslVerify;
    char apiToken[256];       // optional openHAB API token (sent as Bearer)

    // items, set on the command line
    char itemPV[128];
    char itemHouse[128];
    char itemSoC[128];
    char itemGrid[128];       // single item, positive = import, negative = export
    char itemGridIn[128];     // or two items: grid import (always positive)
    char itemGridOut[128];    //               grid export (always positive)
    char itemBattery[128];    // single item, positive = charging, negative = discharging
    char itemBatIn[128];      // or two items: battery charging (always positive)
    char itemBatOut[128];     //               battery discharging (always positive)

    // display options, stored in setup.conf
    int  updateInterval;      // seconds
    int  invertPV;
    int  invertGrid;
    int  invertBattery;
    int  invertHouse;
    int  unitMode;

    cOhPvStatusConfig(void) {
        strn0cpy(openhabHost, "127.0.0.1", sizeof(openhabHost));
        openhabPort = 8080;
        useSSL      = false;
        sslVerify   = true;
        apiToken[0] = '\0';

        strn0cpy(itemPV,      "PVPower",      sizeof(itemPV));
        strn0cpy(itemHouse,   "HousePower",   sizeof(itemHouse));
        strn0cpy(itemSoC,     "BatterySoC",   sizeof(itemSoC));
        strn0cpy(itemGrid,    "GridPower",    sizeof(itemGrid));
        itemGridIn[0]  = '\0';
        itemGridOut[0] = '\0';
        strn0cpy(itemBattery, "BatteryPower", sizeof(itemBattery));
        itemBatIn[0]   = '\0';
        itemBatOut[0]  = '\0';

        updateInterval = 10;
        invertPV       = 0;
        invertGrid     = 0;
        invertBattery  = 0;
        invertHouse    = 0;
        unitMode       = umAuto;
    }

    bool hasGridSplit(void) const { return itemGridIn[0] != '\0' && itemGridOut[0] != '\0'; }
    bool hasBatSplit(void)  const { return itemBatIn[0]  != '\0' && itemBatOut[0]  != '\0'; }
};

extern cOhPvStatusConfig OhPvStatusConfig;

#endif //__OHPVSTATUS_H
