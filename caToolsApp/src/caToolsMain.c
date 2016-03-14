/* caToolsMain.c */
/* Author:  Rok Vuga Date:    FEB 2016 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>
#include <getopt.h>
#include <stdbool.h>
#include <assert.h>

#include <cantProceed.h>
#include "cadef.h"
#include "alarmString.h"
#include "alarm.h"


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

struct arguments {
   double caTimeout;        //ca timeout
   int32_t  dbrRequestType; //dbr request type
   bool num;                //same as -int
   enum roundType round;	//type of rounding: round(), ceil(), floor()
   int32_t prec;            //precision
   bool hex;                //display as hex
   bool bin;                //display as bin
   bool oct;                //display as oct
   bool plain;              //ignore formatting switches
   char dblFormatType;     	//format type for decimal numbers (see -e -f -g option)
   bool str;                //try to interpret values as strings
   bool stat;               //status and severity on
   bool nostat;             //status and severity off
   bool noname;             //hide name
   bool nounit;             //hide units
   char timestamp;      	//timestamp type ('r', 'u' or 'c')
   double timeout;      	//cawait timeout
   bool date;               //server date
   bool localdate;      	//client date
   bool time;               //server time
   bool localtime;      	//client time
   char fieldSeparator;     //array field separator for output
   char inputSeparator;     //array field separator for input
   enum tool tool;      	//tool type
   int64_t numUpdates;      //number of monitor updates after which to quit
   bool parseArray;         //use inputSeparator to parse array
   int64_t outNelm;         //number of array elements to read
   bool nord;               //display number of array elements
   u_int16_t verbosity;     //verbosity level: see VERBOSITY_* defines
};

//intialize arguments
struct arguments arguments = {
        .caTimeout = CA_DEFAULT_TIMEOUT,
        .dbrRequestType = -1,	//decide type based on requested data
        .num = false,
        .round = roundType_no_rounding,
        .prec = -1,	//use precision from the record
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
        .timeout = -1,	//no timeout for cawait
        .date = false,
        .localdate = false,
        .time = false,
        .localtime = false,
        .fieldSeparator = ' ' ,
        .inputSeparator = ' ',
        .tool = tool_unknown,
        .numUpdates = -1,	//never exit
        .parseArray = false,
        .outNelm = -1,	//number of elements equal to field count
        .nord = false,
        .verbosity = VERBOSITY_ERR // show errors
};

enum operator {	//possible conditions for cawait
    operator_gt = 1,
    operator_gte,
    operator_lt,
    operator_lte,
    operator_eq,
    operator_neq,
    operator_in,
    operator_out
};


struct field {
    char *name;             // the name of the channel.field
    chid id;                // the id of the channel.field
    long connectionState;   // channel connected/disconnected
    bool created;           // channel creation for the field was successfull
    bool done;              // indicates if callback has finished processing this channel
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

struct channel {
    struct field    base;       // the name of the channel
    struct field    proc;       // sibling channel for writing to proc field
    char           *name;       // the name of the channel
    struct field    fields[noFields];    // sibling channels for fields (description, severities, ...)

    long            type;       // dbr type
    unsigned long   count;      // element count
    unsigned long   inNelm;     // requested number of elements for writing
    unsigned long   outNelm;    // requested number of elements for reading
    u_int32_t       i;          // process variable id
    char      	  **writeStr;   // value(s) to be written
    enum operator	conditionOperator; //cawait operator
    double       	conditionOperands[2]; //cawait operands
};




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
epicsTimeStamp programStartTime;	//timestamp indicating program start
epicsTimeStamp *lastUpdate;      	//timestamp indicating last update per channel
bool *firstUpdate;            	//indicates that lastUpdate has not been initialized
epicsTimeStamp timeoutTime;      	//when to stop monitoring (-timeout)

bool runMonitor;                //indicates when to stop monitoring according to -timeout, -n or cawait condition is true
u_int32_t numMonitorUpdates;    //counts updates needed by -n


/**
 * @brief usage prints the usage of the tools
 * @param stream is the output stream for printing
 * @param tool is the selected tool
 * @param programName is the command line name of the program
 */
void usage(FILE *stream, enum tool tool, char *programName){

    //usage:
    if (tool == caget || tool == cagets || tool == camon || tool == cado) {
        fprintf(stream, "\nUsage: %s [flags] <pv> [<pv> ...]\n", programName);
    }
    else if (tool == cawait) {
        fprintf(stream, "\nUsage: %s [flags] <pv> <condition> [<pv> <condition> ...]\n", programName);
    }
    else if (tool == caput || tool == caputq  ) {
        fprintf(stream, "\nUsage: %s [flags] <pv> <value> [<pv> <value> ...]\n", programName);
    }
    else if (tool == cainfo  ) {
        fprintf(stream, "\nUsage: %s [flags] <pv> [<pv> ...]\n", programName);
    }
    else { //tool unknown
        fprintf(stream, "\nUnknown tool. Try %s -tool with any of the following arguments: caget, caput, cagets, "\
                "caputq, cado, camon, cawait, cainfo.\n", programName);
        return;
    }

    //descriptions
    fputs("\n",stream);

    if (tool == caget) {
        fputs("Reads PV values(s).\n", stream);
    }
    if (tool == cagets) {
        fputs("First processes the PV(s), then reads the value(s).\n", stream);
    }
    if (tool == caput || tool == caputq) {
        if (tool == caput) {
            fputs("Writes value(s) to the PV(s), waits until processing finishes and returns updated value(s).\n", stream);
        }
        else { //caputq
            fputs("Writes value(s) to PV(s), but does not wait for the processing to finish. Does not have any output (except if an error occurs).\n", stream);
        }
        fputs("Array handling:\n", stream);
        fputs(  "- When -a option is set, input separator (-inSep argument) is used to parse elements in an array.\n"\
                "- When input separator (-inSep argument) is explicitly defined, -a option is automatically used."
                " See following examples which produce the same result,"\
                " namely write 1, 2 and 3 into record pvA and 4, 5, 6 into pvB:\n", stream);
        fputs("  caput -a pvA '1 2 3' pvB '4 5 6'\n", stream);
        fputs("  caput -inSep ; pvA '1;2;3' pvB '4;5;6'\n", stream);
    }
    if (tool == cado) {
        fputs("Writes 1 to PV(s), but does not wait for the processing to finish. Does not have any output (except if an error occurs).\n", stream);
    }
    if (tool == camon) {
        fputs("Monitors the PV(s).\n", stream);
    }
    if (tool == cawait) {
         fputs("Monitors the PV(s), but only displays the values when they match the provided conditions. When at least one of the conditions is true, the program exits." \
               "The conditions are specified as a string containing the operator together with the values.\n", stream);
        fputs("Following operators are supported:  >, <, <=, >=, ==, !=, !, ==A...B(in interval), !=A...B or !A...B (out of interval). For example, "\
                "cawait pv '==1...5' exits after pv value is inside the interval [1,5].\n", stream);
    }
    if (tool == cainfo) {
        fputs("Displays detailed information about the provided records.\n", stream);
    }

    //flags common for all tools
    fputs("\n", stream);
    fputs("Accepted flags:\n", stream);
    fputs("\n", stream);
    fputs("  -h                   Help: Print this message\n", stream);
    fputs("  -v                   Verbosity. Options:\n", stream);
    fprintf(stream,"                       Print error messages: %d (default)\n", VERBOSITY_ERR);
    fprintf(stream,"                       Also print warning messages: %d\n", VERBOSITY_WARN);
    fprintf(stream,"                       Also print error messages that occur periodically: %d\n", VERBOSITY_ERR_PERIODIC);
    fprintf(stream,"                       Also print warning messages that occur periodically: %d\n\n", VERBOSITY_WARN_PERIODIC);

    //flags common for most of the tools
    if (tool != cado){
        fputs("Channel Access options\n", stream);
        if (tool != cainfo) {
            fputs("  -dbrtype <type>      Type of DBR request to use for communicating with the server.\n", stream);
            fputs("                       Use string (DBR_ prefix may be omitted, or number of one of the following types:\n", stream);
            fputs("           DBR_STRING     0  DBR_STS_FLOAT    9  DBR_TIME_LONG   19  DBR_CTRL_SHORT    29\n"
                  "           DBR_INT        1  DBR_STS_ENUM    10  DBR_TIME_DOUBLE 20  DBR_CTRL_INT      29\n"
                  "           DBR_SHORT      1  DBR_STS_CHAR    11  DBR_GR_STRING   21  DBR_CTRL_FLOAT    30\n"
                  "           DBR_FLOAT      2  DBR_STS_LONG    12  DBR_GR_SHORT    22  DBR_CTRL_ENUM     31\n"
                  "           DBR_ENUM       3  DBR_STS_DOUBLE  13  DBR_GR_INT      22  DBR_CTRL_CHAR     32\n"
                  "           DBR_CHAR       4  DBR_TIME_STRING 14  DBR_GR_FLOAT    23  DBR_CTRL_LONG     33\n"
                  "           DBR_LONG       5  DBR_TIME_INT    15  DBR_GR_ENUM     24  DBR_CTRL_DOUBLE   34\n"
                  "           DBR_DOUBLE     6  DBR_TIME_SHORT  15  DBR_GR_CHAR     25  DBR_STSACK_STRING 37\n"
                  "           DBR_STS_STRING 7  DBR_TIME_FLOAT  16  DBR_GR_LONG     26  DBR_CLASS_NAME    38\n"
                  "           DBR_STS_SHORT  8  DBR_TIME_ENUM   17  DBR_GR_DOUBLE   27\n"
                  "           DBR_STS_INT    8  DBR_TIME_CHAR   18  DBR_CTRL_STRING 28\n", stream);

        }
        fprintf(stream, "  -w <time>            Wait time in seconds, specifies CA timeout. (default: %d s)\n", CA_DEFAULT_TIMEOUT);
    }

    //flags associated with writing
    if (tool == caput || tool == caputq) {
        fputs("Formating input : Array format options\n", stream);
        fputs("  -a                   Input separator (-inSep argument) is used to parse elements in an array.\n", stream);
        fputs("  -inSep <separator>   Separator used between array elements in <value>.\n", stream);
        fputs("                       If not specified, space is used.\n", stream);
        fputs("                       If specified, '-a' option is automatically used. \n", stream);
    }

    // flags associated with monitoring
    if (tool == camon) {
        fputs("Monitoring options\n", stream);
        fputs("  -n <number>          Exit the program after <number> updates.\n", stream);
        fputs("  -timestamp <option>  Display relative timestamps. Options:\n", stream);
        fputs("                            r: time elapsed since start of the program,\n", stream);
        fputs("                            u: time elapsed since last update of any PV,\n", stream);
        fputs("                            c: time elapsed since last update separately for each PV.\n", stream);
    }
    if (tool == cawait) {
        fputs("Waiting options\n", stream);
        fputs("  -timeout <number>    Exit the program after <number> seconds without an update.\n", stream);
    }

    //Flags associated with returned values. Used by both reading and writing tools.
    if (tool == caget || tool == cagets || tool == camon
            || tool == cawait ||  tool == caput) {
        fputs("Formating output : General options\n", stream);
        fputs("  -noname              Hide PV name.\n", stream);
        fputs("  -nostat              Never display alarm status and severity.\n", stream);
        fputs("  -stat                Always display alarm status and severity.\n", stream);
        fputs("  -plain               Ignore all formatting switches (displays only PV value) except date/time related.\n", stream);

        fputs("Formating output : Time options\n", stream);
        fputs("  -d -date             Display server date.\n", stream);
        fputs("  -localdate           Display client date.\n", stream);
        fputs("  -localtime           Display client time.\n", stream);
        fputs("  -t, -time            Display server time.\n", stream);

        fputs("Formating output : Integer format options\n", stream);
        fputs("  -bin                 Display integer values in binary format.\n", stream);
        fputs("  -hex                 Display integer values in hexadecimal format.\n", stream);
        fputs("  -oct                 Display integer values in octal format.\n", stream);
        fputs("  -s                   Interpret value(s) as char (number to ascii).\n", stream);

        fputs("Formating output : Floating point format options\n", stream);
        fputs("  -e <number>          Format doubles using scientific notation with precision <number>. Overrides -prec option.\n", stream);
        fputs("  -f <number>          Format doubles using floating point with precision <number>. Overrides -prec option.\n", stream);
        fputs("  -g <number>          Format doubles using shortest representation with precision <number>. Overrides -prec option.\n", stream);
        fputs("  -prec <number>       Override PREC field with <number>. (default: PREC field).\n", stream);
        fputs("  -round <option>      Round floating point value(s). Options:\n", stream);
        fputs("                                 round: round to nearest (default).\n", stream);
        fputs("                                 ceil: round up,\n", stream);
        fputs("                                 floor: round down,\n", stream);

        fputs("Formating output : Enum/char format options\n", stream);
        fputs("  -int, -num           Display enum/char values as numbers.\n", stream);

        fputs("Formating output : Array format options\n", stream);
        fputs("  -nord                Display number of array elements before their values.\n", stream);
        fputs("  -outNelm <number>    Number of array elements to read.\n", stream);
        fputs("  -outSep <number>     Separator between array elements.\n", stream);
    }
}


