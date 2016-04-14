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
//checks whether end of src matches the string match.
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

    while(i <= len) {  // field name + '.'
        if(name[len-i] == '.') {    // found '.' which separates base record name and field name
            name[len-i] = '\0';     // just terminate the string here.
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
        warnPrint("%s\n", argument);   // end of upper fprintf
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