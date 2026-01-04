#pragma once

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