/**
 * @brief clearStr cleares a string
 * @param str the string to clear
 */
static inline void clearStr(char *str) {
    str[0] = '\0';
}

/**
 * @brief isStrEmpty checks if provided string is empty
 * @param str the string to check
 * @return true if empty, false otherwise
 */
static bool isStrEmpty(char *str) {
    return str[0] == '\0';
}

/**
 * @brief removePrefix removes prefix from data and returns true if successfull
 * @param data is the data to remove the prefix from
 * @param prefix is the prefix to remove from data
 * @return true if prefix was found and removed, false otherwise
 */
bool removePrefix (char **data, char const *prefix) {
    size_t pos = 0;
    while (prefix[pos] != '\0') {
        if ((*data)[pos] != prefix[pos]) {
            return false;
        }
        pos++;
    }
    *data += pos;
    return true;
}

//parses input string and extracts operators and numerals,
//then saves them to the channel structure.
//Supported operators: >,<,<=,>=,==,!=,!, ==A...B(in interval), !=A...B(out of interval), !A...B (out of interval).
bool cawaitParseCondition(struct channel *channel, char *str)
{
    enum operator operator;
    double arg1;
    double arg2 = 0;


    if (removePrefix(&str, "!")) {
        operator = operator_neq;
        removePrefix(&str, "=");
    }
    else if (removePrefix(&str, ">=")) {
        operator = operator_gte;
    }
    else if (removePrefix(&str, "<=")) {
        operator = operator_lte;
    }
    else if (removePrefix(&str, ">")) {
        operator = operator_gt;
    }
    else if (removePrefix(&str, "<")) {
        operator = operator_lt;
    }
    else if (removePrefix(&str, "==")) {
        operator = operator_eq;
    }
    else {
        operator = operator_eq;
    }


    char *token = strtok(str, "...");
    char *endptr;

    arg1 = strtod(str, &endptr);
    if (endptr == token || *endptr != '\0') {
        errPrint("Invalid operand in condition.\n");
        return false;
    }

    token = strtok(NULL, "...");
    if (operator != operator_eq && operator != operator_neq && token) {
        errPrint("Invalid syntax for interval condition.\n");
        return false;
    }
    else if(token) {
        arg2 = strtod(token, &endptr);
        if (endptr == str || *endptr) {
            errPrint("Invalid second operand for interval condition.\n");
            return false;
        }
    }

    // TODO: if arg1 > arg2 when using ... syntax

    channel->conditionOperator = operator;
    channel->conditionOperands[0] = arg1;
    channel->conditionOperands[1] = arg2;

    return true;
}


int cawaitEvaluateCondition(struct channel channel, evargs args){
//evaluates output of channel i against the corresponding condition.
//returns 1 if matching, 0 otherwise, and -1 if error.
//Before evaluation, channel output is converted to double. If this is
//not successful, the function returns -1. If the channel in question
//is an array, the condition is evaluated against the first element.

    //get value
    void *nativeValue = dbr_value_ptr(args.dbr, args.type);

    //convert the value to double
    double dblValue;
    int32_t baseType = args.type % (LAST_TYPE+1);
    switch (baseType){
    case DBR_DOUBLE:
        dblValue = (double) *(dbr_double_t*)nativeValue;
        break;
    case DBR_FLOAT:
        dblValue = (double) *(dbr_float_t*)nativeValue;
        break;
    case DBR_INT:
        dblValue = (double) *(dbr_int_t*)nativeValue;
        break;
    case DBR_LONG:
        dblValue = (double) *(dbr_long_t*)nativeValue;
        break;
    case DBR_CHAR:
    case DBR_STRING:
        if (sscanf(nativeValue, "%lf", &dblValue) <= 0){        // TODO: is this always null-terminated?
            errPeriodicPrint("Record %s value %s cannot be converted to double.\n", channel.base.name, (char*)nativeValue);
            return -1;
        }
        break;
    case DBR_ENUM:
        dblValue = (double) *(dbr_enum_t*)nativeValue;
        break;
    default:
        errPrint("Unrecognized DBR type.\n");
        return -1;
        break;
    }

    //evaluate and exit
    switch (channel.conditionOperator){
    case operator_gt:
        return dblValue > channel.conditionOperands[0];
    case operator_gte:
        return dblValue >= channel.conditionOperands[0];
    case operator_lt:
        return dblValue < channel.conditionOperands[0];
    case operator_lte:
        return dblValue <= channel.conditionOperands[0];
    case operator_eq:
        return dblValue == channel.conditionOperands[0];
    case operator_neq:
        return dblValue != channel.conditionOperands[0];
    case operator_in:
        return (dblValue >= channel.conditionOperands[0]) && (dblValue <= channel.conditionOperands[1]);
    case operator_out:
        return !((dblValue >= channel.conditionOperands[0]) && (dblValue <= channel.conditionOperands[1]));
    }

    return -1;
}


#define printBits(x) \
    for (int32_t i = sizeof(x) * 8 - 1; i >= 0; i--) { \
        fputc('0' + ((x >> i) & 1), stdout); \
    }

int printValue(evargs args, int32_t precision){
//Parses the data fetched by ca_get callback according to the data type and formatting arguments.
//The result is printed to stdout.

    int32_t baseType;
    void *value;
    double valueDbl = 0;

    value = dbr_value_ptr(args.dbr, args.type);
    baseType = args.type % (LAST_TYPE+1);   // convert appropriate TIME, GR, CTRL,... type to basic one

    //loop over the whole array

    long j; //element counter

    for (j=0; j<args.count; ++j){

        if (j){
            printf ("%c", arguments.fieldSeparator); //insert element separator
        }

        switch (baseType) {
        case DBR_STRING:
            printf("\"%.*s\"", MAX_STRING_SIZE, ((dbr_string_t*) value)[j]);    // TODO: is this always null-terminated?
            break;
        case DBR_FLOAT:
        case DBR_DOUBLE:{
            if (baseType == DBR_FLOAT) valueDbl = ((dbr_float_t*)value)[j];
            else valueDbl = ((dbr_double_t*)value)[j];

            //round if desired or will be writen as hex oct, bin or string
            if (arguments.round == roundType_round || arguments.hex || arguments.oct || arguments.bin || arguments.str) {
                valueDbl = round(valueDbl);
            }
            else if(arguments.round == roundType_ceil) {
                    valueDbl = ceil(valueDbl);
            }
            else if (arguments.round == roundType_floor) {
                valueDbl = floor(valueDbl);
            }

            if (!(arguments.hex || arguments.oct || arguments.bin)) { // if hex, oct or bin, do not brake but use DBR_LONG code to print out the value
                char format = 'f'; // default write as float
                if (arguments.dblFormatType != '\0') {
                    //override records default prec
                    format = arguments.dblFormatType;
                }
                if (arguments.prec >= 0) {
                    //use precision and format as specified by -efg or -prec arguments
                    precision = arguments.prec;
                }

                if (precision != -1 && !arguments.plain) { // when DBR_XXX_DOUBLE, DBR_XXX_FLOAT has precision info. Also, print raw when -plain is used.
                    char formatStr[30];
                    sprintf(formatStr, "%%-.%"PRId32"%c", precision, format);
                    printf(formatStr, valueDbl);
                }
                else { // case where no precision info is avaliable
                    printf("%-f", valueDbl);
                }
                break;
            }
        }
        case DBR_LONG:{
            int32_t valueInt32;
            if (baseType == DBR_FLOAT || baseType == DBR_DOUBLE) valueInt32 = (int32_t)valueDbl;
            else valueInt32 = ((dbr_long_t*)value)[j];

            //display dec, hex, bin, oct if desired
            if (arguments.hex){
                printf("0x%" PRIx32, valueInt32);
            }
            else if (arguments.oct){
                printf("0o%" PRIo32, valueInt32);
            }
            else if (arguments.bin){
                printf("0b");
                printBits(valueInt32);
            }
            else if (arguments.str){
                printf("%c", (u_int8_t)valueInt32);
            }
            else{
                printf("%" PRId32, valueInt32);
            }
            break;
        }
        case DBR_INT:{
            int16_t valueInt16 = ((dbr_int_t*)value)[j];

            //display dec, hex, bin, oct, char if desired
            if (arguments.hex){
                printf("0x%" PRIx16, (u_int16_t)valueInt16);
            }
            else if (arguments.oct){
                printf("0o%" PRIo16, (u_int16_t)valueInt16);
            }
            else if (arguments.bin){
                printf("0b");
                printBits((u_int16_t)valueInt16);
            }
            else if (arguments.str){
                printf("%c", (u_int8_t)valueInt16);
            }
            else{
                printf("%" PRId16, valueInt16);
            }
            break;
        }
        case DBR_ENUM: {
            dbr_enum_t v = ((dbr_enum_t *)value)[j];
            if (v >= MAX_ENUM_STATES){
                printf("Illegal enum index: %d", v);
                break;
            }

            if (!arguments.num && !arguments.bin && !arguments.hex && !arguments.oct) { // if not requested as a number check if we can get string


                if (dbr_type_is_GR(args.type)) {
                    if (v >= ((struct dbr_gr_enum *)args.dbr)->no_str) {
                        printf("Enum index value %d greater than the number of strings", v);
                    }
                    else{
                        printf("\"%.*s\"", MAX_ENUM_STRING_SIZE, ((struct dbr_gr_enum *)args.dbr)->strs[v]);
                    }
                    break;
                }
                else if (dbr_type_is_CTRL(args.type)) {
                    if (v >= ((struct dbr_ctrl_enum *)args.dbr)->no_str) {
                        printf("Enum index value %d greater than the number of strings", v);
                    }
                    else{
                        printf("\"%.*s\"", MAX_ENUM_STRING_SIZE, ((struct dbr_ctrl_enum *)args.dbr)->strs[v]);
                    }
                    break;
                }
            }
            // else write value as number.
            if (arguments.hex){
                printf("0x%" PRIx16, v);
            }
            else if (arguments.oct){
                printf("0o%" PRIo16, v);
            }
            else if (arguments.bin){
                printf("0b");
                printBits(v);
            }
            else{
                printf("%" PRId16, v);
            }
            break;
        }
        case DBR_CHAR:{
            if (arguments.num) {	//check if requested as a a number
                printf("%" PRIu8, ((u_int8_t*) value)[j]);
            }
            else if (arguments.hex){
                printf("0x%" PRIx8, ((u_int8_t*) value)[j]);
            }
            else if (arguments.oct){
                printf("0o%" PRIo8, ((u_int8_t*) value)[j]);
            }
            else if (arguments.bin){
                printf("0b");
                printBits(((u_int8_t*) value)[j]);
            }
            else{
                fputc(((char*) value)[j], stdout);
            }
            break;
        }

        default:
            errPeriodicPrint("Unrecognized DBR type.\n");
            break;
        }

    }

    return 0;
}


