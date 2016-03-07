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

struct arguments
{
   float caTimeout;         //ca timeout
   int  dbrRequestType;     //dbr request type
   bool num;                //same as -int
   enum roundType round;	//type of rounding: round(), ceil(), floor()
   int prec;                //precision
   bool hex;				//display as hex
   bool bin;				//display as bin
   bool oct;				//display as oct
   bool plain;				//ignore formatting switches
   char dblFormatType;     	//format type for decimal numbers (see -e -f -g option)
   bool s;					//try to interpret values as strings
   bool stat;				//status and severity on
   bool nostat;				//status and severity off
   bool noname;				//hide name
   bool nounit;				//hide units
   char timestamp;			//timestamp type ruc
   float timeout;			//cawait timeout
   bool date;				//server date
   bool localdate;			//client date
   bool time;				//server time
   bool localtime;			//client time
   char fieldSeparator;		//array field separator for output
   char inputSeparator;		//array field separator for input
   enum tool tool;			//tool type
   int numUpdates;			//number of monitor updates after which to quit
   int inNelm;				//number of array elements to write
   int outNelm;				//number of array elements to read
   bool nord;				//display number of array elements
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
        .s = false,
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
        .inNelm = 1,
        .outNelm = -1,	//number of elements equal to field count
        .nord = false
};

enum operator{	//possible conditions for cawait
    operator_gt = 1,
    operator_gte,
    operator_lt,
    operator_lte,
    operator_eq,
    operator_neq,
    operator_in,
    operator_out
};


struct channel{
    char            *name;  	// the name of the channel
    chid            id;     	// channel id
    char			*procName;	// sibling channel for writing to proc field
    chid			procId;		// sibling channel for writing to proc field
    char			*descName;	// sibling channel for reading desc field
    chid			descId;		// sibling channel for reading desc field
    char			*hhsvName;	// sibling channel for reading hhsv field
    chid			hhsvId;		// sibling channel for reading hhsv field
    char			*hsvName;	// sibling channel for reading hsv field
    chid			hsvId;		// sibling channel for reading hsv field
    char			*lsvName;	// sibling channel for reading lsv field
    chid			lsvId;		// sibling channel for reading lsv field
    char			*llsvName;	// sibling channel for reading llsv field
    chid			llsvId;		// sibling channel for reading llsv field
    char 			*unsvName;  // sibling channel for reading unsv field
    chid 			unsvId; 	// sibling channel for reading unsv field
    char 			*cosvName;  // ..
    chid 			cosvId;		// ..
    char 			*zrsvName;	// ..
    chid 			zrsvId;		// ..
    char 			*onsvName;	// ..
    chid 			onsvId;		// ..
    char 			*twsvName;	// ..
    chid 			twsvId;		// ..
    char 			*thsvName;	// ..
    chid 			thsvId;		// ..
    char 			*frsvName;	// ..
    chid 			frsvId;		// ..
    char 			*fvsvName;	// ..
    chid 			fvsvId;		// ..
    char 			*sxsvName;	// ..
    chid 			sxsvId;		// ..
    char 			*svsvName;	// ..
    chid 			svsvId;		// ..
    char 			*eisvName;	// ..
    chid 			eisvId;		// ..
    char 			*nisvName;	// ..
    chid 			nisvId;		// ..
    char 			*tesvName;	// ..
    chid 			tesvId;		// ..
    char 			*elsvName;	// ..
    chid 			elsvId;		// ..
    char 			*tvsvName;	// ..
    chid 			tvsvId;		// ..
    char 			*ttsvName;	// ..
    chid 			ttsvId;		// ..
    char 			*ftsvName;	// ..
    chid 			ftsvId;		// ..
    char 			*ffsvName;	// ..
    chid 			ffsvId;		// ..
    long            type;   	// dbr type
    long            count;  	// element count
    long            inNelm;  	// requested number of elements for writing
    long            outNelm;  	// requested number of elements for reading
    bool			firstConnection; // indicates if channel has connected at least once
    bool            done;   	// indicates if callback has finished processing this channel
    int 			i;			// process value id
    char			*writeStr;	// value(s) to be written
    enum operator	conditionOperator; //cawait operator
    double 			conditionOperands[2]; //cawait operands
};


//output strings
static int const LEN_TIMESTAMP = 50;
static int const LEN_RECORD_NAME = 61;
static int const LEN_SEVSTAT = 30;
static int const LEN_UNITS = 20+MAX_UNITS_SIZE;
char **outDate,**outTime, **outSev, **outStat, **outUnits, **outLocalDate, **outLocalTime;
char **outTimestamp; //relative timestamps for camon


//timestamps needed by -timestamp
epicsTimeStamp *timestampRead;		//timestamps of received data (per channel)
epicsTimeStamp programStartTime;	//timestamp indicating program start
epicsTimeStamp *lastUpdate;			//timestamp indicating last update per channel
bool *firstUpdate;					//indicates that lastUpdate has not been initialized
epicsTimeStamp timeoutTime;			//when to stop monitoring (-timeout)

