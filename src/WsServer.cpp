#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "WsServer.hpp"

#include "ActionRegistry.hpp"
#include "Response.hpp"

#include <Geode/Geode.hpp>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

using namespace geode::prelude;
using websocketpp::connection_hdl;
using websocketpp::lib::bind;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;

namespace editorcursor {

using WSServer = websocketpp::server<websocketpp::config::asio>;

static WSServer* g_wsServer = nullptr;
static std::thread g_wsThread;
static std::atomic<bool> g_wsRunning{false};
static std::mutex g_sendMutex;

static void handlePing(
    std::string const& nonce,
    std::function<void(std::string const&)> const& reply
) {
    safeSend(
        reply,
        makeSuccess(
            nonce,
            "ping",
            "PING",
            matjson::makeObject({
                {"modLoaded", true},
                {"inEditor", g_inEditor.load()},
            })
        )
    );
}

static void onOpen(connection_hdl) {
    log::info("[EditorCursor] WebSocket client connected");
}

static void onClose(connection_hdl) {
    log::debug("[EditorCursor] WebSocket client disconnected");
}

static void onMessage(
    connection_hdl hdl,
    WSServer::message_ptr msg,
    std::function<void(connection_hdl const&, std::string const&)> const& send
) {
    auto reply = [&](std::string const& payload) { send(hdl, payload); };

    try {
        auto const& payload = msg->get_payload();
        if (payload.empty()) {
            safeSend(
                reply,
                makeError("", "ingress", "EMPTY_PAYLOAD", "Message body is empty", true)
            );
            return;
        }

        auto parsed = matjson::parse(payload);
        if (!parsed) {
            safeSend(
                reply,
                makeError("", "parse", "INVALID_JSON", "Failed to parse JSON payload", true)
            );
            return;
        }

        auto root = parsed.unwrap();
        if (!root.isObject()) {
            safeSend(
                reply,
                makeError("", "parse", "INVALID_JSON", "JSON root must be an object", true)
            );
            return;
        }

        auto request = root;

        if (!request.contains("nonce") || !request["nonce"].isString()) {
            safeSend(
                reply,
                makeError("", "parse", "MISSING_NONCE", "Request must include a string nonce field", true)
            );
            return;
        }

        auto nonce = request["nonce"].asString().unwrapOr("");
        if (nonce.empty()) {
            safeSend(
                reply,
                makeError("", "parse", "MISSING_NONCE", "Request nonce must not be empty", true)
            );
            return;
        }

        if (!request.contains("action") || !request["action"].isString()) {
            safeSend(
                reply,
                makeError(
                    nonce,
                    "parse",
                    "UNKNOWN_ACTION",
                    "Request must include a string action field",
                    false
                )
            );
            return;
        }

        auto action = request["action"].asString().unwrapOr("");
        if (action == "PING") {
            handlePing(nonce, reply);
            return;
        }

        auto const* def = findActionDefinition(action);
        if (!def) {
            safeSend(
                reply,
                makeError(
                    nonce,
                    "parse",
                    "UNKNOWN_ACTION",
                    "Unsupported action: " + action,
                    false,
                    action
                )
            );
            return;
        }

        if (def->requiresEditor && !g_inEditor.load()) {
            safeSend(reply, makeNotInEditorError(nonce, def->name));
            return;
        }

        queueAction(hdl, nonce, action, request);
    } catch (std::exception const& e) {
        log::error("[EditorCursor] onMessage exception: {}", e.what());
        safeSend(
            reply,
            makeError("", "ingress", "INTERNAL_ERROR", "Failed to process WebSocket message", true)
        );
    } catch (...) {
        log::error("[EditorCursor] onMessage unknown exception");
        safeSend(
            reply,
            makeError("", "ingress", "INTERNAL_ERROR", "Failed to process WebSocket message", true)
        );
    }
}

void sendToClient(connection_hdl const& hdl, std::string const& payload) {
    if (!g_wsServer || !g_wsRunning.load()) return;

    try {
        if (hdl.expired()) {
            log::warn("[EditorCursor] Attempted to send to expired connection");
            return;
        }

        std::lock_guard lock(g_sendMutex);
        g_wsServer->send(hdl, payload, websocketpp::frame::opcode::text);
    } catch (std::exception const& e) {
        log::error("[EditorCursor] sendToClient exception: {}", e.what());
    } catch (...) {
        log::error("[EditorCursor] sendToClient unknown exception");
    }
}

void startWsServer(int port) {
    if (g_wsRunning.load()) return;

    try {
        g_wsServer = new WSServer();
        g_wsServer->set_access_channels(websocketpp::log::alevel::none);
        g_wsServer->set_error_channels(websocketpp::log::elevel::none);
        g_wsServer->init_asio();
        g_wsServer->set_reuse_addr(true);

        g_wsServer->set_open_handler(bind(&onOpen, _1));
        g_wsServer->set_close_handler(bind(&onClose, _1));
        g_wsServer->set_message_handler([](connection_hdl hdl, WSServer::message_ptr msg) {
            onMessage(hdl, msg, +[](connection_hdl const& h, std::string const& p) {
                sendToClient(h, p);
            });
        });

        g_wsServer->listen(websocketpp::lib::asio::ip::address::from_string("127.0.0.1"), static_cast<uint16_t>(port));
        g_wsServer->start_accept();

        g_wsRunning.store(true);
        g_wsThread = std::thread([]() {
            try {
                if (g_wsServer) {
                    g_wsServer->run();
                }
            } catch (std::exception const& e) {
                log::error("[EditorCursor] WebSocket thread exception: {}", e.what());
            } catch (...) {
                log::error("[EditorCursor] WebSocket thread unknown exception");
            }
            g_wsRunning.store(false);
        });

        log::info("[EditorCursor] WebSocket server listening on 127.0.0.1:{}", port);
    } catch (std::exception const& e) {
        log::error("[EditorCursor] Failed to start WebSocket server: {}", e.what());
        stopWsServer();
    } catch (...) {
        log::error("[EditorCursor] Failed to start WebSocket server: unknown error");
        stopWsServer();
    }
}

void stopWsServer() {
    g_wsRunning.store(false);

    if (g_wsServer) {
        try {
            g_wsServer->stop_listening();
            g_wsServer->stop();
        } catch (...) {
            log::warn("[EditorCursor] Exception while stopping WebSocket server");
        }
    }

    if (g_wsThread.joinable()) {
        g_wsThread.join();
    }

    delete g_wsServer;
    g_wsServer = nullptr;

    {
        std::lock_guard lock(g_actionsMutex);
        g_actions.clear();
    }

    log::info("[EditorCursor] WebSocket server stopped");
}

} // namespace editorcursor
