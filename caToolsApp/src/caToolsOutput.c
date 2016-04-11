#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <math.h>

#include "caToolsTypes.h"
#include "caToolsOutput.h"

int printOut(struct channel *chan, arguments_T *arguments){
     printf("New Printout Function!");
     printf("\nTool:%i", arguments->tool);
     printf("\nChannel:%s\n", chan->base.name);
     return 0;
}

#define printBits(x) \
    for (int32_t i = sizeof(x) * 8 - 1; i >= 0; i--) { \
        fputc('0' + ((x >> i) & 1), stdout); \
    }

/**
 * @brief isStrEmpty checks if provided string is empty
 * @param str the string to check
 * @return true if empty, false otherwise
 */
static bool isStrEmpty(char *str) {
    return str[0] == '\0';
}

int printValue(evargs args, int32_t precision, arguments_T *arguments){
//Parses the data fetched by ca_get callback according to the data type and formatting arguments->
//The result is printed to stdout.

    int32_t baseType;
    void *value;
    double valueDbl = 0;

    value = dbr_value_ptr(args.dbr, args.type);
    baseType = args.type % (LAST_TYPE+1);   // convert appropriate TIME, GR, CTRL,... type to basic one

    //loop over the whole array

    long j; //element counter

    for (j=0; j<args.count; ++j){

        if (j){
            printf ("%c", arguments->fieldSeparator); //insert element separator
        }

        switch (baseType) {
        case DBR_STRING:
            printf("\"%.*s\"", MAX_STRING_SIZE, ((dbr_string_t*) value)[j]);    // TODO: is this always null-terminated?
            break;
        case DBR_FLOAT:
        case DBR_DOUBLE:{
            if (baseType == DBR_FLOAT) valueDbl = ((dbr_float_t*)value)[j];
            else valueDbl = ((dbr_double_t*)value)[j];

            //round if desired or will be writen as hex oct, bin or string
            if (arguments->round == roundType_round || arguments->hex || arguments->oct || arguments->bin || arguments->str) {
                valueDbl = round(valueDbl);
            }
            else if(arguments->round == roundType_ceil) {
                    valueDbl = ceil(valueDbl);
            }
            else if (arguments->round == roundType_floor) {
                valueDbl = floor(valueDbl);
            }

            if (!(arguments->hex || arguments->oct || arguments->bin)) { // if hex, oct or bin, do not brake but use DBR_LONG code to print out the value
                char format = 'f'; // default write as float
                if (arguments->dblFormatType != '\0') {
                    //override records default prec
                    format = arguments->dblFormatType;
                }
                if (arguments->prec >= 0) {
                    //use precision and format as specified by -efg or -prec arguments
                    precision = arguments->prec;
                }

                if (precision != -1 && !arguments->plain) { // when DBR_XXX_DOUBLE, DBR_XXX_FLOAT has precision info. Also, print raw when -plain is used.
                    char formatStr[30];
                    sprintf(formatStr, "%%-.%"PRId32"%c", precision, format);
                    printf(formatStr, valueDbl);
                }
                else { // case where no precision info is avaliable
                    printf("%-f", valueDbl);
                }
                break;
            }
        }
        case DBR_LONG:{
            int32_t valueInt32;
            if (baseType == DBR_FLOAT || baseType == DBR_DOUBLE) valueInt32 = (int32_t)valueDbl;
            else valueInt32 = ((dbr_long_t*)value)[j];

            //display dec, hex, bin, oct if desired
            if (arguments->hex){
                printf("0x%" PRIx32, valueInt32);
            }
            else if (arguments->oct){
                printf("0o%" PRIo32, valueInt32);
            }
            else if (arguments->bin){
                printf("0b");
                printBits(valueInt32);
            }
            else if (arguments->str){
                printf("%c", (u_int8_t)valueInt32);
            }
            else{
                printf("%" PRId32, valueInt32);
            }
            break;
        }
        case DBR_INT:{
            int16_t valueInt16 = ((dbr_int_t*)value)[j];

            //display dec, hex, bin, oct, char if desired
            if (arguments->hex){
                printf("0x%" PRIx16, (u_int16_t)valueInt16);
            }
            else if (arguments->oct){
                printf("0o%" PRIo16, (u_int16_t)valueInt16);
            }
            else if (arguments->bin){
                printf("0b");
                printBits((u_int16_t)valueInt16);
            }
            else if (arguments->str){
                printf("%c", (u_int8_t)valueInt16);
            }
            else{
                printf("%" PRId16, valueInt16);
            }
            break;
        }
        case DBR_ENUM: {
            dbr_enum_t v = ((dbr_enum_t *)value)[j];
            if (v >= MAX_ENUM_STATES){
                printf("Illegal enum index: %d", v);
                break;
            }

            if (!arguments->num && !arguments->bin && !arguments->hex && !arguments->oct) { // if not requested as a number check if we can get string


                if (dbr_type_is_GR(args.type)) {
                    if (v >= ((struct dbr_gr_enum *)args.dbr)->no_str) {
                        printf("Enum index value %d greater than the number of strings", v);
                    }
                    else{
                        printf("\"%.*s\"", MAX_ENUM_STRING_SIZE, ((struct dbr_gr_enum *)args.dbr)->strs[v]);
                    }
                    break;
                }
                else if (dbr_type_is_CTRL(args.type)) {
                    if (v >= ((struct dbr_ctrl_enum *)args.dbr)->no_str) {
                        printf("Enum index value %d greater than the number of strings", v);
                    }
                    else{
                        printf("\"%.*s\"", MAX_ENUM_STRING_SIZE, ((struct dbr_ctrl_enum *)args.dbr)->strs[v]);
                    }
                    break;
                }
            }
            // else write value as number.
            if (arguments->hex){
                printf("0x%" PRIx16, v);
            }
            else if (arguments->oct){
                printf("0o%" PRIo16, v);
            }
            else if (arguments->bin){
                printf("0b");
                printBits(v);
            }
            else{
                printf("%" PRId16, v);
            }
            break;
        }
        case DBR_CHAR:{
            if (arguments->num) {    //check if requested as a a number
                printf("%" PRIu8, ((u_int8_t*) value)[j]);
            }
            else if (arguments->hex){
                printf("0x%" PRIx8, ((u_int8_t*) value)[j]);
            }
            else if (arguments->oct){
                printf("0o%" PRIo8, ((u_int8_t*) value)[j]);
            }
            else if (arguments->bin){
                printf("0b");
                printBits(((u_int8_t*) value)[j]);
            }
            else{
                fputc(((char*) value)[j], stdout);
            }
            break;
        }

        default:
            errPeriodicPrint("Unrecognized DBR type.\n");
            break;
        }

    }

    return 0;
}