bool runMonitor;					//indicates when to stop monitoring according to -timeout, -n or cawait condition is true
uint32_t numMonitorUpdates;		//counts updates needed by -n

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
        fprintf(stream, "\nUsage: %s <pv> [<pv> ...]\n", programName);
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
        fputs("Arrays can be handled either by specifying number of elements as an argument to -inNelm option"\
                " or grouping the array elements in a single string after PV name. See following examples which produce the same"\
                " result, namely write 1, 2 and 3 into record pvA and 4, 5, 6 into pvB:\n", stream);
        fputs("  caput -inNelm 3 pvA 1 2 3 pvB 4 5 6\n", stream);
        fputs("  caput pvA '1 2 3' pvB '4 5 6'\n", stream);
        fputs("  caput -inSep ; pvA '1;2;3' pvB '4;5;6'\n", stream);
    }
    if (tool == cado) {
        fputs("Writes 1 to PV(s), but does not wait for the processing to finish. Does not have any output (except if an error occurs).\n", stream);
    }
    if (tool == camon) {
        fputs("Monitors the PV(s).\n", stream);
    }
    if (tool == cawait) {
         fputs("Monitors the PV(s), but only displays the values when they match the provided conditions. The conditions are specified as a"\
                " string containing the operator together with the values.\n", stream);
        fputs("Following operators are supported:  >,<,<=,>=,==,!=, ==A...B(in interval), !=A...B(out of interval). For example, "\
                "cawait pv '==1...5' ignores all pv values except those inside the interval [1,5].\n", stream);
    }
    if (tool == cainfo) {
        fputs("Displays detailed information about the provided records.\n", stream);
    }

    //flags common for all tools
    fputs("\n", stream);
    fputs("Accepted flags:\n", stream);
    fputs("\n", stream);
    fputs("  -h                   Help: Print this message\n", stream);

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
        fputs("  -inNelm <number>     Number of array elements to write. Must match the number of provided values.\n", stream);
        fputs("  -inSep <separator>   Separator used between array elements in <value>. If not specified, space is used.\n", stream);
    }

    // flags associated with monitoring
    if (tool == camon) {
        fputs("Monitoring options\n", stream);
        fputs("  -n <number>          Exit the program after <number> updates.\n", stream);
        fputs("  -timestamp <otpion>  Display relative timestamps. Options:\n", stream);
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

        fputs("Formating output : Intiger format options\n", stream);
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

static inline void clearStr(char *str){
//clears input string str
    str[0] = '\0';
}


static bool isStrEmpty(char *str){
//returns true is the input string is empty
    return str[0] == '\0';
}



bool cawaitParseCondition(struct channel *channel, char *argumentConditionStr){
//parses input string and extracts operators and numerals,
//then saves them to the channel structure.
//Supported operators: >,<,<=,>=,==,!=, ==A...B(in interval), !=A...B(out of interval).

    size_t i=0, j=0;				//counters
    bool interval=false;	//indicates if interval is desired

    //create a local copy of condition string for parsing
    char *conditionStr = callocMustSucceed(strlen(argumentConditionStr), sizeof(char), "cawaitParseCondition\n");

    //strip spaces, and replace '...' with ':'
    while(argumentConditionStr[i] != '\0') {
        if (argumentConditionStr[i] != ' '){
            if (argumentConditionStr[i] == '.' && argumentConditionStr[i+1] == '.' && argumentConditionStr[i+2] == '.'){
                conditionStr[j++] = ':';
                i += 2;
                interval=true;
                continue;
            }
            conditionStr[j++] = argumentConditionStr[i];
        }

        i++;
    }
    for (i=0, j=0; i<strlen(argumentConditionStr); ++i){

    }
    if (conditionStr[0] == '\0'){
        fprintf(stderr,"Empty condition string.\n");
        return false;
    }

    //Parse the processed conditions string. The first character is always an operator. The
    //second character is part of operator if it is not numerical. The operator is followed by
    //a double operand. Optionally, this is followed by ':' and another double operand.
    char *format;
    if (conditionStr[1]>='0' && conditionStr[1]<='9'){
        format = "%c%lf:%lf";
    }
    else{
        format = "%2c%lf:%lf";
    }

    char operator[3] = {0,0,0};
    double operand1, operand2;
    int nParsed;
    nParsed = sscanf(conditionStr, format, operator, &operand1, &operand2);

    //finally save the extracted operator and operands into the channel struct.
    if (nParsed == 2 && interval == false){

        channel->conditionOperands[0] = operand1;

        if (strcmp(operator, ">")==0)       channel->conditionOperator = operator_gt;
        else if (strcmp(operator, "<")==0)  channel->conditionOperator = operator_lt;
        else if (strcmp(operator, ">=")==0) channel->conditionOperator = operator_gte;
        else if (strcmp(operator, "<=")==0) channel->conditionOperator = operator_lte;
        else if (strcmp(operator, "==")==0)	channel->conditionOperator = operator_eq;
        else if (strcmp(operator, "!=")==0) channel->conditionOperator = operator_neq;
        else{
            fprintf(stderr, "Problem interpreting condition string: %s. Unknown operator %s.", conditionStr, operator);
            return false;
        }
    }
    else if (nParsed == 3 && interval == true){

        channel->conditionOperands[0] = operand1;
        channel->conditionOperands[1] = operand2;

        if (strcmp(operator, "==")==0) 		channel->conditionOperator = operator_in;
        else if (strcmp(operator, "!=")==0) channel->conditionOperator = operator_out;
        else{
            fprintf(stderr, "Problem interpreting condition string: %s. Only operators == and != are supported if the"\
                    "value is to be compared to an interval.",conditionStr);
            return false;
        }
    }
    else{
        fprintf(stderr, "Problem interpreting condition string: %s. Wrong number of operands.", conditionStr);
        return false;
    }
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
    unsigned int baseType = args.type % (LAST_TYPE+1);
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
        if (sscanf(nativeValue, "%lf", &dblValue) <= 0){    // TODO: nativeValue might not be null terminated
            fprintf(stderr, "Record %s value %s cannot be converted to double.\n", channel.name, (char*)nativeValue);
            return -1;
        }
        break;
    case DBR_ENUM:
        dblValue = (double) *(dbr_enum_t*)nativeValue;
        break;
    default:
        fprintf(stderr, "Unrecognized DBR type.\n");
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

int printValue(evargs args, int precision){
//Parses the data fetched by ca_get callback according to the data type and formatting arguments.
//The result is printed to stdout.

    unsigned int baseType;
    void *value;
    double valueDbl = 0;

    value = dbr_value_ptr(args.dbr, args.type);
    baseType = args.type % (LAST_TYPE+1);   // convert appropriate TIME, GR, CTRL,... type to basic one

    //loop over the whole array

    int j; //element counter

    for (j=0; j<args.count; ++j){

        if (j){
            printf ("%c", arguments.fieldSeparator); //insert element separator
        }

        switch (baseType) {
        case DBR_STRING:
            printf("%.*s", MAX_STRING_SIZE, ((dbr_string_t*) value)[j]);
            break;
        case DBR_FLOAT:
        case DBR_DOUBLE:{
            if (baseType == DBR_FLOAT) valueDbl = ((dbr_float_t*)value)[j];
            else valueDbl = ((dbr_double_t*)value)[j];

            //round if desired or will be writen as hex oct or bin
            if (arguments.round == roundType_round || arguments.hex || arguments.oct || arguments.bin) {
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
                    sprintf(formatStr, "%%-.%d%c", precision, format);
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
            else if (arguments.s){
                printf("%c", (uint8_t)valueInt32);
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
                printf("0x%" PRIx16, (uint16_t)valueInt16);
            }
            else if (arguments.oct){
                printf("0o%" PRIo16, (uint16_t)valueInt16);
            }
            else if (arguments.bin){
                printf("0b");
                printBits((uint16_t)valueInt16);
            }
            else if (arguments.s){
                printf("%c", (uint8_t)valueInt16);
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
                        printf("%.*s", MAX_ENUM_STRING_SIZE, ((struct dbr_gr_enum *)args.dbr)->strs[v]);
                    }
                    break;
                }
                else if (dbr_type_is_CTRL(args.type)) {
                    if (v >= ((struct dbr_ctrl_enum *)args.dbr)->no_str) {
                        printf("Enum index value %d greater than the number of strings", v);
                    }
                    else{
                        printf("%.*s", MAX_ENUM_STRING_SIZE, ((struct dbr_ctrl_enum *)args.dbr)->strs[v]);
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
                printf("%" PRIu8, ((uint8_t*) value)[j]);
            }
            else if (arguments.hex){
                printf("0x%" PRIx8, ((uint8_t*) value)[j]);
            }
            else if (arguments.oct){
                printf("0o%" PRIo8, ((uint8_t*) value)[j]);
            }
            else if (arguments.bin){
                printf("0b");
                printBits(((uint8_t*) value)[j]);
            }
            else{
                fputc(((char*) value)[j], stdout);
            }
            break;
        }

        default:
            fprintf(stderr, "Unrecognized DBR type.\n");
            break;
        }

    }

    return 0;
}


int printOutput(int i, evargs args, int precision){
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
    if (!arguments.noname)	printf("%s ",((struct channel *)args.usr)->name);

    if (arguments.nord)	printf("%lu ", args.count); // show nord if requested

    //value(s)
    printValue(args, precision);


    fputc(' ',stdout);

    //egu
    if (!isStrEmpty(outUnits[i])) printf("%s ",outUnits[i]);

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



void validateTimestamp(epicsTimeStamp *timestamp, const char* name){
//checks a timestamp for illegal values.
    if (timestamp->nsec >= 1000000000ul){
        fprintf(stderr,"Warning: invalid number of nanoseconds in timestamp: %s - assuming 0.\n",name);
        timestamp->nsec=0;
    }
}

int getTimeStamp(int i) {
//calculates timestamp for monitor tool, formats it and saves it into the global string.

    epicsTimeStamp elapsed;
    bool negative=false;
    int commonI = (arguments.timestamp == 'c') ? i : 0;
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
    validateTimestamp(&timestampRead[ch->i], ch->name);
#define units_get_cb(T) sprintf(outUnits[ch->i], "%s", ((struct T *)args.dbr)->units);
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
        fprintf(stderr, "Error in read callback. %s.\n", ca_message(args.status));
        return;
    }
    struct channel *ch = (struct channel *)args.usr;

    int precision=-1;
    uint32_t status=0, severity=0;

    //clear global output strings; the purpose of this callback is to overwrite them
    //the exception are units, which we may be getting from elsewhere; we only clear them if we can write them
    clearStr(outDate[ch->i]);
    clearStr(outTime[ch->i]);
    clearStr(outSev[ch->i]);
    clearStr(outStat[ch->i]);
    clearStr(outLocalDate[ch->i]);
    clearStr(outLocalTime[ch->i]);
    if (args.type >= DBR_GR_STRING){
        clearStr(outUnits[ch->i]);
    }

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
            printf("Can not print %s DBR type (numeric type code: %ld). \n", dbr_type_to_text(args.type), args.type);
            break;
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
        //time_t localTime = time(0);
        epicsTimeStamp localTime;
        epicsTimeGetCurrent(&localTime);
        validateTimestamp(&localTime, "localTime");

        if (arguments.localdate) epicsTimeToStrftime(outLocalDate[ch->i], LEN_TIMESTAMP, "%Y-%m-%d", &localTime);
        if (arguments.localtime) epicsTimeToStrftime(outLocalTime[ch->i], LEN_TIMESTAMP, "%H:%M:%S.%06f", &localTime);
    }

    //hide units?
    if (arguments.nounit) clearStr(outUnits[ch->i]);


    //if monitor, there is extra stuff to do before printing data out.
    bool shouldPrint = true;
    if (arguments.tool == cawait || arguments.tool == camon) {
        if (arguments.timestamp) getTimeStamp(ch->i);	//calculate relative timestamps.

        if (arguments.numUpdates != -1) {
            numMonitorUpdates++;	//increase counter of updates

            if (numMonitorUpdates > arguments.numUpdates) { // when channel subscription is made an update fires. This one does not count towards numMonitorUpdates.
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
    ch->done = true;

}

static void caWriteCallback (evargs args) {
//does nothing except signal that writing is finished.

    //check if status is ok
    if (args.status != ECA_NORMAL){
        fprintf(stderr, "Error in write callback. %s.\n", ca_message(args.status));
        return;
    }

    struct channel *ch = (struct channel *)args.usr;

    ch->done = true;
}

static void getStaticUnitsCallback (evargs args) {
//reads channel units and saves them to global strings. Is called every time a channel
//connects (if needed).

    //check if data was returned
    if (args.status != ECA_NORMAL){
        fprintf(stderr, "Error in initial read callback. %s.\n", ca_message(args.status));
        return;
    }
    struct channel *ch = (struct channel *)args.usr;

    //get units
    switch(args.type){
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
        fprintf(stderr, "Units for record %s cannot be displayed.\n", ch->name);
        break;
    }
}

void monitorLoop (struct channel *channels, int nChannels){
//runs monitor loop. Stops after -n updates (camon, cawait) or
//after -timeout is exceeded (cawait).

    while (runMonitor){
        ca_pend_event(0.1);
    }
}

// Wait for request completition, or for caTimeout to occur
// bool firstConnection: include chanel[].firstConnection when checking for callback completition
void waitForCompletition(struct channel *channels, int nChannels, bool firstConnection) {
    int i;
    bool elapsed = false, allDone = false;
    epicsTimeStamp timeoutNow, timeout;

    epicsTimeGetCurrent(&timeout);
    epicsTimeAddSeconds(&timeout, arguments.caTimeout);

    while(!allDone && !elapsed) {
        ca_pend_event(0.1);
        // check for timeout
        epicsTimeGetCurrent(&timeoutNow);
        if (epicsTimeGreaterThanEqual(&timeoutNow, &timeout)) {
            printf("Timeout while waiting for PV results (more than %f seconds elapsed).\n", arguments.caTimeout);
            elapsed = true;
        }

        // check for callback completition
        allDone=true;
        for (i=0; i<nChannels; ++i) allDone &= (channels[i].done || channels[i].firstConnection == firstConnection);
    }
}

void caRequest(struct channel *channels, int nChannels) {
//sends get or put requests. ca_get or ca_put are called multiple times, depending on the tool. The reading,
//parsing and printing of returned data is performed in callbacks.
    int status=-1, i, j;


    for(i=0; i < nChannels; i++) {

        if (!channels[i].firstConnection) {//skip disconnected channels
            channels[i].done = true;
            continue;
        }

        if (arguments.tool == caget) {
            status = ca_array_get_callback(channels[i].type, channels[i].outNelm, channels[i].id, caReadCallback, &channels[i]);
        }
        else if (arguments.tool == caput || arguments.tool == caputq){
            long nakedType = channels[i].type % (LAST_TYPE+1);   // use naked dbr_xx type for put

            //convert input string to the nakedType
            status = ECA_BADTYPE;
            switch (nakedType){
            case DBR_STRING:
                status = ca_array_put_callback(nakedType, channels[i].inNelm, channels[i].id, channels[i].writeStr, caWriteCallback, &channels[i]);
                break;
            case DBR_INT:{//and short
                int16_t inputI[channels[i].inNelm];
                for (j=0;j<channels[i].inNelm; ++j){
                    if (sscanf(&channels[i].writeStr[j*MAX_STRING_SIZE],"%"SCNd16,&inputI[j]) <= 0) {
                        fprintf(stderr, "Impossible to convert input %s to format %s\n",&channels[i].writeStr[j*MAX_STRING_SIZE], dbr_type_to_text(nakedType));
                    }
                }
                status = ca_array_put_callback(nakedType, channels[i].inNelm, channels[i].id, inputI, caWriteCallback, &channels[i]);
                }
                break;
            case DBR_FLOAT:{
                float inputF[channels[i].inNelm];
                for (j=0;j<channels[i].inNelm; ++j){
                    if (sscanf(&channels[i].writeStr[j*MAX_STRING_SIZE],"%f",&inputF[j]) <= 0) {
                        fprintf(stderr, "Impossible to convert input %s to format %s\n",&channels[i].writeStr[j*MAX_STRING_SIZE], dbr_type_to_text(nakedType));
                    }
                }
                status = ca_array_put_callback(nakedType, channels[i].inNelm, channels[i].id, inputF, caWriteCallback, &channels[i]);
                }
                break;
            case DBR_ENUM:{
                //check if put data is provided as a number
                uint16_t inputI[channels[i].inNelm];
                if (sscanf(&channels[i].writeStr[0],"%"SCNu16,&inputI[0])){
                    for (j=0;j<channels[i].inNelm; ++j){
                        if (sscanf(&channels[i].writeStr[j*MAX_STRING_SIZE],"%"SCNu16,&inputI[j]) <= 0){
                            fprintf(stderr, "String '%s' cannot be converted to enum index.\n", &channels[i].writeStr[j*MAX_STRING_SIZE]);
                            break;
                        }
                        if (inputI[j]>=MAX_ENUM_STATES) fprintf(stderr, "Warning: enum index value '%d' may be too large.\n", inputI[j]);
                    }
                    status = ca_array_put_callback(nakedType, channels[i].inNelm, channels[i].id, inputI, caWriteCallback, &channels[i]);
                }
                else{
                    //send as a string
                    status = ca_array_put_callback(DBR_STRING, channels[i].inNelm, channels[i].id, channels[i].writeStr, caWriteCallback, &channels[i]);
                }
                }
                break;
            case DBR_CHAR:{
                char inputC[channels[i].inNelm];
                for (j=0;j<channels[i].inNelm; ++j){
                    if (sscanf(&channels[i].writeStr[j],"%c",&inputC[j]) <= 0) {
                        fprintf(stderr, "Impossible to convert input %s to format %s\n",channels[i].writeStr, dbr_type_to_text(nakedType));
                    }
                }
                status = ca_array_put_callback(nakedType, channels[i].inNelm, channels[i].id, inputC, caWriteCallback, &channels[i]);
                }
                break;
            case DBR_LONG:{
                int32_t inputL[channels[i].inNelm];
                for (j=0;j<channels[i].inNelm; ++j){
                    if (sscanf(&channels[i].writeStr[j*MAX_STRING_SIZE],"%"SCNd32,&inputL[j]) <= 0) {
                        fprintf(stderr, "Impossible to convert input %s to format %s\n",&channels[i].writeStr[j*MAX_STRING_SIZE], dbr_type_to_text(nakedType));
                    }
                }
                status = ca_array_put_callback(nakedType, channels[i].inNelm, channels[i].id, inputL, caWriteCallback, &channels[i]);
                }
                break;
            case DBR_DOUBLE:{
                double inputD[channels[i].inNelm];
                for (j=0;j<channels[i].inNelm; ++j){
                    if (sscanf(&channels[i].writeStr[j*MAX_STRING_SIZE],"%lf",&inputD[j]) <= 0) {
                        fprintf(stderr, "Impossible to convert input %s to format %s\n",&channels[i].writeStr[j*MAX_STRING_SIZE], dbr_type_to_text(nakedType));
                    }
                }
                status = ca_array_put_callback(nakedType, channels[i].inNelm, channels[i].id, inputD, caWriteCallback, &channels[i]);
                }
                break;
            }
        }
        else if(arguments.tool == cagets || arguments.tool == cado){
            int inputI=1;
            status = ca_array_put_callback(DBF_CHAR, 1, channels[i].procId, &inputI, caWriteCallback, &channels[i]);
        }
        if (status != ECA_NORMAL) fprintf(stderr, "Problem creating request for process variable %s: %s.\n", channels[i].name,ca_message(status));
    }

    // wait for callbacks
    waitForCompletition(channels, nChannels, false);


    //if caput or cagets issue a new read request.
    if (arguments.tool == caput || arguments.tool == cagets){

        for (i=0; i<nChannels; ++i){
            if (!channels[i].firstConnection) {//skip disconnected channels
                channels[i].done = true;
                continue;
            }
            if (channels[i].done){
                status = ca_array_get_callback(channels[i].type, channels[i].outNelm, channels[i].id, caReadCallback, &channels[i]);
                if (status != ECA_NORMAL) fprintf(stderr, "Problem creating get request for channel %s: %s.\n", channels[i].name,ca_message(status));
            }
            else{
                fprintf(stderr, "Put callback for channel %s did not complete.\n", channels[i].name);
            }
        }

        waitForCompletition(channels, nChannels, false);

    }

}

void cainfoRequest(struct channel *channels, int nChannels){
//this function does all the work for caInfo tool. Reads channel data using ca_get and then prints.
    int i,j, status;

    bool readAccess, writeAccess;

    for(i=0; i < nChannels; i++){

        if (!channels[i].firstConnection) continue; //channel doesn't exist

        channels[i].count = ca_element_count ( channels[i].id );

        channels[i].type = dbf_type_to_DBR_CTRL(ca_field_type(channels[i].id));

        //allocate data for all caget returns
        void *data, *dataDesc, *dataHhsv, *dataHsv, *dataLsv, *dataLlsv, *dataUnsv, *dataCosv, *dataZrsv, *dataOnsv, *dataTwsv, *dataThsv;
        void *dataFrsv, *dataFvsv, *dataSxsv, *dataSvsv, *dataEisv,*dataNisv, *dataTesv,*dataElsv,*dataTvsv,*dataTtsv,*dataFtsv,*dataFfsv;
        data = callocMustSucceed(1, dbr_size_n(channels[i].type, channels[i].count), "caInfoRequest");
        dataDesc = callocMustSucceed(1, dbr_size_n(DBR_STS_STRING, 1), "caInfoRequest");
        dataHhsv = callocMustSucceed(1, dbr_size_n(DBR_STS_STRING, 1), "caInfoRequest");
        dataHsv = callocMustSucceed(1, dbr_size_n(DBR_STS_STRING, 1), "caInfoRequest");
        dataLsv = callocMustSucceed(1, dbr_size_n(DBR_STS_STRING, 1), "caInfoRequest");
        dataLlsv = callocMustSucceed(1, dbr_size_n(DBR_STS_STRING, 1), "caInfoRequest");
        dataUnsv = callocMustSucceed(1, dbr_size_n(DBR_STS_STRING, 1), "caInfoRequest");
        dataCosv = callocMustSucceed(1, dbr_size_n(DBR_STS_STRING, 1), "caInfoRequest");
        dataZrsv = callocMustSucceed(1, dbr_size_n(DBR_STS_STRING, 1), "caInfoRequest");
        dataOnsv = callocMustSucceed(1, dbr_size_n(DBR_STS_STRING, 1), "caInfoRequest");
        dataTwsv = callocMustSucceed(1, dbr_size_n(DBR_STS_STRING, 1), "caInfoRequest");
        dataThsv = callocMustSucceed(1, dbr_size_n(DBR_STS_STRING, 1), "caInfoRequest");
        dataFrsv = callocMustSucceed(1, dbr_size_n(DBR_STS_STRING, 1), "caInfoRequest");
        dataFvsv = callocMustSucceed(1, dbr_size_n(DBR_STS_STRING, 1), "caInfoRequest");
        dataSxsv = callocMustSucceed(1, dbr_size_n(DBR_STS_STRING, 1), "caInfoRequest");
        dataSvsv = callocMustSucceed(1, dbr_size_n(DBR_STS_STRING, 1), "caInfoRequest");
        dataEisv = callocMustSucceed(1, dbr_size_n(DBR_STS_STRING, 1), "caInfoRequest");
        dataNisv = callocMustSucceed(1, dbr_size_n(DBR_STS_STRING, 1), "caInfoRequest");
        dataTesv = callocMustSucceed(1, dbr_size_n(DBR_STS_STRING, 1), "caInfoRequest");
        dataElsv = callocMustSucceed(1, dbr_size_n(DBR_STS_STRING, 1), "caInfoRequest");
        dataTvsv = callocMustSucceed(1, dbr_size_n(DBR_STS_STRING, 1), "caInfoRequest");
        dataTtsv = callocMustSucceed(1, dbr_size_n(DBR_STS_STRING, 1), "caInfoRequest");
        dataFtsv = callocMustSucceed(1, dbr_size_n(DBR_STS_STRING, 1), "caInfoRequest");
        dataFfsv = callocMustSucceed(1, dbr_size_n(DBR_STS_STRING, 1), "caInfoRequest");


        //general ctrl data
        status = ca_array_get(channels[i].type, channels[i].count, channels[i].id, data);
        if (status != ECA_NORMAL) fprintf(stderr, "CA error %s occurred while trying to create ca_get request for record %s.\n", ca_message(status), channels[i].name);

        //desc
        status = ca_array_get(DBR_STS_STRING, 1, channels[i].descId, dataDesc);
        if (status != ECA_NORMAL) fprintf(stderr, "Problem reading DESC field of record %s: %s.\n", channels[i].name, ca_message(status));

        //standard severity fields
        if (!isStrEmpty(channels[i].hhsvName) && channels[i].hhsvId){
            status = ca_array_get(DBR_STS_STRING, 1, channels[i].hhsvId, dataHhsv);
            if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
        }
        if (!isStrEmpty(channels[i].hsvName) && channels[i].hsvId){
            status = ca_array_get(DBR_STS_STRING, 1, channels[i].hsvId, dataHsv);
            if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
        }
        if (!isStrEmpty(channels[i].lsvName) && channels[i].lsvId){
            status = ca_array_get(DBR_STS_STRING, 1, channels[i].lsvId, dataLsv);
            if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
        }
        if (!isStrEmpty(channels[i].llsvName) && channels[i].llsvId){
            status = ca_array_get(DBR_STS_STRING, 1, channels[i].llsvId, dataLlsv);
            if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
        }
        //multi-bit severity fields
        if (!isStrEmpty(channels[i].unsvName) && channels[i].unsvId){
            status = ca_array_get(DBR_STS_STRING, 1, channels[i].unsvId, dataUnsv);
            if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
        }
        if (!isStrEmpty(channels[i].cosvName) && channels[i].cosvId){
            status = ca_array_get(DBR_STS_STRING, 1, channels[i].cosvId, dataCosv);
            if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
        }
        if (!isStrEmpty(channels[i].zrsvName) && channels[i].zrsvId){
            status = ca_array_get(DBR_STS_STRING, 1, channels[i].zrsvId, dataZrsv);
            if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
        }
        if (!isStrEmpty(channels[i].onsvName) && channels[i].onsvId){
            status = ca_array_get(DBR_STS_STRING, 1, channels[i].onsvId, dataOnsv);
            if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
        }
        if (!isStrEmpty(channels[i].twsvName) && channels[i].twsvId){
            status = ca_array_get(DBR_STS_STRING, 1, channels[i].twsvId, dataTwsv);
            if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
        }
        if (!isStrEmpty(channels[i].thsvName) && channels[i].thsvId){
            status = ca_array_get(DBR_STS_STRING, 1, channels[i].thsvId, dataThsv);
            if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
        }
        if (!isStrEmpty(channels[i].frsvName) && channels[i].frsvId){
            status = ca_array_get(DBR_STS_STRING, 1, channels[i].frsvId, dataFrsv);
            if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
        }
        if (!isStrEmpty(channels[i].fvsvName) && channels[i].fvsvId){
            status = ca_array_get(DBR_STS_STRING, 1, channels[i].fvsvId, dataFvsv);
            if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
        }
        if (!isStrEmpty(channels[i].sxsvName) && channels[i].sxsvId){
            status = ca_array_get(DBR_STS_STRING, 1, channels[i].sxsvId, dataSxsv);
            if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
        }
        if (!isStrEmpty(channels[i].svsvName) && channels[i].svsvId){
            status = ca_array_get(DBR_STS_STRING, 1, channels[i].svsvId, dataSvsv);
            if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
        }
        if (!isStrEmpty(channels[i].eisvName) && channels[i].eisvId){
            status = ca_array_get(DBR_STS_STRING, 1, channels[i].eisvId, dataEisv);
            if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
        }
        if (!isStrEmpty(channels[i].nisvName) && channels[i].nisvId){
            status = ca_array_get(DBR_STS_STRING, 1, channels[i].nisvId, dataNisv);
            if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
        }
        if (!isStrEmpty(channels[i].tesvName) && channels[i].tesvId){
            status = ca_array_get(DBR_STS_STRING, 1, channels[i].tesvId, dataTesv);
            if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
        }
        if (!isStrEmpty(channels[i].elsvName) && channels[i].elsvId){
            status = ca_array_get(DBR_STS_STRING, 1, channels[i].elsvId, dataElsv);
            if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
        }
        if (!isStrEmpty(channels[i].tvsvName) && channels[i].tvsvId){
            status = ca_array_get(DBR_STS_STRING, 1, channels[i].tvsvId, dataTvsv);
            if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
        }
        if (!isStrEmpty(channels[i].ttsvName) && channels[i].ttsvId){
            status = ca_array_get(DBR_STS_STRING, 1, channels[i].ttsvId, dataTtsv);
            if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
        }
        if (!isStrEmpty(channels[i].ftsvName) && channels[i].ftsvId){
            status = ca_array_get(DBR_STS_STRING, 1, channels[i].ftsvId, dataFtsv);
            if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
        }
        if (!isStrEmpty(channels[i].ffsvName) && channels[i].ffsvId){
            status = ca_array_get(DBR_STS_STRING, 1, channels[i].ffsvId, dataFfsv);
            if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
        }


        status = ca_pend_io(arguments.caTimeout);
        if (status != ECA_TIMEOUT) fprintf (stderr,"All issued requests for record fields did not complete.\n");

        //start printing
        fputc('\n',stdout);
        fputc('\n',stdout);
        printf("%s\n", channels[i].name);														//name
        printf("%s\n", ((struct dbr_sts_string *)dataDesc)->value);								//description
        printf("\n");
        printf("    Native DBF type: %s\n", dbf_type_to_text(ca_field_type(channels[i].id)));	//field type
        printf("    Number of elements: %ld\n", channels[i].count);								//number of elements


        switch (channels[i].type){
        case DBR_CTRL_STRING:
            printf("    Value: %s", ((struct dbr_sts_string *)data)->value);					//value
            fputc('\n',stdout);
            printf("    Alarm status: %s, severity: %s\n", \
                    epicsAlarmConditionStrings[((struct dbr_sts_string *)data)->status], \
                    epicsAlarmSeverityStrings[((struct dbr_sts_string *)data)->severity]);		//status and severity
            break;
        case DBR_CTRL_INT://and short
            printf("    Value: %" PRId16"\n", ((struct dbr_ctrl_int *)data)->value);					//value
            printf("    Units: %s\n", ((struct dbr_ctrl_int *)data)->units);					//units
            fputc('\n',stdout);
            printf("    Alarm status: %s, severity: %s\n", \
                    epicsAlarmConditionStrings[((struct dbr_ctrl_int *)data)->status], \
                    epicsAlarmSeverityStrings[((struct dbr_ctrl_int *)data)->severity]);		//status and severity
            fputc('\n',stdout);
            printf("	Warning upper limit: %" PRId16", lower limit: %"PRId16"\n", \
                    ((struct dbr_ctrl_int *)data)->upper_warning_limit, ((struct dbr_ctrl_int *)data)->lower_warning_limit); //warning limits
            printf("	Alarm upper limit: %" PRId16", lower limit: %" PRId16"\n", \
                    ((struct dbr_ctrl_int *)data)->upper_alarm_limit, ((struct dbr_ctrl_int *)data)->lower_alarm_limit); //alarm limits
            printf("	Control upper limit: %"PRId16", lower limit: %"PRId16"\n", ((struct dbr_ctrl_int *)data)->upper_ctrl_limit,\
                    ((struct dbr_ctrl_int *)data)->lower_ctrl_limit);							//control limits
            printf("	Display upper limit: %"PRId16", lower limit: %"PRId16"\n", ((struct dbr_ctrl_int *)data)->upper_disp_limit,\
                    ((struct dbr_ctrl_int *)data)->lower_disp_limit);							//display limits
            break;
        case DBR_CTRL_FLOAT:
            printf("    Value: %f\n", ((struct dbr_ctrl_float *)data)->value);
            printf("    Units: %s\n", ((struct dbr_ctrl_float *)data)->units);
            fputc('\n',stdout);
            printf("    Alarm status: %s, severity: %s\n", \
                    epicsAlarmConditionStrings[((struct dbr_ctrl_float *)data)->status], \
                    epicsAlarmSeverityStrings[((struct dbr_ctrl_float *)data)->severity]);
            printf("\n");
            printf("	Warning upper limit: %f, lower limit: %f\n", \
                    ((struct dbr_ctrl_float *)data)->upper_warning_limit, ((struct dbr_ctrl_float *)data)->lower_warning_limit);
            printf("	Alarm upper limit: %f, lower limit: %f\n", \
                    ((struct dbr_ctrl_float *)data)->upper_alarm_limit, ((struct dbr_ctrl_float *)data)->lower_alarm_limit);
            printf("	Control upper limit: %f, lower limit: %f\n", ((struct dbr_ctrl_float *)data)->upper_ctrl_limit,\
                    ((struct dbr_ctrl_float *)data)->lower_ctrl_limit);
            printf("	Display upper limit: %f, lower limit: %f\n", ((struct dbr_ctrl_float *)data)->upper_disp_limit,\
                    ((struct dbr_ctrl_float *)data)->lower_disp_limit);
            fputc('\n',stdout);
            printf("	Precision: %"PRId16"\n",((struct dbr_ctrl_float *)data)->precision);
            printf("	RISC alignment: %"PRId16"\n",((struct dbr_ctrl_float *)data)->RISC_pad);
            break;
        case DBR_CTRL_ENUM:
            printf("    Value: %"PRId16"\n", ((struct dbr_ctrl_enum *)data)->value);
            fputc('\n',stdout);
            printf("    Alarm status: %s, severity: %s\n", \
                    epicsAlarmConditionStrings[((struct dbr_ctrl_enum *)data)->status], \
                    epicsAlarmSeverityStrings[((struct dbr_ctrl_enum *)data)->severity]);

            printf("	Number of enum strings: %"PRId16"\n", ((struct dbr_ctrl_enum *)data)->no_str);
            for (j=0; j<((struct dbr_ctrl_enum *)data)->no_str; ++j){
                printf("	string %d: %s\n", j, ((struct dbr_ctrl_enum *)data)->strs[j]);
            }
            break;
        case DBR_CTRL_CHAR:
            printf("    Value: %c\n", ((struct dbr_ctrl_char *)data)->value);
            printf("    Units: %s\n", ((struct dbr_ctrl_char *)data)->units);
            fputc('\n',stdout);
            printf("    Alarm status: %s, severity: %s\n", \
                    epicsAlarmConditionStrings[((struct dbr_ctrl_char *)data)->status], \
                    epicsAlarmSeverityStrings[((struct dbr_ctrl_char *)data)->severity]);
            printf("\n");
            printf("	Warning upper limit: %c, lower limit: %c\n", \
                    ((struct dbr_ctrl_char *)data)->upper_warning_limit, ((struct dbr_ctrl_char *)data)->lower_warning_limit);
            printf("	Alarm upper limit: %c, lower limit: %c\n", \
                    ((struct dbr_ctrl_char *)data)->upper_alarm_limit, ((struct dbr_ctrl_char *)data)->lower_alarm_limit);
            printf("	Control upper limit: %c, lower limit: %c\n", ((struct dbr_ctrl_char *)data)->upper_ctrl_limit,\
                    ((struct dbr_ctrl_char *)data)->lower_ctrl_limit);
            printf("	Display upper limit: %c, lower limit: %c\n", ((struct dbr_ctrl_char *)data)->upper_disp_limit,\
                    ((struct dbr_ctrl_char *)data)->lower_disp_limit);
            break;
        case DBR_CTRL_LONG:
            printf("    Value: %"PRId32"\n", ((struct dbr_ctrl_long *)data)->value);
            printf("    Units: %s\n", ((struct dbr_ctrl_long *)data)->units);
            fputc('\n',stdout);
            printf("    Alarm status: %s, severity: %s\n", \
                    epicsAlarmConditionStrings[((struct dbr_ctrl_long *)data)->status], \
                    epicsAlarmSeverityStrings[((struct dbr_ctrl_long *)data)->severity]);
            fputc('\n',stdout);
            printf("	Warning upper limit: %"PRId32", lower limit: %"PRId32"\n", \
                    ((struct dbr_ctrl_long *)data)->upper_warning_limit, ((struct dbr_ctrl_long *)data)->lower_warning_limit);
            printf("	Alarm upper limit: %"PRId32", lower limit: %"PRId32"\n", \
                    ((struct dbr_ctrl_long *)data)->upper_alarm_limit, ((struct dbr_ctrl_long *)data)->lower_alarm_limit);
            printf("	Control upper limit: %"PRId32", lower limit: %"PRId32"\n", ((struct dbr_ctrl_long *)data)->upper_ctrl_limit,\
                    ((struct dbr_ctrl_long *)data)->lower_ctrl_limit);
            printf("	Display upper limit: %"PRId32", lower limit: %"PRId32"\n", ((struct dbr_ctrl_long *)data)->upper_disp_limit,\
                    ((struct dbr_ctrl_long *)data)->lower_disp_limit);
            break;
        case DBR_CTRL_DOUBLE:
            printf("    Value: %f\n", ((struct dbr_ctrl_double *)data)->value);
            printf("    Units: %s\n", ((struct dbr_ctrl_double *)data)->units);
            fputc('\n',stdout);
            printf("    Alarm status: %s, severity: %s\n", \
                    epicsAlarmConditionStrings[((struct dbr_ctrl_double *)data)->status], \
                    epicsAlarmSeverityStrings[((struct dbr_ctrl_double *)data)->severity]);
            fputc('\n',stdout);
            printf("	Warning upper limit: %f, lower limit: %f\n", \
                    ((struct dbr_ctrl_double *)data)->upper_warning_limit, ((struct dbr_ctrl_double *)data)->lower_warning_limit);
            printf("	Alarm upper limit: %f, lower limit: %f\n", \
                    ((struct dbr_ctrl_double *)data)->upper_alarm_limit, ((struct dbr_ctrl_double *)data)->lower_alarm_limit);
            printf("	Control upper limit: %f, lower limit: %f\n", ((struct dbr_ctrl_double *)data)->upper_ctrl_limit,\
                    ((struct dbr_ctrl_double *)data)->lower_ctrl_limit);
            printf("	Display upper limit: %f, lower limit: %f\n", ((struct dbr_ctrl_double *)data)->upper_disp_limit,\
                    ((struct dbr_ctrl_double *)data)->lower_disp_limit);
            fputc('\n',stdout);
            printf("	Precision: %"PRId16"\n",((struct dbr_ctrl_double *)data)->precision);
            printf("	RISC alignment: %"PRId16"\n",((struct dbr_ctrl_double *)data)->RISC_pad0);
            break;
        }

        fputc('\n',stdout);
        if (!isStrEmpty(channels[i].hhsvName)) printf("	HIHI alarm severity: %s\n", ((struct dbr_sts_string *)dataHhsv)->value);		//severities
        if (!isStrEmpty(channels[i].hsvName)) printf("	HIGH alarm severity: %s\n", ((struct dbr_sts_string *)dataHsv)->value);
        if (!isStrEmpty(channels[i].lsvName)) printf("	LOW alarm severity: %s\n", ((struct dbr_sts_string *)dataLsv)->value);
        if (!isStrEmpty(channels[i].llsvName)) printf("	LOLO alarm severity: %s\n", ((struct dbr_sts_string *)dataLlsv)->value);
        if (!isStrEmpty(channels[i].unsvName)) printf("	UNSV alarm severity: %s\n", ((struct dbr_sts_string *)dataUnsv)->value);
        if (!isStrEmpty(channels[i].cosvName)) printf("	COSV alarm severity: %s\n", ((struct dbr_sts_string *)dataCosv)->value);
        if (!isStrEmpty(channels[i].zrsvName)) printf("	ZRSV alarm severity: %s\n", ((struct dbr_sts_string *)dataZrsv)->value);
        if (!isStrEmpty(channels[i].onsvName)) printf("	ONSV alarm severity: %s\n", ((struct dbr_sts_string *)dataOnsv)->value);
        if (!isStrEmpty(channels[i].twsvName)) printf("	TWSV alarm severity: %s\n", ((struct dbr_sts_string *)dataTwsv)->value);
        if (!isStrEmpty(channels[i].thsvName)) printf("	THSV alarm severity: %s\n", ((struct dbr_sts_string *)dataThsv)->value);
        if (!isStrEmpty(channels[i].frsvName)) printf("	FRSV alarm severity: %s\n", ((struct dbr_sts_string *)dataFrsv)->value);
        if (!isStrEmpty(channels[i].fvsvName)) printf("	FVSV alarm severity: %s\n", ((struct dbr_sts_string *)dataFvsv)->value);
        if (!isStrEmpty(channels[i].sxsvName)) printf("	SXSV alarm severity: %s\n", ((struct dbr_sts_string *)dataSxsv)->value);
        if (!isStrEmpty(channels[i].svsvName)) printf("	SVSV alarm severity: %s\n", ((struct dbr_sts_string *)dataSvsv)->value);
        if (!isStrEmpty(channels[i].eisvName)) printf("	EISV alarm severity: %s\n", ((struct dbr_sts_string *)dataEisv)->value);
        if (!isStrEmpty(channels[i].nisvName)) printf("	NISV alarm severity: %s\n", ((struct dbr_sts_string *)dataNisv)->value);
        if (!isStrEmpty(channels[i].tesvName)) printf("	TESV alarm severity: %s\n", ((struct dbr_sts_string *)dataTesv)->value);
        if (!isStrEmpty(channels[i].elsvName)) printf("	ELSV alarm severity: %s\n", ((struct dbr_sts_string *)dataElsv)->value);
        if (!isStrEmpty(channels[i].tvsvName)) printf("	TVSV alarm severity: %s\n", ((struct dbr_sts_string *)dataTvsv)->value);
        if (!isStrEmpty(channels[i].ttsvName)) printf("	TTSV alarm severity: %s\n", ((struct dbr_sts_string *)dataTtsv)->value);
        if (!isStrEmpty(channels[i].ftsvName)) printf("	FTSV alarm severity: %s\n", ((struct dbr_sts_string *)dataFtsv)->value);
        if (!isStrEmpty(channels[i].ffsvName)) printf("	FFSV alarm severity: %s\n", ((struct dbr_sts_string *)dataFfsv)->value);
        fputc('\n',stdout);

        readAccess = ca_read_access(channels[i].id);
        writeAccess = ca_write_access(channels[i].id);

        printf("	IOC name: %s\n", ca_host_name(channels[i].id));									//host name
        printf("	Read access: "); if(readAccess) printf("yes\n"); else printf("no\n");			//read and write access
        printf("	Write access: "); if(writeAccess) printf("yes\n"); else printf("no\n");


        free(data);
        free(dataDesc);
        free(dataHhsv);
        free(dataHsv);
        free(dataLsv);
        free(dataLlsv);
        free(dataUnsv);
        free(dataCosv);
        free(dataZrsv);
        free(dataOnsv);
        free(dataTwsv);
        free(dataThsv);
        free(dataFrsv);
        free(dataFvsv);
        free(dataSxsv);
        free(dataSvsv);
        free(dataEisv);
        free(dataNisv);
        free(dataTesv);
        free(dataElsv);
        free(dataTvsv);
        free(dataElsv);
        free(dataTtsv);
        free(dataFtsv);
        free(dataFfsv);

    }

}

void channelStatusCallback(struct connection_handler_args args){
//callback for ca_create_channel. Is executed whenever a channel connects or
//disconnects. When the channel connects for the first time, several properties
//are set, such as number of elements and request types.
//Upon (every) connection, a separate request to obtain units is issued if
//needed. On channel disconnect, the data is cleared.

    struct channel *ch = ( struct channel * ) ca_puser ( args.chid );

    int status;


    if ( args.op == CA_OP_CONN_UP ) {

        if (!ch->firstConnection){
            //determine channel properties

            //how many array elements to request
            ch->count = ca_element_count ( ch->id );
            if (arguments.outNelm == -1) ch->outNelm = ch->count;
            else if(arguments.outNelm > 0 && arguments.outNelm < ch->count) ch->outNelm = arguments.outNelm;
            else{
                ch->outNelm = ch->count;
                fprintf(stderr, "Invalid number %d of requested elements to read - number of elements in %s is %ld.\n", arguments.outNelm, ch->name, ch->count);
            }

            //request type
            if (arguments.dbrRequestType == -1){   //if DBR type not specified, decide based on desired details
                if (arguments.time || arguments.date || arguments.timestamp){
                    //time specified, use TIME_
                    if (arguments.s && ca_field_type(ch->id) == DBF_ENUM){
                        //if enum && s, use time_string
                        ch->type = DBR_TIME_STRING;
                    }
                    else{ //else use time_native
                        ch->type = dbf_type_to_DBR_TIME(ca_field_type(ch->id));

                        //if units, issue get request for metadata. String and enum records don't have units, so skip.
                        unsigned int baseType = ca_field_type(ch->id);
                        if (!arguments.nounit && baseType != DBF_STRING && baseType != DBF_ENUM){
                            int reqType = dbf_type_to_DBR_GR(baseType);

                            status = ca_array_get_callback(reqType, ch->count, ch->id, getStaticUnitsCallback, ch);
                            if (status != ECA_NORMAL) fprintf(stderr, "Problem creating ca_get request for process value %s: %s\n",ch->name, ca_message(status));
                            status = ca_flush_io();//flush this so it gets processed before everything else
                            if (status != ECA_NORMAL) fprintf(stderr, "Problem flushing process value %s: %s.\n", ch->name, ca_message(status));
                        }

                    }
                }
                else{// use GR_ by default
                    ch->type = dbf_type_to_DBR_GR(ca_field_type(ch->id));
                }
            }
            else ch->type = arguments.dbrRequestType;

            if (arguments.tool == camon || arguments.tool == cawait){
                //create subscription
                status = ca_create_subscription(ch->type, ch->outNelm, ch->id, DBE_VALUE | DBE_ALARM | DBE_LOG, caReadCallback, ch, NULL);
                if (status != ECA_NORMAL) fprintf(stderr, "Problem creating subscription for process value %s: %s.\n",ch->name, ca_message(status));
                if (arguments.tool == cawait && arguments.timeout!=-1){
                    //set first
                    epicsTimeGetCurrent(&timeoutTime);
                    epicsTimeAddSeconds(&timeoutTime, arguments.timeout);
                }
                //TODOrunMonitor = true;//we need this here to display the current state of the channel
            }

            ch->firstConnection = true;
        }
        else{//get just metadata if needed
            if (!arguments.nounit && (arguments.time || arguments.date || arguments.timestamp)){
                int reqType = dbf_type_to_DBR_GR(ca_field_type(ch->id));
                status = ca_array_get_callback(reqType, ch->count, ch->id, getStaticUnitsCallback, ch);
                if (status != ECA_NORMAL) fprintf(stderr, "Problem creating ca_get request for process value %s: %s\n",ch->name, ca_message(status));
                status = ca_flush_io();
                if (status != ECA_NORMAL) fprintf(stderr, "Problem flushing process value %s: %s.\n", ch->name, ca_message(status));
            }
        }

    }
    else if(args.op == CA_OP_CONN_DOWN){
        //clear metadata
        clearStr(outUnits[ch->i]);
    }
}

bool checkStatus(int status){
//checks status of channel create_ and clear_
    if (status != ECA_NORMAL){
        fprintf(stderr, "CA error %s occurred while trying "
                "to start channel access.\n", ca_message(status));
        SEVCHK(status, "CA error");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

bool createChannelMustSucceed(const char *pChanName, caCh *pConnStateCallback, void *pUserPrivate, capri priority, chid *pChanID){
//creates CA connection
    int status = ca_create_channel(pChanName, pConnStateCallback, pUserPrivate, priority, pChanID);
    return checkStatus(status);    // error if the channels are not created
}


int caInit(struct channel *channels, int nChannels){
//creates contexts and channels.
    int status, i;

    status = ca_context_create(ca_disable_preemptive_callback);

    if (checkStatus(status) != EXIT_SUCCESS) return EXIT_FAILURE;

    for(i=0; i < nChannels; i++){

        if(createChannelMustSucceed(channels[i].name, channelStatusCallback, &channels[i], CA_PRIORITY, &channels[i].id)) return EXIT_FAILURE;

        //if tool = cagets or cado, each channel has a sibling connecting to the proc field
        if (arguments.tool == cagets || arguments.tool == cado){
            if (createChannelMustSucceed(channels[i].procName, 0 , 0, CA_PRIORITY, &channels[i].procId)) return EXIT_FAILURE;
        }
    }

    //wait for connection callbacks to complete
    waitForCompletition(channels, nChannels, true);
    for (i=0; i<nChannels; ++i){
        if (!channels[i].firstConnection) fprintf (stderr,"Process variable %s not connected.\n", channels[i].name);
    }

    if (arguments.tool == cainfo){
        //if tool = cainfo, each channel has siblings connecting to the desc and *sv fields
        for (i=0; i<nChannels; ++i){
            if (!channels[i].firstConnection) continue; //skip if not connected


            if (createChannelMustSucceed(channels[i].descName, 0 , 0, CA_PRIORITY, &channels[i].descId)) return EXIT_FAILURE;
            if (createChannelMustSucceed(channels[i].hhsvName, 0 , 0, CA_PRIORITY, &channels[i].hhsvId)) return EXIT_FAILURE;
            if (createChannelMustSucceed(channels[i].hsvName, 0 , 0, CA_PRIORITY, &channels[i].hsvId)) return EXIT_FAILURE;
            if (createChannelMustSucceed(channels[i].lsvName, 0 , 0, CA_PRIORITY, &channels[i].lsvId)) return EXIT_FAILURE;
            if (createChannelMustSucceed(channels[i].llsvName, 0 , 0, CA_PRIORITY, &channels[i].llsvId)) return EXIT_FAILURE;

            //we flush the channels now in order to detect what types of severities the record specifies.
            status = ca_pend_io ( arguments.caTimeout );

            if(status == ECA_TIMEOUT) {
                //channel doesn't have usual sev fields
                clearStr(channels[i].hhsvName);
                clearStr(channels[i].hsvName);
                clearStr(channels[i].lsvName);
                clearStr(channels[i].llsvName);

                //try multi-bit record severities
                if (createChannelMustSucceed(channels[i].unsvName, 0 , 0, CA_PRIORITY, &channels[i].unsvId)) return EXIT_FAILURE;
                if (createChannelMustSucceed(channels[i].cosvName, 0 , 0, CA_PRIORITY, &channels[i].cosvId)) return EXIT_FAILURE;
                if (createChannelMustSucceed(channels[i].zrsvName, 0 , 0, CA_PRIORITY, &channels[i].zrsvId)) return EXIT_FAILURE;
                if (createChannelMustSucceed(channels[i].onsvName, 0 , 0, CA_PRIORITY, &channels[i].onsvId)) return EXIT_FAILURE;
                if (createChannelMustSucceed(channels[i].twsvName, 0 , 0, CA_PRIORITY, &channels[i].twsvId)) return EXIT_FAILURE;
                if (createChannelMustSucceed(channels[i].thsvName, 0 , 0, CA_PRIORITY, &channels[i].thsvId)) return EXIT_FAILURE;
                if (createChannelMustSucceed(channels[i].frsvName, 0 , 0, CA_PRIORITY, &channels[i].frsvId)) return EXIT_FAILURE;
                if (createChannelMustSucceed(channels[i].fvsvName, 0 , 0, CA_PRIORITY, &channels[i].fvsvId)) return EXIT_FAILURE;
                if (createChannelMustSucceed(channels[i].sxsvName, 0 , 0, CA_PRIORITY, &channels[i].sxsvId)) return EXIT_FAILURE;
                if (createChannelMustSucceed(channels[i].svsvName, 0 , 0, CA_PRIORITY, &channels[i].svsvId)) return EXIT_FAILURE;
                if (createChannelMustSucceed(channels[i].eisvName, 0 , 0, CA_PRIORITY, &channels[i].eisvId)) return EXIT_FAILURE;
                if (createChannelMustSucceed(channels[i].nisvName, 0 , 0, CA_PRIORITY, &channels[i].nisvId)) return EXIT_FAILURE;
                if (createChannelMustSucceed(channels[i].tesvName, 0 , 0, CA_PRIORITY, &channels[i].tesvId)) return EXIT_FAILURE;
                if (createChannelMustSucceed(channels[i].elsvName, 0 , 0, CA_PRIORITY, &channels[i].elsvId)) return EXIT_FAILURE;
                if (createChannelMustSucceed(channels[i].tvsvName, 0 , 0, CA_PRIORITY, &channels[i].tvsvId)) return EXIT_FAILURE;
                if (createChannelMustSucceed(channels[i].ttsvName, 0 , 0, CA_PRIORITY, &channels[i].ttsvId)) return EXIT_FAILURE;
                if (createChannelMustSucceed(channels[i].ftsvName, 0 , 0, CA_PRIORITY, &channels[i].ftsvId)) return EXIT_FAILURE;
                if (createChannelMustSucceed(channels[i].ffsvName, 0 , 0, CA_PRIORITY, &channels[i].ffsvId)) return EXIT_FAILURE;

                status = ca_pend_io ( arguments.caTimeout );

                if(status == ECA_TIMEOUT) {
                    //channel doesn't have multi-bit severity fields
                    clearStr(channels[i].unsvName);
                    clearStr(channels[i].cosvName);
                    clearStr(channels[i].zrsvName);
                    clearStr(channels[i].onsvName);
                    clearStr(channels[i].twsvName);
                    clearStr(channels[i].thsvName);
                    clearStr(channels[i].frsvName);
                    clearStr(channels[i].fvsvName);
                    clearStr(channels[i].sxsvName);
                    clearStr(channels[i].svsvName);
                    clearStr(channels[i].eisvName);
                    clearStr(channels[i].nisvName);
                    clearStr(channels[i].tesvName);
                    clearStr(channels[i].elsvName);
                    clearStr(channels[i].tvsvName);
                    clearStr(channels[i].ttsvName);
                    clearStr(channels[i].ftsvName);
                    clearStr(channels[i].ffsvName);
                }
            }
            else{
                //channel doesn't have multi-bit severity fields
                clearStr(channels[i].unsvName);
                clearStr(channels[i].cosvName);
                clearStr(channels[i].zrsvName);
                clearStr(channels[i].onsvName);
                clearStr(channels[i].twsvName);
                clearStr(channels[i].thsvName);
                clearStr(channels[i].frsvName);
                clearStr(channels[i].fvsvName);
                clearStr(channels[i].sxsvName);
                clearStr(channels[i].svsvName);
                clearStr(channels[i].eisvName);
                clearStr(channels[i].nisvName);
                clearStr(channels[i].tesvName);
                clearStr(channels[i].elsvName);
                clearStr(channels[i].tvsvName);
                clearStr(channels[i].ttsvName);
                clearStr(channels[i].ftsvName);
                clearStr(channels[i].ffsvName);
            }

        }
    }


    return EXIT_SUCCESS;
}

int caDisconnect(struct channel * channels, int nChannels){
    int status, i;
    int exitStatus = EXIT_SUCCESS;

    for(i=0; i < nChannels; i++){
        status = ca_clear_channel(channels[i].id);
        if( status != ECA_NORMAL) {
            fprintf(stderr, "CA error %s occurred while trying to stop channel access.\n", ca_message(status));
            SEVCHK(status, "CA error");
            exitStatus = EXIT_FAILURE;
            break;
        }
    }
    return exitStatus;
}


bool endsWith(char * src, char * match){
//checks whether end of src matches the string match.
    if (strlen(src)>=strlen(match)){
        return !strcmp(src + (strlen(src)-strlen(match)), match) ;
    }
    else return false;
}

int main ( int argc, char ** argv )
{//main function: reads arguments, allocates memory, calls ca* functions, frees memory and exits.

    int opt;                    // getopt() current option
    int opt_long;               // getopt_long() current long option
    int nChannels=0;              // Number of channels
    int i,j;                      // counter
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
        {"stat",    	no_argument, 		0,  0 },	//status and severity on
        {"nostat",  	no_argument,  		0,  0 },	//status and severity off
        {"noname",  	no_argument,        0,  0 },	//hide name
        {"nounit",  	no_argument,        0,  0 },	//hide units
        {"timestamp",	required_argument, 	0,  0 },	//timestamp type r,u,c
        {"localdate",	no_argument, 		0,  0 },	//client date
        {"time",		no_argument, 		0,  0 },	//server time
        {"localtime",	no_argument, 		0,  0 },	//client time
        {"date",		no_argument, 		0,  0 },	//server date
        {"inNelm",		required_argument,	0,  0 },	//number of array elements - write
        {"outNelm", 	required_argument,	0,  0 },	//number of array elements - read
        {"outSep",		required_argument,	0,  0 },	//array field separator - read
        {"inSep",		required_argument,	0,  0 },	//array field separator - write
        {"nord",		no_argument,		0,  0 },	//display number of array elements
        {"tool",		required_argument, 	0,	0 },	//tool
        {"timeout",   	required_argument, 	0,	0 },	//timeout
        {"dbrtype",   	required_argument, 	0,	0 },	//dbrtype
        {0,         	0,                 	0,  0 }
    };
    putenv("POSIXLY_CORRECT="); //Behave correctly on GNU getopt systems = stop parsing after 1st non option is encountered

    while ((opt = getopt_long_only(argc, argv, ":w:e:f:g:n:sthd", long_options, &opt_long)) != -1) {
        switch (opt) {
        case 'w':
            if (sscanf(optarg, "%f", &arguments.caTimeout) != 1){    // type was not given as float
                arguments.caTimeout = CA_DEFAULT_TIMEOUT;
                fprintf(stderr, "Requested timeout invalid - ignored. ('%s -h' for help.)\n", argv[0]);
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
            if (sscanf(optarg, "%d", &digits) != 1){
                fprintf(stderr,
                        "Invalid precision argument '%s' "
                        "for option '-%c' - ignored.\n", optarg, opt);
            } else {
                if (digits>=0){
                    if (arguments.dblFormatType == '\0' && arguments.prec >= 0) { // in case of overiding -prec
                        fprintf(stderr, "Option '-%c' overrides precision set with '-prec'.\n", opt);
                    }
                    arguments.dblFormatType = opt; // set 'e' 'f' or 'g'
                    arguments.prec = digits;
                }
                else{
                    fprintf(stderr, "Precision %d for option '-%c' "
                            "out of range - ignored.\n", digits, opt);
                }
            }
            break;
        case 's':	//try to interpret values as strings
            arguments.s = true;
            break;
        case 't':	//same as time
            arguments.time = true;
            break;
        case 'n':	//stop monitor after numUpdates
            arguments.numUpdates = 0;
            if (sscanf(optarg, "%d", &arguments.numUpdates) != 1) {
                fprintf(stderr, "Invalid argument '%s' for option '-%c' - ignored.\n", optarg, opt);
            }
            else {
                if (arguments.numUpdates < 0) {
                    fprintf(stderr, "Number of updates for option '-%c' must be greater or equal to zero - ignored.\n", opt);
                    arguments.numUpdates = -1;
                }
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
                        fprintf(stderr,
                            "Invalid round type '%s' "
                            "for option '%s' - ignored.\n", optarg, long_options[opt_long].name);
                    }
                } else { // type was given as a number
                    if( type < roundType_round || roundType_floor < type) {   // out of range check
                        arguments.round = roundType_no_rounding;
                        fprintf(stderr,
                            "Invalid round type '%s' "
                            "for option '%s' - ignored.\n", optarg, long_options[opt_long].name);
                    } else{
                        arguments.round = type;
                    }
                }
                break;
            case 3:   // prec
                if (arguments.dblFormatType == '\0') { // formating with -f -g -e has priority
                    if (sscanf(optarg, "%d", &arguments.prec) != 1){
                        fprintf(stderr,
                                "Invalid precision argument '%s' "
                                "for option '%s' - ignored.\n", optarg, long_options[opt_long].name);
                    } else if (arguments.prec < 0) {
                       fprintf(stderr, "Precision %d for option '%s' "
                               "out of range - ignored.\n", arguments.prec, long_options[opt_long].name);
                       arguments.prec = -1;
                    }
                }
                else {
                    fprintf(stderr, "Option '-prec' ignored because of '-f', '-e' or '-g'.\n");
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
                   fprintf(stderr,	"Invalid argument '%s' "
                            "for option '%s' - ignored. Allowed arguments: r,u,c.\n", optarg, long_options[opt_long].name);
                    arguments.timestamp = 0;
                } else {
                    if (arguments.timestamp != 'r' && arguments.timestamp != 'u' && arguments.timestamp != 'c') {
                        fprintf(stderr,	"Invalid argument '%s' "
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
            case 17:   // inNelm - number of elements - write
                if (sscanf(optarg, "%d", &arguments.inNelm) != 1){
                    fprintf(stderr, "Invalid count argument '%s' "
                            "for option '%s' - ignored.\n", optarg, long_options[opt_long].name);
                }
                else {
                    if (arguments.inNelm < 1) {
                        fprintf(stderr, "Count number for option '%s' "
                                "must be positive integer - ignored.\n", long_options[opt_long].name);
                        arguments.inNelm = 1;
                    }
                }
                break;
            case 18:   // outNelm - number of elements - read
                if (sscanf(optarg, "%d", &arguments.outNelm) != 1){
                    fprintf(stderr, "Invalid count argument '%s' "
                            "for option '%s' - ignored.\n", optarg, long_options[opt_long].name);
                }
                else {
                    if (arguments.outNelm < 1) {
                        fprintf(stderr, "Count number for option '%s' "
                                "must be positive integer - ignored.\n", long_options[opt_long].name);
                        arguments.outNelm = -1;
                    }
                }
                break;
            case 19:   // field separator for output
                if (sscanf(optarg, "%c", &arguments.fieldSeparator) != 1){
                    fprintf(stderr, "Invalid argument '%s' "
                            "for option '%s' - ignored.\n", optarg, long_options[opt_long].name);
                }
                break;
            case 20:   // field separator for input
                if (sscanf(optarg, "%c", &arguments.inputSeparator) != 1){
                    fprintf(stderr, "Invalid argument '%s' "
                            "for option '%s' - ignored.\n", optarg, long_options[opt_long].name);
                }
                break;
            case 21:   // nord
                arguments.nord = true;
                break;
            case 22:	// tool
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
            case 23: //timeout
                if (sscanf(optarg, "%f", &arguments.timeout) != 1){
                    fprintf(stderr, "Invalid timeout argument '%s' "
                            "for option '%s' - ignored.\n", optarg, long_options[opt_long].name);
                }
                else {
                    if (arguments.timeout <= 0) {
                        fprintf(stderr, "Timeout argument must be positive - ignored.\n");
                        arguments.timeout = -1;
                    }
                }
                break;
            case 24: //dbrtype
                if (sscanf(optarg, "%d", &arguments.dbrRequestType) != 1)     // type was not given as an integer
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
                    fprintf(stderr, "Requested dbr type out of range "
                            "or invalid - ignored. ('%s -h' for help.)\n", argv[0]);
                    arguments.dbrRequestType = -1;
                }
                break;
            }
            break;
        case '?':
            fprintf(stderr, "Unrecognized option: '-%c'. ('%s -h' for help.)\n", optopt, argv[0]);
            return EXIT_FAILURE;
            break;
        case ':':
            fprintf(stderr, "Option '-%c' requires an argument. ('%s -h' for help.)\n", optopt, argv[0]);
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
     if(arguments.tool == cainfo && (arguments.s || arguments.num || arguments.bin  || arguments.hex || arguments.oct \
             || arguments.dbrRequestType != -1 || arguments.prec != -1 || arguments.round != roundType_no_rounding || arguments.plain \
             || arguments.dblFormatType != '\0' || arguments.stat || arguments.nostat || arguments.noname || arguments.nounit \
             || arguments.timestamp != '\0' || arguments.timeout != -1 || arguments.date || arguments.time || arguments.localdate \
             || arguments.localtime || arguments.fieldSeparator != ' ' || arguments.inputSeparator != ' ' || arguments.numUpdates != -1\
             || arguments.inNelm != 1 || arguments.outNelm != -1 || arguments.nord)){
         fprintf(stderr, "The only option allowed for cainfo is -w. Ignoring the rest.\n");
     }
     else if (arguments.tool != camon){
         if (arguments.timestamp != 0){
             fprintf(stderr, "Option -timestamp can only be specified with camon.\n");
         }
         if (arguments.numUpdates != -1){
             fprintf(stderr, "Option -n can only be specified with camon.\n");
         }
     }
     else if (arguments.inNelm != 1 && (arguments.tool != caput && arguments.tool != caputq)){
         fprintf(stderr, "Option -inNelm can only be specified with caput or caputq.\n");
     }
     if ((arguments.outNelm != -1 || arguments.num !=false || arguments.hex !=false || arguments.bin !=false || arguments.oct !=false)\
             && (arguments.tool == cado || arguments.tool == caputq)){
         fprintf(stderr, "Option -outNelm, -num, -hex, -bin and -oct cannot be specified with cado or caputq, because they have no output.\n");
     }
     if (arguments.nostat != false && arguments.stat != false){
         fprintf(stderr, "Options -stat and -nostat are mutually exclusive.\n");
         arguments.nostat = false;
     }
     if (arguments.hex + arguments.bin + arguments.oct > 1 ) {
         arguments.hex = false;
         arguments.bin = false;
         arguments.oct = false;
         fprintf(stderr, "Options -hex and -bin and -oct are mutually exclusive. Ignoring.\n");
     }
     if (arguments.num && arguments.s){
         fprintf(stderr, "Options -num and -s are mutually exclusive.\n");
     }
     if (arguments.plain) {
         printf("Warning: -plain option overrides all formatting switches.\n"); // TODO warning was not printed on PSI catools
         arguments.num =false; arguments.hex =false; arguments.bin = false; arguments.oct =false; arguments.s =false;
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
    else if (arguments.tool == caput || arguments.tool == caputq){
        //It takes (inNelm+pv name) elements to define each channel
        if ((argc - optind) % (arguments.inNelm +1)){
            fprintf(stderr, "One of the PVs is missing the value to be written ('%s -h' for help).\n", argv[0]);
            return EXIT_FAILURE;
        }
        nChannels = (argc - optind)/(arguments.inNelm +1);
    }
    else if (arguments.tool == cawait){
        if ((argc - optind) % 2){
            fprintf(stderr, "One of the PVs is missing the condition ('%s -h' for help).\n", argv[0]);
            return EXIT_FAILURE;
        }
        nChannels = (argc - optind)/2;	// half of the args are PV names, rest conditions
    }


    if (nChannels < 1)
    {
        fprintf(stderr, "No record name specified. ('%s -h' for help.)\n", argv[0]);
        return EXIT_FAILURE;
    }

    //allocate memory for channel structures
    channels = (struct channel *) callocMustSucceed (nChannels, sizeof(struct channel), "main");
    for (i=0;i<nChannels;++i){
        if (arguments.tool == cagets || arguments.tool == cado){
            channels[i].procName = callocMustSucceed (LEN_RECORD_NAME + 6, sizeof(char), "main");//6 spaces for .(field name) + null termination
        }
        if(arguments.tool == cainfo){
            channels[i].descName = callocMustSucceed (LEN_RECORD_NAME + 6, sizeof(char), "main");
            channels[i].hhsvName = callocMustSucceed (LEN_RECORD_NAME + 6, sizeof(char), "main");
            channels[i].hsvName = callocMustSucceed (LEN_RECORD_NAME + 6, sizeof(char), "main");
            channels[i].lsvName = callocMustSucceed (LEN_RECORD_NAME + 6, sizeof(char), "main");
            channels[i].llsvName = callocMustSucceed (LEN_RECORD_NAME + 6, sizeof(char), "main");
            channels[i].llsvName = callocMustSucceed (LEN_RECORD_NAME + 6, sizeof(char), "main");
            channels[i].unsvName = callocMustSucceed (LEN_RECORD_NAME + 6, sizeof(char), "main");
            channels[i].cosvName = callocMustSucceed (LEN_RECORD_NAME + 6, sizeof(char), "main");
            channels[i].zrsvName = callocMustSucceed (LEN_RECORD_NAME + 6, sizeof(char), "main");
            channels[i].onsvName = callocMustSucceed (LEN_RECORD_NAME + 6, sizeof(char), "main");
            channels[i].twsvName = callocMustSucceed (LEN_RECORD_NAME + 6, sizeof(char), "main");
            channels[i].thsvName = callocMustSucceed (LEN_RECORD_NAME + 6, sizeof(char), "main");
            channels[i].frsvName = callocMustSucceed (LEN_RECORD_NAME + 6, sizeof(char), "main");
            channels[i].fvsvName = callocMustSucceed (LEN_RECORD_NAME + 6, sizeof(char), "main");
            channels[i].sxsvName = callocMustSucceed (LEN_RECORD_NAME + 6, sizeof(char), "main");
            channels[i].svsvName = callocMustSucceed (LEN_RECORD_NAME + 6, sizeof(char), "main");
            channels[i].eisvName = callocMustSucceed (LEN_RECORD_NAME + 6, sizeof(char), "main");
            channels[i].nisvName = callocMustSucceed (LEN_RECORD_NAME + 6, sizeof(char), "main");
            channels[i].tesvName = callocMustSucceed (LEN_RECORD_NAME + 6, sizeof(char), "main");
            channels[i].elsvName = callocMustSucceed (LEN_RECORD_NAME + 6, sizeof(char), "main");
            channels[i].tvsvName = callocMustSucceed (LEN_RECORD_NAME + 6, sizeof(char), "main");
            channels[i].ttsvName = callocMustSucceed (LEN_RECORD_NAME + 6, sizeof(char), "main");
            channels[i].ftsvName = callocMustSucceed (LEN_RECORD_NAME + 6, sizeof(char), "main");
            channels[i].ffsvName = callocMustSucceed (LEN_RECORD_NAME + 6, sizeof(char), "main");
        }
    }



    // Copy PV names from command line
    for (i = 0; optind < argc; i++, optind++){
        //printf("PV %d: %s\n", i, argv[optind]);
        channels[i].name = argv[optind];
        channels[i].i = i;	// channel number, serves to synchronise pvs and output.

        if (arguments.tool == caput || arguments.tool == caputq){
            if (arguments.inNelm > 1){ //treat next nelm arguments as values
                channels[i].writeStr = callocMustSucceed (arguments.inNelm*MAX_STRING_SIZE, sizeof(char), "main");

                for (j=0; j<arguments.inNelm; ++j){
                    int charsToWrite = snprintf(&channels[i].writeStr[j*MAX_STRING_SIZE], MAX_STRING_SIZE, "%s", argv[optind+1]);
                    if (charsToWrite >= MAX_STRING_SIZE) fprintf(stderr,"Input %s is longer than the allowed size %d - truncating.\n", argv[optind+1], MAX_STRING_SIZE);
                    optind++;
                }
                channels[i].inNelm = arguments.inNelm;
            }
            else{ //next argument is a string of values separated by inputSeparator
                //count the number of values in it (= number of separators +1)
                int count;
                for (j=0, count=0; argv[optind+1][j]; j++) {
                    count += (argv[optind+1][j] == arguments.inputSeparator);
                }
                count+=1;
                //and use it to allocate the string
                channels[i].writeStr = callocMustSucceed (count*MAX_STRING_SIZE, sizeof(char), "main");

                if (count == 1){//the argument to write consists of just a single element.
                    int charsToWrite = snprintf(&channels[i].writeStr[0], MAX_STRING_SIZE, "%s", argv[optind+1]);
                    if (charsToWrite >= MAX_STRING_SIZE) fprintf(stderr,"Input %s is longer than the allowed size %d - truncating.\n", argv[optind+1], MAX_STRING_SIZE);
                    channels[i].inNelm = 1;
                }
                else{//parse the string assuming each element is delimited by the inputSeparator char
                    char inSep[2] = {arguments.inputSeparator, 0};
                    j=0;
                    char *token = strtok(argv[optind+1], inSep);
                    while(token) {
                        int charsToWrite = snprintf(&channels[i].writeStr[j*MAX_STRING_SIZE] , MAX_STRING_SIZE, "%s", token);
                        if (charsToWrite >= MAX_STRING_SIZE) fprintf(stderr,"Input %s is longer than the allowed size %d - truncating.\n", token, MAX_STRING_SIZE);
                        j++;
                        token = strtok(NULL, inSep);
                    }
                    channels[i].inNelm = j;
                }
                //finally advance to the next argument
                optind++;
            }
        }
        else if (arguments.tool == cawait) {
            //next argument is the condition string
            if (!cawaitParseCondition(&channels[i], argv[optind+1])) return EXIT_FAILURE;
            optind++;
        }
        else if (arguments.tool == cagets || arguments.tool == cado) {

            size_t lenName = strlen(channels[i].name);
            if(lenName > LEN_RECORD_NAME + 4) { //worst case scenario: longest name + .PROC
                fprintf(stderr, "Record name over %d characters: %s\n", LEN_RECORD_NAME + 4, channels[i].name);
                return EXIT_FAILURE;
            }

            strcpy(channels[i].procName, channels[i].name);

            //append .PROC
            if (lenName > 4 && strcmp(&channels[i].procName[lenName - 4], ".VAL") == 0) {
                //if last 4 elements equal .VAL, replace them
                strcpy(&channels[i].procName[lenName - 4],".PROC");
            }
            else {
                //otherwise simply append
                strcat(&channels[i].procName[lenName],".PROC");
            }
        }
        else if (arguments.tool == cainfo) {
            //set sibling channels for getting desc and severity data
            size_t lenName = strlen(channels[i].name);
            if(lenName > LEN_RECORD_NAME + 3) { //worst case scenario: longest name + .VAL
                fprintf(stderr, "Record name over %d characters: %s\n", LEN_RECORD_NAME + 3, channels[i].name);
                return EXIT_FAILURE;
            }

            strcpy(channels[i].descName, channels[i].name);
            strcpy(channels[i].hhsvName, channels[i].name);
            strcpy(channels[i].hsvName, channels[i].name);
            strcpy(channels[i].lsvName, channels[i].name);
            strcpy(channels[i].llsvName, channels[i].name);
            strcpy(channels[i].unsvName, channels[i].name);
            strcpy(channels[i].cosvName, channels[i].name);
            strcpy(channels[i].zrsvName, channels[i].name);
            strcpy(channels[i].onsvName, channels[i].name);
            strcpy(channels[i].twsvName, channels[i].name);
            strcpy(channels[i].thsvName, channels[i].name);
            strcpy(channels[i].frsvName, channels[i].name);
            strcpy(channels[i].fvsvName, channels[i].name);
            strcpy(channels[i].sxsvName, channels[i].name);
            strcpy(channels[i].svsvName, channels[i].name);
            strcpy(channels[i].eisvName, channels[i].name);
            strcpy(channels[i].nisvName, channels[i].name);
            strcpy(channels[i].tesvName, channels[i].name);
            strcpy(channels[i].elsvName, channels[i].name);
            strcpy(channels[i].tvsvName, channels[i].name);
            strcpy(channels[i].ttsvName, channels[i].name);
            strcpy(channels[i].ftsvName, channels[i].name);
            strcpy(channels[i].ffsvName, channels[i].name);

            //append .DESC, .HHSV, .HSV, .LSV, .LLSV, etc
            if (lenName > 4 && endsWith(channels[i].name, ".VAL")){
                //if last 4 elements equal .VAL, replace them
                strcpy(&channels[i].descName[lenName - 4],".DESC");
                strcpy(&channels[i].hhsvName[lenName - 4],".HHSV");
                strcpy(&channels[i].hsvName[lenName - 4],".HSV");
                strcpy(&channels[i].lsvName[lenName - 4],".LSV");
                strcpy(&channels[i].llsvName[lenName - 4],".LLSV");
                strcpy(&channels[i].unsvName[lenName - 4],".UNSV");
                strcpy(&channels[i].cosvName[lenName - 4],".COSV");
                strcpy(&channels[i].zrsvName[lenName - 4],".ZRSV");
                strcpy(&channels[i].onsvName[lenName - 4],".ONSV");
                strcpy(&channels[i].twsvName[lenName - 4],".TWSV");
                strcpy(&channels[i].thsvName[lenName - 4],".THSV");
                strcpy(&channels[i].frsvName[lenName - 4],".FRSV");
                strcpy(&channels[i].fvsvName[lenName - 4],".FVSV");
                strcpy(&channels[i].sxsvName[lenName - 4],".SXSV");
                strcpy(&channels[i].svsvName[lenName - 4],".SVSV");
                strcpy(&channels[i].eisvName[lenName - 4],".EISV");
                strcpy(&channels[i].nisvName[lenName - 4],".NISV");
                strcpy(&channels[i].tesvName[lenName - 4],".TESV");
                strcpy(&channels[i].elsvName[lenName - 4],".ELSV");
                strcpy(&channels[i].tvsvName[lenName - 4],".TVSV");
                strcpy(&channels[i].ttsvName[lenName - 4],".TTSV");
                strcpy(&channels[i].ftsvName[lenName - 4],".FTSV");
                strcpy(&channels[i].ffsvName[lenName - 4],".FFSV");
            }
            else{
                //otherwise simply append
                strcat(channels[i].descName,".DESC");
                strcat(channels[i].hhsvName,".HHSV");
                strcat(channels[i].hsvName,".HSV");
                strcat(channels[i].lsvName,".LSV");
                strcat(channels[i].llsvName,".LLSV");
                strcat(channels[i].unsvName,".UNSV");
                strcat(channels[i].cosvName,".COSV");
                strcat(channels[i].zrsvName,".ZRSV");
                strcat(channels[i].onsvName,".ONSV");
                strcat(channels[i].twsvName,".TWSV");
                strcat(channels[i].thsvName,".THSV");
                strcat(channels[i].frsvName,".FRSV");
                strcat(channels[i].fvsvName,".FVSV");
                strcat(channels[i].sxsvName,".SXSV");
                strcat(channels[i].svsvName,".SVSV");
                strcat(channels[i].eisvName,".EISV");
                strcat(channels[i].nisvName,".NISV");
                strcat(channels[i].tesvName,".TESV");
                strcat(channels[i].elsvName,".ELSV");
                strcat(channels[i].tvsvName,".TVSV");
                strcat(channels[i].ttsvName,".TTSV");
                strcat(channels[i].ftsvName,".FTSV");
                strcat(channels[i].ffsvName,".FFSV");
            }

        }
    }


    //allocate memory for output strings
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
    if(caInit(channels, nChannels) != EXIT_SUCCESS) return EXIT_FAILURE;

    if (arguments.tool == cainfo){
        cainfoRequest(channels, nChannels);
    }
    else if(arguments.tool == camon || arguments.tool == cawait){
        monitorLoop(channels, nChannels);
    }
    else{
        caRequest(channels, nChannels);
    }

    if (caDisconnect(channels, nChannels) != EXIT_SUCCESS) return EXIT_FAILURE;

    //free channels
    for (i=0;i<nChannels;++i){
        free(channels[i].writeStr);
        free(channels[i].procName);
        free(channels[i].descName);
        free(channels[i].hhsvName);
        free(channels[i].hsvName);
        free(channels[i].lsvName);
        free(channels[i].llsvName);
        free(channels[i].unsvName);
        free(channels[i].cosvName);
        free(channels[i].zrsvName);
        free(channels[i].onsvName);
        free(channels[i].twsvName);
        free(channels[i].thsvName);
        free(channels[i].frsvName);
        free(channels[i].fvsvName);
        free(channels[i].sxsvName);
        free(channels[i].svsvName);
        free(channels[i].eisvName);
        free(channels[i].nisvName);
        free(channels[i].tesvName);
        free(channels[i].elsvName);
        free(channels[i].tvsvName);
        free(channels[i].ttsvName);
        free(channels[i].ftsvName);
        free(channels[i].ffsvName);
    }
    free(channels);

    //free output strings
    for(i = 0; i < nChannels; i++){
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


    return EXIT_SUCCESS;
}
