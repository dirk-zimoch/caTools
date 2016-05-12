#ifndef caToolsInput
#define caToolsInput

#include <stdio.h>
#include "caToolsTypes.h"

/**
 * @brief usage prints the usage of the tools
 * @param stream is the output stream for printing
 * @param tool is the selected tool
 * @param programName is the command line name of the program
 */
void usage(FILE *stream, enum tool tool, char *programName);

/**
 * @brief parseArguments - parses command line arguments and fills up the arguments struct
 * @param argc from main
 * @param argv from main
 * @param nChannels pointer to the variable holding the number of channels
 * @param arguments pointer to the variable holding the arguments
 * @return true if everything went ok
 */
bool parseArguments(int argc, char ** argv, u_int32_t *nChannels, arguments_T *arguments);

/**
 * @brief parseChannels - allocates global array of channel structs and fills it up
 * @param argc from main
 * @param argv from main
 * @param nChannels number of channels obtained from parseArguments()
 * @param arguments - arguments struct
 * @param channels - pointer to the variable holding the channels array
 * @return true if everything went ok
 */
bool parseChannels(int argc, char ** argv, u_int32_t nChannels,  arguments_T *arguments, struct channel *channels);

/**
 * @brief cawaitParseCondition parses input string and extracts operators and numerals, saves them to the channel structure
 * @param channel pointer to the channel structure
 * @return true if everything went ok
 */
bool cawaitParseCondition(struct channel *channel, char *str);

/**
 * @brief castStrToDBR convert command line argument string to epics DBR type
 * @param data pointer to the data pointer
 * @param ch pointer to the channel struct
 * @param arguments - arguments struct
 * @return true if everything went ok
 */
bool castStrToDBR(void ** data, struct channel * ch, short * pBaseType, arguments_T * arguments);

#endif
