#include <SDL2/SDL.h>
#include "http_client.h"
#include <codecvt>
#include <cpr/api.h>
#include <cpr/body.h>
#include <cpr/session.h>
#include <cpr/status_codes.h>
#include <lauxlib.h>
#include <lua.h>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <iostream>
#include <cpr/cpr.h>
#include <utility>
#include <vector>
#include <map>
#include <algorithm>
#include <filesystem>

#include "../../scripting/regsitry.h"

std::vector<HttpResponse> HttpClient::responseQueue;
std::mutex HttpClient::queueMutex;
std::atomic<bool> HttpClient::isShuttingDown(false);

void HttpClient::Init() {
  isShuttingDown = false;
}

void HttpClient::ShutDown() {
  isShuttingDown = true;
}


void HttpClient::FetchAsync(const std::string &url, const std::string &method, long timeout, const std::string &body,
    const std::map<std::string, std::string> &headers, int luaCallbackRef) {
  std::thread([url, method, timeout, body, headers, luaCallbackRef]() {
    cpr::Session session;
    session.SetUrl(cpr::Url{url});
    session.SetTimeout(std::chrono::milliseconds(timeout));

#if defined (__linux__)
    const char* certPaths[] = {
    "/etc/ssl/certs/ca-certificates.crt",
    "/etc/pki/tls/certs/ca-bundle.crt",
    "/etc/ssl/ca-bundle.pem"
    };
    for (const char* cert : certPaths) {
    if (std::filesystem::exists(cert)) {
    session.SetOption(cpr::Ssl(cpr::ssl::CaInfo{cert}));
    break;
    }
    }
#endif

    if (!body.empty()) {
      session.SetBody(cpr::Body{body});
    }

    cpr::Header cprHeaders;
    for (const auto& kv : headers) {
      cprHeaders[kv.first] = kv.second;
    }

    if (!body.empty() && cprHeaders.find("Content-Type") == cprHeaders.end()) {
      cprHeaders["Content-Type"] = "application/json";
    }

    if (!cprHeaders.empty()) {
      session.SetHeader(cprHeaders);
    }

    cpr::Response r;
    if (method == "POST") r = session.Post();
    else if (method == "PUT") r = session.Put();
    else if (method == "DELETE") r = session.Delete();
    else if (method == "PATCH") r = session.Patch();
    else r = session.Get();

    if (isShuttingDown) return;

    HttpResponse res;
    res.statusCode = r.status_code;
    res.body = r.text;
    res.error = r.error.message;
    res.luaCallbackRef = luaCallbackRef;

    std::lock_guard<std::mutex> lock(queueMutex);
    responseQueue.push_back(res);
    SDL_Event s_event;
    SDL_zero(s_event);
    s_event.type = SDL_USEREVENT;
    SDL_PushEvent(&s_event);
    }).detach();
}


bool HttpClient::ProcessQueue(lua_State *L) {
  std::vector<HttpResponse> localQueue;

  {
    std::lock_guard<std::mutex> lock(queueMutex);
    if (responseQueue.empty()) {
      return false;
    }
    localQueue = std::move(responseQueue);
    responseQueue.clear();
  }

  for (const auto& res : localQueue) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, res.luaCallbackRef);
    lua_newtable(L);
    lua_pushinteger(L, res.statusCode);
    lua_setfield(L, -2, "status");

    lua_pushstring(L, res.body.c_str());
    lua_setfield(L, -2, "body");

    lua_pushstring(L, res.error.c_str());
    lua_setfield(L, -2, "error");

    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
      std::cerr << "[Net Error] Lua Callback failed: " << lua_tostring(L, -1) << std::endl;
      lua_pop(L, 1);
    }

    luaL_unref(L, LUA_REGISTRYINDEX, res.luaCallbackRef);
  }

  return true;
}

int Lua_Fetch(lua_State* L) {
  std::string url = luaL_checkstring(L, 1);

  std::string method = "GET";
  long timeout = 10000; // 10 second default timeout
  std::string body = "";
  std::map<std::string, std::string> headers;

  int callbackIndex = 2;
  
  if (lua_istable(L, 2)) {
    callbackIndex = 3;

    lua_getfield(L, 2, "method");
    if (lua_isstring(L, -1)) {
      method = lua_tostring(L, -1);
      std::transform(method.begin(), method.end(), method.begin(), ::toupper);
    }
    lua_pop(L, 1);

    lua_getfield(L, 2, "timeout");
    if (lua_isnumber(L, -1)) timeout = lua_tointeger(L, -1);
    lua_pop(L, 1);

    // Parse body
    lua_getfield(L, 2, "body");
    if (lua_isstring(L, -1)) body = lua_tostring(L, -1);
    lua_pop(L, 1);

    // Parse headers table
    lua_getfield(L, 2, "headers");
    if (lua_istable(L, -1)) {
      lua_pushnil(L);
      while (lua_next(L, -2) != 0) {
        if (lua_type(L, -2) == LUA_TSTRING && lua_type(L, -1) == LUA_TSTRING) {
          headers[lua_tostring(L, -2)] = lua_tostring(L, -1);
        }
        lua_pop(L, 1);
      }
    }
    lua_pop(L, 1);
  }

  luaL_checktype(L, callbackIndex, LUA_TFUNCTION);
  lua_pushvalue(L, callbackIndex);
  int callbackRef = luaL_ref(L, LUA_REGISTRYINDEX);

  HttpClient::FetchAsync(url, method, timeout, body, headers, callbackRef);
  return 0;
}

AutoRegisterLua autoRegFetch("fetch", Lua_Fetch);


