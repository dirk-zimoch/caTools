#ifndef caToolsUtils
#define caToolsUtils

#include <stdlib.h>
#include "caToolsTypes.h"

#define customPrint(level,output,M, ...) if(g_verbosity >= level) fprintf(output, M,##__VA_ARGS__)
#define warnPrint(M, ...) customPrint(VERBOSITY_WARN, stdout, "Warning: "M, ##__VA_ARGS__)
#define warnPeriodicPrint(M, ...) customPrint(VERBOSITY_WARN_PERIODIC, stdout, "Warning: "M, ##__VA_ARGS__)
#define errPrint(M, ...) customPrint(VERBOSITY_ERR, stderr, "Error: "M, ##__VA_ARGS__)
#define errPeriodicPrint(M, ...) customPrint(VERBOSITY_ERR_PERIODIC, stderr, "Error: "M, ##__VA_ARGS__)
#define debugPrint(M, ...) customPrint(VERBOSITY_DEBUG, stderr, "Debug: "M, ##__VA_ARGS__)

#define dbr_type_to_DBF(X) X % (LAST_TYPE+1)

/* Add some basic docs (what does the function do, what is expected as arguments ... (or add them to the header) */

bool removePrefix (char **data, char const *prefix);
bool endsWith(char * src, char * match);
void getBaseChannelName(char *name);
size_t truncate(char *argument);
bool isStrEmpty(char *str);
bool isValField(char *name);
bool timeLessThan(const epicsTimeStamp *pLeft, const epicsTimeStamp *pRight);
bool epicsTimeDiffFull(epicsTimeStamp *diff, const epicsTimeStamp *pLeft, const epicsTimeStamp *pRight);
void validateTimestamp(epicsTimeStamp *timestamp, const char* name);
bool isPrintable(char * str, size_t n);
bool getMetadataFromEvArgs(struct channel * ch, evargs args);
int getBaseType(int dbfType);

/**
 * @brief clearStr cleares a string
 * @param str the string to clear
 */
static inline void clearStr(char *str) {
    str[0] = '\0';
}

#endif

