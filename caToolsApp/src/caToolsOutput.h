#ifndef caToolsOutput
#define caToolsOutput

int printOut(struct channel *chan, arguments_T *arguments);
int printValue(evargs args, int32_t precision, arguments_T *arguments);
int printOutput(int i, evargs args, int32_t precision, arguments_T *arguments);


#endif