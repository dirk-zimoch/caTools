#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h> 
#include "caToolsTypes.h"
#include "caToolsUtils.h"


/**
 * @brief removePrefix removes prefix from data and returns true if successfull
 * @param data is the data to remove the prefix from
 * @param prefix is the prefix to remove from data
 * @return true if prefix was found and removed, false otherwise
 */
bool removePrefix (char **data, char const *prefix) {
    size_t pos = 0;
    while (prefix[pos] != '\0') {
        if ((*data)[pos] != prefix[pos]) {
            return false;
        }
        pos++;
    }
    *data += pos;
    return true;
}

bool endsWith(char * src, char * match) {
/* checks whether end of src matches the string match. */
    if (strlen(src) >= strlen(match)) {
        return !strcmp(src + (strlen(src)-strlen(match)), match) ;
    }
    else return false;
}

/**
 * @brief getBaseChannelName will strip chanel name to base channel name. Base channel name is chanel name, without field names (without .VAL, .PREC, ...)
 * @param name channel name to strip (null terminated string)
 */
void getBaseChannelName(char *name) {
    size_t len = strlen(name);
    size_t i = 0;

    while(i <= len) {  /*  field name + '.' */
        if(name[len-i] == '.') {    /*  found '.' which separates base record name and field name */
            name[len-i] = '\0';     /*  just terminate the string here. */
            break;
        }
        i++;
    }
}

/**
 * @brief isValField returns true if channel name points to VAL field 
 * @param name CA channel name (null terminated string)
 */
bool isValField(char *name) {
    char *s;

    if (endsWith(name, ".VAL")){
    	return true;
    }
	if (strchr(name, '.') == NULL){
		return true;
	}
	return false;    
}

size_t truncate(char *argument) {
    size_t length = strlen(argument);
    if (length > MAX_STRING_SIZE) {
        warnPrint("Input %s is longer than the allowed size %u. Truncating to ", argument, MAX_STRING_SIZE);
        length = MAX_STRING_SIZE;
        argument[length] = '\0';
        warnPrint("%s\n", argument);   /*  end of upper fprintf */
    }
    return length;
}

/**
 * @brief isStrEmpty checks if provided string is empty
 * @param str the string to check
 * @return true if empty, false otherwise
 */
bool isStrEmpty(char *str) {
    return str[0] == '\0';
}


/*  the same as epicsTimeLessThan(), except it only checks if pLeft < pRight */
bool timeLessThan(const epicsTimeStamp *pLeft, const epicsTimeStamp *pRight) {
    return (pLeft->secPastEpoch < pRight->secPastEpoch || (pLeft->secPastEpoch == pRight->secPastEpoch && pLeft->nsec < pRight->nsec));
}


bool epicsTimeDiffFull(epicsTimeStamp *diff, const epicsTimeStamp *pLeft, const epicsTimeStamp *pRight) {
/*  Calculates difference between two epicsTimeStamps: like epicsTimeDiffInSeconds but returning the answer */
/* in form of a timestamp. The absolute value of the difference pLeft - pRight is saved to the timestamp diff, and the */
/* returned value indicates if the said difference is negative. */
    bool negative = timeLessThan(pLeft, pRight);
    if (negative) { /* switch left and right */
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


void validateTimestamp(epicsTimeStamp *timestamp, const char* name) {
/* checks a timestamp for illegal values. */
    if (timestamp->nsec >= 1000000000ul) {
        errPeriodicPrint("Warning: invalid number of nanoseconds in timestamp: %s - assuming 0.\n", name);
        timestamp->nsec = 0;
    }
}

bool isPrintable(char * str, size_t n){
    /* check if the string of characters is printable */
    size_t i = 0;
    char c = '\0';
    while(i < n) {
        c = str[i];
        if (c == '\0')
            return true;
        else if(((c & 0x7f) >= 0x20 || c == 0x0a || c == 0x0c) && c != 0x7f)
            i++;
        else{
            debugPrint("isPrintable() - Invalid character: %c (%x)\n", c, c);
            return false;
        }

    }
    return true;
}



/*macros for reading requested data */
#define severity_status_get(T) \
ch->status = ((struct T *)args.dbr)->status; \
ch->severity = ((struct T *)args.dbr)->severity;
#define timestamp_get(T) \
    g_timestampRead[ch->i] = ((struct T *)args.dbr)->stamp;\
    validateTimestamp(&g_timestampRead[ch->i], ch->base.name);
#define units_get_cb(T) clearStr(g_outUnits[ch->i]); sprintf(g_outUnits[ch->i], "%s", ((struct T *)args.dbr)->units);
#define precision_get(T) ch->prec = (((struct T *)args.dbr)->precision);

bool getMetadataFromEvArgs(struct channel * ch, evargs args){
    /* clear global output strings; the purpose of this callback is to overwrite them */
    /* the exception are units, which we may be getting from elsewhere; we only clear them if we can write them */
    debugPrint("getMetadataFromEvArgs() - %s\n", ch->base.name);
    clearStr(g_outDate[ch->i]);
    clearStr(g_outTime[ch->i]);
    clearStr(g_outSev[ch->i]);
    clearStr(g_outStat[ch->i]);
    clearStr(g_outLocalDate[ch->i]);
    clearStr(g_outLocalTime[ch->i]);

    ch->status=0;
    ch->severity=0;
    ch->precision = 6; /* default precision if none obtained from the IOC*/

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
            return;
    }
}
