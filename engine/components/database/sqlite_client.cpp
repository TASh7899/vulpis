#include "sqlite_client.h"
#include <algorithm>
#include <exception>
#include <lauxlib.h>
#include <lua.h>
#include <memory>
#include <mutex>
#include <sqlite3.h>
#include <iostream>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include "../../scripting/regsitry.h"
#include "../../components/system/pathUtils.h"

sqlite3* SqliteClient::db = nullptr;
std::atomic<bool> SqliteClient::isShuttingDown(false);
std::thread SqliteClient::workerThread;
std::mutex SqliteClient::taskMutex;
std::condition_variable SqliteClient::taskCV;
std::queue<SqlTask> SqliteClient::taskQueue;
std::mutex SqliteClient::resultMutex;
std::vector<SqlResult> SqliteClient::resultQueue;

bool SqliteClient::Init(const std::string &dbFilename) {
  std::string fullPath = (Vulpis::getCacheDirectory()/dbFilename).string();

  if (sqlite3_open(fullPath.c_str(), &db) != SQLITE_OK) {
    std::cerr << "[SQLite Error] Cannot open database: " << sqlite3_errmsg(db) << std::endl;
    return false;
  }

  sqlite3_exec(db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
  sqlite3_exec(db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);

  isShuttingDown = false;
  workerThread = std::thread(WorkerLoop);
  return true;
}

void SqliteClient::ShutDown() {
  isShuttingDown = true;
  taskCV.notify_one();
  if (workerThread.joinable()) {
    workerThread.join();
  }
  if (db) {
    sqlite3_close(db);
    db = nullptr;
  }
}

void SqliteClient::WorkerLoop() {
  while (true) {
    SqlTask task;
    {
      std::unique_lock<std::mutex> lock(taskMutex);
      taskCV.wait(lock, []{ return !taskQueue.empty() || isShuttingDown; });
      if (isShuttingDown && taskQueue.empty()) {
        break;
      }

      task = taskQueue.front();
      taskQueue.pop();
    }
    SqlResult result;
    result.callbackRef = task.callbackRef;
    result.success = true;

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, task.query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
      result.success = false;
      result.error = sqlite3_errmsg(db);
    } else {
      int cols = sqlite3_column_count(stmt);
      while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::unordered_map<std::string, std::string> row;
        for (int i = 0; i < cols; i++) {
          const char* colName = sqlite3_column_name(stmt, i);
          const char* colText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
          row[colName] = colText ? colText : "";
        }
        result.rows.push_back(std::move(row));
      }
      result.rowsAffected = sqlite3_changes(db);
      result.lastInsertRowId = sqlite3_last_insert_rowid(db);
      sqlite3_finalize(stmt);
    }

    std::lock_guard<std::mutex> lock(resultMutex);
    resultQueue.push_back(std::move(result));
  }
}

void SqliteClient::ExecuteAsync(const std::string &query, int luaCallbackRef) {
  std::lock_guard<std::mutex> lock(taskMutex);
  taskQueue.push({query, luaCallbackRef});
  taskCV.notify_one();
}

bool SqliteClient::ProcessQueue(lua_State* L) {
  std::vector<SqlResult> localQueue;
  {
    std::lock_guard<std::mutex> lock(resultMutex);
    if (resultQueue.empty()) return false;
    localQueue = std::move(resultQueue);
    resultQueue.clear();
  }

  for (const auto& res : localQueue) {
    if (res.callbackRef != LUA_NOREF && res.callbackRef != LUA_REFNIL) {
      lua_rawgeti(L, LUA_REGISTRYINDEX, res.callbackRef);
      
      if (lua_isfunction(L, -1)) {
        lua_newtable(L);

        lua_pushboolean(L, res.success);
        lua_setfield(L, -2, "success");

        if (!res.success) {
          lua_pushstring(L, res.error.c_str());
          lua_setfield(L, -2, "error");
        }

        lua_pushinteger(L, res.rowsAffected);
        lua_setfield(L, -2, "rowsAffected");

        lua_pushinteger(L, res.lastInsertRowId);
        lua_setfield(L, -2, "lastInsertRowId");

        lua_newtable(L);
        for (size_t i = 0; i < res.rows.size(); ++i) {
          lua_newtable(L);
          for (const auto& [key, val] : res.rows[i]) {
            lua_pushstring(L, val.c_str());
            lua_setfield(L, -2, key.c_str());
          }
          lua_rawseti(L, -2, i + 1);
        }
        lua_setfield(L, -2, "rows");

        if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
          std::cerr << "[SQLite Error] Lua Callback failed: " << lua_tostring(L, -1) << std::endl;
          lua_pop(L, 1);
        }
      } else {
        lua_pop(L, -1);
      }
      luaL_unref(L, LUA_REGISTRYINDEX, res.callbackRef);
    }
  }
  return true;
}

int l_dbQuery(lua_State* L) {
    std::string query = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2);
    int callbackRef = luaL_ref(L, LUA_REGISTRYINDEX);

    SqliteClient::ExecuteAsync(query, callbackRef);
    return 0;
}

AutoRegisterLua regDbQuery("dbQuery", l_dbQuery);

