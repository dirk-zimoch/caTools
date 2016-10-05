#ifndef caToolsTypes
#define caToolsTypes

#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>

#define CA_DEFAULT_TIMEOUT 1
#define CA_PEND_EVENT_TIMEOUT 0.01

/* string lengths */
#define LEN_TIMESTAMP 80              /* Max length of the timestamp string */
#define LEN_UNITS  20+MAX_UNITS_SIZE  /* Max length of the units string */
#define LEN_RECORD_NAME  60           /* Max length of the epics record name */
#define LEN_RECORD_FIELD 4            /* Max length of the epics record field name */
#define LEN_FQN_NAME (LEN_RECORD_NAME + LEN_RECORD_FIELD + 2) /* Max length of the full epics record name */


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
   enum roundType round;    /* type of rounding: round(), ceil(), floor() */
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
   char timestamp;          /* timestamp type ('r', 'u' or 'c') */
   double timeout;          /* cawait timeout */
   bool date;               /* server date */
   bool localdate;          /* client date */
   bool time;               /* server time */
   bool localtime;          /* client timeqmake -project */
   char fieldSeparator;     /* array field separator for output */
   char inputSeparator;     /* array field separator for input */
   enum tool tool;          /* tool type */
   int64_t numUpdates;      /* number of monitor updates after which to quit */
   bool parseArray;         /* use inputSeparator to parse array */
   int64_t outNelm;         /* number of array elements to read */
   bool nord;               /* display number of array elements */
   u_int16_t verbosity;     /* verbosity level: see VERBOSITY_* defines */
   double period;           /* periodic execution */
   char* separator;         /* periodic execution separator string */
   char* tfmt;              /* time stamp format */
   char* ltfmt;             /* local time stamp */
} arguments_T;

enum channelField {
    field_desc = 0,
    field_rtyp,
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
     * cadef.h can be included only once in the project, since it defines some globals
     * if cadef.h is not already included for current compile unit some epics types and
     * definitions are defined here
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
    char *name;             /* the name of the ca channel */
    chid id;                /* the id of the ca channel */
    long connectionState;   /* channel connected/disconnected  */
    bool created;           /* channel creation for the field was successfull */
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
    operator_out,
    operator_streq,
    operator_strneq
};

/* enum describing the "state machine" state of each channel during the execution of catools applocation */
enum state{
    base_waiting,
    base_created,
    sibl_waiting,
    sibl_created,
    put_waiting,
    put_done,
    read_waiting,
    reading,
};

struct channel {
    struct field    base;       /* the name of the channel */
    struct field    proc;       /* sibling channel for writing to proc field */
    struct field    lstr;       /* sibling channel for reading string as an array of chars  */
    epicsTimeStamp  lstrTimestamp;  /* timestamp when the request for long string channel was sent */
    bool            lstrDone;   /* long string channel connection was already handled */
    struct field    fields[24]; /* sibling channels for fields (description, severities, long strings...)  */
    short           type;       /* dbr type */
    size_t          count;      /* maximum array element count in the server. Zero is returned if the channel is disconnected */
    size_t          inNelm;     /* requested number of elements for writing */
    size_t          outNelm;    /* requested number of elements for reading */
    int             i;          /* process variable id */
    int             nRequests;  /* holds the number of requests to finish */
    char            *inpStr;    /* pointer to the input in argv for this chanel */
    enum operator   conditionOperator;    /* cawait operator */
    double          conditionOperands[2]; /* cawait operands */
    int             status;     /* status */
    int      	    severity; 	/* severity */
    int             prec;       /* precision */
    char*           units;      /* units */
    epicsTimeStamp  timestamp;  /* time stamp */
    epicsTimeStamp  lastUpdate; /* last update for timestamp diff */
    enum state      state;      /* state of the channel within catools application */
};

/* global strings see caToolsGlobals.c */
extern char **g_outTimestamp;

extern epicsTimeStamp g_programStartTime, g_commonLastUpdate; /* see caToolsGlobals.c */

extern int g_verbosity;



#endif
