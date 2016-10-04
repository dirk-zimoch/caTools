#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include "epicsString.h"
#include "cantProceed.h"
#include "caToolsTypes.h"
#include "caToolsUtils.h"
#include "caToolsInput.h"


/**
 * @brief usage prints the usage of the tools
 * @param stream is the output stream for printing
 * @param tool is the selected tool
 * @param programName is the command line name of the program
 */
void usage(FILE *stream, enum tool tool, char *programName){

    /* usage: */
    if (tool == caget || tool == cagets || tool == camon || tool == cado) {
        fprintf(stream, "\nUsage: %s [flags] <pv> [<pv> ...]\n", programName);
    }
    else if (tool == cawait) {
        fprintf(stream, "\nUsage: %s [flags] <pv> <condition> [<pv> <condition> ...]\n", programName);
    }
    else if (tool == caput || tool == caputq  ) {
        fprintf(stream, "\nUsage: %s [flags] <pv> <value> [<pv> <value> ...]\n", programName);
    }
    else if (tool == cainfo  ) {
        fprintf(stream, "\nUsage: %s [flags] <pv> [<pv> ...]\n", programName);
    }
    else { /* tool unknown */
        fprintf(stream, "\nUnknown tool. Try %s -tool with any of the following arguments: caget, caput, cagets, "\
                "caputq, cado, camon, cawait, cainfo.\n", programName);
        return;
    }

    /* descriptions */
    fputs("\n",stream);

    if (tool == caget) {
        fputs("Reads PV values(s).\n", stream);
    }
    if (tool == cagets) {
        fputs("First processes the PV(s), then reads the value(s).\n", stream);
    }
    if (tool == caput || tool == caputq) {
        if (tool == caput) {
            fputs("Writes value(s) to the PV(s), waits until processing finishes and returns updated value(s).\n", stream);
        }
        else { /* caputq */
            fputs("Writes value(s) to PV(s), but does not wait for the processing to finish. Does not have any output (except if an error occurs).\n", stream);
        }
        fputs("Array handling:\n", stream);
        fputs(  "- When -a option is set, input separator (-inSep argument) is used to parse elements in an array.\n"\
                "- When input separator (-inSep argument) is explicitly defined, -a option is automatically used."
                " See following examples which produce the same result,"\
                " namely write 1, 2 and 3 into record pvA and 4, 5, 6 into pvB.\n Let records pvA and pvB be waveforms of chars:\n", stream);
        fputs("  caput pvA 'abc' pvB 'def'\n", stream);
        fputs("  caput -a pvA 'aa bb cc' pvB 'dd ee ff'\n", stream);
        fputs("  caput -inSep ';' pvA 'a;b;c' pvB 'a;b;c'\n", stream);
    }
    if (tool == cado) {
        fputs("Writes 1 to PROC field, does not wait for the processing to finish. Does not have any output (except if an error occurs).\n", stream);
    }
    if (tool == camon) {
        fputs("Monitors the PV(s).\n", stream);
    }
    if (tool == cawait) {
         fputs("Monitors the PV(s), but only displays the values when they match the provided conditions. When at least one of the conditions is true, the program exits." \
               "The conditions are specified as a string containing the operator together with the values.\n", stream);
        fputs("Following operators are supported:  >, <, <=, >=, ==, !=, !, ==A...B(in interval), !=A...B or !A...B (out of interval). For example, "\
                "cawait pv '==1...5' exits after pv value is inside the interval [1,5]."
              "For string values only ==, != and ! operators are supported\n", stream);
    }
    if (tool == cainfo) {
        fputs("Displays detailed information about the provided records.\n", stream);
    }

    /* flags common for all tools */
    fputs("\n", stream);
    fputs("Accepted flags:\n", stream);
    fputs("\n", stream);
    fputs("  -h, -?, -help        Help: Print this message\n", stream);
    fputs("  -v                   Verbosity. Options:\n", stream);
    fprintf(stream,"                       Print error messages: %d\n", VERBOSITY_ERR);
    fprintf(stream,"                       Also print warning messages: %d (default)\n", VERBOSITY_WARN);
    fprintf(stream,"                       Also print error messages that occur periodically: %d\n", VERBOSITY_ERR_PERIODIC);
    fprintf(stream,"                       Also print warning messages that occur periodically: %d\n\n", VERBOSITY_WARN_PERIODIC);

    /* flags common for most of the tools */
    if (tool != cado){
        fputs("Channel Access options\n", stream);
        fprintf(stream, "  -w <time>            Wait time in seconds, specifies CA timeout. Value 0 means wait forever.\n"
                        "                       (default: %d s)\n", CA_DEFAULT_TIMEOUT);
        if (tool != cainfo) {
            fputs("  -dbrtype <type>      Type of DBR request to use for retrieving values\n"
                  "                       from the server. Use string (DBR_ prefix may be\n"
                  "                       omitted, or number of one of the following types:\n", stream);
            fputs("             DBR_STRING     0  DBR_STS_LONG    12  DBR_GR_ENUM       24\n"
                  "             DBR_INT        1  DBR_STS_DOUBLE  13  DBR_GR_CHAR       25\n"
                  "             DBR_SHORT      1  DBR_TIME_STRING 14  DBR_GR_LONG       26\n"
                  "             DBR_FLOAT      2  DBR_TIME_INT    15  DBR_GR_DOUBLE     27\n"
                  "             DBR_ENUM       3  DBR_TIME_SHORT  15  DBR_CTRL_STRING   28\n"
                  "             DBR_CHAR       4  DBR_TIME_FLOAT  16  DBR_CTRL_SHORT    29\n"
                  "             DBR_LONG       5  DBR_TIME_ENUM   17  DBR_CTRL_INT      29\n"
                  "             DBR_DOUBLE     6  DBR_TIME_CHAR   18  DBR_CTRL_FLOAT    30\n"
                  "             DBR_STS_STRING 7  DBR_TIME_LONG   19  DBR_CTRL_ENUM     31\n"
                  "             DBR_STS_SHORT  8  DBR_TIME_DOUBLE 20  DBR_CTRL_CHAR     32\n"
                  "             DBR_STS_INT    8  DBR_GR_STRING   21  DBR_CTRL_LONG     33\n"
                  "             DBR_STS_FLOAT  9  DBR_GR_SHORT    22  DBR_CTRL_DOUBLE   34\n"
                  "             DBR_STS_ENUM  10  DBR_GR_INT      22  DBR_STSACK_STRING 37\n"
                  "             DBR_STS_CHAR  11  DBR_GR_FLOAT    23  DBR_CLASS_NAME    38\n\n", stream);
        }

    }

    /* flags associated with writing */
    if (tool == caput || tool == caputq) {
        fputs("Parsing input : Array format options \n", stream);
        fputs("  -a                   Force parsing as array.\n", stream);
        fputs("  -inSep <separator>   Separator used between array elements in <value>.\n", stream);
        fputs("                       If not specified, space is used.\n", stream);
        fputs("                       If specified, '-a' option is automatically used. \n", stream);
    }
    if (tool == caput || tool == caputq ||tool == cawait) {
        fputs("Parsing input : Other format specifiers \n", stream);
        fputs("  -num                 Interpret value(s) as numbers\n", stream);
        fputs("  -s                   Interpret value(s) as string or chars.\n", stream);
        if(tool != cawait){
            fputs("  -bin                 Sets the numeric base for integers to 2.\n"
                  "                       Binary input without any prefix or suffix.\n", stream);
            fputs("  -hex                 Sets the numeric base for integers to 16.\n"
                  "                       Hexadcimal input, 0x, or 0X prefix is optional\n", stream);
            fputs("  -oct                 Sets the numeric base for integers to \n"
                  "                       Octal input without any prefix or suffix.\n", stream);
        }
    }

    /*  flags associated with monitoring */
    if (tool == camon) {
        fputs("Monitoring options\n", stream);
        fputs("  -n <number>          Exit the program after <number> updates.\n", stream);
        fputs("  -timestamp <option>  Display relative timestamps. Options:\n", stream);
        fputs("                            r: server timestamp relative to the local time when the program started,\n", stream);
        fputs("                            u: time elapsed since last update of any PV,\n", stream);
        fputs("                            c: time elapsed since last update separately for each PV.\n", stream);
    }
    if (tool == cawait || tool == camon) {
        fputs("Timeout options\n", stream);
        fputs("  -timeout <number>    Exit the program after <number> seconds.\n", stream);
    }
    else {
        fputs("Loop options\n", stream);
        fputs("  -period <seconds>    Repeat periodically.\n", stream);
        fputs("  -sep <string>        Print separator string beween loops.\n", stream);
        fputs("  -n <number>          Exit after <number> loops.\n", stream);
    }

    /* Flags associated with returned values. Used by both reading and writing tools. */
    if (tool == caget || tool == cagets || tool == camon
            || tool == cawait ||  tool == caput) {
        fputs("Formating output : General options\n", stream);
        fputs("  -noname              Hide PV name.\n", stream);
        fputs("  -nostat              Never display alarm status and severity.\n", stream);
        fputs("  -nounit              Never display units.\n", stream);
        fputs("  -stat                Always display alarm status and severity.\n", stream);
        fputs("  -plain               Ignore all formatting switches (displays\n"\
              "                       only PV value) except date/time related.\n", stream);

        fputs("Formating output : Time options\n", stream);
        fputs("  -d -date             Display server date.\n", stream);
        fputs("  -localdate           Display client date.\n", stream);
        fputs("  -localtime           Display client time.\n", stream);
        fputs("  -t, -time            Display server time.\n", stream);
        fputs("  -tfmt <timeformat>   Display server time with format\n", stream);
        fputs("  -ltfmt <timeformat>  Display client time with format\n", stream);

        fputs("Formating output : Integer format options\n", stream);
        fputs("  -bin                 Display integer values in binary format.\n", stream);
        fputs("  -hex                 Display integer values in hexadecimal format.\n", stream);
        fputs("  -oct                 Display integer values in octal format.\n", stream);
        fputs("  -int, -num           Display enum/char values as numbers.\n", stream);
        fputs("  -s                   Interpret value(s) as char (number to ascii).\n", stream);

        fputs("Formating output : Floating point format options\n", stream);
        fputs("  -e <number>          Format doubles using scientific notation with\n" \
              "                       precision <number>. Overrides -prec option.\n", stream);
        fputs("  -f <number>          Format doubles using floating point with precision\n"\
              "                       <number>. Overrides -prec option.\n", stream);
        fputs("  -g <number>          Use %g format, printing <number> of most significant digits.\n"\
              "                       %g formats doubles using shorter representation of -e or -f.\n"\
              "                       Overrides -prec option.\n", stream);
        fputs("  -prec <number>       Override PREC field with <number>.\n"\
              "                       (default: PREC field).\n", stream);
        fputs("  -round <option>      Round floating point value(s). Options:\n", stream);
        fputs("                                 round: round to nearest.\n", stream);
        fputs("                                 ceil: round up,\n", stream);
        fputs("                                 floor: round down,\n", stream);

        fputs("Formating output : Array format options\n", stream);
        fputs("  -nord                Display number of array elements before their values.\n", stream);
        fputs("  -outNelm <number>    Number of array elements to read.\n", stream);
        fputs("  -outSep <number>     Separator between array elements.\n", stream);
    }
}

