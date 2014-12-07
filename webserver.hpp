#pragma once

#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <boost/filesystem/path.hpp>
#include <thread>

namespace deepness
{
    class Webserver
    {
    public:
        Webserver(boost::filesystem::path document_root);
        ~Webserver();
    private:
        using Server = websocketpp::server<websocketpp::config::asio>;
        void handleHttp(websocketpp::connection_hdl);

        boost::filesystem::path m_root;
        Server m_server;
        std::thread m_thread;
    };
}
