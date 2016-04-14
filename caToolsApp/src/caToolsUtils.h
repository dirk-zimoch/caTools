#ifndef caToolsUtils
#define caToolsUtils

bool removePrefix (char **data, char const *prefix);
bool endsWith(char * src, char * match);
void getBaseChannelName(char *name);
size_t truncate(char *argument);
bool isStrEmpty(char *str);
bool isValField(char *name);
bool timeLessThan(const epicsTimeStamp *pLeft, const epicsTimeStamp *pRight);
bool epicsTimeDiffFull(epicsTimeStamp *diff, const epicsTimeStamp *pLeft, const epicsTimeStamp *pRight);
void validateTimestamp(epicsTimeStamp *timestamp, const char* name);
/**
 * @brief clearStr cleares a string
 * @param str the string to clear
 */
static inline void clearStr(char *str) {
    str[0] = '\0';
}

#endif