int printOutput(int i, evargs args, int32_t precision){
// prints global output strings corresponding to i-th channel.

    //if both local and server times are requested, clarify which is which
    bool doubleTime = (arguments.localdate || arguments.localtime) && (arguments.date || arguments.time);
    if (doubleTime){
        fputs("server time: ",stdout);
    }

    //server date
    if (!isStrEmpty(outDate[i]))	printf("%s ",outDate[i]);
    //server time
    if (!isStrEmpty(outTime[i]))	printf("%s ",outTime[i]);

    if (doubleTime){
        fputs("local time: ",stdout);
    }

    //local date
    if (!isStrEmpty(outLocalDate[i]))	printf("%s ",outLocalDate[i]);
    //local time
    if (!isStrEmpty(outLocalTime[i]))	printf("%s ",outLocalTime[i]);


    //timestamp if monitor and if requested
    if ((arguments.tool == camon || arguments.tool == cainfo) && arguments.timestamp)	printf("%s ", outTimestamp[i]);

    //channel name
    if (!arguments.noname)	printf("%s ",((struct channel *)args.usr)->base.name);

    if (arguments.nord)	printf("%lu ", args.count); // show nord if requested

    //value(s)
    printValue(args, precision);


    fputc(' ',stdout);

    //egu
    if (!isStrEmpty(outUnits[i]) && !arguments.nounit) printf("%s ",outUnits[i]);

    //severity
    if (!isStrEmpty(outSev[i])) printf("(%s",outSev[i]);

    //status
    if (!isStrEmpty(outStat[i])) printf(" %s)",outStat[i]);
    else if (!isStrEmpty(outSev[i])) putc(')', stdout);

    putc('\n', stdout);
    return 0;
}


// the same as epicsTimeLessThan(), except it only checks if pLeft < pRight
bool timeLessThan(const epicsTimeStamp *pLeft, const epicsTimeStamp *pRight) {
    return (pLeft->secPastEpoch < pRight->secPastEpoch || (pLeft->secPastEpoch == pRight->secPastEpoch && pLeft->nsec < pRight->nsec));
}


bool epicsTimeDiffFull(epicsTimeStamp *diff, const epicsTimeStamp *pLeft, const epicsTimeStamp *pRight) {
// Calculates difference between two epicsTimeStamps: like epicsTimeDiffInSeconds but returning the answer
//in form of a timestamp. The absolute value of the difference pLeft - pRight is saved to the timestamp diff, and the
//returned value indicates if the said difference is negative.
    bool negative = timeLessThan(pLeft, pRight);
    if (negative) { //switch left and right
        const epicsTimeStamp *temp = pLeft;
        pLeft = pRight;
        pRight = temp;
    }

    if (pLeft->nsec >= pRight->nsec) {
        diff->secPastEpoch = pLeft->secPastEpoch - pRight->secPastEpoch;
        diff->nsec = pLeft->nsec - pRight->nsec;
    } else {
        diff->secPastEpoch = pLeft->secPastEpoch - pRight->secPastEpoch - 1;
        diff->nsec = pLeft->nsec + 1000000000ul - pRight->nsec;
    }
    return negative;
}


void validateTimestamp(epicsTimeStamp *timestamp, const char* name) {
//checks a timestamp for illegal values.
    if (timestamp->nsec >= 1000000000ul) {
        errPeriodicPrint("Warning: invalid number of nanoseconds in timestamp: %s - assuming 0.\n", name);
        timestamp->nsec = 0;
    }
}

int getTimeStamp(u_int32_t i) {
//calculates timestamp for monitor tool, formats it and saves it into the global string.

    epicsTimeStamp elapsed;
    bool negative=false;
    u_int32_t commonI = (arguments.timestamp == 'c') ? i : 0;
    bool showEmpty = false;

    if (arguments.timestamp == 'r') {
        //calculate elapsed time since startTime
        negative = epicsTimeDiffFull(&elapsed, &timestampRead[i], &programStartTime);
    }
    else if(arguments.timestamp == 'c' || arguments.timestamp == 'u') {
        //calculate elapsed time since last update; if using 'c' keep
        //timestamps separate for each channel, otherwise use lastUpdate[0]
        //for all channels (commonI).

        // firstUpodate var is set at the end of caReadCallback, just before printing results.
        if (firstUpdate[i]) {
            negative = epicsTimeDiffFull(&elapsed, &timestampRead[i], &lastUpdate[commonI]);
        }
        else {
            firstUpdate[i] = true;
            showEmpty = true;
        }

        lastUpdate[commonI] = timestampRead[i]; // reset
    }

    //convert to h,m,s,ns
    struct tm tm;
    unsigned long nsec;
    int status = epicsTimeToGMTM(&tm, &nsec, &elapsed);
    assert(status == epicsTimeOK);

    if (showEmpty) {
        //this is the first update for this channel
        sprintf(outTimestamp[i],"%19c",' ');
    }
    else {	//save to outTs string
        char cSign = negative ? '-' : ' ';
        sprintf(outTimestamp[i],"%c%02d:%02d:%02d.%09lu", cSign,tm.tm_hour, tm.tm_min, tm.tm_sec, nsec);
    }

    return 0;
}


//macros for reading requested data
#define severity_status_get(T) \
status = ((struct T *)args.dbr)->status; \
severity = ((struct T *)args.dbr)->severity;
#define timestamp_get(T) \
    timestampRead[ch->i] = ((struct T *)args.dbr)->stamp;\
    validateTimestamp(&timestampRead[ch->i], ch->base.name);
#define units_get_cb(T) clearStr(outUnits[ch->i]); sprintf(outUnits[ch->i], "%s", ((struct T *)args.dbr)->units);
#define precision_get(T) precision = (((struct T *)args.dbr)->precision);

static void caReadCallback (evargs args){
//reads and parses data fetched by calls. First, the global strings holding the output are cleared. Then, depending
//on the type of the returned data, the available information is extracted. The extracted info is then saved to the
//global strings and printed.

    //if we are in monitor and just waiting for the program to exit, don't proceed.
    bool monitor = (arguments.tool == camon || arguments.tool == cawait);
    if (monitor && runMonitor == false) return;

    //check if data was returned
    if (args.status != ECA_NORMAL){
        errPeriodicPrint("Error in read callback. %s.\n", ca_message(args.status));
        return;
    }
    struct channel *ch = (struct channel *)args.usr;

    int32_t precision=-1;
    u_int32_t status=0, severity=0;

    //clear global output strings; the purpose of this callback is to overwrite them
    //the exception are units, which we may be getting from elsewhere; we only clear them if we can write them
    clearStr(outDate[ch->i]);
    clearStr(outTime[ch->i]);
    clearStr(outSev[ch->i]);
    clearStr(outStat[ch->i]);
    clearStr(outLocalDate[ch->i]);
    clearStr(outLocalTime[ch->i]);

    //read requested data
    // rev RV: some doubles and floats has no precision to take. Precision set here and hanled in printValues in none-standard way
    precision = -1; // for all double and float types without this info
    switch (args.type) {
        case DBR_GR_STRING:
        case DBR_CTRL_STRING:
        case DBR_STS_STRING:
            severity_status_get(dbr_sts_string);
            break;

        case DBR_STS_INT: //and SHORT
            severity_status_get(dbr_sts_short);
            break;

        case DBR_STS_FLOAT:
            severity_status_get(dbr_sts_float);
            break;

        case DBR_STS_ENUM:
            severity_status_get(dbr_sts_enum);
            break;

        case DBR_STS_CHAR:
            severity_status_get(dbr_sts_char);
            break;

        case DBR_STS_LONG:
            severity_status_get(dbr_sts_long);
            break;

        case DBR_STS_DOUBLE:
            severity_status_get(dbr_sts_double);
            break;

        case DBR_TIME_STRING:
            severity_status_get(dbr_time_string);
            timestamp_get(dbr_time_string);
            break;

        case DBR_TIME_INT:  //and SHORT
            severity_status_get(dbr_time_short);
            timestamp_get(dbr_time_short);
            break;

        case DBR_TIME_FLOAT:
            severity_status_get(dbr_time_float);
            timestamp_get(dbr_time_float);
            break;

        case DBR_TIME_ENUM:
            severity_status_get(dbr_time_enum);
            timestamp_get(dbr_time_enum);
            break;

        case DBR_TIME_CHAR:
            severity_status_get(dbr_time_char);
            timestamp_get(dbr_time_char);
            break;

        case DBR_TIME_LONG:
            severity_status_get(dbr_time_long);
            timestamp_get(dbr_time_long);
            break;

        case DBR_TIME_DOUBLE:
            severity_status_get(dbr_time_double);
            timestamp_get(dbr_time_double);
            break;

        case DBR_GR_INT:    // and SHORT
            severity_status_get(dbr_gr_short);
            units_get_cb(dbr_gr_short);
            break;

        case DBR_GR_FLOAT:
            severity_status_get(dbr_gr_float);
            units_get_cb(dbr_gr_float);
            precision_get(dbr_gr_float);
            break;

        case DBR_GR_ENUM:
            severity_status_get(dbr_gr_enum);
            // does not have units
            break;

        case DBR_GR_CHAR:
            severity_status_get(dbr_gr_char);
            units_get_cb(dbr_gr_char);
            break;

        case DBR_GR_LONG:
            severity_status_get(dbr_gr_long);
            units_get_cb(dbr_gr_long);
            break;

        case DBR_GR_DOUBLE:
            severity_status_get(dbr_gr_double);
            units_get_cb(dbr_gr_double);
            precision_get(dbr_gr_double);
            break;

        case DBR_CTRL_SHORT:  // and DBR_CTRL_INT
            severity_status_get(dbr_ctrl_short);
            units_get_cb(dbr_ctrl_short);
            break;

        case DBR_CTRL_FLOAT:
            severity_status_get(dbr_ctrl_float);
            units_get_cb(dbr_ctrl_float);
            precision_get(dbr_ctrl_float);
            break;

        case DBR_CTRL_ENUM:
            severity_status_get(dbr_ctrl_enum);
            // does not have units
            break;

        case DBR_CTRL_CHAR:
            severity_status_get(dbr_ctrl_char);
            units_get_cb(dbr_ctrl_char);
            break;

        case DBR_CTRL_LONG:
            severity_status_get(dbr_ctrl_long);
            units_get_cb(dbr_ctrl_long);
            break;

        case DBR_CTRL_DOUBLE:
            severity_status_get(dbr_ctrl_double);
            units_get_cb(dbr_ctrl_double);
            precision_get(dbr_ctrl_double);
            break;
        case DBR_STRING://dont print the warning if any of these
        case DBR_SHORT:
        case DBR_FLOAT:
        case DBR_ENUM:
        case DBR_CHAR:
        case DBR_LONG:
        case DBR_DOUBLE: break;
        default :
            errPeriodicPrint("Can not print %s DBR type (DBR numeric type code: %ld). \n", dbr_type_to_text(args.type), args.type);
            return;
    }


    //do formating

    //check alarm limits
    if (arguments.stat || (!arguments.nostat && (status != 0 || severity != 0))) { //  display status/severity
        if (status <= lastEpicsAlarmCond) sprintf(outStat[ch->i],"STAT:%s", epicsAlarmConditionStrings[status]); // strcpy(outStat[ch->i], epicsAlarmConditionStrings[status]);
        else sprintf(outStat[ch->i],"UNKNOWN: %u",status);

        if (severity <= lastEpicsAlarmSev) sprintf(outSev[ch->i],"SEVR:%s", epicsAlarmSeverityStrings[severity]);//strcpy(outSev[ch->i], epicsAlarmSeverityStrings[severity]);
        else sprintf(outSev[ch->i],"UNKNOWN: %u",status);
    }

    if (args.type >= DBR_TIME_STRING && args.type <= DBR_TIME_DOUBLE){//otherwise we don't have it
        //we assume that manually specifying dbr_time implies -time or -date.
        if (arguments.date || arguments.dbrRequestType != -1) epicsTimeToStrftime(outDate[ch->i], LEN_TIMESTAMP, "%Y-%m-%d", &timestampRead[ch->i]);
        if (arguments.time || arguments.dbrRequestType != -1) epicsTimeToStrftime(outTime[ch->i], LEN_TIMESTAMP, "%H:%M:%S.%06f", &timestampRead[ch->i]);
    }


    //show local date or time?
    if (arguments.localdate || arguments.localtime){
        epicsTimeStamp localTime;
        epicsTimeGetCurrent(&localTime);
        //validateTimestamp(&localTime, "localTime");

        if (arguments.localdate) epicsTimeToStrftime(outLocalDate[ch->i], LEN_TIMESTAMP, "%Y-%m-%d", &localTime);
        if (arguments.localtime) epicsTimeToStrftime(outLocalTime[ch->i], LEN_TIMESTAMP, "%H:%M:%S.%06f", &localTime);
    }

    //if monitor, there is extra stuff to do before printing data out.
    bool shouldPrint = true;
    if (arguments.tool == cawait || arguments.tool == camon) {
        if (arguments.timestamp) getTimeStamp(ch->i);	//calculate relative timestamps.

        if (arguments.numUpdates != -1) {
            numMonitorUpdates++;	//increase counter of updates

            if (numMonitorUpdates > arguments.numUpdates) {
                runMonitor = false;
            }
        }

        if (arguments.tool == cawait) {
            //check stop condition
            if (arguments.timeout!=-1) {
                epicsTimeStamp timeoutNow;
                epicsTimeGetCurrent(&timeoutNow);

                if (epicsTimeGreaterThanEqual(&timeoutNow, &timeoutTime)) {
                    //we are done waiting
                    printf("Condition not met in %f seconds - exiting.\n",arguments.timeout);
                    runMonitor = false;
                    shouldPrint = false;
                }
            }

            //check display condition
            if (cawaitEvaluateCondition(*ch, args) == 1) {
                runMonitor = false;
            }else{
                shouldPrint = false;
            }
        }
    }


    //finish
    if(shouldPrint) printOutput(ch->i, args, precision);
    ch->base.done = true;
}

