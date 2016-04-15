#ifndef caToolsOutput
#define caToolsOutput

#include <stdint.h>
#include "caToolsTypes.h"

//Its generally a good idea to have a standalone header file, meaning that the header file
// can be parsed regardless of where it was included from.


//Add docs
int printValue(evargs args, int32_t precision, arguments_T *arguments);
int printOutput(int i, evargs args, int32_t precision, arguments_T *arguments);


#endif
