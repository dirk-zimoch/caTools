#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <math.h>

#include "caToolsTypes.h"
#include "caToolsUtils.h"
#include "caToolsOutput.h"

#define printBits(x) \
    for (int32_t i = sizeof(x) * 8 - 1; i >= 0; i--) { \
        fputc('0' + ((x >> i) & 1), stdout); \
    }


void printValue(evargs args, arguments_T *arguments){
/* Parses the data fetched by ca_get callback according to the data type and formatting arguments-> */
/* The result is printed to stdout. */


    int32_t baseType;
    void *value;
    double valueDbl = 0;

    value = dbr_value_ptr(args.dbr, args.type);
    baseType = dbr_type_to_DBF(args.type);   /*  convert appropriate TIME, GR, CTRL,... type to basic one */
    struct channel *ch = (struct channel *)args.usr;
    

    debugPrint("printValue() - baseType: %s\n", dbr_type_to_text(baseType));

    /* handle long strings */
    if(baseType == DBR_CHAR &&
           !(arguments->bin || arguments->hex || arguments->oct || arguments->num) &&   /* no special numeric formating specified */
                (ca_field_type(ch->base.id)==DBF_STRING ||   /* if base channel is DBF_STRING*/
                 arguments->str ||                           /* if requested string formatting */
                 args.count > 1 && isPrintable((char *) value, (size_t) args.count))/* if array returned and all characters are printable */
        )
    {  /* print as string */
        printf("\"%.*s\"", (int) args.count, (char *) value); 
        return;
    }

    /* loop over the whole array */

    long j; /* element counter */

    for (j=0; j<args.count; ++j){

        if (j){
            printf ("%c", arguments->fieldSeparator); /* insert element separator */
        }

        switch (baseType) {
        case DBR_STRING:
            debugPrint("printValue() - case DBR_STRING - print as normal string\n");
            printf("\"%.*s\"", MAX_STRING_SIZE, ((dbr_string_t*) value)[j]);    /*  TODO: is this always null-terminated? */
            break;
        case DBR_FLOAT:
        case DBR_DOUBLE:{
            debugPrint("printValue() - case DBR_FLOAT or DBR_DOUBLE\n");
            if (baseType == DBR_FLOAT) valueDbl = ((dbr_float_t*)value)[j];
            else valueDbl = ((dbr_double_t*)value)[j];

            /* round if desired or will be writen as hex oct, bin or string */
            if (arguments->round == roundType_round || arguments->hex || arguments->oct || arguments->bin || arguments->str) {
                valueDbl = round(valueDbl);
            }
            else if(arguments->round == roundType_ceil) {
                    valueDbl = ceil(valueDbl);
            }
            else if (arguments->round == roundType_floor) {
                valueDbl = floor(valueDbl);
            }

            if (!(arguments->hex || arguments->oct || arguments->bin)) { /*  if hex, oct or bin, do not brake but use DBR_LONG code to print out the value */
                char format = 'f'; /*  default write as float */
                int precision = ch->prec;
                if(ch->prec < 0){
                    /* handle negative precision from channel */
                    format = 'e';
                    precision = -ch->prec;
                }
                if (arguments->plain) {
                    /* print raw when -plain is used or no precision defined */
                    printf("%-f", valueDbl);
                }else{
                    if (arguments->dblFormatType != '\0'){  /* if formatting arguments were used */
                        /* use precision and format as specified by -efg or -prec arguments */
                        format = arguments->dblFormatType;
                        precision = arguments->prec;
                    }
                    /* format and  print */
                    char formatStr[30];
                    sprintf(formatStr, "%%-.%"PRId32"%c", precision, format);
                    printf(formatStr, valueDbl);
                }
                break;
            }
        }
        case DBR_LONG:{
            debugPrint("printValue() - case DBR_LONG\n");
            int32_t valueInt32;
            if (baseType == DBR_FLOAT || baseType == DBR_DOUBLE) valueInt32 = (int32_t)valueDbl;
            else valueInt32 = ((dbr_long_t*)value)[j];

            /* display dec, hex, bin, oct if desired */
            if (arguments->hex){
                printf("0x%" PRIx32, valueInt32);
            }
            else if (arguments->oct){
                printf("0o%" PRIo32, valueInt32);
            }
            else if (arguments->bin){
                printf("0b");
                printBits(valueInt32);
            }
            else if (arguments->str){
                printf("%c", (u_int8_t)valueInt32);
            }
            else{
                printf("%" PRId32, valueInt32);
            }
            break;
        }
        case DBR_INT:{
            debugPrint("printValue() - case DBR_INT\n");
            int16_t valueInt16 = ((dbr_int_t*)value)[j];

            /* display dec, hex, bin, oct, char if desired */
            if (arguments->hex){
                printf("0x%" PRIx16, (u_int16_t)valueInt16);
            }
            else if (arguments->oct){
                printf("0o%" PRIo16, (u_int16_t)valueInt16);
            }
            else if (arguments->bin){
                printf("0b");
                printBits((u_int16_t)valueInt16);
            }
            else if (arguments->str){
                printf("%c", (u_int8_t)valueInt16);
            }
            else{
                printf("%" PRId16, valueInt16);
            }
            break;
        }
        case DBR_ENUM: {
            debugPrint("printValue() - case DBR_ENUM\n");
            dbr_enum_t v = ((dbr_enum_t *)value)[j];
            if (v >= MAX_ENUM_STATES){
                printf("Illegal enum index: %d", v);
                break;
            }

            if (!arguments->num && !arguments->bin && !arguments->hex && !arguments->oct) { /*  if not requested as a number check if we can get string */


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
            /*  else write value as number. */
            if (arguments->hex){
                printf("0x%" PRIx16, v);
            }
            else if (arguments->oct){
                printf("0o%" PRIo16, v);
            }
            else if (arguments->bin){
                printf("0b");
                printBits(v);
            }
            else{
                printf("%" PRId16, v);
            }
            break;
        }
        case DBR_CHAR:{
           debugPrint("printValue() - case DBR_CHAR\n");
           if (arguments->hex){
                printf("0x%" PRIx8, ((u_int8_t*) value)[j]);
            }
            else if (arguments->oct){
                printf("0o%" PRIo8, ((u_int8_t*) value)[j]);
            }
            else if (arguments->bin){
                printf("0b");
                printBits(((u_int8_t*) value)[j]);
            }
            else{    /* output as a number */
                printf("%" PRIu8, ((u_int8_t*) value)[j]);
            }
            break;
        }

        default:
            errPeriodicPrint("Unrecognized DBR type.\n");
            break;
        }

    }

}


void printOutput(evargs args, arguments_T *arguments){
/*  prints global output strings corresponding to i-th channel. */

    struct channel *ch = (struct channel *)args.usr;
    /* do formating */

    /* check alarm limits */
    if (arguments->stat || (!arguments->nostat && (ch->status != 0 || ch->severity != 0))) { /*   display status/severity */
        if (ch->status <= lastEpicsAlarmCond) sprintf(g_outStat[ch->i],"STAT:%s", epicsAlarmConditionStrings[ch->status]); /*  strcpy(outStat[ch->i], epicsAlarmConditionStrings[ch->status]); */
        else sprintf(g_outStat[ch->i],"UNKNOWN: %u",ch->status);

        if (ch->severity <= lastEpicsAlarmSev) sprintf(g_outSev[ch->i],"SEVR:%s", epicsAlarmSeverityStrings[ch->severity]);/* strcpy(outSev[ch->i], epicsAlarmSeverityStrings[ch->]); */
        else sprintf(g_outSev[ch->i],"UNKNOWN: %u",ch->status);
    }

    if (args.type >= DBR_TIME_STRING && args.type <= DBR_TIME_DOUBLE){/* otherwise we don't have it */
        /* we assume that manually specifying dbr_time implies -time or -date. */
        if (arguments->date || arguments->dbrRequestType != -1) epicsTimeToStrftime(g_outDate[ch->i], LEN_TIMESTAMP, "%Y-%m-%d", &g_timestampRead[ch->i]);
        if (arguments->time || arguments->dbrRequestType != -1) epicsTimeToStrftime(g_outTime[ch->i], LEN_TIMESTAMP, "%H:%M:%S.%06f", &g_timestampRead[ch->i]);
    }


    /* show local date or time? */
    if (arguments->localdate || arguments->localtime){
        epicsTimeStamp localTime;
        epicsTimeGetCurrent(&localTime);
        /* validateTimestamp(&localTime, "localTime"); */

        if (arguments->localdate) epicsTimeToStrftime(g_outLocalDate[ch->i], LEN_TIMESTAMP, "%Y-%m-%d", &localTime);
        if (arguments->localtime) epicsTimeToStrftime(g_outLocalTime[ch->i], LEN_TIMESTAMP, "%H:%M:%S.%06f", &localTime);
    }

    /* if both local and server times are requested, clarify which is which */
    bool doubleTime = (arguments->localdate || arguments->localtime) && (arguments->date || arguments->time);
    if (doubleTime){
        fputs("server time: ",stdout);
    }

    /* server date */
    if (!isStrEmpty(g_outDate[ch->i]))    printf("%s ",g_outDate[ch->i]);
    /* server time */
    if (!isStrEmpty(g_outTime[ch->i]))    printf("%s ",g_outTime[ch->i]);

    if (doubleTime){
        fputs("local time: ",stdout);
    }

    /* local date */
    if (!isStrEmpty(g_outLocalDate[ch->i]))   printf("%s ",g_outLocalDate[ch->i]);
    /* local time */
    if (!isStrEmpty(g_outLocalTime[ch->i]))   printf("%s ",g_outLocalTime[ch->i]);


    /* timestamp if monitor and if req[i]uested */
    if ((arguments->tool == camon || arguments->tool == cainfo) && arguments->timestamp)   printf("%s ", g_outTimestamp[ch->i]);

    /* channel name */
    if (!arguments->noname)  printf("%s ",ch->base.name);

    if (arguments->nord) printf("%lu ", args.count); /*  show nord if requested */

    /* value(s) */
    printValue(args, arguments);


    fputc(' ',stdout);

    /* egu */
    if (!isStrEmpty(g_outUnits[ch->i]) && !arguments->nounit) printf("%s ",g_outUnits[ch->i]);

    /* severity */
    if (!isStrEmpty(g_outSev[ch->i])) printf("(%s",g_outSev[ch->i]);

    /* status */
    if (!isStrEmpty(g_outStat[ch->i])) printf(" %s)",g_outStat[ch->i]);
    else if (!isStrEmpty(g_outSev[ch->i])) putc(')', stdout);

    putc('\n', stdout);
    return 0;
}


