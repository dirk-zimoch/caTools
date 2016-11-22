#ifndef caToolsUtils
#define caToolsUtils

#include <stdlib.h>
#include "caToolsTypes.h"

/*  verbosity levels */
#define VERBOSITY_ERR           2
#define VERBOSITY_WARN          3
#define VERBOSITY_ERR_PERIODIC  4
#define VERBOSITY_WARN_PERIODIC 5
#define VERBOSITY_DEBUG		    10

/* print outputs depending on verbosity level */
#define customPrint(level,output,M, ...) if(g_verbosity >= level) fprintf(output, M,##__VA_ARGS__)
#define warnPrint(M, ...) customPrint(VERBOSITY_WARN, stdout, "Warning: "M, ##__VA_ARGS__)
#define warnPeriodicPrint(M, ...) customPrint(VERBOSITY_WARN_PERIODIC, stdout, "Warning: "M, ##__VA_ARGS__)
#define errPrint(M, ...) customPrint(VERBOSITY_ERR, stderr, "Error: "M, ##__VA_ARGS__)
#define errPeriodicPrint(M, ...) customPrint(VERBOSITY_ERR_PERIODIC, stderr, "Error: "M, ##__VA_ARGS__)
#define debugPrint(M, ...) customPrint(VERBOSITY_DEBUG, stderr, "Debug: "M, ##__VA_ARGS__)

#ifndef VERSION_STR
#define VERSION_STR "development build "__DATE__":"__TIME__
#endif

/**
  * @brief dbr_type_to_DBF returns base dbf type from any dbr type
  * @param X dbr type
  */
#define dbr_type_to_DBF(X) X % (LAST_TYPE+1)

/**
 * @brief removePrefix removes prefix from data and returns true if successfull
 * @param data is the data to remove the prefix from
 * @param prefix is the prefix to remove from data
 * @return true if prefix was found and removed, false otherwise
 */
bool removePrefix (char **data, char const *prefix);

/**
 * @brief endsWith checks if src ends with match
 * @param src
 * @param match
 * @return true if src ends with match
 */
bool endsWith(char * src, char * match);

/**
 * @brief getBaseChannelName cuts string *name at the dot character if found
 * @param name
 */
void getBaseChannelName(char *name);

/**
 * @brief truncate - truncates the string to the epics MAX_STRING_SIZE
 * @param argument
 * @return length of the string after truncation
 */
size_t truncate(char *argument);

/**
 * @brief isStrEmpty checks if provided string is empty
 * @param str the string to check
 * @return true if empty, false otherwise
 */
bool isStrEmpty(char *str);

/**
 * @brief isValField
 * @param name - epics channel name
 * @return true if the channel name points to the .VAL field
 */
bool isValField(char *name);

/**
 * @brief timeLessThan - the same as epicsTimeLessThan(), except it only checks if pLeft < pRight
 * @param pLeft
 * @param pRight
 * @return
 */
bool timeLessThan(const epicsTimeStamp *pLeft, const epicsTimeStamp *pRight);

/**
 * @brief epicsTimeDiffFull - Calculates difference between two epicsTimeStamps
 * @param diff
 * @param pLeft
 * @param pRight
 * @return timestamp
 */
bool epicsTimeDiffFull(epicsTimeStamp *diff, const epicsTimeStamp *pLeft, const epicsTimeStamp *pRight);

/**
 * @brief validateTimestamp - checks a timestamp for illegal values and prints warning
 * @param timestamp
 * @param name
 */
void validateTimestamp(epicsTimeStamp *timestamp, const char* name);

/**
 * @brief isPrintable checks if every character before '\0' is printable
 * @param str - string
 * @param n - string length
 * @return true if the string is printable
 */
bool isPrintable(char * str, size_t n);

/**
 * @brief clearStr cleares a string
 * @param str the string to clear
 */
static inline void clearStr(char *str) {
    str[0] = '\0';
}

#endif

