/*
 * menu.h: status display and setup page of the ohpvstatus plugin
 *
 * See the README file for copyright information and how to reach the author.
 */

#ifndef __OHPVSTATUS_MENU_H
#define __OHPVSTATUS_MENU_H

#include <vdr/menuitems.h>
#include <vdr/osdbase.h>
#include "openhab.h"

class cMenuOhPvStatus : public cOsdMenu {
private:
    cOpenHabPoller poller_;
    cEnergyData    data_;
    bool           haveData_;

    void    BuildDisplay(void);
    void    AddPower(const char *Label, const cEnergyValue &Value, bool Invert);
    void    AddEnergy(const char *Label, const cEnergyValue &Value);
    void    AddLine(int Index, const cEnergyValue &Value);
    cString FormatEnergy(double Wh) const;
    void    AddSignedPower(const char *LabelPos, const char *LabelNeg, const char *LabelNone,
                           const cEnergyValue &Value, bool Invert);
    cString FormatPower(double Watts) const;
    cString FormatSoCBar(double SoC) const;

public:
    cMenuOhPvStatus(void);
    virtual eOSState ProcessKey(eKeys Key);
};

class cMenuOhPvStatusSetup : public cMenuSetupPage {
private:
    int tempInterval_;
    int tempInvertPV_;
    int tempInvertGrid_;
    int tempInvertBattery_;
    int tempInvertHouse_;
    int tempUnitMode_;
    const char *unitModeNames_[3];

protected:
    virtual void Store(void);

public:
    cMenuOhPvStatusSetup(void);
};

#endif //__OHPVSTATUS_MENU_H
