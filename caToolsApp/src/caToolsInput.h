#ifndef caToolsInput
#define caToolsInput

/* Add correct includes */

void usage(FILE *stream, enum tool tool, char *programName);
int parseArguments(int argc, char ** argv, u_int32_t *nChannels, arguments_T *arguments);
bool parseChannels(int argc, char ** argv, u_int32_t nChannels,  arguments_T *arguments, struct channel *channels);
bool cawaitParseCondition(struct channel *channel, char *str);
bool castStrToDBR(void ** data, char **str, unsigned long nelm, int32_t baseType);
bool parseAsArray(struct channel * ch, arguments_T * arguments);

#endif
