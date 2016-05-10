/* caToolsMain.c */



#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>
#include <getopt.h>
#include <stdbool.h>

#include "cantProceed.h"
#include "cadef.h"
#include "alarmString.h"
#include "alarm.h"
#include "caToolsTypes.h"
#include "caToolsGlobals.c"
#include "caToolsOutput.h"
#include "caToolsInput.h"
#include "caToolsUtils.h"


/**
 * @brief caReadCallback callback called by ca_get or subscription
 * @param args
 */
static void caReadCallback (evargs args){
    debugPrint("caWriteCallback()\n");
    /*check if status is ok */
    if (args.status != ECA_NORMAL){
        errPrint("Error in read callback. %s.\n", ca_message(args.status));
        return;
    }

    struct channel *ch = (struct channel *)args.usr;
    debugPrint("ReadCallback() callbacks to process: %i\n", ch->nRequests);
    debugPrint("ReadCallback() type: %s\n", dbr_type_to_text(args.type));
    ch->nRequests --; /* number of returned callbacks before printing */

    /*if we are in monitor or cawait and just waiting for the program to exit, don't proceed. */
    if (g_runMonitor == false){
        ch->nRequests=0; /* make sure that the waitForCallbacks() also finishes */
        return;
    }

    /* get metadata from args and fill the global strings and channel properties:
     * status, severity, timestamp, precision, units*/
    getMetadataFromEvArgs(ch, args);

    bool shouldPrint = true;

    /* wait for all initial get requests to arrive before printing or further actions */
    if(ch->nRequests > 0){
        shouldPrint = false;
    }
    else{
        ch->state = reading;

        /* update relative timestamps only after all initial get requests have arived */
        if ((arguments.tool == cawait || arguments.tool == camon) && arguments.timestamp)
            getTimeStamp(ch->i, &arguments);	/*calculate relative timestamps. */

        if (arguments.tool == cawait) {
            /* check for timeout */
            if (arguments.timeout!=-1) {
                epicsTimeStamp timeoutNow;
                epicsTimeGetCurrent(&timeoutNow);

                if (epicsTimeGreaterThanEqual(&timeoutNow, &g_timeoutTime)) {
                    /* we are done waiting */
                    printf("Condition not met in %f seconds - exiting.\n",arguments.timeout);
                    g_runMonitor = false;
                    shouldPrint = false;
                }
            }

            /* check wait condition */
            if (cawaitEvaluateCondition(ch, args)) {
                g_runMonitor = false;
            }else{
                shouldPrint = false;
            }
        }

        ch->nRequests = 0; /* keep at zero to prevent overroll */
    }

    /* finish */
    if(shouldPrint){
        /* handle -n option, count plus one and stop after n */
        if (arguments.numUpdates != -1) {
            g_numMonitorUpdates++;	/*increase counter of updates */

            if (g_numMonitorUpdates >= arguments.numUpdates) {
                g_runMonitor = false;
            }
        }

        /* print out */
        printOutput(args, &arguments);
    }
    ch->base.done = true;
}

/**
 * @brief caWriteCallback - callback called by ca_put does nothing special except signals that writing has finished.
 * @param args
 */
static void caWriteCallback (evargs args) {
    debugPrint("caWriteCallback()\n");
    /*check if status is ok */
    if (args.status != ECA_NORMAL){
        errPrint("Error in write callback. %s.\n", ca_message(args.status));
        return;
    }

    struct channel *ch = (struct channel *)args.usr;
    debugPrint("caWriteCallback() callbacks to process: %i\n", ch->nRequests);
    ch->nRequests --;
    ch->base.done = true;
    if(ch->nRequests == 0) ch->state = put_done;
}

/**
 * @brief waitForCallbacks Wait for request completition of all requests for a channel (or it's fields), or for caTimeout to occur
 *        It waits until all channels[i].nRequests <= 0 or timeout.
 *        Callback functions should do channels[i].nRequests -- for each callback and functions
 *        that issue request should set some positive number to channels[i].nRequests.
 *
 * @param channels poiter to the global channel array
 * @param nChannels number of existing chawaitForCallbacksnnels
 */
