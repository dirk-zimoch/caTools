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

#endif
