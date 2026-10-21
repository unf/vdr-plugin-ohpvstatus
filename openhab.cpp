#include "openhab.h"
#include "ohpvstatus.h"
#include <curl/curl.h>
#include <string>
#include <cstdlib>

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *s)
{
    s->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

bool OpenHabFetchItem(const std::string &host, int port, bool useSSL, bool sslVerify,
                      const std::string &itemName, double &value)
{
    if (itemName.empty())
        return false;

    CURL *curl = curl_easy_init();
    if (!curl)
        return false;

    std::string scheme = useSSL ? "https" : "http";
    std::string url = scheme + "://" + host + ":" + std::to_string(port) +
                      "/rest/items/" + itemName + "/state";
    std::string response;

    curl_easy_setopt(curl, CURLOPT_URL,           url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,     &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,       5L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT,3L);

    if (useSSL && !sslVerify) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
        return false;

    try {
        value = std::stod(response);
        return true;
    } catch (...) {
        return false;
    }
}

// Convenience wrapper using global config
static bool Fetch(const std::string &itemName, double &value)
{
    return OpenHabFetchItem(
        OhPvStatusConfig.openhabHost,
        OhPvStatusConfig.openhabPort,
        OhPvStatusConfig.useSSL,
        OhPvStatusConfig.sslVerify,
        itemName, value);
}

cEnergyData OpenHabFetchAll()
{
    cEnergyData data;
    bool ok = true;

    ok &= Fetch(OhPvStatusConfig.itemPV,    data.pvPower);
    ok &= Fetch(OhPvStatusConfig.itemHouse, data.housePower);

    // Grid: split or single
    if (OhPvStatusConfig.hasGridSplit()) {
        double in = 0, out = 0;
        bool okIn  = Fetch(OhPvStatusConfig.itemGridIn,  in);
        bool okOut = Fetch(OhPvStatusConfig.itemGridOut, out);
        ok &= okIn && okOut;
        // import positive, export negative
        data.gridPower = in - out;
    } else {
        ok &= Fetch(OhPvStatusConfig.itemGrid, data.gridPower);
    }

    // Battery: split or single
    if (OhPvStatusConfig.hasBatSplit()) {
        double in = 0, out = 0;
        bool okIn  = Fetch(OhPvStatusConfig.itemBatIn,  in);
        bool okOut = Fetch(OhPvStatusConfig.itemBatOut, out);
        ok &= okIn && okOut;
        // charging positive, discharging negative
        data.batteryPower = in - out;
    } else {
        ok &= Fetch(OhPvStatusConfig.itemBattery, data.batteryPower);
    }

    // SoC optional
    if (OhPvStatusConfig.itemSoC[0] != '\0')
        Fetch(OhPvStatusConfig.itemSoC, data.batterySoC);

    data.valid = ok;
    return data;
}
