# Basic test for caTools
This folder contains an example IOC and a few scripts to help automate basic testing of caTools. The basic test checks for functionalities such as:

- writing various types of values (double, int, string, ...) with various types of formatting (hex, bin, ...)
- reading various types of values (double, int, string, ...) with various types of formatting (hex, bin, ...)
- reading long strings
- character conversion (numeric and string)
- double conversion (binary, hex, string)
- reading and writing enum values as string or number
- various formatting switches (date/time, stat, noname, ...)

Tools used in the tests are: caget, caput, cagets, caputq and cado.

## How to
Move to the folder, where caTools were copied and build. Create create symbolic links, or make copies of the executable appropriate for your architecture in the same folder for: caget, caput, cagets, caputq and cado

Then:

- in the first terminal, start the IOC in the `test/IOC` folder

        cd test/IOC
        iocsh startup.script
    
- in the second terminal, run the `basicTest.sh` script, and pipe it's output to `basicTest.out` file

        cd test
        ./basicTest.sh &> basicTest.out
        
- files `basicTest.out` and `basicTest.example` should be the same (except timestamps), or the test fails

### EPICS version 3.13
There are differences in behavior when using caTools on IOCs running older version of EPICS:

- .NORD and .NELM fields of waveforms are handled differently
- no long string support

Running this test on 3.13 version of EPICS works, but user must expect different results. Some differences are mentioned above, some other are:

- puts on CTT:STRING records will fail because it's OUT link is not valid
- various severities will be reported because of too long names (eg. SEVR:INVALID STAT:LINK)