void waitForCallbacks(struct channel *channels, u_int32_t nChannels) {
    u_int32_t i;
    size_t j;
    bool elapsed = false, allDone = false;
    epicsTimeStamp timeoutNow, timeout;
    size_t nFields = noFields;

    epicsTimeGetCurrent(&timeout);
    epicsTimeAddSeconds(&timeout, arguments.caTimeout);

    while(!allDone) {
        ca_pend_event(CA_PEND_EVENT_TIMEOUT);
        /* check for timeout */
        epicsTimeGetCurrent(&timeoutNow);

        if (epicsTimeGreaterThanEqual(&timeoutNow, &timeout)) {
            warnPrint("Timeout while waiting for PV response (more than %f seconds elapsed).\n", arguments.caTimeout);
            break;
        }

        /* check for callback completition */
        allDone = true;
        for (i=0; i < nChannels; ++i) {
            if (channels[i].nRequests > 0){
                allDone = false;
                break;
            }
        }
    }
    debugPrint("waitForCallbacks - done.\n");

}

/**
 * @brief caGenerateWriteRequests - generates write requests for one channel depending on the tool and arguments
 * @param chnpointer to channel struct
 * @param arguments pointer to arguments struct
 * @return true on success
 */
bool caGenerateWriteRequests(struct channel *ch, arguments_T * arguments){
    debugPrint("caGenerateWriteRequests() - %s\n", ch->base.name);
    int status;

    ch->state=put_waiting;
    /* request ctrl type */
    int32_t baseType = ca_field_type(ch->base.id);

    ch->nRequests=0;
   
   if (arguments->tool == caput || arguments->tool == caputq) {        

        void * input;

        /* parse as array if needed */
        if(!parseAsArray(ch, arguments)) return false;

        /* parse input strings to dbr types */
        if (castStrToDBR(&input, ch->writeStr, &ch->inNelm, baseType, arguments)) {
            if (ch->inNelm > ch->count){
                warnPrint("Number of input elements is: %i, but channel can accept only %i.\n", ch->inNelm, ch->count);
                ch->inNelm = ch->count;
            }
            debugPrint("nelm: %i\n", ch->inNelm);
            ch->nRequests++;
            /* dont wait for callback in case of caputq */
            if(arguments->tool == caputq) status = ca_array_put(baseType, ch->inNelm, ch->base.id, input);
            else status = ca_array_put_callback(baseType, ch->inNelm, ch->base.id, input, caWriteCallback, ch);
            free(input);
        }
        else {
            status = ECA_BADTYPE;
            free(input);
            return false;
        }
    }
    else if((arguments->tool == cagets) && ch->proc.created) {
        /* in case of cagets put 1 to the proc field */
        ch->nRequests++;
        dbr_char_t input=1;
        status = ca_array_put_callback(DBF_CHAR, 1, ch->proc.id, &input, caWriteCallback, ch);
    }
    else if(arguments->tool == cado) {
       /* in case of cado put 1 to the proc field, don't wait for callback */
        ch->nRequests++;
        dbr_char_t input = 1;
        status = ca_array_put(DBF_CHAR, 1, ch->proc.id, &input);
    }

    if (status != ECA_NORMAL) {
        errPrint("Problem creating put request for process variable %s: %s.\n", ch->base.name, ca_message(status));
        ch->nRequests --;
        return false;
    }
    return true;
}

/**
 * @brief caGenerateReadRequests - generate read requests for one channel depending on tool and arguments
 *          Sets dbrtype and sibling or base channel to read from, depending or arguments and channel properties.
 *          If no other type is specified, dbr_ctrl type is used for the first request, if we need
 *          timestamp, second request is issued using dbr_time type. In case of camon or cawait,
 *          subscription is created to the appropriate channel with appropriate type.
 *          Returned data is handled in caReadCallback
 * @param ch pointer to channel struct
 * @param arguments pointer to arguments struct
 * @return true on success
 */
