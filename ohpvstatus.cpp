/*
 * ohpvstatus.cpp: a plugin for the Video Disk Recorder
 *
 * Shows PV production, house consumption, battery and grid power and the
 * battery state of charge from an openHAB installation in the VDR OSD.
 *
 * See the README file for copyright information and how to reach the author.
 */

#include "ohpvstatus.h"
#include "menu.h"
#include <vdr/plugin.h>
#include <curl/curl.h>
#include <getopt.h>
#include <cstdlib>
#include <cstring>

static const char *VERSION        = "0.5.0";
static const char *DESCRIPTION    = trNOOP("openHAB PV & Energy Status");
static const char *MAINMENUENTRY  = trNOOP("openHAB PV Status");

cOhPvStatusConfig OhPvStatusConfig;

class cPluginOhPvStatus : public cPlugin {
public:
    cPluginOhPvStatus(void) {}
    virtual ~cPluginOhPvStatus() {}

    virtual const char *Version(void)       { return VERSION; }
    virtual const char *Description(void)   { return tr(DESCRIPTION); }
    virtual const char *MainMenuEntry(void) { return tr(MAINMENUENTRY); }

    virtual const char *CommandLineHelp(void)
    {
        return
            "  -H HOST,        --host=HOST          openHAB hostname or IP (default: 127.0.0.1)\n"
            "  -P PORT,        --port=PORT          openHAB port (default: 8080)\n"
            "                  --ssl                use HTTPS instead of HTTP\n"
            "                  --ssl-verify=no      do not verify the SSL certificate\n"
            "                  --token=TOKEN        openHAB API token, if openHAB requires one\n"
            "                  --pv=ITEM            item for PV power (default: PVPower)\n"
            "                  --house=ITEM         item for house power (default: HousePower)\n"
            "                  --soc=ITEM           item for battery SoC in % (default: BatterySoC,\n"
            "                                       --soc= without item hides the SoC line)\n"
            "  Grid (choose one):\n"
            "                  --grid=ITEM          single item, positive = import, negative = export\n"
            "                                       (default: GridPower)\n"
            "                  --grid-in=ITEM       grid import (always positive)\n"
            "                  --grid-out=ITEM      grid export (always positive)\n"
            "  Battery (choose one):\n"
            "                  --battery=ITEM       single item, positive = charging, negative = discharging\n"
            "                                       (default: BatteryPower)\n"
            "                  --bat-in=ITEM        battery charging (always positive)\n"
            "                  --bat-out=ITEM       battery discharging (always positive)\n"
            "  Optional lines, shown only if an item is given:\n"
            "                  --pv-today=ITEM      PV energy of today (W/Wh/kWh state)\n"
            "                  --line1=ITEM         free line 1, any item (number or text)\n"
            "                  --label1=TEXT        its label (default: the item name),\n"
            "                                       quote texts with spaces: --label1=\"My text\"\n"
            "                  --decimals1=N        decimals of a numeric state (0-3, default: 1)\n"
            "                  --line2=ITEM, --label2=TEXT, --decimals2=N   free line 2\n";
    }

