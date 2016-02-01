/* caToolsMain.cpp */
/* Author:  Sašo Skube Date:    30JUL2015 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
#define CA_TIMEOUT 5

enum roundType{
    roundType_default=0,
    roundType_ceil=1,
    roundType_floor=2
};


struct arguments
{
   float w;					//ca timeout
   int  d;					//precision
   bool num;    			//same as -int
   enum roundType round;
   int prec;
   bool hex;
   bool bin;
   bool oct;
   bool plain;
   int digits;  			// precision for e, f, g option
   char dblFormatStr[30]; 	// Format string to print doubles (see -e -f -g option)
   bool s;					//try to interpret values as strings
   char stat;
   bool noname;
   bool nounit;

   int count;   			// Request and print / write up to <num> elements.
};

//const struct arguments arguments_init = {
struct arguments arguments = {
    CA_TIMEOUT,         // w
    -1,                 // d
    false,              // num
    roundType_default,  // round
    -1,                 // prec
    false,              // hex
    false,              // bin
    false,              // oct
    false,              // plain
    -1,                 // digits
    "%g",               // dblFormatStr
    false,              // s
    0,                  // stat
    0,                  // noname
    0,                  // nounit
    0                   // count
};

//struct arguments arguments = arguments_init;

struct channel{
    char            *name;  // the name of the channel
    chid            id;     // channel id
    long            type;   // dbr type
    long            count;  // element count
    bool            done;   // indicates if callback has done processing this channel
};


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

int valueToString(char *stringValue, unsigned int type, const void *dbr, int i){
    dbr_enum_t v;
    unsigned int baseType;
    void *value;

    value = dbr_value_ptr(dbr, type);
    baseType = type % (LAST_TYPE+1);   // convert appropriate TIME, GR, CTRL,... type to basic one
    // TODO everything
    switch (baseType) {
        case DBR_STRING:
            strcpy(stringValue, ((dbr_string_t*) value)[i]);
            break;

        case DBR_INT:
        	if (arguments.hex){
        		sprintf(stringValue, "%x", ((dbr_int_t*)value)[i]);
        	}
        	else if (arguments.oct){
        		sprintf(stringValue, "%o", ((dbr_int_t*)value)[i]);
        	}
        	else if (arguments.bin){
        		sprintBits(stringValue, sizeof(((dbr_int_t*)value)[i]), &((dbr_int_t*)value)[i]);
        	}
        	else{
        		sprintf(stringValue, "%d", ((dbr_int_t*)value)[i]);
        	}
            break;

        //case DBR_SHORT: equals DBR_INT
        //    sprintf(stringValue, "%d", ((dbr_short_t*)value)[i]);
        //    break;

        case DBR_FLOAT:
        	sprintf(stringValue, arguments.dblFormatStr, ((dbr_float_t*)value)[i]);
            break;

        case DBR_ENUM:
            v = ((dbr_enum_t *)value)[i];

            if (!arguments.num && !arguments.bin && !arguments.oct && !arguments.hex) { // if not requested as a number check if we can get string
                if (dbr_type_is_GR(type)) {
                    sprintf(stringValue, "%s", ((struct dbr_gr_enum *)dbr)->strs[v]);
                    break;
                }
                else if (dbr_type_is_CTRL(type)) {
                    sprintf(stringValue, "%s", ((struct dbr_ctrl_enum *)dbr)->strs[v]);
                    break;
                }
            }

            sprintf(stringValue, "%d", v); // TODO numFormatString?
            break;

        case DBR_CHAR:
            sprintf(stringValue, "%c", ((dbr_char_t*) value)[i]);
            break;

        case DBR_LONG:
        	if (arguments.hex){
        		sprintf(stringValue, "%x", ((dbr_long_t*)value)[i]);
        	}
        	else if (arguments.oct){
        		sprintf(stringValue, "%o", ((dbr_long_t*)value)[i]);
        	}
        	else if (arguments.bin){
        		sprintBits(stringValue, sizeof(((dbr_long_t*)value)[i]), &((dbr_long_t*)value)[i]);
        	}
        	else{
        		sprintf(stringValue, "%d", ((dbr_long_t*)value)[i]);
        	}
            break;

        case DBR_DOUBLE:
            sprintf(stringValue, arguments.dblFormatStr, ((dbr_double_t*)value)[i]);
            break;

        default:
            strcpy(stringValue, "Unrecognized DBR type");
    }

    return 0;
}


static void caCallback (evargs args){
    struct channel *ch = (struct channel *)args.usr;
    int i;
    char fieldSeparator = ' ';  // TODO get from arguments XXX ni zahtevano??
    int outNelm = -1;

    // output strings
#define LEN_TIMESTAMP 50
#define LEN_RECORD_NAME 61
    char timestamp[LEN_TIMESTAMP], severityStatus[2*30], units[20+MAX_UNITS_SIZE], ack[2*30], name[LEN_RECORD_NAME];
    //chsr precision[30], grLimits[6*45], ctrLimits[2*45], enums[30 + (MAX_ENUM_STATES * (20 + MAX_ENUM_STRING_SIZE))];
    int precision=-1;
    int status=0, severity=0;

    sprintf(timestamp,"%s","");
	sprintf(severityStatus,"%s","");
	sprintf(units,"%s","");
	sprintf(name,"%s","");
	sprintf(ack,"%s","");


    //templates for reading requested data
#define severity_status_get(T) \
    status = ((struct T *)args.dbr)->status; \
	severity = ((struct T *)args.dbr)->severity; \
	sprintf(severityStatus, "%s%c%s", epicsAlarmSeverityStrings[severity], fieldSeparator, epicsAlarmConditionStrings[status]);

#define timestamp_get(T) \
		epicsTimeToStrftime(timestamp, LEN_TIMESTAMP, "%Y-%m-%d %H:%M:%S.%06f", (epicsTimeStamp *)&(((struct T *)args.dbr)->stamp.secPastEpoch));

#define units_get(T) sprintf(units, "%s", ((struct T *)args.dbr)->units);

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
            sprintf(ack, "(ACKT:%d ACKS:%d)", ((struct dbr_stsack_string *)args.dbr)->ackt, ((struct dbr_stsack_string *)args.dbr)->acks);
            break;

        default :
            //strcpy (s, "Can not print %s DBR type", dbr_type_to_text(args.type));
            printf("Can not print %s DBR type", dbr_type_to_text(args.type));
            break;
    } // TODO add the rest.... XXX kaksen rest

   /* XXX kaj nucajo tile
	if (args.type >= DBR_CTRL_STRING ) {    // upper and lower control limits are available

	}

    if (args.type >= DBR_GR_STRING ) {      // egu, precision, upper and lower value, alarm and display limits are available

    }

    if (args.type >= DBR_TIME_STRING ) {    // time is available

    }

    if (args.type >= DBR_STS_STRING ) {     // status and severity are available

    }*/


    //construct output
    //check arguments
    if (!arguments.noname){
    	sprintf(name, ch->name);
    }

    if (arguments.nounit){
    	sprintf(units,"%s","");
    }

    if (arguments.stat == 0){//display if alarm
    	if (status == 0 && severity == 0){
        	sprintf(severityStatus,"%s","");
    	}
    }
    else if (arguments.stat == 1){//always display
    	//do nothing
    }
    else if (arguments.stat == 2){//never display
    	sprintf(severityStatus, "%s","");
    }

    //date, time, channel name
    printf( "%s%c%s%c", timestamp, fieldSeparator, name, fieldSeparator);

    //values
    if (outNelm > 0 && outNelm < args.count) args.count = outNelm; //TODO outNelm = zahtevano stevilo elementov

    char valueStr[100];//XXX max size of output string?hmk??
	sprintf(valueStr,"%s","");

    for(i=0; i < args.count; i++){
    	if (i) printf ("%c", fieldSeparator);

    	valueToString(valueStr + strlen(valueStr), args.type, args.dbr, i);
    }
	printf("%s", valueStr);


    //egu, severity, status
    printf( "%c%s%c%s\n", fieldSeparator, units, fieldSeparator, severityStatus);

    //finish
    ch->done = true;
}