int printOutput(int i, evargs args, int32_t precision, arguments_T *arguments){
// prints global output strings corresponding to i-th channel.

    //if both local and server times are requested, clarify which is which
    bool doubleTime = (arguments->localdate || arguments->localtime) && (arguments->date || arguments->time);
    if (doubleTime){
        fputs("server time: ",stdout);
    }

    //server date
    if (!isStrEmpty(outDate[i]))    printf("%s ",outDate[i]);
    //server time
    if (!isStrEmpty(outTime[i]))    printf("%s ",outTime[i]);

    if (doubleTime){
        fputs("local time: ",stdout);
    }

    //local date
    if (!isStrEmpty(outLocalDate[i]))   printf("%s ",outLocalDate[i]);
    //local time
    if (!isStrEmpty(outLocalTime[i]))   printf("%s ",outLocalTime[i]);


    //timestamp if monitor and if requested
    if ((arguments->tool == camon || arguments->tool == cainfo) && arguments->timestamp)   printf("%s ", outTimestamp[i]);

    //channel name
    if (!arguments->noname)  printf("%s ",((struct channel *)args.usr)->base.name);

    if (arguments->nord) printf("%lu ", args.count); // show nord if requested

    //value(s)
    printValue(args, precision, arguments);


    fputc(' ',stdout);

    //egu
    if (!isStrEmpty(outUnits[i]) && !arguments->nounit) printf("%s ",outUnits[i]);

    //severity
    if (!isStrEmpty(outSev[i])) printf("(%s",outSev[i]);

    //status
    if (!isStrEmpty(outStat[i])) printf(" %s)",outStat[i]);
    else if (!isStrEmpty(outSev[i])) putc(')', stdout);

    putc('\n', stdout);
    return 0;
}