static char* translatePercentN(char* s)
{ /* translate %N to %f */
    char* p;
    for (p=s; (p=strchr(p, '%')); p++)
        if (*++p == 'N') *p = 'f';
    return s;
}

/**
 * @brief parseArguments - parses command line arguments and fills up the arguments variable
 * @param argc from main
 * @param argv from main
 * @param nChannels pointer to the variable holding the number of channels
 * @param arguments pointer to the variable holding the arguments
 * @return success
 */
bool parseArguments(int argc, char ** argv, u_int32_t *nChannels, arguments_T *arguments){
    int opt;                    /*  getopt() current option */
    int opt_long;               /*  getopt_long() current long option */

    if (endsWith(argv[0],"caget")) arguments->tool = caget;
    if (endsWith(argv[0],"caput")) arguments->tool = caput;
    if (endsWith(argv[0],"cagets")) arguments->tool = cagets;
    if (endsWith(argv[0],"caputq")) arguments->tool = caputq;
    if (endsWith(argv[0],"camon")) arguments->tool = camon;
    if (endsWith(argv[0],"cawait")) arguments->tool = cawait;
    if (endsWith(argv[0],"cado")) arguments->tool = cado;
    if (endsWith(argv[0],"cainfo")) arguments->tool = cainfo;

    static struct option long_options[] = {
        {"num",         no_argument,        0,  0 },
        {"int",         no_argument,        0,  0 },    /* same as num */
        {"round",       required_argument,  0,  0 },    /* type of rounding:round, ceil, floor */
        {"prec",        required_argument,  0,  0 },    /* precision */
        {"hex",         no_argument,        0,  0 },    /* display as hex */
        {"bin",         no_argument,        0,  0 },    /* display as bin */
        {"oct",         no_argument,        0,  0 },    /* display as oct */
        {"plain",       no_argument,        0,  0 },    /* ignore formatting switches */
        {"stat",        no_argument,        0,  0 },    /* status and severity on */
        {"nostat",      no_argument,        0,  0 },    /* status and severity off */
        {"noname",      no_argument,        0,  0 },    /* hide name */
        {"nounit",      no_argument,        0,  0 },    /* hide units */
        {"timestamp",   required_argument,  0,  0 },    /* timestamp type r,u,c */
        {"localdate",   no_argument,        0,  0 },    /* client date */
        {"time",        no_argument,        0,  0 },    /* server time */
        {"localtime",   no_argument,        0,  0 },    /* client time */
        {"date",        no_argument,        0,  0 },    /* server date */
        {"outNelm",     required_argument,  0,  0 },    /* number of array elements - read */
        {"outSep",      required_argument,  0,  0 },    /* array field separator - read */
        {"inSep",       required_argument,  0,  0 },    /* array field separator - write */
        {"nord",        no_argument,        0,  0 },    /* display number of array elements */
        {"tool",        required_argument,  0,  0 },    /* tool */
        {"timeout",     required_argument,  0,  0 },    /* timeout */
        {"dbrtype",     required_argument,  0,  0 },    /* dbrtype */
        {"period",      required_argument,  0,  0 },    /* periodic execution */
        {"sep",         required_argument,  0,  0 },    /* seperator between periodic runs */
        {"tfmt",        required_argument,  0,  0 },    /* time stamp format */
        {"ltfmt",       required_argument,  0,  0 },    /* local time format */
        {0,             0,                  0,  0 }
    };
    putenv("POSIXLY_CORRECT="); /* Behave correctly on GNU getopt systems = stop parsing after 1st non option is encountered */

    while ((opt = getopt_long_only(argc, argv, "w:e:f:g:n:sthdav:", long_options, &opt_long)) != -1) {
        switch (opt) {
        case 'w':
            if (sscanf(optarg, "%lf", &arguments->caTimeout) != 1){    /*  type was not given as float */
                arguments->caTimeout = CA_DEFAULT_TIMEOUT;
                warnPrint("Requested CA timeout invalid - ignored. ('%s -h' for help.)\n", argv[0]);
            }
            if(arguments->caTimeout < 0) {
                warnPrint("CA timeout must be greater or equal to zero - ignored. ('%s -h' for help.)\n", argv[0]);
            }
            break;
        case 'd': /* same as date */
            arguments->date = true;
            break;
        case 'e':    /* how to format doubles in strings */
        case 'f':
        case 'g':
            if (sscanf(optarg, "%"SCNd32, &arguments->prec) != 1){
                warnPrint("Invalid precision argument '%s' "
                        "for option '-%c' - ignored.\n", optarg, opt);
            } else {
                if (arguments->prec>=0){
                    arguments->dblFormatType = opt; /* set 'e' 'f' or 'g' */
                }
                else{
                    warnPrint("Precision %d for option '-%c' "
                            "out of range. Should be positive integer. - ignored.\n", arguments->prec, opt);
                }
            }
            break;
        case 's':    /* try to interpret values as strings */
            arguments->str = true;
            break;
        case 't':    /* same as time */
            arguments->time = true;
            break;
        case 'n':    /* stop monitor after numUpdates */
            if (sscanf(optarg, "%"SCNd64, &arguments->numUpdates) != 1) {
                warnPrint("Invalid argument '%s' for option '-%c' - ignored.\n", optarg, opt);
                arguments->numUpdates = -1;
            }
            else {
                if (arguments->numUpdates < 0) {
                    warnPrint("Number of updates for option '-%c' must be greater or equal to zero - ignored.\n", opt);
                    arguments->numUpdates = -1;
                }
            }
            break;
        case 'a':
            arguments->parseArray = true;
            break;
        case 'v':
            if (sscanf(optarg, "%"SCNu16, &arguments->verbosity) != 1) {
                warnPrint("Invalid argument '%s' for option '-%c' - ignored.\n", optarg, opt);
            }else{
                g_verbosity = arguments->verbosity;
            }

            break;
        case 0:   /* long options */
            switch (opt_long) {
            case 0:   /* num */
                arguments->num = true;
                break;
            case 1:   /* int */
                arguments->num = true;
                break;
            case 2:   /* round */
                ;/* declaration must not follow label */
                int type;
                if (sscanf(optarg, "%d", &type) != 1) {   /*  type was not given as a number [0, 1, 2] */
                    if(!strcmp("round", optarg)) {
                        arguments->round = roundType_round;
                    } else if(!strcmp("ceil", optarg)) {
                        arguments->round = roundType_ceil;
                    } else if(!strcmp("floor", optarg)) {
                        arguments->round = roundType_floor;
                    } else {
                        arguments->round = roundType_round;
                        warnPrint("Invalid round type '%s' "
                            "for option '%s' - ignored.\n", optarg, long_options[opt_long].name);
                    }
                } else { /* type was given as a number */
                    if( type < roundType_round || roundType_floor < type) {   /*  out of range check */
                        arguments->round = roundType_no_rounding;
                        warnPrint("Invalid round type '%s' "
                            "for option '%s' - ignored.\n", optarg, long_options[opt_long].name);
                    } else{
                        arguments->round = type;
                    }
                }
                break;
            case 3:   /* prec */
                if (arguments->dblFormatType == '\0') { /* formating with -f -g -e has priority */
                    if (sscanf(optarg, "%"SCNd32, &arguments->prec) != 1){
                        warnPrint("Invalid precision argument '%s' "
                                "for option '%s' - ignored.\n", optarg, long_options[opt_long].name);
                    } else{
                       /* set format type depending on sign, exponential for negative, normal for positive argument */
                       if (arguments->prec < 0) arguments->dblFormatType = 'e';
                       else arguments->dblFormatType = 'f';
                       arguments->prec = abs(arguments->prec);
                    }
                }
                else {
                    warnPrint("Option '-prec' ignored because of '-f', '-e' or '-g'.\n");
                }
                break;
            case 4:   /* hex */
                arguments->hex = true;
                arguments->num = true;
                break;
            case 5:   /* bin */
                arguments->bin = true;
                arguments->num = true;
                break;
            case 6:   /* oct */
                arguments->oct = true;
                arguments->num = true;
                break;
            case 7:   /* plain */
                arguments->plain = true;
                break;
            case 8:   /* stat */
                arguments->stat = true;
                break;
            case 9:   /* nostat */
                arguments->nostat = true;
                break;
            case 10:   /* noname */
                arguments->noname = true;
                break;
            case 11:   /* nounit */
                arguments->nounit = true;
                break;
            case 12:   /* timestamp */
                if (sscanf(optarg, "%c", &arguments->timestamp) != 1){
                   warnPrint("Invalid argument '%s' "
                            "for option '%s' - ignored. Allowed arguments: r,u,c.\n", optarg, long_options[opt_long].name);
                    arguments->timestamp = 0;
                } else {
                    if (arguments->timestamp != 'r' && arguments->timestamp != 'u' && arguments->timestamp != 'c') {
                        warnPrint("Invalid argument '%s' "
                            "for option '%s' - ignored. Allowed arguments: r,u,c.\n", optarg, long_options[opt_long].name);
                        arguments->timestamp = 0;
                    }
                }
                break;
            case 13:   /* localdate */
                arguments->localdate = true;
                break;
            case 14:   /* time */
                arguments->time = true;
                break;
            case 15:   /* localtime */
                arguments->localtime = true;
                break;
            case 16:   /* date */
                arguments->date = true;
                break;
            case 17:   /* outx - number of elements - read */
                if (sscanf(optarg, "%"SCNd64, &arguments->outNelm) != 1){
                    warnPrint("Invalid count argument '%s' "
                            "for option '%s' - ignored.\n", optarg, long_options[opt_long].name);
                }
                else {
                    if (arguments->outNelm < 1) {
                        warnPrint("Count number for option '%s' "
                                "must be positive integer - ignored.\n", long_options[opt_long].name);
                        arguments->outNelm = 0;
                    }
                }
                break;
            case 18:   /* field separator for output */
                if (sscanf(optarg, "%c", &arguments->fieldSeparator) != 1){
                    warnPrint("Invalid argument '%s' "
                            "for option '%s' - ignored.\n", optarg, long_options[opt_long].name);
                }
                if(arguments->fieldSeparator == ' ') arguments->fieldSeparator = 0;
                break;
            case 19:   /* field separator for input */
                if (sscanf(optarg, "%c", &arguments->inputSeparator) != 1){
                    warnPrint("Invalid argument '%s' "
                            "for option '%s' - ignored.\n", optarg, long_options[opt_long].name);
                }
                else {
                    arguments->parseArray = true;
                }
                break;
            case 20:   /*  nord */
                arguments->nord = true;
                break;
            case 21:   /*  tool */
                ;/* declaration must not follow label */
                int tool;
                if (sscanf(optarg, "%d", &tool) != 1){   /*  type was not given as a number [0, 1, 2] */
                    if(!strcmp("caget", optarg)){
                        arguments->tool = caget;
                    } else if(!strcmp("camon", optarg)){
                        arguments->tool = camon;
                    } else if(!strcmp("caput", optarg)){
                        arguments->tool = caput;
                    } else if(!strcmp("caputq", optarg)){
                        arguments->tool = caputq;
                    } else if(!strcmp("cagets", optarg)){
                        arguments->tool = cagets;
                    } else if(!strcmp("cado", optarg)){
                        arguments->tool = cado;
                    } else if(!strcmp("cawait", optarg)){
                        arguments->tool = cawait;
                    } else if(!strcmp("cainfo", optarg)){
                        arguments->tool = cainfo;
                    }
                } else{ /*  type was given as a number */
                    if(tool >= caget|| tool <= cainfo){   /* unknown tool case handled down below */
                        arguments->tool = tool_unknown;
                    }
                }
                break;
            case 22:   /* timeout */
                if (sscanf(optarg, "%lf", &arguments->timeout) != 1){
                    warnPrint("Invalid timeout argument '%s' "
                            "for option '%s' - ignored.\n", optarg, long_options[opt_long].name);
                }
                else {
                    if (arguments->timeout <= 0) {
                        warnPrint("Timeout argument must be positive - ignored.\n");
                        arguments->timeout = -1;
                    }
                }
                break;
            case 23:   /* dbrtype */
                if (sscanf(optarg, "%"SCNd32, &arguments->dbrRequestType) != 1)     /*  type was not given as an integer */
                {
                    dbr_text_to_type(optarg, arguments->dbrRequestType);
                    if (arguments->dbrRequestType == -1)                   /*  Invalid? Try prefix DBR_ */
                    {
                        char str[30] = "DBR_";
                        strncat(str, optarg, 25);
                        dbr_text_to_type(str, arguments->dbrRequestType);
                    }
                }
                if (arguments->dbrRequestType < DBR_STRING || arguments->dbrRequestType > DBR_CTRL_DOUBLE) {
                    warnPrint("Requested dbr type out of range "
                            "or invalid - ignored. ('%s -h' for help.)\n", argv[0]);
                    arguments->dbrRequestType = -1;
                }

                break;
            case 24:   /* period */
                if (sscanf(optarg, "%lf", &arguments->period) != 1)
                {
                    warnPrint("Invalid argument '%s' "
                            "for option '%s' - ignored.\n", optarg, long_options[opt_long].name);
                }
                break;
            case 25:   /* sep */
                arguments->separator = epicsStrDup(optarg);
                break;
            case 26:   /* tfmt */
                arguments->tfmt = translatePercentN(epicsStrDup(optarg));
                break;
            case 27:   /* ltfmt */
                arguments->ltfmt = translatePercentN(epicsStrDup(optarg));
                break;
            }
            break;
        case '?':
        case 'h':               /* Print usage */
            usage(stdout, arguments->tool, argv[0]);
            exit(EXIT_SUCCESS); /* do not return here, exit strainght away */
            /* If we just return from the function, main program will not know that only help was printed
             * and that now it hsould exit. Since it only sees success, it will still run camon/cawait loops, etc... */
        default:
            usage(stderr, arguments->tool, argv[0]);
            return false;
        }
     }

     /* check mutually exclusive arguments (without taking dbr type into account) */
     if (arguments->tool == tool_unknown){
         usage(stderr, arguments->tool,argv[0]);
         return false;
     }
     if(arguments->tool == cainfo && (arguments->str || arguments->num || arguments->bin  || arguments->hex || arguments->oct \
             || arguments->dbrRequestType != -1 || arguments->round != roundType_no_rounding || arguments->plain \
             || arguments->dblFormatType != '\0' || arguments->stat || arguments->nostat || arguments->noname || arguments->nounit \
             || arguments->timestamp != '\0' || arguments->timeout != -1 || arguments->date || arguments->time || arguments->localdate \
             || arguments->localtime || arguments->fieldSeparator != 0 || arguments->inputSeparator != ' ' || arguments->numUpdates != -1\
             || arguments->parseArray || arguments->outNelm > 0 || arguments->nord || arguments->stat || arguments->nostat\
             || arguments->noname || arguments->nounit)){
         /* arguments->prec is not checked here, since checking arguments->dblFormatType is also set when arguments->prec is set. */
         warnPrint("The only options allowed for cainfo are -w and -v. Ignoring the rest.\n");
         arguments->dbrRequestType = -1;
     }
     else if (arguments->tool != camon){
         if (arguments->timestamp != 0){
             warnPrint("Option -timestamp can only be specified with camon.\n");
         }
     }
     else if ((arguments->parseArray || arguments->inputSeparator !=' ') && (arguments->tool != caput && arguments->tool != caputq)){
         warnPrint("Option -a and -inSep can only be specified with caput or caputq.\n");
     }
     if ((arguments->outNelm > 0 || arguments->num !=false || arguments->hex !=false || arguments->bin !=false || arguments->oct !=false)\
             && (arguments->tool == cado || arguments->tool == caputq)){
         warnPrint("Option -outNelm, -num, -hex, -bin and -oct cannot be specified with cado or caputq, because they have no output.\n");
     }
     if (arguments->nostat != false && arguments->stat != false){
         warnPrint("Options -stat and -nostat are mutually exclusive. Using -stat.\n");
         arguments->nostat = false;
     }
     if (arguments->hex + arguments->bin + arguments->oct > 1 ) {
         debugPrint("hex:%i,bin:%i,oct:%i", arguments->hex, arguments->bin,arguments->oct);
         arguments->hex = false;
         arguments->bin = false;
         arguments->oct = false;
         warnPrint("Options -hex and -bin and -oct are mutually exclusive. Ignoring.\n");
     }
     if (arguments->num && arguments->str){
         warnPrint("Options -num (-int, -bin, -hex, -oct) and -s are mutually exclusive.\n"
                   "         Ignoring -s.\n");
         arguments->str = false;
     }
     if (arguments->plain || arguments->tool == cainfo) {
         if(arguments->plain && (arguments->num || arguments->hex || arguments->bin || arguments->oct || arguments->str\
                              || arguments->round != roundType_no_rounding || arguments->dblFormatType != '\0'\
                              || arguments->fieldSeparator != 0 || arguments->noname || arguments->nostat\
                              || arguments->stat || arguments->nounit\
                              || arguments->inputSeparator != ' ' || arguments->parseArray)) {
             warnPrint("-plain option overrides all output formatting switches (except date/time related).\n");
         }
         arguments->num =false; arguments->hex =false; arguments->bin = false; arguments->oct =false; arguments->str =false;
         /* arguments->prec is not checked here, since checking arguments->dblFormatType is also set when arguments->prec is set. */
         arguments->round = roundType_no_rounding;
         arguments->dblFormatType = '\0';
         arguments->fieldSeparator = 0;
         arguments->noname = true;
         arguments->nostat = true;
         arguments->stat = false;
         arguments->nounit = true;
     }

    /* Remaining arg list refers to PVs */
    if (arguments->tool == caget || arguments->tool == camon || arguments->tool == cagets || arguments->tool == cado || arguments->tool == cainfo){
       *nChannels = argc - optind;       /*  All remaining args are PV names */
    }
    else {
        if ((argc - optind) % 2) {
            if (arguments->tool == caput || arguments->tool == caputq) fprintf(stderr, "One of the PVs is missing the value to be written ('%s -h' for help).\n", argv[0]);
            if (arguments->tool == cawait)                            fprintf(stderr, "One of the PVs is missing the condition ('%s -h' for help).\n", argv[0]);
            return false;
        }

        *nChannels = (argc - optind) / 2;    /* half of the args are PV names, rest conditions/values */
    }


    if (*nChannels < 1)
    {
        fprintf(stderr, "No record name specified. ('%s -h' for help.)\n", argv[0]);
        return false;
    }

    return true;
}


