#pragma once

#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <boost/filesystem/path.hpp>
#include <thread>
#include <json11.hpp>
#include <unordered_map>

namespace deepness
{
    class Webserver
    {
    public:
        Webserver(boost::filesystem::path document_root);
        ~Webserver();
        /*! \returns false if you should stop sending stuff to this socket. */
        using SendFunc = std::function<bool (std::string const&)>;
        using CommandHandler = std::function<void (json11::Json const& args, SendFunc)>;
        // TODO handle specific resources
        void handleMessage(std::string command, CommandHandler handler);
    private:
        using Server = websocketpp::server<websocketpp::config::asio>;
        void handleHttp(websocketpp::connection_hdl);
        void handleReceivedMessage(websocketpp::connection_hdl, Server::message_ptr msg);
        void handleOpen(websocketpp::connection_hdl);

        boost::filesystem::path m_root;
        Server m_server;
        std::thread m_thread;
        std::mutex m_commandsMutex;
        std::unordered_map<std::string, CommandHandler> m_commands;
    };
}
