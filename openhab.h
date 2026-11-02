/*
 * openhab.h: reading item states from the openHAB REST API
 *
 * See the README file for copyright information and how to reach the author.
 */

#ifndef __OHPVSTATUS_OPENHAB_H
#define __OHPVSTATUS_OPENHAB_H

#include <time.h>
#include <vdr/thread.h>

#define NUMLINES 2                // free lines

struct cEnergyValue {
    double value;             // W, Wh or as sent (k, M and m prefixes of W and Wh converted)
    double raw;               // the number as openHAB sent it
    bool   valid;             // the state is a number
    bool   present;           // openHAB sent a state, also text like "ON"
    char   unit[16];          // the unit as openHAB sent it, e.g. "kWh" or "%"
    char   text[64];          // the state as openHAB sent it
    cEnergyValue(void) : value(0), raw(0), valid(false), present(false) { unit[0] = text[0] = '\0'; }
};

struct cEnergyData {
    bool         reachable;   // openHAB answered
    cEnergyValue pv;          // W
    cEnergyValue pvToday;     // Wh
    cEnergyValue house;       // W
    cEnergyValue grid;        // W, positive = import, negative = export
    cEnergyValue battery;     // W, positive = charging, negative = discharging
    cEnergyValue soc;         // %
    cEnergyValue line[NUMLINES];
    time_t       timestamp;
    cEnergyData(void) : reachable(false), timestamp(0) {}
};

// Converts an item state like "1234", "1.2 kW", "12.3 kWh" or "85 %" into
// W, Wh resp. the number as it is, independent of the locale. Raw and Unit
// (optional) get the number and the unit as openHAB sent them.
// "NULL", "UNDEF" and other text give false.
bool OpenHabParseState(const char *State, double &Value, double *Raw = NULL, char *Unit = NULL, size_t UnitSize = 0);

// Fetches the items in the background while the status menu is open.
class cOpenHabPoller : public cThread {
private:
    cMutex      mutex_;
    cCondVar    condVar_;
    cEnergyData data_;
    bool        newData_;
    bool        refresh_;
    cEnergyData FetchAll(void);
protected:
    virtual void Action(void);
public:
    cOpenHabPoller(void);
    virtual ~cOpenHabPoller();
    void Refresh(void);                  // fetch again now
    bool GetData(cEnergyData &Data);     // true if there is new data since the last call
    bool Stopping(void) { return !Running(); }
};

#endif //__OHPVSTATUS_OPENHAB_H