bool parseChannels(int argc, char ** argv, arguments_T *arguments, struct channel *channels){
    debugPrint("parseChannels()\n");
    u_int32_t i;                      /* counter */
    bool success = true;
    /* Copy PV names from command line */
    for (i = 0; optind < argc; i++, optind++) {
        channels[i].base.name = argv[optind];
        channels[i].prec = 6; /* default precision if none obtained from the IOC*/


        if(strlen(channels[i].base.name) > LEN_RECORD_NAME + LEN_RECORD_FIELD + 1) { /* worst case scenario: longest name + longest field + '.' that separates name and field */
            errPrint("Record name over %d characters: %s - aborting", LEN_RECORD_NAME + LEN_RECORD_FIELD + 1, channels[i].base.name);
            success = false;
            break;
        }

        channels[i].i = i;    /* channel number, serves to synchronise pvs and output. */

        if (arguments->tool == caput || arguments->tool == caputq || arguments->tool == cawait){
            channels[i].inNelm = 1;

             /* store the pointer to correct argv to channels[i].inpStr.
             * We will check for arrays later when we know if channel is an array */
            channels[i].inpStr = argv[optind+1];


            /* finally advance to the next argument */
            optind++;

        }
    }

    return success;
}


bool castStrToDBR(void ** data, struct channel * ch, short * pBaseType, arguments_T * arguments){
    /* convert input string to the baseType */
    debugPrint("castStrToDBR() - start\n");

    int base = 0;   /*  used for number conversion in strto* functions */
    char *endptr;   /*  used in strto* functions */
    bool success = true;
    unsigned long j;
    char ** str; /* holds pointers to parts of the string in tempstr */
    char * tempstr = NULL;

    /* tokenize in case of an array */
    bool tokenize = (ch->count > 1 && !arguments->str) || arguments->parseArray;

    if(tokenize)
    {
        char inSep[2] = {arguments->inputSeparator, 0};
        size_t j;

        /* first count the number of values */
        tempstr = callocMustSucceed(strlen(ch->inpStr)+1, sizeof(char), "parseArray");
        strcpy(tempstr, ch->inpStr);
        j=0;
        char *token = strtok(tempstr, inSep);
        while(token) {
            j++;
            token = strtok(NULL, inSep);
        }
        ch->inNelm = j;

        /* allocate pointer array */
        str = callocMustSucceed(ch->inNelm, sizeof(char*), "Can't allocate  tokens");
        if(str==NULL){
            free(tempstr);
            ch->inNelm = 1;
            cantProceed("Error at allocating tokens");
            return false;
        }

        /* allocate memory at tokens[0] for whole string*/
        strcpy(tempstr, ch->inpStr);
        debugPrint("parseAsArray() - tempstr: %s\n", tempstr);

        /* extract string values */
        token = strtok(tempstr, inSep);
        j=0;

        while(token) {
            str[j]= token;
            debugPrint("parseAsArray() - token: %s\n", str[j]);
            j++;
            token = strtok(NULL, inSep);
        }
    }else{
        debugPrint("parseAsArray() - not an array \n");
        str = callocMustSucceed(1, sizeof(char*), "Can't allocate str in castStrToDBR");
        str[0] = ch->inpStr; /* points to somewhere to argv */
    }


    /* allocate output buffer */
    debugPrint("castStrToDBR() - allocate data with baseType %s and nelm %zu\n", dbr_type_to_text(*pBaseType), ch->inNelm);
    *data = callocMustSucceed(ch->inNelm, dbr_size[*pBaseType], "castStrToDBR");

    /* set up numeric base for sring conversion */
    if(arguments->bin) base = 2;
    else if(arguments->oct) base = 8;
    else if(arguments->hex) base = 16;

    switch (*pBaseType){
    case DBR_INT:/* and short */
        for (j=0; j<ch->inNelm; ++j) {
            ((dbr_int_t *)(*data))[j] = (dbr_int_t)strtoll(str[j], &endptr, base);
            /* if a number before . is found it is also ok */
            if (endptr == str[j] || !(*endptr == '\0' || *endptr == '.') ) {
                if(base) {
                    errPrint("Impossible to convert input %s to %s using numeric base %d\n",str[j], dbr_type_to_text(*pBaseType), base);
                }
                else {
                    errPrint("Impossible to convert input %s to %s as requested\n",str[j], dbr_type_to_text(*pBaseType));
                }
                success = false;
            }
        }
        break;

    case DBR_FLOAT:
        for (j=0; j<ch->inNelm; ++j) {
            ((dbr_float_t *)(*data))[j] = (dbr_float_t)strtof(str[j], &endptr);
            if (endptr == str[j] || *endptr != '\0') {
                errPrint("Impossible to convert input %s to %s\n",str[j], dbr_type_to_text(*pBaseType));
                success = false;
            }
        }
        break;

    case DBR_ENUM:
        ;/* check if put data is provided as a number */
        bool isNumber = true;
        if(!arguments->str){
            for (j=0; j<ch->inNelm; ++j) {
                ((dbr_enum_t *)(*data))[j] = (dbr_enum_t)strtoul(str[j], &endptr, base);
                if (endptr == str[j] || *endptr ) {
                    if(arguments->num){
                        if(base) {
                            errPrint("Impossible to convert input %s to %s using numeric base %d\n",str[j], dbr_type_to_text(*pBaseType), base);
                        }
                        else {
                            errPrint("Impossible to convert input %s to %s as requested\n",str[j], dbr_type_to_text(*pBaseType));
                        }
                        break;
                    }else{
                        isNumber = false;
                    }
                }
            }
        }else{
            isNumber = false;
        }

        /*  if enum is entered as a string, reallocate memory and go to case DBR_STRING */
        if (!isNumber) {
            *pBaseType = DBR_STRING;
            debugPrint("case DBR_ENUM() - string \n");
            free(*data);
            *data = callocMustSucceed(ch->inNelm, dbr_size[*pBaseType], "castStrToDBR");
        }
        else {
            break;
        }

    case DBR_STRING:
        for (j=0; j<ch->inNelm; ++j) {
            int length = truncate(str[j]); /* truncate to MAX_STRING_SIZE */
            debugPrint("truncated string: %s\n", str[j]);
            strncpy(((dbr_string_t *)(*data))[j], str[j], length);
        }
        break;

    case DBR_CHAR:
        debugPrint("castStrToDBR_CHAR() - start\n");
        size_t charsInStr = 0;
        size_t j, k = 0; /*index*/
        long number = 0;
        bool isStr = arguments->str;

        charsInStr = 0;
        for (j=0; j < ch->inNelm; j++) {
            debugPrint("castStrToDBR_CHAR() - try to parse %s\n", str[j]);
            /* try to parse all as 8 bit integer */
            if(!isStr || arguments->num || base != 0){
                number = (dbr_enum_t)strtoul(str[j], &endptr, base);
                if (endptr != str[j] && *endptr == '\0'  && number >=0 && number < 256) {
                    /* is a valid 8 bit number */
                    ((dbr_char_t *)(*data))[j] = (dbr_char_t)(char)number;
                    charsInStr ++;
                    continue;
                }else{
                    if(arguments->num || base != 0){
                        if(base){
                            errPrint("%s can not be parsed as an 8 bit integer using numeric base %d\n", str[j], base);
                        }
                        else {
                            errPrint("%s can not be parsed as an 8 bit integer\n", str[j]);
                        }
                        return false; /* break if one element can not be parsed */
                    }else if (arguments->parseArray){
                        /* array specified - if the element can not be parsed as number just take the first character from it as it is*/
                        ((dbr_char_t *)(*data))[j] = (dbr_char_t)str[j][0];
                    }
                    else{
                        isStr = true;
                        break; /* break if one element can not be parsed */
                    }
                }
            }else if(isStr && arguments->parseArray){
                /* string and array specified - take the first character from each token as it is*/
                ((dbr_char_t *)(*data))[j] = str[j][0];
            }
        }

        /*  handle as string if string */
        if(isStr && !arguments->parseArray){
            debugPrint("castStrToDBR_CHAR() - is string %s\n", ch->inpStr);
            /* reallocate data */
            free(*data);
            size_t charsInStr = strlen(ch->inpStr)+1; /* worst case */
            *data = callocMustSucceed(charsInStr , dbr_size[DBR_CHAR], "Can not allocate data buffer in castStrToDBR_CHAR");

            for (k = 0; k < charsInStr; k++) {
                ((dbr_char_t *)(*data))[k] = (dbr_char_t)ch->inpStr[k];
            }

            ch->inNelm = charsInStr;
        }
        break;

    case DBR_LONG:
        for (j=0; j<ch->inNelm; ++j) {
            ((dbr_long_t *)(*data))[j] = (dbr_long_t)strtoll(str[j], &endptr, base);
            /* if a number before . is found it is also ok */
            if (endptr == str[j] || !(*endptr == '\0' || *endptr == '.')) {
                if(base) {
                    errPrint("Impossible to convert input %s to %s using numeric base %d\n",str[j], dbr_type_to_text(*pBaseType), base);
                }
                else {
                    errPrint("Impossible to convert input %s to %s as requested\n",str[j], dbr_type_to_text(*pBaseType));
                }
                success = false;
            }
        }
        break;

    case DBR_DOUBLE:
        for (j=0; j< ch->inNelm; j++) {
            ((dbr_double_t *)(*data))[j] = (dbr_double_t)strtod(str[j], &endptr);
            if (endptr == str[j] || *endptr != '\0') {
                errPrint("Impossible to convert input %s to %s\n",str[j], dbr_type_to_text(*pBaseType));
                success = false;
                break;
            }
        }
        break;

    default:
        errPrint("Can not print %s DBR type (DBR numeric type code: %"PRId32"). \n", dbr_type_to_text(*pBaseType), *pBaseType);
        success = false;
    }

    /* cleanup */

    if(tokenize && tempstr != NULL){
        free(tempstr);
    }
    free(str);

    debugPrint("castStrToDBR() - end\n");
    return success;
}

