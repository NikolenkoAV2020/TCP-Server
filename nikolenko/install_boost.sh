wget -O   boost_1_75_0.tar.gz https://dl.bintray.com/boostorg/release/1.75.0/source/boost_1_75_0.tar.gz 
tar -xzf  ./boost_1_75_0.tar.gz 
rm        ./boost_1_75_0.tar.gz
cd        boost_1_75_0
pwd
./bootstrap.sh --prefix=/usr/local
./b2
./b2 install
cd        ..
rm -R     ./boost_1_75_0
cat       /usr/local/include/boost/version.hpp | grep "BOOST_LIB_VERSION"
pwd