bool caGenerateReadRequests(struct channel *ch, arguments_T * arguments){
    debugPrint("caGenerateReadRequests() - %s\n", ch->base.name);
    int status;

    /*request ctrl type */
    int32_t baseType = ca_field_type(ch->base.id);
    int32_t reqType = dbf_type_to_DBR_CTRL(baseType); /* use ctrl type by default */
    chid    reqChid = ch->base.id;
    ch->state = read_waiting;

    /* check number of elements. arguments->outNelm is 0 by default.
     * calling ca_create_subscription or ca_request with COUNT 0
     * returns defaultnumber of elements NORD in R3.14.21 or NELM in R3.13.10 */
    if((unsigned long) arguments->outNelm < ch->count) ch->outNelm = (unsigned long) arguments->outNelm;
    else{
        ch->outNelm = ch->count;
        warnPeriodicPrint("Invalid number of requested elements to read (%"PRId64") from %s - reading maximum number of elements (%lu).\n", arguments->outNelm, ch->base.name, ch->count);
    }

    /* if string, connect to long string sibling channel if connected and number of characters is longer than MAX_STRING_SIZE */
    if (baseType==DBF_STRING &&
        ch->lstr.connectionState==CA_OP_CONN_UP &&
        ca_element_count(ch->lstr.id) > MAX_STRING_SIZE &&
        arguments->dbrRequestType==-1)
    {
        reqChid = ch->lstr.id;
        reqType = DBR_CTRL_CHAR;
    }
    /* use dbrtype that has been specified as command line argument */
    if (arguments->dbrRequestType!=-1){
        reqType = arguments->dbrRequestType;
    }

    /* if no other type specified issue first request with ctrl type or with the type that was specified. */
    debugPrint("caGenerateReadRequests() - outNelm: %i\n", ch->outNelm);
    debugPrint("caGenerateReadRequests() - reqType: %s\n", dbr_type_to_text(reqType));
    ch->nRequests ++;
    status = ca_array_get_callback(reqType, ch->outNelm, reqChid, caReadCallback, ch);
    if (status != ECA_NORMAL) {
        errPeriodicPrint("Problem creating get request for process variable %s: %s\n",ch->base.name, ca_message(status));
        ch->nRequests --;
        return false;
    }

    /* if needed issue second request using time type */
    if ((arguments->time || arguments->date || arguments->timestamp) &&
            arguments->dbrRequestType == -1 )
    {
        if (arguments->str && ca_field_type(reqChid) == DBF_ENUM){
            /* if enum && s, use time_string */
            reqType = DBR_TIME_STRING;
        }
        else{ /* else use time_native */
            reqType = dbf_type_to_DBR_TIME(dbr_type_to_DBF(reqType));
        }
        ch->nRequests=ch->nRequests+1;
        status = ca_array_get_callback(reqType, ch->outNelm, reqChid, caReadCallback, ch);
        if (status != ECA_NORMAL) {
            errPeriodicPrint("Problem creating get_request for process variable %s: %s\n",ch->base.name, ca_message(status));
            ch->nRequests --;
            return false;
        }
    }

    /* create subscribtion in case of camon or cawait */
    if( arguments->tool == camon || arguments->tool == cawait ){
        ch->nRequests=ch->nRequests+1;
        status = ca_create_subscription(reqType, ch->outNelm, reqChid, DBE_VALUE | DBE_ALARM | DBE_LOG, caReadCallback, ch, 0);
        if (status != ECA_NORMAL) {
            errPrint("Problem creating subscription for process variable %s: %s.\n",ch->base.name, ca_message(status));
            ch->nRequests --;
            return false;
        }
    }

    return true;
}

/**
 * @brief cainfoRequest - this spaghetti function does all the work for caInfo tool. Reads channel data using ca_get and then prints.
 * @param channels - array of channel structs
 * @param nChannels - number of channels in array
 * @return true on success
 */
