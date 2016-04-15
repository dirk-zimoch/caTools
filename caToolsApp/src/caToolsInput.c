#include <stdio.h>
#include <getopt.h>
#include <string.h>
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

    //usage:
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
    else { //tool unknown
        fprintf(stream, "\nUnknown tool. Try %s -tool with any of the following arguments: caget, caput, cagets, "\
                "caputq, cado, camon, cawait, cainfo.\n", programName);
        return;
    }

    //descriptions
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
        else { //caputq
            fputs("Writes value(s) to PV(s), but does not wait for the processing to finish. Does not have any output (except if an error occurs).\n", stream);
        }
        fputs("Array handling:\n", stream);
        fputs(  "- When -a option is set, input separator (-inSep argument) is used to parse elements in an array.\n"\
                "- When input separator (-inSep argument) is explicitly defined, -a option is automatically used."
                " See following examples which produce the same result,"\
                " namely write 1, 2 and 3 into record pvA and 4, 5, 6 into pvB:\n", stream);
        fputs("  caput -a pvA '1 2 3' pvB '4 5 6'\n", stream);
        fputs("  caput -inSep ; pvA '1;2;3' pvB '4;5;6'\n", stream);
    }
    if (tool == cado) {
        fputs("Writes 1 to PV(s), but does not wait for the processing to finish. Does not have any output (except if an error occurs).\n", stream);
    }
    if (tool == camon) {
        fputs("Monitors the PV(s).\n", stream);
    }
    if (tool == cawait) {
         fputs("Monitors the PV(s), but only displays the values when they match the provided conditions. When at least one of the conditions is true, the program exits." \
               "The conditions are specified as a string containing the operator together with the values.\n", stream);
        fputs("Following operators are supported:  >, <, <=, >=, ==, !=, !, ==A...B(in interval), !=A...B or !A...B (out of interval). For example, "\
                "cawait pv '==1...5' exits after pv value is inside the interval [1,5].\n", stream);
    }
    if (tool == cainfo) {
        fputs("Displays detailed information about the provided records.\n", stream);
    }

    //flags common for all tools
    fputs("\n", stream);
    fputs("Accepted flags:\n", stream);
    fputs("\n", stream);
    fputs("  -h                   Help: Print this message\n", stream);
    fputs("  -v                   Verbosity. Options:\n", stream);
    fprintf(stream,"                       Print error messages: %d (default)\n", VERBOSITY_ERR);
    fprintf(stream,"                       Also print warning messages: %d\n", VERBOSITY_WARN);
    fprintf(stream,"                       Also print error messages that occur periodically: %d\n", VERBOSITY_ERR_PERIODIC);
    fprintf(stream,"                       Also print warning messages that occur periodically: %d\n\n", VERBOSITY_WARN_PERIODIC);

    //flags common for most of the tools
    if (tool != cado){
        fputs("Channel Access options\n", stream);
        if (tool != cainfo) {
            fputs("  -dbrtype <type>      Type of DBR request to use for communicating with the server.\n", stream);
            fputs("                       Use string (DBR_ prefix may be omitted, or number of one of the following types:\n", stream);
            fputs("           DBR_STRING     0  DBR_STS_FLOAT    9  DBR_TIME_LONG   19  DBR_CTRL_SHORT    29\n"
                  "           DBR_INT        1  DBR_STS_ENUM    10  DBR_TIME_DOUBLE 20  DBR_CTRL_INT      29\n"
                  "           DBR_SHORT      1  DBR_STS_CHAR    11  DBR_GR_STRING   21  DBR_CTRL_FLOAT    30\n"
                  "           DBR_FLOAT      2  DBR_STS_LONG    12  DBR_GR_SHORT    22  DBR_CTRL_ENUM     31\n"
                  "           DBR_ENUM       3  DBR_STS_DOUBLE  13  DBR_GR_INT      22  DBR_CTRL_CHAR     32\n"
                  "           DBR_CHAR       4  DBR_TIME_STRING 14  DBR_GR_FLOAT    23  DBR_CTRL_LONG     33\n"
                  "           DBR_LONG       5  DBR_TIME_INT    15  DBR_GR_ENUM     24  DBR_CTRL_DOUBLE   34\n"
                  "           DBR_DOUBLE     6  DBR_TIME_SHORT  15  DBR_GR_CHAR     25  DBR_STSACK_STRING 37\n"
                  "           DBR_STS_STRING 7  DBR_TIME_FLOAT  16  DBR_GR_LONG     26  DBR_CLASS_NAME    38\n"
                  "           DBR_STS_SHORT  8  DBR_TIME_ENUM   17  DBR_GR_DOUBLE   27\n"
                  "           DBR_STS_INT    8  DBR_TIME_CHAR   18  DBR_CTRL_STRING 28\n", stream);

        }
        fprintf(stream, "  -w <time>            Wait time in seconds, specifies CA timeout. (default: %d s)\n", CA_DEFAULT_TIMEOUT);
    }

    //flags associated with writing
    if (tool == caput || tool == caputq) {
        fputs("Formating input : Array format options\n", stream);
        fputs("  -a                   Input separator (-inSep argument) is used to parse elements in an array.\n", stream);
        fputs("  -inSep <separator>   Separator used between array elements in <value>.\n", stream);
        fputs("                       If not specified, space is used.\n", stream);
        fputs("                       If specified, '-a' option is automatically used. \n", stream);
    }

    // flags associated with monitoring
    if (tool == camon) {
        fputs("Monitoring options\n", stream);
        fputs("  -n <number>          Exit the program after <number> updates.\n", stream);
        fputs("  -timestamp <option>  Display relative timestamps. Options:\n", stream);
        fputs("                            r: time elapsed since start of the program,\n", stream);
        fputs("                            u: time elapsed since last update of any PV,\n", stream);
        fputs("                            c: time elapsed since last update separately for each PV.\n", stream);
    }
    if (tool == cawait) {
        fputs("Waiting options\n", stream);
        fputs("  -timeout <number>    Exit the program after <number> seconds without an update.\n", stream);
    }

    //Flags associated with returned values. Used by both reading and writing tools.
    if (tool == caget || tool == cagets || tool == camon
            || tool == cawait ||  tool == caput) {
        fputs("Formating output : General options\n", stream);
        fputs("  -noname              Hide PV name.\n", stream);
        fputs("  -nostat              Never display alarm status and severity.\n", stream);
        fputs("  -stat                Always display alarm status and severity.\n", stream);
        fputs("  -plain               Ignore all formatting switches (displays only PV value) except date/time related.\n", stream);

        fputs("Formating output : Time options\n", stream);
        fputs("  -d -date             Display server date.\n", stream);
        fputs("  -localdate           Display client date.\n", stream);
        fputs("  -localtime           Display client time.\n", stream);
        fputs("  -t, -time            Display server time.\n", stream);

        fputs("Formating output : Integer format options\n", stream);
        fputs("  -bin                 Display integer values in binary format.\n", stream);
        fputs("  -hex                 Display integer values in hexadecimal format.\n", stream);
        fputs("  -oct                 Display integer values in octal format.\n", stream);
        fputs("  -s                   Interpret value(s) as char (number to ascii).\n", stream);

        fputs("Formating output : Floating point format options\n", stream);
        fputs("  -e <number>          Format doubles using scientific notation with precision <number>. Overrides -prec option.\n", stream);
        fputs("  -f <number>          Format doubles using floating point with precision <number>. Overrides -prec option.\n", stream);
        fputs("  -g <number>          Format doubles using shortest representation with precision <number>. Overrides -prec option.\n", stream);
        fputs("  -prec <number>       Override PREC field with <number>. (default: PREC field).\n", stream);
        fputs("  -round <option>      Round floating point value(s). Options:\n", stream);
        fputs("                                 round: round to nearest (default).\n", stream);
        fputs("                                 ceil: round up,\n", stream);
        fputs("                                 floor: round down,\n", stream);

        fputs("Formating output : Enum/char format options\n", stream);
        fputs("  -int, -num           Display enum/char values as numbers.\n", stream);

        fputs("Formating output : Array format options\n", stream);
        fputs("  -nord                Display number of array elements before their values.\n", stream);
        fputs("  -outNelm <number>    Number of array elements to read.\n", stream);
        fputs("  -outSep <number>     Separator between array elements.\n", stream);
    }
}

