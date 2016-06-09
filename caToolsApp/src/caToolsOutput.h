#ifndef caToolsOutput
#define caToolsOutput

#include <stdint.h>
#include "caToolsTypes.h"

/**
 * @brief printOutput - prints metatata and calls printValue();
 * @param i
 * @param args
 * @param arguments
 */
void printOutput(evargs args, arguments_T *arguments);

/**
 * @brief printValue - formats the returned value and prints it - usually called from within printOutput
 * @param args - evargs from callback function
 * @param arguments - pointer to the arguments struct
 */
void printValue(evargs args, arguments_T *arguments);

/**
 * @brief getMetadataFromEvArgs - updates global strings and metadata for the channel
 * @param ch - pointer to struct channel
 * @param args - evargs from callback function
 * @return true if everything goes well
 */
void getMetadataFromEvArgs(struct channel * ch, evargs args);

/**
 * @brief getTimeStamp - calculates the relative timestamp for monitor tool, formats it and saves it into the global string.
 * @param i index of the channel in channel array
 * @param arguments pointer to the arguments struct
 */
void getTimeStamp(size_t i, arguments_T * arguments);

/**
 * @brief cawaitEvaluateCondition evaluates output of channel i against the corresponding wait condition.
 *              returns 1 if matching, 0 otherwise, and -1 if error. Before evaluation, channel output is converted to double. If this is
 *              not successful, the function returns -1. If the channel in question is an array, the condition is evaluated against the first element.
 * @param ch pointer to the channel struct
 * @param args evrargs returned by the read callback function
 * @return true if the condition is fulfilled
 */
bool cawaitEvaluateCondition(struct channel * ch, evargs args, arguments_T * arguments);

/* print integer as binary number */
#define printBits(x) \
    int32_t i; \
    for ( i = sizeof(x) * 8 - 1; i >= 0; i--) { \
        fputc('0' + ((x >> i) & 1), stdout); \
    }


#endif
