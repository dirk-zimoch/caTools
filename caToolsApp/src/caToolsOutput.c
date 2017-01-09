#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <math.h>
#include <assert.h>
#include <string.h>

#include "epicsTime.h"
#include "caToolsTypes.h"
#include "caToolsUtils.h"
#include "caToolsOutput.h"
#include "alarmString.h"


/**
 * @brief getEnumString tries to return enum string from the evargs
 * @param str is pointer to the output string
 * @param args evargs struct obtained from the callback
 * @param j value index as if channel is an array (0 in case of default one dimensional channels)
 * @return true if the string value is obtained - false if not
 */
bool getEnumString(char ** str, evargs * args, size_t j){

    void * value = dbr_value_ptr((*args).dbr, (*args).type);
    dbr_enum_t v = ((dbr_enum_t *)value)[j];
    if (v >= MAX_ENUM_STATES){
        warnPrint("Enum index value %d is greater than MAX_ENUM_STATES %d\n", v, MAX_ENUM_STATES);
        *str = "";
        return false;
    }
    if (dbr_type_is_GR((*args).type)) {
        if (v >= ((struct dbr_gr_enum *)(*args).dbr)->no_str) {
            warnPrint("Enum index value %d greater than the number of strings\n", v);
            *str = "";
            return false;
        }
        else{
            *str = ((struct dbr_gr_enum *)(*args).dbr)->strs[v];
            if(*str[0]=='\0'){
                return false;
            }
            else{
                return true;
            }
        }
    }
    else if (dbr_type_is_CTRL((*args).type)) {
        if (v >= ((struct dbr_ctrl_enum *)(*args).dbr)->no_str) {
            warnPrint("Enum index value %d greater than the number of strings\n", v);
            *str = "";
            return false;
        }
        else{
            *str = ((struct dbr_ctrl_enum *)(*args).dbr)->strs[v];
            if(*str[0]=='\0'){
                return false;
            }
            else{
                return true;
            }
        }
    }
    else if (dbr_type_is_TIME((*args).type)) {
        /* get enum strings from the saved values */
        struct channel *ch = ((struct field *)args->usr)->ch;
        if (v >= ch->enum_no_st) {
            warnPrint("Enum index value %d greater than the number of strings\n", v);
            *str = "";
            return false;
        }
        else{
            *str = ch->enum_strs[v];
            if(*str[0]=='\0'){
                return false;
            }
            else{
                return true;
            }
        }
    }
    return false;
}

/**
 * @brief printValue - formats the returned value and prints it - usually called from within printOutput
 * @param args - evargs from callback function
 * @param arguments - pointer to the arguments struct
 */
