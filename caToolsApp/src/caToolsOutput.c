#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <math.h>
#include <assert.h>
#include <strings.h>
#include <string.h>

#include "epicsTime.h"
#include "caToolsTypes.h"
#include "caToolsUtils.h"
#include "caToolsOutput.h"


bool getEnumString(char ** str, evargs * args, size_t j){
    void * value = dbr_value_ptr((*args).dbr, (*args).type);
    dbr_enum_t v = ((dbr_enum_t *)value)[j];
    if (v >= MAX_ENUM_STATES){
        warnPrint("Illegal enum index: %d\n", v);
        *str = "\0";
        return false;
    }
    if (dbr_type_is_GR((*args).type)) {
        if (v >= ((struct dbr_gr_enum *)(*args).dbr)->no_str) {
            warnPrint("Enum index value %d greater than the number of strings\n", v);
            *str = "\0";
            return false;
        }
        else{
            *str = ((struct dbr_gr_enum *)(*args).dbr)->strs[v];
            return true;
        }
    }
    else if (dbr_type_is_CTRL((*args).type)) {
        if (v >= ((struct dbr_ctrl_enum *)(*args).dbr)->no_str) {
            warnPrint("Enum index value %d greater than the number of strings\n", v);
            *str = "\0";
            return false;
        }
        else{
            *str = ((struct dbr_ctrl_enum *)(*args).dbr)->strs[v];
            return true;
        }
    }
    return true;
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
    if(         (baseType == DBR_CHAR && (ch->type == DBF_STRING || ch->type == DBR_CTRL_STRING)) ||        /* this is long string */
                (baseType == DBR_CHAR && !arguments->fieldSeparator && !arguments->parseArray && !(arguments->num) &&   /* no special numeric formating specified */
                (ch->type == DBF_STRING || ch->type == DBR_CTRL_STRING ||  /* if base channel type is string*/
                 arguments->str ||                           /* if requested string formatting */
                 (args.count > 1 && isPrintable((char *) value, (size_t) args.count))))/* if array returned and all characters are printable */
        )
    {  /* print as string */
        debugPrint("printValue() - case long string with count: %ld\n", args.count);
        printf("\"%.*s\"", (int) args.count, (char *) value);
        return;
    }

    /* loop over the whole array */

    long j; /* element counter */

    for (j=0; j<args.count; ++j){

        if (j){
            if(arguments->fieldSeparator==0)arguments->fieldSeparator=' '; /* use space as default separator */
            printf ("%c", arguments->fieldSeparator); /* insert element separator */
        }

        switch (baseType) {
        case DBR_STRING:
            debugPrint("printValue() - case DBR_STRING - print as normal string\n");
            printf("\"%.*s\"", MAX_STRING_SIZE, ((dbr_string_t*) value)[j]);
            break;
        case DBR_FLOAT:
        case DBR_DOUBLE:{
            debugPrint("printValue() - case DBR_FLOAT or DBR_DOUBLE\n");
            if (baseType == DBR_FLOAT) valueDbl = ((dbr_float_t*)value)[j];
            else valueDbl = ((dbr_double_t*)value)[j];

            /* round if desired or will be writen as hex oct, bin or string */
            if (arguments->round == roundType_round || arguments->hex || arguments->oct || arguments->bin) {
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
                    /* print raw when -plain is used */
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

            if (!arguments->num) { /*  if not requested as a number check if we can get string */
                char * str = NULL;
                if(getEnumString(&str, &args, j)){
                    printf("\"%.*s\"", MAX_ENUM_STRING_SIZE, str);
                    break;
                }else if(arguments->str){
                    errPrint("\nValue can not be converted to corresponding string\n");
                    break;
                }
            }
            /*  else write value as number. */
            dbr_enum_t v = ((dbr_enum_t *)value)[j];
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
                debugPrint("printValue() - case DBR_CHAR - hex\n");
                printf("0x%" PRIx8, ((u_int8_t*) value)[j]);
            }
            else if (arguments->oct){
                debugPrint("printValue() - case DBR_CHAR - oct\n");
                printf("0o%" PRIo8, ((u_int8_t*) value)[j]);
            }
            else if (arguments->bin){
                debugPrint("printValue() - case DBR_CHAR - bin\n");
                printf("0b");
                printBits(((u_int8_t*) value)[j]);
            }
            else if (!arguments->str){    /* output as a number */
                debugPrint("printValue() - case DBR_CHAR - num\n");
                printf("%" PRIu8, ((u_int8_t*) value)[j]);
            }else{ /*print as char*/
               printf("\'%c\'", ((char*) value)[j]);
            }
            break;
        }

        default:
            errPeriodicPrint("Unrecognized DBR type.\n");
            break;
        }
    }
}

static void printTimestamp(const char* fmt, epicsTimeStamp *pts){
    char buffer[100]; /* epicsTimeToStrftime does not report errors on overflow! It just prints less. */
    epicsTimeToStrftime(buffer, sizeof(buffer), fmt, pts);
    fputs(buffer, stdout);
    putc(' ', stdout);
}

