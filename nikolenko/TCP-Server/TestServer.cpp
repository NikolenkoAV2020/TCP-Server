// testboost.cpp : Этот файл содержит функцию "main". Здесь начинается и заканчивается выполнение программы.
//

// Nikolenko.cpp

// Реализовать небольшую key-value базу данных.

// *
// Cервер слушает запросы на указанном IP:PORT и предоставляет API для простой key-value базы данных.
// Внутреннее хранилище сервера - boost::multi_index, который хранится в маппируемом в оперативную память файле.
// Ключ имеет тип - Строка (максимальная длина 1024 символа).
// Значение имеет тип - Строка (максимальная длина 1024 * 1024 символа).

// База данных поддерживает операции:
// *   INSERT - добавить key:value;
// *   UPDATE - изменить key:value;
// *   DELETE - удалить key;
// *   GET    - получить value по key.

// * Если ключ уже существует, то при операции INSERT База Данных возвращает ошибку, что запись отсутсвует.
// * Если ключ не существует, то при операции UPDATE База Данных возвращает ошибку, что запись отсутсвует.
// * Если ключ существует и значение совпадает, то при операции UPDATE База Данных возвращает ошибку, что значение не было изменено.
// * Если ключ не существует, то при операции DELETE База Данных возвращает ошибку об отсутствующей записи.
// * Если ключ не существует, то при операции GET База Данных возвращает ошибку об отсутствующей записи.

// *
// Клиент получает из коммандной строки:
//   - адрес сервера;
//   - команду;
//   - ключ;
//   - значение.

// После выполнения команды клиент возвращает успешность выполнения и ошибку, если она возникла.

// Сервер ведет статистику отправленных и полученных комманд.
// С переодичности в 60 секунд, сервер выводит на std::cerr статистику:
//    - количество записей в БД;
//    - количество успешных/неуспешных операций INSERT;
//    - количество успешных/неуспешных операций UPDATE;
//    - количество успешных/неуспешных операций DELETE;
//    - количество успешных/неуспешных операций GET.

// * Общение по собственному протоколу поверх TCP/IP.
//   Для автоматизации сериализации структур использовать boost::fusion.
// * Для реализации сетевого взаимодействия использовать boost::asio. 
//   Для реализации потоков использовать возможности boost::asio.
// * Для реализации таймеров использовать возможности boost::asio.
// * Для хранения данных использовать boost::multi_index и boost::interprocess. 

// Сборка C++-проекта должна осуществляться с помощью CMake.
// Для сборки сформировать Dokerfile на базе Ubuntu 20.08.

// Результат прислать на почту hr@cyber-core.dev.

#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/chrono.hpp>
#include <boost/regex.hpp>

#include <boost/thread/thread.hpp>

#include <boost/multi_array.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/key_extractors.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/set.hpp>

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/string.hpp>

#include <fstream>
#include <iostream>
#include <cstdlib> 
#include <cstring>
#include <ctime>
#include <string>
#include <stdio.h>

#if !defined(WIN32) and !defined(WINDOWS)
#   define sprintf_s  snprintf
#endif

//-----------------------------------------------------------------------------
// Execute Around Pointer Idiom
// Заимствованный шаблон
//-----------------------------------------------------------------------------

template<typename T, typename mutex_type = std::recursive_mutex>
class SafeObj {
    std::shared_ptr<mutex_type> mtx;
    std::shared_ptr<T>          p;

    void lock  () const { mtx->lock(); }
    void unlock() const { mtx->unlock(); }

public:
    class proxy {
        std::unique_lock< mutex_type > lock;
        T* const p;
    public:
        proxy(T* const _p, mutex_type& _mtx) : lock(_mtx), p(_p) {}
        proxy(proxy&& px) : lock(std::move(px.lock)), p(px.p) {}
        ~proxy() {}
        T* operator -> () {return p;}
        const T* operator -> () const {return p;}
    };

    template< typename ...Args >
    SafeObj(Args ... args) : mtx(std::make_shared< mutex_type >()), p(std::make_shared< T >(args...)) {}

    proxy operator->() { return proxy(p.get(), *mtx); }
    const proxy operator->() const { return proxy(p.get(), *mtx); }
    template< class Args > friend class std::lock_guard;
};

