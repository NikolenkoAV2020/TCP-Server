FROM       ubuntu:20.04
MAINTAINER Andrey Nikolenko Msk <Nikolenko.A.V.2020@yandex.ru>
LABEL      maintainer="Nikolenko.A.V.2020@yandex.ru"
WORKDIR    /usr/src/test_nikolenko
RUN        apt-get update -y && apt-get upgrade -y && apt-get install -y apt-utils
COPY       ./nikolenko ./nikolenko
RUN        cd ./nikolenko && ./install_tools.sh
RUN        cd ./nikolenko && ./install_boost.sh
RUN        cd ./nikolenko && ./build_test.sh
COPY       ./start_test.sh .
CMD        ./start_test.sh