int parseArguments(int argc, char ** argv, u_int32_t *nChannels, arguments_T *arguments){
	int opt;                    // getopt() current option
    int opt_long;               // getopt_long() current long option

	if (endsWith(argv[0],"caget")) arguments->tool = caget;
    if (endsWith(argv[0],"caput")) arguments->tool = caput;
    if (endsWith(argv[0],"cagets")) arguments->tool = cagets;
    if (endsWith(argv[0],"caputq")) arguments->tool = caputq;
    if (endsWith(argv[0],"camon")) arguments->tool = camon;
    if (endsWith(argv[0],"cawait")) arguments->tool = cawait;
    if (endsWith(argv[0],"cado")) arguments->tool = cado;
    if (endsWith(argv[0],"cainfo")) arguments->tool = cainfo;

    static struct option long_options[] = {
        {"num",     	no_argument,        0,  0 },
        {"int",     	no_argument,        0,  0 },    //same as num
        {"round",   	required_argument,  0,  0 },	//type of rounding:round, ceil, floor
        {"prec",    	required_argument,  0,  0 },	//precision
        {"hex",     	no_argument,        0,  0 },	//display as hex
        {"bin",     	no_argument,        0,  0 },	//display as bin
        {"oct",     	no_argument,        0,  0 },	//display as oct
        {"plain",   	no_argument,        0,  0 },	//ignore formatting switches
        {"stat",    	no_argument,       0,  0 },	//status and severity on
        {"nostat",  	no_argument,        0,  0 },	//status and severity off
        {"noname",  	no_argument,        0,  0 },	//hide name
        {"nounit",  	no_argument,        0,  0 },	//hide units
        {"timestamp",	required_argument, 	0,  0 },	//timestamp type r,u,c
        {"localdate",	no_argument,       0,  0 },	//client date
        {"time",      no_argument,       0,  0 },	//server time
        {"localtime",	no_argument,       0,  0 },	//client time
        {"date",      no_argument,       0,  0 },	//server date
        {"outNelm", 	required_argument,	0,  0 },	//number of array elements - read
        {"outSep",      required_argument,	0,  0 },	//array field separator - read
        {"inSep",      required_argument,	0,  0 },	//array field separator - write
        {"nord",      no_argument,      0,  0 },	//display number of array elements
        {"tool",      required_argument, 	0,	0 },	//tool
        {"timeout",   	required_argument, 	0,	0 },	//timeout
        {"dbrtype",   	required_argument, 	0,	0 },	//dbrtype
        {0,         	0,                 	0,  0 }
    };
    putenv("POSIXLY_CORRECT="); //Behave correctly on GNU getopt systems = stop parsing after 1st non option is encountered

    while ((opt = getopt_long_only(argc, argv, ":w:e:f:g:n:sthdav:", long_options, &opt_long)) != -1) {
        switch (opt) {
        case 'w':
            if (sscanf(optarg, "%lf", &arguments->caTimeout) != 1){    // type was not given as float
                arguments->caTimeout = CA_DEFAULT_TIMEOUT;
                warnPrint("Requested timeout invalid - ignored. ('%s -h' for help.)\n", argv[0]);
            }
            break;
        case 'd': // same as date
            arguments->date = true;
            break;
        case 'e':	//how to format doubles in strings
        case 'f':
        case 'g':
            ;//declaration must not follow label
            int32_t digits;
            if (sscanf(optarg, "%"SCNd32, &digits) != 1){
                warnPrint("Invalid precision argument '%s' "
                        "for option '-%c' - ignored.\n", optarg, opt);
            } else {
                if (digits>=0){
                    if (arguments->dblFormatType == '\0' && arguments->prec >= 0) { // in case of overiding -prec
                        warnPrint("Option '-%c' overrides precision set with '-prec'.\n", opt);
                    }
                    arguments->dblFormatType = opt; // set 'e' 'f' or 'g'
                    arguments->prec = digits;
                }
                else{
                    warnPrint("Precision %d for option '-%c' "
                            "out of range - ignored.\n", digits, opt);
                }
            }
            break;
        case 's':	//try to interpret values as strings
            arguments->str = true;
            break;
        case 't':	//same as time
            arguments->time = true;
            break;
        case 'n':	//stop monitor after numUpdates
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
            }
            break;
        case 0:   // long options
            switch (opt_long) {
            case 0:   // num
                arguments->num = true;
                break;
            case 1:   // int
                arguments->num = true;
                break;
            case 2:   // round
                ;//declaration must not follow label
                int type;
                if (sscanf(optarg, "%d", &type) != 1) {   // type was not given as a number [0, 1, 2]
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
                } else { // type was given as a number
                    if( type < roundType_round || roundType_floor < type) {   // out of range check
                        arguments->round = roundType_no_rounding;
                        warnPrint("Invalid round type '%s' "
                            "for option '%s' - ignored.\n", optarg, long_options[opt_long].name);
                    } else{
                        arguments->round = type;
                    }
                }
                break;
            case 3:   // prec
                if (arguments->dblFormatType == '\0') { // formating with -f -g -e has priority
                    if (sscanf(optarg, "%"SCNd32, &arguments->prec) != 1){
                        warnPrint("Invalid precision argument '%s' "
                                "for option '%s' - ignored.\n", optarg, long_options[opt_long].name);
                    } else if (arguments->prec < 0) {
                       warnPrint("Precision %"PRId32" for option '%s' "
                               "out of range - ignored.\n", arguments->prec, long_options[opt_long].name);
                       arguments->prec = -1;
                    }
                }
                else {
                    warnPrint("Option '-prec' ignored because of '-f', '-e' or '-g'.\n");
                }
                break;
            case 4:   //hex
                arguments->hex = true;
                break;
            case 5:   // bin
                arguments->bin = true;
                break;
            case 6:   // oct
                arguments->oct = true;
                break;
            case 7:   // plain
                arguments->plain = true;
                break;
            case 8:   // stat
                arguments->stat = true;
                break;
            case 9:	  // nostat
                arguments->nostat = true;
                break;
            case 10:   // noname
                arguments->noname = true;
                break;
            case 11:   // nounit
                arguments->nounit = true;
                break;
            case 12:   // timestamp
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
            case 13:   // localdate
                arguments->localdate = true;
                break;
            case 14:   // time
                arguments->time = true;
                break;
            case 15:   // localtime
                arguments->localtime = true;
                break;
            case 16:   // date
                arguments->date = true;
                break;
            case 17:   // outNelm - number of elements - read
                if (sscanf(optarg, "%"SCNd64, &arguments->outNelm) != 1){
                    warnPrint("Invalid count argument '%s' "
                            "for option '%s' - ignored.\n", optarg, long_options[opt_long].name);
                }
                else {
                    if (arguments->outNelm < 1) {
                        warnPrint("Count number for option '%s' "
                                "must be positive integer - ignored.\n", long_options[opt_long].name);
                        arguments->outNelm = -1;
                    }
                }
                break;
            case 18:   // field separator for output
                if (sscanf(optarg, "%c", &arguments->fieldSeparator) != 1){
                    warnPrint("Invalid argument '%s' "
                            "for option '%s' - ignored.\n", optarg, long_options[opt_long].name);
                }
                break;
            case 19:   // field separator for input
                if (sscanf(optarg, "%c", &arguments->inputSeparator) != 1){
                    warnPrint("Invalid argument '%s' "
                            "for option '%s' - ignored.\n", optarg, long_options[opt_long].name);
                }
                else {
                    arguments->parseArray = true;
                }
                break;
            case 20:   // nord
                arguments->nord = true;
                break;
            case 21:	// tool
                // rev RV: what is this line below
                ;//c
                int tool;
                if (sscanf(optarg, "%d", &tool) != 1){   // type was not given as a number [0, 1, 2]
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
                } else{ // type was given as a number
                    if(tool >= caget|| tool <= cainfo){   //unknown tool case handled down below
                        arguments->round = tool;
                    }
                }
                break;
            case 22: //timeout
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
            case 23: //dbrtype
                if (sscanf(optarg, "%"SCNd32, &arguments->dbrRequestType) != 1)     // type was not given as an integer
                {
                    dbr_text_to_type(optarg, arguments->dbrRequestType);
                    if (arguments->dbrRequestType == -1)                   // Invalid? Try prefix DBR_
                    {
                        char str[30] = "DBR_";
                        strncat(str, optarg, 25);
                        dbr_text_to_type(str, arguments->dbrRequestType);
                    }
                }
                if (arguments->dbrRequestType < DBR_STRING    || arguments->dbrRequestType > DBR_CTRL_DOUBLE){
                    warnPrint("Requested dbr type out of range "
                            "or invalid - ignored. ('%s -h' for help.)\n", argv[0]);
                    arguments->dbrRequestType = -1;
                }
                break;
            }
            break;
        case '?':
            fprintf(stderr, "Unrecognized option: '-%c'. ('%s -h' for help.). Exiting.\n", optopt, argv[0]);
            return EXIT_FAILURE;
            break;
        case ':':
            fprintf(stderr, "Option '-%c' requires an argument. ('%s -h' for help.). Exiting.\n", optopt, argv[0]);
            return EXIT_FAILURE;

            break;
        case 'h':               //Print usage
            usage(stdout, arguments->tool, argv[0]);
            return EXIT_SUCCESS;
        default:
            usage(stderr, arguments->tool, argv[0]);
            exit(EXIT_FAILURE);
        }
     }

     verbosity = arguments->verbosity;

     //check mutually exclusive arguments (without taking dbr type into account)
     if (arguments->tool == tool_unknown){
         usage(stderr, arguments->tool,argv[0]);
         return EXIT_FAILURE;
     }
     if(arguments->tool == cainfo && (arguments->str || arguments->num || arguments->bin  || arguments->hex || arguments->oct \
             || arguments->dbrRequestType != -1 || arguments->prec != -1 || arguments->round != roundType_no_rounding || arguments->plain \
             || arguments->dblFormatType != '\0' || arguments->stat || arguments->nostat || arguments->noname || arguments->nounit \
             || arguments->timestamp != '\0' || arguments->timeout != -1 || arguments->date || arguments->time || arguments->localdate \
             || arguments->localtime || arguments->fieldSeparator != ' ' || arguments->inputSeparator != ' ' || arguments->numUpdates != -1\
             || arguments->parseArray || arguments->outNelm != -1 || arguments->nord)){
         warnPrint("The only options allowed for cainfo are -w and -v. Ignoring the rest.\n");
     }
     else if (arguments->tool != camon){
         if (arguments->timestamp != 0){
             warnPrint("Option -timestamp can only be specified with camon.\n");
         }
         if (arguments->numUpdates != -1){
             warnPrint("Option -n can only be specified with camon.\n");
         }
     }
     else if ((arguments->parseArray || arguments->inputSeparator !=' ') && (arguments->tool != caput && arguments->tool != caputq)){
         warnPrint("Option -a and -inSep can only be specified with caput or caputq.\n");
     }
     if ((arguments->outNelm != -1 || arguments->num !=false || arguments->hex !=false || arguments->bin !=false || arguments->oct !=false)\
             && (arguments->tool == cado || arguments->tool == caputq)){
         warnPrint("Option -outNelm, -num, -hex, -bin and -oct cannot be specified with cado or caputq, because they have no output.\n");
     }
     if (arguments->nostat != false && arguments->stat != false){
         warnPrint("Options -stat and -nostat are mutually exclusive.\n");
         arguments->nostat = false;
     }
     if (arguments->hex + arguments->bin + arguments->oct > 1 ) {
         arguments->hex = false;
         arguments->bin = false;
         arguments->oct = false;
         warnPrint("Options -hex and -bin and -oct are mutually exclusive. Ignoring.\n");
     }
     if (arguments->num && arguments->str){
         warnPrint("Options -num and -s are mutually exclusive.\n");
     }
     if (arguments->plain || arguments->tool == cainfo) {
         if (arguments->tool != cainfo) warnPrint("-plain option overrides all formatting switches.\n");
         arguments->num =false; arguments->hex =false; arguments->bin = false; arguments->oct =false; arguments->str =false;
         arguments->prec = -1;   // prec is also handled in printValue()
         arguments->round = roundType_no_rounding;
         arguments->dblFormatType = '\0';
         arguments->fieldSeparator = ' ' ;
         arguments->noname = true;
         arguments->nostat = true;
         arguments->stat = false;
         arguments->nounit = true;
     }


    //Remaining arg list refers to PVs
    if (arguments->tool == caget || arguments->tool == camon || arguments->tool == cagets || arguments->tool == cado || arguments->tool == cainfo){
       *nChannels = argc - optind;       // All remaining args are PV names
    }
    else {
        if ((argc - optind) % 2) {
            if (arguments->tool == caput || arguments->tool == caputq) fprintf(stderr, "One of the PVs is missing the value to be written ('%s -h' for help).\n", argv[0]);
            if (arguments->tool == cawait)                            fprintf(stderr, "One of the PVs is missing the condition ('%s -h' for help).\n", argv[0]);
            return EXIT_FAILURE;
        }
        *nChannels = (argc - optind) / 2;	// half of the args are PV names, rest conditions/values
    }


    if (*nChannels < 1)
    {
        fprintf(stderr, "No record name specified. ('%s -h' for help.)\n", argv[0]);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}