using namespace ::boost::multi_index;
using namespace ::boost::interprocess;

//-----------------------------------------------------------------------------
// Statistics
//-----------------------------------------------------------------------------

struct ServerStastistics {
    unsigned int successInsert = 0;
    unsigned int failInsert    = 0;
    unsigned int successUpdate = 0;
    unsigned int failUpdate    = 0;
    unsigned int successDelete = 0;
    unsigned int failDelete    = 0;
    unsigned int successGet    = 0;
    unsigned int failGet       = 0;
};

//-----------------------------------------------------------------------------
// Storage for BD
//-----------------------------------------------------------------------------

struct StorageItem {
    std::string m_key, m_val, addr;
    struct IndByK {};
    struct IndByV {};
    struct ValChange : public std::unary_function<StorageItem, void> {
        std::string p; ValChange(const std::string& _p) : p(_p) {}
        void operator()(StorageItem& r) { r.m_val = p; }
    };
};

typedef boost::multi_index_container<
    StorageItem,
    indexed_by<
        ordered_unique<
            tag<StorageItem::IndByK>, 
            member<StorageItem, 
            std::string, 
            &StorageItem::m_key>
        >,
        ordered_non_unique<
            tag<StorageItem::IndByV>, 
            member<StorageItem, 
            std::string, 
            &StorageItem::m_val>
        >
    >
> StorageContainer;

typedef StorageContainer::index<StorageItem::IndByK>::type  StorageIndK;
typedef StorageContainer::index<StorageItem::IndByV>::type  StorageIndV;
typedef StorageIndK::const_iterator  StorageIteratorK;
typedef StorageIndV::const_iterator  StorageIteratorV;

struct shm_remove {
    // Remove shared memory on construction and destruction
    shm_remove () { shared_memory_object::remove("TCP_test_shared_memory"); }
    ~shm_remove() { shared_memory_object::remove("TCP_test_shared_memory"); }
} remover;

class Storage {
    std::string m_result    = "";
    std::string m_file_path = "";
    std::string m_command   = "";
    std::string m_key       = "";
    std::string m_val       = "";

    managed_shared_memory* shm = nullptr;
    StorageContainer*      container = nullptr;

public:
    ServerStastistics stat;

public:
    Storage () {
        shm = new managed_shared_memory(create_only, "TCP_test_shared_memory", 1000);
        container = shm->construct<StorageContainer>("StorageContainer")(); 
    }

    ~Storage() {
        delete shm; shm = nullptr;
    }

    int execute(char* cmd, std::string* result)
    {
        m_result  = "";
        m_command = "";
        m_key     = "";
        m_val     = "";

        std::string s(cmd);
        int i = s.find("\n");
        m_command = s.substr(0, i);

        if (i > 0) {
            ++i;
            int j = s.find("\n", i);
            m_key = s.substr(i, (size_t)(j - i));

            if (j > 0) {
                ++j;
                int k = s.find("\n", j);
                m_val = s.substr(j, (size_t)(k - j));
            }
        }

        return docommand(result) ? 1 : 0;
    }

    std::string get_result() 
    {
        return m_result;
    } 

    int load (const std::string& file_path)
    {
        if (file_path.length()) m_file_path = file_path;
        if (!m_file_path.length()) return -1;

        return 0;
    }

