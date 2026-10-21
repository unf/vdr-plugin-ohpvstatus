/*
 * openhab.h: reading item states from the openHAB REST API
 *
 * See the README file for copyright information and how to reach the author.
 */

#ifndef __OHPVSTATUS_OPENHAB_H
#define __OHPVSTATUS_OPENHAB_H

#include <time.h>
#include <vdr/thread.h>

struct cEnergyValue {
    double value;
    bool   valid;
    cEnergyValue(void) : value(0), valid(false) {}
};

struct cEnergyData {
    bool         reachable;   // openHAB answered
    cEnergyValue pv;          // W
    cEnergyValue house;       // W
    cEnergyValue grid;        // W, positive = import, negative = export
    cEnergyValue battery;     // W, positive = charging, negative = discharging
    cEnergyValue soc;         // %
    time_t       timestamp;
    cEnergyData(void) : reachable(false), timestamp(0) {}
};

// Converts an item state like "1234", "1.2 kW" or "85 %" into W resp. %,
// independent of the locale. "NULL", "UNDEF" and other text give false.
bool OpenHabParseState(const char *State, double &Value);

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
