#include "menu.h"
#include "ohpvstatus.h"
#include <vdr/plugin.h>
#include <cstdio>
#include <cmath>
#include <ctime>
#include <cstring>

#define BARLEN 30

// ---- Leistungsformatierung -------------------------------------------------

cString cMenuOhPvStatus::FormatPower(double watts) const
{
    char buf[32];
    int mode = OhPvStatusConfig.unitMode;
    bool useKW = (mode == 2) || (mode == 0 && std::fabs(watts) >= 1000.0);

    if (useKW)
        snprintf(buf, sizeof(buf), "%.2f kW", watts / 1000.0);
    else
        snprintf(buf, sizeof(buf), "%.0f W", watts);

    return buf;
}

// ---- SoC Balken ------------------------------------------------------------
// % in Spalte 2, Balken in Spalte 3
// '|' wird von vdrsymbols als grafischer Balken dargestellt

cString cMenuOhPvStatus::FormatSoCBar(double soc) const
{
    if (soc < 0)   soc = 0;
    if (soc > 100) soc = 100;

    int filled = (int)((soc / 100.0) * BARLEN + 0.5);

    char bar[BARLEN + 3];
    bar[0] = '[';
    memset(bar + 1,          '|', filled);
    memset(bar + 1 + filled, ' ', BARLEN - filled);
    bar[BARLEN + 1] = ']';
    bar[BARLEN + 2] = '\0';

    return bar;
}

// ---- cMenuOhPvStatus -------------------------------------------------------
// Drei Spalten: Label (18) | Wert (10) | Balken (Rest)

cMenuOhPvStatus::cMenuOhPvStatus()
: cOsdMenu(tr("openHAB PV Status"), 18, 10), lastUpdate_(0)
{
    SetHelp(tr("Refresh"), NULL, NULL, tr("Setup"));
    Fetch();
    BuildDisplay();
}

void cMenuOhPvStatus::Fetch()
{
    data_       = OpenHabFetchAll();
    lastUpdate_ = time(NULL);
}

void cMenuOhPvStatus::BuildDisplay()
{
    Clear();

    if (!data_.valid) {
        Add(new cOsdItem(tr("--- openHAB not reachable ---"), osUnknown, false));
        Add(new cOsdItem("", osUnknown, false));
        char buf[256];
        snprintf(buf, sizeof(buf), tr("Host: %s:%d"),
                 OhPvStatusConfig.openhabHost, OhPvStatusConfig.openhabPort);
        Add(new cOsdItem(buf, osUnknown, false));
    } else {
        // Batterie SoC mit Balken - ganz oben
        if (OhPvStatusConfig.itemSoC[0] != '\0') {
            char pct[16];
            snprintf(pct, sizeof(pct), "%.0f%%", data_.batterySoC);
            Add(new cOsdItem(cString::sprintf("%s\t%s\t%s",
                tr("Battery SoC:"), pct, *FormatSoCBar(data_.batterySoC)),
                osUnknown, false));
        }

        // PV Erzeugung
        double pvW = OhPvStatusConfig.invertPV ? -data_.pvPower : data_.pvPower;
        Add(new cOsdItem(cString::sprintf("%s\t%s",
            tr("PV Production:"), *FormatPower(pvW)), osUnknown, false));

        // Hausverbrauch
        double houseW = OhPvStatusConfig.invertHouse ? -data_.housePower : data_.housePower;
        Add(new cOsdItem(cString::sprintf("%s\t%s",
            tr("House Consumption:"), *FormatPower(houseW)), osUnknown, false));

        // Batterieleistung
        double batW = OhPvStatusConfig.invertBattery
                      ? -data_.batteryPower : data_.batteryPower;
        const char *batLabel = (batW >= 0)
                               ? tr("Battery Charging:")
                               : tr("Battery Discharging:");
        Add(new cOsdItem(cString::sprintf("%s\t%s",
            batLabel, *FormatPower(std::fabs(batW))), osUnknown, false));

        // Netz
        double gridW = OhPvStatusConfig.invertGrid
                       ? -data_.gridPower : data_.gridPower;
        const char *gridLabel = (gridW >= 0)
                                ? tr("Grid Import:")
                                : tr("Grid Export:");
        Add(new cOsdItem(cString::sprintf("%s\t%s",
            gridLabel, *FormatPower(std::fabs(gridW))), osUnknown, false));

        // Zeitstempel
        Add(new cOsdItem("", osUnknown, false));
        char timebuf[64];
        struct tm *tm = localtime(&lastUpdate_);
        strftime(timebuf, sizeof(timebuf), tr("Updated: %H:%M:%S"), tm);
        Add(new cOsdItem(timebuf, osUnknown, false));
    }

    Display();
}

eOSState cMenuOhPvStatus::ProcessKey(eKeys Key)
{
    if (lastUpdate_ > 0 &&
        (time(NULL) - lastUpdate_) >= OhPvStatusConfig.updateInterval) {
        Fetch();
        BuildDisplay();
    }

    eOSState state = cOsdMenu::ProcessKey(Key);

    switch (Key) {
        case kRed:
            Fetch();
            BuildDisplay();
            return osContinue;
        case kBlue:
            return AddSubMenu(static_cast<cOsdMenu*>(new cMenuOhPvStatusSetup()));
        default:
            break;
    }

    return state;
}

// ---- cMenuOhPvStatusSetup --------------------------------------------------

const char * const cMenuOhPvStatusSetup::unitModeNames_[] = {
    "Auto", "W", "kW", NULL
};

cMenuOhPvStatusSetup::cMenuOhPvStatusSetup()
{
    tempInterval_      = OhPvStatusConfig.updateInterval;
    tempInvertPV_      = OhPvStatusConfig.invertPV;
    tempInvertGrid_    = OhPvStatusConfig.invertGrid;
    tempInvertBattery_ = OhPvStatusConfig.invertBattery;
    tempInvertHouse_   = OhPvStatusConfig.invertHouse;
    tempUnitMode_      = OhPvStatusConfig.unitMode;

    Add(new cMenuEditIntItem (tr("Update interval (s)"), &tempInterval_,   1, 300));
    Add(new cMenuEditStraItem(tr("Unit display"),        &tempUnitMode_,   3, unitModeNames_));
    Add(new cMenuEditBoolItem(tr("Invert PV"),           &tempInvertPV_));
    Add(new cMenuEditBoolItem(tr("Invert Grid"),         &tempInvertGrid_));
    Add(new cMenuEditBoolItem(tr("Invert Battery"),      &tempInvertBattery_));
    Add(new cMenuEditBoolItem(tr("Invert House"),        &tempInvertHouse_));
}

void cMenuOhPvStatusSetup::Store()
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
