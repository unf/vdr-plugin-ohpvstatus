/*
 * openhab.cpp: reading item states from the openHAB REST API
 *
 * See the README file for copyright information and how to reach the author.
 */

#include "openhab.h"
#include "ohpvstatus.h"
#include <curl/curl.h>
#include <cctype>
#include <locale>
#include <sstream>
#include <string>

#define MAXSTATELEN    4096   // an item state is short, anything longer is no state
#define CONNECTTIMEOUT 3L     // seconds
#define TOTALTIMEOUT   5L     // seconds per request

// ---- parsing -----------------------------------------------------------------

// factor of the prefix of a W or Wh unit, 0 for any other unit
static double PowerEnergyFactor(const std::string &Unit)
{
    std::string prefix;
    if (Unit.size() >= 2 && Unit.compare(Unit.size() - 2, 2, "Wh") == 0)
        prefix = Unit.substr(0, Unit.size() - 2);
    else if (Unit.size() >= 1 && Unit[Unit.size() - 1] == 'W')
        prefix = Unit.substr(0, Unit.size() - 1);
    else
        return 0;
    if (prefix.empty()) return 1.0;
    if (prefix == "k")  return 1000.0;
    if (prefix == "M")  return 1000000.0;
    if (prefix == "m")  return 0.001;
    return 0;                             // e.g. "ct/kWh"
}

bool OpenHabParseState(const char *State, double &Value, double *Raw, char *Unit, size_t UnitSize)
{
    if (!State)
        return false;
    std::istringstream is(State);
    is.imbue(std::locale::classic());     // always a decimal point, whatever VDR's locale is
    double v;
    if (!(is >> v))
        return false;                     // NULL, UNDEF, ON, ...
    std::string unit;
    std::getline(is >> std::ws, unit);    // the rest, units may contain spaces
    while (!unit.empty() && isspace((unsigned char)unit[unit.size() - 1]))
        unit.erase(unit.size() - 1);
    if (Raw)
        *Raw = v;
    if (Unit && UnitSize)
        strn0cpy(Unit, unit.c_str(), UnitSize);
    double f = PowerEnergyFactor(unit);
    Value = f ? v * f : v;
    return true;
}

// ---- HTTP --------------------------------------------------------------------

static size_t WriteCallback(void *Contents, size_t Size, size_t NMemb, void *UserP)
{
    std::string *s = static_cast<std::string *>(UserP);
    size_t n = Size * NMemb;
    if (s->size() + n > MAXSTATELEN)
        return 0;
    s->append(static_cast<char *>(Contents), n);
    return n;
}