static void caWriteCallback (evargs args) {
//does nothing except signal that writing is finished.

    //check if status is ok
    if (args.status != ECA_NORMAL){
        errPrint("Error in write callback. %s.\n", ca_message(args.status));
        return;
    }

    struct channel *ch = (struct channel *)args.usr;

    ch->base.done = true;
}


static void getStaticUnitsCallback (evargs args) {
//reads channel units and saves them to global strings. Is called every time a channel
//connects (if needed).

    //check if data was returned
    if (args.status != ECA_NORMAL) {
        return;
    }
    struct channel *ch = (struct channel *)args.usr;

    //get units
    switch(args.type) {
    case DBR_GR_INT:    // and SHORT
        units_get_cb(dbr_gr_short);
        break;
    case DBR_GR_FLOAT:
        units_get_cb(dbr_gr_float);
        break;
    case DBR_GR_CHAR:
        units_get_cb(dbr_gr_float);
        break;
    case DBR_GR_LONG:
        units_get_cb(dbr_gr_long);
        break;
    case DBR_GR_DOUBLE:
        units_get_cb(dbr_gr_double);
        break;
    default:
        errPeriodicPrint("Units for record %s cannot be displayed.\n", ch->base.name);
        break;
    }
}

void monitorLoop (){
//runs monitor loop. Stops after -n updates (camon, cawait) or
//after -timeout is exceeded (cawait).

    while (runMonitor){
        ca_pend_event(0.1);
    }
}

// Wait for request completition of a channel and it's fields, or for caTimeout to occur

/**
 * @brief waitForCompletition Wait for request completition of channels (or it's fields), or for caTimeout to occur
 * @param nChannels number of existing channels
 * @param checkChannels will check for channel completition if true, or for chanel fields completition of false
 */
void waitForCompletition(struct channel *channels, u_int32_t nChannels, bool checkChannels) {
    u_int32_t i;
    size_t j;
    bool elapsed = false, allDone = false;
    epicsTimeStamp timeoutNow, timeout;
    size_t nFields = noFields;

    epicsTimeGetCurrent(&timeout);
    epicsTimeAddSeconds(&timeout, arguments.caTimeout);

    while(!allDone && !elapsed) {
        ca_pend_event(0.1);
        // check for timeout
        epicsTimeGetCurrent(&timeoutNow);
        if (epicsTimeGreaterThanEqual(&timeoutNow, &timeout)) {
            warnPrint("Timeout while waiting for PV response (more than %f seconds elapsed).\n", arguments.caTimeout);
            elapsed = true;
        }

        // check for callback completition
        allDone=true;
        for (i=0; i < nChannels; ++i) {
            if (checkChannels) allDone &= channels[i].base.done || channels[i].base.connectionState == CA_OP_CONN_DOWN;   // ignore disconnected channels
            else for (j=0; j < nFields; ++j) allDone &= channels[i].fields[j].done || channels[i].fields[j].connectionState == CA_OP_CONN_DOWN;   // ignore disconnected field channels
        }
    }
    // reset completition flags
    for (i=0; i < nChannels; ++i) {
        if (checkChannels) channels[i].base.done = false;
        else for (j=0; j < nFields; ++j) channels[i].fields[j].done = false;
    }
}

bool caRequest(struct channel *channels, u_int32_t nChannels) {
//sends get or put requests. ca_get or ca_put are called multiple times, depending on the tool. The reading,
//parsing and printing of returned data is performed in callbacks.
    int status = -1;
    u_int32_t i;
    unsigned long j;

    for(i=0; i < nChannels; i++) {
        if (channels[i].base.connectionState != CA_OP_CONN_UP) {//skip disconnected channels
            continue;
        }

        if (arguments.tool == caget) {
            status = ca_array_get_callback(channels[i].type, channels[i].outNelm, channels[i].base.id, caReadCallback, &channels[i]);
        }
        else if (arguments.tool == caput || arguments.tool == caputq) {
            int32_t baseType = channels[i].type % (LAST_TYPE+1);   // use naked dbr_xx type for put
            void *input = callocMustSucceed(channels[i].inNelm, dbr_size[baseType], "request input");
            int base = 0;   // used for number conversion in strto* functions
            char *endptr;   // used in strto* functions
            bool success = true;

            //if(arguments.bin) base = 2; TODO can be used to select binary input for numbers

            //convert input string to the baseType
            status = ECA_BADTYPE;
            switch (baseType){
            case DBR_INT://and short
                for (j=0; j<channels[i].inNelm; ++j) {
                    ((dbr_int_t *)input)[j] = (dbr_int_t)strtoll(channels[i].writeStr[j], &endptr, base);
                    if (endptr == channels[i].writeStr[j] || *endptr != '\0') {
                        errPrint("Impossible to convert input %s to format %s\n",channels[i].writeStr[j], dbr_type_to_text(baseType));
                        success = false;
                    }
                }
                break;

            case DBR_FLOAT:
                for (j=0; j<channels[i].inNelm; ++j) {
                    ((dbr_float_t *)input)[j] = (dbr_float_t)strtof(channels[i].writeStr[j], &endptr);
                    if (endptr == channels[i].writeStr[j] || *endptr != '\0') {
                        errPrint("Impossible to convert input %s to format %s\n",channels[i].writeStr[j], dbr_type_to_text(baseType));
                        success = false;
                    }
                }
                break;

            case DBR_ENUM:
                ;//check if put data is provided as a number
                bool isNumber = true;
                for (j=0; j<channels[i].inNelm; ++j) {
                    ((dbr_enum_t *)input)[j] = (dbr_enum_t)strtoull(channels[i].writeStr[j], &endptr, base);
                    if (endptr == channels[i].writeStr[j] || *endptr != '\0') {
                        isNumber = false;
                    }
                }

                // if enum is entered as a string, reallocate memory and go to case DBR_STRING
                if (!isNumber) {
                    baseType = DBR_STRING;

                    free(input);
                    input = callocMustSucceed(channels[i].inNelm, dbr_size[baseType], "request input");
                }
                else {
                    break;
                }

            case DBR_STRING:
                for (j=0; j<channels[i].inNelm; ++j) {
                    strcpy(((dbr_string_t *)input)[j], channels[i].writeStr[j]);
                }
                break;

            case DBR_CHAR:{
                    // count number of characters to write
                    size_t charsInStr = 0;
                    size_t charsPerStr[channels[i].inNelm];
                    for (j=0; j < channels[i].inNelm; ++j) {
                        charsPerStr[j] = strlen(channels[i].writeStr[j]);
                        charsInStr += charsPerStr[j];
                    }

                    if (charsInStr != channels[i].inNelm) { // don't fiddle with memory if we don't have to
                        free(input);
                        input = callocMustSucceed(charsInStr, dbr_size[baseType], "request input");
                    }

                    // store all the chars to write
                    charsInStr = 0;
                    for (j=0; j < channels[i].inNelm; ++j) {
                        for (size_t k = 0; k < charsPerStr[j]; k++) {
                            ((dbr_char_t *)input)[charsInStr] = (dbr_char_t)channels[i].writeStr[j][k];
                            charsInStr++;
                        }
                    }
                    channels[i].inNelm = charsInStr;
                }
                break;

            case DBR_LONG:
                for (j=0; j<channels[i].inNelm; ++j) {
                    ((dbr_long_t *)input)[j] = (dbr_long_t)strtoll(channels[i].writeStr[j], &endptr, base);
                    if (endptr == channels[i].writeStr[j] || *endptr != '\0') {
                        errPrint("Impossible to convert input %s to format %s\n",channels[i].writeStr[j], dbr_type_to_text(baseType));
                        success = false;
                    }
                }
                break;

            case DBR_DOUBLE:
                for (j=0; j<channels[i].inNelm; ++j) {
                    ((dbr_double_t *)input)[j] = (dbr_double_t)strtod(channels[i].writeStr[j], &endptr);
                    if (endptr == channels[i].writeStr[j] || *endptr != '\0') {
                        errPrint("Impossible to convert input %s to format %s\n",channels[i].writeStr[j], dbr_type_to_text(baseType));
                        success = false;
                    }
                }
                break;

            default:
                errPrint("Can not print %s DBR type (DBR numeric type code: %"PRId32"). \n", dbr_type_to_text(baseType), baseType);
                success = false;
            }

            if (success) {
                if(arguments.tool == caputq) status = ca_array_put(baseType, channels[i].inNelm, channels[i].base.id, input);
                else status = ca_array_put_callback(baseType, channels[i].inNelm, channels[i].base.id, input, caWriteCallback, &channels[i]);
                free(input);
            }
            else {
                free(input);
                return false;
            }
        }
        else if(arguments.tool == cagets && channels[i].proc.created) {
            dbr_char_t input=1;
            status = ca_array_put_callback(DBF_CHAR, 1, channels[i].proc.id, &input, caWriteCallback, &channels[i]);
        }
        else if(arguments.tool == cado) {
            dbr_char_t input = 1;
            status = ca_array_put(DBF_CHAR, 1, channels[i].base.id, &input); // old PSI tools behaved this way. Is it better to fill entire array?
        }
        if (status != ECA_NORMAL) {
            errPrint("Problem creating request for process variable %s: %s.\n", channels[i].base.name, ca_message(status));
            return false;
        }
    }

    if(arguments.tool == cado || arguments.tool == caputq) return true;  // we do not wait if cado or caputq

     // wait for callbacks
    waitForCompletition(channels, nChannels, true);

    bool success = true;
    //if caput or cagets issue a new read request.
    if (arguments.tool == caput || arguments.tool == cagets){

        for (i=0; i<nChannels; ++i) {
            if (channels[i].base.connectionState != CA_OP_CONN_UP) {//skip disconnected channels
                continue;
            }
            status = ca_array_get_callback(channels[i].type, channels[i].outNelm, channels[i].base.id, caReadCallback, &channels[i]);
            if (status != ECA_NORMAL) {
                errPrint("Problem creating get request for channel %s: %s.\n", channels[i].base.name,ca_message(status));
                return false;
            }

        }

        waitForCompletition(channels, nChannels, true);
    }

    return success;
}

