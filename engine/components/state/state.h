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
#include <functional>
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

    // Focus manager APIs
    void setFocus(const std::string& key) {
      if (focusKey == key) return;
      focusKey = key;
      dirty = true;
      for (auto &cb : focusListeners) {
        if (cb) cb(focusKey);
      }
    }

    std::string getFocus() const {
      return focusKey;
    }

    // Register a listener for focus changes. Listeners are invoked with the
    // new focus key (may be empty string when focus is cleared).
    void registerFocusListener(std::function<void(const std::string&)> cb) {
      focusListeners.push_back(cb);
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
    std::string focusKey;
    std::vector<std::function<void(const std::string&)>> focusListeners;
};

void registerStateBindings(lua_State* L);


