// TestClient.cpp 
//

#include <fstream>
#include <iostream>
#include <cstdlib> 
#include <cstring>
#include <ctime>
#include <string>
#include <stdio.h>

#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/chrono.hpp>
#include <boost/regex.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/local_time/local_time.hpp>

#if !defined(WIN32) and !defined(WINDOWS)
#   define sprintf_s  snprintf
#endif

using namespace boost::asio;
using namespace boost::posix_time;

bool test_ip_adress(const std::string& s, std::string* ip)
{
    if (s == "localhost") {
        if (ip) *ip = "127.0.0.1";
        return true;
    }
    boost::regex  pattern("(\\d{1,3}(\\.\\d{1,3}){3})");
    boost::smatch match;
    if (!boost::regex_search(s, match, pattern)) return false;
    if (ip) *ip = match[1];
    return true;
}

bool test_command(const std::string& s, std::string* cmd)
{
    std::string c = s;
    std::transform(c.begin(), c.end(), c.begin(), toupper);
    if (c == "INSERT" ||
        c == "UPDATE" ||
        c == "DELETE" ||
        c == "GET")
    {
        if (cmd) *cmd = c;
        return true;
    }
    return false;
}

bool test_command_string(
    int argc,
    char* argv[],
    std::string& adress,
    std::string& command,
    std::string& key,
    std::string& value)
{
    adress = "127.0.0.1";
    command = "INSERT";
    key = "";
    value = "";

    int i = 1;
    while (i < argc) {
        if (argc > i) {
            if (test_ip_adress(argv[i], &adress)) {
                ++i;
                continue;
            }
        }
        if (argc > i) {
            std::string cmd;
            if (test_command(argv[i], &cmd)) {
                command = cmd;
                ++i;
                continue;
            }
        }
        if (argc > i && !key.length()) {
            key = argv[i];
            ++i;
            continue;
        }
        if (argc > i && !value.length()) {
            value = argv[i];
            ++i;
            continue;
        }
        break;
    }
    if (command == "GET" || command == "DELETE") {
        if (value.length() || !key.length()) return false;
    }
    if (command == "INSERT" || command == "UPDATE") {
        if (!value.length() || !key.length()) return false;
    }

    return true;
}

ptime        start_time      = second_clock::local_time();
unsigned int max_durations   = 30;
const char   server_def_ip[] = "127.0.0.1";
const int    server_port     = 31415;
io_service   service;

std::ostream& operator << (std::ostream& stream, const boost::system::error_code& err)
{
    return stream
        << err.category().name()
        << " message: "
        << err.message() << std::endl;
}

class TestClient : public boost::enable_shared_from_this<TestClient>
{
    std::string      m_adress = server_def_ip;
    std::string      m_command = "";
    std::string      m_key = "";
    std::string      m_value = "";
    ip::tcp::socket  m_socket;
    static const int m_message_length = 3072;
    char             m_write_buf[m_message_length];

public:
    TestClient(
        const std::string& adress,
        const std::string& command,
        const std::string& key,
        const std::string& value) :
        m_adress(adress),
        m_command(command),
        m_key(key),
        m_value(value),
        m_socket(service)
    {
    }

    void start()
    {
        std::cout << "Test client started..." << std::endl;
        connect();
    }

    void connect()
    {
        auto endpoint = ip::tcp::endpoint(ip::address::from_string(m_adress), server_port);
        auto binder = boost::bind(&TestClient::on_connect, shared_from_this(), _1);
        m_socket.async_connect(endpoint, binder);
    }

    void on_connect(const boost::system::error_code& err)
    {
        if (err) {
            ptime current_time = second_clock::local_time();
            auto sec = (current_time - start_time).seconds();
            if (sec > max_durations) {
                std::cout << "Connection error: " << err << std::endl;
                close();
                return;
            }
            std::cout << "Unsuccessful attempt to connect to the server. Retry connection..." << std::endl;
            boost::chrono::milliseconds period(1000);
            boost::this_thread::sleep_for(period);
            connect();
            return;
        }
        write();
    }

    void close()
    {
        if (!m_socket.is_open()) return;
        std::cout << "Client close." << std::endl;
        m_socket.close();
    }

    void write()
    {
        std::string s = m_command + "\n" + m_key;
        if (m_value.length()) s += "\n" + m_value;
        sprintf_s(m_write_buf, m_message_length, "%s", s.c_str());

        auto buf = buffer(m_write_buf, m_message_length);
        auto hnd = boost::bind(&TestClient::on_write, shared_from_this(), _1);

        async_write(m_socket, buf, hnd);

        std::replace(s.begin(), s.end(), '\n', '\t');
        std::cout << "Sent to server:     " << s << std::endl;
    }

    void on_write(const boost::system::error_code& err)
    {
        if (err) {
            std::cout << "Write error: " << err << std::endl;
            close();
            return;
        }
        read_answer();
    }

    void read_answer()
    {
        auto buf = buffer(m_write_buf, m_message_length);
        auto hnd = boost::bind(&TestClient::on_read_answer, shared_from_this(), _1);
        async_read(m_socket, buf, hnd);
    }

    void on_read_answer(const boost::system::error_code& err)
    {
        if (err) std::cout << "Read answer error: " << err << std::endl;
        else     std::cout << "Answer from server: " << std::string(m_write_buf) << std::endl;
    }
};

int main(int argc, char* argv[])
{
#if defined WIN32 or defined WINDOWS
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);
#else
    setlocale(LC_ALL, "Russian");
#endif
    std::string adress;
    std::string command;
    std::string key;
    std::string value;

    if (!test_command_string(argc, argv, adress, command, key, value)) {
        std::cout << "Invalid command format." << std::endl;
        std::cout << "Using: " << std::endl;
        std::cout << "       testclient <COMMAND> <key string>  <value string>" << std::endl;
        std::cout << "       Commands: INSERT, UPDATE, DELETE, GET" << std::endl;
        return 0;
    }
   
    boost::shared_ptr<TestClient> c = boost::make_shared<TestClient>(
        adress,
        command,
        key,
        value);

    c->start();
    service.run();

    return 0;
}