    virtual bool ProcessArgs(int argc, char *argv[])
    {
        enum {
            OPT_PV = 256, OPT_HOUSE, OPT_SOC,
            OPT_GRID, OPT_GRID_IN, OPT_GRID_OUT,
            OPT_BAT, OPT_BAT_IN, OPT_BAT_OUT,
            OPT_SSL, OPT_SSL_VERIFY, OPT_TOKEN,
            OPT_PV_TODAY,
            OPT_LINE1, OPT_LABEL1, OPT_DECIMALS1,
            OPT_LINE2, OPT_LABEL2, OPT_DECIMALS2
        };
        static struct option long_options[] = {
            { "host",       required_argument, NULL, 'H'            },
            { "port",       required_argument, NULL, 'P'            },
            { "ssl",        no_argument,       NULL, OPT_SSL        },
            { "ssl-verify", required_argument, NULL, OPT_SSL_VERIFY },
            { "token",      required_argument, NULL, OPT_TOKEN      },
            { "pv",         required_argument, NULL, OPT_PV         },
            { "house",      required_argument, NULL, OPT_HOUSE      },
            { "soc",        required_argument, NULL, OPT_SOC        },
            { "grid",       required_argument, NULL, OPT_GRID       },
            { "grid-in",    required_argument, NULL, OPT_GRID_IN    },
            { "grid-out",   required_argument, NULL, OPT_GRID_OUT   },
            { "battery",    required_argument, NULL, OPT_BAT        },
            { "bat-in",     required_argument, NULL, OPT_BAT_IN     },
            { "bat-out",    required_argument, NULL, OPT_BAT_OUT    },
            { "pv-today",   required_argument, NULL, OPT_PV_TODAY   },
            { "line1",      required_argument, NULL, OPT_LINE1      },
            { "label1",     required_argument, NULL, OPT_LABEL1     },
            { "decimals1",  required_argument, NULL, OPT_DECIMALS1  },
            { "line2",      required_argument, NULL, OPT_LINE2      },
            { "label2",     required_argument, NULL, OPT_LABEL2     },
            { "decimals2",  required_argument, NULL, OPT_DECIMALS2  },
            { NULL, 0, NULL, 0 }
        };

        cOhPvStatusConfig &c = OhPvStatusConfig;
        int opt;
        while ((opt = getopt_long(argc, argv, "H:P:", long_options, NULL)) != -1) {
            switch (opt) {
                case 'H':            strn0cpy(c.openhabHost, optarg, sizeof(c.openhabHost)); break;
                case 'P':            c.openhabPort = atoi(optarg); break;
                case OPT_SSL:        c.useSSL    = true; break;
                case OPT_SSL_VERIFY: c.sslVerify = strcasecmp(optarg, "no") != 0; break;
                case OPT_TOKEN:      strn0cpy(c.apiToken,    optarg, sizeof(c.apiToken));    break;
                case OPT_PV:         strn0cpy(c.itemPV,      optarg, sizeof(c.itemPV));      break;
                case OPT_HOUSE:      strn0cpy(c.itemHouse,   optarg, sizeof(c.itemHouse));   break;
                case OPT_SOC:        strn0cpy(c.itemSoC,     optarg, sizeof(c.itemSoC));     break;
                case OPT_GRID:       strn0cpy(c.itemGrid,    optarg, sizeof(c.itemGrid));    break;
                case OPT_GRID_IN:    strn0cpy(c.itemGridIn,  optarg, sizeof(c.itemGridIn));  break;
                case OPT_GRID_OUT:   strn0cpy(c.itemGridOut, optarg, sizeof(c.itemGridOut)); break;
                case OPT_BAT:        strn0cpy(c.itemBattery, optarg, sizeof(c.itemBattery)); break;
                case OPT_BAT_IN:     strn0cpy(c.itemBatIn,   optarg, sizeof(c.itemBatIn));   break;
                case OPT_BAT_OUT:    strn0cpy(c.itemBatOut,  optarg, sizeof(c.itemBatOut));  break;
                case OPT_PV_TODAY:   strn0cpy(c.itemPVToday, optarg, sizeof(c.itemPVToday)); break;
                case OPT_LINE1:      strn0cpy(c.itemLine[0],  optarg, sizeof(c.itemLine[0]));  break;
                case OPT_LABEL1:     strn0cpy(c.labelLine[0], optarg, sizeof(c.labelLine[0])); break;
                case OPT_DECIMALS1:  c.decimalsLine[0] = constrain(atoi(optarg), 0, 3); break;
                case OPT_LINE2:      strn0cpy(c.itemLine[1],  optarg, sizeof(c.itemLine[1]));  break;
                case OPT_LABEL2:     strn0cpy(c.labelLine[1], optarg, sizeof(c.labelLine[1])); break;
                case OPT_DECIMALS2:  c.decimalsLine[1] = constrain(atoi(optarg), 0, 3); break;
                default:             return false;
            }
        }
        if (c.openhabPort <= 0 || c.openhabPort > 65535) {
            esyslog("ohpvstatus: invalid port %d", c.openhabPort);
            return false;
        }
        if ((c.itemGridIn[0] != '\0') != (c.itemGridOut[0] != '\0'))
            esyslog("ohpvstatus: --grid-in and --grid-out must be used together, using --grid");
        if ((c.itemBatIn[0] != '\0') != (c.itemBatOut[0] != '\0'))
            esyslog("ohpvstatus: --bat-in and --bat-out must be used together, using --battery");
        return true;
    }

    virtual bool Initialize(void)
    {
        return curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
    }

    virtual void Stop(void)
    {
        curl_global_cleanup();
    }

    virtual cOsdObject *MainMenuAction(void)
    {
        return new cMenuOhPvStatus;
    }

    virtual cMenuSetupPage *SetupMenu(void)
    {
        return new cMenuOhPvStatusSetup;
    }

    virtual bool SetupParse(const char *Name, const char *Value)
    {
        cOhPvStatusConfig &c = OhPvStatusConfig;
        if      (!strcasecmp(Name, "Interval"))    c.updateInterval = constrain(atoi(Value), 1, 300);
        else if (!strcasecmp(Name, "UnitMode"))    c.unitMode       = constrain(atoi(Value), 0, 2);
        else if (!strcasecmp(Name, "InvertPV"))    c.invertPV       = atoi(Value);
        else if (!strcasecmp(Name, "InvertGrid"))  c.invertGrid     = atoi(Value);
        else if (!strcasecmp(Name, "InvertBat"))   c.invertBattery  = atoi(Value);
        else if (!strcasecmp(Name, "InvertHouse")) c.invertHouse    = atoi(Value);
        else
            return false;
        return true;
    }
};

VDRPLUGINCREATOR(cPluginOhPvStatus);
