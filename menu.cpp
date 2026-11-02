/*
 * menu.cpp: status display and setup page of the ohpvstatus plugin
 *
 * See the README file for copyright information and how to reach the author.
 */

#include "menu.h"
#include "ohpvstatus.h"
#include <vdr/plugin.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>

#define BARLEN 30

// ---- formatting --------------------------------------------------------------

cString cMenuOhPvStatus::FormatPower(double Watts) const
{
    int mode = OhPvStatusConfig.unitMode;
    bool useKW = (mode == umKW) || (mode == umAuto && std::fabs(Watts) >= 1000.0);
    if (useKW)
        return cString::sprintf("%.2f kW", Watts / 1000.0);
    return cString::sprintf("%.0f W", Watts);
}

cString cMenuOhPvStatus::FormatEnergy(double Wh) const
{
    int mode = OhPvStatusConfig.unitMode;
    bool useKWh = (mode == umKW) || (mode == umAuto && std::fabs(Wh) >= 1000.0);
    if (useKWh)
        return cString::sprintf("%.2f kWh", Wh / 1000.0);
    return cString::sprintf("%.0f Wh", Wh);
}

// '|' is drawn as a bar segment by the vdrsymbols fonts
cString cMenuOhPvStatus::FormatSoCBar(double SoC) const
{
    if (SoC < 0)
        SoC = 0;
    if (SoC > 100)
        SoC = 100;
    int filled = (int)((SoC / 100.0) * BARLEN + 0.5);
    char bar[BARLEN + 3];
    bar[0] = '[';
    memset(bar + 1, '|', filled);
    memset(bar + 1 + filled, ' ', BARLEN - filled);
    bar[BARLEN + 1] = ']';
    bar[BARLEN + 2] = '\0';
    return bar;
}

// ---- cMenuOhPvStatus ---------------------------------------------------------
// three columns: label | value | bar

cMenuOhPvStatus::cMenuOhPvStatus(void)
: cOsdMenu(tr("openHAB PV Status"), 20, 12), haveData_(false)
{
    SetHelp(tr("Refresh"), NULL, NULL, tr("Setup"));
    poller_.Start();
    BuildDisplay();
}

void cMenuOhPvStatus::AddPower(const char *Label, const cEnergyValue &Value, bool Invert)
{
    if (Value.valid)
        Add(new cOsdItem(cString::sprintf("%s\t%s", Label, *FormatPower(Invert ? -Value.value : Value.value)), osUnknown, false));
    else
        Add(new cOsdItem(cString::sprintf("%s\t%s", Label, tr("n/a")), osUnknown, false));
}

void cMenuOhPvStatus::AddEnergy(const char *Label, const cEnergyValue &Value)
{
    if (Value.valid)
        Add(new cOsdItem(cString::sprintf("%s\t%s", Label, *FormatEnergy(Value.value)), osUnknown, false));
    else
        Add(new cOsdItem(cString::sprintf("%s\t%s", Label, tr("n/a")), osUnknown, false));
}

// a free line shows the number with the configured decimals and the unit as
// openHAB sent it, a text state (e.g. "ON") as it is
void cMenuOhPvStatus::AddLine(int Index, const cEnergyValue &Value)
{
    const cOhPvStatusConfig &c = OhPvStatusConfig;
    cString label = cString::sprintf("%s:", c.labelLine[Index][0] ? c.labelLine[Index] : c.itemLine[Index]);
    cString value;
    if (Value.valid)
        value = cString::sprintf("%.*f%s%s", c.decimalsLine[Index], Value.raw, Value.unit[0] ? " " : "", Value.unit);
    else if (Value.present)
        value = Value.text;
    else
        value = tr("n/a");
    Add(new cOsdItem(cString::sprintf("%s\t%s", *label, *value), osUnknown, false));
}

void cMenuOhPvStatus::AddSignedPower(const char *LabelPos, const char *LabelNeg, const char *LabelNone,
                                     const cEnergyValue &Value, bool Invert)
{
    if (!Value.valid) {
        Add(new cOsdItem(cString::sprintf("%s\t%s", LabelNone, tr("n/a")), osUnknown, false));
        return;
    }
    double w = Invert ? -Value.value : Value.value;
    Add(new cOsdItem(cString::sprintf("%s\t%s", w >= 0 ? LabelPos : LabelNeg, *FormatPower(std::fabs(w))), osUnknown, false));
}

