#include "caToolsTypes.h"

#define CA_PRIORITY CA_PRIORITY_MIN
#define CA_DEFAULT_TIMEOUT 1

// warning and error printout
#define VERBOSITY_ERR           2
#define VERBOSITY_WARN          3
#define VERBOSITY_ERR_PERIODIC  4
#define VERBOSITY_WARN_PERIODIC 5

#define customPrint(level,output,M, ...) if(arguments.verbosity >= level) fprintf(output, M,##__VA_ARGS__)
#define warnPrint(M, ...) customPrint(VERBOSITY_WARN, stdout, "Warning: "M, ##__VA_ARGS__)
#define warnPeriodicPrint(M, ...) customPrint(VERBOSITY_WARN_PERIODIC, stdout, "Warning: "M, ##__VA_ARGS__)
#define errPrint(M, ...) customPrint(VERBOSITY_ERR, stderr, "Error: "M, ##__VA_ARGS__)
#define errPeriodicPrint(M, ...) customPrint(VERBOSITY_ERR_PERIODIC, stderr, "Error: "M, ##__VA_ARGS__)


//intialize arguments
arguments_T arguments = {
        .caTimeout = CA_DEFAULT_TIMEOUT,
        .dbrRequestType = -1, //decide type based on requested data
        .num = false,
        .round = roundType_no_rounding,
        .prec = -1, //use precision from the record
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
        .timeout = -1,  //no timeout for cawait
        .date = false,
        .localdate = false,
        .time = false,
        .localtime = false,
        .fieldSeparator = ' ' ,
        .inputSeparator = ' ',
        .tool = tool_unknown,
        .numUpdates = -1, //never exit
        .parseArray = false,
        .outNelm = -1,  //number of elements equal to field count
        .nord = false,
        .verbosity = VERBOSITY_ERR // show errors
};


const char * fields[] = {
    ".DESC",
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

#define noFields (sizeof (fields) / sizeof (const char *))

//output strings
// TODO: most of theese should go in struct channel
static u_int32_t const LEN_TIMESTAMP = 50;
static u_int32_t const LEN_RECORD_NAME = 60;
static u_int32_t const LEN_SEVSTAT = 30;
static u_int32_t const LEN_UNITS = 20+MAX_UNITS_SIZE;
static u_int32_t const LEN_RECORD_FIELD = 4;
char *errorTimestamp;   // timestamp used in caCustomExceptionHandler
char **outDate,**outTime, **outSev, **outStat, **outUnits, **outLocalDate, **outLocalTime;
char **outTimestamp; //relative timestamps for camon


//timestamps needed by -timestamp
epicsTimeStamp *timestampRead;      //timestamps of received data (per channel)
epicsTimeStamp programStartTime;  //timestamp indicating program start
epicsTimeStamp *lastUpdate;       //timestamp indicating last update per channel
bool *firstUpdate;              //indicates that lastUpdate has not been initialized
epicsTimeStamp timeoutTime;       //when to stop monitoring (-timeout)

bool runMonitor;                //indicates when to stop monitoring according to -timeout, -n or cawait condition is true
u_int32_t numMonitorUpdates;    //counts updates needed by -n