void printOutput(evargs args, arguments_T *arguments){
/*  prints global output strings corresponding to i-th channel. */

    struct channel *ch = (struct channel *)args.usr;
    bool isTimeType = arguments->dbrRequestType >= DBR_TIME_STRING && arguments->dbrRequestType <= DBR_TIME_DOUBLE;

    /* do formating */

    /* if both local and server times are requested, clarify which is which */
    bool doubleTime = (arguments->localdate || arguments->localtime || arguments->tfmt) &&
        (arguments->date || arguments->time || isTimeType || arguments->ltfmt);
    if (doubleTime)
        fputs("server time: ", stdout);

    /* we assume that manually specifying dbr_time implies -time and -date if no -tfmt is given. */
    if (arguments->date || (isTimeType && !arguments->tfmt))
        printTimestamp("%Y-%m-%d", &ch->timestamp);
    if (arguments->time || (isTimeType && !arguments->tfmt))
        printTimestamp("%H:%M:%S.%06f", &ch->timestamp);
    if (arguments->tfmt)
        printTimestamp(arguments->tfmt, &ch->timestamp);

    /* show local date or time? */
    if (arguments->localdate || arguments->localtime || arguments->ltfmt) {
        epicsTimeStamp localTime;
        epicsTimeGetCurrent(&localTime);

        if (doubleTime)
            fputs("local time: ", stdout);
        /* validateTimestamp(&localTime, "localTime"); */

        if (arguments->localdate)
            printTimestamp("%Y-%m-%d", &localTime);
        if (arguments->localtime)
            printTimestamp("%H:%M:%S.%06f", &localTime);
        if (arguments->ltfmt)
            printTimestamp(arguments->ltfmt, &localTime);
    }

    /* timestamp if monitor*/
    if ((arguments->tool == camon || arguments->tool == cainfo) && arguments->timestamp) {
        if(arguments->localdate || arguments->localtime || arguments->date || arguments->time || isTimeType) {
            fputs("timestamp: ", stdout);
        }
        printf("%s ", g_outTimestamp[ch->i]);
    }

    /* channel name */
    if (!arguments->noname) {
        fputs(ch->base.name, stdout);
        putc(' ', stdout);
    }

    if (arguments->nord) {
        if(args.chid == ch->lstr.id) {  /* long strings are strings...so nord is 1 */
            fputs("1 ", stdout);
        }
        else {
            printf("%lu ", args.count); /*  show nord if requested */
        }
    }

    /* value(s) */
    printValue(args, arguments);

    /* egu */
    if (ch->units && !arguments->nounit)
    {
        putc(' ', stdout);
        fputs(ch->units, stdout);
    }

    /* display severity/status if necessary */
    if (arguments->stat || (!arguments->nostat && (ch->status != 0 || ch->severity != 0))) {
        fputs(" SEVR:", stdout);
        if (ch->severity <= lastEpicsAlarmSev)
            fputs(epicsAlarmSeverityStrings[ch->severity], stdout);
        else
            printf("%u", ch->severity);
        fputs(" STAT:", stdout);
        if (ch->status <= lastEpicsAlarmCond)
            fputs(epicsAlarmConditionStrings[ch->status], stdout);
        else
            printf("%u", ch->status);
    }
    putc('\n', stdout);
    return;
}


/*macros for reading requested data */
#define severity_status_get(T) \
ch->status = ((struct T *)args.dbr)->status; \
ch->severity = ((struct T *)args.dbr)->severity;
#define timestamp_get(T) \
    ch->timestamp = ((struct T *)args.dbr)->stamp;\
    validateTimestamp(&ch->timestamp, ch->base.name);
#define units_get_cb(T) ch->units = strdup(((struct T *)args.dbr)->units);
#define precision_get(T) ch->prec = (((struct T *)args.dbr)->precision);

void getMetadataFromEvArgs(struct channel * ch, evargs args){
    /* clear global output strings; the purpose of this callback is to overwrite them */
    /* the exception are units and precision, which we may be getting from elsewhere; we only clear them if we can write them */
    debugPrint("getMetadataFromEvArgs() - %s\n", ch->base.name);

    ch->status=0;
    ch->severity=0;

    /* read requested data */
    switch (args.type) {
        case DBR_GR_STRING:
        case DBR_CTRL_STRING:
        case DBR_STS_STRING:
            severity_status_get(dbr_sts_string);
            break;

        case DBR_STS_INT: /*and SHORT */
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

        case DBR_TIME_INT:  /*and SHORT */
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

        case DBR_GR_INT:    /* and SHORT */
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
            /* does not have units */
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

        case DBR_CTRL_SHORT:  /* and DBR_CTRL_INT */
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
            /* does not have units */
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

        case DBR_CHAR:
        case DBR_STRING:/*dont print the warning if any of these */
        case DBR_SHORT:
        case DBR_FLOAT:
        case DBR_ENUM:
        case DBR_LONG:
        case DBR_DOUBLE: break;
        default :
            errPeriodicPrint("Can not print %s DBR type (DBR numeric type code: %ld). \n", dbr_type_to_text(args.type), args.type);
    }
}

