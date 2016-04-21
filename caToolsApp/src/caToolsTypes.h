#ifndef caToolsTypes
#define caToolsTypes

#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>

#define CA_DEFAULT_TIMEOUT 1


/* Document what theese defines do */
#define LEN_TIMESTAMP 50
#define LEN_SEVSTAT 30
#define LEN_UNITS  20+MAX_UNITS_SIZE
#define LEN_RECORD_NAME  60
#define LEN_RECORD_FIELD 4
#define LEN_FQN_NAME LEN_RECORD_NAME + LEN_RECORD_FIELD + 2


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
   double caTimeout;        /* ca timeout */
   int32_t  dbrRequestType; /* dbr request type */
   bool num;                /* same as -int */
   enum roundType round;  /* type of rounding: round(), ceil(), floor() */
   int32_t prec;            /* precision */
   bool hex;                /* display as hex */
   bool bin;                /* display as bin */
   bool oct;                /* display as oct */
   bool plain;              /* ignore formatting switches */
   char dblFormatType;      /* format type for decimal numbers (see -e -f -g option) */
   bool str;                /* try to interpret values as strings */
   bool stat;               /* status and severity on */
   bool nostat;             /* status and severity off */
   bool noname;             /* hide name */
   bool nounit;             /* hide units */
   char timestamp;        /* timestamp type ('r', 'u' or 'c') */
   double timeout;        /* cawait timeout */
   bool date;               /* server date */
   bool localdate;        /* client date */
   bool time;               /* server time */
   bool localtime;        /* client timeqmake -project */
   char fieldSeparator;     /* array field separator for output */
   char inputSeparator;     /* array field separator for input */
   enum tool tool;        /* tool type */
   int64_t numUpdates;      /* number of monitor updates after which to quit */
   bool parseArray;         /* use inputSeparator to parse array */
   int64_t outNelm;         /* number of array elements to read */
   bool nord;               /* display number of array elements */
   u_int16_t verbosity;     /* verbosity level: see VERBOSITY_* defines */
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
    /*
     * cadef.h can be included only ones, since it defines some globals
     * if cadef.h is not already included some epics types and definitions are defined here
     */
	#include "db_access.h"
	#include "alarm.h"

    /*
     *  External OP codes for CA operations
     */
    #define CA_OP_GET             0
    #define CA_OP_PUT             1
    #define CA_OP_CREATE_CHANNEL  2
    #define CA_OP_ADD_EVENT       3
    #define CA_OP_CLEAR_EVENT     4
    #define CA_OP_OTHER           5

    /*
     * used with connection_handler_args
     */
    #define CA_OP_CONN_UP       6
    #define CA_OP_CONN_DOWN     7

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
    char *name;             /* the name of the channel.field */
    chid id;                /* the id of the channel.field */
    long connectionState;   /* channel connected/disconnected  */
    bool created;           /* channel creation for the field was successfull */
    bool done;              /* indicates if callback has finished processing this channel */
    struct channel * ch;	/* reference to the channel */
};

enum operator { /* possible conditions for cawait */
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
    struct field    base;       /* the name of the channel */
    struct field    proc;       /* sibling channel for writing to proc field */
    struct field    lstr;       /* sibling channel for reading string as an array of chars  */
    char           *name;       /* the name of the channel */
    struct field    fields[23]; /* sibling channels for fields (description, severities, ...) Used only by caInfo */

    short           type;       /* dbr type This should probably be a short? Double check before though...  */
    int      		precision;
    size_t          count;      /* element count */
    size_t          inNelm;     /* requested number of elements for writing */
    size_t          outNelm;    /* requested number of elements for reading */
    int             i;          /* process variable id */
    int             nRequests;  /* holds the number of requests to finish */
    char          **writeStr;   /* vqmake -projectalue(s) to be written */
    enum operator   conditionOperator;    /* cawait operator */
    double          conditionOperands[2]; /* cawait operands */
    int             status;          /*  status */
    int      		severity; 		 /*  severity */
    int             prec;            /* precision */

};

extern char **g_outDate,**g_outTime, **g_outSev, **g_outStat, **g_outUnits, **g_outLocalDate, **g_outLocalTime, **g_outTimestamp;



extern epicsTimeStamp *g_timestampRead;

extern int g_verbosity;



#endif
