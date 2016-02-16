/* caToolsMain.cpp */
/* Author:  Sašo Skube Date:    30JUL2015 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h> // XXX is ok?

#include "cadef.h"  // for channel access
#include "alarmString.h"  // XXX is ok?
#include <getopt.h>


#include <stdbool.h>
#ifndef __bool_true_false_are_defined
    #ifdef _Bool
        #define bool                        _Bool
    #else
        #define bool                        char
    #endif
    #define true                            1
    #define false                           0
    #define __bool_true_false_are_defined   1
#endif



#define CA_PRIORITY CA_PRIORITY_MIN
#define CA_TIMEOUT 1

enum roundType{
	roundType_no_rounding=-1,
    roundType_default=0,
    roundType_ceil=1,
    roundType_floor=2
};

enum tool{
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
   int  d;					//precision
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
   bool date;				//server date
   bool localdate;			//client date
   bool time;				//server time
   bool localtime;			//client time
   char fieldSeparator;		//array field separator
   enum tool tool;			//tool type
   int numUpdates;			//number of monitor updates after which to quit
   int inNelm;				//number of array elements to write
   int outNelm;				//number of array elements to read
   bool nord;				//display number of array elements
};

//intialize arguments
struct arguments arguments = {
    CA_TIMEOUT,         // w
    -1,                 // d
    false,              // num
    roundType_no_rounding,  // round
    -1,                 // prec
    false,              // hex
    false,              // bin
    false,              // oct
    false,              // plain
    -1,                 // digits
    "%g",               // dblFormatStr
    false,              // s
    false,              // stat
    false,              // nostat
    0,                  // noname
    0,                  // nounit
    0,					// timestamp
    false,				// server date
    false,				// localdate
    false,				// time
    false,				// localtime
    ' '	,				// field separator
    caget,				// tool
    -1,					// numUpdates
    1,					// inNelm
    -1,					// outNelm
    false,				// nord
};

struct channel{
    char            *name;  // the name of the channel
    chid            id;     // channel id
    char			*procName;	//sibling channel for writing to proc field
    chid			procId;	// id of sibling channel for writing to proc field
    char			*descName;	//sibling channel for writing to desc field
    chid			descId;	// id of sibling channel for writing to desc field
    char			*hhsvName;	//sibling channel for writing to hhsv field
    chid			hhsvId;	// id of sibling channel for writing to hhsv field
    char			*hsvName;	//sibling channel for writing to hsv field
    chid			hsvId;	// id of sibling channel for writing to hsv field
    char			*lsvName;	//sibling channel for writing to lsv field
    chid			lsvId;	// id of sibling channel for writing to lsv field
    char			*llsvName;	//sibling channel for writing to llsv field
    chid			llsvId;	// id of sibling channel for writing to llsv field
    long            type;   // dbr type
    long            count;  // element count
    long            inNelm;  // requested number of elements for writing
    long            outNelm;  // requested number of elements for reading
    bool            done;   // indicates if callback has finished processing this channel
    int 			i;		// process value id
    char			*writeStr;	// value(s) to be written
    char 			*conditionStr;	//cawait condition
};


//output strings
#define LEN_TIMESTAMP 50
#define LEN_RECORD_NAME 61
#define LEN_VALUE 100 //XXX max value size?hmk??
#define LEN_SEVSTAT 30
#define LEN_UNITS 20+MAX_UNITS_SIZE
char **outDate,**outTime, **outSev, **outStat, **outUnits, **outName, **outValue, **outLocalDate, **outLocalTime;
char **outTimestamp; //relative timestamps for camon

epicsTimeStamp *timestampRead;		//timestamps of received data (per channel)

epicsTimeStamp programStartTime;	//timestamp indicating program start, needed by -timestamp



void usage(FILE *stream, char *programName){

	if (!strcmp(programName, "caget") || !strcmp(programName, "cagets") || !strcmp(programName, "camon") || !strcmp(programName, "cado") ){
		fprintf(stream, "Usage: %s [flags] [channel] [channel] ... [channel]\n", programName);
		fprintf(stream, "Accepted flags:\n");
		//
	}

	if (!strcmp(programName, "caput") || !strcmp(programName, "caputq") ){
		fprintf(stream, "Usage: %s [flags] [channel] [value] [channel] [value] ... [channel] [value]\n", programName);
		fprintf(stream, "Accepted flags:\n");
		//
	}


	if (!strcmp(programName, "cawait") ){
		fprintf(stream, "Usage: %s [flags] [channel] [condition] [channel] [condition] ... [channel] [condition]\n", programName);
		fprintf(stream, "Accepted flags:\n");
		//
		//displays
	}

	if (!strcmp(programName, "cainfo") ){
		fprintf(stream, "Usage: %s [flags] [channel] [channel] ... [channel]\n", programName);
		//displays ...
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



int checkStatus(int status){
    if (status != ECA_NORMAL){
        fprintf(stderr, "CA error %s occurred while trying "
                "to start channel access.\n", ca_message(status));
        SEVCHK(status, "CA error");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}


int cawaitCondition(struct channel channel, int k){
//evaluates output of channel i against the corresponding condition.
//returns 1 if matching, 0 otherwise, and -1 if error.
//Before evaluation, channel output is converted to double. If this is
//not successful, the function returns -1. If the channel is question
//is an array, the condition is evaluated against the first element.
//Supported operators: >,<,<=,>=,==,!=, ==A...B(in interval), !=A...B(out of interval).


	int i,j;				//counters
	bool interval=false;	//indicates if interval is desired
	int output;				//1,0,-1

	double channelValue;
	if (sscanf(outValue[k], "%lf", &channelValue) <= 0){
		fprintf(stderr, "Channel %s value %s cannot be converted to double.", channel.name, outValue[k]);
		return -1;
	}

	//create a local copy of condition string without spaces, and replace '...' with ':', which is easier to parse
	char *conditionStr;
	conditionStr = calloc(strlen(channel.conditionStr) ,sizeof(char));
	if (!conditionStr) fprintf(stderr, "Memory allocation error.\n");
	for (i=0, j=0; i<strlen(channel.conditionStr); ++i){
		if (channel.conditionStr[i] != ' '){
			if (channel.conditionStr[i] == '.' && channel.conditionStr[i+1] == '.' && channel.conditionStr[i+2] == '.'){
				conditionStr[j++] = ':';
				i += 2;
				interval=true;
				continue;
			}
			conditionStr[j++] = channel.conditionStr[i];
		}
	}

	//string to hold parsing format
	char *format;
	format = calloc(2*strlen(channel.conditionStr) ,sizeof(char));
	if (!format) fprintf(stderr, "Memory allocation error.\n");

	//string to hold operator
	char operator[3] = {0,0,0};


	//Parse the conditions string. The first character is always an operator.
	//The second character is operator if it is not numerical. The operator is followed by
	//a double. Optionally, this is followed by ':' and another double.
	if (conditionStr[1]>='0' && conditionStr[1]<='9'){
		strcpy(format, "%c%lf:%lf");
	}
	else{
		strcpy(format, "%2c%lf:%lf");
	}

	double operand1, operand2;
	int nParsed;
	nParsed = sscanf(conditionStr, format, operator, &operand1, &operand2);

	if (nParsed == 2 && interval == false){

		if (strcmp(operator, ">")==0){
			output = channelValue > operand1 ? 1 : 0;
		}
		else if (strcmp(operator, "<")==0){
			output = channelValue < operand1 ? 1 : 0;
		}
		else if (strcmp(operator, ">=")==0){
			output = channelValue >= operand1 ? 1 : 0;
		}
		else if (strcmp(operator, "<=")==0){
			output = channelValue <= operand1 ? 1 : 0;
		}
		else if (strcmp(operator, "==")==0){
			output = channelValue == operand1 ? 1 : 0;
		}
		else if (strcmp(operator, "!=")==0){
			output = channelValue != operand1 ? 1 : 0;
		}
		else{
			fprintf(stderr, "Problem interpreting condition string: %s. Unknown operator %s.", conditionStr, operator);
			output = -1;
		}

	}
	else if (nParsed == 3 && interval == interval){
		if (strcmp(operator, "==")==0){
			output = (channelValue >= operand1) && (channelValue <= operand2) ? 1 : 0;
		}
		else if (strcmp(operator, "!=")==0){
			output = (channelValue <= operand1) || (channelValue >= operand2) ? 1 : 0;
		}
		else{
			fprintf(stderr, "Problem interpreting condition string: %s. Only operators == and != are supported if the"\
					"value is to be compared to an interval.",conditionStr);
			output = -1;
		}
	}
	else{
		fprintf(stderr, "Problem interpreting condition string: %s. Wrong number of operands.", conditionStr);
		output = -1;
	}



	free(conditionStr);
	free(format);

	return output;
}


void sprintBits(char *outStr, size_t const size, void const * const ptr){
//Converts a decimal number to binary and saves it to outStr.
//The decimal number is pointed to by ptr and is of memory size size.
//Assumes little endian.

    unsigned char *b = (unsigned char*) ptr;
    unsigned char byte;
    int i, j;
    int k=0;

    for (i=size-1;i>=0;i--)
    {
        for (j=7;j>=0;j--)
        {
            byte = b[i] & (1<<j);
            byte >>= j;
            sprintf(outStr + k, "%u", byte);
            k++;
        }
    }
}

int valueToString(char *stringValue, unsigned int type, const void *dbr, int count, int precision){
//Parses the data fetched by ca_get callback according to the data type and formatting arguments.
//The result is saved into the global value output string.

    dbr_enum_t v;
    unsigned int baseType;
    void *value;

    value = dbr_value_ptr(dbr, type);
    baseType = type % (LAST_TYPE+1);   // convert appropriate TIME, GR, CTRL,... type to basic one

    //loop over the whole array
    int i; //element counter
    char *stringCursor = stringValue; //pointer to start of the string
    for (i=0; i<count; ++i){
    	stringCursor = stringValue + strlen(stringValue); //move along the string for each element

    	if (i){
    		sprintf (stringCursor, "%c", arguments.fieldSeparator); //insert element separator
        	stringCursor = stringValue + strlen(stringValue); //move
    	}

    	switch (baseType) {
    	case DBR_STRING:
    		strcpy(stringCursor, ((dbr_string_t*) value)[i]);
    		break;

    	case DBR_INT:
    		//display dec, hex, bin, oct if desired
    		if (arguments.hex){
    			sprintf(stringCursor, "%x", ((dbr_int_t*)value)[i]);
    		}
    		else if (arguments.oct){
    			sprintf(stringCursor, "%o", ((dbr_int_t*)value)[i]);
    		}
    		else if (arguments.bin){
    			sprintBits(stringCursor, sizeof(((dbr_int_t*)value)[i]), &((dbr_int_t*)value)[i]);
    		}
    		else{
    			sprintf(stringCursor, "%d", ((dbr_int_t*)value)[i]);
    		}
    		break;

		//case DBR_SHORT: equals DBR_INT
    		//    sprintf(stringValue, "%d", ((dbr_short_t*)value)[i]);
    		//    break;

    	case DBR_FLOAT:
    		//round if desired
    		if (arguments.round == roundType_no_rounding){
    			if (arguments.digits == -1 && arguments.prec == -1){
    				//display with precision defined by the record
        			sprintf(stringCursor, "%-.*g", precision, ((dbr_float_t*)value)[i]);
    			}
    			else if (arguments.digits == -1 && arguments.prec != -1){
    				//display with precision defined by arguments.prec
        			sprintf(stringCursor, "%-.*g", arguments.prec, ((dbr_float_t*)value)[i]);
    			}
    			else if (arguments.digits != -1 && arguments.prec == -1){
    				//use precision and format as specified by -efg flags
        			sprintf(stringCursor, arguments.dblFormatStr, ((dbr_float_t*)value)[i]);
    			}
    		}
    		else if (arguments.round == roundType_default){
    			sprintf(stringCursor, arguments.dblFormatStr, roundf(((dbr_float_t*)value)[i]));
    		}
    		else if (arguments.round == roundType_ceil){
    			sprintf(stringCursor, arguments.dblFormatStr, ceilf(((dbr_float_t*)value)[i]));
    		}
    		else if (arguments.round == roundType_floor){
    			sprintf(stringCursor, arguments.dblFormatStr, floorf(((dbr_float_t*)value)[i]));
    		}
    		break;

    	case DBR_ENUM:
    		v = ((dbr_enum_t *)value)[i];
    		if (!arguments.num && !arguments.bin && !arguments.hex && !arguments.oct) { // if not requested as a number check if we can get string
    			if (dbr_type_is_GR(type)) {
    				if (v > ((struct dbr_gr_enum *)dbr)->no_str) {
    					fprintf(stderr, "Warning: enum index value '%d' may be too large.\n", v);
    				}
    				sprintf(stringCursor, "%s", ((struct dbr_gr_enum *)dbr)->strs[v]);
    				break;
    			}
    			else if (dbr_type_is_CTRL(type)) {
    				if (v > ((struct dbr_ctrl_enum *)dbr)->no_str) {
    					fprintf(stderr, "Warning: enum index value '%d' may be too large.\n", v);
    				}
    				sprintf(stringCursor, "%s", ((struct dbr_ctrl_enum *)dbr)->strs[v]);
    				break;
    			}
    		}
    		// else return string value as number.
    		if (arguments.hex){
    			sprintf(stringCursor, "%x", v);
    		}
    		else if (arguments.oct){
    			sprintf(stringCursor, "%o", v);
    		}
    		else if (arguments.bin){
    			sprintBits(stringCursor, sizeof(v), &v);
    		}
    		else{
    			sprintf(stringCursor, "%d", v);
    		}
    		break;

    	case DBR_CHAR:
    		if (!arguments.num) {	//check if requested as a a number
    			sprintf(stringCursor, "%c", ((dbr_char_t*) value)[i]);
    		}
    		else{
    			sprintf(stringCursor, "%d", ((dbr_char_t*) value)[i]);
    		}
    		break;

    	case DBR_LONG:
    		//display dec, hex, bin, oct if desired
    		if (arguments.hex){
    			sprintf(stringCursor, "%x", ((dbr_long_t*)value)[i]);
    		}
    		else if (arguments.oct){
    			sprintf(stringCursor, "%o", ((dbr_long_t*)value)[i]);
    		}
    		else if (arguments.bin){
    			sprintBits(stringCursor, sizeof(((dbr_long_t*)value)[i]), &((dbr_long_t*)value)[i]);
    		}
    		else{
    			sprintf(stringCursor, "%d", ((dbr_long_t*)value)[i]);
    		}
    		break;

    	case DBR_DOUBLE:
    		//round if desired
    		if (arguments.round == roundType_no_rounding){
    			if (arguments.digits == -1 && arguments.prec == -1){
    				//display with precision defined by the record
    				sprintf(stringCursor, "%-.*g", precision, ((dbr_double_t*)value)[i]);
    			}
    			else if (arguments.digits == -1 && arguments.prec != -1){
    				//display with precision defined by arguments.prec
    				sprintf(stringCursor, "%-.*g", arguments.prec, ((dbr_double_t*)value)[i]);
    			}
    			else if (arguments.digits != -1 && arguments.prec == -1){
    				//use precision and format as specified by -efg flags
    				sprintf(stringCursor, arguments.dblFormatStr, ((dbr_double_t*)value)[i]);
    			}
    		}
    		else if (arguments.round == roundType_default){
    			sprintf(stringCursor, arguments.dblFormatStr, round(((dbr_double_t*)value)[i]));
    		}
    		else if (arguments.round == roundType_ceil){
    			sprintf(stringCursor, arguments.dblFormatStr, ceil(((dbr_double_t*)value)[i]));
    		}
    		else if (arguments.round == roundType_floor){
    			sprintf(stringCursor, arguments.dblFormatStr, floor(((dbr_double_t*)value)[i]));
    		}
    		break;

    	default:
    		strcpy(stringCursor, "Unrecognized DBR type");
    		break;
    	}
    }

    return 0;
}


static void enumToString (evargs args){
//caget callback function which receives read enum data and tries to interpret it
//as a string, which is then written to memory pointed at by args.usr. If the data
//can't be read as a string (i.e. dbr type is not ctrl or gr), number is written.
//The result is written to the global value output string.

	//check if data was returned
	if (args.status != ECA_NORMAL){
		fprintf(stderr, "CA error %s occurred while trying "
		                "to start channel access.\n", ca_message(args.status));
		return;
	}

    struct channel *ch = (struct channel *)args.usr; 	//the channel to which this callback belongs

	char *outString = outValue[ch->i];	//string to which the output is written = global string of values
	void *value = dbr_value_ptr(args.dbr, args.type); //pointer to value part of the returned structure
	dbr_enum_t v; //temporary value holder

	//loop over the whole array
	int i; //element counter
	char *stringCursor = outString; //pointer to start of the string
	for (i=0; i<args.count; ++i){
		stringCursor = outString + strlen(outString); //move along the string for each element

		if (i){
			sprintf (stringCursor, "%c", arguments.fieldSeparator); //insert element separator
			stringCursor = outString + strlen(outString); //move
		}

		v = ((dbr_enum_t *)value)[i];
		if (v > ((struct dbr_gr_enum *)args.dbr)->no_str) fprintf(stderr, "Warning: enum index value '%d' may be too large.\n", v);
		if (!arguments.num && !arguments.bin && !arguments.hex && !arguments.oct) { // if not requested as a number check if we can get string
			if (dbr_type_is_GR(args.type)) {
				if (v > ((struct dbr_gr_enum *)args.dbr)->no_str) {
					fprintf(stderr, "Warning: enum index value '%d' may be too large.\n", v);
				}
				sprintf(outString, "%s", ((struct dbr_gr_enum *)args.dbr)->strs[v]);
				continue;
			}
			else if (dbr_type_is_CTRL(args.type)) {
				if (v > ((struct dbr_ctrl_enum *)args.dbr)->no_str) {
					fprintf(stderr, "Warning: enum index value '%d' may be too large.\n", v);
				}
				sprintf(outString, "%s", ((struct dbr_ctrl_enum *)args.dbr)->strs[v]);
				continue;
			}
		}

		// if this fails return string value as number.
		if (arguments.bin){
			sprintBits(outString, sizeof(v), &v);
		}
		else if (arguments.oct){
			sprintf(outString, "%o", v);
		}
		else if (arguments.hex){
			sprintf(outString, "%x", v);
		}
		else{
			sprintf(outString, "%d", v);
		}
	}

	//finish
	ch->done = true;
}

int printOutput(int i){
// prints global output strings corresponding to i-th channel.

	//if both local and server times are requested, clarify which is which
	if ((arguments.localdate || arguments.localtime) && (arguments.date || arguments.time)){
		printf("server time: ");
	}

    //server date
	if (strcmp(outDate[i],"") != 0)	printf("%s ",outDate[i]);
	//server time
	if (strcmp(outTime[i],"") != 0)	printf("%s ",outTime[i]);

	if ((arguments.localdate || arguments.localtime) && (arguments.date || arguments.time)){
		printf("local time: ");
	}

	//local date
	if (strcmp(outLocalDate[i],"") != 0)	printf("%s ",outLocalDate[i]);
	//local time
	if (strcmp(outLocalTime[i],"") != 0)	printf("%s ",outLocalTime[i]);


    //timestamp if monitor and if requested
    if (arguments.tool == camon && arguments.timestamp)	printf("%s ", outTimestamp[i]);

    //channel name
    if (strcmp(outName[i],"") != 0)	printf("%s ",outName[i]);

    //value(s)
    printf("%s ", outValue[i]);

    //egu
    if (strcmp(outUnits[i],"") != 0)printf("%s ",outUnits[i]);

    //severity
    if (strcmp(outSev[i],"") != 0) printf("%s ",outSev[i]);

    //status
	if (strcmp(outStat[i],"") != 0) printf("%s ",outStat[i]);

	printf("\n");
    return 0;
}


int epicsTimeDiffFull(epicsTimeStamp *diff, const epicsTimeStamp * pLeft, const epicsTimeStamp * pRight ){
// Calculates difference between two epicsTimeStampst: like epicsTimeDiffInSeconds but also taking nanoseconds
//into account. The absolute value of the difference pLeft - pRight is saved to the timestamp diff, and the sign
//of the said difference is returned.

	double timeLeft = pLeft->secPastEpoch + 1e-9 * pLeft->nsec;
	double timeRight = pRight->secPastEpoch + 1e-9 * pRight->nsec;

	double difference = (timeLeft - timeRight);

	diff->secPastEpoch = fabs(trunc(difference));	//number of seconds is the integer part.
	diff->nsec = 1e9 * (fabs(difference) - fabs(trunc(difference)));	//number of nanoseconds is the decimal part.
	if (difference > 0){
		return 1;//positive
	}
	else if(difference < 0){
		return -1;//negative
	}
	else{
		return 0;//equal
	}

}

static void caReadCallback (evargs args){
//reads and parses data fetched by calls. First, the global strings holding the output are cleared. Then, depending
//on the type of the returned data, the available information is extracted. The extracted info is then saved to the
//global strings. The actual value of the PV is an exception; its reading, parsing, and storing are performed by
//calling valueToString or enumToString. Note that the latter is actually a callback function for another get request.

	//check if data was returned
	if (args.status != ECA_NORMAL){
		fprintf(stderr, "Error in read callback. %s.\n", ca_message(args.status));
		return;
	}

    struct channel *ch = (struct channel *)args.usr;

    // output strings - local copies
    char locDate[LEN_TIMESTAMP],locTime[LEN_TIMESTAMP], locSev[30], locStat[30], locUnits[20+MAX_UNITS_SIZE];
    char locName[LEN_RECORD_NAME], locLocalDate[LEN_TIMESTAMP],locLocalTime[LEN_TIMESTAMP];
    int precision=-1;
    int status=0, severity=0;

    //initialize local copies of output strings
    sprintf(locDate,"%s","");
    sprintf(locTime,"%s","");
	sprintf(locSev,"%s","");
	sprintf(locStat,"%s","");
	sprintf(locUnits,"%s","");
	sprintf(locName,"%s","");
	sprintf(locLocalDate,"%s","");
	sprintf(locLocalTime,"%s","");
	//there is no locValue

	//also clear global output strings; the purpose of this callback is to overwrite them
	sprintf(outDate[ch->i],"%s","");
	sprintf(outTime[ch->i],"%s","");
	sprintf(outSev[ch->i],"%s","");
	sprintf(outStat[ch->i],"%s","");
	sprintf(outUnits[ch->i],"%s","");
	sprintf(outName[ch->i],"%s","");
	sprintf(outValue[ch->i],"%s","");
	sprintf(outLocalDate[ch->i],"%s","");
	sprintf(outLocalTime[ch->i],"%s","");

    //templates for reading requested data
#define severity_status_get(T) \
    status = ((struct T *)args.dbr)->status; \
	severity = ((struct T *)args.dbr)->severity; \
	sprintf(locSev, "%s", epicsAlarmSeverityStrings[severity]); \
	sprintf(locStat, "%s", epicsAlarmConditionStrings[status]);

#define timestamp_get(T) \
		epicsTimeToStrftime(locDate, LEN_TIMESTAMP, "%Y-%m-%d", (epicsTimeStamp *)&(((struct T *)args.dbr)->stamp.secPastEpoch));\
		epicsTimeToStrftime(locTime, LEN_TIMESTAMP, "%H:%M:%S.%06f", (epicsTimeStamp *)&(((struct T *)args.dbr)->stamp.secPastEpoch));\
		timestampRead[ch->i] = (epicsTimeStamp)((struct T *)args.dbr)->stamp;

#define units_get(T) sprintf(locUnits, "%s", ((struct T *)args.dbr)->units);

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

        //case DBR_TIME_INT: not implemented, same as dbr short
        //    severity_status_get(dbr_time_);
        //    break;

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

        //case DBR_GR_STRING:
        //    not implemented (see db_access.h)
        //    break;

        case DBR_GR_SHORT:    // and DBR_GR_INT
            severity_status_get(dbr_gr_short);
            units_get(dbr_gr_short);
            break;

        case DBR_GR_FLOAT:
            severity_status_get(dbr_gr_float);
            units_get(dbr_gr_float);
            precision = (int)(((struct dbr_gr_float *)args.dbr)->precision);
            break;

        case DBR_GR_ENUM:
            severity_status_get(dbr_gr_enum);
            // does not have units
            break;

        case DBR_GR_CHAR:
            severity_status_get(dbr_gr_char);
            units_get(dbr_gr_char);
            break;

        case DBR_GR_LONG:
            severity_status_get(dbr_gr_long);
            units_get(dbr_gr_long);
            break;

        case DBR_GR_DOUBLE:
            severity_status_get(dbr_gr_double);
            units_get(dbr_gr_double);
            precision = (int)(((struct dbr_gr_double *)args.dbr)->precision);
            break;

        //case DBR_CTRL_STRING:
        //    not implemented (see db_access.h)
        //    break;

        case DBR_CTRL_SHORT:  // and DBR_CTRL_INT
            severity_status_get(dbr_ctrl_short);
            units_get(dbr_ctrl_short);
            break;

        case DBR_CTRL_FLOAT:
            severity_status_get(dbr_ctrl_float);
            units_get(dbr_ctrl_float);
            precision = (int)(((struct dbr_ctrl_float *)args.dbr)->precision);
            break;

        case DBR_CTRL_ENUM:
            severity_status_get(dbr_ctrl_enum);
            // does not have units
            break;

        case DBR_CTRL_CHAR:
            severity_status_get(dbr_ctrl_char);
            units_get(dbr_ctrl_char);
            break;

        case DBR_CTRL_LONG:
            severity_status_get(dbr_ctrl_long);
            units_get(dbr_ctrl_long);
            break;

        case DBR_CTRL_DOUBLE:
            severity_status_get(dbr_ctrl_double);
            units_get(dbr_ctrl_double);
            precision = (int)(((struct dbr_ctrl_double *)args.dbr)->precision);
            break;


        default :
            printf("Can not print %s DBR type. \n", dbr_type_to_text(args.type));
            break;
    }



    //check formating arguments
    //hide name?
    if (!arguments.noname){
    	sprintf(locName, ch->name);
    }

    //hide units?
    if (arguments.nounit){
    	sprintf(locUnits,"%s","");
    }

    //hide alarms?
    if (arguments.stat){//always display
    	//do nothing
    }
    if (arguments.nostat){//never display
    	sprintf(locSev, "%s","");
    	sprintf(locStat, "%s","");
    }
    else{//display if alarm
    	if (status == 0 && severity == 0){
        	sprintf(locSev, "%s","");
        	sprintf(locStat, "%s","");
    	}
    }

    //hide date or time?
    //we assume that manually specifying dbr_time does not imply -time or -date.
	if (!arguments.date){
		sprintf(locDate,"%s","");
	}
	if (!arguments.time){
		sprintf(locTime,"%s","");
	}

	//show local date or time?
	if (arguments.localdate || arguments.localtime){
		time_t localTime = time(0);
		if (arguments.localdate){
			strftime(locLocalDate, LEN_TIMESTAMP, "%Y-%m-%d", localtime(&localTime));
		}
		if (arguments.localtime){
			strftime(locLocalTime, LEN_TIMESTAMP, "%H:%M:%S", localtime(&localTime));
		}
	}

	//sprintf loc -> out
	sprintf(outDate[ch->i], locDate);
	sprintf(outTime[ch->i], locTime);
	sprintf(outName[ch->i], locName);
	sprintf(outUnits[ch->i], locUnits);
	sprintf(outSev[ch->i], locSev);
	sprintf(outStat[ch->i], locStat);



    //construct values string and write to global string directly
	//special case: enum
	if ((args.type == DBR_STS_STRING || DBR_TIME_STRING) && (arguments.num || arguments.bin || arguments.hex || arguments.oct) \
			&& ca_field_type(ch->id) == DBF_ENUM){
		//special case 1: field type is enum, dbr type is string, and number is requested.
		//to obtain value number, another request is needed.
		status = ca_array_get_callback(DBR_TIME_ENUM, ch->outNelm, ch->id, enumToString, ch);
		if (status != ECA_NORMAL) fprintf(stderr, "CA error %s occurred while trying to create ca_get request.\n", ca_message(status));

		status = ca_flush_io();
		if(status != ECA_NORMAL) fprintf (stderr,"Problem flushing channel %s.\n", ch->name);
		//ch->done is set by enum2string callback.
	}
	else if (args.type >= DBR_STS_SHORT && args.type <= DBR_TIME_DOUBLE \
			&& arguments.s && ca_field_type(ch->id) == DBF_ENUM){
		//special case 2: field type is enum, dbr type is time or sts but not string, and string is requested.
		//we already have time, but to obtain value in the form of string, another request is needed.

		status = ca_array_get_callback(DBR_GR_ENUM, ch->outNelm, ch->id, enumToString, ch);
		if (status != ECA_NORMAL) fprintf(stderr, "CA error %s occurred while trying to create ca_get request.\n", ca_message(status));

		status = ca_flush_io();
		if(status != ECA_NORMAL) fprintf (stderr,"Problem flushing channel %s.\n", ch->name);
		//ch->done is set by enum2string callback.
	}
	else{ //get values as usual
		valueToString(outValue[ch->i], args.type, args.dbr, args.count, precision);
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



void caRequest(struct channel *channels, int nChannels){
//sends get or put requests for all tools except cainfo. ca_get or ca_put are called multiple times, depending
//on the tool. The reading and parsing of returned data is performed in callbacks. Once the callbacks finish,
//the the data is printed. If the tool type is monitor, a loop is entered in which the data is printed repeatedly.
    int status, i, j;

    for(i=0; i < nChannels; i++){
        channels[i].count = ca_element_count ( channels[i].id );

        if (arguments.nord){
        	printf("# of elements in %s: %lu\n", channels[i].name, channels[i].count);
        }

        //how many array elements to request
        if(arguments.inNelm > 0 && arguments.inNelm <= channels[i].count ){
        	channels[i].inNelm = arguments.inNelm;
        }
        else{
        	channels[i].inNelm = 1;
        	if (channels[i].count == 0) fprintf(stderr, "Channel %s is probably not connected.\n", channels[i].name);
        	else fprintf(stderr, "Invalid number %d of requested elements to write. Defaulting to 1.\n", arguments.inNelm);
        }
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
			status = ca_create_subscription(channels[i].type, channels[i].outNelm, channels[i].id, DBE_VALUE | DBE_ALARM | DBE_LOG,\
					caReadCallback, &channels[i], NULL);
			if (status != ECA_NORMAL) fprintf(stderr, "CA error %s occurred while trying to create monitor.\n", ca_message(status));
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

    status = ca_flush_io ();
    if(status == ECA_TIMEOUT){
    	printf ("Flush error: %s.\n", ca_message(status));
    }

    //if caput or cagets, wait for put callback and issue a new read request.
    if (arguments.tool == caput || arguments.tool == cagets){

    	epicsTimeStamp waitStart;
    	epicsTimeGetCurrent(&waitStart);

    	epicsTimeStamp waitNow;
    	epicsTimeStamp elapsed;

    	bool doneEach[nChannels];
    	for (i=0; i<nChannels; ++i) doneEach[i]=false;

    	while(1){
    		for (i=0; i<nChannels; ++i){
    			if (channels[i].done){
    				status = ca_array_get_callback(channels[i].type, channels[i].count, channels[i].id, caReadCallback, &channels[i]);
    				if (status != ECA_NORMAL) fprintf(stderr, "CA error %s occurred while trying to create ca_get request.\n", ca_message(status));

    				doneEach[i] = true;		//replaces ch.done
    				channels[i].done = false; //reset ch.done in order to reuse it by callback fcn
    			}
    		}

    		//exit if all of callbacks completed
    		bool doneAll = true;
    		for (i=0; i<nChannels; ++i){
    			doneAll &= doneEach[i];
    		}
    		if (doneAll) break;

    		//or if timeout
    		epicsTimeGetCurrent(&waitNow);
    		epicsTimeDiffFull(&elapsed, &waitStart, &waitNow);

    		if (elapsed.secPastEpoch + 1e-9*elapsed.nsec > arguments.w){
    			fprintf(stderr,"Timeout while waiting for an answer (put) from channel(s): ");
    			for (i=0; i<nChannels; ++i){
    				if (!doneEach[i]){
    					fprintf(stderr,"%s ", channels[i].name);
    				}
    			}
				fprintf(stderr,".\n");
    			break;
    		}
    	}

    	status = ca_flush_io ();
    	if(status == ECA_TIMEOUT) fprintf (stderr,"Flush error: %s.\n", ca_message(status));
    }

    //wait for read callbacks
    epicsTimeStamp waitStart;
    epicsTimeGetCurrent(&waitStart);

    epicsTimeStamp waitNow;
    epicsTimeStamp elapsed;
    bool doneEach[nChannels];
    for (i=0; i<nChannels; ++i) doneEach[i]=false;

    while(1){
		for (i=0; i<nChannels; ++i){
			if (channels[i].done){
				doneEach[i] = true;
			}
		}

		//break if all callbacks completed
		bool doneAll = true;
		for (i=0; i<nChannels; ++i){
			doneAll &= doneEach[i];
		}
		if (doneAll) break;

		//or if timeout
		epicsTimeGetCurrent(&waitNow);
		epicsTimeDiffFull(&elapsed, &waitStart, &waitNow);

		if (elapsed.secPastEpoch + 1e-9*elapsed.nsec > arguments.w){
			fprintf(stderr,"Timeout while waiting for an answer (get) from channel(s): ");
			for (i=0; i<nChannels; ++i){
				if (!doneEach[i]){
					fprintf(stderr,"%s ", channels[i].name);
				}
			}
			fprintf(stderr,".\n");
			break;
		}
    }

    //print final output or enter monitoring loop
    if (arguments.tool == caget || arguments.tool == caput){
    	for (i=0; i<nChannels; ++i){
    		printOutput(i);
    	}
    }
    else if (arguments.tool == camon || arguments.tool == cawait){

    	int numUpdates = 0;	//needed if -n
    	epicsTimeStamp lastUpdate[nChannels]; //needed if -timestamp u,c
    	for (i=0; i<=nChannels;++i){
    		lastUpdate[i].secPastEpoch=0;
    	}

    	while (1){

    		for (i=0; i<nChannels; ++i){

    			if (channels[i].done){

    				if (arguments.tool == cawait){
    					if (cawaitCondition(channels[i], i) != 1) {
    						channels[i].done = false;
    						continue;
    					}
    				}



    				if (arguments.timestamp == 'r') {
    					//calculate elapsed time since startTime
    					epicsTimeStamp elapsed;
    					int iSign;
    					iSign = epicsTimeDiffFull(&elapsed, &timestampRead[i], &programStartTime);

    					//convert to h,m,s,ns
    					struct tm tm;
    					unsigned long nsec;
    					epicsTimeToGMTM(&tm, &nsec, &elapsed);

    					//save to outTs string
    					char cSign = ' ';
    					if (iSign<0) cSign='-';
    					sprintf(outTimestamp[i],"%c%02d:%02d:%02d.%09lu", cSign,tm.tm_hour, tm.tm_min, tm.tm_sec, nsec);
    				}
    				else if(arguments.timestamp == 'c' || arguments.timestamp == 'u'){
						//calculate elapsed time since last update; if using 'c' keep
    					//timestamps separate for each channel, otherwise use lastUpdate[0]
    					//for all channels.
    					int ii=0;
    					if (arguments.timestamp == 'c') ii = i;
    					else if (arguments.timestamp == 'u') ii = 0;

    					if (lastUpdate[ii].secPastEpoch != 0){

    						epicsTimeStamp elapsed;
    						int iSign;
    						iSign = epicsTimeDiffFull(&elapsed, &timestampRead[i], &lastUpdate[ii]);

    						//convert to h,m,s,ns
    						struct tm tm;
    						unsigned long nsec;
    						epicsTimeToGMTM(&tm, &nsec, &elapsed);

    						//save to outTs string
    						char cSign = ' ';
    						if (iSign<0) cSign='-';
    						sprintf(outTimestamp[i],"%c%02d:%02d:%02d.%09lu", cSign,tm.tm_hour, tm.tm_min, tm.tm_sec, nsec);
    					}
    					else{
    						//this is the first update for this channel
    						sprintf(outTimestamp[i],"%19c",' ');
    					}

    					//reset
    					lastUpdate[ii] = timestampRead[i];
    				}

					printOutput(i);
					channels[i].done = false;

			    	if (arguments.numUpdates != -1)	{
			    		++numUpdates;
			    		if (numUpdates >= arguments.numUpdates) break;
			    	}
				}
    		}
    		//check if numUpdates is enough to exit program
    		if (arguments.numUpdates != -1 && numUpdates >= arguments.numUpdates) break;
    	}
    }
}

void cainfoRequest(struct channel *channels, int nChannels){
//this function does all the work for caInfo tool. Reads channel data using ca_get and then prints.
	int i,j, status;

	char locSev[30]="", locStat[30]="", locUnits[20+MAX_UNITS_SIZE]="";
	char locRISC[30]="";
	char precision[30]="", upperAlarmLimit[45]="", lowerAlarmLimit[45]="", upperWarningLimit[45]="", lowerWarningLimit[45]="";
	char upperCtrlLimit[45]="", lowerCtrlLimit[45]="";
	char upperDispLimit[45]="", lowerDispLimit[45]="";
	char enumStrs[MAX_ENUM_STATES][MAX_ENUM_STRING_SIZE], enumNoStr[30]="";
	char value[LEN_VALUE]="";
	bool readAccess, writeAccess;
	char description[MAX_STRING_SIZE]="", hhSevr[30]="", hSevr[30]="", lSevr[30]="", llSevr[30]="";


	for(i=0; i < nChannels; i++){
		channels[i].count = ca_element_count ( channels[i].id );

		channels[i].type = dbf_type_to_DBR_CTRL(ca_field_type(channels[i].id));

		void *data, *dataDesc, *dataHhsv, *dataHsv, *dataLsv, *dataLlsv;
		data = malloc(dbr_size_n(channels[i].type, channels[i].count));
		dataDesc = malloc(dbr_size_n(channels[i].type, 1));
		dataHhsv = malloc(dbr_size_n(channels[i].type, 1));
		dataHsv = malloc(dbr_size_n(channels[i].type, 1));
		dataLsv = malloc(dbr_size_n(channels[i].type, 1));
		dataLlsv = malloc(dbr_size_n(channels[i].type, 1));
		if (!data || !dataDesc || !dataHhsv || !dataHsv || !dataLsv || !dataLlsv){
			fprintf(stderr, "Memory allocation error.");
		}


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

		switch (channels[i].type){
		case DBR_CTRL_INT://and short
			sprintf(locStat, "%s", epicsAlarmConditionStrings[((struct dbr_ctrl_int *)data)->status]);
			sprintf(locSev, "%s",epicsAlarmSeverityStrings[((struct dbr_ctrl_int *)data)->severity]);
			sprintf(locUnits, "%s", ((struct dbr_ctrl_int *)data)->units);
			sprintf(upperDispLimit, "%d", ((struct dbr_ctrl_int *)data)->upper_disp_limit);
			sprintf(lowerDispLimit, "%d", ((struct dbr_ctrl_int *)data)->lower_disp_limit);
			sprintf(upperAlarmLimit, "%d", ((struct dbr_ctrl_int *)data)->upper_alarm_limit);
			sprintf(lowerAlarmLimit, "%d", ((struct dbr_ctrl_int *)data)->lower_alarm_limit);
			sprintf(upperWarningLimit, "%d", ((struct dbr_ctrl_int *)data)->upper_warning_limit);
			sprintf(lowerWarningLimit, "%d", ((struct dbr_ctrl_int *)data)->lower_warning_limit);
			sprintf(upperCtrlLimit, "%d", ((struct dbr_ctrl_int *)data)->upper_ctrl_limit);
			sprintf(lowerCtrlLimit, "%d", ((struct dbr_ctrl_int *)data)->lower_ctrl_limit);
			sprintf(value, "%d", ((struct dbr_ctrl_int *)data)->value);
			break;
		case DBR_CTRL_FLOAT:
			sprintf(locStat, "%s", epicsAlarmConditionStrings[((struct dbr_ctrl_float *)data)->status]);
			sprintf(locSev, "%s", epicsAlarmSeverityStrings[((struct dbr_ctrl_float *)data)->severity]);
			sprintf(locUnits, "%s", ((struct dbr_ctrl_float *)data)->units);
			sprintf(upperDispLimit, "%f", ((struct dbr_ctrl_float *)data)->upper_disp_limit);
			sprintf(lowerDispLimit, "%f", ((struct dbr_ctrl_float *)data)->lower_disp_limit);
			sprintf(upperAlarmLimit, "%f", ((struct dbr_ctrl_float *)data)->upper_alarm_limit);
			sprintf(lowerAlarmLimit, "%f", ((struct dbr_ctrl_float *)data)->lower_alarm_limit);
			sprintf(upperWarningLimit, "%f", ((struct dbr_ctrl_float *)data)->upper_warning_limit);
			sprintf(lowerWarningLimit, "%f", ((struct dbr_ctrl_float *)data)->lower_warning_limit);
			sprintf(upperCtrlLimit, "%f", ((struct dbr_ctrl_float *)data)->upper_ctrl_limit);
			sprintf(lowerCtrlLimit, "%f", ((struct dbr_ctrl_float *)data)->lower_ctrl_limit);
			sprintf(precision, "%d", ((struct dbr_ctrl_float *)data)->precision);
			sprintf(locRISC, "%d", ((struct dbr_ctrl_float *)data)->RISC_pad);
			sprintf(value, "%f", ((struct dbr_ctrl_float *)data)->value);
			break;
		case DBR_CTRL_ENUM:
			sprintf(locStat, "%s", epicsAlarmConditionStrings[((struct dbr_ctrl_enum *)data)->status]);
			sprintf(locSev, "%s", epicsAlarmSeverityStrings[((struct dbr_ctrl_enum *)data)->severity]);
			sprintf(enumNoStr, "%d", ((struct dbr_ctrl_enum *)data)->no_str);
			for (j=0; j<((struct dbr_ctrl_enum *)data)->no_str; ++j){
				sprintf(enumStrs[j], "%s", ((struct dbr_ctrl_enum *)data)->strs[j]);
			}
			sprintf(value, "%d", ((struct dbr_ctrl_enum *)data)->value);
			break;
		case DBR_CTRL_CHAR:
			sprintf(locStat, "%s", epicsAlarmConditionStrings[((struct dbr_ctrl_char *)data)->status]);
			sprintf(locSev, "%s", epicsAlarmSeverityStrings[((struct dbr_ctrl_char *)data)->severity]);
			sprintf(locUnits, "%s", ((struct dbr_ctrl_char *)data)->units);
			sprintf(upperDispLimit, "%c", ((struct dbr_ctrl_char *)data)->upper_disp_limit);
			sprintf(lowerDispLimit, "%c", ((struct dbr_ctrl_char *)data)->lower_disp_limit);
			sprintf(upperAlarmLimit, "%c", ((struct dbr_ctrl_char *)data)->upper_alarm_limit);
			sprintf(lowerAlarmLimit, "%c", ((struct dbr_ctrl_char *)data)->lower_alarm_limit);
			sprintf(upperWarningLimit, "%c", ((struct dbr_ctrl_char *)data)->upper_warning_limit);
			sprintf(lowerWarningLimit, "%c", ((struct dbr_ctrl_char *)data)->lower_warning_limit);
			sprintf(upperCtrlLimit, "%c", ((struct dbr_ctrl_char *)data)->upper_ctrl_limit);
			sprintf(lowerCtrlLimit, "%c", ((struct dbr_ctrl_char *)data)->lower_ctrl_limit);
			sprintf(locRISC, "%c", ((struct dbr_ctrl_char *)data)->RISC_pad);
			sprintf(value, "%c", ((struct dbr_ctrl_char *)data)->value);
			break;
		case DBR_CTRL_LONG:
			sprintf(locStat, "%s", epicsAlarmConditionStrings[((struct dbr_ctrl_long *)data)->status]);
			sprintf(locSev, "%s", epicsAlarmSeverityStrings[((struct dbr_ctrl_long *)data)->severity]);
			sprintf(locUnits, "%s", ((struct dbr_ctrl_long *)data)->units);
			sprintf(upperDispLimit, "%d", ((struct dbr_ctrl_long *)data)->upper_disp_limit);
			sprintf(lowerDispLimit, "%d", ((struct dbr_ctrl_long *)data)->lower_disp_limit);
			sprintf(upperAlarmLimit, "%d", ((struct dbr_ctrl_long *)data)->upper_alarm_limit);
			sprintf(lowerAlarmLimit, "%d", ((struct dbr_ctrl_long *)data)->lower_alarm_limit);
			sprintf(upperWarningLimit, "%d", ((struct dbr_ctrl_long *)data)->upper_warning_limit);
			sprintf(lowerWarningLimit, "%d", ((struct dbr_ctrl_long *)data)->lower_warning_limit);
			sprintf(upperCtrlLimit, "%d", ((struct dbr_ctrl_long *)data)->upper_ctrl_limit);
			sprintf(lowerCtrlLimit, "%d", ((struct dbr_ctrl_long *)data)->lower_ctrl_limit);
			sprintf(value, "%d", ((struct dbr_ctrl_long *)data)->value);
			break;
		case DBR_CTRL_DOUBLE:
			sprintf(locStat, "%s", epicsAlarmConditionStrings[((struct dbr_ctrl_double *)data)->status]);
			sprintf(locSev, "%s", epicsAlarmSeverityStrings[((struct dbr_ctrl_double *)data)->severity]);
			sprintf(locUnits, "%s", ((struct dbr_ctrl_double *)data)->units);
			sprintf(upperDispLimit, "%lf", ((struct dbr_ctrl_double *)data)->upper_disp_limit);
			sprintf(lowerDispLimit, "%lf", ((struct dbr_ctrl_double *)data)->lower_disp_limit);
			sprintf(upperAlarmLimit, "%lf", ((struct dbr_ctrl_double *)data)->upper_alarm_limit);
			sprintf(lowerAlarmLimit, "%lf", ((struct dbr_ctrl_double *)data)->lower_alarm_limit);
			sprintf(upperWarningLimit, "%lf", ((struct dbr_ctrl_double *)data)->upper_warning_limit);
			sprintf(lowerWarningLimit, "%lf", ((struct dbr_ctrl_double *)data)->lower_warning_limit);
			sprintf(upperCtrlLimit, "%lf", ((struct dbr_ctrl_double *)data)->upper_ctrl_limit);
			sprintf(lowerCtrlLimit, "%lf", ((struct dbr_ctrl_double *)data)->lower_ctrl_limit);
			sprintf(precision, "%d", ((struct dbr_ctrl_double *)data)->precision);
			sprintf(locRISC, "%d", ((struct dbr_ctrl_double *)data)->RISC_pad0);
			sprintf(value, "%lf", ((struct dbr_ctrl_double *)data)->value);
			break;
		}

		readAccess = ca_read_access(channels[i].id);
		writeAccess = ca_write_access(channels[i].id);

		sprintf(description, "%s", ((struct dbr_sts_string *)dataDesc)->value);
		sprintf(hhSevr, "%s", ((struct dbr_sts_string *)dataHhsv)->value);
		sprintf(hSevr, "%s", ((struct dbr_sts_string *)dataHsv)->value);
		sprintf(lSevr, "%s", ((struct dbr_sts_string *)dataLsv)->value);
		sprintf(llSevr, "%s", ((struct dbr_sts_string *)dataLlsv)->value);


		//print everything
		printf("\n");
		printf("\n");
		printf("%s\n", channels[i].name);
		printf("%s\n\n", description);
		printf("	Number of elements: %ld\n", channels[i].count);
		printf("	Value: %s\n", value);
		if (channels[i].type != DBR_CTRL_ENUM) {
			printf("	Units: %s\n", locUnits);
		}
		printf("	Alarm status: %s, severity: %s\n", locStat, locSev);
		printf("\n");
		printf("	Native DBF type: %s\n", dbf_type_to_text(ca_field_type(channels[i].id)));
		printf("\n");
		if (channels[i].type != DBR_CTRL_ENUM){
			printf("	Warning upper limit: %s, lower limit: %s\n", locStat, locSev);
			printf("	Alarm upper limit: %s, lower limit: %s\n", locStat, locSev);
			printf("	Control upper limit: %s, lower limit: %s\n", locStat, locSev);
			printf("	Display upper limit: %s, lower limit: %s\n", locStat, locSev);

			printf("\n");
			printf("	HIHI alarm severity: %s\n", hhSevr);
			printf("	HIGH alarm severity: %s\n", hSevr);
			printf("	LOW alarm severity: %s\n", lSevr);
			printf("	LOLO alarm severity: %s\n", llSevr);
			printf("\n");

			if (channels[i].type == DBR_CTRL_FLOAT || DBR_CTRL_DOUBLE){
				printf("	Precision: %s\n",precision);
			}

			if (channels[i].type == DBR_CTRL_FLOAT || channels[i].type == DBR_CTRL_DOUBLE || channels[i].type == DBR_CTRL_CHAR){
				printf("	RISC alignment: %s\n",locRISC);
			}
		}
		else if (channels[i].type == DBR_CTRL_ENUM){
			printf("	Number of enum strings: %s\n",enumNoStr);
			for (j=0; j<((struct dbr_ctrl_enum *)data)->no_str; ++j){
				printf("	string %d: %s\n", j, enumStrs[j]);
			}
		}
		printf("\n");
		printf("	IOC name: %s\n", ca_host_name(channels[i].id));
		printf("	Read access: "); if(readAccess) printf("yes\n"); else printf("no\n");
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

    status = ca_context_create(ca_enable_preemptive_callback);
    if (checkStatus(status) != EXIT_SUCCESS) return EXIT_FAILURE;

    for(i=0; i < nChannels; i++){
        status = ca_create_channel(channels[i].name, 0 , 0, CA_PRIORITY, &channels[i].id);
        if(checkStatus(status) != EXIT_SUCCESS) return EXIT_FAILURE;    // we exit if the channels are not created

        //if tool = cagets or cado, each channel has a sibling connecting to the proc field
        if (arguments.tool == cagets || arguments.tool == cado){
        	status = ca_create_channel(channels[i].procName, 0 , 0, CA_PRIORITY, &channels[i].procId);
        	if(checkStatus(status) != EXIT_SUCCESS) return EXIT_FAILURE;
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
        	if(checkStatus(status) != EXIT_SUCCESS) return EXIT_FAILURE;
        	status = ca_pend_io ( arguments.w );
        	if(status == ECA_TIMEOUT) printf ("Channel %s is missing DESC field.\n", channels[i].name);

        	status = ca_create_channel(channels[i].hhsvName, 0 , 0, CA_PRIORITY, &channels[i].hhsvId);
        	if(checkStatus(status) != EXIT_SUCCESS) return EXIT_FAILURE;
        	status = ca_pend_io ( arguments.w );
        	if(status == ECA_TIMEOUT) printf ("Channel %s is missing HHSV field.\n", channels[i].name);

        	status = ca_create_channel(channels[i].hsvName, 0 , 0, CA_PRIORITY, &channels[i].hsvId);
        	if(checkStatus(status) != EXIT_SUCCESS) return EXIT_FAILURE;
        	status = ca_pend_io ( arguments.w );
        	if(status == ECA_TIMEOUT) printf ("Channel %s is missing HSV field.\n", channels[i].name);


        	status = ca_create_channel(channels[i].lsvName, 0 , 0, CA_PRIORITY, &channels[i].lsvId);
        	if(checkStatus(status) != EXIT_SUCCESS) return EXIT_FAILURE;
        	status = ca_pend_io ( arguments.w );
        	if(status == ECA_TIMEOUT) printf ("Channel %s is missing LSV field.\n", channels[i].name);

        	status = ca_create_channel(channels[i].llsvName, 0 , 0, CA_PRIORITY, &channels[i].llsvId);
        	if(checkStatus(status) != EXIT_SUCCESS) return EXIT_FAILURE;
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
        {"fs",			required_argument,	0,  0 },	//array field separator
        {"nord",		no_argument,		0,  0 },	//display number of array elements
        {"tool",		required_argument, 	0,	0 },	//tool
        {0,         	0,                 	0,  0 }
    };
    putenv("POSIXLY_CORRECT="); //Behave correctly on GNU getopt systems = stop parsing after 1st non option is encountered
    while ((opt = getopt_long_only(argc, argv, ":w:d:e:f:g:n:sth", long_options, &opt_long)) != -1) {
        switch (opt) {
        case 'w':
        	if (sscanf(optarg, "%f", &arguments.w) != 1){    // type was not given as float
        		arguments.w = CA_TIMEOUT;
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
        	if (arguments.d < DBR_STRING    || arguments.d > DBR_CLASS_NAME
        			|| arguments.d == DBR_PUT_ACKT || arguments.d == DBR_PUT_ACKS)
        	{
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
            case 19:   // field separator
            	if (sscanf(optarg, "%c", &arguments.fieldSeparator) != 1){
            		fprintf(stderr, "Invalid argument '%s' "
            				"for option '-%c' - ignored.\n", optarg, opt);
            	}
            	break;
            case 20:   // nord
            	arguments.nord = true;
            	break;
            case 21:	// tool
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
            		} else {
            			arguments.tool = caget;
            			fprintf(stderr,	"Invalid tool call '%s' "
            					"for option '-%c' - caget assumed.\n", optarg, opt);
            		}
            	} else{ // type was given as a number
            		if(tool < caget|| tool > cainfo){   // out of range check
            			arguments.round = caget;
            			fprintf(stderr,	"Invalid tool call '%s' "
            					"for option '-%c' - caget assumed.\n", optarg, opt);
            		} else{
            			arguments.round = tool;
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
            usage(stdout, argv[0]);
            return EXIT_SUCCESS;
        default:
            usage(stderr, argv[0]);
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
		fprintf(stderr, "Precision -prec already specified as argument to -e, -f or -g - ignored.\n");
		arguments.prec = -1;
	}

	epicsTimeGetCurrent(&programStartTime);

	//Remaining arg list refers to PVs
	if (arguments.tool == caget || arguments.tool == camon || arguments.tool == cagets || arguments.tool == cado || arguments.tool == cainfo){
		nChannels = argc - optind;       // Remaining arg list are PV names
	}
	else if (arguments.tool == caput || arguments.tool == caputq){
		if ((argc - optind) % (arguments.inNelm +1)){ //it takes (inNelm+pv name) elements to define each channel
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
    channels = (struct channel *) calloc (nChannels, sizeof(struct channel));
    if (!channels) {
    	fprintf(stderr, "Memory allocation for channel structures failed.\n");
    	return EXIT_FAILURE;
    }
    for (i=0;i<nChannels;++i){
    	channels[i].writeStr = calloc (arguments.inNelm*MAX_STRING_SIZE, sizeof(char));
    	channels[i].procName = calloc (LEN_RECORD_NAME, sizeof(char));
    	channels[i].descName = calloc (LEN_RECORD_NAME, sizeof(char));
    	channels[i].hhsvName = calloc (LEN_RECORD_NAME, sizeof(char));
    	channels[i].hsvName = calloc (LEN_RECORD_NAME, sizeof(char));
    	channels[i].lsvName = calloc (LEN_RECORD_NAME, sizeof(char));
    	channels[i].llsvName = calloc (LEN_RECORD_NAME, sizeof(char));
        //name and condition strings dont have to be allocated because they merely point somewhere else
    	if (!channels[i].writeStr || !channels[i].procName || !channels[i].descName || !channels[i].hhsvName || \
    			!channels[i].hsvName || !channels[i].lsvName || !channels[i].llsvName ) {
    		fprintf(stderr, "Memory allocation for channel structures failed.\n");
    		return EXIT_FAILURE;
    	}
    }



    // Copy PV names from command line
    for (i = 0; optind < argc; i++, optind++){
        //printf("PV %d: %s\n", i, argv[optind]);
        channels[i].name = argv[optind];
        channels[i].i = i;	// channel number, serves to synchronise pvs and output.

        if (arguments.tool == caput || arguments.tool == caputq){
        	for (j=0; j<arguments.inNelm; ++j){	//the nelm next arguments are values
        		strcpy(&channels[i].writeStr[j*MAX_STRING_SIZE], argv[optind+1]);
        		optind++;
        	}
        	//xXx CE JE tu dolocen input separator, potem naredi parsing funkcijo ki sestavi writeStr iz enega samega argumenta.
        }
        else if (arguments.tool == cawait){
        	channels[i].conditionStr = argv[optind+1]; //next argument is the condition string
        	optind++;

        }
        else if(arguments.tool == cagets || arguments.tool == cado){

        	strcpy(channels[i].procName, channels[i].name);

        	//append .PROC
        	if (strcmp(&channels[i].procName[strlen(channels[i].procName) - 4], ".VAL") == 0){
        		//if last 4 elements equal .VAL, replace them
        		strcpy(&channels[i].procName[strlen(channels[i].procName) - 4],".PROC");
        	}
        	else{
        		//otherwise simply append
        		strcat(&channels[i].procName[strlen(channels[i].procName)],".PROC");
        	}
        }
        else if(arguments.tool == cainfo){
        	strcpy(channels[i].descName, channels[i].name);
        	strcpy(channels[i].hhsvName, channels[i].name);
        	strcpy(channels[i].hsvName, channels[i].name);
        	strcpy(channels[i].lsvName, channels[i].name);
        	strcpy(channels[i].llsvName, channels[i].name);

        	//append .DESC, .HHSV, .HSV, .LSV, .LLSV
        	if (strcmp(&channels[i].name[strlen(channels[i].name) - 4], ".VAL") == 0){
        		//if last 4 elements equal .VAL, replace them
        		strcpy(&channels[i].descName[strlen(channels[i].descName) - 4],".DESC");
        		strcpy(&channels[i].hhsvName[strlen(channels[i].hhsvName) - 4],".HHSV");
        		strcpy(&channels[i].hsvName[strlen(channels[i].hsvName) - 4],".HSV");
        		strcpy(&channels[i].lsvName[strlen(channels[i].lsvName) - 4],".LSV");
        		strcpy(&channels[i].llsvName[strlen(channels[i].llsvName) - 4],".LLSV");
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
	outValue = malloc(nChannels * sizeof(char *));
	outDate = malloc(nChannels * sizeof(char *));
	outTime = malloc(nChannels * sizeof(char *));
	outSev = malloc(nChannels * sizeof(char *));
	outStat = malloc(nChannels * sizeof(char *));
	outUnits = malloc(nChannels * sizeof(char *));
	outName = malloc(nChannels * sizeof(char *));
	outTimestamp = malloc(nChannels * sizeof(char *));
	outLocalDate = malloc(nChannels * sizeof(char *));
	outLocalTime = malloc(nChannels * sizeof(char *));
	if (!outValue || !outDate || !outTime || !outSev || !outStat || !outUnits || !outName || !outTimestamp){
		fprintf(stderr, "Memory allocation error.\n");
	}
	for(i = 0; i < nChannels; i++){
		outValue[i] = malloc(LEN_VALUE * sizeof(char));
		outDate[i] = malloc(LEN_TIMESTAMP * sizeof(char));
		outTime[i] = malloc(LEN_TIMESTAMP * sizeof(char));
		outSev[i] = malloc(LEN_SEVSTAT * sizeof(char));
		outStat[i] = malloc(LEN_SEVSTAT * sizeof(char));
		outUnits[i] = malloc(LEN_UNITS * sizeof(char));
		outName[i] = malloc(LEN_RECORD_NAME * sizeof(char));
		outTimestamp[i] = malloc(LEN_TIMESTAMP * sizeof(char));
		outLocalDate[i] = malloc(LEN_TIMESTAMP * sizeof(char));
		outLocalTime[i] = malloc(LEN_TIMESTAMP * sizeof(char));
		if (!outValue[i] || !outDate[i] || !outTime[i] || !outSev[i] || !outStat[i] ||\
				!outUnits[i] || !outName[i] || !outTimestamp[i] || !outDate[i] || !outTime[i]){
			fprintf(stderr, "Memory allocation error.\n");
		}
	}
	//memory for timestamp
	timestampRead = malloc(nChannels * sizeof(epicsTimeStamp));
	if (!timestampRead) fprintf(stderr, "Memory allocation error.\n");


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
    	free(outName[i]);
    	free(outTimestamp[i]);
    	free(outLocalDate[i]);
    	free(outLocalTime[i]);
    }
    free(outValue);
    free(outDate);
    free(outTime);
    free(outSev);
    free(outStat);
    free(outUnits);
    free(outName);
    free(outTimestamp);
    free(outLocalDate);
    free(outLocalTime);

    //free monitor's timestamp
    free(timestampRead);

    return EXIT_SUCCESS;
}