bool cainfoRequest(struct channel *channels, u_int32_t nChannels){
//this function does all the work for caInfo tool. Reads channel data using ca_get and then prints.
    int status;
    u_int32_t i, j;

    bool readAccess, writeAccess;
    u_int32_t nFields = noFields;
    const char *delimeter = "-------------------------------";

    for(i=0; i < nChannels; i++){

        if (channels[i].base.connectionState != CA_OP_CONN_UP) continue; // skip unconnected channels

        channels[i].count = ca_element_count ( channels[i].base.id );

        channels[i].type = dbf_type_to_DBR_CTRL(ca_field_type(channels[i].base.id));

        //allocate data for all caget returns
        void *data;
        struct dbr_sts_string *fieldData[nFields];

        data = callocMustSucceed(1, dbr_size_n(channels[i].type, channels[i].count), "caInfoRequest");


        //general ctrl data
        status = ca_array_get(channels[i].type, channels[i].count, channels[i].base.id, data);
        if (status != ECA_NORMAL) {
            errPrint("CA error %s occurred while trying to create ca_get request for record %s.\n", ca_message(status), channels[i].base.name);
            return false;
        }

        for(j=0; j < nFields; j++) {
            if(channels[i].fields[j].connectionState == CA_OP_CONN_UP) {
                fieldData[j] = callocMustSucceed(1, dbr_size_n(DBR_STS_STRING, 1), "caInfoRequest: field");
                status = ca_array_get(DBR_STS_STRING, 1, channels[i].fields[j].id, fieldData[j]);
                if (status != ECA_NORMAL) errPrint("Problem reading %s field of record %s: %s.\n", fields[j], channels[i].base.name, ca_message(status));
            }
            else {
                fieldData[j] = NULL;
            }
        }


        status = ca_pend_io(arguments.caTimeout);
        if (status != ECA_NORMAL) {
            if (status == ECA_TIMEOUT) {
                errPrint("All issued requests for record fields did not complete.\n");
            }
            else {
                errPrint("All issued requests for record fields did not complete.\n");
                return false;
            }
        }


        //start printing
        fputc('\n',stdout);
        fputc('\n',stdout);
        printf("%s\n%s\n", delimeter, channels[i].base.name);                                          //name
        if(fieldData[field_desc] != NULL) printf("\tDescription: %s\n", fieldData[field_desc]->value);          //description
        printf("\tNative DBF type: %s\n", dbf_type_to_text(ca_field_type(channels[i].base.id)));                     //field type
        printf("\tNumber of elements: %ld\n", channels[i].count);                                               //number of elements


        evargs args;
        args.chid = channels[i].base.id;
        args.count = ca_element_count ( channels[i].base.id );
        args.dbr = data;
        args.status = ECA_NORMAL;
        args.type = channels[i].type; //ca_field_type(channels[i].base.id);
        args.usr = &channels[i];
        printf("\tValue: ");
        printValue(args, -1);
        fputc('\n',stdout);

        switch (channels[i].type){
        case DBR_CTRL_STRING:
            fputc('\n',stdout);
            printf("\tAlarm status: %s, severity: %s\n",
                    epicsAlarmConditionStrings[((struct dbr_sts_string *)data)->status],
                    epicsAlarmSeverityStrings[((struct dbr_sts_string *)data)->severity]);      //status and severity
            break;
        case DBR_CTRL_INT://and short
            printf("\tUnits: %s\n", ((struct dbr_ctrl_int *)data)->units);            	//units
            fputc('\n',stdout);
            printf("\tAlarm status: %s, severity: %s\n",
                    epicsAlarmConditionStrings[((struct dbr_ctrl_int *)data)->status],
                    epicsAlarmSeverityStrings[((struct dbr_ctrl_int *)data)->severity]);      //status and severity
            fputc('\n',stdout);
            printf("\tWarning\tupper limit: %" PRId16"\n\t\tlower limit: %"PRId16"\n",
                    ((struct dbr_ctrl_int *)data)->upper_warning_limit, ((struct dbr_ctrl_int *)data)->lower_warning_limit); //warning limits
            printf("\tAlarm\tupper limit: %" PRId16"\n\t\tlower limit: %" PRId16"\n",
                    ((struct dbr_ctrl_int *)data)->upper_alarm_limit, ((struct dbr_ctrl_int *)data)->lower_alarm_limit); //alarm limits
            printf("\tControl\tupper limit: %"PRId16"\n\t\tlower limit: %"PRId16"\n", ((struct dbr_ctrl_int *)data)->upper_ctrl_limit,\
                    ((struct dbr_ctrl_int *)data)->lower_ctrl_limit);                  	//control limits
            printf("\tDisplay\tupper limit: %"PRId16"\n\t\tower limit: %"PRId16"\n",
                   ((struct dbr_ctrl_int *)data)->upper_disp_limit, ((struct dbr_ctrl_int *)data)->lower_disp_limit);                  	//display limits
            break;
        case DBR_CTRL_FLOAT:
            printf("\tUnits: %s\n", ((struct dbr_ctrl_float *)data)->units);
            fputc('\n',stdout);
            printf("\tAlarm status: %s, severity: %s\n",
                    epicsAlarmConditionStrings[((struct dbr_ctrl_float *)data)->status],
                    epicsAlarmSeverityStrings[((struct dbr_ctrl_float *)data)->severity]);
            printf("\n");
            printf("\tWarning\tupper limit: %f\n\t\tlower limit: %f\n",
                    ((struct dbr_ctrl_float *)data)->upper_warning_limit, ((struct dbr_ctrl_float *)data)->lower_warning_limit);
            printf("\tAlarm\tupper limit: %f\n\t\tlower limit: %f\n",
                    ((struct dbr_ctrl_float *)data)->upper_alarm_limit, ((struct dbr_ctrl_float *)data)->lower_alarm_limit);
            printf("\tControl\tupper limit: %f\n\t\tlower limit: %f\n",
                    ((struct dbr_ctrl_float *)data)->upper_ctrl_limit, ((struct dbr_ctrl_float *)data)->lower_ctrl_limit);
            printf("\tDisplay\tupper limit: %f\n\t\tlower limit: %f\n",
                    ((struct dbr_ctrl_float *)data)->upper_disp_limit, ((struct dbr_ctrl_float *)data)->lower_disp_limit);
            fputc('\n',stdout);
            printf("\tPrecision: %"PRId16"\n",((struct dbr_ctrl_float *)data)->precision);
            printf("\tRISC alignment: %"PRId16"\n",((struct dbr_ctrl_float *)data)->RISC_pad);
            break;
        case DBR_CTRL_ENUM:
            fputc('\n',stdout);
            printf("\tAlarm status: %s, severity: %s\n",
                    epicsAlarmConditionStrings[((struct dbr_ctrl_enum *)data)->status],
                    epicsAlarmSeverityStrings[((struct dbr_ctrl_enum *)data)->severity]);

            printf("\tNumber of enum strings: %"PRId16"\n", ((struct dbr_ctrl_enum *)data)->no_str);
            for (j=0; j < (u_int32_t)(((struct dbr_ctrl_enum *)data)->no_str); ++j) {
                printf("\tstring %"PRIu32": %s\n", j, ((struct dbr_ctrl_enum *)data)->strs[j]);
            }
            break;
        case DBR_CTRL_CHAR:
            printf("\tUnits: %s\n", ((struct dbr_ctrl_char *)data)->units);
            fputc('\n',stdout);
            printf("\tAlarm status: %s, severity: %s\n",
                    epicsAlarmConditionStrings[((struct dbr_ctrl_char *)data)->status],
                    epicsAlarmSeverityStrings[((struct dbr_ctrl_char *)data)->severity]);
            printf("\n");
            printf("\tWarning\tupper limit: %c\n\t\tlower limit: %c\n",
                    ((struct dbr_ctrl_char *)data)->upper_warning_limit, ((struct dbr_ctrl_char *)data)->lower_warning_limit);
            printf("\tAlarm\tupper limit: %c\n\t\tlower limit: %c\n",
                    ((struct dbr_ctrl_char *)data)->upper_alarm_limit, ((struct dbr_ctrl_char *)data)->lower_alarm_limit);
            printf("\tControl\tupper limit: %c\n\t\tlower limit: %c\n", ((struct dbr_ctrl_char *)data)->upper_ctrl_limit,\
                    ((struct dbr_ctrl_char *)data)->lower_ctrl_limit);
            printf("\tDisplay\tupper limit: %c\n\t\tlower limit: %c\n", ((struct dbr_ctrl_char *)data)->upper_disp_limit,\
                    ((struct dbr_ctrl_char *)data)->lower_disp_limit);
            break;
        case DBR_CTRL_LONG:
            printf("\tUnits: %s\n", ((struct dbr_ctrl_long *)data)->units);
            fputc('\n',stdout);
            printf("\tAlarm status: %s, severity: %s\n",
                    epicsAlarmConditionStrings[((struct dbr_ctrl_long *)data)->status],
                    epicsAlarmSeverityStrings[((struct dbr_ctrl_long *)data)->severity]);
            fputc('\n',stdout);
            printf("\tWarning\tupper limit: %"PRId32"\n\t\tlower limit: %"PRId32"\n",
                    ((struct dbr_ctrl_long *)data)->upper_warning_limit, ((struct dbr_ctrl_long *)data)->lower_warning_limit);
            printf("\tAlarm\tupper limit: %"PRId32"\n\t\tlower limit: %"PRId32"\n",
                    ((struct dbr_ctrl_long *)data)->upper_alarm_limit, ((struct dbr_ctrl_long *)data)->lower_alarm_limit);
            printf("\tControl\tupper limit: %"PRId32"\n\t\tlower limit: %"PRId32"\n", ((struct dbr_ctrl_long *)data)->upper_ctrl_limit,\
                    ((struct dbr_ctrl_long *)data)->lower_ctrl_limit);
            printf("\tDisplay\tupper limit: %"PRId32"\n\t\tlower limit: %"PRId32"\n", ((struct dbr_ctrl_long *)data)->upper_disp_limit,\
                    ((struct dbr_ctrl_long *)data)->lower_disp_limit);
            break;
        case DBR_CTRL_DOUBLE:
            printf("\tUnits: %s\n", ((struct dbr_ctrl_double *)data)->units);
            fputc('\n',stdout);
            printf("\tAlarm status: %s, severity: %s\n",
                    epicsAlarmConditionStrings[((struct dbr_ctrl_double *)data)->status],
                    epicsAlarmSeverityStrings[((struct dbr_ctrl_double *)data)->severity]);
            fputc('\n',stdout);
            printf("\tWarning\tupper limit: %f\n\t\tlower limit: %f\n",
                    ((struct dbr_ctrl_double *)data)->upper_warning_limit, ((struct dbr_ctrl_double *)data)->lower_warning_limit);
            printf("\tAlarm\tupper limit: %f\n\t\tlower limit: %f\n",
                    ((struct dbr_ctrl_double *)data)->upper_alarm_limit, ((struct dbr_ctrl_double *)data)->lower_alarm_limit);
            printf("\tControl\tupper limit: %f\n\t\tlower limit: %f\n", ((struct dbr_ctrl_double *)data)->upper_ctrl_limit,\
                    ((struct dbr_ctrl_double *)data)->lower_ctrl_limit);
            printf("\tDisplay\tupper limit: %f\n\t\tlower limit: %f\n", ((struct dbr_ctrl_double *)data)->upper_disp_limit,\
                    ((struct dbr_ctrl_double *)data)->lower_disp_limit);
            fputc('\n',stdout);
            printf("\tPrecision: %"PRId16"\n",((struct dbr_ctrl_double *)data)->precision);
            printf("\tRISC alignment: %"PRId16"\n",((struct dbr_ctrl_double *)data)->RISC_pad0);
            break;
        }

        fputc('\n',stdout);
        for(j=field_hhsv; j < nFields; j++) {
            if (fieldData[j] != NULL) {
                printf("\t%s alarm severity: %s\n", fields[j], fieldData[j]->value);
                free(fieldData[j]);
            }
        }
        fputc('\n',stdout);

        readAccess = ca_read_access(channels[i].base.id);
        writeAccess = ca_write_access(channels[i].base.id);

        printf("\tIOC name: %s\n", ca_host_name(channels[i].base.id));                           //host name
        printf("\tRead access: "); if(readAccess) printf("yes\n"); else printf("no\n");     //read and write access
        printf("\tWrite access: "); if(writeAccess) printf("yes\n"); else printf("no\n");
        printf("%s\n", delimeter);

        free(data);
    }
    return true;
}