//parses input string and extracts operators and numerals,
//then saves them to the channel structure.
//Supported operators: >,<,<=,>=,==,!=,!, ==A...B(in interval), !=A...B(out of interval), !A...B (out of interval).
bool cawaitParseCondition(struct channel *channel, char *str)
{
    enum operator operator;
    double arg1;
    double arg2 = 0;


    if (removePrefix(&str, "!")) {
        operator = operator_neq;
        removePrefix(&str, "=");
    }
    else if (removePrefix(&str, ">=")) {
        operator = operator_gte;
    }
    else if (removePrefix(&str, "<=")) {
        operator = operator_lte;
    }
    else if (removePrefix(&str, ">")) {
        operator = operator_gt;
    }
    else if (removePrefix(&str, "<")) {
        operator = operator_lt;
    }
    else if (removePrefix(&str, "==")) {
        operator = operator_eq;
    }
    else {
        operator = operator_eq;
    }


    char *token = strtok(str, "...");
    char *endptr;

    arg1 = strtod(str, &endptr);
    if (endptr == token || *endptr != '\0') {
        errPrint("Invalid operand in condition.\n");
        return false;
    }

    token = strtok(NULL, "...");
    if (operator != operator_eq && operator != operator_neq && token) {
        errPrint("Invalid syntax for interval condition.\n");
        return false;
    }
    else if(token) {
        arg2 = strtod(token, &endptr);
        if (endptr == str || *endptr) {
            errPrint("Invalid second operand for interval condition.\n");
            return false;
        }
    }

    // TODO: if arg1 > arg2 when using ... syntax

    channel->conditionOperator = operator;
    channel->conditionOperands[0] = arg1;
    channel->conditionOperands[1] = arg2;

    return true;
}