    int save(const std::string& file_path = "")
    {
        if (file_path.length()) m_file_path = file_path;
        if (!m_file_path.length()) return -1;

        if (container->size ()) {
        }

        return 0;
    }

private:
    int docommand(std::string* result)
    {
        auto exit_error = [&] (int err_code, unsigned int& count) -> int {
            m_result = "An entry with the \"" + m_key + "\" key is not found.";
            if (result) *result = m_result;
            ++count;
            return err_code;
        };

        m_result = "";
        const StorageIndK& ik  = container->get<StorageItem::IndByK>();
        StorageIteratorK   itk = ik.find(m_key);
        StorageItem        item;

        if (m_command == "INSERT") {
            if (itk != ik.end()) {
                m_result = "An entry with the \"" + m_key + "\" key already exists.";
                ++stat.failInsert;
                if (result) *result = m_result;
                return 1;
            }
            item.m_key = m_key;
            item.m_val = m_val;
            auto ok    = container->insert(item).second;
            m_result   = std::string("Command INSERT is ") + (ok ? "successful" : "failed") + " execute.";
            ++(ok ? stat.successInsert : stat.failInsert);
        }
        else if (m_command == "UPDATE") {
            if (itk == ik.end()) return exit_error(2, stat.failUpdate);
            StorageIteratorV itv = container->project<StorageItem::IndByV>(itk);
            StorageIndV&     iv  = container->get<StorageItem::IndByV>();
            auto ok  = iv.modify(itv, StorageItem::ValChange(m_val));
            m_result = std::string("Command UPDATE is ") + (ok ? "successful" : "failed") + " execute.";
            ++(ok ? stat.successUpdate : stat.failUpdate);
        }
        else if (m_command == "DELETE") {
            if (itk == ik.end()) return exit_error(4, stat.failDelete);
            auto r   = container->erase(itk);
            auto key = r->m_key;
            bool ok  = (key != m_key); 
            m_result = std::string("Command DELETE is ") + (ok ? "successful" : "failed") + " execute.";
            ++(ok ? stat.successDelete : stat.failDelete);
        }
        else if (m_command == "GET") {
            if (itk == ik.end()) return exit_error(5, stat.failGet);
            m_key    = (*itk).m_key;
            m_val    = (*itk).m_val;
            m_result = "Get is successful: key = \"" + m_key + "\" value = \"" + m_val + "\"";
            ++stat.successGet;
        }
        if (result) *result = m_result;
        return 0;
    }
}; 

//-----------------------------------------------------------------------------
// Global objects
//-----------------------------------------------------------------------------

using namespace boost::asio;

typedef boost::asio::deadline_timer Timer;
typedef boost::posix_time::seconds  Interval;
typedef SafeObj<Storage>            SafeStorage;

// Storage create like thread safe object.
SafeStorage storage;

// Timeout for show statistics.
const int   time_interval = 60;
Timer      *ptimer = nullptr;

// Port number
const int   server_port   = 31415;

//-----------------------------------------------------------------------------
// Thread body for show statistics time from time
//-----------------------------------------------------------------------------

void statistics_show_loop(const boost::system::error_code& /*e*/) 
{
    std::cerr << " ----------- Test server statistics------" << std::endl;
    std::cerr << "           " << "Successfull"   "       Fail" << std::endl;
    std::cerr << " ----------------------------------------" << std::endl;
    std::cerr << " Insert:   " << std::setw(11) << storage->stat.successInsert << std::setw(11) << storage->stat.failInsert << std::endl;
    std::cerr << " Update:   " << std::setw(11) << storage->stat.successUpdate << std::setw(11) << storage->stat.failUpdate << std::endl;
    std::cerr << " Delete:   " << std::setw(11) << storage->stat.successDelete << std::setw(11) << storage->stat.failDelete << std::endl;
    std::cerr << " Get   :   " << std::setw(11) << storage->stat.successGet    << std::setw(11) << storage->stat.failGet    << std::endl;
    std::cerr << " ----------------------------------------" << std::endl << std::endl;

    if (!ptimer) return;
    ptimer->expires_at(ptimer->expires_at() + Interval(time_interval));
    ptimer->async_wait(statistics_show_loop);
}

//-----------------------------------------------------------------------------
// Implementation of Server.
//-----------------------------------------------------------------------------

std::ostream& operator << (std::ostream& stream, const boost::system::error_code& err)
{
    return stream
        << err.category().name()
        << " message: "
        << err.message() << std::endl;
}

class Server :public boost::enable_shared_from_this<Server>
{
private:
    io_service&      m_service;
    static const int m_message_length = 3072;
    char             m_read_buf[m_message_length];
    ip::tcp::socket  m_socket;
    boost::scoped_ptr<ip::tcp::acceptor> m_acc;

public:
    Server(io_service& service_) : 
        m_service(service_),
        m_socket(service_)
    {
    }

    void start()
    {
        std::cout << "Server is started..." << std::endl;
        accept();
    }

