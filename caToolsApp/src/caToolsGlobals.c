/* This file contains declaration of global variables.
This is included from caToolsMain.c only. */

#include "caToolsTypes.h"
#include "caToolsUtils.h"

#define CA_PRIORITY CA_PRIORITY_MIN

/* intialize arguments */
arguments_T arguments = {
        .caTimeout = CA_DEFAULT_TIMEOUT,
        .dbrRequestType = -1, /* decide type based on requested data */
        .num = false,
        .round = roundType_no_rounding,
        .prec = 0,
        .hex = false,
        .bin = false,
        .oct = false,
        .plain = false,
        .dblFormatType = '\0',
        .str = false,
        .stat = false,
        .nostat = false,
        .noname = false,
        .nounit = false,
        .timestamp = '\0',
        .timeout = -1,  /* no timeout for cawait */
        .date = false,
        .localdate = false,
        .time = false,
        .localtime = false,
        .fieldSeparator = 0 ,
        .inputSeparator = ' ',
        .tool = tool_unknown,
        .numUpdates = -1, /* never exit */
        .parseArray = false,
        .outNelm = 0,  /* return number of elements defined by IOC */
        .nord = false,
        .verbosity = VERBOSITY_WARN, /*  show warnings */
        .period = -1, /* do not run periodically */
};


const char * cainfo_fields[] = {
    ".DESC",
    ".RTYP",
    ".HHSV",
    ".HSV",
    ".LSV",
    ".LLSV",
    ".UNSV",
    ".COSV",
    ".ZRSV",
    ".ONSV",
    ".TWSV",
    ".THSV",
    ".FRSV",
    ".FVSV",
    ".SXSV",
    ".SVSV",
    ".EISV",
    ".NISV",
    ".TESV",
    ".ELSV",
    ".TVSV",
    ".TTSV",
    ".FTSV",
    ".FFSV"
};

#define noFields (sizeof (cainfo_fields) / sizeof (const char *))

/* timestamps needed by -timestamp */
epicsTimeStamp g_programStartTime;  /* timestamp indicating program start */
epicsTimeStamp g_commonLastUpdate;
epicsTimeStamp g_timeoutTime;       /* when to stop monitoring (-timeout) */

bool g_runMonitor;                /* indicates when to stop monitoring according to -timeout, -n or cawait condition is true */
uint32_t g_numMonitorUpdates;    /* counts updates needed by -n */
int g_verbosity = VERBOSITY_WARN;  /* global verbosity */
