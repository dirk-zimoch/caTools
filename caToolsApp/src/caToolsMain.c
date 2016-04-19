/* caToolsMain.c */
/* Author:  Rok Vuga     Date:    FEB 2016 */
/* Author:  Tomaz Sustar Date:    FEB 2016 */
/* Author:  Tom Slejko   Date:    FEB 2016 */


#include <stdio.h>
#include <getopt.h>
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
#include "caToolsTypes.h"
#include "caToolsGlobals.c"
#include "caToolsOutput.h"
#include "caToolsInput.h"
#include "caToolsUtils.h"

int cawaitEvaluateCondition(struct channel channel, evargs args){
/* At some point change the comments fom C++ to C style */


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

int getTimeStamp(u_int32_t i) {
//calculates timestamp for monitor tool, formats it and saves it into the global string.

    epicsTimeStamp elapsed;
    bool negative=false;
    u_int32_t commonI = (arguments.timestamp == 'c') ? i : 0;
    bool showEmpty = false;

    if (arguments.timestamp == 'r') {
        //calculate elapsed time since startTime
        negative = epicsTimeDiffFull(&elapsed, &g_timestampRead[i], &g_programStartTime);
    }
    else if(arguments.timestamp == 'c' || arguments.timestamp == 'u') {
        //calculate elapsed time since last update; if using 'c' keep
        //timestamps separate for each channel, otherwise use lastUpdate[0]
        //for all channels (commonI).

        // firstUpodate var is set at the end of caReadCallback, just before printing results.
        if (g_firstUpdate[i]) {
            negative = epicsTimeDiffFull(&elapsed, &g_timestampRead[i], &g_lastUpdate[commonI]);
        }
        else {
            g_firstUpdate[i] = true;
            showEmpty = true;
        }

        g_lastUpdate[commonI] = g_timestampRead[i]; // reset
    }

    //convert to h,m,s,ns
    struct tm tm;
    unsigned long nsec;
    int status = epicsTimeToGMTM(&tm, &nsec, &elapsed);
    assert(status == epicsTimeOK);

    if (showEmpty) {
        //this is the first update for this channel
        sprintf(g_outTimestamp[i],"%19c",' ');
    }
    else {  //save to outTs string
        char cSign = negative ? '-' : ' ';
        sprintf(g_outTimestamp[i],"%c%02d:%02d:%02d.%09lu", cSign,tm.tm_hour, tm.tm_min, tm.tm_sec, nsec);
    }

    return 0;
}

//macros for reading requested data
#define severity_status_get(T) \
status = ((struct T *)args.dbr)->status; \
severity = ((struct T *)args.dbr)->severity;
#define timestamp_get(T) \
    g_timestampRead[ch->i] = ((struct T *)args.dbr)->stamp;\
    validateTimestamp(&g_timestampRead[ch->i], ch->base.name);
#define units_get_cb(T) clearStr(g_outUnits[ch->i]); sprintf(g_outUnits[ch->i], "%s", ((struct T *)args.dbr)->units);
#define precision_get(T) precision = (((struct T *)args.dbr)->precision);




static void caReadCallback (evargs args){
//reads and parses data fetched by calls. First, the global strings holding the output are cleared. Then, depending
//on the type of the returned data, the available information is extracted. The extracted info is then saved to the
//global strings and printed.

    //if we are in monitor and just waiting for the program to exit, don't proceed.
    bool monitor = (arguments.tool == camon || arguments.tool == cawait);
    if (monitor && g_runMonitor == false) return;

    //check if data was returned
    if (args.status != ECA_NORMAL){
        errPeriodicPrint("Error in read callback. %s.\n", ca_message(args.status));
        return;
    }
    struct channel *ch = (struct channel *)args.usr;
    debugPrint("ReadCallback() callbacks to process: %i\n", ch->nRequests);
    ch->nRequests=ch->nRequests-1;
    int32_t precision=-1;
    u_int32_t status=0, severity=0;

    //clear global output strings; the purpose of this callback is to overwrite them
    //the exception are units, which we may be getting from elsewhere; we only clear them if we can write them
    clearStr(g_outDate[ch->i]);
    clearStr(g_outTime[ch->i]);
    clearStr(g_outSev[ch->i]);
    clearStr(g_outStat[ch->i]);
    clearStr(g_outLocalDate[ch->i]);
    clearStr(g_outLocalTime[ch->i]);

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
            //handle long strings - remember long string
            if(ca_field_type(ch->base.id)==DBF_STRING) {
                ch->longStr = mallocMustSucceed(args.count, "caReadCallback");
                strcpy(ch->longStr, (char*) dbr_value_ptr(args.dbr, args.type)); //TOOD check if this is valid later
            }
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

        case DBR_CHAR:   
        case DBR_STRING://dont print the warning if any of these
        case DBR_SHORT:
        case DBR_FLOAT:
        case DBR_ENUM:            
        case DBR_LONG:
        case DBR_DOUBLE: break;
        default :
            errPeriodicPrint("Can not print %s DBR type (DBR numeric type code: %ld). \n", dbr_type_to_text(args.type), args.type);
            return;
    }

    // remember status and severity
    ch->status = status;
    ch->severity = severity;


    //if monitor, there is extra stuff to do before printing data out.
    bool shouldPrint = true;

    if (arguments.tool == cawait || arguments.tool == camon) {
        if (arguments.timestamp) getTimeStamp(ch->i);	//calculate relative timestamps.

        if (arguments.numUpdates != -1) {
            g_numMonitorUpdates++;	//increase counter of updates

            if (g_numMonitorUpdates > arguments.numUpdates) {
                g_runMonitor = false;
            }
        }

        if (arguments.tool == cawait) {
            //check stop condition
            if (arguments.timeout!=-1) {
                epicsTimeStamp timeoutNow;
                epicsTimeGetCurrent(&timeoutNow);

                if (epicsTimeGreaterThanEqual(&timeoutNow, &g_timeoutTime)) {
                    //we are done waiting
                    printf("Condition not met in %f seconds - exiting.\n",arguments.timeout);
                    g_runMonitor = false;
                    shouldPrint = false;
                }
            }

            //check display condition
            if (cawaitEvaluateCondition(*ch, args) == 1) {
                g_runMonitor = false;
            }else{
                shouldPrint = false;
            }
        }
    }else if(ch->nRequests > 0){
        // wait for all requests
        shouldPrint = false;
    }



    //finish
    if(shouldPrint) printOutput(ch->i, args, precision, &arguments);
    ch->base.done = true;
}

