echo "Start this test just after first starting the test IOC"
echo "caGet output"
../caget CTT:PROC-CALC
echo
echo "caGets output"
../cagets CTT:PROC-CALC
echo
echo "making caDo request"
../cado CTT:PROC-CALC
echo "caGet output"
../caget CTT:PROC-CALC
echo
echo "done"