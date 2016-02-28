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
		fputs("-w <number>           timeout for CA calls\n", stream);
		return; //does not have any other flags
	}


	//flags
	fputs("\n\n", stream);
	fputs("Accepted flags:\n", stream);

	//common for all tools
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


static inline void strClear(char *str){
//clears input string str
	str[0] = '\0';
}


static bool strEmpty(char *str){
//returns true is the input string is empty
	return str[0] == '\0';
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
    					printf("%*s", MAX_ENUM_STRING_SIZE, ((struct dbr_gr_enum *)args.dbr)->strs[v]);
    				}
    				break;
    			}
    			else if (dbr_type_is_CTRL(args.type)) {
    				if (v >= ((struct dbr_ctrl_enum *)args.dbr)->no_str) {
    					printf("Enum index value %d greater than the number of strings", v);
    				}
    				else{
    					printf("%*s", MAX_ENUM_STRING_SIZE, ((struct dbr_ctrl_enum *)args.dbr)->strs[v]);
    				}
    				break;
    			}
    		}
    		// else write value as number.
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
    		if (arguments.num) {	//check if requested as a a number
    			printf("%" PRId8, valueChr);
    		}
    		else{
    			printf("%c", valueChr);
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
	if (!strEmpty(outDate[i]))	printf("%s ",outDate[i]);
	//server time
	if (!strEmpty(outTime[i]))	printf("%s ",outTime[i]);

	if (doubleTime){
		fputs("local time: ",stdout);
	}

	//local date
	if (!strEmpty(outLocalDate[i]))	printf("%s ",outLocalDate[i]);
	//local time
	if (!strEmpty(outLocalTime[i]))	printf("%s ",outLocalTime[i]);


    //timestamp if monitor and if requested
    if (arguments.tool == camon && arguments.timestamp)	printf("%s ", outTimestamp[i]);

    //channel name
    if (!arguments.noname)	printf("%s ",((struct channel *)args.usr)->name);

    //value(s)
    printValue(i, args, precision);

	fputc(' ',stdout);

    //egu
    if (!strEmpty(outUnits[i])) printf("%s ",outUnits[i]);

    //severity
    if (!strEmpty(outSev[i])) printf("%s ",outSev[i]);

    //status
	if (!strEmpty(outStat[i])) printf("%s ",outStat[i]);

	fputc('\n',stdout);
    return 0;
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
    unsigned int status=0, severity=0;

	//clear global output strings; the purpose of this callback is to overwrite them
    //the exception are units, which we may be getting from elsewhere; we only clear them if we can write them
    strClear(outDate[ch->i]);
    strClear(outSev[ch->i]);
    strClear(outStat[ch->i]);
    strClear(outLocalDate[ch->i]);
    strClear(outLocalTime[ch->i]);
    if (args.type >= DBR_GR_STRING){
    	strClear(outUnits[ch->i]);
    }

    //read requested data
    switch (args.type) {
    	case DBR_GR_STRING:
    	case DBR_CTRL_STRING:
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
    	strClear(outSev[ch->i]);
    	strClear(outStat[ch->i]);
    }
    else{//display if alarm
    	if (status == 0 && severity == 0){
        	strClear(outSev[ch->i]);
        	strClear(outStat[ch->i]);
    	}
    }

   	//time
    if (args.type >= DBR_TIME_STRING && args.type <= DBR_TIME_DOUBLE){//otherwise we don't have it
    	//we don't assume that manually specifying dbr_time implies -time or -date.
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
    if (arguments.nounit) strClear(outUnits[ch->i]);


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


	//finish
	printOutput(ch->i, args, precision);
	ch->done = true;

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

static void getStaticUnitsCallback (evargs args){
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
    case DBR_GR_SHORT:
    	units_get_cb(dbr_gr_short);
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
    	fprintf(stderr, "Units for channel %s cannot be displayed.\n", ch->name);
    	break;
    }

}

void monitorLoop (struct channel *channels, int nChannels){
//runs monitor loop. Stops after -n updates (camon) or after -timeout
//is exceeded (cawait).

	int i;

	if(arguments.tool == cawait) {
		//initialization for timestamps
		for (i=0; i<nChannels;++i) firstUpdate[i]=false;
	}

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


void caRequest(struct channel *channels, int nChannels){
//sends get or put requests. ca_get or ca_put are called multiple times, depending on the tool. The reading,
//parsing and printing of returned data is performed in callbacks.
    int status=-1, i, j;


    for(i=0; i < nChannels; i++){

    	if (!channels[i].firstConnection) {//skip disconnected channels
    		channels[i].done = true;
    		continue;
    	}

    	if (arguments.tool == caget){
			status = ca_array_get_callback(channels[i].type, channels[i].outNelm, channels[i].id, caReadCallback, &channels[i]);
			if (status != ECA_NORMAL) fprintf(stderr, "Problem creating get request for channel %s: %s.\n", channels[i].name,ca_message(status));
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
            	int inputI[channels[i].inNelm];
            	for (j=0;j<channels[i].inNelm; ++j){
					if (sscanf(&channels[i].writeStr[j*MAX_STRING_SIZE],"%d",&inputI[j]) <= 0) {
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
            	int inputI[channels[i].inNelm];
				if (sscanf(&channels[i].writeStr[0],"%d",&inputI[0])){
					for (j=0;j<channels[i].inNelm; ++j){
						if (sscanf(&channels[i].writeStr[j*MAX_STRING_SIZE],"%d",&inputI[j]) <= 0){
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
            	long inputL[channels[i].inNelm];
            	for (j=0;j<channels[i].inNelm; ++j){
            		if (sscanf(&channels[i].writeStr[j*MAX_STRING_SIZE],"%li",&inputL[j]) <= 0) {
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
		if (status != ECA_NORMAL) fprintf(stderr, "Problem creating put request for channel %s: %s.\n", channels[i].name,ca_message(status));
    }

    // wait for callbacks
    float elapsed = 0;
    while (elapsed <=arguments.w){
    	status = ca_pend_event (0.1);
    	elapsed += 0.1;
    	bool allDone=true;
    	for (i=0; i<nChannels; ++i) allDone *= (channels[i].done || !channels[i].firstConnection);
    	if (allDone) break;
    }
    if (status != ECA_TIMEOUT) fprintf (stderr,"Some of the callbacks have not completed.\n");


    //if caput or cagets issue a new read request.
    if (arguments.tool == caput || arguments.tool == cagets){

    	for (i=0; i<nChannels; ++i){
        	if (!channels[i].firstConnection) {//skip disconnected channels
        		channels[i].done = true;
        		continue;
        	}
    		if (channels[i].done){
    			status = ca_array_get_callback(channels[i].type, channels[i].count, channels[i].id, caReadCallback, &channels[i]);
    			if (status != ECA_NORMAL) fprintf(stderr, "Problem creating get request for channel %s: %s.\n", channels[i].name,ca_message(status));
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
        	for (i=0; i<nChannels; ++i) allDone *= (channels[i].done || !channels[i].firstConnection);
        	if (allDone) break;
        }
        if (status != ECA_TIMEOUT) fprintf (stderr,"Some of the read callbacks have not completed.\n");

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
		status = ca_array_get(channels[i].type , channels[i].count, channels[i].id, data);
		if (status != ECA_NORMAL) fprintf(stderr, "CA error %s occurred while trying to create ca_get request for record %s.\n", ca_message(status), channels[i].name);

		//desc
		status = ca_array_get(DBR_STS_STRING, 1, channels[i].descId, dataDesc);
		if (status != ECA_NORMAL) fprintf(stderr, "Problem reading DESC field of record %s: %s.\n", channels[i].name, ca_message(status));

		//standard severity fields
		if (!strEmpty(channels[i].hhsvName) && channels[i].hhsvId){
			status = ca_array_get(DBR_STS_STRING, 1, channels[i].hhsvId, dataHhsv);
			if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
		}
		if (!strEmpty(channels[i].hsvName) && channels[i].hsvId){
			status = ca_array_get(DBR_STS_STRING, 1, channels[i].hsvId, dataHsv);
			if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
		}
		if (!strEmpty(channels[i].lsvName) && channels[i].lsvId){
			status = ca_array_get(DBR_STS_STRING, 1, channels[i].lsvId, dataLsv);
			if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
		}
		if (!strEmpty(channels[i].llsvName) && channels[i].llsvId){
			status = ca_array_get(DBR_STS_STRING, 1, channels[i].llsvId, dataLlsv);
			if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
		}
		//multi-bit severity fields
		if (!strEmpty(channels[i].unsvName) && channels[i].unsvId){
			status = ca_array_get(DBR_STS_STRING, 1, channels[i].unsvId, dataUnsv);
			if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
		}
		if (!strEmpty(channels[i].cosvName) && channels[i].cosvId){
			status = ca_array_get(DBR_STS_STRING, 1, channels[i].cosvId, dataCosv);
			if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
		}
		if (!strEmpty(channels[i].zrsvName) && channels[i].zrsvId){
			status = ca_array_get(DBR_STS_STRING, 1, channels[i].zrsvId, dataZrsv);
			if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
		}
		if (!strEmpty(channels[i].onsvName) && channels[i].onsvId){
			status = ca_array_get(DBR_STS_STRING, 1, channels[i].onsvId, dataOnsv);
			if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
		}
		if (!strEmpty(channels[i].twsvName) && channels[i].twsvId){
			status = ca_array_get(DBR_STS_STRING, 1, channels[i].twsvId, dataTwsv);
			if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
		}
		if (!strEmpty(channels[i].thsvName) && channels[i].thsvId){
			status = ca_array_get(DBR_STS_STRING, 1, channels[i].thsvId, dataThsv);
			if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
		}
		if (!strEmpty(channels[i].frsvName) && channels[i].frsvId){
			status = ca_array_get(DBR_STS_STRING, 1, channels[i].frsvId, dataFrsv);
			if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
		}
		if (!strEmpty(channels[i].fvsvName) && channels[i].fvsvId){
			status = ca_array_get(DBR_STS_STRING, 1, channels[i].fvsvId, dataFvsv);
			if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
		}
		if (!strEmpty(channels[i].sxsvName) && channels[i].sxsvId){
			status = ca_array_get(DBR_STS_STRING, 1, channels[i].sxsvId, dataSxsv);
			if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
		}
		if (!strEmpty(channels[i].svsvName) && channels[i].svsvId){
			status = ca_array_get(DBR_STS_STRING, 1, channels[i].svsvId, dataSvsv);
			if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
		}
		if (!strEmpty(channels[i].eisvName) && channels[i].eisvId){
			status = ca_array_get(DBR_STS_STRING, 1, channels[i].eisvId, dataEisv);
			if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
		}
		if (!strEmpty(channels[i].nisvName) && channels[i].nisvId){
			status = ca_array_get(DBR_STS_STRING, 1, channels[i].nisvId, dataNisv);
			if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
		}
		if (!strEmpty(channels[i].tesvName) && channels[i].tesvId){
			status = ca_array_get(DBR_STS_STRING, 1, channels[i].tesvId, dataTesv);
			if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
		}
		if (!strEmpty(channels[i].elsvName) && channels[i].elsvId){
			status = ca_array_get(DBR_STS_STRING, 1, channels[i].elsvId, dataElsv);
			if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
		}
		if (!strEmpty(channels[i].tvsvName) && channels[i].tvsvId){
			status = ca_array_get(DBR_STS_STRING, 1, channels[i].tvsvId, dataTvsv);
			if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
		}
		if (!strEmpty(channels[i].ttsvName) && channels[i].ttsvId){
			status = ca_array_get(DBR_STS_STRING, 1, channels[i].ttsvId, dataTtsv);
			if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
		}
		if (!strEmpty(channels[i].ftsvName) && channels[i].ftsvId){
			status = ca_array_get(DBR_STS_STRING, 1, channels[i].ftsvId, dataFtsv);
			if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
		}
		if (!strEmpty(channels[i].ffsvName) && channels[i].ffsvId){
			status = ca_array_get(DBR_STS_STRING, 1, channels[i].ffsvId, dataFfsv);
			if (status != ECA_NORMAL) fprintf(stderr, "Problem reading severity field of record %s: %s.\n", channels[i].name, ca_message(status));
		}


		ca_pend_io(arguments.w);

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
		if (!strEmpty(channels[i].hhsvName)) printf("	HIHI alarm severity: %s\n", ((struct dbr_sts_string *)dataHhsv)->value);		//severities
		if (!strEmpty(channels[i].hsvName)) printf("	HIGH alarm severity: %s\n", ((struct dbr_sts_string *)dataHsv)->value);
		if (!strEmpty(channels[i].lsvName)) printf("	LOW alarm severity: %s\n", ((struct dbr_sts_string *)dataLsv)->value);
		if (!strEmpty(channels[i].llsvName)) printf("	LOLO alarm severity: %s\n", ((struct dbr_sts_string *)dataLlsv)->value);
		if (!strEmpty(channels[i].unsvName)) printf("	UNSV alarm severity: %s\n", ((struct dbr_sts_string *)dataUnsv)->value);
		if (!strEmpty(channels[i].cosvName)) printf("	COSV alarm severity: %s\n", ((struct dbr_sts_string *)dataCosv)->value);
		if (!strEmpty(channels[i].zrsvName)) printf("	ZRSV alarm severity: %s\n", ((struct dbr_sts_string *)dataZrsv)->value);
		if (!strEmpty(channels[i].onsvName)) printf("	ONSV alarm severity: %s\n", ((struct dbr_sts_string *)dataOnsv)->value);
		if (!strEmpty(channels[i].twsvName)) printf("	TWSV alarm severity: %s\n", ((struct dbr_sts_string *)dataTwsv)->value);
		if (!strEmpty(channels[i].thsvName)) printf("	THSV alarm severity: %s\n", ((struct dbr_sts_string *)dataThsv)->value);
		if (!strEmpty(channels[i].frsvName)) printf("	FRSV alarm severity: %s\n", ((struct dbr_sts_string *)dataFrsv)->value);
		if (!strEmpty(channels[i].fvsvName)) printf("	FVSV alarm severity: %s\n", ((struct dbr_sts_string *)dataFvsv)->value);
		if (!strEmpty(channels[i].sxsvName)) printf("	SXSV alarm severity: %s\n", ((struct dbr_sts_string *)dataSxsv)->value);
		if (!strEmpty(channels[i].svsvName)) printf("	SVSV alarm severity: %s\n", ((struct dbr_sts_string *)dataSvsv)->value);
		if (!strEmpty(channels[i].eisvName)) printf("	EISV alarm severity: %s\n", ((struct dbr_sts_string *)dataEisv)->value);
		if (!strEmpty(channels[i].nisvName)) printf("	NISV alarm severity: %s\n", ((struct dbr_sts_string *)dataNisv)->value);
		if (!strEmpty(channels[i].tesvName)) printf("	TESV alarm severity: %s\n", ((struct dbr_sts_string *)dataTesv)->value);
		if (!strEmpty(channels[i].elsvName)) printf("	ELSV alarm severity: %s\n", ((struct dbr_sts_string *)dataElsv)->value);
		if (!strEmpty(channels[i].tvsvName)) printf("	TVSV alarm severity: %s\n", ((struct dbr_sts_string *)dataTvsv)->value);
		if (!strEmpty(channels[i].ttsvName)) printf("	TTSV alarm severity: %s\n", ((struct dbr_sts_string *)dataTtsv)->value);
		if (!strEmpty(channels[i].ftsvName)) printf("	FTSV alarm severity: %s\n", ((struct dbr_sts_string *)dataFtsv)->value);
		if (!strEmpty(channels[i].ffsvName)) printf("	FFSV alarm severity: %s\n", ((struct dbr_sts_string *)dataFfsv)->value);
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
			if (arguments.nord)	printf("# of elements in %s: %lu\n", ch->name, ch->count);
			if (arguments.outNelm == -1) ch->outNelm = ch->count;
			else if(arguments.outNelm > 0 && arguments.outNelm < ch->count) ch->outNelm = arguments.outNelm;
			else{
				ch->outNelm = ch->count;
				fprintf(stderr, "Invalid number %d of requested elements to read.", arguments.outNelm);
			}

			//request type
			if (arguments.d == -1){   //if DBR type not specified, decide based on desired details
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
							if (status != ECA_NORMAL) fprintf(stderr, "Problem creating ca_get request for channel %s: %s\n",ch->name, ca_message(status));
							status = ca_flush_io();
							if (status != ECA_NORMAL) fprintf(stderr, "Problem flushing channel %s: %s.\n", ch->name, ca_message(status));
	        			}

	        		}
	        	}
	        	else{// use GR_ by default
	        		ch->type = dbf_type_to_DBR_GR(ca_field_type(ch->id));
	        	}
			}
			else ch->type = arguments.d;

			if (arguments.tool == camon || arguments.tool == cawait){
				//create subscription
				status = ca_create_subscription(ch->type, ch->outNelm, ch->id, DBE_VALUE | DBE_ALARM | DBE_LOG, caReadCallback, ch, NULL);
				if (status != ECA_NORMAL) fprintf(stderr, "Problem creating subscription for channel %s: %s.\n",ch->name, ca_message(status));
				if (arguments.tool == cawait && arguments.timeout!=-1){
					//set first timeout
					epicsTimeGetCurrent(&timeoutTime);
					validateTimestamp(timeoutTime, "timeout");
					epicsTimeAddSeconds(&timeoutTime, arguments.timeout);
				}
				runMonitor = true;
			}

			ch->firstConnection = true;
    	}
    	else{//get just metadata if needed
			if (!arguments.nounit && (arguments.time || arguments.date || arguments.timestamp)){
				int reqType = dbf_type_to_DBR_GR(ca_field_type(ch->id));
				status = ca_array_get_callback(reqType, ch->count, ch->id, getStaticUnitsCallback, ch);
				if (status != ECA_NORMAL) fprintf(stderr, "Problem creating ca_get request for channel %s: %s\n",ch->name, ca_message(status));
				status = ca_flush_io();
				if (status != ECA_NORMAL) fprintf(stderr, "Problem flushing channel %s: %s.\n", ch->name, ca_message(status));
			}
    	}

    }
    else if(args.op == CA_OP_CONN_DOWN){
    	//clear metadata
    	strClear(outUnits[ch->i]);
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
    float elapsed = 0;
    while (elapsed <=arguments.w){
    	status = ca_pend_event (0.1);
    	elapsed += 0.1;
    	bool allDone=true;
    	for (i=0; i<nChannels; ++i) allDone *= channels[i].firstConnection;
    	if (allDone) break;
    }
    for (i=0; i<nChannels; ++i){
    	if (!channels[i].firstConnection) fprintf (stderr,"Channel %s not connected.\n", channels[i].name);
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
			status = ca_pend_io ( arguments.w );

			if(status == ECA_TIMEOUT) {
				//channel doesn't have usual sev fields
				strClear(channels[i].hhsvName);
				strClear(channels[i].hsvName);
				strClear(channels[i].lsvName);
				strClear(channels[i].llsvName);

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

				status = ca_pend_io ( arguments.w );

				if(status == ECA_TIMEOUT) {
					//channel doesn't have multi-bit severity fields
					strClear(channels[i].unsvName);
					strClear(channels[i].cosvName);
					strClear(channels[i].zrsvName);
					strClear(channels[i].onsvName);
					strClear(channels[i].twsvName);
					strClear(channels[i].thsvName);
					strClear(channels[i].frsvName);
					strClear(channels[i].fvsvName);
					strClear(channels[i].sxsvName);
					strClear(channels[i].svsvName);
					strClear(channels[i].eisvName);
					strClear(channels[i].nisvName);
					strClear(channels[i].tesvName);
					strClear(channels[i].elsvName);
					strClear(channels[i].tvsvName);
					strClear(channels[i].ttsvName);
					strClear(channels[i].ftsvName);
					strClear(channels[i].ffsvName);
				}
			}
			else{
				//channel doesn't have multi-bit severity fields
				strClear(channels[i].unsvName);
				strClear(channels[i].cosvName);
				strClear(channels[i].zrsvName);
				strClear(channels[i].onsvName);
				strClear(channels[i].twsvName);
				strClear(channels[i].thsvName);
				strClear(channels[i].frsvName);
				strClear(channels[i].fvsvName);
				strClear(channels[i].sxsvName);
				strClear(channels[i].svsvName);
				strClear(channels[i].eisvName);
				strClear(channels[i].nisvName);
				strClear(channels[i].tesvName);
				strClear(channels[i].elsvName);
				strClear(channels[i].tvsvName);
				strClear(channels[i].ttsvName);
				strClear(channels[i].ftsvName);
				strClear(channels[i].ffsvName);
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
        if( checkStatus(status) != EXIT_SUCCESS) exitStatus = EXIT_FAILURE;
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


    if (endsWith(argv[0],"caget")) arguments.tool = caget;
    if (endsWith(argv[0],"caput")) arguments.tool = caput;
    if (endsWith(argv[0],"cagets")) arguments.tool = cagets;
    if (endsWith(argv[0],"caputq")) arguments.tool = caputq;
    if (endsWith(argv[0],"camon")) arguments.tool = camon;
    if (endsWith(argv[0],"cawait")) arguments.tool = cawait;
    if (endsWith(argv[0],"cado")) arguments.tool = cado;
    if (endsWith(argv[0],"cainfo")) arguments.tool = cainfo;


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
                else{
                    fprintf(stderr, "Precision %d for option '-%c' "
                            "out of range - ignored.\n", arguments.digits, opt);
                    arguments.digits = -1;
                }
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
                		char formatType = arguments.dblFormatStr[strlen(arguments.dblFormatStr)-1];
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
    if (arguments.tool == tool_unknown){
    	usage(stderr, arguments.tool,argv[0]);
    	return EXIT_FAILURE;
    }
    if(arguments.tool == cainfo && (arguments.s || arguments.num || arguments.bin  || arguments.hex || arguments.oct \
    		|| arguments.d != -1 || arguments.prec != -1 || arguments.round != roundType_no_rounding || arguments.plain \
    		|| arguments.digits != -1 || arguments.stat || arguments.nostat || arguments.noname || arguments.nounit \
    		|| arguments.timestamp != '\0' || arguments.timeout != -1 || arguments.date || arguments.time || arguments.localdate \
    		|| arguments.localtime || arguments.fieldSeparator != ' ' || arguments.inputSeparator != ' ' || arguments.numUpdates != -1\
    		|| arguments.inNelm != 1 || arguments.outNelm != -1 || arguments.nord)){
		fprintf(stderr, "The only option allowed for cainfo is -w.\n");
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


	epicsTimeGetCurrent(&programStartTime);
	validateTimestamp(programStartTime, "Start time");

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
    	channels[i].llsvName = callocMustSucceed (LEN_RECORD_NAME + 5, sizeof(char), "main");
    	channels[i].unsvName = callocMustSucceed (LEN_RECORD_NAME + 5, sizeof(char), "main");
    	channels[i].cosvName = callocMustSucceed (LEN_RECORD_NAME + 5, sizeof(char), "main");
    	channels[i].zrsvName = callocMustSucceed (LEN_RECORD_NAME + 5, sizeof(char), "main");
    	channels[i].onsvName = callocMustSucceed (LEN_RECORD_NAME + 5, sizeof(char), "main");
    	channels[i].twsvName = callocMustSucceed (LEN_RECORD_NAME + 5, sizeof(char), "main");
    	channels[i].thsvName = callocMustSucceed (LEN_RECORD_NAME + 5, sizeof(char), "main");
    	channels[i].frsvName = callocMustSucceed (LEN_RECORD_NAME + 5, sizeof(char), "main");
    	channels[i].fvsvName = callocMustSucceed (LEN_RECORD_NAME + 5, sizeof(char), "main");
    	channels[i].sxsvName = callocMustSucceed (LEN_RECORD_NAME + 5, sizeof(char), "main");
    	channels[i].svsvName = callocMustSucceed (LEN_RECORD_NAME + 5, sizeof(char), "main");
    	channels[i].eisvName = callocMustSucceed (LEN_RECORD_NAME + 5, sizeof(char), "main");
    	channels[i].nisvName = callocMustSucceed (LEN_RECORD_NAME + 5, sizeof(char), "main");
    	channels[i].tesvName = callocMustSucceed (LEN_RECORD_NAME + 5, sizeof(char), "main");
    	channels[i].elsvName = callocMustSucceed (LEN_RECORD_NAME + 5, sizeof(char), "main");
    	channels[i].tvsvName = callocMustSucceed (LEN_RECORD_NAME + 5, sizeof(char), "main");
    	channels[i].ttsvName = callocMustSucceed (LEN_RECORD_NAME + 5, sizeof(char), "main");
    	channels[i].ftsvName = callocMustSucceed (LEN_RECORD_NAME + 5, sizeof(char), "main");
    	channels[i].ffsvName = callocMustSucceed (LEN_RECORD_NAME + 5, sizeof(char), "main");

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
        		channels[i].writeStr = callocMustSucceed (count *MAX_STRING_SIZE, sizeof(char), "main");

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
        	//set sibling channels for getting desc and severity data
        	size_t lenName = strlen(channels[i].name);
        	assert (lenName < LEN_RECORD_NAME + 4);//worst case scenario: longest name + .VAL

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
	lastUpdate = mallocMustSucceed(nChannels * sizeof(epicsTimeStamp),"main");
	firstUpdate = mallocMustSucceed(nChannels * sizeof(bool),"main");

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