// aborts a running request as soon as the poller is stopped
static int ProgressCallback(void *ClientP, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
{
    return static_cast<cOpenHabPoller *>(ClientP)->Stopping() ? 1 : 0;
}

enum eFetchResult { frOk, frInvalid, frUnreachable };

static eFetchResult FetchItem(CURL *Curl, const std::string &BaseUrl, const char *Item, cEnergyValue &Value)
{
    Value = cEnergyValue();
    std::string url = BaseUrl + Item + "/state";
    std::string response;
    curl_easy_setopt(Curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(Curl, CURLOPT_WRITEDATA, &response);
    CURLcode res = curl_easy_perform(Curl);
    if (res == CURLE_WRITE_ERROR)
        return frInvalid;                 // answer too long, but openHAB is there
    if (res != CURLE_OK) {
        dsyslog("ohpvstatus: %s: %s", url.c_str(), curl_easy_strerror(res));
        return frUnreachable;
    }
    long code = 0;
    curl_easy_getinfo(Curl, CURLINFO_RESPONSE_CODE, &code);
    if (code != 200)
        return frInvalid;                 // 404 = no such item, 401 = token needed
    while (!response.empty() && isspace((unsigned char)response[response.size() - 1]))
        response.erase(response.size() - 1);
    if (response.empty() || response == "NULL" || response == "UNDEF")
        return frInvalid;
    Value.present = true;
    strn0cpy(Value.text, response.c_str(), sizeof(Value.text));
    double v;
    if (!OpenHabParseState(response.c_str(), v, &Value.raw, Value.unit, sizeof(Value.unit)))
        return frInvalid;                 // text state, only shown in a free line
    Value.value = v;
    Value.valid = true;
    return frOk;
}

// ---- cOpenHabPoller ----------------------------------------------------------

cOpenHabPoller::cOpenHabPoller(void)
: cThread("ohpvstatus poller"), newData_(false), refresh_(false)
{
}

cOpenHabPoller::~cOpenHabPoller()
{
    Cancel(-1);                           // Running() is false from now on
    {
        cMutexLock lock(&mutex_);
        refresh_ = true;                  // do not start a new wait
        condVar_.Broadcast();
    }
    Cancel(5);                            // a running request is aborted within a second
}

cEnergyData cOpenHabPoller::FetchAll(void)
{
    const cOhPvStatusConfig &c = OhPvStatusConfig;
    cEnergyData d;
    d.timestamp = time(NULL);

    CURL *curl = curl_easy_init();
    if (!curl)
        return d;
    std::string base = *cString::sprintf("%s://%s:%d/rest/items/",
                                         c.useSSL ? "https" : "http", c.openhabHost, c.openhabPort);
    struct curl_slist *headers = NULL;
    if (c.apiToken[0]) {
        headers = curl_slist_append(headers, *cString::sprintf("Authorization: Bearer %s", c.apiToken));
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,    WriteCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ProgressCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA,     this);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS,       0L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL,         1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT,   CONNECTTIMEOUT);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,          TOTALTIMEOUT);
    if (c.useSSL && !c.sslVerify) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }

    bool gridSplit = c.hasGridSplit();
    bool batSplit  = c.hasBatSplit();
    cEnergyValue gridIn, gridOut, batIn, batOut;
    struct { const char *item; cEnergyValue *value; } requests[] = {
        { c.itemPV,                                   &d.pv },
        { c.itemPVToday,                              &d.pvToday },
        { c.itemHouse,                                &d.house },
        { c.itemSoC,                                  &d.soc },
        { gridSplit ? c.itemGridIn  : c.itemGrid,     gridSplit ? &gridIn : &d.grid },
        { gridSplit ? c.itemGridOut : NULL,           &gridOut },
        { batSplit  ? c.itemBatIn   : c.itemBattery,  batSplit ? &batIn : &d.battery },
        { batSplit  ? c.itemBatOut  : NULL,           &batOut },
        { c.itemLine[0],                              &d.line[0] },
        { c.itemLine[1],                              &d.line[1] },
    };

    // one connection for all items, stop at the first connection error
    d.reachable = true;
    for (size_t i = 0; i < sizeof(requests) / sizeof(requests[0]) && Running(); i++) {
        if (!requests[i].item || !requests[i].item[0])
            continue;
        if (FetchItem(curl, base, requests[i].item, *requests[i].value) == frUnreachable) {
            d.reachable = false;
            break;
        }
    }
    if (gridSplit && gridIn.valid && gridOut.valid) {
        d.grid.value = gridIn.value - gridOut.value;   // import positive, export negative
        d.grid.valid = true;
    }
    if (batSplit && batIn.valid && batOut.valid) {
        d.battery.value = batIn.value - batOut.value;  // charging positive, discharging negative
        d.battery.valid = true;
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return d;
}

void cOpenHabPoller::Action(void)
{
    bool wasReachable = true;
    while (Running()) {
        cEnergyData d = FetchAll();
        if (!Running())
            break;
        if (d.reachable != wasReachable) {
            if (d.reachable)
                isyslog("ohpvstatus: openHAB %s:%d is reachable again", OhPvStatusConfig.openhabHost, OhPvStatusConfig.openhabPort);
            else
                esyslog("ohpvstatus: openHAB %s:%d is not reachable", OhPvStatusConfig.openhabHost, OhPvStatusConfig.openhabPort);
            wasReachable = d.reachable;
        }
        cMutexLock lock(&mutex_);
        data_    = d;
        newData_ = true;
        if (!refresh_)
            condVar_.TimedWait(mutex_, max(OhPvStatusConfig.updateInterval, 1) * 1000);
        refresh_ = false;
    }
}

void cOpenHabPoller::Refresh(void)
{
    cMutexLock lock(&mutex_);
    refresh_ = true;
    condVar_.Broadcast();
}

bool cOpenHabPoller::GetData(cEnergyData &Data)
{
    cMutexLock lock(&mutex_);
    if (!newData_)
        return false;
    Data     = data_;
    newData_ = false;
    return true;
}
