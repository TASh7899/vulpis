#include <SDL2/SDL.h>
#include "websockets_client.h"
#include <IXWebSocketMessage.h>
#include <iostream>
#include <iterator>
#include <lua.hpp>
#include <ixwebsocket/IXWebSocket.h>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "../../../scripting/regsitry.h"

std::vector<WsEvent> WebSocketClient::eventQueue;
std::mutex WebSocketClient::queueMutex;
std::unordered_map<int, std::shared_ptr<ix::WebSocket>> WebSocketClient::activeSockets;
int WebSocketClient::nextConnectionId = 1;

void WebSocketClient::Init() {}

void WebSocketClient::ShutDown() {
  for (auto pair : activeSockets) {
    pair.second->stop();
  }
  activeSockets.clear();
}

int WebSocketClient::Connect(const std::string &url, int luaCallbackRef) {
  int connId = nextConnectionId++;
  auto webSocket = std::make_shared<ix::WebSocket>();
  webSocket->setUrl(url);

  webSocket->setOnMessageCallback([connId, luaCallbackRef](const ix::WebSocketMessagePtr& msg) {
    WsEvent ev;
    ev.callbackRef = luaCallbackRef;

    if (msg->type == ix::WebSocketMessageType::Open) {
      ev.type = "open";
      ev.data = "Connected";
    } 
    else if (msg->type == ix::WebSocketMessageType::Message) {
      ev.type = "message";
      ev.data = msg->str;
    } 
    else if (msg->type == ix::WebSocketMessageType::Error) {
      ev.type = "error";
      ev.data = msg->errorInfo.reason;
    } 
    else if (msg->type == ix::WebSocketMessageType::Close) {
      ev.type = "close";
      ev.data = "Closed";
    }

    if (!ev.type.empty()) {
      std::lock_guard<std::mutex> lock(queueMutex);
      eventQueue.push_back(ev);
      SDL_Event s_event;
      SDL_zero(s_event);
      s_event.type = SDL_USEREVENT;
      SDL_PushEvent(&s_event);
    }
  });

  webSocket->start();
  activeSockets[connId] = webSocket;

  return connId;
}

bool WebSocketClient::Send(int connectionId, const std::string& message) {
    auto it = activeSockets.find(connectionId);
    if (it != activeSockets.end()) {
        it->second->send(message);
        return true;
    }
    return false;
}

void WebSocketClient::Close(int connectionId) {
    auto it = activeSockets.find(connectionId);
    if (it != activeSockets.end()) {
        it->second->stop();
        activeSockets.erase(it);
    }
}

bool WebSocketClient::ProcessQueue(lua_State *L) {
  std::vector<WsEvent> localQueue;
  {
    std::lock_guard<std::mutex> lock(queueMutex);
    if (eventQueue.empty()) return false;
    localQueue = std::move(eventQueue);
    eventQueue.clear();
  }

  for (const auto& ev : localQueue) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, ev.callbackRef);

    if (!lua_isfunction(L, -1)) {
      lua_pop(L, 1);
      continue;
    }

    lua_newtable(L);
    lua_pushstring(L, ev.type.c_str());
    lua_setfield(L, -2, "type");

    lua_pushstring(L, ev.data.c_str());
    lua_setfield(L, -2, "data");

    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
      std::cerr << "[WS Error] Lua Callback failed: " << lua_tostring(L, -1) << std::endl;
      lua_pop(L, 1);
    }

    if (ev.type == "close") {
      luaL_unref(L, LUA_REGISTRYINDEX, ev.callbackRef);
    }
  }
  return true;
}

int l_wsConnect(lua_State* L) {
  std::string url = luaL_checkstring(L, 1);
  luaL_checktype(L, 2, LUA_TFUNCTION);
  lua_pushvalue(L, 2);
  int callbackRef = luaL_ref(L, LUA_REGISTRYINDEX);

  int id = WebSocketClient::Connect(url, callbackRef);
  lua_pushinteger(L, id);
  return 1;
}

int l_wsSend(lua_State* L) {
  int id = luaL_checkinteger(L, 1);
  std::string msg = luaL_checkstring(L, 2);
  bool success = WebSocketClient::Send(id, msg);
  lua_pushboolean(L, success);
  return 1;
}

int l_wsClose(lua_State* L) {
  int id = luaL_checkinteger(L, 1);
  WebSocketClient::Close(id);
  return 0;
}

AutoRegisterLua regWsConnect("wsConnect", l_wsConnect);
AutoRegisterLua regWsSend("wsSend", l_wsSend);
AutoRegisterLua regWsClose("wsClose", l_wsClose);


