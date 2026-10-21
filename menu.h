#ifndef __MENU_H
#define __MENU_H

#include <vdr/osdbase.h>
#include <vdr/menuitems.h>
#include <vdr/plugin.h>
#include "openhab.h"

class cMenuOhPvStatus : public cOsdMenu {
private:
    cEnergyData data_;
    time_t      lastUpdate_;

    void        BuildDisplay();
    void        Fetch();
    cString     FormatPower(double watts) const;
    cString     FormatSoCBar(double soc) const;

public:
    cMenuOhPvStatus();
    virtual eOSState ProcessKey(eKeys Key);
};

class cMenuOhPvStatusSetup : public cMenuSetupPage {
private:
    int  tempInterval_;
    int  tempInvertPV_;
    int  tempInvertGrid_;
    int  tempInvertBattery_;
    int  tempInvertHouse_;
    int  tempUnitMode_;

    static const char * const unitModeNames_[];

protected:
    virtual void Store();

public:
    cMenuOhPvStatusSetup();
};

#endif //__MENU_H
