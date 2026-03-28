#include "http_client.h"
#include <codecvt>
#include <cpr/api.h>
#include <cpr/body.h>
#include <cpr/status_codes.h>
#include <lauxlib.h>
#include <lua.h>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <iostream>
#include <cpr/cpr.h>
#include <utility>
#include <vector>

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

void HttpClient::GetAsync(const std::string &url, int luaCallbackRef) {
  std::thread([url, luaCallbackRef]() {
    cpr::Response r = cpr::Get(cpr::Url{url});
    if (isShuttingDown) return;
    HttpResponse res;
    res.statusCode = r.status_code;
    res.body = r.text;
    res.error = r.error.message;
    res.luaCallbackRef= luaCallbackRef;

    std::lock_guard<std::mutex> lock(queueMutex);
    responseQueue.push_back(res);
  }).detach();
}

void HttpClient::ProcessQueue(lua_State *L) {
  std::vector<HttpResponse> localQueue;

  {
    std::lock_guard<std::mutex> lock(queueMutex);
    if (responseQueue.empty()) {
      return;
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

}

int Lua_HttpGet(lua_State* L) {
  std::string url = luaL_checkstring(L, 1);
  luaL_checktype(L, 2, LUA_TFUNCTION);

  lua_pushvalue(L, 2);
  int callbackRef = luaL_ref(L, LUA_REGISTRYINDEX);
  
  HttpClient::GetAsync(url, callbackRef);
  return 0;
}
AutoRegisterLua autoRegHttpGet("httpGet", Lua_HttpGet);

void HttpClient::PostAsync(const std::string &url, const std::string &payload, int luaCallbackRef) {
  std::thread([url, payload, luaCallbackRef]() {
    cpr::Response r = cpr::Post(cpr::Url{url}, cpr::Body{payload}, cpr::Header{{"Content-Type", "application/json"}});

    if (isShuttingDown) return;

    HttpResponse res;
    res.statusCode = r.status_code;
    res.body = r.text;
    res.error = r.error.message;
    res.luaCallbackRef = luaCallbackRef;

    std::lock_guard<std::mutex> lock(queueMutex);
    responseQueue.push_back(res);

  }).detach();
}

int Lua_HttpPost(lua_State* L) {
  std::string url = luaL_checkstring(L, 1);
  std::string payload = luaL_checkstring(L, 2);
  luaL_checktype(L, 3, LUA_TFUNCTION);

  lua_pushvalue(L, 3);
  int callbackRef = luaL_ref(L, LUA_REGISTRYINDEX);

  HttpClient::PostAsync(url, payload, callbackRef);
  return 0;
}
AutoRegisterLua autoRegHttpPost("httpPost", Lua_HttpPost);


