#pragma once
#ifndef _WIN32
#include <endian.h>
#else
// Windows doesn't have endian.h, but we can define what we need or skip it 
// if it's not actually used for critical types in this header.
#endif
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include "../../lua.hpp"

using StateValue = std::variant<int, float, std::string, bool>;

class StateManager {
  public:
    static StateManager& instance() {
      static StateManager instance;
      return instance;
    }

    StateValue getState(const std::string key, StateValue defaultValue) {
      if (store.find(key) == store.end()) {
        store[key] = defaultValue;
      }
      return store[key];
    }

    void setState(const std::string& key, StateValue value) {
      store[key] = value;
      dirty = true;
    }

    bool isDirty() const {
      return dirty;
    }

    void clearDirty() {
      dirty = false;
    }

  private:
    std::unordered_map<std::string, StateValue> store;
    bool dirty = false;
};

void registerStateBindings(lua_State* L);


