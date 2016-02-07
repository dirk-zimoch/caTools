/* caToolsMain.cpp */
/* Author:  Sašo Skube Date:    30JUL2015 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h> // XXX is ok?

#include "cadef.h"  // for channel access
#include "alarmString.h"  // XXX is ok?
#include <getopt.h> // for getopt() epicsGetopt.h ???

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

//*******************************//
//      in order to use sleep()  //
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif
//*******************************//


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
	camon=2
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
   int count;   			// Request and print / write up to <num> elements.
   char fieldSeparator;		//array field separator
   enum tool tool;			//tool type
   int numUpdates;			//number of monitor updates after which to quit
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
    -1 ,                // count
    ' '	,				// field separator
    caget,				// tool
    -1					// numUpdates
};

struct channel{
    char            *name;  // the name of the channel
    chid            id;     // channel id
    long            type;   // dbr type
    long            count;  // element count
    bool            done;   // indicates if callback has done processing this channel
    int 			i;		// process value id
};


//output strings
#define LEN_TIMESTAMP 50
#define LEN_RECORD_NAME 61
#define LEN_VALUE 100 //XXX max value size?hmk??
#define LEN_SEVSTAT 30
#define LEN_UNITS 20+MAX_UNITS_SIZE
char **outDate,**outTime, **outSev, **outStat, **outUnits, **outName, **outValue;
char **outTimestamp; //relative timestamps for camon

//timestamps of received data (per channel)
epicsTimeStamp *timestampRead;