void cMenuOhPvStatus::BuildDisplay(void)
{
    const cOhPvStatusConfig &c = OhPvStatusConfig;
    Clear();

    if (!haveData_) {
        Add(new cOsdItem(tr("Reading data from openHAB..."), osUnknown, false));
    }
    else if (!data_.reachable) {
        Add(new cOsdItem(tr("--- openHAB not reachable ---"), osUnknown, false));
        Add(new cOsdItem("", osUnknown, false));
        Add(new cOsdItem(cString::sprintf(tr("Host: %s:%d"), c.openhabHost, c.openhabPort), osUnknown, false));
    }
    else {
        if (c.itemSoC[0]) {
            if (data_.soc.valid)
                Add(new cOsdItem(cString::sprintf("%s\t%.0f %%\t%s", tr("Battery SoC:"), data_.soc.value, *FormatSoCBar(data_.soc.value)), osUnknown, false));
            else
                Add(new cOsdItem(cString::sprintf("%s\t%s", tr("Battery SoC:"), tr("n/a")), osUnknown, false));
        }
        if (c.itemPVToday[0])
            AddEnergy(tr("PV Today:"), data_.pvToday);
        AddPower(tr("PV Production:"), data_.pv, c.invertPV);
        AddPower(tr("House Consumption:"), data_.house, c.invertHouse);
        AddSignedPower(tr("Battery Charging:"), tr("Battery Discharging:"), tr("Battery:"), data_.battery, c.invertBattery);
        AddSignedPower(tr("Grid Import:"), tr("Grid Export:"), tr("Grid:"), data_.grid, c.invertGrid);
        for (int i = 0; i < NUMLINES; i++) {
            if (c.itemLine[i][0])
                AddLine(i, data_.line[i]);
        }

        Add(new cOsdItem("", osUnknown, false));
        char timebuf[64];
        struct tm tm;
        localtime_r(&data_.timestamp, &tm);
        strftime(timebuf, sizeof(timebuf), tr("Updated: %H:%M:%S"), &tm);
        Add(new cOsdItem(timebuf, osUnknown, false));
    }
    Display();
}

eOSState cMenuOhPvStatus::ProcessKey(eKeys Key)
{
    bool hadSubMenu = HasSubMenu();
    eOSState state = cOsdMenu::ProcessKey(Key);
    if (HasSubMenu())
        return state;                     // never draw over the setup page

    bool rebuild = hadSubMenu;            // the setup may have changed unit or invert
    if (poller_.GetData(data_)) {
        haveData_ = true;
        rebuild = true;
    }
    if (state == osUnknown) {
        switch (Key) {
            case kRed:
                poller_.Refresh();
                state = osContinue;
                break;
            case kBlue: {
                cMenuOhPvStatusSetup *setup = new cMenuOhPvStatusSetup;
                setup->SetPlugin(cPluginManager::GetPlugin(PLUGIN_NAME_I18N));   // so that Store() saves to setup.conf
                return AddSubMenu(setup);
            }
            default:
                break;
        }
    }
    if (rebuild)
        BuildDisplay();
    return state;
}

// ---- cMenuOhPvStatusSetup ----------------------------------------------------

cMenuOhPvStatusSetup::cMenuOhPvStatusSetup(void)
{
    unitModeNames_[umAuto] = tr("auto");
    unitModeNames_[umW]    = "W";
    unitModeNames_[umKW]   = "kW";

    tempInterval_      = OhPvStatusConfig.updateInterval;
    tempInvertPV_      = OhPvStatusConfig.invertPV;
    tempInvertGrid_    = OhPvStatusConfig.invertGrid;
    tempInvertBattery_ = OhPvStatusConfig.invertBattery;
    tempInvertHouse_   = OhPvStatusConfig.invertHouse;
    tempUnitMode_      = OhPvStatusConfig.unitMode;

    Add(new cMenuEditIntItem (tr("Update interval (s)"), &tempInterval_, 1, 300));
    Add(new cMenuEditStraItem(tr("Unit display"),        &tempUnitMode_, 3, unitModeNames_));
    Add(new cMenuEditBoolItem(tr("Invert PV"),           &tempInvertPV_));
    Add(new cMenuEditBoolItem(tr("Invert Grid"),         &tempInvertGrid_));
    Add(new cMenuEditBoolItem(tr("Invert Battery"),      &tempInvertBattery_));
    Add(new cMenuEditBoolItem(tr("Invert House"),        &tempInvertHouse_));
}

void cMenuOhPvStatusSetup::Store(void)
{
    OhPvStatusConfig.updateInterval = tempInterval_;
    OhPvStatusConfig.unitMode       = tempUnitMode_;
    OhPvStatusConfig.invertPV       = tempInvertPV_;
    OhPvStatusConfig.invertGrid     = tempInvertGrid_;
    OhPvStatusConfig.invertBattery  = tempInvertBattery_;
    OhPvStatusConfig.invertHouse    = tempInvertHouse_;

    SetupStore("Interval",    OhPvStatusConfig.updateInterval);
    SetupStore("UnitMode",    OhPvStatusConfig.unitMode);
    SetupStore("InvertPV",    OhPvStatusConfig.invertPV);
    SetupStore("InvertGrid",  OhPvStatusConfig.invertGrid);
    SetupStore("InvertBat",   OhPvStatusConfig.invertBattery);
    SetupStore("InvertHouse", OhPvStatusConfig.invertHouse);
}
