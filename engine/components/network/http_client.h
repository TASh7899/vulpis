#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <lua.hpp>

struct HttpResponse {
  int statusCode;
  std::string body;
  std::string error;
  int luaCallbackRef;
};

class HttpClient {
  public:
    static void Init();
    static void ShutDown();
    static void GetAsync(const std::string& url, int luaCallbackRef);
    static void PostAsync(const std::string& url, const std::string& payload, int luaCallbackRef);
    static void ProcessQueue(lua_State* L);

  private:
    static std::vector<HttpResponse> responseQueue;
    static std::mutex queueMutex;
    static std::atomic<bool> isShuttingDown;
};




