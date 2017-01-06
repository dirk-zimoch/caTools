echo
echo "caPut"
..\caput %1 CTT:TEST_1-AI.SCAN ".5 second"
..\caput %1 CTT:TEST_1-AI.EGU "tooLongEguToWrite"
..\caput %1 CTT:TEST_MBBO 5
..\caput %1 CTT:TEST_MBBO.ZRVL 0
..\caput %1 CTT:TEST_MBBO.DESC "CTT description"
..\caput %1 CTT:STRING "Value field can have only fourty chars_"
..\caput %1 CTT:STRING.OUT "ten1234567twenty1234thirty1234forty12345fifty12345sixty1234_"
..\caput %1 CTT:TEST_WVF-CHAR 'a 1 b 2 c 3'
..\caput %1 CTT:TEST_WVF-DOUBLE "1 2.3 1e2 0xA"
..\caput %1 CTT:TEST_WVF-DOUBLE.PREC 5
..\caput %1 CTT:TEST_WVF-FLOAT "1 2.3 1e2 0xA"
..\caput %1 CTT:TEST_WVF-LONG "1 2.3 100 0xA 6 7 8 9 10 11"
..\caput %1 CTT:TEST_WVF-SHORT "1 100 1000 10000 100000 1000000"
..\caput %1 CTT:TEST_WVF-STRING "1st string bring dring ping"
..\caput %1 CTT:MOTOR 5
..\caput %1 CTT:MOTOR.VELO 8
..\caput %1 CTT:STATIC-LONGIN 0
..\caput %1 CTT:STATIC-AO 0
echo
echo "caPutq"
..\caputq %1 CTT:TEST_1-AI.SCAN ".2 second"
..\caputq %1 CTT:TEST_1-AI.EGU "ohTooLongEguToWrite"
..\caputq %1 CTT:TEST_MBBO 4
..\caputq %1 CTT:TEST_MBBO.ZRVL 1
..\caputq %1 CTT:TEST_MBBO.DESC "CTTest description"
..\caputq %1 CTT:STRING "Value field can have only fourty chars!"
..\caputq %1 CTT:STRING.OUT "ten1234567twenty1234thirty1234forty12345fifty12345sixty1234!"
..\caputq %1 CTT:TEST_WVF-CHAR '1 a 2 b 3 c'
..\caputq %1 CTT:TEST_WVF-DOUBLE "2 2.3 1e2 0xA"
..\caputq %1 CTT:TEST_WVF-FLOAT "2 2.3 1e2 0xA"
..\caputq %1 CTT:TEST_WVF-LONG "2 2.3 100 0xA 6 7 8 9 10 11"
..\caputq %1 CTT:TEST_WVF-SHORT "2 100 1000 10000 100000 1000000"
..\caputq %1 CTT:TEST_WVF-STRING "2nd string bring dring ping"
..\caputq %1 CTT:MOTOR 4
..\caputq %1 CTT:MOTOR.VELO 5
..\caputq %1 CTT:STATIC-LONGIN 1
..\caputq %1 CTT:STATIC-AO 1