bool cainfoRequest(struct channel *channels, u_int32_t nChannels){
    int status;
    u_int32_t i, j;

    bool readAccess, writeAccess;
    u_int32_t nFields = noFields;
    const char *delimeter = "-------------------------------";

    for(i=0; i < nChannels; i++){

        if (channels[i].base.connectionState != CA_OP_CONN_UP) continue; /* skip unconnected channels */

        channels[i].count = ca_element_count ( channels[i].base.id );

        channels[i].type = dbf_type_to_DBR_CTRL(ca_field_type(channels[i].base.id));

        /*allocate data for all caget returns */
        void *data;
        struct dbr_sts_string *fieldData[nFields];

        data = callocMustSucceed(1, dbr_size_n(channels[i].type, channels[i].count), "caInfoRequest");


        /*general ctrl data */
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


        /*start printing */
        fputc('\n',stdout);
        fputc('\n',stdout);
        printf("%s\n%s\n", delimeter, channels[i].base.name);                                          /*name */
        if(fieldData[field_desc] != NULL) printf("\tDescription: %s\n", fieldData[field_desc]->value);          /*description */
        printf("\tNative DBF type: %s\n", dbf_type_to_text(ca_field_type(channels[i].base.id)));                     /*field type */
        printf("\tNumber of elements: %ld\n", channels[i].count);                                               /*number of elements */


        evargs args;
        args.chid = channels[i].base.id;
        args.count = ca_element_count ( channels[i].base.id );
        args.dbr = data;
        args.status = ECA_NORMAL;
        args.type = channels[i].type; /*ca_field_type(channels[i].base.id); */
        args.usr = &channels[i];
        printf("\tValue: ");
        printValue(args, &arguments);
        fputc('\n',stdout);

        switch (channels[i].type){
        case DBR_CTRL_STRING:
            fputc('\n',stdout);
            printf("\tAlarm status: %s, severity: %s\n",
                    epicsAlarmConditionStrings[((struct dbr_sts_string *)data)->status],
                    epicsAlarmSeverityStrings[((struct dbr_sts_string *)data)->severity]);      /*status and severity */
            break;
        case DBR_CTRL_INT:/*and short */
            printf("\tUnits: %s\n", ((struct dbr_ctrl_int *)data)->units);              /*units */
            fputc('\n',stdout);
            printf("\tAlarm status: %s, severity: %s\n",
                    epicsAlarmConditionStrings[((struct dbr_ctrl_int *)data)->status],
                    epicsAlarmSeverityStrings[((struct dbr_ctrl_int *)data)->severity]);      /*status and severity */
            fputc('\n',stdout);
            printf("\tWarning\tupper limit: %" PRId16"\n\t\tlower limit: %"PRId16"\n",
                    ((struct dbr_ctrl_int *)data)->upper_warning_limit, ((struct dbr_ctrl_int *)data)->lower_warning_limit); /*warning limits */
            printf("\tAlarm\tupper limit: %" PRId16"\n\t\tlower limit: %" PRId16"\n",
                    ((struct dbr_ctrl_int *)data)->upper_alarm_limit, ((struct dbr_ctrl_int *)data)->lower_alarm_limit); /*alarm limits */
            printf("\tControl\tupper limit: %"PRId16"\n\t\tlower limit: %"PRId16"\n", ((struct dbr_ctrl_int *)data)->upper_ctrl_limit,\
                    ((struct dbr_ctrl_int *)data)->lower_ctrl_limit);                   /*control limits */
            printf("\tDisplay\tupper limit: %"PRId16"\n\t\tower limit: %"PRId16"\n",
                   ((struct dbr_ctrl_int *)data)->upper_disp_limit, ((struct dbr_ctrl_int *)data)->lower_disp_limit);                   /*display limits */
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

            /* */    break;
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

        printf("\tIOC name: %s\n", ca_host_name(channels[i].base.id));                           /*host name */
        printf("\tRead access: "); if(readAccess) printf("yes\n"); else printf("no\n");     /*read and write access */
        printf("\tWrite access: "); if(writeAccess) printf("yes\n"); else printf("no\n");
        printf("%s\n", delimeter);

        free(data);
    }
    return true;
}

/**
 * @brief caRequest - calls caGenerateWriteRequests and caGenerateReadRequests for each channel depending on tool
 * @param channels - array of channel structs
 * @param nChannels - number of channel structs
 * @return true on success
 */
bool caRequest(struct channel *channels, u_int32_t nChannels) {
    bool success = true;
    u_int32_t i;

    /* generate write requests  */
    if (arguments.tool == caput || 
        arguments.tool == caputq || 
        arguments.tool == cado ||
        arguments.tool == cagets)
    {
        for(i=0; i < nChannels; i++) {
            if (channels[i].base.connectionState != CA_OP_CONN_UP) {/*skip disconnected channels */
                continue;
            }            
            success = caGenerateWriteRequests(&channels[i], &arguments);
        }
        /* if cado or caputq exit right away */
        if(arguments.tool == cado || arguments.tool == caputq) return success;
        /* wait for callbacks to finish before issuing read requests */
        waitForCallbacks(channels, nChannels);

    }
    /* generate read requests */
    if (arguments.tool == caget || 
        arguments.tool == cagets || 
        arguments.tool == caput  ||
        arguments.tool == camon ||
        arguments.tool == cawait)
    {
        for(i=0; i < nChannels; i++) {
            if (channels[i].base.connectionState != CA_OP_CONN_UP) {/*skip disconnected channels */
                continue;
            }

            success = caGenerateReadRequests(&channels[i], &arguments);
        }
    }

    /* wait for callbacks to finish before ending the program */
    waitForCallbacks(channels, nChannels);

    /* call the spaghetti function if cainfo */
    if (arguments.tool == cainfo)
    {
        success = cainfoRequest(channels, nChannels);
    }

    return success;
}

/**
 * @brief channelInitCallback - callback for initField. Is executed whenever a base or sibling channel connects or disconnects.
 *                              Sets proper flags and extracts initial data such as count from the response.
 * @param args
 */
void channelInitCallback(struct connection_handler_args args){
    debugPrint("channelInitCallback()\n");

    struct field   *field = ( struct field * ) ca_puser ( args.chid );
    struct channel *ch = field->ch;
    debugPrint("channelInitCallback() callbacks to process: %i\n", ch->nRequests);
    ch->nRequests --; /* Used to notify waitForCallbacks about finished request */

    if (field->connectionState == CA_OP_OTHER) {
        /* first callback */
        field->done = true;   /* set field to done only on first connection, not when connection goes up / down */
        if (&ch->base == field){
            ch->state = base_created;
            debugPrint("channelInitCallback() ch->state: %i\n", ch->state);
        }else{
            if(ch->nRequests == 0) ch->state = sibl_created; /* all sibling channels connected - not a necesarry condition for the program to continue */
        }

    }

    /* update the connection state */
    field->connectionState = args.op;

    debugPrint("channelInitCallback() - %s\n", field->name);

    if(args.op == CA_OP_CONN_UP){ /* connected */
        debugPrint("channelInitCallback() - connected\n");
        debugPrint("channelInitCallback() - COUNT: %i\n", ca_element_count(field->id));
        if (&ch->base == field){
            /* base channel */
            ch->count = ca_element_count(field->id);
        }

    }
    else{ /* disconected */
        debugPrint("channelInitCallback() - not connected\n");
        if (&ch->base == field){
            /* base channel */
            printf(" %s DISCONNECTED!\n", ch->base.name);
        }
    }
    debugPrint("channelInitCallback() ch->state: %i\n", ch->state);

}

/**
 * @brief checkStatus and print an error message if th status is not indicating success
 * @param status
 * @return true for status indicating success
 */
bool checkStatus(int status){
/*checks status of channel create_ and clear_ */
    if (status != ECA_NORMAL){
        errPrint("CA error %s occurred\n", ca_message(status));
        SEVCHK(status, "CA error");
        return false;
    }
    return true;
}

/**
 * @brief caCustomExceptionHandler
 * @param args
 */
void caCustomExceptionHandler( struct exception_handler_args args) {
    char buf[512];
    const char *pName;
    epicsTimeStamp timestamp;	/*timestamp indicating program start */

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

/**
 * @brief initField - creates ca_channel and sets initial flags for channel and field structure
 * @param ch - poiter to the channel structure
 * @param field - pointer to the field structure
 * @return returns true if the channel was created succesfully
 */
bool initField(struct channel *ch, struct field * field)
{
    debugPrint("initField() - %s\n", field->name);
    field->connectionState = CA_OP_OTHER; /* ca connection state - is updated in initCallback */
    field->done = false;                  /* indicates wether the first callback arrived */
    field->ch = ch;                       /* pointer back to channel structure */
    ch->nRequests ++;        /* indicates number of issued requests. Callback functions substract one from this field and take corresponding actions on ch->nRequests==0 */
    int status = ca_create_channel(field->name, channelInitCallback, field, CA_PRIORITY, &field->id);
    field->created = checkStatus(status);    /* error if the channels are not created */
    return field->created;
}

/**
 * @brief initSiblings call initField to create sibling channells depending on arguments and channel properties
 * @param ch pointer to the channel struct
 * @param arguments pointer to the arguments struct
 * @return true if any siblings were created
 */
bool initSiblings(struct channel *ch, arguments_T *arguments){
    debugPrint("caInitSiblings()\n");
    bool hasSiblings = false;
    ch->state=sibl_waiting;

    /*if tool = cagets or cado, each channel has a sibling connecting to the proc field */
    if (arguments->tool == cagets || arguments->tool == cado) {
        ch->proc.name = callocMustSucceed (LEN_FQN_NAME, sizeof(char), "main");/*2 spaces for .(field name) + null termination */
        strcpy(ch->proc.name, ch->base.name); /*Consider using strn_xxxx everywhere */
        getBaseChannelName(ch->proc.name); /*append .PROC */
        strncat(ch->proc.name, ".PROC",LEN_FQN_NAME);
        hasSiblings = initField(ch, &ch->proc);  
    }

    /* open sibling channel for long strings */
    if(ca_field_type(ch->base.id)==DBF_STRING && 
        !isValField(ch->base.name) &&
            (arguments->tool == caget ||
            arguments->tool == cagets ||
            arguments->tool == cainfo ||
            arguments->tool == camon  ||
            arguments->tool == cawait )){
        debugPrint("caInitSiblings() - string chanel\n");
        debugPrint("caInitSiblings() - dbftype: %s\n", dbf_type_to_text(ca_field_type(ch->base.id)));
        ch->lstr.name = callocMustSucceed (strlen(ch->base.name) + 2, sizeof(char), "main"); /*2 spaces for $ + null termination */
        strcpy(ch->lstr.name, ch->base.name);
        strcat(ch->lstr.name, "$");
        hasSiblings = initField(ch, &ch->lstr);
    }

    /* open all sibling channels for cainfo */
    if (arguments->tool == cainfo) {
        size_t nFields = noFields;  /* so we don't calculate in each loop */
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

/**
 * @brief caInit open base channels by calling initField() for each channel, calls initSiblings() to open sibling channels for each channel
 * @param channels array of channel structs
 * @param nChannels number of channels
 * @return true on success
 */
bool caInit(struct channel *channels, u_int32_t nChannels){
    /*creates contexts and channels. */
    int status;
    u_int32_t i;
    debugPrint("caInit()\n");
    status = ca_context_create(ca_disable_preemptive_callback);

    if (!checkStatus(status)) return false;

    ca_add_exception_event ( caCustomExceptionHandler , 0 );

    /* open base channels */
    for(i=0; i < nChannels; i++) {
        channels[i].nRequests = 0;
        channels[i].state=base_waiting;
        initField(&channels[i], &channels[i].base);
    }

    /* wait for all base channels to connect or timeout */
    waitForCallbacks(channels, nChannels);
    debugPrint("caInit() - First Connections completed\n");

    /* open silbling channels if needed (in case of cainfo, cagets or long string) */
    bool siblings = false;
    for (i=0; i < nChannels; ++i) {
        if (channels[i].base.connectionState == CA_OP_CONN_UP)
            siblings = initSiblings(&channels[i], &arguments);
        else
            printf("Process variable %s not connected.\n", channels[i].base.name);
    }
    if (siblings){
        /* if there are siblings, wait for them to connect or timeout*/
        waitForCallbacks(channels, nChannels);
        debugPrint("caInit() - Siblings connected\n");
    }
    
    return true;
}

/**
 * @brief monitorLoop - keeps the main thread alive for camon or cawait
 *                      also connects to the channels that were not connected at the start of the program,
 *                      but they became alive.
 * @param channels - array of channel structs
 * @param nChannels - number of channels
 * @param arguments - pointer to the arguments struct
 */
void monitorLoop (struct channel * channels, size_t nChannels, arguments_T * arguments){
/*runs monitor loop. Stops after -n updates (camon, cawait) or */
/*after -timeout is exceeded (cawait). */
    size_t i = 0;

    while (g_runMonitor){
        ca_pend_event(CA_PEND_EVENT_TIMEOUT);
        /* connect channels that were not connected at the start of the program in caInit */
        for(i = 0; i < nChannels; i++){
            if(channels[i].state==base_created){
                caGenerateReadRequests(&channels[i], arguments);
            }
        }
    }
}

/**
 * @brief caDisconnect - disconnect all connected fields
 * @param channels - array of channel structs
 * @param nChannels - number of channels
 * @return true on success
 */
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
        if (channels[i].lstr.created) {
            status = ca_clear_channel(channels[i].lstr.id);
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


/**
 * @brief allocateStringBuffers - allocates global string buffers
 * @param nChannels - number of channels
 */
void allocateStringBuffers(u_int32_t nChannels){
    /*allocate memory for output strings */
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
    /*memory for timestamp */
    g_timestampRead = mallocMustSucceed(nChannels * sizeof(epicsTimeStamp),"main");
    if (arguments.tool == camon || arguments.tool == cawait){
        g_lastUpdate = mallocMustSucceed(nChannels * sizeof(epicsTimeStamp),"main");
        g_firstUpdate = callocMustSucceed(nChannels, sizeof(bool),"main");
        g_numMonitorUpdates = 0;
    }
}

/**
 * @brief freeChannels - free global channels array
 * @param channels - array of channel structs
 * @param nChannels - number of channels
 */
void freeChannels(struct channel * channels, u_int32_t nChannels){
    /* free channels */
    for (size_t i=0 ;i < nChannels; ++i) {
        free(channels[i].writeStr); /* elements of writeStr point to parts of **argv */

        if ( arguments.tool == cagets || arguments.tool == cado )
            free(channels[i].proc.name);

        if (arguments.tool == cainfo) {
            size_t nFields = noFields;  /* so we don't calculate in each loop */
            for (int j=0; j < nFields; j++) {
                free(channels[i].fields[j].name);
            }
        }
    }
    free(channels);
}

/**
 * @brief freeStringBuffers free global string buffers
 * @param nChannels
 */
void freeStringBuffers(u_int32_t nChannels){
    /*free output strings */
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

    /*free monitor's timestamp */
    free(g_timestampRead);
    free(g_lastUpdate);
    free(g_firstUpdate);

    free(g_errorTimestamp);
}

/**
 * @brief main reads arguments, allocates memory, calls ca* functions, frees memory and exits.
 * @param argc
 * @param argv
 * @return exit status
 */
int main ( int argc, char ** argv ){

    u_int32_t nChannels=0;              /* Number of channels */
    struct channel *channels;

    g_runMonitor = true;

    /* remember time */
    epicsTimeGetCurrent(&g_programStartTime);
    bool success = true;

    if(!(success=parseArguments(argc, argv, &nChannels, &arguments))){
        debugPrint("main() - no succes with parseArguments\n");
        goto the_very_end;
    }

    /* allocate memory for channel structures */
    channels = (struct channel *) callocMustSucceed (nChannels, sizeof(struct channel), "Could not allocate channels"); /*Consider adding more descriptive error message. */

    /* parse command line and fill up the channel structures*/
    if(!(success = parseChannels(argc, argv, nChannels, &arguments, channels))){
        debugPrint("main() - no succes with parseChannels\n");
        goto clear_channels;
    }

    allocateStringBuffers(nChannels);

    /*start channel access */
    if(!(success = caInit(channels, nChannels))){
        debugPrint("main() - no succes with caInit\n");
        goto clear_global_strings;
    }

    /* set timeout time from now */
    if (arguments.timeout!=-1){
        /* set first */
        epicsTimeGetCurrent(&g_timeoutTime);

        epicsTimeAddSeconds(&g_timeoutTime, arguments.timeout);
    }

    /* issue ca requests and subscribtions */
    if(!(success = caRequest(channels, nChannels))){
        debugPrint("main() - no succes with caRequest\n");
        goto ca_disconnect;
    }


    /* wait for monitor or cawait to finish */
    if( arguments.tool == camon || arguments.tool == cawait ) {
        debugPrint("main() - enter monitor loop\n");
        monitorLoop(channels, nChannels, &arguments);
    }

    /* cleanup */
    ca_disconnect:
        success = caDisconnect(channels, nChannels);
        ca_context_destroy();
    clear_global_strings:
        freeStringBuffers(nChannels);
    clear_channels:
        freeChannels(channels, nChannels);
    the_very_end:
        if (success) return EXIT_SUCCESS;
        else return EXIT_FAILURE;
}
