# caTools
Detailed help for each tool is available with `-h` flag. Eg. run `caget -h` to get help for caget tool.


The following channel access tools are available:

- `caget`: Reads PV values(s). Usage:

        caget [flags] <pv> [<pv> ...]

- `cagets`: First processes the PV(s), then reads the value(s). Usage:

        cagets [flags] <pv> [<pv> ...]
        
- `camon`: Monitors the PV(s). Usage:

        camon [flags] <pv> [<pv> ...]
        
- `caput`: Writes value(s) to the PV(s), waits until processing finishes and returns updated value(s). Usage:

        caput [flags] <pv> <value> [<pv> <value> ...]
        
- `caputq`: Writes value(s) to PV(s), but does not wait for the processing to finish. Does not have any output (except if an error occurs). Usage:

        caputq [flags] <pv> <value> [<pv> <value> ...]
        
- `cado`: Writes 1 to PV(s), but does not wait for the processing to finish. Does not have any output (except if an error occurs). Usage:

        cado [flags] <pv> [<pv> ...]
        
- `cawait`: Monitors the PV(s), but only displays the values when they match the provided conditions. Usage:

        cawait [flags] <pv> <condition> [<pv> <condition> ...]
The conditions are specified as a string containing the operator together with the values.
Following operators are supported:  `>, <, <=, >=, ==, !=, ==A...B, !=A...B`. 

        
- `cainfo`: Displays detailed information about the provided records. Usage:

        cainfo [flags] <pv> [<pv> ...]


## caWait examples
caWait tool can be used in scripts, as it will exit upon meeting user specified condition. Here are a few examples:

- Exit when value of caTools-TEST:AI is smaller than 5.

           cawait caTools-TEST:AI '<5'
       
- Exit when value of caTools-TEST:AI is greater or equal to 5.

           cawait caTools-TEST:AI '>=5'
       
- Exit when value of caTools-TEST:AI is not 5.

           cawait caTools-TEST:AI '!5'
       
- Exit when value of caTools-TEST:AI is between 5 and 10.

           cawait caTools-TEST:AI '5...10'
       
- Exit when value of caTools-TEST:AI is not between 5 and 10.

           cawait caTools-TEST:AI '!5...10'
       

## Build from source
The tools are structured as an EPICS application.

1. If you do not have the sources, git clone this repository in your local folder ( referenced as the `<top>` folder).
1. Amend `<top>/configure/RELEASE` to match your site specifics.
1. Amend `<top>/configure/CONFIG_SITE` to match your site specifics.
1. Move to `<top>` folder and run `make`
1. Find the executable in `<top>/bin/<architecture>/caTools`
1. Find symbolic links for all the tools in `<top>/bin/<architecture>/` folder.
