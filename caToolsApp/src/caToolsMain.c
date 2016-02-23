/* caToolsMain.c */
/* Author:  Rok Vuga Date:    FEB 2016 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>
#include <getopt.h>
#include <stdbool.h>
#include <cantProceed.h>
#include <assert.h>

#include "cadef.h"
#include "alarmString.h"
#include "alarm.h"



#define CA_PRIORITY CA_PRIORITY_MIN
#define CA_DEFAULT_TIMEOUT 1

enum roundType{
	roundType_no_rounding=-1,
    roundType_default=0,
    roundType_ceil=1,
    roundType_floor=2
};

enum tool{
	tool_unknown=0,
	caget=1,
	camon=2,
	caput=3,
	caputq=4,
	cagets=5,
	cado=6,
	cawait=7,
	cainfo=8
};

struct arguments
{
   float w;					//ca timeout
   int  d;					//dbr request type
   bool num;    			//same as -int
   enum roundType round;	//type of rounding:default, ceil, floor
   int prec;				//precision
   bool hex;				//display as hex
   bool bin;				//display as bin
   bool oct;				//display as oct
   bool plain;				//ignore formatting switches
   int digits;  			// precision for e, f, g option
   char dblFormatStr[30]; 	// Format string to print doubles (see -e -f -g option)
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
		.w = CA_DEFAULT_TIMEOUT,
		.d = -1,	//decide type based on requested data
		.num = false,
		.round = roundType_no_rounding,
		.prec = -1,	//use precision from the record
		.hex = false,
		.bin = false,
		.oct = false,
		.plain = false,
		.digits = -1,
		.dblFormatStr = "%g",
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
	gt =1,
	gte=2,
	lt =3,
	lte=4,
	eq =5,
	neq=6,
	in =7,
	out=8
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
    long            type;   	// dbr type
    long            count;  	// element count
    long            inNelm;  	// requested number of elements for writing
    long            outNelm;  	// requested number of elements for reading
    bool            done;   	// indicates if callback has finished processing this channel
    int 			i;			// process value id
    char			*writeStr;	// value(s) to be written
    enum operator	conditionOperator; //cawait operator
    double 			conditionOperands[2]; //cawait operands
};


//output strings
static int const LEN_TIMESTAMP = 50;
static int const LEN_RECORD_NAME = 61;
static int const LEN_VALUE = 100;
static int const LEN_SEVSTAT = 30;
static int const LEN_UNITS = 20+MAX_UNITS_SIZE;
char **outDate,**outTime, **outSev, **outStat, **outUnits, **outValue, **outLocalDate, **outLocalTime;
char **outTimestamp; //relative timestamps for camon
char ***outEnumStrs;  //for camon intitialization
unsigned int *numEnumStrs;

//timestamps needed by -timestamp
epicsTimeStamp *timestampRead;		//timestamps of received data (per channel)
epicsTimeStamp programStartTime;	//timestamp indicating program start
epicsTimeStamp *lastUpdate;			//timestamp indicating last update per channel
bool *firstUpdate;					//indicates that lastUpdate has not been initialized
epicsTimeStamp timeoutTime;			//when to stop monitoring (-timeout)

bool runMonitor;					//indicates when to stop monitoring according to -timeout or -n
unsigned int numMonitorUpdates;		//counts updates needed by -n


//templates for reading requested data
#define severity_status_get(T) \
status = ((struct T *)args.dbr)->status; \
severity = ((struct T *)args.dbr)->severity;
#define timestamp_get(T) \
	timestampRead[ch->i] = ((struct T *)args.dbr)->stamp;\
	validateTimestamp(timestampRead[ch->i], ch->name);
#define units_get_cb(T) sprintf(outUnits[ch->i], "%s", ((struct T *)args.dbr)->units);
#define units_get_mon(T) sprintf(outUnits[channel.i], "%s", ((struct T *)data)->units);
#define precision_get(T) precision = (((struct T *)args.dbr)->precision);

void usage(FILE *stream, enum tool tool, char *programName){//TODO channel->pv?

	//usage:
	if (tool == caget || tool == cagets || tool == camon || tool == cado ){
		fprintf(stream, "\nUsage: %s [flags] <channel> [<channel> ...]\n", programName);
	}
	else if (tool == cawait){
		fprintf(stream, "\nUsage: %s [flags] <channel> <condition> [<channel> <condition> ...]\n", programName);
	}
	else if (tool == caput || tool == caputq  ){
		fprintf(stream, "\nUsage: %s [flags] <channel> <value> [<channel> <value> ...]\n", programName);
	}
	else if (tool == cainfo  ){
		fprintf(stream, "\nUsage: %s <channel> [<channel> ...]\n", programName);
	}
	else { //tool unknown
		fprintf(stream, "\nUnknown tool. Try %s -tool with any of the following arguments: caget, caput, cagets, "\
				"caputq, cado, camon, cawait, cainfo.\n", programName);
		return;
	}

	//descriptions
	fputs("\n",stream);

	if (tool == caget ){
		fputs("Reads values from channel(s).\n", stream);
	}
	if (tool == cagets){
		fputs("First writes 1 to PROC fields of the provided channels. Then reads the values.\n", stream);
	}
	if (tool == caput ){
		fputs("Writes value(s) to channels(s). Then reads the updated values and displays them.\n", stream);
		fputs("Arrays can be handled either by specifying number of elements as an argument to -inNelm option"\
				" or grouping the elements to write in a string following pv name. E.g. the following two commands produce the same"\
				" result, namely write 1, 2 and 3 into record pvA and 4, 5, 6 into pvB:\n", stream);
		fputs("caput -inNelm 3 pvA 1 2 3 pvB 4 5 6 \ncaput pvA '1 2 3' pvB '4 5 6'\n", stream);
		fputs("The following tries to write '1 2 3', 'pvB' and '4 5 6' into pvA record:\n", stream);
		fputs("caput -inNelm 3 pvA '1 2 3' pvB '4 5 6'\n", stream);
	}
	if (tool == caputq ){
		fputs("Writes value(s) to channels(s), then exits. Does not have any output (except if an error occurs).\n", stream);
		fputs("Arrays can be handled either by specifying number of elements as an argument to -inNelm option"\
				" or grouping the elements to write in a string following pv name. E.g. the following two commands produce the same"\
				" result, namely write 1, 2 and 3 into record pvA and 4, 5, 6 into pvB:\n", stream);
		fputs("caputq -inNelm 3 pvA 1 2 3 pvB 4 5 6 \ncaputq pvA '1 2 3' pvB '4 5 6'\n", stream);
		fputs("The following tries to write '1 2 3', 'pvB' and '4 5 6' into pvA record:\n", stream);
		fputs("caputq -inNelm 3 pvA '1 2 3' pvB '4 5 6'\n", stream);
	}
	if (tool == cado ){
		fputs("First writes 1 to PROC fields of the provided channels, then exits. Does not have any output (except if an error occurs).\n", stream);
	}
	if (tool == camon ){
		fputs("Monitors the provided channels.\n", stream);
	}
	if (tool == cawait){
		fputs("Monitors the channels, but only displays values when they match the provided conditions. The conditions are specified as a"\
				" string containing the operator together with the values.\n", stream);
		fputs("The following operators are supported:  >,<,<=,>=,==,!=, ==A...B(in interval), !=A...B(out of interval). For example, "\
				"cawait pv '==1...5' ignores all pv values except those inside the interval [1,5].\n", stream);
	}
	if (tool == cainfo  ){
		fputs("Displays detailed information about the provided channels.\n", stream);
		return; //does not have any flags
	}


	//flags
	fputs("\n\n", stream);
	fputs("Accepted flags:\n", stream);

	//common for all tools (except cainfo)
	fputs("-d <type>             type of DBR request to use for communicating with the server\n", stream);
	fputs("-w <number>           timeout for CA calls\n", stream);

	//flags associated with reading
	if (tool == caget || tool == cagets || tool == camon \
			|| tool == cawait ||  tool == caput ){
		fputs("-num                  display enum/char values as numbers\n", stream);
		fputs("-int                  same as -num\n", stream);
		fputs("-round <type>         rounding of floating point values. Arguments: ceil, floor, default.\n", stream);
		fputs("-prec <number>        precision for displaying floating point values. Default is equal to PREC field of the record.\n", stream);
		fputs("-hex                  display integer values in hexadecimal format\n", stream);
		fputs("-oct                  display integer values in octal format\n", stream);
		fputs("-plain                ignore formatting switches\n", stream);
		fputs("-stat                 always display alarm status and severity\n", stream);
		fputs("-nostat               never display alarm status and severity\n", stream);
		fputs("-noname               hide record name\n", stream);
		fputs("-date                 display server date\n", stream);
		fputs("-localdate            display client date\n", stream);
		fputs("-time                 display server time\n", stream);
		fputs("-localtime            display client time\n", stream);
		fputs("-outNelm  <number>    number of array elements to read\n", stream);
		fputs("-outFs <number>       field separator for displaying array elements\n", stream);
		fputs("-nord                 display number of array elements before their values\n", stream);
		fputs("-w <number>           timeout for CA calls\n", stream);
		fputs("-d                    same as -date\n", stream);													//XXX XXX
		fputs("-e <number>           format doubles using scientific notation with precition <number>. Overrides -prec option.\n", stream);
		fputs("-f <number>           format doubles using floating point with precition <number>. Overrides -prec option.\n", stream);
		fputs("-g <number>           format doubles using shortest representation with precition <number>. Overrides -prec option.\n", stream);
		fputs("-s                    interpret values as strings\n", stream);
		fputs("-t                    same as -time\n", stream);

		if (tool == camon){
			fputs("-timestamp <char>     display relative timestamps. <char> = r displays time elapsed since start " \
					"of the program. <char> = u displays time elapsed since last update of any channel.  <char> = c displays "\
					"time elapsed since last update separately for each channel\n", stream);
			fputs("-n <number>           exit the program after <number> updates\n", stream);
		}
		if (tool == cawait){
			fputs("-timeout <number>     exit the program after <number> seconds without an update\n", stream);
		}

	}

	//flags associated with writing
	if (tool == caput || tool == caputq ){
		fputs("-inNelm               number of array elements to write. Needs to match the number of provided values.\n", stream);
		fputs("-inFs                 field separator used in the string containing array elements to write\n", stream);

	}
}

void dumpArguments(struct arguments *args){
    printf("\n"
           "CA timeout:\tw = %f\n"
           "Requested DBR type:\td = %d\n"
           "Try to interpret values as numbers:\tnum = %d\n"
           "Round number to int:\tround = %d\n"
           "Precision:\tprec = %d\n"
           "Digits:\tdigit = %d\n"
           "Number as hex:\thex = %d\n"
           "Number as binary:\tbin = %d\n"
           "Number as octal:\toct = %d\n"
           "Ignore formatting switches on read:\tplain = %d\n"
           "Try to interpret values as strings:\ts = %d\n"
           "Double formatting:\ts = %s\n"
           "Status and severity:\ts = %d\n"
           "Display channel name:\ts = %d\n"
           "Display units:\ts = %d\n"
           "\n",
           args->w, args->d, args->num, (int)args->round, args->prec, args->digits,
           args->hex, args->bin, args->oct, args->plain, args->s, args->dblFormatStr,
           args->stat, args->noname, args->nounit);
}



bool checkStatus(int status){
    if (status != ECA_NORMAL){
        fprintf(stderr, "CA error %s occurred while trying "
                "to start channel access.\n", ca_message(status));
        SEVCHK(status, "CA error");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}


bool cawaitParseCondition(struct channel *channel, char *argumentConditionStr){
//parses input string and extracts operators and numerals,
//then saves them to the channel structure.
//Supported operators: >,<,<=,>=,==,!=, ==A...B(in interval), !=A...B(out of interval).

	int i,j;				//counters
	bool interval=false;	//indicates if interval is desired

	//create a local copy of condition string for parsing
	char *conditionStr = callocMustSucceed(strlen(argumentConditionStr), sizeof(char), "cawaitParseCondition\n");

	//strip spaces, and replace '...' with ':'
	for (i=0, j=0; i<strlen(argumentConditionStr); ++i){
		if (argumentConditionStr[i] != ' '){
			if (argumentConditionStr[i] == '.' && argumentConditionStr[i+1] == '.' && argumentConditionStr[i+2] == '.'){
				conditionStr[j++] = ':';
				i += 2;
				interval=true;
				continue;
			}
			conditionStr[j++] = argumentConditionStr[i];
		}
	}
	if (conditionStr[0] == '\0'){
		fprintf(stderr,"Empty condition string.\n");
		return EXIT_FAILURE;
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

		if (strcmp(operator, ">")==0)       channel->conditionOperator = gt;
		else if (strcmp(operator, "<")==0)  channel->conditionOperator = lt;
		else if (strcmp(operator, ">=")==0) channel->conditionOperator = gte;
		else if (strcmp(operator, "<=")==0) channel->conditionOperator = lte;
		else if (strcmp(operator, "==")==0)	channel->conditionOperator = eq;
		else if (strcmp(operator, "!=")==0) channel->conditionOperator = neq;
		else{
			fprintf(stderr, "Problem interpreting condition string: %s. Unknown operator %s.", conditionStr, operator);
			return EXIT_FAILURE;
		}
	}
	else if (nParsed == 3 && interval == true){

		channel->conditionOperands[0] = operand1;
		channel->conditionOperands[1] = operand2;

		if (strcmp(operator, "==")==0) 		channel->conditionOperator = in;
		else if (strcmp(operator, "!=")==0) channel->conditionOperator = out;
		else{
			fprintf(stderr, "Problem interpreting condition string: %s. Only operators == and != are supported if the"\
					"value is to be compared to an interval.",conditionStr);
			return EXIT_FAILURE;
		}
	}
	else{
		fprintf(stderr, "Problem interpreting condition string: %s. Wrong number of operands.", conditionStr);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
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
		dblValue = (double) *(double*)nativeValue;
		break;
	case DBR_FLOAT:
		dblValue = (double) *(float*)nativeValue;
		break;
	case DBR_INT:
		dblValue = (double) *(int16_t*)nativeValue;
		break;
	case DBR_LONG:
		dblValue = (double) *(int32_t*)nativeValue;
		break;
	case DBR_CHAR:
	case DBR_STRING:
	case DBR_ENUM:
		if (sscanf(nativeValue, "%lf", &dblValue) <= 0){
			fprintf(stderr, "Channel %s value %s cannot be converted to double.", channel.name, (char*)nativeValue);
			return -1;
		}
		break;
	default:
		fprintf(stderr, "Unrecognized DBR type.\n");
		return -1;
		break;
	}

	//evaluate and exit
	switch (channel.conditionOperator){
	case gt:
		return dblValue > channel.conditionOperands[0];
	case gte:
		return dblValue >= channel.conditionOperands[0];
	case lt:
		return dblValue < channel.conditionOperands[0];
	case lte:
		return dblValue <= channel.conditionOperands[0];
	case eq:
		return dblValue == channel.conditionOperands[0];
	case neq:
		return dblValue != channel.conditionOperands[0];
	case in:
		return (dblValue >= channel.conditionOperands[0]) && (dblValue <= channel.conditionOperands[1]);
	case out:
		return !((dblValue >= channel.conditionOperands[0]) && (dblValue <= channel.conditionOperands[1]));
	}

	return -1;
}




#define printBits(x) \
	size_t int_size = sizeof(x) * 8; \
	int i;\
	for (i = int_size-1; i >= 0; i--) { \
		printf("%c", '0' + ((x >> i) & 1) ); \
	}

int printValue(int i, evargs args, int precision){
//Parses the data fetched by ca_get callback according to the data type and formatting arguments.
//The result is printed to stdout.

    unsigned int baseType;
    void *value;

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
    	case DBR_INT:{
    		int16_t valueInt16 = ((dbr_int_t*)value)[j];
    		//display dec, hex, bin, oct if desired
    		if (arguments.hex){
    			printf("%" PRIx16, (uint16_t)valueInt16);
    		}
    		else if (arguments.oct){
    			printf("%" PRIo16, (uint16_t)valueInt16);
    		}
    		else if (arguments.bin){
    			printBits((uint16_t)valueInt16);
    		}
    		else{
    			printf("%" PRId16, valueInt16);
    		}
    		break;
    	}
		//case DBR_SHORT: equals DBR_INT
    	case DBR_FLOAT:
    	case DBR_DOUBLE:{
    		double valueDbl;
    		if (baseType == DBR_FLOAT) valueDbl = ((dbr_float_t*)value)[j];
    		else valueDbl = ((dbr_double_t*)value)[j];

    		//round if desired
    		if (arguments.round == roundType_default) valueDbl = round(valueDbl);
    		else if (arguments.round == roundType_ceil) valueDbl = ceil(valueDbl);
    		else if (arguments.round == roundType_floor) valueDbl = floor(valueDbl);

    		if (arguments.digits == -1 && arguments.prec == -1){
    			//display with precision defined by the record
    			printf("%-.*g", precision, valueDbl);
    		}
			else{
				//use precision and format as specified by -efg or -prec arguments
				printf(arguments.dblFormatStr, valueDbl);
			}
//XXX g is hardcoded because of the way options have been set up. -e -f and -g require an argument specifying precision,
// therefore it is not possible to specify an arbitrary format combined with precision from the record. I asked Tom and
// he said that options have been previously set up in the code in order to ensure compatibility with previous tools, so
// I left this alone.
    		break;
    	}
    	case DBR_ENUM: {
    	    dbr_enum_t v = ((dbr_enum_t *)value)[j];
    		if (v >= MAX_ENUM_STATES){
    			fprintf(stderr, "Illegal enum index '%d'.\n", v);
    			break;
    		}


    		if (!arguments.num && !arguments.bin && !arguments.hex && !arguments.oct) { // if not requested as a number check if we can get string

        		//if monitor we reuse strings from before
        		bool monitor = (arguments.tool == camon || arguments.tool == cawait);
        		if (monitor){
    				if (v >= numEnumStrs[i]) {
    					fprintf(stderr, "Enum index value '%d' greater than the number of strings.\n", v);
    				}
    				else{
    					printf("%*s", MAX_ENUM_STRING_SIZE, outEnumStrs[i][v]);
    				}
    				break;
        		}
        		else if (dbr_type_is_GR(args.type)) {
    				if (v >= ((struct dbr_gr_enum *)args.dbr)->no_str) {
    					fprintf(stderr, "Enum index value '%d' greater than the number of strings.\n", v);
    				}
    				else{
    					printf("%*s", MAX_ENUM_STRING_SIZE, ((struct dbr_gr_enum *)args.dbr)->strs[v]);
    				}
    				break;
    			}
    			else if (dbr_type_is_CTRL(args.type)) {
    				if (v >= ((struct dbr_ctrl_enum *)args.dbr)->no_str) {
    					fprintf(stderr, "Enum index value '%d' greater than the number of strings.\n", v);
    				}
    				else{
    					printf("%*s", MAX_ENUM_STRING_SIZE, ((struct dbr_ctrl_enum *)args.dbr)->strs[v]);
    				}
    				break;
    			}
    		}
    		// else return string value as number.
    		if (arguments.hex){
    			printf("%" PRIx16, v);
    		}
    		else if (arguments.oct){
    			printf("%" PRIo16, v);
    		}
    		else if (arguments.bin){
    			printBits(v);
    		}
    		else{
    			printf("%" PRId16, v);
    		}
    		break;
    	}
    	case DBR_CHAR:{
    		char valueChr = ((dbr_char_t*) value)[j];
    		if (!arguments.num) {	//check if requested as a a number
    			printf("%c", valueChr);
    		}
    		else{
    			printf("%d", valueChr);
    		}
    		break;
    	}
    	case DBR_LONG:{
    		int32_t valueInt32 = ((dbr_long_t*)value)[j];
    		//display dec, hex, bin, oct if desired
    		if (arguments.hex){
    			printf("%" PRIx32, (uint32_t)valueInt32);
    		}
    		else if (arguments.oct){
    			printf("%" PRIo32, (uint32_t)valueInt32);
    		}
    		else if (arguments.bin){
    			printBits((uint16_t)valueInt32);
    		}
    		else{
    			printf("%" PRId32, valueInt32);
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
	if (outDate[i][0] != '\0')	printf("%s ",outDate[i]);
	//server time
	if (outTime[i][0] != '\0')	printf("%s ",outTime[i]);

	if (doubleTime){
		fputs("local time: ",stdout);
	}

	//local date
	if (outLocalDate[i][0] != '\0')	printf("%s ",outLocalDate[i]);
	//local time
	if (outLocalDate[i][0] != '\0')	printf("%s ",outLocalTime[i]);


    //timestamp if monitor and if requested
    if (arguments.tool == camon && arguments.timestamp)	printf("%s ", outTimestamp[i]);

    //channel name
    if (!arguments.noname)	printf("%s ",((struct channel *)args.usr)->name);

    //value(s)
    printValue(i, args, precision);

	fputc(' ',stdout);

    //egu
    if (outUnits[i][0] != '\0') printf("%s ",outUnits[i]);

    //severity
    if (outSev[i][0] != '\0') printf("%s ",outSev[i]);

    //status
	if (outStat[i][0] != '\0') printf("%s ",outStat[i]);

	fputc('\n',stdout);
    return 0;
}



static void caReadCallback2 (evargs args){
//callback function which serves to provide additional data in cases where two ca_get requests are
//needed in order to obtain all necessary information. This function is called executed if either units
//are missing from the first callback, or enum value received is of wrong type (string/number).


	//check if data was returned
	if (args.status != ECA_NORMAL){
		fprintf(stderr, "CA error %s occurred while trying to start channel access.\n", ca_message(args.status));
		return;
	}
    struct channel *ch = (struct channel *)args.usr; 	//the channel to which this callback belongs

    //if we got here, we probably don't have units, so we need to extract them now.
    //precision needs to be extracted again because it is not global.
    int precision=-1;
    switch (args.type) {
    case DBR_GR_SHORT:    // and DBR_GR_INT
    	units_get_cb(dbr_gr_short);
    	break;

    case DBR_GR_FLOAT:
    	units_get_cb(dbr_gr_float);
    	precision_get(dbr_gr_float);
    	break;

    case DBR_GR_CHAR:
    	units_get_cb(dbr_gr_char);
    	break;

    case DBR_GR_LONG:
    	units_get_cb(dbr_gr_long);
    	break;

    case DBR_GR_DOUBLE:
    	units_get_cb(dbr_gr_double);
    	precision_get(dbr_gr_double);
    	break;

    }


	//finish
	ch->done = true;
	printOutput(ch->i, args, precision);
}



bool epicsTimeDiffFull(epicsTimeStamp *diff, const epicsTimeStamp *pLeft, const epicsTimeStamp *pRight){
// Calculates difference between two epicsTimeStamps: like epicsTimeDiffInSeconds but returning the answer
//in form of a timestamp. The absolute value of the difference pLeft - pRight is saved to the timestamp diff, and the
//returned value indicates if the said difference is negative.
	bool negative = epicsTimeLessThan(pLeft, pRight);
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



void validateTimestamp(epicsTimeStamp timestamp, char* name){
//checks a timestamp for illegal values.
	if (timestamp.nsec >= 1000000000ul){
		fprintf(stderr,"Warning: invalid number of nanoseconds in timestamp: %s - assuming 0.\n",name);
		timestamp.nsec=0;
	}
}

int getTimeStamp(int i){
//calculates timestamp for monitor tool, formats it and saves it into the global string.

	epicsTimeStamp elapsed;
	bool negative=false;
	int commonI = (arguments.timestamp == 'c') ? i : 0;
	bool showEmpty = false;

	if (arguments.timestamp == 'r') {
		//calculate elapsed time since startTime
		negative = epicsTimeDiffFull(&elapsed, &timestampRead[i], &programStartTime);
	}
	else if(arguments.timestamp == 'c' || arguments.timestamp == 'u'){
		//calculate elapsed time since last update; if using 'c' keep
		//timestamps separate for each channel, otherwise use lastUpdate[0]
		//for all channels (commonI).

		negative = epicsTimeDiffFull(&elapsed, &timestampRead[i], &lastUpdate[commonI]);

		lastUpdate[commonI] = timestampRead[i]; // reset

		if (!firstUpdate[commonI]){
			firstUpdate[commonI] = true;
			showEmpty = true;
		}
	}

	//convert to h,m,s,ns
	struct tm tm;
	unsigned long nsec;
	int status = epicsTimeToGMTM(&tm, &nsec, &elapsed);
	assert(status == epicsTimeOK);

	if (arguments.timestamp != 'r' && showEmpty){
		//this is the first update for this channel
		sprintf(outTimestamp[i],"%19c",' ');
	}
	else{	//save to outTs string
		char cSign = negative ? '-' : ' ';
		sprintf(outTimestamp[i],"%c%02d:%02d:%02d.%09lu", cSign,tm.tm_hour, tm.tm_min, tm.tm_sec, nsec);
	}

	return 0;
}





static void caReadCallback (evargs args){
//reads and parses data fetched by calls. First, the global strings holding the output are cleared. Then, depending
//on the type of the returned data, the available information is extracted. The extracted info is then saved to the
//global strings. The actual value of the PV is an exception; its reading, parsing, and storing are performed by
//calling valueToString or enumToString. Note that the latter is actually a callback function for another get request.

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
    unsigned int status=0, severity=0;


	//clear global output strings; the purpose of this callback is to overwrite them
    outDate[ch->i][0]='\0';
    outSev[ch->i][0]='\0';
    outStat[ch->i][0]='\0';
    outValue[ch->i][0]='\0';
    outLocalDate[ch->i][0]='\0';
    outLocalTime[ch->i][0]='\0';
    if(!monitor) outUnits[ch->i][0]='\0';



    //read requested data
    switch (args.type) {
        case DBR_STS_STRING:
            severity_status_get(dbr_sts_string);
            break;

        case DBR_STS_SHORT: //and INT
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

        case DBR_TIME_SHORT:
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

        case DBR_GR_SHORT:    // and DBR_GR_INT
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
            precision = (int)(((struct dbr_ctrl_float *)args.dbr)->precision);
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
            printf("Can not print %s DBR type. \n", dbr_type_to_text(args.type));
            break;
    }


    //do formating

    //check alarm limits
    if (status <= lastEpicsAlarmCond) sprintf(outStat[ch->i], "%s", epicsAlarmConditionStrings[status]);
    else sprintf(outStat[ch->i],"UNKNOWN STATUS: %d",status);

    if (severity <= lastEpicsAlarmSev) sprintf(outSev[ch->i], "%s", epicsAlarmSeverityStrings[severity]);
    else sprintf(outSev[ch->i],"UNKNOWN SEVERITY: %d",status);

    //hide alarms?
    if (arguments.stat){//always display
    	//do nothing
    }
    if (arguments.nostat){//never display
    	outSev[ch->i][0] = '\0';
    	outStat[ch->i][0] = '\0';
    }
    else{//display if alarm
    	if (status == 0 && severity == 0){
        	outSev[ch->i][0] = '\0';
        	outStat[ch->i][0] = '\0';
    	}
    }

   	//time
    if (args.type >= DBR_TIME_STRING && args.type <= DBR_TIME_DOUBLE){//otherwise we don't have it
    	//we don't assume manually specifying dbr_time implies -time or -date.
    	if (arguments.date) epicsTimeToStrftime(outDate[ch->i], LEN_TIMESTAMP, "%Y-%m-%d", &timestampRead[ch->i]);
    	if (arguments.time) epicsTimeToStrftime(outTime[ch->i], LEN_TIMESTAMP, "%H:%M:%S.%06f", &timestampRead[ch->i]);
    }


	//show local date or time?
	if (arguments.localdate || arguments.localtime){
		time_t localTime = time(0);
		if (arguments.localdate) strftime(outLocalDate[ch->i], LEN_TIMESTAMP, "%Y-%m-%d", localtime(&localTime));
		if (arguments.localtime) strftime(outLocalTime[ch->i], LEN_TIMESTAMP, "%H:%M:%S", localtime(&localTime));
	}

    //hide units?
    if (arguments.nounit) outUnits[ch->i][0] = '\0';


	//if monitor, there is extra stuff to do before printing data out.
	if (arguments.tool == cawait || arguments.tool == camon){
		if (arguments.timestamp) getTimeStamp(ch->i);	//calculate relative timestamps

		numMonitorUpdates++;	//increase counter of updates

		//check stop conditions for -n (camon)
		if (arguments.tool == camon && arguments.numUpdates != -1 && numMonitorUpdates >= arguments.numUpdates){
			runMonitor = false;
		}

		if (arguments.tool == cawait){
			//check stop condition
			if (arguments.timeout!=-1){
				epicsTimeStamp timeoutNow;
				epicsTimeGetCurrent(&timeoutNow);
				validateTimestamp(timeoutNow, "timeout");
				if (epicsTimeGreaterThanEqual(&timeoutNow, &timeoutTime)){
					//we are done waiting
					printf("No updates for %f seconds - exiting.\n",arguments.timeout);
					runMonitor = false;
					return;
				}
			}

			//check display condition
			if (cawaitEvaluateCondition(*ch, args) != 1){
				return;
			}
			else if(arguments.timeout!=-1){ // if condition matches, set new timeout and proceed
				epicsTimeGetCurrent(&timeoutTime);
				validateTimestamp(timeoutTime, "timeout time");

				epicsTimeAddSeconds(&timeoutTime, arguments.timeout);
			}

		}


	}


    //print output, make another ca request if not all info is available (except if DBR type was requested by the user)
	if (args.type <= DBR_TIME_DOUBLE && !arguments.nounit && !monitor && arguments.d == -1){
		//special case 3: time was requested which does not have units info. Another request is needed to get it.
		unsigned int baseType = args.type % (LAST_TYPE+1);
		status = ca_array_get_callback(dbf_type_to_DBR_GR(baseType), ch->outNelm, ch->id, caReadCallback2, ch);
		if (status != ECA_NORMAL) fprintf(stderr, "CA error %s occurred while trying to create ca_get request.\n", ca_message(status));

		status = ca_flush_io();
		if(status != ECA_NORMAL) fprintf (stderr,"Problem flushing channel %s.\n", ch->name);
		//ch->done and printing is set by callback2.
	}
	else{ //printf output as usual
		printOutput(ch->i, args, precision);
		//finish
		ch->done = true;
	}
}

static void caWriteCallback (evargs args){
//does nothing except signal that writing is finished.

	//check if status is ok
	if (args.status != ECA_NORMAL){
		fprintf(stderr, "Error in write callback. %s.\n", ca_message(args.status));
		return;
	}

    struct channel *ch = (struct channel *)args.usr;

    ch->done = true;
}

int getStaticMonitorData(struct channel channel){
//gets units and enum strings for channel i. Saves them to global strings for later reuse.

	unsigned int baseType = channel.type % (LAST_TYPE+1);

	//first check if this call is needed at all
	if (baseType == DBF_ENUM || (!arguments.nounit && (arguments.time || arguments.date || arguments.timestamp))) {
		// do nothing but proceed if this is enum, or if units are needed along with any of the time info.
	}
	else return 0;


	int reqType = dbf_type_to_DBR_GR(baseType);

	void *data;
	data = mallocMustSucceed(dbr_size_n(reqType, channel.count), "getStaticMonitorData");

	int status = ca_array_get(reqType, channel.count, channel.id, data);
	if (status != ECA_NORMAL) fprintf(stderr, "CA error %s occurred while trying to create ca_get "\
			"request for record %s.\n", ca_message(status), channel.name);

	status = ca_pend_io ( arguments.w );
	if(status == ECA_TIMEOUT) printf ("Cannot obtain units or enum strings of channel %s.\n", channel.name);


	if (baseType == DBF_ENUM){
		//get enum strings
		int j;
		for (j=0; j<MAX_ENUM_STATES; ++j){
			strcpy(outEnumStrs[channel.i][j], ((struct dbr_gr_enum *)data)->strs[j]);
		}
		numEnumStrs[channel.i] = ((struct dbr_gr_enum *)data)->no_str;
	}
	else{//get units
		switch(reqType){
		case DBR_GR_SHORT:
			units_get_mon(dbr_gr_short);
			break;
		case DBR_GR_CHAR:
			units_get_mon(dbr_gr_float);
			break;
		case DBR_GR_LONG:
			units_get_mon(dbr_gr_long);
			break;
		case DBR_GR_DOUBLE:
			units_get_mon(dbr_gr_double);
			break;
		default:
			fprintf(stderr, "Units for channel %s cannot be displayed.\n", channel.name);
			break;
		}
	}

	free(data);
	return 0;
}




void caRequest(struct channel *channels, int nChannels){
//sends get or put requests for all tools except cainfo. ca_get or ca_put are called multiple times, depending
//on the tool. The reading and parsing of returned data is performed in callbacks. Once the callbacks finish,
//the the data is printed. If the tool type is monitor, a loop is entered in which the data is printed repeatedly.
    int status=-1, i, j;

    for(i=0; i < nChannels; i++){
        channels[i].count = ca_element_count ( channels[i].id );

        if (arguments.nord){
        	printf("# of elements in %s: %lu\n", channels[i].name, channels[i].count);
        }

        //how many array elements to request
        if (arguments.outNelm == -1){
        	channels[i].outNelm = channels[i].count;
        }
        else if(arguments.outNelm > 0 && arguments.outNelm < channels[i].count){
        	channels[i].outNelm = arguments.outNelm;
        }
        else{
        	channels[i].outNelm = channels[i].count;
        	fprintf(stderr, "Invalid number %d of requested elements to read.", arguments.outNelm);
        }



        channels[i].done = false;

        if (arguments.d == -1){   //if DBR type not specified, use native and decide based on desired level of detail.

        	if (arguments.time || arguments.date || arguments.timestamp){
        		if (arguments.s && ca_field_type(channels[i].id) == DBF_ENUM){
        			channels[i].type = DBR_TIME_STRING;
        		}
            	channels[i].type = dbf_type_to_DBR_TIME(ca_field_type(channels[i].id));
        	}
        	else{	//default: GR
        		channels[i].type = dbf_type_to_DBR_GR(ca_field_type(channels[i].id));
        		//since there is no gr_string, use sts_string instead
        		if (channels[i].type == DBR_GR_STRING){
        			channels[i].type = DBR_STS_STRING;
        		}
        	}
        }
        else{
        	channels[i].type = arguments.d;
        }

        if (arguments.tool == caget){
			status = ca_array_get_callback(channels[i].type, channels[i].outNelm, channels[i].id, caReadCallback, &channels[i]);
			if (status != ECA_NORMAL) fprintf(stderr, "CA error %s occurred while trying to create ca_get request.\n", ca_message(status));
		}
        else if (arguments.tool == camon || arguments.tool == cawait){
        	//get presumably static data first
        	getStaticMonitorData(channels[i]);

			status = ca_create_subscription(channels[i].type, channels[i].outNelm, channels[i].id, DBE_VALUE | DBE_ALARM | DBE_LOG,\
					caReadCallback, &channels[i], NULL);
			if (status != ECA_NORMAL) fprintf(stderr, "CA error %s occurred while trying to create monitor.\n", ca_message(status));
	    	runMonitor = true;
	    	if (arguments.tool == cawait && arguments.timeout!=-1){
	    		//set first timeout
	    		epicsTimeGetCurrent(&timeoutTime);
	    		validateTimestamp(timeoutTime, "timeout");
				epicsTimeAddSeconds(&timeoutTime, arguments.timeout);
	    	}
        }
        else if (arguments.tool == caput || arguments.tool == caputq){
            long nakedType = channels[i].type % (LAST_TYPE+1);   // use naked dbr_xx type for put

            //input will be one of these
        	int inputI[channels[i].inNelm];
        	float inputF[channels[i].inNelm];
        	char inputC[channels[i].inNelm];
        	long inputL[channels[i].inNelm];
        	double inputD[channels[i].inNelm];


            //convert input string to the nakedType
        	status = ECA_BADTYPE;
            switch (nakedType){
            case DBR_STRING:
    			status = ca_array_put_callback(nakedType, channels[i].inNelm, channels[i].id, channels[i].writeStr, caWriteCallback, &channels[i]);
    			break;
            case DBR_INT://and short
            	for (j=0;j<channels[i].inNelm; ++j){
					if (sscanf(&channels[i].writeStr[j*MAX_STRING_SIZE],"%d",&inputI[j]) <= 0) {
						fprintf(stderr, "Impossible to convert input %s to format %s\n",&channels[i].writeStr[j*MAX_STRING_SIZE], dbr_type_to_text(nakedType));
					}
            	}
            	status = ca_array_put_callback(nakedType, channels[i].inNelm, channels[i].id, inputI, caWriteCallback, &channels[i]);
            	break;
            case DBR_FLOAT:
            	for (j=0;j<channels[i].inNelm; ++j){
            		if (sscanf(&channels[i].writeStr[j*MAX_STRING_SIZE],"%f",&inputF[j]) <= 0) {
            			fprintf(stderr, "Impossible to convert input %s to format %s\n",&channels[i].writeStr[j*MAX_STRING_SIZE], dbr_type_to_text(nakedType));
            		}
            	}
            	status = ca_array_put_callback(nakedType, channels[i].inNelm, channels[i].id, inputF, caWriteCallback, &channels[i]);
            	break;
            case DBR_ENUM:
				//check if put data is provided as a number
				if (sscanf(&channels[i].writeStr[0],"%d",&inputI[0])){
					for (j=1;j<channels[i].inNelm; ++j){
						if (sscanf(&channels[i].writeStr[j*MAX_STRING_SIZE],"%d",&inputI[j]) <= 0){
							fprintf(stderr, "String '%s' cannot be converted to enum index.\n", &channels[i].writeStr[j*MAX_STRING_SIZE]);
						}
            			if (inputI[j]>15) fprintf(stderr, "Warning: enum index value '%d' may be too large.\n", inputI[j]);
            		}
	            	status = ca_array_put_callback(nakedType, channels[i].inNelm, channels[i].id, inputI, caWriteCallback, &channels[i]);
            	}
            	else{
            		//send as a string
            		status = ca_array_put_callback(DBR_STRING, channels[i].inNelm, channels[i].id, channels[i].writeStr, caWriteCallback, &channels[i]);
            	}
            	break;
            case DBR_CHAR:
            	for (j=0;j<channels[i].inNelm; ++j){
            		if (sscanf(&channels[i].writeStr[j],"%c",&inputC[j]) <= 0) {
            			fprintf(stderr, "Impossible to convert input %s to format %s\n",channels[i].writeStr, dbr_type_to_text(nakedType));
            		}
            	}
            	status = ca_array_put_callback(nakedType, channels[i].inNelm, channels[i].id, inputC, caWriteCallback, &channels[i]);
            	break;
            case DBR_LONG:
            	for (j=0;j<channels[i].inNelm; ++j){
            		if (sscanf(&channels[i].writeStr[j*MAX_STRING_SIZE],"%li",&inputL[j]) <= 0) {
            			fprintf(stderr, "Impossible to convert input %s to format %s\n",&channels[i].writeStr[j*MAX_STRING_SIZE], dbr_type_to_text(nakedType));
            		}
            	}
            	status = ca_array_put_callback(nakedType, channels[i].inNelm, channels[i].id, inputL, caWriteCallback, &channels[i]);
            	break;
            case DBR_DOUBLE:
            	for (j=0;j<channels[i].inNelm; ++j){
            		if (sscanf(&channels[i].writeStr[j*MAX_STRING_SIZE],"%lf",&inputD[j]) <= 0) {
            			fprintf(stderr, "Impossible to convert input %s to format %s\n",&channels[i].writeStr[j*MAX_STRING_SIZE], dbr_type_to_text(nakedType));
            		}
            	}
            	status = ca_array_put_callback(nakedType, channels[i].inNelm, channels[i].id, inputD, caWriteCallback, &channels[i]);
            	break;
            }
			if (status != ECA_NORMAL) fprintf(stderr, "CA error %s occurred while trying to create a put request.\n", ca_message(status));


        }
        else if(arguments.tool == cagets || arguments.tool == cado){

        	int inputI=1;
        	status = ca_array_put_callback(DBF_CHAR, 1, channels[i].procId, &inputI, caWriteCallback, &channels[i]);
        }

    }

    float elapsed = 0;
    while (elapsed <=arguments.w){
    	status = ca_pend_event (0.1);
    	elapsed += 0.1;
    	bool allDone=true;
    	for (i=0; i<nChannels; ++i) allDone *= channels[i].done;
    	if (allDone) break;
    }
    if (status != ECA_TIMEOUT) fprintf (stderr,"Some of the callbacks have not completed.\n");


    //if caput or cagets, wait for put callback and issue a new read request.
    if (arguments.tool == caput || arguments.tool == cagets){

    	for (i=0; i<nChannels; ++i){
    		if (channels[i].done){
    			status = ca_array_get_callback(channels[i].type, channels[i].count, channels[i].id, caReadCallback, &channels[i]);
    			if (status != ECA_NORMAL) fprintf(stderr, "CA error %s occurred while trying to create ca_get request.\n", ca_message(status));
    		}
    		else{
    			fprintf(stderr, "Put callback for channel %s did not complete.\n", channels[i].name);
    		}
    	}

    	//wait for read callbacks
        while (elapsed <=arguments.w){
        	status = ca_pend_event (0.1);
        	elapsed += 0.1;
        	bool allDone=true;
        	for (i=0; i<nChannels; ++i) allDone *= channels[i].done;
        	if (allDone) break;
        }
        if (status != ECA_TIMEOUT) fprintf (stderr,"Some of the callbacks have not completed.\n");

    }

    //enter monitor loop
    if (arguments.tool == camon || arguments.tool == cawait){

        if(arguments.tool == cawait) {for (i=0; i<nChannels;++i) firstUpdate[i]=false;}

    	while (runMonitor){
    		ca_pend_event(0.1);

    		if(arguments.tool == cawait && arguments.timeout!=-1){ //check stop condition
    			epicsTimeStamp timeoutNow;
    			epicsTimeGetCurrent(&timeoutNow);
    			validateTimestamp(timeoutNow, "timeout");
    			if (epicsTimeGreaterThanEqual(&timeoutNow, &timeoutTime)){
    				if (runMonitor) printf("No updates for %f seconds - exiting.\n",arguments.timeout);
    				break;
    			}
    		}
    	}
    }
}

void cainfoRequest(struct channel *channels, int nChannels){
//this function does all the work for caInfo tool. Reads channel data using ca_get and then prints.
	int i,j, status;

	bool readAccess, writeAccess;


	for(i=0; i < nChannels; i++){
		channels[i].count = ca_element_count ( channels[i].id );

		channels[i].type = dbf_type_to_DBR_CTRL(ca_field_type(channels[i].id));

		void *data, *dataDesc, *dataHhsv, *dataHsv, *dataLsv, *dataLlsv;
		data = mallocMustSucceed(dbr_size_n(channels[i].type, channels[i].count), "caInfoRequest");
		dataDesc = mallocMustSucceed(dbr_size_n(channels[i].type, 1), "caInfoRequest");
		dataHhsv = mallocMustSucceed(dbr_size_n(channels[i].type, 1), "caInfoRequest");
		dataHsv = mallocMustSucceed(dbr_size_n(channels[i].type, 1), "caInfoRequest");
		dataLsv = mallocMustSucceed(dbr_size_n(channels[i].type, 1), "caInfoRequest");
		dataLlsv = mallocMustSucceed(dbr_size_n(channels[i].type, 1), "caInfoRequest");



		//general ctrl data
		status = ca_array_get(channels[i].type, channels[i].count, channels[i].id, data);
		if (status != ECA_NORMAL) fprintf(stderr, "CA error %s occurred while trying to create ca_get request for record %s.\n", ca_message(status), channels[i].name);

		//desc
		status = ca_array_get(DBR_STS_STRING, 1, channels[i].descId, dataDesc);
		if (status != ECA_NORMAL) fprintf(stderr, "CA error %s occurred while trying to read "\
				"DESC field of record %s.\n", ca_message(status), channels[i].name);

		//hhsv
		status = ca_array_get(DBR_STS_STRING, 1, channels[i].hhsvId, dataHhsv);
		if (status != ECA_NORMAL) fprintf(stderr, "CA error %s occurred while trying to read "\
				"HHSV field of record %s.\n", ca_message(status),channels[i].name);

		//hsv
		status = ca_array_get(DBR_STS_STRING, 1, channels[i].hsvId, dataHsv);
		if (status != ECA_NORMAL) fprintf(stderr, "CA error %s occurred while trying to read "\
				"HSV field of record %s.\n", ca_message(status),channels[i].name);

		//lsv
		status = ca_array_get(DBR_STS_STRING, 1, channels[i].lsvId, dataLsv);
		if (status != ECA_NORMAL) fprintf(stderr, "CA error %s occurred while trying to read "\
				"LSV field of record %s.\n", ca_message(status),channels[i].name);

		//llsv
		status = ca_array_get(DBR_STS_STRING, 1, channels[i].llsvId, dataLlsv);
		if (status != ECA_NORMAL) fprintf(stderr, "CA error %s occurred while trying to read "\
				"LLSV field of record %s.\n", ca_message(status),channels[i].name);


		ca_pend_io(arguments.w);


		fputc('\n',stdout);
		fputc('\n',stdout);
		printf("%s\n", channels[i].name);														//name
		printf("%s\n", ((struct dbr_sts_string *)dataDesc)->value);								//description
		printf("\n");
		printf("    Native DBF type: %s\n", dbf_type_to_text(ca_field_type(channels[i].id)));	//field type
		printf("    Number of elements: %ld\n", channels[i].count);								//number of elements


		switch (channels[i].type){
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
		printf("	HIHI alarm severity: %s\n", ((struct dbr_sts_string *)dataHhsv)->value);		//severities
		printf("	HIGH alarm severity: %s\n", ((struct dbr_sts_string *)dataHsv)->value);
		printf("	LOW alarm severity: %s\n", ((struct dbr_sts_string *)dataLsv)->value);
		printf("	LOLO alarm severity: %s\n", ((struct dbr_sts_string *)dataLlsv)->value);
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
	}


}


int caInit(struct channel *channels, int nChannels){
//creates contexts and channels.
    int status, i;

    //status = ca_context_create(ca_enable_preemptive_callback);
    status = ca_context_create(ca_disable_preemptive_callback);

    if (checkStatus(status) != EXIT_SUCCESS) return EXIT_FAILURE;

    for(i=0; i < nChannels; i++){
        status = ca_create_channel(channels[i].name, 0 , 0, CA_PRIORITY, &channels[i].id);
        if(checkStatus(status)) return EXIT_FAILURE;    // we exit if the channels are not created

        //if tool = cagets or cado, each channel has a sibling connecting to the proc field
        if (arguments.tool == cagets || arguments.tool == cado){
        	status = ca_create_channel(channels[i].procName, 0 , 0, CA_PRIORITY, &channels[i].procId);
        	if(checkStatus(status)) return EXIT_FAILURE;
        }
        else if (arguments.tool == cainfo){
        	//if tool = cainfo, each channel has siblings connecting to the desc and *sv fields
        	//we flush each connecting to each field in order to detect missing fields.
        	status = ca_pend_io ( arguments.w );
        	if(status == ECA_TIMEOUT){
        		printf ("Cannot connect to channel %s.\n", channels[i].name);
        		break;
        	}

        	status = ca_create_channel(channels[i].descName, 0 , 0, CA_PRIORITY, &channels[i].descId);
        	if(checkStatus(status)) return EXIT_FAILURE;
        	status = ca_pend_io ( arguments.w );
        	if(status == ECA_TIMEOUT) printf ("Channel %s is missing DESC field.\n", channels[i].name);

        	status = ca_create_channel(channels[i].hhsvName, 0 , 0, CA_PRIORITY, &channels[i].hhsvId);
        	if(checkStatus(status)) return EXIT_FAILURE;
        	status = ca_pend_io ( arguments.w );
        	if(status == ECA_TIMEOUT) printf ("Channel %s is missing HHSV field.\n", channels[i].name);

        	status = ca_create_channel(channels[i].hsvName, 0 , 0, CA_PRIORITY, &channels[i].hsvId);
        	if(checkStatus(status) != EXIT_SUCCESS) return EXIT_FAILURE;
        	status = ca_pend_io ( arguments.w );
        	if(status == ECA_TIMEOUT) printf ("Channel %s is missing HSV field.\n", channels[i].name);

        	status = ca_create_channel(channels[i].lsvName, 0 , 0, CA_PRIORITY, &channels[i].lsvId);
        	if(checkStatus(status)) return EXIT_FAILURE;
        	status = ca_pend_io ( arguments.w );
        	if(status == ECA_TIMEOUT) printf ("Channel %s is missing LSV field.\n", channels[i].name);

        	status = ca_create_channel(channels[i].llsvName, 0 , 0, CA_PRIORITY, &channels[i].llsvId);
        	if(checkStatus(status)) return EXIT_FAILURE;
        	status = ca_pend_io ( arguments.w );
        	if(status == ECA_TIMEOUT) printf ("Channel %s is missing LLSV field.\n", channels[i].name);
        }
    }

    status = ca_pend_io ( arguments.w ); //wait for channels to connect
    if(status == ECA_TIMEOUT){
        printf ("Some of the channels did not connect\n");
    }

    return EXIT_SUCCESS;
}

int caDisconnect(struct channel * channels, int nChannels){
    int status, i;
    int exitStatus = EXIT_SUCCESS;

    for(i=0; i < nChannels; i++){
        status = ca_clear_channel(channels[i].id);
        if( checkStatus(status) != EXIT_SUCCESS) exitStatus = EXIT_FAILURE;
    }
    return exitStatus;
}


int main ( int argc, char ** argv )
{//main function: reads arguments, allocates memory, calls ca* functions, frees memory and exits.

    int opt;                    // getopt() current option
    int opt_long;               // getopt_long() current long option
    int nChannels=0;              // Number of channels
    int i,j;                      // counter
    struct channel *channels;


    //attempt to recognize tool from path/tool by matching ends of argv[0]
    if (!strcmp(argv[0] + (strlen(argv[0])-strlen("caget")), "caget")) arguments.tool = caget;
    else if (!strcmp(argv[0] + (strlen(argv[0])-strlen("caput")), "caput")) arguments.tool = caput;
    else if (!strcmp(argv[0] + (strlen(argv[0])-strlen("cagets")), "cagets")) arguments.tool = cagets;
    else if (!strcmp(argv[0] + (strlen(argv[0])-strlen("caputq")), "caputq")) arguments.tool = caputq;
    else if (!strcmp(argv[0] + (strlen(argv[0])-strlen("camon")), "camon")) arguments.tool = camon;
    else if (!strcmp(argv[0] + (strlen(argv[0])-strlen("cawait")), "cawait")) arguments.tool = cawait;
    else if (!strcmp(argv[0] + (strlen(argv[0])-strlen("cado")), "cado")) arguments.tool = cado;
    else if (!strcmp(argv[0] + (strlen(argv[0])-strlen("cainfo")), "cainfo")) arguments.tool = cainfo;

    static struct option long_options[] = {
        {"num",     	no_argument,        0,  0 },
        {"int",     	no_argument,        0,  0 },    //same as num
        {"round",   	required_argument,  0,  0 },	//type of rounding:default, ceil, floor
        {"prec",    	required_argument,  0,  0 },	//precision
        {"hex",     	no_argument,        0,  0 },	//display as hex
        {"bin",     	no_argument,        0,  0 },	//display as bin
        {"oct",     	no_argument,        0,  0 },	//display as oct
        {"plain",   	no_argument,        0,  0 },	//ignore formatting switches
        {"stat",    	no_argument, 		0,  0 },	//status and severity on
        {"nostat",  	no_argument,  		0,  0 },	//status and severity off
        {"noname",  	no_argument,        0,  0 },	//hide name
        {"nounit",  	no_argument,        0,  0 },	//hide units
        {"timestamp",	required_argument, 	0,  0 },	//timestamp type ruc
        {"localdate",	no_argument, 		0,  0 },	//client date
        {"time",		no_argument, 		0,  0 },	//server time
        {"localtime",	no_argument, 		0,  0 },	//client time
        {"date",		no_argument, 		0,  0 },	//server date
        {"inNelm",		required_argument,	0,  0 },	//number of array elements - write
        {"outNelm",		required_argument,	0,  0 },	//number of array elements - read
        {"outFs",		required_argument,	0,  0 },	//array field separator - read
        {"inFs",		required_argument,	0,  0 },	//array field separator - write
        {"nord",		no_argument,		0,  0 },	//display number of array elements
        {"tool",		required_argument, 	0,	0 },	//tool
        {"timeout",		required_argument, 	0,	0 },	//timeout
        {0,         	0,                 	0,  0 }
    };
    putenv("POSIXLY_CORRECT="); //Behave correctly on GNU getopt systems = stop parsing after 1st non option is encountered
    while ((opt = getopt_long_only(argc, argv, ":w:d:e:f:g:n:sth", long_options, &opt_long)) != -1) {
        switch (opt) {
        case 'w':
        	if (sscanf(optarg, "%f", &arguments.w) != 1){    // type was not given as float
        		arguments.w = CA_DEFAULT_TIMEOUT;
        		fprintf(stderr, "Requested timeout invalid - ignored. ('%s -h' for help.)\n", argv[0]);
        	}
            break;
        case 'd':
        	if (sscanf(optarg, "%d", &arguments.d) != 1)     // type was not given as an integer
        	{
        		dbr_text_to_type(optarg, arguments.d);
        		if (arguments.d == -1)                   // Invalid? Try prefix DBR_
        		{
        			char str[30] = "DBR_";
        			strncat(str, optarg, 25);
        			dbr_text_to_type(str, arguments.d);
        		}
        	}
        	if (arguments.d < DBR_STRING    || arguments.d > DBR_CTRL_DOUBLE){
        		fprintf(stderr, "Requested dbr type out of range "
        				"or invalid - ignored. ('%s -h' for help.)\n", argv[0]);
        		arguments.d = -1;
        	}
        	break;
        case 'e':	//how to format doubles in strings
        case 'f':
        case 'g':
            if (sscanf(optarg, "%d", &arguments.digits) != 1){
                fprintf(stderr,
                        "Invalid precision argument '%s' "
                        "for option '-%c' - ignored.\n", optarg, opt);
            } else {
                if (arguments.digits>=0)
                    sprintf(arguments.dblFormatStr, "%%-.%d%c", arguments.digits, opt);
                else
                    fprintf(stderr, "Precision %d for option '-%c' "
                            "out of range - ignored.\n", arguments.digits, opt);
                arguments.digits = -1;
            }
            break;
        case 's':	//try to interpret values as strings
            arguments.s = true;
            break;
        case 't':	//show server time
        	arguments.time = true;
        	break;
        case 'n':	//stop monitor after numUpdates
        	arguments.numUpdates = 1;
			if (sscanf(optarg, "%d", &arguments.numUpdates) != 1){
                fprintf(stderr, "Invalid argument '%s' for option '-%c' - ignored.\n", optarg, opt);
            } else
            {
                if (arguments.numUpdates < 1) {
                    fprintf(stderr, "Number of updates for option '-%c' must greater than zero - ignored.\n", opt);
                    arguments.numUpdates = -1;
                }
            }
        	break;
        case 0:   // long options
            switch (opt_long){
            case 0:   // num
                arguments.num = true;
                break;
            case 1:   // int
                arguments.num = true;
                break;
            case 2:   // round
            	;//declaration must not follow label
                int type;
                if (sscanf(optarg, "%d", &type) != 1){   // type was not given as a number [0, 1, 2]
                    if(!strcmp("default", optarg)){
                        arguments.round = roundType_default;
                    } else if(!strcmp("ceil", optarg)){
                        arguments.round = roundType_ceil;
                    } else if(!strcmp("floor", optarg)){
                        arguments.round = roundType_floor;
                    } else {
                        arguments.round = roundType_default;
                        fprintf(stderr,
                            "Invalid round type '%s' "
                            "for option '-%c' - ignored.\n", optarg, opt);
                    }
                } else{ // type was given as a number
                    if(roundType_floor < type || type < roundType_default){   // out of range check
                        arguments.round = roundType_no_rounding;
                        fprintf(stderr,
                            "Invalid round type '%s' "
                            "for option '-%c' - ignored.\n", optarg, opt);
                    } else{
                    	arguments.round = type;
                    }
                }
                break;
            case 3:   // prec
                if (sscanf(optarg, "%d", &arguments.prec) != 1){
                    fprintf(stderr,
                            "Invalid precision argument '%s' "
                            "for option '-%c' - ignored.\n", optarg, opt);
                } else {
                	if (arguments.prec < 0) {
                		fprintf(stderr, "Precision %d for option '-%c' "
                				"out of range - ignored.\n", arguments.prec, opt);
                		arguments.prec = -1;
                	}
                	else{ //write to dblFormatStr, keeping the format type
                		char formatType = arguments.dblFormatStr[strlen(arguments.dblFormatStr)];
                        sprintf(arguments.dblFormatStr, "%%-.%d%c", arguments.prec, formatType);
                	}
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
            				"for option '-%c' - ignored. Allowed arguments: r,u,c.\n", optarg, opt);
            		arguments.timestamp = 0;
            	} else {
            		if (arguments.timestamp != 'r' && arguments.timestamp != 'u' && arguments.timestamp != 'c') {
            			fprintf(stderr,	"Invalid argument '%s' "
							"for option '-%c' - ignored. Allowed arguments: r,u,c.\n", optarg, opt);
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
            				"for option '-%c' - ignored.\n", optarg, opt);
            	}
            	else {
            		if (arguments.inNelm < 1) {
            			fprintf(stderr, "Count number for option '-%c' "
            					"must be positive integer - ignored.\n", opt);
            			arguments.inNelm = 1;
            		}
            	}
            	break;
            case 18:   // outNelm - number of elements - read
            	if (sscanf(optarg, "%d", &arguments.outNelm) != 1){
            		fprintf(stderr, "Invalid count argument '%s' "
            				"for option '-%c' - ignored.\n", optarg, opt);
            	}
            	else {
            		if (arguments.outNelm < 1) {
            			fprintf(stderr, "Count number for option '-%c' "
            					"must be positive integer - ignored.\n", opt);
            			arguments.outNelm = -1;
            		}
            	}
            	break;
            case 19:   // field separator for output
            	if (sscanf(optarg, "%c", &arguments.fieldSeparator) != 1){
            		fprintf(stderr, "Invalid argument '%s' "
            				"for option '-%c' - ignored.\n", optarg, opt);
            	}
            	break;
            case 20:   // field separator for input
            	if (sscanf(optarg, "%c", &arguments.inputSeparator) != 1){
            		fprintf(stderr, "Invalid argument '%s' "
            				"for option '-%c' - ignored.\n", optarg, opt);
            	}
            	break;
            case 21:   // nord
            	arguments.nord = true;
            	break;
            case 22:	// tool
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
            case 23:
            	if (sscanf(optarg, "%f", &arguments.timeout) != 1){
            		fprintf(stderr, "Invalid timeout argument '%s' "
            				"for option '-%c' - ignored.\n", optarg, opt);
            	}
            	else {
            		if (arguments.timeout <= 0) {
            			fprintf(stderr, "Timeout argument must be positive - ignored.\n");
            			arguments.timeout = -1;
            		}
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
    if (arguments.tool != camon){
    	if (arguments.timestamp != 0){
    		fprintf(stderr, "Option -timestamp can only be specified with camon.\n");
    	}
    	if (arguments.numUpdates != -1){
    		fprintf(stderr, "Option -n can only be specified with camon.\n");
    	}
    }
    if (arguments.inNelm != 1 && (arguments.tool != caput && arguments.tool != caputq)){
		fprintf(stderr, "Option -inNelm can only be specified with caput or caputq.\n");
    }
    if ((arguments.outNelm != -1 || arguments.num !=false || arguments.hex !=false || arguments.bin !=false || arguments.oct !=false)\
    		&& (arguments.tool == cado || arguments.tool == caputq)){
    	fprintf(stderr, "Option -outNelm, -num, -hex, -bin and -oct cannot be specified with cado or caputq, because they have no output.\n");
    }
    if (arguments.nostat != false || arguments.stat != false){
    	fprintf(stderr, "Options -stat and -nostat are mutually exclusive.\n");
    	arguments.nostat = false;
    }
    if (((arguments.hex || arguments.bin) && arguments.oct) || \
    		((arguments.oct || arguments.bin) && arguments.hex) || \
    		((arguments.hex || arguments.oct) && arguments.bin) ){
    	fprintf(stderr, "Options -hex and -bin and -oct are mutually exclusive.\n");
    }
    if (arguments.num && arguments.s){
    	fprintf(stderr, "Options -num and -s are mutually exclusive.\n");
    }
    if (arguments.digits != -1 && arguments.prec != -1){
    	fprintf(stderr, "Precision specified twice: by -prec as well as argument to -e, -f or -g.\n");
	}
    if (arguments.plain && (arguments.num || arguments.hex ||arguments.bin||arguments.oct || arguments.digits != -1 || arguments.prec != -1 \
    		|| arguments.s || arguments.round != roundType_no_rounding || strcmp(arguments.dblFormatStr,"%g") || arguments.fieldSeparator != ' ' )){
    	printf("Warning: -plain option overrides all formatting switches.\n");
    	arguments.num =false; arguments.hex =false; arguments.bin = false; arguments.oct =false; arguments.s =false;
    	arguments.digits = -1; arguments.prec = -1;
    	arguments.round = roundType_no_rounding;
    	strcpy(arguments.dblFormatStr,"%g");
    	arguments.fieldSeparator = ' ' ;
    }
    if (arguments.tool == tool_unknown){
    	usage(stderr, arguments.tool,argv[0]);
    	return EXIT_FAILURE;
    }

	epicsTimeGetCurrent(&programStartTime);
	validateTimestamp(programStartTime, "Start time");

	//Remaining arg list refers to PVs
	if (arguments.tool == caget || arguments.tool == camon || arguments.tool == cagets || arguments.tool == cado || arguments.tool == cainfo){
		nChannels = argc - optind;       // Remaining arg list are PV names
	}
	else if (arguments.tool == caput || arguments.tool == caputq){
		//default case, it takes (inNelm+pv name) elements to define each channel
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
        fprintf(stderr, "No channel name specified. ('%s -h' for help.)\n", argv[0]);
        return EXIT_FAILURE;
    }

    //allocate memory for channel structures
    channels = (struct channel *) callocMustSucceed (nChannels, sizeof(struct channel), "main");
    if (!channels) {
    	fprintf(stderr, "Memory allocation for channel structures failed.\n");
    	return EXIT_FAILURE;
    }
    for (i=0;i<nChannels;++i){
    	channels[i].procName = callocMustSucceed (LEN_RECORD_NAME + 5, sizeof(char), "main");//5 spaces for .(field name)
    	channels[i].descName = callocMustSucceed (LEN_RECORD_NAME + 5, sizeof(char), "main");
    	channels[i].hhsvName = callocMustSucceed (LEN_RECORD_NAME + 5, sizeof(char), "main");
    	channels[i].hsvName = callocMustSucceed (LEN_RECORD_NAME + 5, sizeof(char), "main");
    	channels[i].lsvName = callocMustSucceed (LEN_RECORD_NAME + 5, sizeof(char), "main");
    	channels[i].llsvName = callocMustSucceed (LEN_RECORD_NAME + 5, sizeof(char), "main");
        //name and condition strings dont have to be allocated because they merely point somewhere else
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
        	}
        	else{ //next argument is a string of values separated by inputSeparator
        		//count the number of values in it (= number of separators +1)
        		int count;
        		//char *str = arg;
        		for (j=0, count=0; argv[optind+1][j]; j++) {
        			count += (argv[optind+1][j] == arguments.inputSeparator);
        		}
        		count+=1;
        		//and use it to allocate the string
        		channels[i].writeStr = callocMustSucceed (count *MAX_STRING_SIZE, sizeof(char), "main");
        		if (!channels[i].writeStr ) {
        			fprintf(stderr, "Memory allocation for channel structures failed.\n");
        			return EXIT_FAILURE;
        		}

        		if (count == 1){//the argument to write consists of just a single element.
					int charsToWrite = snprintf(&channels[i].writeStr[0], MAX_STRING_SIZE, "%s", argv[optind+1]);
					if (charsToWrite >= MAX_STRING_SIZE) fprintf(stderr,"Input %s is longer than the allowed size %d - truncating.\n", argv[optind+1], MAX_STRING_SIZE);
					channels[i].inNelm = 1;
        		}
        		else{//parse the string assuming each element is delimited by the inputSeparator char
        			char inFs[2] = {arguments.inputSeparator, 0};
        			j=0;
        			char *token = strtok(argv[optind+1], inFs);
        			while(token) {
        				int charsToWrite = snprintf(&channels[i].writeStr[j*MAX_STRING_SIZE] , MAX_STRING_SIZE, "%s", token);
        				if (charsToWrite >= MAX_STRING_SIZE) fprintf(stderr,"Input %s is longer than the allowed size %d - truncating.\n", token, MAX_STRING_SIZE);
        				j++;
        				token = strtok(NULL, inFs);
        			}
        			channels[i].inNelm = j;
        		}
        		//finally advance to the next argument
        		optind++;
        	}
        }
        else if (arguments.tool == cawait){
        	//next argument is the condition string
        	if (cawaitParseCondition(&channels[i], argv[optind+1])) return EXIT_FAILURE;
        	optind++;
        }
        else if(arguments.tool == cagets || arguments.tool == cado){

        	size_t lenName = strlen(channels[i].name);
        	assert (lenName < LEN_RECORD_NAME + 4); //worst case scenario: longest name + .VAL

        	strcpy(channels[i].procName, channels[i].name);

        	//append .PROC
        	if (lenName > 4 && strcmp(&channels[i].procName[lenName - 4], ".VAL") == 0){
        		//if last 4 elements equal .VAL, replace them
        		strcpy(&channels[i].procName[lenName - 4],".PROC");
        	}
        	else{
        		//otherwise simply append
        		strcat(&channels[i].procName[lenName],".PROC");
        	}
        }
        else if(arguments.tool == cainfo){
        	size_t lenName = strlen(channels[i].name);
        	assert (lenName < LEN_RECORD_NAME + 4);//worst case scenario: longest name + .VAL

        	strcpy(channels[i].descName, channels[i].name);
        	strcpy(channels[i].hhsvName, channels[i].name);
        	strcpy(channels[i].hsvName, channels[i].name);
        	strcpy(channels[i].lsvName, channels[i].name);
        	strcpy(channels[i].llsvName, channels[i].name);

        	//append .DESC, .HHSV, .HSV, .LSV, .LLSV
        	if (lenName > 4 && strcmp(&channels[i].name[lenName- 4], ".VAL") == 0){
        		//if last 4 elements equal .VAL, replace them
        		strcpy(&channels[i].descName[lenName - 4],".DESC");
        		strcpy(&channels[i].hhsvName[lenName - 4],".HHSV");
        		strcpy(&channels[i].hsvName[lenName - 4],".HSV");
        		strcpy(&channels[i].lsvName[lenName - 4],".LSV");
        		strcpy(&channels[i].llsvName[lenName - 4],".LLSV");
        	}
        	else{
        		//otherwise simply append
        		strcat(&channels[i].descName[strlen(channels[i].descName)],".DESC");
        		strcat(&channels[i].hhsvName[strlen(channels[i].hhsvName)],".HHSV");
        		strcat(&channels[i].hsvName[strlen(channels[i].hsvName)],".HSV");
        		strcat(&channels[i].lsvName[strlen(channels[i].lsvName)],".LSV");
        		strcat(&channels[i].llsvName[strlen(channels[i].llsvName)],".LLSV");

        	}

        }
    }


    //dumpArguments(&arguments);


    //allocate memory for output strings
	outValue = mallocMustSucceed(nChannels * sizeof(char *),"main");
	outDate = mallocMustSucceed(nChannels * sizeof(char *),"main");
	outTime = mallocMustSucceed(nChannels * sizeof(char *),"main");
	outSev = mallocMustSucceed(nChannels * sizeof(char *),"main");
	outStat = mallocMustSucceed(nChannels * sizeof(char *),"main");
	outUnits = mallocMustSucceed(nChannels * sizeof(char *),"main");
	outTimestamp = mallocMustSucceed(nChannels * sizeof(char *),"main");
	outLocalDate = mallocMustSucceed(nChannels * sizeof(char *),"main");
	outLocalTime = mallocMustSucceed(nChannels * sizeof(char *),"main");
	outEnumStrs = mallocMustSucceed(nChannels * sizeof(char **),"main");
	numEnumStrs = mallocMustSucceed(nChannels * sizeof(unsigned int),"main");
	for(i = 0; i < nChannels; i++){
		outValue[i] = mallocMustSucceed(LEN_VALUE * sizeof(char),"main");
		outDate[i] = mallocMustSucceed(LEN_TIMESTAMP * sizeof(char),"main");
		outTime[i] = mallocMustSucceed(LEN_TIMESTAMP * sizeof(char),"main");
		outSev[i] = mallocMustSucceed(LEN_SEVSTAT * sizeof(char),"main");
		outStat[i] = mallocMustSucceed(LEN_SEVSTAT * sizeof(char),"main");
		outUnits[i] = mallocMustSucceed(LEN_UNITS * sizeof(char),"main");
		outTimestamp[i] = mallocMustSucceed(LEN_TIMESTAMP * sizeof(char),"main");
		outLocalDate[i] = mallocMustSucceed(LEN_TIMESTAMP * sizeof(char),"main");
		outLocalTime[i] = mallocMustSucceed(LEN_TIMESTAMP * sizeof(char),"main");
		outEnumStrs[i] = mallocMustSucceed(MAX_ENUM_STATES * sizeof(char *),"main");
		for (j=0; j < MAX_ENUM_STATES; ++j){
			outEnumStrs[i][j] = mallocMustSucceed(MAX_ENUM_STRING_SIZE * sizeof(char),"main");
		}
	}
	//memory for timestamp
	timestampRead = mallocMustSucceed(nChannels * sizeof(epicsTimeStamp),"main");
	lastUpdate = mallocMustSucceed(nChannels * sizeof(epicsTimeStamp),"main");
	firstUpdate = mallocMustSucceed(nChannels * sizeof(bool),"main");

    if(caInit(channels, nChannels) != EXIT_SUCCESS) return EXIT_FAILURE;

    if (arguments.tool == cainfo){
    	cainfoRequest(channels, nChannels);
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
    }
    free(channels);

    //free output strings
    for(i = 0; i < nChannels; i++){
    	free(outValue[i]);
    	free(outDate[i]);
    	free(outTime[i]);
    	free(outSev[i]);
    	free(outStat[i]);
    	free(outUnits[i]);
    	free(outTimestamp[i]);
    	free(outLocalDate[i]);
    	free(outLocalTime[i]);
    	for (j=0 ; j< MAX_ENUM_STATES; ++j)	free(outEnumStrs[i][j]);
    	free(outEnumStrs[i]);
    }
    free(outValue);
    free(outDate);
    free(outTime);
    free(outSev);
    free(outStat);
    free(outUnits);
    free(outTimestamp);
    free(outLocalDate);
    free(outLocalTime);
	free(outEnumStrs);

    //free monitor's timestamp
    free(timestampRead);
    free(lastUpdate);
    free(firstUpdate);


    return EXIT_SUCCESS;
}
