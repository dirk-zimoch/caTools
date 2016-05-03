#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h> 
#include "caToolsTypes.h"
#include "caToolsUtils.h"



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
    if (length > MAX_STRING_SIZE-1) {
        warnPrint("Input %s is longer than the allowed size %u.\n", argument, MAX_STRING_SIZE-1);
        length = MAX_STRING_SIZE;
        argument[length-1] = '\0';
        warnPrint("Truncating to: %s\n", argument);   /*  end of upper fprintf */
    }
    return length;
}

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