bool parseChannels(int argc, char ** argv, u_int32_t nChannels,  arguments_T *arguments, struct channel *channels){
	u_int32_t i,j;                      // counter
	bool success = true;
	// Copy PV names from command line
    for (i = 0; optind < argc; i++, optind++) {
        channels[i].base.name = argv[optind];

        if(strlen(channels[i].base.name) > LEN_RECORD_NAME + LEN_RECORD_FIELD + 1) { //worst case scenario: longest name + longest field + '.' that separates name and field
            errPrint("Record name over %d characters: %s - aborting", LEN_RECORD_NAME + LEN_RECORD_FIELD + 1, channels[i].base.name);
            success = false;
            break;
        }

        channels[i].i = i;	// channel number, serves to synchronise pvs and output.

        if (arguments->tool == caput || arguments->tool == caputq){
            channels[i].inNelm = 1;

            if (!arguments->parseArray) {  //the argument to write consists of just a single element.
                truncate(argv[optind+1]);

                // allocate resources and set pointer to the argument
                channels[i].writeStr = callocMustSucceed (channels[i].inNelm, sizeof(char*), "main");
                channels[i].writeStr[0] = argv[optind+1];
            }
            else{//parse the string assuming each element is delimited by the inputSeparator char
                char inSep[2] = {arguments->inputSeparator, 0};

                // first count the number of arguments->
                char* tempstr = callocMustSucceed(strlen(argv[optind+1])+1, sizeof(char), "main");
                strcpy(tempstr, argv[optind+1]);
                j=0;
                char *token = strtok(tempstr, inSep);
                while(token) {
                    j++;
                    token = strtok(NULL, inSep);
                }
                free(tempstr);
                channels[i].inNelm = j;

                // allocate resources
                channels[i].writeStr = callocMustSucceed (channels[i].inNelm, sizeof(char*), "main");

                // extract arguments
                j=0;
                token = strtok(argv[optind+1], inSep);
                while(token) {
                    truncate(token);
                    channels[i].writeStr[j] = token;
                    j++;
                    token = strtok(NULL, inSep);
                }
            }
            //finally advance to the next argument
            optind++;

        }
        else if (arguments->tool == cawait) {
            //next argument is the condition string
            if (!cawaitParseCondition(&channels[i], argv[optind+1])) {
                success = false;
                break;
            }
            optind++;
        }
    }

    return success;
}