void channelStatusCallback(struct connection_handler_args args){
//callback for ca_create_channel. Is executed whenever a channel connects or
//disconnects. When the channel connects for the first time, several properties
//are set, such as number of elements and request types.
//Upon (every) connection, a separate request to obtain units is issued if
//needed. On channel disconnect, the data is cleared.

    struct channel *ch = ( struct channel * ) ca_puser ( args.chid );
    int status;

    if (ch->base.connectionState == CA_OP_OTHER) {
        ch->base.done = true;   // set channel to done only on first connection, not when connection goes up / down
    }

    ch->base.connectionState = args.op;

    if ( args.op == CA_OP_CONN_UP ) {
        //how many array elements to request
        ch->count = ca_element_count ( ch->base.id );
        if (arguments.outNelm == -1) ch->outNelm = ch->count;
        else if(arguments.outNelm > 0 && (unsigned long) arguments.outNelm < ch->count) ch->outNelm = (unsigned long) arguments.outNelm;
        else{
            ch->outNelm = ch->count;
            warnPeriodicPrint("Invalid number of requested elements to read (%"PRId64") from %s - reading maximum number of elements (%lu).\n", arguments.outNelm, ch->base.name, ch->count);
        }

        //request type
        if (arguments.dbrRequestType == -1){   //if DBR type not specified, decide based on desired details
            if (arguments.time || arguments.date || arguments.timestamp){
                //time specified, use TIME_
                if (arguments.str && ca_field_type(ch->base.id) == DBF_ENUM){
                    //if enum && s, use time_string
                    ch->type = DBR_TIME_STRING;
                }
                else{ //else use time_native
                    ch->type = dbf_type_to_DBR_TIME(ca_field_type(ch->base.id));
                }
            }
            else{// use GR_ by default
                ch->type = dbf_type_to_DBR_GR(ca_field_type(ch->base.id));
            }
        }
        else ch->type = arguments.dbrRequestType;

        //if units, issue get request for metadata. String and enum records don't have units, so skip.
        int32_t baseType = ca_field_type(ch->base.id);
        if (!arguments.nounit && baseType != DBF_STRING && baseType != DBF_ENUM){
            int32_t reqType = dbf_type_to_DBR_GR(baseType);

            status = ca_array_get_callback(reqType, ch->count, ch->base.id, getStaticUnitsCallback, ch);
            if (status != ECA_NORMAL) {
                errPeriodicPrint("Problem creating ca_get request for process variable %s: %s\n",ch->base.name, ca_message(status));
                return;
            }
            status = ca_flush_io();//flush this so it gets processed before everything else.
            if (status != ECA_NORMAL) {
                errPeriodicPrint("Problem flushing process variable %s: %s.\n", ch->base.name, ca_message(status));
            }
        }
    }
    else {
        printf(" %s disconnected\n", ch->base.name);
    }
}

void channelFieldStatusCallback(struct connection_handler_args args){
//callback for ca_create_channel. Is executed whenever a channel connects or
//disconnects. It is used for field channels in caInfo tool.
//When a field for a channel that exist cannot be created, we clear the channel for the field.

    struct field *field = ( struct field * ) ca_puser ( args.chid );

    field->connectionState = args.op;

    if ( args.op != CA_OP_CONN_UP ) {
        ca_clear_channel(args.chid);
    }

    field->done = true;
}

bool checkStatus(int status){
//checks status of channel create_ and clear_
    if (status != ECA_NORMAL){
        errPrint("CA error %s occurred\n", ca_message(status));
        SEVCHK(status, "CA error");
        return false;
    }
    return true;
}

bool createChannelMustSucceed(const char *pChanName, caCh *pConnStateCallback, void *pUserPrivate, capri priority, chid *pChanID){
//creates CA connection
    int status = ca_create_channel(pChanName, pConnStateCallback, pUserPrivate, priority, pChanID);
    return checkStatus(status);    // error if the channels are not created
}

void caCustomExceptionHandler( struct exception_handler_args args) {
    char buf[512];
    const char *pName;
    epicsTimeStamp timestamp;	//timestamp indicating program start

    if ( args.chid ) {
        pName = ca_name ( args.chid );
    }
    else {
        pName = "?";
    }

    epicsTimeGetCurrent(&timestamp);
    epicsTimeToStrftime(errorTimestamp, LEN_TIMESTAMP, "%Y-%m-%d %H:%M:%S.%06f", &timestamp);

    if (args.stat == ECA_DISCONN || args.stat == ECA_DISCONNCHID) {
        printf("%s ", errorTimestamp);
    }
    else {
        sprintf ( buf,
                "%s in %s - with request chanel=%s operation=%ld data type=%s count=%ld",
                errorTimestamp, args.ctx, pName, args.op, dbr_type_to_text ( args.type ), args.count );
        ca_signal ( args.stat, buf );
    }
}

bool caInit(struct channel *channels, u_int32_t nChannels){
//creates contexts and channels.
    int status;
    u_int32_t i;

    status = ca_context_create(ca_disable_preemptive_callback);

    if (!checkStatus(status)) return false;

    ca_add_exception_event ( caCustomExceptionHandler , 0 );

    for(i=0; i < nChannels; i++) {
        channels[i].base.created = createChannelMustSucceed(channels[i].base.name, channelStatusCallback, &channels[i], CA_PRIORITY, &channels[i].base.id);
        if (!channels[i].base.created) return false;

        //if tool = cagets, each channel has a sibling connecting to the proc field
        if (arguments.tool == cagets) {
            channels[i].proc.created = createChannelMustSucceed(channels[i].proc.name, 0 , 0, CA_PRIORITY, &channels[i].proc.id);
            if (!channels[i].proc.created) return false;
        }
    }

    //wait for connection callbacks to complete
    waitForCompletition(channels, nChannels, true);

    for (i=0; i < nChannels; ++i) {
        if (channels[i].base.connectionState != CA_OP_CONN_UP) {
            printf("Process variable %s not connected.\n", channels[i].base.name);
            if ((arguments.tool == camon || arguments.tool == cawait)) numMonitorUpdates++;   // connection callback does not count towards number of updates.
        }

        if (arguments.tool == camon || arguments.tool == cawait) {
            //create subscription
            status = ca_create_subscription(channels[i].type, channels[i].outNelm, channels[i].base.id, DBE_VALUE | DBE_ALARM | DBE_LOG, caReadCallback, &channels[i], 0);
            if (status != ECA_NORMAL) {
                errPrint("Problem creating subscription for process variable %s: %s.\n",channels[i].base.name, ca_message(status));
            }
        }
    }

    if (arguments.tool == cawait && arguments.timeout!=-1){
        //set first
        epicsTimeGetCurrent(&timeoutTime);
        epicsTimeAddSeconds(&timeoutTime, arguments.timeout);
    }
    if (arguments.tool == camon || arguments.tool == cawait) {
        waitForCompletition(channels, nChannels, true);
    }

    if (arguments.tool == cainfo){
        //if tool = cainfo, each channel has siblings connecting to the desc and *sv fields
        for (i=0; i<nChannels; ++i){
            if (channels[i].base.connectionState != CA_OP_CONN_UP) { // if not connected skip
                continue;
            }

            size_t nFields = noFields;  // so we don't calculate in each loop
            size_t j;
            for (j=0; j < nFields; j++) {
                channels[i].fields[j].created = true;
                if (!createChannelMustSucceed(channels[i].fields[j].name, channelFieldStatusCallback, &channels[i].fields[j], CA_PRIORITY, &channels[i].fields[j].id)) {
                    errPrint("Problem creating subscription for process variable %s: %s.\n", channels[i].fields[j].name, ca_message(status));
                    channels[i].fields[j].created = false;
                }
            }
            ca_flush_io();
        }
        // process channel creation for fields
        waitForCompletition(channels, nChannels, false);
    }

    return true;
}

bool caDisconnect(struct channel * channels, u_int32_t nChannels){
    int status;
    u_int32_t i;
    bool success = true;
    size_t nFields = noFields;

    for (i=0; i < nChannels; i++) {
        if (channels[i].base.created) {
            status = ca_clear_channel(channels[i].base.id);
            success &= checkStatus(status);
        }

        if (arguments.tool == cagets) {
            if (channels[i].proc.created) {
                status = ca_clear_channel(channels[i].proc.id);
                success &= checkStatus(status);
            }
        }

        if (arguments.tool == cainfo) {
            for(size_t j=0; j < nFields; j++) {
                if (channels[i].proc.created) {
                    status = ca_clear_channel(channels[i].proc.id);
                    success &= checkStatus(status);
                }
            }
        }
    }
    return success;
}


bool endsWith(char * src, char * match) {
//checks whether end of src matches the string match.
    if (strlen(src) >= strlen(match)) {
        return !strcmp(src + (strlen(src)-strlen(match)), match) ;
    }
    else return false;
}

/**
 * @brief getBaseChannelName will strip chanel name to base channel name. Base channel name is chanel name, without field names (without .VAL, .PREC, ...)
 * @param name channel name to strip (null terminated string)
 */
void getBaseChannelName(char *name) {
    size_t len = strlen(name);
    size_t i = 0;

    while(i <= len) {  // field name + '.'
        if(name[len-i] == '.') {    // found '.' which separates base record name and field name
            name[len-i] = '\0';     // just terminate the string here.
            break;
        }
        i++;
    }
}

size_t truncate(char *argument) {
    size_t length = strlen(argument);
    if (length > MAX_STRING_SIZE) {
        warnPrint("Input %s is longer than the allowed size %u. Truncating to ", argument, MAX_STRING_SIZE);
        length = MAX_STRING_SIZE;
        argument[length] = '\0';
        warnPrint("%s\n", argument);   // end of upper fprintf
    }
    return length;
}

