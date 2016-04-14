#ifndef caToolsTypes
#define caToolsTypes

#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>

#define CA_DEFAULT_TIMEOUT 1

// warning and error printout
#define VERBOSITY_ERR           2
#define VERBOSITY_WARN          3
#define VERBOSITY_ERR_PERIODIC  4
#define VERBOSITY_WARN_PERIODIC 5
#define VERBOSITY_DEBUG		    10

enum roundType {
    roundType_no_rounding = -1,
    roundType_round,
    roundType_ceil,
    roundType_floor
};

enum tool {
    tool_unknown = 0,
    caget,
    camon,
    caput,
    caputq,
    cagets,
    cado,
    cawait,
    cainfo
};

typedef struct {
   double caTimeout;        //ca timeout
   int32_t  dbrRequestType; //dbr request type
   bool num;                //same as -int
   enum roundType round;  //type of rounding: round(), ceil(), floor()
   int32_t prec;            //precision
   bool hex;                //display as hex
   bool bin;                //display as bin
   bool oct;                //display as oct
   bool plain;              //ignore formatting switches
   char dblFormatType;      //format type for decimal numbers (see -e -f -g option)
   bool str;                //try to interpret values as strings
   bool stat;               //status and severity on
   bool nostat;             //status and severity off
   bool noname;             //hide name
   bool nounit;             //hide units
   char timestamp;        //timestamp type ('r', 'u' or 'c')
   double timeout;        //cawait timeout
   bool date;               //server date
   bool localdate;        //client date
   bool time;               //server time
   bool localtime;        //client time
   char fieldSeparator;     //array field separator for output
   char inputSeparator;     //array field separator for input
   enum tool tool;        //tool type
   int64_t numUpdates;      //number of monitor updates after which to quit
   bool parseArray;         //use inputSeparator to parse array
   int64_t outNelm;         //number of array elements to read
   bool nord;               //display number of array elements
   u_int16_t verbosity;     //verbosity level: see VERBOSITY_* defines
} arguments_T;

enum channelField {
    field_desc = 0,
    field_hhsv,
    field_hsv,
    field_lsv,
    field_llsv,
    field_unsv,
    field_cosv,
    field_zrsv,
    field_onsv,
    field_twsv,
    field_thsv,
    field_frsv,
    field_fvsv,
    field_sxsv,
    field_svsv,
    field_eisv,
    field_nisv,
    field_tesv,
    field_elsv,
    field_tvsv,
    field_ttsv,
    field_ftsv,
    field_ffsv
};

#ifndef INCLcadefh
	#include "db_access.h"
	#include "alarm.h"
	typedef struct oldChannelNotify *chid;
	typedef chid                    chanId; /* for when the structures field name is "chid" */

	typedef struct event_handler_args {
	    void            *usr;   /* user argument supplied with request */
	    chanId          chid;   /* channel id */
	    long            type;   /* the type of the item returned */ 
	    long            count;  /* the element count of the item returned */
	    const void      *dbr;   /* a pointer to the item returned */
	    int             status; /* ECA_XXX status of the requested op from the server */
	} evargs;
#endif

struct field {
    char *name;             // the name of the channel.field
    chid id;                // the id of the channel.field
    long connectionState;   // channel connected/disconnected
    bool created;           // channel creation for the field was successfull
    bool done;              // indicates if callback has finished processing this channel
    struct channel * ch;	// reference to the channel
};

enum operator { //possible conditions for cawait
    operator_gt = 1,
    operator_gte,
    operator_lt,
    operator_lte,
    operator_eq,
    operator_neq,
    operator_in,
    operator_out
};

struct channel {
    struct field    base;       // the name of the channel
    struct field    proc;       // sibling channel for writing to proc field
    struct field    str$;       // sibling channel for reading string as an array of chars
    char           *name;       // the name of the channel
    char           *longStr;	// long string
    struct field    fields[23]; // fields[noFields];    // sibling channels for fields (description, severities, ...)

    long            type;       // dbr type
    int32_t 		precision;
    unsigned long   count;      // element count
    unsigned long   inNelm;     // requested number of elements for writing
    unsigned long   outNelm;    // requested number of elements for reading
    u_int32_t       i;          // process variable id
    u_int32_t       nRequests;  // holds the number of requests to finish
    char          **writeStr;   // value(s) to be written
    enum operator conditionOperator; //cawait operator
    double        conditionOperands[2]; //cawait operands
    u_int32_t       status;          // status
    u_int32_t		severity; 		 // severity

};

extern char **outDate,**outTime, **outSev, **outStat, **outUnits, **outLocalDate, **outLocalTime, **outTimestamp;
extern u_int32_t const LEN_RECORD_FIELD;
extern u_int32_t const LEN_RECORD_NAME;
extern u_int32_t const LEN_TIMESTAMP;
extern epicsTimeStamp *timestampRead;

extern int verbosity;
#define customPrint(level,output,M, ...) if(verbosity >= level) fprintf(output, M,##__VA_ARGS__)
#define warnPrint(M, ...) customPrint(VERBOSITY_WARN, stdout, "Warning: "M, ##__VA_ARGS__)
#define warnPeriodicPrint(M, ...) customPrint(VERBOSITY_WARN_PERIODIC, stdout, "Warning: "M, ##__VA_ARGS__)
#define errPrint(M, ...) customPrint(VERBOSITY_ERR, stderr, "Error: "M, ##__VA_ARGS__)
#define errPeriodicPrint(M, ...) customPrint(VERBOSITY_ERR_PERIODIC, stderr, "Error: "M, ##__VA_ARGS__)
#define debugPrint(M, ...) customPrint(VERBOSITY_DEBUG, stderr, "Debug: "M, ##__VA_ARGS__)



#endif