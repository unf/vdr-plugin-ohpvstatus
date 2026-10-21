#ifndef __OPENHAB_H
#define __OPENHAB_H

#include <string>

struct cEnergyData {
    double pvPower;        // W
    double gridPower;      // W, positive=import, negative=export
    double batteryPower;   // W, positive=charging, negative=discharging
    double housePower;     // W
    double batterySoC;     // percent 0-100
    bool   valid;

    cEnergyData()
    : pvPower(0), gridPower(0), batteryPower(0), housePower(0), batterySoC(0), valid(false) {}
};

bool OpenHabFetchItem(const std::string &host, int port, bool useSSL, bool sslVerify,
                      const std::string &itemName, double &value);

cEnergyData OpenHabFetchAll();

#endif //__OPENHAB_H
