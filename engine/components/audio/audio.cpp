#include "audio.h"
#include <iostream>

#define MINIAUDIO_IMPLEMENTATION
#include "../../../third_party/miniaudio/miniaudio.h"
#include "../../scripting/regsitry.h"

namespace Audio {
  ma_engine engine;
  ma_sound currentMusic;
  bool isInitialized = false;
  bool hasMusic = false;

  void Init() {
    if (ma_engine_init(NULL, &engine) != MA_SUCCESS) {
      std::cerr << "[Audio] Failed to initialize miniaudio engine." << std::endl;
      return;
    }
    isInitialized = true;
    std::cout << "[Audio] Miniaudio engine initialized successfully." << std::endl;
  }

  void ShutDown() {
    if (isInitialized) {
      ma_engine_uninit(&engine);
      isInitialized = false;
    }
  }

  int PlaySound(lua_State *L) {
    if (!isInitialized) {
      return 0;
    }
    if (lua_isstring(L, 1)) {
      const char* filepath = lua_tostring(L, 1);
      ma_engine_play_sound(&engine, filepath, NULL);
    } else {
      std::cerr << "[Audio] playSound expects a string filepath." << std::endl;
    }
    return 0;
  }

  int PlayMusic(lua_State *L) {
    if (!isInitialized) return 0;
    if (lua_isstring(L, 1)) {
      if (hasMusic) {
        ma_engine_uninit(&engine);
      }
      ma_result result = ma_sound_init_from_file(&engine, lua_tostring(L, 1), 0, NULL, NULL, &currentMusic);
      if (result == MA_SUCCESS) {
                ma_sound_start(&currentMusic);
                hasMusic = true;
      }
    }
    return 0;
  }

  int PauseMusic(lua_State* L) {
        if (hasMusic) ma_sound_stop(&currentMusic);
        return 0;
  }

  int ResumeMusic(lua_State* L) {
    if (hasMusic) ma_sound_start(&currentMusic);
    return 0;
  }

  AutoRegisterLua regPlaySound("playSound", PlaySound);
  AutoRegisterLua regPlayMusic("playMusic", PlayMusic);
  AutoRegisterLua regPauseMusic("pauseMusic", PauseMusic);
  AutoRegisterLua regResumeMusic("resumeMusic", ResumeMusic);

}