/* parses input string and extracts operators and numerals,
* then saves them to the channel structure.
* Supported operators: >,<,<=,>=,==,!=,!, ==A...B(in interval), !=A...B(out of interval), !A...B (out of interval). */
bool cawaitParseCondition(struct channel *channel, char **str, arguments_T * arguments)
{
    debugPrint("cawaitParseCondition()\n");
    enum operator op;
    double arg1;
    double arg2 = 0;


    if (removePrefix(str, "!")) {
        op = operator_neq;
        removePrefix(str, "=");
    }
    else if (removePrefix(str, ">=")) {
        op = operator_gte;
    }
    else if (removePrefix(str, "<=")) {
        op = operator_lte;
    }
    else if (removePrefix(str, ">")) {
        op = operator_gt;
    }
    else if (removePrefix(str, "<")) {
        op = operator_lt;
    }
    else if (removePrefix(str, "==")) {
        op = operator_eq;
    }
    else {
        op = operator_eq;
    }

    if(arguments->str){
        if (op==operator_eq || op==operator_neq){
            if(op==operator_eq) op=operator_streq;
            else op=operator_strneq;
            channel->conditionOperator = op;
            return true;
        }
        else{
            errPrint("Only == and != can be used with strings.\n");
            return false;
        }
    }

    char *endptr;

    bool couldBeString = (op == operator_eq || op == operator_neq) && !arguments->num &&
            (channel->type == DBF_CHAR || channel->type == DBF_ENUM || channel->type == DBF_STRING);

    arg1 = strtod(*str, &endptr);
    if (endptr == *str) {
        if(couldBeString && strlen(*str)){
            /*take as str*/
            if(op==operator_eq) op=operator_streq;
            else op=operator_strneq;
            channel->conditionOperator = op;
            return true;
        }else{
            if(couldBeString) {
                errPrint("Invalid operand in condition.\n");
            }
            else {
                errPrint("Invalid operator in condition. Only == and != can be used with string operands.\n");
            }

            return false;
        }
    }
    debugPrint("endptr: %s\n", endptr);
    if (removePrefix(&endptr, "...") || removePrefix(&endptr, "..")) {
        if (!*endptr){ /*if end of the string something is missing*/
             errPrint("Second operand is missing.\n");
             return false;
        }
        if (op != operator_eq && op != operator_neq) {
            errPrint("Invalid syntax for interval condition.\n");
            return false;
        }
        char *tmp = endptr; /* start of the second operand */
        arg2 = strtod(tmp, &endptr);
        if (endptr == tmp || *endptr) {
            if(couldBeString){
                /*take as str*/
                if(op==operator_eq) op=operator_streq;
                else op=operator_strneq;
                channel->conditionOperator = op;
                return true;
            }
            else{
                errPrint("Invalid second operand for interval condition.\n");
                return false;
            }
        }
        /* we have an interval */
        if(op==operator_eq)  op = operator_in;
        if(op==operator_neq) op = operator_out;
    }else if(*endptr){
        if(couldBeString){
            if(op==operator_eq) op=operator_streq;
            else op=operator_strneq;
            channel->conditionOperator = op;
            return true;
        }else{
            errPrint("Invalid syntax in condition.\n");
            return false;
        }
    }

    /*  if interval make the first operand always smaller one*/
    if( ( op==operator_in || op==operator_out ) && ( arg1 > arg2 ) ){
        channel->conditionOperands[0] = arg2;
        channel->conditionOperands[1] = arg1;
    }else{
        channel->conditionOperands[0] = arg1;
        channel->conditionOperands[1] = arg2;
    }
    channel->conditionOperator = op;

    return true;
}