int main ( int argc, char ** argv )
{//main function: reads arguments, allocates memory, calls ca* functions, frees memory and exits.

    int opt;                    // getopt() current option
    int opt_long;               // getopt_long() current long option
    u_int32_t nChannels=0;              // Number of channels
    u_int32_t i,j;                      // counter
    struct channel *channels;

    runMonitor = true;

    if (endsWith(argv[0],"caget")) arguments.tool = caget;
    if (endsWith(argv[0],"caput")) arguments.tool = caput;
    if (endsWith(argv[0],"cagets")) arguments.tool = cagets;
    if (endsWith(argv[0],"caputq")) arguments.tool = caputq;
    if (endsWith(argv[0],"camon")) arguments.tool = camon;
    if (endsWith(argv[0],"cawait")) arguments.tool = cawait;
    if (endsWith(argv[0],"cado")) arguments.tool = cado;
    if (endsWith(argv[0],"cainfo")) arguments.tool = cainfo;

    epicsTimeGetCurrent(&programStartTime);

    static struct option long_options[] = {
        {"num",     	no_argument,        0,  0 },
        {"int",     	no_argument,        0,  0 },    //same as num
        {"round",   	required_argument,  0,  0 },	//type of rounding:round, ceil, floor
        {"prec",    	required_argument,  0,  0 },	//precision
        {"hex",     	no_argument,        0,  0 },	//display as hex
        {"bin",     	no_argument,        0,  0 },	//display as bin
        {"oct",     	no_argument,        0,  0 },	//display as oct
        {"plain",   	no_argument,        0,  0 },	//ignore formatting switches
        {"stat",    	no_argument,       0,  0 },	//status and severity on
        {"nostat",  	no_argument,        0,  0 },	//status and severity off
        {"noname",  	no_argument,        0,  0 },	//hide name
        {"nounit",  	no_argument,        0,  0 },	//hide units
        {"timestamp",	required_argument, 	0,  0 },	//timestamp type r,u,c
        {"localdate",	no_argument,       0,  0 },	//client date
        {"time",      no_argument,       0,  0 },	//server time
        {"localtime",	no_argument,       0,  0 },	//client time
        {"date",      no_argument,       0,  0 },	//server date
        {"outNelm", 	required_argument,	0,  0 },	//number of array elements - read
        {"outSep",      required_argument,	0,  0 },	//array field separator - read
        {"inSep",      required_argument,	0,  0 },	//array field separator - write
        {"nord",      no_argument,      0,  0 },	//display number of array elements
        {"tool",      required_argument, 	0,	0 },	//tool
        {"timeout",   	required_argument, 	0,	0 },	//timeout
        {"dbrtype",   	required_argument, 	0,	0 },	//dbrtype
        {0,         	0,                 	0,  0 }
    };
    putenv("POSIXLY_CORRECT="); //Behave correctly on GNU getopt systems = stop parsing after 1st non option is encountered

    while ((opt = getopt_long_only(argc, argv, ":w:e:f:g:n:sthdav:", long_options, &opt_long)) != -1) {
        switch (opt) {
        case 'w':
            if (sscanf(optarg, "%lf", &arguments.caTimeout) != 1){    // type was not given as float
                arguments.caTimeout = CA_DEFAULT_TIMEOUT;
                warnPrint("Requested timeout invalid - ignored. ('%s -h' for help.)\n", argv[0]);
            }
            break;
        case 'd': // same as date
            arguments.date = true;
            break;
        case 'e':	//how to format doubles in strings
        case 'f':
        case 'g':
            ;//declaration must not follow label
            int32_t digits;
            if (sscanf(optarg, "%"SCNd32, &digits) != 1){
                warnPrint("Invalid precision argument '%s' "
                        "for option '-%c' - ignored.\n", optarg, opt);
            } else {
                if (digits>=0){
                    if (arguments.dblFormatType == '\0' && arguments.prec >= 0) { // in case of overiding -prec
                        warnPrint("Option '-%c' overrides precision set with '-prec'.\n", opt);
                    }
                    arguments.dblFormatType = opt; // set 'e' 'f' or 'g'
                    arguments.prec = digits;
                }
                else{
                    warnPrint("Precision %d for option '-%c' "
                            "out of range - ignored.\n", digits, opt);
                }
            }
            break;
        case 's':	//try to interpret values as strings
            arguments.str = true;
            break;
        case 't':	//same as time
            arguments.time = true;
            break;
        case 'n':	//stop monitor after numUpdates
            if (sscanf(optarg, "%"SCNd64, &arguments.numUpdates) != 1) {
                warnPrint("Invalid argument '%s' for option '-%c' - ignored.\n", optarg, opt);
                arguments.numUpdates = -1;
            }
            else {
                if (arguments.numUpdates < 0) {
                    warnPrint("Number of updates for option '-%c' must be greater or equal to zero - ignored.\n", opt);
                    arguments.numUpdates = -1;
                }
            }
            break;
        case 'a':
            arguments.parseArray = true;
            break;
        case 'v':
            if (sscanf(optarg, "%"SCNu16, &arguments.verbosity) != 1) {
                warnPrint("Invalid argument '%s' for option '-%c' - ignored.\n", optarg, opt);
            }
            break;
        case 0:   // long options
            switch (opt_long) {
            case 0:   // num
                arguments.num = true;
                break;
            case 1:   // int
                arguments.num = true;
                break;
            case 2:   // round
                ;//declaration must not follow label
                int type;
                if (sscanf(optarg, "%d", &type) != 1) {   // type was not given as a number [0, 1, 2]
                    if(!strcmp("round", optarg)) {
                        arguments.round = roundType_round;
                    } else if(!strcmp("ceil", optarg)) {
                        arguments.round = roundType_ceil;
                    } else if(!strcmp("floor", optarg)) {
                        arguments.round = roundType_floor;
                    } else {
                        arguments.round = roundType_round;
                        warnPrint("Invalid round type '%s' "
                            "for option '%s' - ignored.\n", optarg, long_options[opt_long].name);
                    }
                } else { // type was given as a number
                    if( type < roundType_round || roundType_floor < type) {   // out of range check
                        arguments.round = roundType_no_rounding;
                        warnPrint("Invalid round type '%s' "
                            "for option '%s' - ignored.\n", optarg, long_options[opt_long].name);
                    } else{
                        arguments.round = type;
                    }
                }
                break;
            case 3:   // prec
                if (arguments.dblFormatType == '\0') { // formating with -f -g -e has priority
                    if (sscanf(optarg, "%"SCNd32, &arguments.prec) != 1){
                        warnPrint("Invalid precision argument '%s' "
                                "for option '%s' - ignored.\n", optarg, long_options[opt_long].name);
                    } else if (arguments.prec < 0) {
                       warnPrint("Precision %"PRId32" for option '%s' "
                               "out of range - ignored.\n", arguments.prec, long_options[opt_long].name);
                       arguments.prec = -1;
                    }
                }
                else {
                    warnPrint("Option '-prec' ignored because of '-f', '-e' or '-g'.\n");
                }
                break;
            case 4:   //hex
                arguments.hex = true;
                break;
            case 5:   // bin
                arguments.bin = true;
                break;
            case 6:   // oct
                arguments.oct = true;
                break;
            case 7:   // plain
                arguments.plain = true;
                break;
            case 8:   // stat
                arguments.stat = true;
                break;
            case 9:	  // nostat
                arguments.nostat = true;
                break;
            case 10:   // noname
                arguments.noname = true;
                break;
            case 11:   // nounit
                arguments.nounit = true;
                break;
            case 12:   // timestamp
                if (sscanf(optarg, "%c", &arguments.timestamp) != 1){
                   warnPrint("Invalid argument '%s' "
                            "for option '%s' - ignored. Allowed arguments: r,u,c.\n", optarg, long_options[opt_long].name);
                    arguments.timestamp = 0;
                } else {
                    if (arguments.timestamp != 'r' && arguments.timestamp != 'u' && arguments.timestamp != 'c') {
                        warnPrint("Invalid argument '%s' "
                            "for option '%s' - ignored. Allowed arguments: r,u,c.\n", optarg, long_options[opt_long].name);
                        arguments.timestamp = 0;
                    }
                }
                break;
            case 13:   // localdate
                arguments.localdate = true;
                break;
            case 14:   // time
                arguments.time = true;
                break;
            case 15:   // localtime
                arguments.localtime = true;
                break;
            case 16:   // date
                arguments.date = true;
                break;
            case 17:   // outNelm - number of elements - read
                if (sscanf(optarg, "%"SCNd64, &arguments.outNelm) != 1){
                    warnPrint("Invalid count argument '%s' "
                            "for option '%s' - ignored.\n", optarg, long_options[opt_long].name);
                }
                else {
                    if (arguments.outNelm < 1) {
                        warnPrint("Count number for option '%s' "
                                "must be positive integer - ignored.\n", long_options[opt_long].name);
                        arguments.outNelm = -1;
                    }
                }
                break;
            case 18:   // field separator for output
                if (sscanf(optarg, "%c", &arguments.fieldSeparator) != 1){
                    warnPrint("Invalid argument '%s' "
                            "for option '%s' - ignored.\n", optarg, long_options[opt_long].name);
                }
                break;
            case 19:   // field separator for input
                if (sscanf(optarg, "%c", &arguments.inputSeparator) != 1){
                    warnPrint("Invalid argument '%s' "
                            "for option '%s' - ignored.\n", optarg, long_options[opt_long].name);
                }
                else {
                    arguments.parseArray = true;
                }
                break;
            case 20:   // nord
                arguments.nord = true;
                break;
            case 21:	// tool
                // rev RV: what is this line below
                ;//c
                int tool;
                if (sscanf(optarg, "%d", &tool) != 1){   // type was not given as a number [0, 1, 2]
                    if(!strcmp("caget", optarg)){
                        arguments.tool = caget;
                    } else if(!strcmp("camon", optarg)){
                        arguments.tool = camon;
                    } else if(!strcmp("caput", optarg)){
                        arguments.tool = caput;
                    } else if(!strcmp("caputq", optarg)){
                        arguments.tool = caputq;
                    } else if(!strcmp("cagets", optarg)){
                        arguments.tool = cagets;
                    } else if(!strcmp("cado", optarg)){
                        arguments.tool = cado;
                    } else if(!strcmp("cawait", optarg)){
                        arguments.tool = cawait;
                    } else if(!strcmp("cainfo", optarg)){
                        arguments.tool = cainfo;
                    }
                } else{ // type was given as a number
                    if(tool >= caget|| tool <= cainfo){   //unknown tool case handled down below
                        arguments.round = tool;
                    }
                }
                break;
            case 22: //timeout
                if (sscanf(optarg, "%lf", &arguments.timeout) != 1){
                    warnPrint("Invalid timeout argument '%s' "
                            "for option '%s' - ignored.\n", optarg, long_options[opt_long].name);
                }
                else {
                    if (arguments.timeout <= 0) {
                        warnPrint("Timeout argument must be positive - ignored.\n");
                        arguments.timeout = -1;
                    }
                }
                break;
            case 23: //dbrtype
                if (sscanf(optarg, "%"SCNd32, &arguments.dbrRequestType) != 1)     // type was not given as an integer
                {
                    dbr_text_to_type(optarg, arguments.dbrRequestType);
                    if (arguments.dbrRequestType == -1)                   // Invalid? Try prefix DBR_
                    {
                        char str[30] = "DBR_";
                        strncat(str, optarg, 25);
                        dbr_text_to_type(str, arguments.dbrRequestType);
                    }
                }
                if (arguments.dbrRequestType < DBR_STRING    || arguments.dbrRequestType > DBR_CTRL_DOUBLE){
                    warnPrint("Requested dbr type out of range "
                            "or invalid - ignored. ('%s -h' for help.)\n", argv[0]);
                    arguments.dbrRequestType = -1;
                }
                break;
            }
            break;
        case '?':
            fprintf(stderr, "Unrecognized option: '-%c'. ('%s -h' for help.). Exiting.\n", optopt, argv[0]);
            return EXIT_FAILURE;
            break;
        case ':':
            fprintf(stderr, "Option '-%c' requires an argument. ('%s -h' for help.). Exiting.\n", optopt, argv[0]);
            return EXIT_FAILURE;

            break;
        case 'h':               //Print usage
            usage(stdout, arguments.tool, argv[0]);
            return EXIT_SUCCESS;
        default:
            usage(stderr, arguments.tool, argv[0]);
            exit(EXIT_FAILURE);
        }
     }

     //check mutually exclusive arguments (without taking dbr type into account)
     if (arguments.tool == tool_unknown){
         usage(stderr, arguments.tool,argv[0]);
         return EXIT_FAILURE;
     }
     if(arguments.tool == cainfo && (arguments.str || arguments.num || arguments.bin  || arguments.hex || arguments.oct \
             || arguments.dbrRequestType != -1 || arguments.prec != -1 || arguments.round != roundType_no_rounding || arguments.plain \
             || arguments.dblFormatType != '\0' || arguments.stat || arguments.nostat || arguments.noname || arguments.nounit \
             || arguments.timestamp != '\0' || arguments.timeout != -1 || arguments.date || arguments.time || arguments.localdate \
             || arguments.localtime || arguments.fieldSeparator != ' ' || arguments.inputSeparator != ' ' || arguments.numUpdates != -1\
             || arguments.parseArray || arguments.outNelm != -1 || arguments.nord)){
         warnPrint("The only options allowed for cainfo are -w and -v. Ignoring the rest.\n");
     }
     else if (arguments.tool != camon){
         if (arguments.timestamp != 0){
             warnPrint("Option -timestamp can only be specified with camon.\n");
         }
         if (arguments.numUpdates != -1){
             warnPrint("Option -n can only be specified with camon.\n");
         }
     }
     else if ((arguments.parseArray || arguments.inputSeparator !=' ') && (arguments.tool != caput && arguments.tool != caputq)){
         warnPrint("Option -a and -inSep can only be specified with caput or caputq.\n");
     }
     if ((arguments.outNelm != -1 || arguments.num !=false || arguments.hex !=false || arguments.bin !=false || arguments.oct !=false)\
             && (arguments.tool == cado || arguments.tool == caputq)){
         warnPrint("Option -outNelm, -num, -hex, -bin and -oct cannot be specified with cado or caputq, because they have no output.\n");
     }
     if (arguments.nostat != false && arguments.stat != false){
         warnPrint("Options -stat and -nostat are mutually exclusive.\n");
         arguments.nostat = false;
     }
     if (arguments.hex + arguments.bin + arguments.oct > 1 ) {
         arguments.hex = false;
         arguments.bin = false;
         arguments.oct = false;
         warnPrint("Options -hex and -bin and -oct are mutually exclusive. Ignoring.\n");
     }
     if (arguments.num && arguments.str){
         warnPrint("Options -num and -s are mutually exclusive.\n");
     }
     if (arguments.plain || arguments.tool == cainfo) {
         if (arguments.tool != cainfo) warnPrint("-plain option overrides all formatting switches.\n");
         arguments.num =false; arguments.hex =false; arguments.bin = false; arguments.oct =false; arguments.str =false;
         arguments.prec = -1;   // prec is also handled in printValue()
         arguments.round = roundType_no_rounding;
         arguments.dblFormatType = '\0';
         arguments.fieldSeparator = ' ' ;
         arguments.noname = true;
         arguments.nostat = true;
         arguments.stat = false;
         arguments.nounit = true;
     }


    //Remaining arg list refers to PVs
    if (arguments.tool == caget || arguments.tool == camon || arguments.tool == cagets || arguments.tool == cado || arguments.tool == cainfo){
        nChannels = argc - optind;       // All remaining args are PV names
    }
    else {
        if ((argc - optind) % 2) {
            if (arguments.tool == caput || arguments.tool == caputq) fprintf(stderr, "One of the PVs is missing the value to be written ('%s -h' for help).\n", argv[0]);
            if (arguments.tool == cawait)                            fprintf(stderr, "One of the PVs is missing the condition ('%s -h' for help).\n", argv[0]);
            return EXIT_FAILURE;
        }
        nChannels = (argc - optind) / 2;	// half of the args are PV names, rest conditions/values
    }


    if (nChannels < 1)
    {
        fprintf(stderr, "No record name specified. ('%s -h' for help.)\n", argv[0]);
        return EXIT_FAILURE;
    }

    //allocate memory for channel structures
    channels = (struct channel *) callocMustSucceed (nChannels, sizeof(struct channel), "main");
    for (i=0; i < nChannels; ++i) {
        channels[i].base.connectionState = CA_OP_OTHER;
        channels[i].base.created = false;
        channels[i].base.done = false;

        if (arguments.tool == cagets) {
            channels[i].proc.name = callocMustSucceed (LEN_RECORD_NAME + LEN_RECORD_FIELD + 2, sizeof(char), "main");//2 spaces for .(field name) + null termination
            channels[i].proc.created = false;
        }
        if(arguments.tool == cainfo) {
            size_t nFields = noFields;  // so we don't calculate in each loop
            for (j=0; j < nFields; j++) {
                channels[i].fields[j].name = callocMustSucceed(LEN_RECORD_NAME + LEN_RECORD_FIELD + 2, sizeof(char), fields[j]);
                channels[i].fields[j].connectionState = CA_OP_OTHER;
                channels[i].fields[j].created = false;
                channels[i].fields[j].done = false;
            }
        }
    }


    bool success = true;
    // Copy PV names from command line
    for (i = 0; optind < argc; i++, optind++) {
        channels[i].base.name = argv[optind];

        if(strlen(channels[i].base.name) > LEN_RECORD_NAME + LEN_RECORD_FIELD + 1) { //worst case scenario: longest name + longest field + '.' that separates name and field
            errPrint("Record name over %d characters: %s - aborting", LEN_RECORD_NAME + LEN_RECORD_FIELD + 1, channels[i].base.name);
            success = false;
            break;
        }

        channels[i].i = i;	// channel number, serves to synchronise pvs and output.

        if (arguments.tool == caput || arguments.tool == caputq){
            channels[i].inNelm = 1;

            if (!arguments.parseArray) {  //the argument to write consists of just a single element.
                truncate(argv[optind+1]);

                // allocate resources and set pointer to the argument
                channels[i].writeStr = callocMustSucceed (channels[i].inNelm, sizeof(char*), "main");
                channels[i].writeStr[0] = argv[optind+1];
            }
            else{//parse the string assuming each element is delimited by the inputSeparator char
                char inSep[2] = {arguments.inputSeparator, 0};

                // first count the number of arguments.
                char* tempstr = callocMustSucceed(strlen(argv[optind+1])+1, sizeof(char), "main");
                strcpy(tempstr, argv[optind+1]);
                j=0;
                char *token = strtok(tempstr, inSep);
                while(token) {
                    j++;
                    token = strtok(NULL, inSep);
                }
                free(tempstr);
                channels[i].inNelm = j;

                // allocate resources
                channels[i].writeStr = callocMustSucceed (channels[i].inNelm, sizeof(char*), "main");

                // extract arguments
                j=0;
                token = strtok(argv[optind+1], inSep);
                while(token) {
                    truncate(token);
                    channels[i].writeStr[j] = token;
                    j++;
                    token = strtok(NULL, inSep);
                }
            }
            //finally advance to the next argument
            optind++;

        }
        else if (arguments.tool == cawait) {
            //next argument is the condition string
            if (!cawaitParseCondition(&channels[i], argv[optind+1])) {
                success = false;
                break;
            }
            optind++;
        }
        else if (arguments.tool == cagets) {

            strcpy(channels[i].proc.name, channels[i].base.name);

            //append .PROC
            getBaseChannelName(channels[i].proc.name);
            strcat(channels[i].proc.name, ".PROC");
        }
        else if (arguments.tool == cainfo) {
            //set sibling channels for getting desc and severity data
            getBaseChannelName(channels[i].base.name);

            size_t nFields = noFields;  // so we don't calculate in each loop
            for (j=0; j < nFields; j++) {
                strcpy(channels[i].fields[j].name, channels[i].base.name);
                strcat(channels[i].fields[j].name, fields[j]);
            }
        }
    }


    //allocate memory for output strings
    errorTimestamp = callocMustSucceed(LEN_TIMESTAMP, sizeof(char),"errorTimestamp");
    outDate = callocMustSucceed(nChannels, sizeof(char *),"main");
    outTime = callocMustSucceed(nChannels, sizeof(char *),"main");
    outSev = callocMustSucceed(nChannels, sizeof(char *),"main");
    outStat = callocMustSucceed(nChannels, sizeof(char *),"main");
    outUnits = callocMustSucceed(nChannels, sizeof(char *),"main");
    outTimestamp = callocMustSucceed(nChannels, sizeof(char *),"main");
    outLocalDate = callocMustSucceed(nChannels, sizeof(char *),"main");
    outLocalTime = callocMustSucceed(nChannels, sizeof(char *),"main");
    for(i = 0; i < nChannels; i++){
        outDate[i] = callocMustSucceed(LEN_TIMESTAMP, sizeof(char),"main");
        outTime[i] = callocMustSucceed(LEN_TIMESTAMP, sizeof(char),"main");
        outSev[i] = callocMustSucceed(LEN_SEVSTAT, sizeof(char),"main");
        outStat[i] = callocMustSucceed(LEN_SEVSTAT, sizeof(char),"main");
        outUnits[i] = callocMustSucceed(LEN_UNITS, sizeof(char),"main");
        outTimestamp[i] = callocMustSucceed(LEN_TIMESTAMP, sizeof(char),"main");
        outLocalDate[i] = callocMustSucceed(LEN_TIMESTAMP, sizeof(char),"main");
        outLocalTime[i] = callocMustSucceed(LEN_TIMESTAMP, sizeof(char),"main");
    }
    //memory for timestamp
    timestampRead = mallocMustSucceed(nChannels * sizeof(epicsTimeStamp),"main");
    if (arguments.tool == camon || arguments.tool == cawait){
        lastUpdate = mallocMustSucceed(nChannels * sizeof(epicsTimeStamp),"main");
        firstUpdate = callocMustSucceed(nChannels, sizeof(bool),"main");
        numMonitorUpdates = 0;
    }

    //start channel access
    if(success) success = caInit(channels, nChannels);

    if (success && arguments.tool == cainfo) {
        success = cainfoRequest(channels, nChannels);
    }
    else if(success && (arguments.tool == camon || arguments.tool == cawait)) {
        monitorLoop();
    }
    else {
        if (success) success = caRequest(channels, nChannels);
    }

    success &= caDisconnect(channels, nChannels);

    ca_context_destroy();

    //free channels
    for (i=0 ;i < nChannels; ++i) {
        free(channels[i].writeStr);
        if (arguments.tool == cagets) free(channels[i].proc.name);

        if (arguments.tool == cainfo) {
            size_t nFields = noFields;  // so we don't calculate in each loop
            for (j=0; j < nFields; j++) {
                free(channels[i].fields[j].name);
            }
        }
    }
    free(channels);

    //free output strings
    for(i = 0; i < nChannels; i++) {
        free(outDate[i]);
        free(outTime[i]);
        free(outSev[i]);
        free(outStat[i]);
        free(outUnits[i]);
        free(outTimestamp[i]);
        free(outLocalDate[i]);
        free(outLocalTime[i]);
    }
    free(outDate);
    free(outTime);
    free(outSev);
    free(outStat);
    free(outUnits);
    free(outTimestamp);
    free(outLocalDate);
    free(outLocalTime);

    //free monitor's timestamp
    free(timestampRead);
    free(lastUpdate);
    free(firstUpdate);

    free(errorTimestamp);

    if (success) return EXIT_SUCCESS;
    else return EXIT_FAILURE;
}