void getTimeStamp(struct channel * ch, arguments_T * arguments) {
/*calculates timestamp for monitor tool, formats it and saves it into the global string. */

    epicsTimeStamp elapsed;
    bool negative=false;
    size_t commonI = (arguments->timestamp == 'c') ? ch->i : 0;
    bool showEmpty = false;

    if (arguments->timestamp == 'r') {
        /*calculate elapsed time since startTime */
        negative = epicsTimeDiffFull(&elapsed, &ch->timestamp, &g_programStartTime);
    }
    else if(arguments->timestamp == 'c' || arguments->timestamp == 'u') {
        /*calculate elapsed time since last update; if using 'c' keep */
        /*timestamps separate for each channel, otherwise use lastUpdate[0] */
        /*for all channels (commonI). */

        /* firstUpdate var is set at the end of caReadCallback, just before printing results. */
        if (g_firstUpdate[ch->i]) {
            negative = epicsTimeDiffFull(&elapsed, &ch->timestamp, &g_lastUpdate[commonI]);
        }
        else {
            g_firstUpdate[ch->i] = true;
            showEmpty = true;
        }

        g_lastUpdate[commonI] = ch->timestamp; /* reset */
    }

    /*convert to h,m,s,ns */
    struct tm tm;
    unsigned long nsec;
    int status = epicsTimeToGMTM(&tm, &nsec, &elapsed);
    assert(status == epicsTimeOK);

    if (showEmpty) {
        /*this is the first update for this channel */
        sprintf(g_outTimestamp[ch->i],"%19c",' ');
    }
    else {  /*save to outTs string */
        char cSign = negative ? '-' : ' ';
        sprintf(g_outTimestamp[ch->i],"%c%02d:%02d:%02d.%09lu", cSign,tm.tm_hour, tm.tm_min, tm.tm_sec, nsec);
    }
}


bool cawaitEvaluateCondition(struct channel * ch, evargs args){
    /*evaluates output of channel i against the corresponding condition. */
    /*returns 1 if matching, 0 otherwise, and -1 if error. */
    /*Before evaluation, channel output is converted to double. If this is */
    /*not successful, the function returns -1. If the channel in question */
    /*is an array, the condition is evaluated against the first element. */

    /*get value */
    void *nativeValue = dbr_value_ptr(args.dbr, args.type);
    bool isStrOperator = ch->conditionOperator == operator_streq || ch->conditionOperator == operator_strneq;

    /*convert the value to double */
    double dblValue;
    char * strVal = NULL;
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
        if(!isStrOperator){
            dblValue = (double) *(dbr_char_t*)nativeValue;
            break;
        }else{
            strVal = (char *) nativeValue;
        }
    case DBR_STRING:
        if (!isStrOperator){
            if(sscanf(nativeValue, "%lf", &dblValue) <= 0){        /* TODO: is this always null-terminated? */
                errPeriodicPrint("Record %s value %s cannot be converted to double.\n", ch->base.name, (char*)nativeValue);
                return false;
            }
        }else{
            strVal = (char *) nativeValue;
        }
        break;
    case DBR_ENUM:
        if(!isStrOperator){
            dblValue = (double) *(dbr_enum_t*)nativeValue;
        }else{
            getEnumString(&strVal, &args, 0);
        }
        break;
    default:
        errPrint("Unrecognized DBR type.\n");
        return false;
        break;
    }

    debugPrint("cawaitEvaluateCondition() - operator: %i, a0: %f, a1: %f, value: %f\n", ch->conditionOperator, ch->conditionOperands[0], ch->conditionOperands[1], dblValue);

    /*evaluate and exit */
    switch (ch->conditionOperator){
    case operator_gt:
        return dblValue > ch->conditionOperands[0];
    case operator_gte:
        return dblValue >= ch->conditionOperands[0];
    case operator_lt:
        return dblValue < ch->conditionOperands[0];
    case operator_lte:
        return dblValue <= ch->conditionOperands[0];
    case operator_eq:
        return dblValue == ch->conditionOperands[0];
    case operator_neq:
        return dblValue != ch->conditionOperands[0];
    case operator_in:
        return (dblValue >= ch->conditionOperands[0]) && (dblValue <= ch->conditionOperands[1]);
    case operator_out:
        return !((dblValue >= ch->conditionOperands[0]) && (dblValue <= ch->conditionOperands[1]));
    case operator_streq:
        debugPrint("cawaitEvaluateCondition() - streq: \"%s\" ... \"%s\"\n",ch->inpStr, strVal);
        return (strcmp(ch->inpStr, strVal) == 0);
    case operator_strneq:
        debugPrint("cawaitEvaluateCondition() - strneq: \"%s\" ... \"%s\"\n",ch->inpStr, strVal);
        return (strcmp(ch->inpStr, strVal) != 0);
    }

    return false;
}