    void accept()
    {
        auto endpoint = ip::tcp::endpoint(ip::tcp::v4(), server_port);
        auto acceptor = new ip::tcp::acceptor(m_service, endpoint);
        m_acc.reset(acceptor);
        auto hnd = boost::bind(&Server::on_accept, shared_from_this(), _1);
        m_acc->async_accept(m_socket, hnd);
    }

    void on_accept(const boost::system::error_code& err)
    {
        if (err) {
            std::cout << "Accept error: " << err << std::endl;
            close();
            return;
        }
        read();
    }

    void close()
    {
        std::cout << "Close server." << std::endl;
        if (m_socket.is_open())        m_socket.close();
        if (m_acc && m_acc->is_open()) m_acc->close();
    }

    void read()
    {
        auto buf = buffer(m_read_buf, m_message_length);
        auto hnd = boost::bind(&Server::on_read, shared_from_this(), _1);
        async_read(m_socket, buf, hnd);
    }

    void on_read(const boost::system::error_code& err)
    {
        if (err) {
            // End of file. Client dropped connection.
            if (err.value() == 2) {
                // Close current socket
                if (m_socket.is_open())        m_socket.close();
                if (m_acc && m_acc->is_open()) m_acc->close();
                // New accept
                accept();
                // Exit from current read.
                return;
            }
            // Error reading from socket
            std::cout << "Error in read: " << err;
            // Close socket and close server.
            close();
            return;
        }

        std::string s(m_read_buf);
        std::replace(s.begin(), s.end(), '\n', '\t');
        std::cout << "Received from client: " << s << std::endl;

        // Execute received command.
        std::string result;
        storage->execute(m_read_buf, &result);

        // Answer to client. 
        write_answer(result);
    }

    void write_answer(std::string result)
    {
        sprintf_s(m_read_buf, m_message_length, "%s", result.c_str());
        auto buf = buffer(m_read_buf);
        auto hnd = boost::bind(&Server::on_write_answer, shared_from_this(), _1, _2);
        async_write(m_socket, buf, hnd);
        std::cout << "Sent to client:       " << result << std::endl;
    }

    void on_write_answer(const boost::system::error_code& err, size_t bytes)
    {
        if (err) {
            std::cout << "Error in write: " << err << std::endl;
            close();
            return;
        }
        // Begin new reading from socket
        read();
    }
};

//-----------------------------------------------------------------------------
// Main program
//-----------------------------------------------------------------------------

int main(int argc, char* argv[])
{
/*
    {
        StorageContainer store;
        StorageItem r1 = { "Basilio", "022" };
        bool ok1 = store.insert(r1).second;
        StorageItem r2 = { "Pupkinio", "022" };
        bool ok2 = store.insert(r2).second;
        StorageItem r3 = { "aaaa", "023" };
        bool ok3 = store.insert(r3).second;
        StorageItem r4 = { "sssss", "024" };
        bool ok4 = store.insert(r4).second;

        std::string find_id = "Pupkinio";
        typedef StorageContainer::index<StorageItem::IndByK>::type NList;
        typedef StorageContainer::index<StorageItem::IndByV>::type PList;
        NList& ns = store.get<StorageItem::IndByK >();
        PList& ps = store.get<StorageItem::IndByV>();
        NList::const_iterator nit = ns.find(find_id);
        if (nit != ns.end()) {
            PList::const_iterator pit = store.project<StorageItem::IndByV>(nit);
            ps.modify(pit, StorageItem::ValChange("134"));
        }
        nit = ns.find(find_id);
        auto fine = (*nit).m_val;
       std::cout << fine << std::endl;
    }
*/
#if defined WIN32 or defined WINDOWS
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);
#else
    setlocale(LC_ALL, "Russian");
#endif

    std::string storage_file_path = "./test_storage";
    if (storage->load(storage_file_path)) {
        std::cout << "Error of open storage file. Server closing..." << std::endl;
        return 1;
    }

    io_service serv_service;

    Timer timer(serv_service, Interval(time_interval));    
    timer.async_wait(statistics_show_loop);
    ptimer = &timer;

    boost::shared_ptr<Server> s = boost::make_shared<Server>(serv_service);
    s->start();
    serv_service.run();

    std::cout << "Server closing..." << std::endl;
    if (storage->save()) {
        std::cout << "Error of save storage file." << std::endl;
        return 1;
    }
}
