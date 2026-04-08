#pragma once
#include <lua.hpp>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <map>

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
    static void ProcessQueue(lua_State* L);

    static void FetchAsync(
        const std::string& url, 
        const std::string& method, 
        long timeout, 
        const std::string& body, 
        const std::map<std::string, std::string>& headers, 
        int luaCallbackRef
    );

  private:
    static std::vector<HttpResponse> responseQueue;
    static std::mutex queueMutex;
    static std::atomic<bool> isShuttingDown;
};