bool castStrToDBR(void ** data, char **str, unsigned long nelm, int32_t baseType){
    debugPrint("castStrToDBR() - start\n");
    //convert input string to the baseType
    *data = callocMustSucceed(nelm, dbr_size[baseType], "castStrToDBR");
    int base = 0;   // used for number conversion in strto* functions
    char *endptr;   // used in strto* functions
    bool success = true;
    unsigned long j;
    switch (baseType){
    case DBR_INT://and short
        for (j=0; j<nelm; ++j) {
            ((dbr_int_t *)(*data))[j] = (dbr_int_t)strtoll(str[j], &endptr, base);
            if (endptr == str[j] || *endptr != '\0') {
                errPrint("Impossible to convert input %s to format %s\n",str[j], dbr_type_to_text(baseType));
                success = false;
            }
        }
        break;

    case DBR_FLOAT:
        for (j=0; j<nelm; ++j) {
            ((dbr_float_t *)(*data))[j] = (dbr_float_t)strtof(str[j], &endptr);
            if (endptr == str[j] || *endptr != '\0') {
                errPrint("Impossible to convert input %s to format %s\n",str[j], dbr_type_to_text(baseType));
                success = false;
            }
        }
        break;

    case DBR_ENUM:
        ;//check if put data is provided as a number
        bool isNumber = true;
        for (j=0; j<nelm; ++j) {
            ((dbr_enum_t *)(*data))[j] = (dbr_enum_t)strtoull(str[j], &endptr, base);
            if (endptr == str[j] || *endptr != '\0') {
                isNumber = false;
            }
        }

        // if enum is entered as a string, reallocate memory and go to case DBR_STRING
        if (!isNumber) {
            baseType = DBR_STRING;

            free(data);
            *data = callocMustSucceed(nelm, dbr_size[baseType], "castStrToDBR");
        }
        else {
            break;
        }

    case DBR_STRING:
        for (j=0; j<nelm; ++j) {
            strcpy(((dbr_string_t *)(*data))[j], str[j]);
        }
        break;

    case DBR_CHAR:{
            // count number of characters to write
            size_t charsInStr = 0;
            size_t charsPerStr[nelm];
            for (j=0; j < nelm; ++j) {
                charsPerStr[j] = strlen(str[j]);
                charsInStr += charsPerStr[j];
            }

            if (charsInStr != nelm) { // don't fiddle with memory if we don't have to
                free(*data);
                *data = callocMustSucceed(charsInStr, dbr_size[baseType], "castStrToDBR");
            }

            // store all the chars to write
            charsInStr = 0;
            for (j=0; j < nelm; ++j) {
                for (size_t k = 0; k < charsPerStr[j]; k++) {
                    ((dbr_char_t *)(*data))[charsInStr] = (dbr_char_t)str[j][k];
                    charsInStr++;
                }
            }
            nelm = charsInStr;
        }
        break;

    case DBR_LONG:
        for (j=0; j<nelm; ++j) {
            ((dbr_long_t *)(*data))[j] = (dbr_long_t)strtoll(str[j], &endptr, base);
            if (endptr == str[j] || *endptr != '\0') {
                errPrint("Impossible to convert input %s to format %s\n",str[j], dbr_type_to_text(baseType));
                success = false;
            }
        }
        break;

    case DBR_DOUBLE:
        for (j=0; j<nelm; ++j) {
            ((dbr_double_t *)(*data))[j] = (dbr_double_t)strtod(str[j], &endptr);
            if (endptr == str[j] || *endptr != '\0') {
                errPrint("Impossible to convert input %s to format %s\n",str[j], dbr_type_to_text(baseType));
                success = false;
            }
        }
        break;

    default:
        errPrint("Can not print %s DBR type (DBR numeric type code: %"PRId32"). \n", dbr_type_to_text(baseType), baseType);
        success = false;
    }
    debugPrint("castStrToDBR() - end\n");
    return success;
}