int caInit(struct channel *channels, int nChannels){
    int status, i;

    status = ca_context_create(ca_enable_preemptive_callback);
    if (checkStatus(status) != EXIT_SUCCESS) return EXIT_FAILURE;

    for(i=0; i < nChannels; i++){
        // TODO if monitor = create subscription
        //status = ca_create_channel(pv, &channelStatusCallback, 0, CA_PRIORITY, &channelId);
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

        if(arguments.count > 0 && arguments.count < channels[i].count) channels[i].count = arguments.count;

        channels[i].done = false;
        if (arguments.d == -1){
        	//if DBR type not specified, use native.
        	//default: GR
        	channels[i].type = dbf_type_to_DBR_GR(ca_field_type(channels[i].id));
        	//TODO if only time and/or severity&status, use DBR_TIME_native
        	//TODO if more, make 2 requests: first time, then GBR???
        }
        else{
        	channels[i].type = arguments.d;
        }
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
        // TODO add argument -silent
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
        {"num",     no_argument,        0,  0 },
        {"int",     no_argument,        0,  0 },    // same as num
        {"round",   optional_argument,  0,  0 },	//type of rounding:default, ceil, floor
        {"prec",    required_argument,  0,  0 },	//precision
        {"hex",     no_argument,        0,  0 },	//display as hex
        {"bin",     no_argument,        0,  0 },	//display as bin
        {"oct",     no_argument,        0,  0 },	//display as oct
        {"plain",   no_argument,        0,  0 },	//ignore formatting switches
        {"stat",    required_argument,  0,  0 },	//status and severity XXX???
        {"noname",  no_argument,        0,  0 },	//hide name
        {"nounit",  no_argument,        0,  0 },	//hide units
        {0,         0,                  0,  0 }
    };	//TODO COUNT!
    // TODO mutually exclusive
    while ((opt = getopt_long_only(argc, argv, "w:d:e:f:g:sh", long_options, &opt_long)) != -1) {
        switch (opt) {
        case 'w':
        	if (sscanf(optarg, "%f", &arguments.w) != 1){    // type was not given as float
        		arguments.w = CA_TIMEOUT;
        		fprintf(stderr, "Requested timeout invalid - ignored. ('%s -h' for help.)\n", argv[0]);
        	}
            break;
        case 'd':
            if (sscanf(optarg, "%d", &arguments.d) != 1)    // type was not given as an integer
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
        case 'e':	//how to show format doubles in strings
        case 'f':
        case 'g':
            if (sscanf(optarg, "%d", &arguments.digits) != 1){
                fprintf(stderr,
                        "Invalid precision argument '%s' "
                        "for option '-%c' - ignored.\n", optarg, opt);
            } else {
                if (arguments.digits>=0)    // TODO: max value??
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
        case 0:   // long options
            switch (opt_long){
            case 0:   // num
                arguments.num = true;
                break;
            case 1:   // int
                arguments.num = true;
                break;
            case 2:   // round
            	;//needs to precede declaration because of C
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
                        arguments.round = roundType_default;
                        fprintf(stderr,
                            "Invalid round type '%s' "
                            "for option '-%c' - ignored.\n", optarg, opt);
                    } else{
                        //arguments.round = (roundType)type;//casting causes error?? XXX
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
                //TODO error checking
                arguments.stat = atoi(optarg);
                break;
            case 9:   // noname
                arguments.noname = true;
                break;
            case 10:   // nounit
                arguments.nounit = true;
                break;
            }
            break;
        case '?':
            fprintf(stderr, "Unrecognized option: '-%c'. ('%s -h' for help.)\n", optopt, argv[0]);
            return EXIT_FAILURE;
        case ':':
            fprintf(stderr, "Option '-%c' requires an argument. ('%s -h' for help.)\n", optopt, argv[0]);
            return EXIT_FAILURE;
        case 'h':               /* Print usage */
            usage(stdout, argv[0]);
            return EXIT_SUCCESS;
        default:
            usage(stderr, argv[0]);
            exit(EXIT_FAILURE);
        }
    }

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
    }

    dumpArguments(&arguments);

    if(caInit(channels, nChannels) != EXIT_SUCCESS) return EXIT_FAILURE;
    caRequest(channels, nChannels);
    if (caDisconnect(channels, nChannels) != EXIT_SUCCESS) return EXIT_FAILURE;

    free(channels);

    return EXIT_SUCCESS;
}
