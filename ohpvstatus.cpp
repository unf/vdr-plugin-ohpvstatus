#include "ohpvstatus.h"
#include "menu.h"
#include <vdr/plugin.h>
#include <getopt.h>
#include <cstring>
#include <cstdlib>

static const char *VERSION        = "0.3.0";
static const char *DESCRIPTION    = "openHAB PV & Energy Status";
static const char *MAINMENUENTRY  = "openHAB PV Status";

cOhPvStatusConfig OhPvStatusConfig;

class cPluginOhPvStatus : public cPlugin {
public:
    cPluginOhPvStatus() {}
    virtual ~cPluginOhPvStatus() {}

    virtual const char *Version()       { return VERSION; }
    virtual const char *Description()   { return tr(DESCRIPTION); }
    virtual const char *MainMenuEntry() { return tr(MAINMENUENTRY); }

    virtual const char *CommandLineHelp()
    {
        return
            "  -H HOST,        --host=HOST          openHAB hostname or IP (default: 127.0.0.1)\n"
            "  -P PORT,        --port=PORT          openHAB port (default: 8080)\n"
            "                  --ssl                Use HTTPS instead of HTTP\n"
            "                  --ssl-verify=no      Disable SSL certificate verification\n"
            "                  --pv=ITEM            Item for PV power (default: PVPower)\n"
            "                  --house=ITEM         Item for house power (default: HousePower)\n"
            "                  --soc=ITEM           Item for battery SoC (default: BatterySoC)\n"
            "  Grid (choose one):\n"
            "                  --grid=ITEM          Single item, positive=import negative=export\n"
            "                  --grid-in=ITEM       Grid import (always positive)\n"
            "                  --grid-out=ITEM      Grid export (always positive)\n"
            "  Battery (choose one):\n"
            "                  --battery=ITEM       Single item, positive=charge negative=discharge\n"
            "                  --bat-in=ITEM        Battery charging (always positive)\n"
            "                  --bat-out=ITEM       Battery discharging (always positive)\n";
    }

    virtual bool ProcessArgs(int argc, char *argv[])
    {
        enum {
            OPT_PV = 1, OPT_HOUSE, OPT_SOC,
            OPT_GRID, OPT_GRID_IN, OPT_GRID_OUT,
            OPT_BAT, OPT_BAT_IN, OPT_BAT_OUT,
            OPT_SSL, OPT_SSL_VERIFY
        };

        static struct option long_options[] = {
            { "host",       required_argument, NULL, 'H'           },
            { "port",       required_argument, NULL, 'P'           },
            { "ssl",        no_argument,       NULL, OPT_SSL       },
            { "ssl-verify", required_argument, NULL, OPT_SSL_VERIFY},
            { "pv",         required_argument, NULL, OPT_PV        },
            { "house",      required_argument, NULL, OPT_HOUSE     },
            { "soc",        required_argument, NULL, OPT_SOC       },
            { "grid",       required_argument, NULL, OPT_GRID      },
            { "grid-in",    required_argument, NULL, OPT_GRID_IN   },
            { "grid-out",   required_argument, NULL, OPT_GRID_OUT  },
            { "battery",    required_argument, NULL, OPT_BAT       },
            { "bat-in",     required_argument, NULL, OPT_BAT_IN    },
            { "bat-out",    required_argument, NULL, OPT_BAT_OUT   },
            { NULL, 0, NULL, 0 }
        };

        int c, option_index = 0;
        while ((c = getopt_long(argc, argv, "H:P:", long_options, &option_index)) != -1) {
            switch (c) {
                case 'H': strncpy(OhPvStatusConfig.openhabHost,  optarg, sizeof(OhPvStatusConfig.openhabHost));  break;
                case 'P': OhPvStatusConfig.openhabPort = atoi(optarg); break;
                case OPT_SSL:        OhPvStatusConfig.useSSL    = true;  break;
                case OPT_SSL_VERIFY: OhPvStatusConfig.sslVerify = (strcasecmp(optarg, "no") != 0); break;
                case OPT_PV:         strncpy(OhPvStatusConfig.itemPV,      optarg, sizeof(OhPvStatusConfig.itemPV));      break;
                case OPT_HOUSE:      strncpy(OhPvStatusConfig.itemHouse,   optarg, sizeof(OhPvStatusConfig.itemHouse));   break;
                case OPT_SOC:        strncpy(OhPvStatusConfig.itemSoC,     optarg, sizeof(OhPvStatusConfig.itemSoC));     break;
                case OPT_GRID:       strncpy(OhPvStatusConfig.itemGrid,    optarg, sizeof(OhPvStatusConfig.itemGrid));    break;
                case OPT_GRID_IN:    strncpy(OhPvStatusConfig.itemGridIn,  optarg, sizeof(OhPvStatusConfig.itemGridIn));  break;
                case OPT_GRID_OUT:   strncpy(OhPvStatusConfig.itemGridOut, optarg, sizeof(OhPvStatusConfig.itemGridOut)); break;
                case OPT_BAT:        strncpy(OhPvStatusConfig.itemBattery, optarg, sizeof(OhPvStatusConfig.itemBattery)); break;
                case OPT_BAT_IN:     strncpy(OhPvStatusConfig.itemBatIn,   optarg, sizeof(OhPvStatusConfig.itemBatIn));   break;
                case OPT_BAT_OUT:    strncpy(OhPvStatusConfig.itemBatOut,  optarg, sizeof(OhPvStatusConfig.itemBatOut));  break;
                default: break;
            }
        }
        return true;
    }

    virtual bool Start() { return true; }
    virtual void Stop()  {}

    virtual cOsdObject *MainMenuAction()
    {
        return new cMenuOhPvStatus();
    }

    virtual cMenuSetupPage *SetupMenu()
    {
        return new cMenuOhPvStatusSetup();
    }

    virtual bool SetupParse(const char *Name, const char *Value)
    {
        if      (!strcmp(Name, "Interval"))    OhPvStatusConfig.updateInterval = atoi(Value);
        else if (!strcmp(Name, "UnitMode"))    OhPvStatusConfig.unitMode       = atoi(Value);
        else if (!strcmp(Name, "InvertPV"))    OhPvStatusConfig.invertPV       = atoi(Value);
        else if (!strcmp(Name, "InvertGrid"))  OhPvStatusConfig.invertGrid     = atoi(Value);
        else if (!strcmp(Name, "InvertBat"))   OhPvStatusConfig.invertBattery  = atoi(Value);
        else if (!strcmp(Name, "InvertHouse")) OhPvStatusConfig.invertHouse    = atoi(Value);
        else return false;
        return true;
    }
};

VDRPLUGINCREATOR(cPluginOhPvStatus);
