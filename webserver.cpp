#include "webserver.hpp"
#include <websocketpp/server.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <json11.hpp>

namespace fs = boost::filesystem;

namespace
{
    bool isInDirectory(fs::path const& path, fs::path const& root)
    {
        auto absolutepath = fs::absolute(path);
        auto absoluteroot = fs::absolute(root);
        return boost::starts_with(absolutepath.string(), absoluteroot.string());
    }
}

namespace deepness
{
    Webserver::Webserver(boost::filesystem::path document_root)
        : m_root(std::move(document_root))
    {
        using std::placeholders::_1;
        using std::placeholders::_2;
        // logging settings
        m_server.set_access_channels(websocketpp::log::alevel::all);
        m_server.clear_access_channels(websocketpp::log::alevel::frame_payload);
        m_server.init_asio();
        m_server.set_reuse_addr(true);
        m_server.set_http_handler(std::bind(&Webserver::handleHttp, this, _1));
        m_server.set_message_handler(std::bind(&Webserver::handleReceivedMessage, this, _1, _2));
        m_server.listen(8080);
        m_server.start_accept();
        m_thread = std::thread([this] {
                m_server.run();
            });
    }

    Webserver::~Webserver()
    {
        m_server.stop();
        m_thread.join();
    }

    void Webserver::handleHttp(websocketpp::connection_hdl handle)
    {
        using namespace std;
        using namespace websocketpp;
        auto connection = m_server.get_con_from_hdl(handle);
        auto path = m_root / connection->get_resource();
        if(!isInDirectory(path, m_root))
        {
            connection->set_status(http::status_code::not_found);
            return;
        }
        if(!fs::exists(path))
        {
            auto indexpath = path / "index.html";
            if(fs::exists(indexpath))
            {
                path = std::move(indexpath);
            }
            else
            {
                connection->set_status(http::status_code::not_found);
                return;
            }
        }
        fs::ifstream s(path, ios_base::in | ios_base::binary);
        s.seekg(0, ios::end);
        string data;
        data.reserve(s.tellg());
        s.seekg(0, ios::beg);
        data.assign(istreambuf_iterator<char>(s),
                    istreambuf_iterator<char>());
        connection->set_status(http::status_code::ok);
        connection->set_body(move(data));
    }

    void Webserver::handleReceivedMessage(websocketpp::connection_hdl handle, Server::message_ptr msg)
    {
        using namespace json11;
        std::string error;
        auto doc = Json::parse(msg->get_payload().c_str(), error);
        assert(!doc.is_null());
        auto const& cmd = doc["cmd"];
        if(!cmd.is_string())
        {
            std::cerr << "Bad message received: " << msg->get_payload();
            return;
        }
        CommandHandler handler;
        {
            std::lock_guard<std::mutex> lock(m_commandsMutex);
            auto it = m_commands.find(cmd.string_value());
            if(it == m_commands.end())
            {
                std::cerr << "Unexpected command received: " << cmd.string_value();
                return;
            }
            handler = it->second;
        }
        handler(doc["args"], [this, handle](std::string const& message) {
                websocketpp::lib::error_code ec;
                auto connection = m_server.get_con_from_hdl(handle, ec);
                if(ec || !connection)
                {
                    std::cerr << "Error sending message: " << ec.message();
                    return false;
                }
                ec = connection->send(message);
                if(ec)
                {
                    std::cerr << "Error sending message: " << ec.message();
                    return false;
                }
                return true;
            });
    }

    void Webserver::handleMessage(std::string command, CommandHandler handler)
    {
        std::lock_guard<std::mutex> lock(m_commandsMutex);
        m_commands[std::move(command)] = std::move(handler);
    }
}
