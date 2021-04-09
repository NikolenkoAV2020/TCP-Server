mkdir ./TCP-Test
cd    ./TCP-Server
pwd
mkdir ./build
cd    ./build
pwd
cmake ..
make 
cd    ../..
rm -R ./TCP-Server/build
pwd
cd    ./TCP-Client
pwd
mkdir ./build
cd    ./build
pwd
cmake ..
make 
cd    ../..
rm -R ./TCP-Client/build
pwd
