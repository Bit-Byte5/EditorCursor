#pragma once

#include <functional>
#include <string>

#include <websocketpp/common/connection_hdl.hpp>

namespace editorcursor {

void startWsServer(int port);
void stopWsServer();

void sendToClient(
    websocketpp::connection_hdl const& hdl,
    std::string const& payload
);

} // namespace editorcursor