void printValue(evargs args, arguments_T *arguments){
/* Parses the data fetched by ca_get callback according to the data type and formatting arguments-> */
/* The result is printed to stdout. */


    int32_t baseType;
    void *value;
    double valueDbl = 0;

    value = dbr_value_ptr(args.dbr, args.type);
    baseType = dbr_type_to_DBF(args.type);   /*  convert appropriate TIME, GR, CTRL,... type to basic one */
    struct channel *ch = ((struct field *)args.usr)->ch;


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
                printf("%c", (uint8_t)valueInt32);
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
                printf("0x%" PRIx16, (uint16_t)valueInt16);
            }
            else if (arguments->oct){
                printf("0o%" PRIo16, (uint16_t)valueInt16);
            }
            else if (arguments->bin){
                printf("0b");
                printBits((uint16_t)valueInt16);
            }
            else if (arguments->str){
                printf("%c", (uint8_t)valueInt16);
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
                printf("0x%" PRIx8, ((uint8_t*) value)[j]);
            }
            else if (arguments->oct){
                debugPrint("printValue() - case DBR_CHAR - oct\n");
                printf("0o%" PRIo8, ((uint8_t*) value)[j]);
            }
            else if (arguments->bin){
                debugPrint("printValue() - case DBR_CHAR - bin\n");
                printf("0b");
                printBits(((uint8_t*) value)[j]);
            }
            else if (!arguments->str){    /* output as a number */
                debugPrint("printValue() - case DBR_CHAR - num\n");
                printf("%" PRIu8, ((uint8_t*) value)[j]);
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

/**
 * @brief printTimestamp prints epics timestamp
 * @param fmt - time format
 * @param pts - epics timestamp
 */
static void printTimestamp(const char* fmt, epicsTimeStamp *pts){
    char buffer[100]; /* epicsTimeToStrftime does not report errors on overflow! It just prints less. */
    epicsTimeToStrftime(buffer, sizeof(buffer), fmt, pts);
    fputs(buffer, stdout);
    putc(' ', stdout);
}

/**
 * @brief printOutput - prints metatata and calls printValue();
 * @param args - evargs from callback function
 * @param arguments - pointer to the (input flags) arguments struct
 */
void printOutput(evargs args, arguments_T *arguments){
/*  prints global output strings corresponding to i-th channel. */

    struct channel *ch = ((struct field *)args.usr)->ch;
    bool isTimeType = arguments->dbrRequestType >= DBR_TIME_STRING && arguments->dbrRequestType <= DBR_TIME_DOUBLE;

    /* do formating */

    /* if both local and server times are requested, clarify which is which */
    bool doubleTime = (arguments->localdate || arguments->localtime || arguments->tfmt) +
        (arguments->date || arguments->time || isTimeType || arguments->ltfmt) +
        (arguments->timestamp != 0) > 1;

    /* relative timestamp */
    if ((arguments->tool == camon || arguments->tool == cainfo) && arguments->timestamp) {
        epicsTimeStamp* lastTimeStamp;
        epicsTimeStamp elapsed;
        bool negative;

        lastTimeStamp = arguments->timestamp == 'r' ? &g_programStartTime : arguments->timestamp == 'c' ? &ch->lastUpdate : &g_commonLastUpdate;

        if(doubleTime)
            fputs("timestamp:", stdout);

        if (lastTimeStamp->secPastEpoch == 0 && lastTimeStamp->nsec == 0)
            fputs("                    ", stdout);
        else
        {
            /*convert to h,m,s,ns */
            struct tm tm;
            unsigned long nsec;

            negative = epicsTimeDiffFull(&elapsed, &ch->timestamp, lastTimeStamp);
            epicsTimeToGMTM(&tm, &nsec, &elapsed);

            printf("%c%02d:%02d:%02d.%09lu ", negative ? '-' : ' ', tm.tm_hour, tm.tm_min, tm.tm_sec, nsec);
        }
        if (arguments->timestamp != 'r')
            *lastTimeStamp = ch->timestamp;
    }

    /* server time stamp */
    if (arguments->date || arguments->time || arguments->tfmt || isTimeType) {
        if (doubleTime)
            fputs("server time: ", stdout);

        /* we assume that manually specifying dbr_time implies -time and -date if no -tfmt is given. */
        if (arguments->date || (isTimeType && !arguments->tfmt))
            printTimestamp("%Y-%m-%d", &ch->timestamp);
        if (arguments->time || (isTimeType && !arguments->tfmt))
            printTimestamp("%H:%M:%S.%06f", &ch->timestamp);
        if (arguments->tfmt)
            printTimestamp(arguments->tfmt, &ch->timestamp);
    }

    /* local time */
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

/**
 * @brief getMetadataFromEvArgs - updates global strings and metadata for the channel
 * @param ch - pointer to struct channel
 * @param args - evargs from callback function
 * @return true if everything goes well
 */
void getMetadataFromEvArgs(struct channel * ch, evargs args){
    /* clear global output strings; the purpose of this callback is to overwrite them */
    /* the exception are units and precision, which we may be getting from elsewhere; we only clear them if we can write them */
    debugPrint("getMetadataFromEvArgs() - %s\n", ch->base.name);

    /* define macros for reading requested data */
        #define severity_status_get(T) \
        ch->status = ((struct T *)args.dbr)->status; \
        ch->severity = ((struct T *)args.dbr)->severity;
        #define timestamp_get(T) \
            ch->timestamp = ((struct T *)args.dbr)->stamp;\
            validateTimestamp(&ch->timestamp, ch->base.name);
        #define units_get_cb(T) ch->units = strdup(((struct T *)args.dbr)->units);
        #define precision_get(T) ch->prec = (((struct T *)args.dbr)->precision);

        /* allocate memmory for meta data strings if needed and generate a meta string for a particular dbr metadata */
        /* T ... dbr_type,
         * D ... ch->fieldname
         * F ... printf format  */
#ifdef _WIN32
        #define get_meta_string(T, D, F) if(ch->D == NULL) \
            ch->D = (char *) callocMustSucceed(MAX_STRING_SIZE, sizeof(char), "Can't allocate  buffer for meta string");\
            if(ch->D != NULL) { \
                _snprintf(ch->D, MAX_STRING_SIZE, F, ((struct T *)args.dbr)->D); \
                ch->D[MAX_STRING_SIZE-1] = '\0'; \
            }
#else
        #define get_meta_string(T, D, F) if(ch->D == NULL) \
            ch->D = (char *) callocMustSucceed(MAX_STRING_SIZE, sizeof(char), "Can't allocate  buffer for meta string");\
            if(ch->D != NULL) { \
                snprintf(ch->D, MAX_STRING_SIZE, F, ((struct T *)args.dbr)->D); \
            }
#endif



    /* end of macros */

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
            get_meta_string(dbr_gr_int, lower_disp_limit, "%"PRId16"");
            get_meta_string(dbr_gr_int, upper_disp_limit, "%"PRId16"");
            break;

        case DBR_GR_FLOAT:
            severity_status_get(dbr_gr_float);
            units_get_cb(dbr_gr_float);
            precision_get(dbr_gr_float);
            get_meta_string(dbr_gr_float, lower_disp_limit, "%g");
            get_meta_string(dbr_gr_float, upper_disp_limit, "%g");
            break;

        case DBR_GR_ENUM:
            severity_status_get(dbr_gr_enum);
            /* does not have units */
            break;

        case DBR_GR_CHAR:
            severity_status_get(dbr_gr_char);
            units_get_cb(dbr_gr_char);
            get_meta_string(dbr_gr_char, lower_disp_limit, "%"PRIu8"");
            get_meta_string(dbr_gr_char, upper_disp_limit, "%"PRIu8"");
            break;

        case DBR_GR_LONG:
            severity_status_get(dbr_gr_long);
            units_get_cb(dbr_gr_long);
            get_meta_string(dbr_gr_long, lower_disp_limit, "%"PRId32"");
            get_meta_string(dbr_gr_long, upper_disp_limit, "%"PRId32"");
            break;

        case DBR_GR_DOUBLE:
            severity_status_get(dbr_gr_double);
            units_get_cb(dbr_gr_double);
            precision_get(dbr_gr_double);
            get_meta_string(dbr_gr_double, lower_disp_limit, "%g");
            get_meta_string(dbr_gr_double, upper_disp_limit, "%g");
            break;

        case DBR_CTRL_SHORT:  /* and DBR_CTRL_INT */
            severity_status_get(dbr_ctrl_short);
            units_get_cb(dbr_ctrl_short);
            get_meta_string(dbr_ctrl_short, lower_disp_limit, "%"PRId16"");
            get_meta_string(dbr_ctrl_short, upper_disp_limit, "%"PRId16"");
            get_meta_string(dbr_ctrl_short, lower_warning_limit, "%"PRId16"");
            get_meta_string(dbr_ctrl_short, upper_warning_limit, "%"PRId16"");
            get_meta_string(dbr_ctrl_short, lower_alarm_limit, "%"PRId16"");
            get_meta_string(dbr_ctrl_short, upper_alarm_limit, "%"PRId16"");
            get_meta_string(dbr_ctrl_short, lower_ctrl_limit, "%"PRId16"");
            get_meta_string(dbr_ctrl_short, upper_ctrl_limit, "%"PRId16"");
            break;

        case DBR_CTRL_FLOAT:
            severity_status_get(dbr_ctrl_float);
            units_get_cb(dbr_ctrl_float);
            precision_get(dbr_ctrl_float);
            get_meta_string(dbr_ctrl_float, lower_disp_limit, "%g");
            get_meta_string(dbr_ctrl_float, upper_disp_limit, "%g");
            get_meta_string(dbr_ctrl_float, lower_warning_limit, "%g");
            get_meta_string(dbr_ctrl_float, upper_warning_limit, "%g");
            get_meta_string(dbr_ctrl_float, lower_alarm_limit, "%g");
            get_meta_string(dbr_ctrl_float, upper_alarm_limit, "%g");
            get_meta_string(dbr_ctrl_float, lower_ctrl_limit, "%g");
            get_meta_string(dbr_ctrl_float, upper_ctrl_limit, "%g");
            break;

        case DBR_CTRL_ENUM:
            severity_status_get(dbr_ctrl_enum);
            /* does not have units */

            /* remember enum strings to be able to print them later */
            ch->enum_no_st = ((struct dbr_ctrl_enum *)args.dbr)->no_str;
            if (ch->enum_strs == NULL)
                ch->enum_strs = (char **)callocMustSucceed(MAX_STRING_SIZE, sizeof(((struct dbr_ctrl_enum *)args.dbr)->strs), "Can't allocate buffer for enum strings in getMetadataFromEvArgs");
            if (ch->enum_strs != NULL){
                int j=0;
                for (j=0; j<ch->enum_no_st; j++){
                    ch->enum_strs[j]=strdup(((struct dbr_ctrl_enum *)args.dbr)->strs[j]);
                }
            }
            break;

        case DBR_CTRL_CHAR:
            severity_status_get(dbr_ctrl_char);
            units_get_cb(dbr_ctrl_char);
            get_meta_string(dbr_ctrl_char, lower_disp_limit, "%"PRIu8"");
            get_meta_string(dbr_ctrl_char, upper_disp_limit, "%"PRIu8"");
            get_meta_string(dbr_ctrl_char, lower_warning_limit, "%"PRIu8"");
            get_meta_string(dbr_ctrl_char, upper_warning_limit, "%"PRIu8"");
            get_meta_string(dbr_ctrl_char, lower_alarm_limit, "%"PRIu8"");
            get_meta_string(dbr_ctrl_char, upper_alarm_limit, "%"PRIu8"");
            get_meta_string(dbr_ctrl_char, lower_ctrl_limit, "%"PRIu8"");
            get_meta_string(dbr_ctrl_char, upper_ctrl_limit, "%"PRIu8"");
            break;

        case DBR_CTRL_LONG:
            severity_status_get(dbr_ctrl_long);
            units_get_cb(dbr_ctrl_long);
            get_meta_string(dbr_ctrl_long, lower_disp_limit, "%"PRId32"");
            get_meta_string(dbr_ctrl_long, upper_disp_limit, "%"PRId32"");
            get_meta_string(dbr_ctrl_long, lower_warning_limit, "%"PRId32"");
            get_meta_string(dbr_ctrl_long, upper_warning_limit, "%"PRId32"");
            get_meta_string(dbr_ctrl_long, lower_alarm_limit, "%"PRId32"");
            get_meta_string(dbr_ctrl_long, upper_alarm_limit, "%"PRId32"");
            get_meta_string(dbr_ctrl_long, lower_ctrl_limit, "%"PRId32"");
            get_meta_string(dbr_ctrl_long, upper_ctrl_limit, "%"PRId32"");
            break;

        case DBR_CTRL_DOUBLE:
            severity_status_get(dbr_ctrl_double);
            units_get_cb(dbr_ctrl_double);
            precision_get(dbr_ctrl_double);
            get_meta_string(dbr_ctrl_double, lower_disp_limit, "%g");
            get_meta_string(dbr_ctrl_double, upper_disp_limit, "%g");
            get_meta_string(dbr_ctrl_double, lower_warning_limit, "%g");
            get_meta_string(dbr_ctrl_double, upper_warning_limit, "%g");
            get_meta_string(dbr_ctrl_double, lower_alarm_limit, "%g");
            get_meta_string(dbr_ctrl_double, upper_alarm_limit, "%g");
            get_meta_string(dbr_ctrl_double, lower_ctrl_limit, "%g");
            get_meta_string(dbr_ctrl_double, upper_ctrl_limit, "%g");
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

/**
 * @brief cawaitEvaluateCondition evaluates output of channel i against the corresponding wait condition.
 *              returns 1 if matching, 0 otherwise, and -1 if error. Before evaluation, channel output is converted to double. If this is
 *              not successful, the function returns -1. If the channel in question is an array, the condition is evaluated against the first element.
 * @param ch pointer to the channel struct
 * @param args evrargs returned by the read callback function
 * @return true if the condition is fulfilled
 */
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
    double dblValue = 0;
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

/**
 * @brief printCainfo - prints metatata and calls printValue();
 * @param args - evargs from callback function
 * @param arguments - pointer to the (input flags) arguments struct
 */
void printCainfo(evargs args, arguments_T *arguments){
    uint32_t j;

    struct channel *ch = ((struct field *)args.usr)->ch;

    bool readAccess, writeAccess;
    const char *delimeter = "-------------------------------";

    /*start printing */
    fputc('\n',stdout);
    fputc('\n',stdout);
    printf("%s\n%s\n", delimeter, ch->base.name);                                               /*name */

    if(isValField(ch->base.name))
        if(ch->fields[field_desc].val != NULL) printf("\tDescription: %s\n", ch->fields[field_desc].val);  /*description */

    if(ch->fields[field_rtyp].val != NULL) printf("\tRecord type: %s\n", ch->fields[field_rtyp].val);  /*record type */
    printf("\tNative DBF type: %s\n", dbf_type_to_text(ca_field_type(ch->base.id)));            /*field type */
    printf("\tNumber of elements: %zu\n", ch->count);                                           /*number of elements */
    fputs("\tTime: ", stdout);
    printTimestamp("%Y-%m-%d %H:%M:%S.%06f", &ch->timestamp);                                   /*timestamp*/
    fputc('\n',stdout);
    printf("\tValue: ");                                                                        /*value*/
    printValue(args, arguments);
    fputc('\n',stdout);
    if(ch->units)
        printf("\tUnits: %s\n", ch->units);
    fputc('\n',stdout);
    printf("\tAlarm status: %s, severity: %s\n",
           epicsAlarmSeverityStrings[ch->severity],
           epicsAlarmConditionStrings[ch->status]);
    if(ch->upper_warning_limit || ch->lower_warning_limit)
        printf("\n\tWarning\tupper limit: %s\n\t\tlower limit: %s\n",
                ch->upper_warning_limit, ch->lower_warning_limit);
    if(ch->upper_alarm_limit || ch->lower_alarm_limit)
        printf("\tAlarm\tupper limit: %s\n\t\tlower limit: %s\n",
                ch->upper_alarm_limit, ch->lower_alarm_limit);
    if(ch->upper_ctrl_limit || ch->upper_ctrl_limit)
        printf("\tControl\tupper limit: %s\n\t\tlower limit: %s\n",
               ch->upper_ctrl_limit, ch->lower_ctrl_limit);
    if(ch->upper_disp_limit || ch->upper_disp_limit)
        printf("\tDisplay\tupper limit: %s\n\t\tlower limit: %s\n",
               ch->upper_disp_limit, ch->lower_disp_limit);
    fputc('\n',stdout);
    if(dbr_type_is_FLOAT(args.type) || dbr_type_is_DOUBLE(args.type))
        printf("\tPrecision: %"PRId16"\n\n",ch->prec);


    if(ch->enum_strs){
        printf("\tNumber of enum strings: %"PRId16"\n", ch->enum_no_st);
        for (j=0; j < ch->enum_no_st; ++j) {
            printf("\tstring %"PRIu32": %s\n", j, ch->enum_strs[j]);
        }
        if(j>0) fputc('\n',stdout);
    }

    bool printed_any = false;
    if(!isValField(ch->base.name)) {
        getBaseChannelName(ch->base.name);
        printf("\t%s info:\n", ch->base.name);
        if(ch->fields[field_desc].val != NULL) printf("\tDescription: %s\n", ch->fields[field_desc].val);
        printed_any = true;
    }

    uint32_t nFields = sizeof(ch->fields)/sizeof(ch->fields[0]);
    for(j=field_hhsv; j < nFields; j++) {
        if (ch->fields[j].val != NULL) {
            printf("\t%s alarm severity: %.*s\n", cainfo_fields[j], MAX_STRING_SIZE, ch->fields[j].val);
            printed_any = true;
        }
    }
    if (printed_any)
        fputc('\n',stdout);


    readAccess = ca_read_access(ch->base.id);
    writeAccess = ca_write_access(ch->base.id);

    printf("\tCA host name: %s\n", ca_host_name(ch->base.id));                           /*host name */
    printf("\tRead access: "); if(readAccess) printf("yes\n"); else printf("no\n");     /*read and write access */
    printf("\tWrite access: "); if(writeAccess) printf("yes\n"); else printf("no\n");
    printf("%s\n", delimeter);

    return;
}
