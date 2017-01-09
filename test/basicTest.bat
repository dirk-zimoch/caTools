echo
echo "-----------------------------------------"
echo "Various output formatting"
echo "-----------------------------------------"
echo "Using: -plain"
call get.bat -plain
echo
echo "Using: -noname -nostat -nounit"
call get.bat -noname -nostat -nounit
echo
echo "Using: -stat -plain"
call get.bat -stat -plain
echo
echo "Using: -stat -nostat"
call get.bat -stat -nostat
echo
echo "Using: -d -t -localdate -localtime"
call get.bat -d -t -localdate -localtime
echo

echo
echo "-----------------------------------------"
echo "caPut on waveforms"
echo "-----------------------------------------"
echo "with -bin"
call putWf.bat -bin
echo
echo "with -oct"
call putWf.bat -oct
echo
echo "with -hex"
call putWf.bat -hex

echo
echo "-----------------------------------------"
echo "Test setting and reading enum values with number or string"
echo "-----------------------------------------"
..\caput CTT:TEST_MBBO 1
..\caput CTT:TEST_MBBO "25.00 Hz"
..\caget -num CTT:TEST_MBBO
..\caget  CTT:TEST_MBBO

..\caput CTT:BO 0
..\caput CTT:BO 1
..\caput CTT:BO 0
..\caput -s CTT:BO 0
..\caput CTT:BO 0

echo
echo "-----------------------------------------"
echo "Test setting and reading chars as numbers or strings"
echo "-----------------------------------------"
..\caput -s CTT:TEST_WVF-CHAR "1 2"
..\caget -num CTT:TEST_WVF-CHAR
..\caget CTT:TEST_WVF-CHAR
..\caput CTT:TEST_WVF-CHAR "1 2"
..\caget -num CTT:TEST_WVF-CHAR
..\caget CTT:TEST_WVF-CHAR
..\caput -num CTT:TEST_WVF-CHAR "50 32 32 32 49"
..\caget -num CTT:TEST_WVF-CHAR
..\caget CTT:TEST_WVF-CHAR

echo
echo "-----------------------------------------"
echo "Double conversion"
echo "-----------------------------------------"
..\caput CTT:CALC 10.23
..\caput CTT:CALC.PREC 4
..\caget -hex CTT:CALC
..\caget -bin CTT:CALC
..\caget -oct CTT:CALC
..\caget -s CTT:CALC
..\caget -num CTT:CALC

echo
echo "-----------------------------------------"
echo "Number of elements"
echo "-----------------------------------------"
..\caget -nord CTT:TEST_WVF-CHAR CTT:TEST_WVF-DOUBLE CTT:TEST_WVF-FLOAT CTT:TEST_WVF-LONG CTT:TEST_WVF-SHORT CTT:TEST_WVF-STRING CTT:PROC-CALC CTT:RecordNameThatIsLongerThanFortyCharsForLongStringTest.NAME
