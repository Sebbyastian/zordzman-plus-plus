#pragma once

#include <functional>

#include "common/logger/Logger.hpp"
#include "json11.hpp"

#include "Client.hpp"
#include "Map.hpp"

#include <vector>
#include <SDL_net.h>
#include <fstream>

#define RECV_BUFFER_SIZE 1024
#define UDP_PORT 4545

/// The Zordsman server
namespace server {

// pls help
// typedef std::function<void(Server *server, Client *client, json11::Json
// entity)> MessageHandler;

class Server {

public:
    Server(IPaddress address, unsigned int max_clients, std::string map_name);
    ~Server();
    int exec();

    /// @brief Broadcast a message to all clients
    ///
    /// See Client::send().
    void sendAll(std::string type, json11::Json entity);

    /// @brief Add a message handler
    ///
    /// When a message of the given type is received all handlers for that
    /// message type are called with the message 'entity' field as the Json
    /// parameter.
    void addHandler(std::string type,
                    std::function<void(Server *server, Client *client,
                                       json11::Json entity)> handler);

private:
    void initSDL();
    /// @brief Accept all pending connections
    ///
    /// This accept(2)s all pending connections on the listening socket. These
    /// new connections are wrapped in a `Client` and added to the `m_clients`
    /// collection as well has having their socket added to the socket set.
    /// If the max number of clients has been reached the new client will be
    /// disconnected immediately.
    void acceptConnections();

    void handleMapRequest(Server *server, Client *client, json11::Json entity);

    /// @brief Handle `net.udp` message from clients
    ///
    /// net.udp is used by the client to specify the port number of its UDP
    /// socket. The message entity should be a valid port number as an integer.
    ///
    /// The handler will attempt to reserve a socket channel for the client,
    /// disconnecting if it fails to. If successful the client's m_channel is
    /// set to the allocated channel.
    void handleNetUDP(Server *server, Client *client, json11::Json entity);

    unsigned int m_max_clients;
    TCPsocket m_socket;
    UDPsocket m_udp_socket;
    IPaddress m_address;
    std::vector<Client> m_clients;
    SDLNet_SocketSet m_socket_set;
    common::Logger m_logger;
    map::Level m_map;
    std::map<std::string,
             std::vector<std::function<void(Server *server, Client *client,
                                            json11::Json entity)>>> m_handlers;
};
} // namespace server
