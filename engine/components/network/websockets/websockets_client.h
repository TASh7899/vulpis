#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <memory>

struct lua_State;

namespace ix { class WebSocket; }

struct WsEvent {
  int callbackRef;
  std::string type; // "open", "message", "error", "close"
  std::string data;
};

class WebSocketClient {
  public:
    static void Init();
    static void ShutDown();

    static bool ProcessQueue(lua_State* L);

    static int Connect(const std::string& url, int luaCallbackRef);
    static bool Send(int connectionId, const std::string& message);
    static void Close(int connectionId);

  private:
    static std::vector<WsEvent> eventQueue;
    static std::mutex queueMutex;
    static int nextConnectionId;

    static std::unordered_map<int, std::shared_ptr<ix::WebSocket>> activeSockets;
};