void usage(FILE *stream, char *programName){
    fprintf(stream, "Usage: %s [-w] [-d <type>] pvList\n", programName);
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


/*rabimo to?? ne dela v C XXX
void channelStatusCallback(connection_handler_args args){
    printf("Connection status changed:\n"
           "Channel id: %ld went %ld [6 = up\t7 = down]\n",
           (long)args.chid, args.op);
}*/

int checkStatus(int status){
    if (status != ECA_NORMAL){
        fprintf(stderr, "CA error %s occurred while trying "
                "to start channel access.\n", ca_message(status));  // TODO error message not the best one
        SEVCHK(status, "CA error");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}


void sprintBits(char *outStr, size_t const size, void const * const ptr)
//Converts a decimal number to binary and saves it to outStr.
//The decimal number is pointed to by ptr and is of memory size size.
//Assumes little endian.
{
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

int valueToString(char *stringValue, unsigned int type, const void *dbr, int count){
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
    			sprintf(stringCursor, arguments.dblFormatStr, ((dbr_float_t*)value)[i]);
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
    				sprintf(stringCursor, "%s", ((struct dbr_gr_enum *)dbr)->strs[v]);
    				break;
    			}
    			else if (dbr_type_is_CTRL(type)) {
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
    			sprintf(stringCursor, arguments.dblFormatStr, ((dbr_double_t*)value)[i]);
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
		if (!arguments.num && !arguments.bin && !arguments.hex && !arguments.oct) { // if not requested as a number check if we can get string
			if (dbr_type_is_GR(args.type)) {
				sprintf(outString, "%s", ((struct dbr_gr_enum *)args.dbr)->strs[v]);
				continue;
			}
			else if (dbr_type_is_CTRL(args.type)) {
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
// prints output strings corresponding to i-th channel.

    //date
	if (strcmp(outDate[i],"") != 0)	printf("%s ",outDate[i]);

	//time
	if (strcmp(outTime[i],"") != 0)	printf("%s ",outTime[i]);

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
// left - right: like epicsTimeDiffInSeconds but also taking nanoseconds into account.
// The timestamp is always positive, sign is output by the return value.
	double timeLeft = pLeft->secPastEpoch + 1e-9 * pLeft->nsec;
	double timeRight = pRight->secPastEpoch + 1e-9 * pRight->nsec;

	double difference = (timeLeft - timeRight);

	diff->secPastEpoch = fabs(trunc(difference));	//number of seconds is the integer part.
	diff->nsec = 1e9 * (fabs(difference) - fabs(trunc(difference)));	//number of nanoseconds is the decimal part.
	if (difference > 0){
		return 1;
	}
	else if(difference < 0){
		return -1;
	}
	else{
		return 0;
	}

}

static void caCallback (evargs args){

	//check if data was returned
	if (args.status != ECA_NORMAL){
		fprintf(stderr, "No data received from server, error code: %d.\n", args.status);
		return;
	}

    struct channel *ch = (struct channel *)args.usr;

    // output strings - local copies
    char locDate[LEN_TIMESTAMP],locTime[LEN_TIMESTAMP], locSev[30], locStat[30], locUnits[20+MAX_UNITS_SIZE], locAck[2*30], locName[LEN_RECORD_NAME];
    //chsr precision[30], grLimits[6*45], ctrLimits[2*45], enums[30 + (MAX_ENUM_STATES * (20 + MAX_ENUM_STRING_SIZE))];
    int precision=-1;
    int status=0, severity=0;

    //initialize local copies of output strings
    sprintf(locDate,"%s","");
    sprintf(locTime,"%s","");
	sprintf(locSev,"%s","");
	sprintf(locStat,"%s","");
	sprintf(locUnits,"%s","");
	sprintf(locName,"%s","");
	//there is no locValue
	sprintf(locAck,"%s","");//?

	//also clear global output strings; the purpose of this callback is to overwrite them
	sprintf(outDate[ch->i],"%s","");
	sprintf(outTime[ch->i],"%s","");
	sprintf(outSev[ch->i],"%s","");
	sprintf(outStat[ch->i],"%s","");
	sprintf(outUnits[ch->i],"%s","");
	sprintf(outName[ch->i],"%s","");
	sprintf(outValue[ch->i],"%s","");


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

        case DBR_STSACK_STRING: // do sutff here XXX kaksen stuff
            severity_status_get(dbr_stsack_string);
            //??sprintf(ack, "(ACKT:%d ACKS:%d)", ((struct dbr_stsack_string *)args.dbr)->ackt, ((struct dbr_stsack_string *)args.dbr)->acks);
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
	//use local instead?
	if (arguments.localdate || arguments.localtime){
		time_t localTime = time(0);
		if (arguments.localdate){
			strftime(locDate, LEN_TIMESTAMP, "%Y-%m-%d", localtime(&localTime));
		}
		if (arguments.localtime){
			strftime(locTime, LEN_TIMESTAMP, "%H:%M:%S", localtime(&localTime));
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

	if (args.type >= DBR_STS_STRING && args.type <= DBR_TIME_DOUBLE && \
			arguments.s && ca_field_type(ch->id) == DBF_ENUM){
		//special case: field type is enum, dbr type is time or sts, and string is requested.
		//In this case, two requests are needed.
		//we already have time, but to obtain value in the form of string, another request is needed.

		status = ca_array_get_callback(DBR_GR_ENUM, ch->count, ch->id, enumToString, ch);
		if (status == ECA_DISCONN) {
			printf("Channel %s is unable to connect.\n", ch->name);
		}
		else if (status == ECA_BADTYPE) {
			printf("Invalid DBR type requested for channel %s.\n", ch->name);
		}
		else if (status == ECA_BADCHID) {
			printf("Channel %s has a corrupted ID.\n", ch->name);
		}
		else if (status == ECA_BADCOUNT) {
			printf("Channel %s: Requested count larger than native element count.\n", ch->name);
		}
		else if (status == ECA_GETFAIL) {
			printf("Channel %s: A local database get failed.\n", ch->name);
		}
		else if (status == ECA_NORDACCESS) {
			printf("Read access denied for channel %s.\n", ch->name);
		}
		else if (status == ECA_ALLOCMEM) {
			printf("Unable to allocate memory for channel %s.\n", ch->name);
		}
		status = ca_flush_io();
		if(status != ECA_NORMAL){
			printf ("Problem flushing channel %s.\n", ch->name);
		}


	}
	else{ //get values as usual
		valueToString(outValue[ch->i], args.type, args.dbr, args.count);
		//finish
		ch->done = true;

	}
}




int caInit(struct channel *channels, int nChannels){
    int status, i;

    status = ca_context_create(ca_enable_preemptive_callback);
    if (checkStatus(status) != EXIT_SUCCESS) return EXIT_FAILURE;

    for(i=0; i < nChannels; i++){
        status = ca_create_channel(channels[i].name, 0 , 0, CA_PRIORITY, &channels[i].id);
        if(checkStatus(status) != EXIT_SUCCESS) return EXIT_FAILURE;    // we exit if the channels are not created
    }

    status = ca_pend_io ( arguments.w ); // will wait until channels connect because create channel is not using a callback
    if(status == ECA_TIMEOUT){
        printf ("Some of the channels did not connect\n");
    }

    return EXIT_SUCCESS;
}

void caRequest(struct channel *channels, int nChannels){
    int status, i, retry = 0;
    bool done = false;

    for(i=0; i < nChannels; i++){
        channels[i].count = ca_element_count ( channels[i].id );
        printf("# of elements in %s: %lu\n", channels[i].name, channels[i].count);

        //how many elements to request
        if(arguments.count > 0 && arguments.count < channels[i].count) channels[i].count = arguments.count;

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
			status = ca_array_get_callback(channels[i].type, channels[i].count, channels[i].id, caCallback, &channels[i]);
			if (status == ECA_DISCONN) {
				printf("Channel %s is unable to connect.\n", channels[i].name);
			}
			else if (status == ECA_BADTYPE) {
				printf("Invalid DBR type requested for channel %s.\n", channels[i].name);
			}
			else if (status == ECA_BADCHID) {
				printf("Channel %s has a corrupted ID.\n", channels[i].name);
			}
			else if (status == ECA_BADCOUNT) {
				printf("Channel %s: Requested count larger than native element count.\n", channels[i].name);
			}
			else if (status == ECA_GETFAIL) {
				printf("Channel %s: A local database get failed.\n", channels[i].name);
			}
			else if (status == ECA_NORDACCESS) {
				printf("Read access denied for channel %s.\n", channels[i].name);
			}
			else if (status == ECA_ALLOCMEM) {
				printf("Unable to allocate memory for channel %s.\n", channels[i].name);
			}
        }
        else if (arguments.tool == camon){
			status = ca_create_subscription(channels[i].type, channels[i].count, channels[i].id, DBE_VALUE | DBE_ALARM | DBE_LOG,\
					caCallback, &channels[i], NULL);
			if (status != ECA_NORMAL) fprintf(stderr, "CA error %s occurred while trying to create monitor.\n", ca_message(status));
        }


    }

    status = ca_pend_io ( arguments.w ); // will wait until channels connect (create channel is without callback)
    if(status == ECA_TIMEOUT){
        printf ("Some of the channels are not connected\n");
    }

    while(!done || retry < 5){ // TODO set retry
        done = true;
        for(i=0; i < nChannels; i++){
            done &= channels[i].done;
        }
        retry++;
#ifdef _WIN32
        Sleep(1);
#else
        usleep(1000);  /* sleep for 1000 milliSeconds */
#endif
    }
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
{
    int opt;                    // getopt() current option
    int opt_long;               // getopt_long() current long option
    int nChannels;              // Number of channels
    int i;                      // counter
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
        {"nelm",		required_argument,	0,  0 },	//number of array elements
        {"fs",			required_argument,	0,  0 },	//array field separator
        {"tool",		required_argument, 	0,	0 },	//tool
        {0,         	0,                 	0,  0 }
    };
    // TODO mutually exclusive arguments!
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
                    if (arguments.prec < 0) {    // TODO: max value??
                        fprintf(stderr, "Precision %d for option '-%c' "
                                "out of range - ignored.\n", arguments.prec, opt);
                        arguments.prec = -1;
                    }
                }
                break;
            case 4:   //hex
                arguments.hex = true; //TODO warning if these requested for non integer type
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
            case 17:   // number of elements
            	if (sscanf(optarg, "%d", &arguments.count) != 1){
            		fprintf(stderr, "Invalid count argument '%s' "
            				"for option '-%c' - ignored.\n", optarg, opt);
            	}
            	else {
            		if (arguments.count < 1) {
            			fprintf(stderr, "Count number for option '-%c' "
            					"must be positive integer - ignored.\n", opt);
            			arguments.count = -1;
            		}
            	}
            	break;
            case 18:   // field separator
            	if (sscanf(optarg, "%c", &arguments.fieldSeparator) != 1){
            		fprintf(stderr, "Invalid argument '%s' "
            				"for option '-%c' - ignored.\n", optarg, opt);
            	}
            	break;
            case 19:	// tool
            	;//c
            	int tool;
            	if (sscanf(optarg, "%d", &tool) != 1){   // type was not given as a number [0, 1, 2]
            		if(!strcmp("caget", optarg)){
            			arguments.tool = caget;
            		} else if(!strcmp("camon", optarg)){
            			arguments.tool = camon;
            		} else {
            			arguments.tool = caget;
            			fprintf(stderr,	"Invalid tool call '%s' "
            					"for option '-%c' - caget assumed.\n", optarg, opt);
            		}
            	} else{ // type was given as a number
            		if(tool < caget|| tool > camon){   // out of range check
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

    epicsTimeStamp startTime;	//timestamp indicating program start, needed by -timestamp
	epicsTimeGetCurrent(&startTime);

    nChannels = argc - optind;       // Remaining arg list are PV names
    if (nChannels < 1)
    {
        fprintf(stderr, "No channel name specified. ('%s -h' for help.)\n", argv[0]);
        return EXIT_FAILURE;
    }

    channels = (struct channel *) calloc (nChannels, sizeof(struct channel));
    if (!channels) {
        fprintf(stderr, "Memory allocation for channel structures failed.\n");
        return EXIT_FAILURE;
    }
    for (i = 0; optind < argc; i++, optind++){  // Copy PV names from command line
        printf("PV %d: %s\n", i, argv[optind]);
        channels[i].name = argv[optind];
        channels[i].i = i;	// channel number, serves to synchronise pvs and output.
    }

    dumpArguments(&arguments);

    //allocate memory for output strings
	outValue = malloc(nChannels * sizeof(char *));
	outDate = malloc(nChannels * sizeof(char *));
	outTime = malloc(nChannels * sizeof(char *));
	outSev = malloc(nChannels * sizeof(char *));
	outStat = malloc(nChannels * sizeof(char *));
	outUnits = malloc(nChannels * sizeof(char *));
	outName = malloc(nChannels * sizeof(char *));
	outTimestamp = malloc(nChannels * sizeof(char *));
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
		if (!outValue[i] || !outDate[i] || !outTime[i] || !outSev[i] || !outStat[i] ||\
				!outUnits[i] || !outName[i] || !outTimestamp[i]){
			fprintf(stderr, "Memory allocation error.\n");
		}
	}
	//memory for timestamp
	timestampRead = malloc(nChannels * sizeof(epicsTimeStamp));
	if (!timestampRead) fprintf(stderr, "Memory allocation error.\n");


    if(caInit(channels, nChannels) != EXIT_SUCCESS) return EXIT_FAILURE;
    caRequest(channels, nChannels);

    if (arguments.tool == caget){
    	for (i=0; i<nChannels; ++i){
    		printOutput(i);
    	}
    }
    else if (arguments.tool == camon){

    	int numUpdates = 0;	//needed if -n
    	epicsTimeStamp lastUpdate[nChannels]; //needed if -timestamp u,c
    	for (i=0; i<=nChannels;++i){
    		lastUpdate[i].secPastEpoch=0;
    	}

    	while (1){

    		for (i=0; i<nChannels; ++i){

    			if (channels[i].done){

    				if (arguments.timestamp == 'r') {
    					//calculate elapsed time since startTime
    					epicsTimeStamp elapsed;
    					int iSign;
    					iSign = epicsTimeDiffFull(&elapsed, &timestampRead[i], &startTime);

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
    					int ii;
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

    if (caDisconnect(channels, nChannels) != EXIT_SUCCESS) return EXIT_FAILURE;

    free(channels);
    //TODO free everything

    return EXIT_SUCCESS;
}
