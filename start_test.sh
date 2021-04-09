cd        ./nikolenko/TCP-Test
pwd
echo      Starting Server ...
./testserver >/dev/null 2>&1 < /dev/null &

echo      Starting Client ...
./testclient INSERT q1 Vulue1
./testclient INSERT q2 Vulue2
./testclient INSERT q3 Vulue3
./testclient INSERT q4 Vulue4
./testclient INSERT q5 Vulue5
./testclient GET    q1
./testclient UPDATE q1 "qwerty asdfghVulue5"
./testclient DELETE q1
./testclient INSERT q6  Vulue6
./testclient INSERT q7  Vulue7
./testclient INSERT q8  Vulue8
./testclient DELETE a1
./testclient DELETE a2
./testclient GET    a3
./testclient INSERT q9  Vulue9
./testclient INSERT q10 Vulue10
./testclient GET    q1

echo Exit from clients operations.
