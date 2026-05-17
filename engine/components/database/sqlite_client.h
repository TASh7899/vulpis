#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <lua.hpp>

struct SqlResult {
  int callbackRef;
  bool success;
  std::string error;
  std::vector<std::unordered_map<std::string, std::string>> rows;
  int rowsAffected;
  long long lastInsertRowId;
};

struct SqlTask {
  std::string query;
  int callbackRef;
};

struct sqlite3;

class SqliteClient {
  public:
    static bool Init(const std::string& dbFilename);
    static void ShutDown();
    static bool ProcessQueue(lua_State* L);
    static void ExecuteAsync(const std::string& query, int luaCallbackRef);

  private:
    static sqlite3* db;
    static std::atomic<bool> isShuttingDown;

    static std::thread workerThread;
    static std::mutex taskMutex;
    static std::condition_variable taskCV;
    static std::queue<SqlTask> taskQueue;

    static std::mutex resultMutex;
    static std::vector<SqlResult> resultQueue;

    static void WorkerLoop();
};