static void caWriteCallback (evargs args) {
//does nothing except signal that writing is finished.

    debugPrint("caWriteCallback()\n");
    //check if status is ok
    if (args.status != ECA_NORMAL){
        errPrint("Error in write callback. %s.\n", ca_message(args.status));
        return;
    }

    struct channel *ch = (struct channel *)args.usr;

    ch->base.done = true;
}

void monitorLoop (){
//runs monitor loop. Stops after -n updates (camon, cawait) or
//after -timeout is exceeded (cawait).

    while (g_runMonitor){
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

void waitForCallbacks(struct channel *channels, u_int32_t nChannels) {
    u_int32_t i;
    size_t j;
    bool elapsed = false, allDone = false;
    epicsTimeStamp timeoutNow, timeout;
    size_t nFields = noFields;

    epicsTimeGetCurrent(&timeout);
    epicsTimeAddSeconds(&timeout, arguments.caTimeout);

    while(!allDone) {
        ca_pend_event(0.1); //Consider lowering this something like a millisecond, define it amongst defines
        // check for timeout
        epicsTimeGetCurrent(&timeoutNow);

        /*
         * LOWPRIO, no need to change time
         * This snippet indeed checks for timeouts, but it would make more sense if it would check for a per
         * channel timeout instead.
         */
        if (epicsTimeGreaterThanEqual(&timeoutNow, &timeout)) {
            warnPrint("Timeout while waiting for PV response (more than %f seconds elapsed).\n", arguments.caTimeout);

            // reset completition flags
            for (i=0; i < nChannels; ++i) {
                channels[i].nRequests = 0;
            }

            break;
        }

        // check for callback completition
        allDone = true;
        for (i=0; i < nChannels; ++i) {
            if (channels[i].nRequests > 0){
                allDone = false;
                break;
            }
        }
    }
    debugPrint("waitForCallbacks - done.");

}



bool caGenerateWriteRequests(struct channel *ch, arguments_T * arguments){
    debugPrint("caGenerateWriteRequests() - %s\n", ch->base.name);
    int status;    

    //request ctrl type
    int32_t baseType = ca_field_type(ch->base.id);

    ch->nRequests=0;
   
   if (arguments->tool == caput || arguments->tool == caputq) {        

        //if(arguments.bin) base = 2; TODO can be used to select binary input for numbers
        void * input;

        /* parse as array if needed */
        parseAsArray(ch, arguments);
        if (castStrToDBR(&input, ch->writeStr, ch->inNelm, baseType)) {
            //dbr_char_t * putin = (dbr_char_t *) input;
            debugPrint("nelm: %i\n", ch->inNelm);

            if(arguments->tool == caputq) status = ca_array_put(baseType, ch->inNelm, ch->base.id, input);
            else status = ca_array_put_callback(baseType, ch->inNelm, ch->base.id, input, caWriteCallback, ch);
            //else status = ca_array_put_callback(DBF_CHAR, 1, ch->base.id, &putin, caWriteCallback, ch);
            free(input);
        }
        else {
            status = ECA_BADTYPE;
            free(input);
            return false;
        }
    }
    else if(arguments->tool == cagets && ch->proc.created) {
        dbr_char_t input=1;
        status = ca_array_put_callback(DBF_CHAR, 1, ch->proc.id, &input, caWriteCallback, ch);
    }
    else if(arguments->tool == cado) {
        dbr_char_t input = 1;
        status = ca_array_put(DBF_CHAR, 1, ch->base.id, &input); // old PSI tools behaved this way. Is it better to fill entire array?
    }
    if (status != ECA_NORMAL) {
        errPrint("Problem creating request for process variable %s: %s.\n", ch->base.name, ca_message(status));
        return false;
    }


    return true;
}

bool caGenerateReadRequests(struct channel *ch, arguments_T * arguments){
    debugPrint("caGenerateRequests() - %s\n", ch->base.name);
    int status;    


    //request ctrl type
    int32_t baseType = ca_field_type(ch->base.id);
    int32_t reqType = dbf_type_to_DBR_CTRL(baseType);

    /* check number of elements. arguments->outNelm is 0 by default.
     * calling ca_create_subscription or ca_request with COUNT 0
     * returns defaultnumber of elements NORD in R3.14.21 or NELM in R3.13.10 */
    if((unsigned long) arguments->outNelm < ch->count) ch->outNelm = (unsigned long) arguments->outNelm;
    else{
        ch->outNelm = ch->count;
        warnPeriodicPrint("Invalid number of requested elements to read (%"PRId64") from %s - reading maximum number of elements (%lu).\n", arguments->outNelm, ch->base.name, ch->count);
    }
   
    /* long strings */
    if (baseType==DBF_STRING && ch->lstr.connectionState==CA_OP_CONN_UP && ca_element_count(ch->lstr.id) > MAX_STRING_SIZE)
    {
        ch->nRequests=ch->nRequests+1;
        status = ca_array_get_callback(DBR_CTRL_CHAR, 0, ch->lstr.id, caReadCallback, ch);
        if (status != ECA_NORMAL) {
            errPeriodicPrint("Problem creating ca_get request for process variable %s: %s\n",ch->lstr.name, ca_message(status));
            return;
        }
    }else{
        /* default */
        ch->nRequests=ch->nRequests+1;
        status = ca_array_get_callback(reqType, ch->outNelm, ch->base.id, caReadCallback, ch);
        if (status != ECA_NORMAL) {
            errPeriodicPrint("Problem creating ca_get request for process variable %s: %s\n",ch->base.name, ca_message(status));
            return;
        }
    }
    /* get timestamp if needed */
    if (arguments->time || arguments->date || arguments->timestamp){
        if (arguments->str && ca_field_type(ch->base.id) == DBF_ENUM){
            /* if enum && s, use time_string */
            reqType = DBR_TIME_STRING;
        }
        else{ /* else use time_native */
            reqType = dbf_type_to_DBR_TIME(ca_field_type(ch->base.id));
        }
        ch->nRequests=ch->nRequests+1;
        status = ca_array_get_callback(reqType, ch->outNelm, ch->base.id, caReadCallback, ch);
        if (status != ECA_NORMAL) {
            errPeriodicPrint("Problem creating ca_get request for process variable %s: %s\n",ch->base.name, ca_message(status));
            return;
        }
    }
    if (status != ECA_NORMAL) {
        errPrint("Problem creating request for process variable %s: %s.\n", ch->base.name, ca_message(status));
        return false;
    }

    return true;
}

bool caGenerateSubscription(struct channel *ch, arguments_T * arguments){
    debugPrint("caGenerateSubscription() - %s\n", ch->base.name);
    int status;


    //request ctrl type
    int32_t baseType = ca_field_type(ch->base.id);
    int32_t reqType = dbf_type_to_DBR_CTRL(baseType);

    status = ca_create_subscription(reqType, ch->outNelm, ch->base.id, DBE_VALUE | DBE_ALARM | DBE_LOG, caReadCallback, ch, 0);
    if (status != ECA_NORMAL) {
        errPrint("Problem creating subscription for process variable %s: %s.\n",ch->base.name, ca_message(status));
        return false;
    }
    return true;
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
        printValue(args, -1, &arguments);
        fputc('\n',stdout);

        switch (channels[i].type){
        case DBR_CTRL_STRING:
            fputc('\n',stdout);
            printf("\tAlarm status: %s, severity: %s\n",
                    epicsAlarmConditionStrings[((struct dbr_sts_string *)data)->status],
                    epicsAlarmSeverityStrings[((struct dbr_sts_string *)data)->severity]);      //status and severity
            break;
        case DBR_CTRL_INT://and short
            printf("\tUnits: %s\n", ((struct dbr_ctrl_int *)data)->units);              //units
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
                    ((struct dbr_ctrl_int *)data)->lower_ctrl_limit);                   //control limits
            printf("\tDisplay\tupper limit: %"PRId16"\n\t\tower limit: %"PRId16"\n",
                   ((struct dbr_ctrl_int *)data)->upper_disp_limit, ((struct dbr_ctrl_int *)data)->lower_disp_limit);                   //display limits
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


bool caRequest(struct channel *channels, u_int32_t nChannels) {
//sends get or put requests. ca_get or ca_put are called multiple times, depending on the tool. The reading,
//parsing and printing of returned data is performed in callbacks.
    int status = -1;
    u_int32_t i;

    // generate write requests 
    if (arguments.tool == caput || 
        arguments.tool == caputq || 
        arguments.tool == cado ||
        arguments.tool == cagets)
    {
        for(i=0; i < nChannels; i++) {
            if (channels[i].base.connectionState != CA_OP_CONN_UP) {//skip disconnected channels
                continue;
            }
            if(arguments.tool == cado || arguments.tool == caputq) return true;
            caGenerateWriteRequests(&channels[i], &arguments);
        }
        waitForCallbacks(channels, nChannels);
    }
    // generate read requests caTools-TEST:SECONDS
    if (arguments.tool == caget || 
        arguments.tool == cagets || 
        arguments.tool == caput  ||
        arguments.tool == camon)
    {
        for(i=0; i < nChannels; i++) {
            if (channels[i].base.connectionState != CA_OP_CONN_UP) {//skip disconnected channels
                continue;
            }

            caGenerateReadRequests(&channels[i], &arguments);
        }

        waitForCallbacks(channels, nChannels);
    }

    if (arguments.tool == cainfo)
    {
        cainfoRequest(channels, nChannels);
    }

    return true;    
}





void channelInitCallback(struct connection_handler_args args){
    //Fix desc
    //callback for ca_create_channel. Is executed whenever a channel connects or
    //disconnects. It is used for field channels in caInfo tool.
    //When a field for a channel that exist cannot be created, we clear the channel for the field.
    debugPrint("channelFieldStatusCallback()\n");

    struct field   *field = ( struct field * ) ca_puser ( args.chid );
    struct channel *ch = field->ch;
    debugPrint("channelFieldStatusCallback() callbacks to process: %i\n", ch->nRequests);
    ch->nRequests=ch->nRequests-1; //Used to notify waitForCallbacks about finished request

    if (field->connectionState == CA_OP_OTHER) {
        field->done = true;   // set field to done only on first connection, not when connection goes up / down
    }

    field->connectionState = args.op;

    debugPrint("channelFieldStatusCallback() - %s\n", field->name);

    if(args.op == CA_OP_CONN_UP){ /* connected */
        debugPrint("channelFieldStatusCallback() - connected\n");
        if (&ch->base == field){
            /* base channel */
            ch->count = ca_element_count(field->id);
            debugPrint("channelFieldStatusCallback() - COUNT: %i\n", ch->count);
        }

    }
    else{ /* disconected */
        debugPrint("channelFieldStatusCallback() - not connected\n");
        if (&ch->base == field){
            /* base channel */
            printf(" %s DISCONNECTED!\n", ch->base.name);
        }
    }
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
    epicsTimeToStrftime(g_errorTimestamp, LEN_TIMESTAMP, "%Y-%m-%d %H:%M:%S.%06f", &timestamp);

    if (args.stat == ECA_DISCONN || args.stat == ECA_DISCONNCHID) {
        printf("%s ", g_errorTimestamp);
    }
    else {
        sprintf ( buf,
                "%s in %s - with request chanel=%s operation=%ld data type=%s count=%ld",
                g_errorTimestamp, args.ctx, pName, args.op, dbr_type_to_text ( args.type ), args.count );
        ca_signal ( args.stat, buf );
    }
}

// init field and crreate ca_channel#
//Document how this works
bool initField(struct channel *ch, struct field * field)
{
    debugPrint("initField() - %s\n", field->name);
    field->connectionState = CA_OP_OTHER;
    field->done = false;
    field->ch = ch;
    ch->nRequests=ch->nRequests+1;
    int status = ca_create_channel(field->name, channelInitCallback, field, CA_PRIORITY, &field->id);
    field->created = checkStatus(status);    // error if the channels are not created
    return field->created;
}

bool initSiblings(struct channel *ch, arguments_T *arguments){
    debugPrint("caInitSiblings()\n");
    bool hasSiblings = false;

    //if tool = cagets, each channel has a sibling connecting to the proc field
    if (arguments->tool == cagets) {
        ch->proc.name = callocMustSucceed (LEN_FQN_NAME, sizeof(char), "main");//2 spaces for .(field name) + null termination
        strcpy(ch->proc.name, ch->base.name); //Consider using strn_xxxx everywhere
        getBaseChannelName(ch->proc.name); //append .PROC
        strncat(ch->proc.name, ".PROC",LEN_FQN_NAME);
        hasSiblings = initField(ch, &ch->proc);  
    }
    // open sibling channel for long strings
    if(ca_field_type(ch->base.id)==DBF_STRING && 
        !isValField(ch->base.name) &&
            (arguments->tool == caget ||
            arguments->tool == cagets ||
            arguments->tool == cainfo ||
            arguments->tool == camon )){
        debugPrint("caInitSiblings() - string chanel\n");
        debugPrint("caInitSiblings() - dbftype: %s\n", dbf_type_to_text(ca_field_type(ch->base.id)));
        ch->lstr.name = callocMustSucceed (strlen(ch->base.name) + 2, sizeof(char), "main"); //2 spaces for $ + null termination
        strcpy(ch->lstr.name, ch->base.name);
        strcat(ch->lstr.name, "$");
        hasSiblings = initField(ch, &ch->lstr);
    }
    if (arguments->tool == cainfo) {
        size_t nFields = noFields;  // so we don't calculate in each loop
        size_t j;
        for (j=0; j < nFields; j++) {
            debugPrint("caInit() - If caInfo for j=%i\n", j);
            ch->fields[j].created = true;
            ch->fields[j].name = callocMustSucceed(LEN_FQN_NAME, sizeof(char), fields[j]);
            strcpy(ch->fields[j].name, ch->base.name);
            getBaseChannelName(ch->fields[j].name);
            strcat(ch->fields[j].name, fields[j]);
            hasSiblings = initField(ch, &ch->fields[j]);            
        }
    }
    return hasSiblings;
}


//WARNING: This never returns false!!!
bool caInit(struct channel *channels, u_int32_t nChannels){
    //creates contexts and channels.
    int status;
    u_int32_t i;
    debugPrint("caInit()\n");
    status = ca_context_create(ca_disable_preemptive_callback);

    if (!checkStatus(status)) return false;

    ca_add_exception_event ( caCustomExceptionHandler , 0 );

    // open base channels
    for(i=0; i < nChannels; i++) {
        channels[i].nRequests = 0;
        initField(&channels[i], &channels[i].base);
    }

    waitForCallbacks(channels, nChannels);
    debugPrint("caInit() - First Connections completed\n");

    // open silbling channels if needed (in case of cainfo, cagets or long string)
    bool siblings = false;
    for (i=0; i < nChannels; ++i) {
        if (channels[i].base.connectionState == CA_OP_CONN_UP)
            siblings = initSiblings(&channels[i], &arguments);
        else
            printf("Process variable %s not connected.\n", channels[i].base.name);
    }
    if (siblings){
        //ca_flush_io();
        waitForCallbacks(channels, nChannels);
        debugPrint("caInit() - Siblings connected\n");
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



void allocateStringBuffers(u_int32_t nChannels){
    //allocate memory for output strings
    g_errorTimestamp = callocMustSucceed(LEN_TIMESTAMP, sizeof(char),"errorTimestamp");
    g_outDate = callocMustSucceed(nChannels, sizeof(char *),"main");
    g_outTime = callocMustSucceed(nChannels, sizeof(char *),"main");
    g_outSev = callocMustSucceed(nChannels, sizeof(char *),"main");
    g_outStat = callocMustSucceed(nChannels, sizeof(char *),"main");
    g_outUnits = callocMustSucceed(nChannels, sizeof(char *),"main");
    g_outTimestamp = callocMustSucceed(nChannels, sizeof(char *),"main");
    g_outLocalDate = callocMustSucceed(nChannels, sizeof(char *),"main");
    g_outLocalTime = callocMustSucceed(nChannels, sizeof(char *),"main");

    for(int i = 0; i < nChannels; i++){
        g_outDate[i] = callocMustSucceed(LEN_TIMESTAMP, sizeof(char),"main");
        g_outTime[i] = callocMustSucceed(LEN_TIMESTAMP, sizeof(char),"main");
        g_outSev[i] = callocMustSucceed(LEN_SEVSTAT, sizeof(char),"main");
        g_outStat[i] = callocMustSucceed(LEN_SEVSTAT, sizeof(char),"main");
        g_outUnits[i] = callocMustSucceed(LEN_UNITS, sizeof(char),"main");
        g_outTimestamp[i] = callocMustSucceed(LEN_TIMESTAMP, sizeof(char),"main");
        g_outLocalDate[i] = callocMustSucceed(LEN_TIMESTAMP, sizeof(char),"main");
        g_outLocalTime[i] = callocMustSucceed(LEN_TIMESTAMP, sizeof(char),"main");
    }
    //memory for timestamp
    g_timestampRead = mallocMustSucceed(nChannels * sizeof(epicsTimeStamp),"main");
    if (arguments.tool == camon || arguments.tool == cawait){
        g_lastUpdate = mallocMustSucceed(nChannels * sizeof(epicsTimeStamp),"main");
        g_firstUpdate = callocMustSucceed(nChannels, sizeof(bool),"main");
        g_numMonitorUpdates = 0;
    }
}

void freeChannels(struct channel * channels, u_int32_t nChannels){
    /* free channels */
    for (int i=0 ;i < nChannels; ++i) {
        free(channels[i].writeStr); //This will not properly deallocate the contents of writeStr.
        /* if (arguments.tool == cagets) */
        free(channels[i].proc.name);

        if (arguments.tool == cainfo) {
            size_t nFields = noFields;  // so we don't calculate in each loop
            for (int j=0; j < nFields; j++) {
                free(channels[i].fields[j].name);
            }
        }
    }
    free(channels);
}

void freeGlobals(u_int32_t nChannels){
    //free output strings
    for(int i = 0; i < nChannels; i++) {
        free(g_outDate[i]);
        free(g_outTime[i]);
        free(g_outSev[i]);
        free(g_outStat[i]);
        free(g_outUnits[i]);
        free(g_outTimestamp[i]);
        free(g_outLocalDate[i]);
        free(g_outLocalTime[i]);
    }
    free(g_outDate);
    free(g_outTime);
    free(g_outSev);
    free(g_outStat);
    free(g_outUnits);
    free(g_outTimestamp);
    free(g_outLocalDate);
    free(g_outLocalTime);

    //free monitor's timestamp
    free(g_timestampRead);
    free(g_lastUpdate);
    free(g_firstUpdate);

    free(g_errorTimestamp);
}

int main ( int argc, char ** argv )
{//main function: reads arguments, allocates memory, calls ca* functions, frees memory and exits.

    u_int32_t nChannels=0;              // Number of channels
    struct channel *channels;

    g_runMonitor = true;

    // remember time
    epicsTimeGetCurrent(&g_programStartTime);
    bool success = true;

    if(!(success=parseArguments(argc, argv, &nChannels, &arguments)))
        goto the_very_end;

    //allocate memory for channel structures
    channels = (struct channel *) callocMustSucceed (nChannels, sizeof(struct channel), "Could not allocate channels"); //Consider adding more descriptive error message.

    success = parseChannels(argc, argv, nChannels, &arguments, channels); //WARNING: This may not correctly return success failure, refracture and clean it up..

    if(!success){
        exit(EXIT_FAILURE);
    }

    allocateStringBuffers(nChannels);

    //start channel access
    //Clear up all if(success) and replace with proper exit and cleanup
    if(success) success = caInit(channels, nChannels);

    // set timeout time from now
    if (arguments.timeout!=-1){
        //set first
        epicsTimeGetCurrent(&g_timeoutTime);
        epicsTimeAddSeconds(&g_timeoutTime, arguments.timeout);
    }

    success = caRequest(channels, nChannels);

    if( arguments.tool == camon || arguments.tool == cawait ) {

        for(size_t i=0; i < nChannels; i++) {
            if (channels[i].base.connectionState != CA_OP_CONN_UP) {//skip disconnected channels //Check if you really need to
                continue;
            }

            caGenerateSubscription(&channels[i], &arguments);
        }

        monitorLoop();
    }


    //Add GOTO definitions for cleanup, reorder cleanup in inverse order compared to allocations...
    //Check LKML and Linux sources for details on clean implementation....

    success &= caDisconnect(channels, nChannels);


    ca_context_destroy();

    freeChannels(channels, nChannels);

    freeGlobals(nChannels);

    the_very_end:
        if (success) return EXIT_SUCCESS;
        else return EXIT_FAILURE;
}
