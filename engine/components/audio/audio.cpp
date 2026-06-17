#include "audio.h"
#include <iostream>
#include <lua.h>
#include <unordered_map>
#include <utility>

#define MINIAUDIO_IMPLEMENTATION
#include "../../../third_party/miniaudio/miniaudio.h"
#include "../../scripting/regsitry.h"

namespace Audio {
  ma_engine engine;
  bool isInitialized = false;

  const int MAX_OVERLAPPING_SOUNDS = 5;

  struct SoundCache {
    ma_sound sourceSound;
    std::vector<ma_sound> instances;
    int currentIndex = 0;
    bool loaded = false;
  };

  std::unordered_map<std::string, SoundCache> soundRegistry;
  
  std::unordered_map<std::string, ma_sound> activeMusicTracks;

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
      for (auto& pair : soundRegistry) {
        if (pair.second.loaded) {
          for (auto& inst : pair.second.instances) {
            ma_sound_uninit(&inst);
          }
          ma_sound_uninit(&pair.second.sourceSound);
        }
      }
      soundRegistry.clear();

      for (auto& pair : activeMusicTracks) {
        ma_sound_uninit(&pair.second);
      }
      activeMusicTracks.clear();

      ma_engine_uninit(&engine);
      isInitialized = false;
    }
  }

  int PlaySound(lua_State *L) {
    if (!isInitialized) return 0;
    
    if (lua_isstring(L, 1)) {
      std::string filepath = lua_tostring(L, 1);
      auto it = soundRegistry.find(filepath);

      if (it == soundRegistry.end()) {
        SoundCache& cache = soundRegistry[filepath];
        ma_result result = ma_sound_init_from_file(
          &engine, filepath.c_str(),
          MA_SOUND_FLAG_DECODE, NULL, NULL, &cache.sourceSound
        );

        if (result == MA_SUCCESS) {
          cache.loaded = true;
          cache.instances.resize(MAX_OVERLAPPING_SOUNDS);

          for (int i = 0; i < MAX_OVERLAPPING_SOUNDS; i++) {
             ma_sound_init_copy(&engine, &cache.sourceSound, 0, NULL, &cache.instances[i]);
          }

          it = soundRegistry.find(filepath);
        } else {
          std::cerr << "[Audio] Failed to load sound into cache: " << filepath << std::endl;
          soundRegistry.erase(filepath);
          return 0;
        }
      }

      if (it->second.loaded) {
        SoundCache& cache = it->second;
        ma_sound* inst = &cache.instances[cache.currentIndex];

        ma_sound_stop(inst);
        ma_sound_seek_to_pcm_frame(inst, 0);
        ma_sound_start(inst);

        cache.currentIndex = (cache.currentIndex + 1) % MAX_OVERLAPPING_SOUNDS;
      }
    } else {
      std::cerr << "[Audio] playSound expects a string filepath." << std::endl;
    }
    return 0;
  }

  int PlayMusic(lua_State *L) {
    if (!isInitialized) return 0;
    
    std::string trackId = "default";
    std::string filepath = "";
    bool loop = true;
    int fadeMs = 0;

    if (lua_isstring(L, 1) && lua_isstring(L, 2)) {
        trackId = lua_tostring(L, 1);
        filepath = lua_tostring(L, 2);
        if (lua_isboolean(L, 3)) loop = lua_toboolean(L, 3);
        if (lua_isnumber(L, 4)) fadeMs = lua_tointeger(L, 4);
    } else if (lua_isstring(L, 1)) {
        filepath = lua_tostring(L, 1);
        if (lua_isboolean(L, 2)) loop = lua_toboolean(L, 2);
        if (lua_isnumber(L, 3)) fadeMs = lua_tointeger(L, 3);
    } else {
        std::cerr << "[Audio] playMusic invalid arguments." << std::endl;
        return 0;
    }

    auto it = activeMusicTracks.find(trackId);
    if (it != activeMusicTracks.end()) {
        ma_sound_stop(&it->second);
        ma_sound_uninit(&it->second);
        activeMusicTracks.erase(it);
    }

    ma_sound* pSound = &activeMusicTracks[trackId];
    ma_result result = ma_sound_init_from_file(&engine, filepath.c_str(), MA_SOUND_FLAG_STREAM, NULL, NULL, pSound);

    if (result == MA_SUCCESS) {
        ma_sound_set_looping(pSound, loop ? MA_TRUE : MA_FALSE);

        if (fadeMs > 0) {
            ma_sound_set_fade_in_milliseconds(pSound, 0.0f, 1.0f, fadeMs);
        }

        ma_sound_start(pSound);
    } else {
        std::cerr << "[Audio] Failed to load music: " << filepath << std::endl;
        activeMusicTracks.erase(trackId);
    }

    return 0;
  }

  int StopMusic(lua_State *L) {
    if (!isInitialized) return 0;
    std::string trackId = "default";
    int fadeMs = 0;

    if (lua_isstring(L, 1)) {
        trackId = lua_tostring(L, 1);
    }
    if (lua_isnumber(L, 2)) {
        fadeMs = lua_tointeger(L, 2);
    }

    auto it = activeMusicTracks.find(trackId);
    if (it != activeMusicTracks.end()) {
        if (fadeMs > 0) {
            float currentVol = ma_sound_get_volume(&it->second);
            ma_sound_set_fade_in_milliseconds(&it->second, currentVol, 0.0f, fadeMs);
        } else {
            ma_sound_stop(&it->second);
            ma_sound_uninit(&it->second);
            activeMusicTracks.erase(it);
        }
    }
    return 0;
  }

  int PauseMusic(lua_State* L) {
    if (!isInitialized) return 0;
    std::string trackId = lua_isstring(L, 1) ? lua_tostring(L, 1) : "default";
    auto it = activeMusicTracks.find(trackId);
    if (it != activeMusicTracks.end()) {
        ma_sound_stop(&it->second);
    }
    return 0;
  }

  int ResumeMusic(lua_State* L) {
    if (!isInitialized) return 0;
    std::string trackId = lua_isstring(L, 1) ? lua_tostring(L, 1) : "default";
    auto it = activeMusicTracks.find(trackId);
    if (it != activeMusicTracks.end()) {
        ma_sound_start(&it->second);
    }
    return 0;
  }

  int SetMasterVolume(lua_State *L) {
    if (!isInitialized) return 0;
    if (lua_isnumber(L, 1)) {
      float volume = (float)lua_tonumber(L, 1);
      ma_engine_set_volume(&engine, volume);
    }
    return 0;
  }

  int SetMusicVolume(lua_State *L) {
    if (!isInitialized) return 0;
    std::string trackId = "default";
    float volume = 1.0f;

    if (lua_isstring(L, 1) && lua_isnumber(L, 2)) {
        trackId = lua_tostring(L, 1);
        volume = (float)lua_tonumber(L, 2);
    } else if (lua_isnumber(L, 1)) {
        volume = (float)lua_tonumber(L, 1);
    }

    auto it = activeMusicTracks.find(trackId);
    if (it != activeMusicTracks.end()) {
        ma_sound_set_volume(&it->second, volume);
    }
    return 0;
  }

  AutoRegisterLua regPlaySound("playSound", PlaySound);
  AutoRegisterLua regPlayMusic("playMusic", PlayMusic);
  AutoRegisterLua regStopMusic("stopMusic", StopMusic); 
  AutoRegisterLua regPauseMusic("pauseMusic", PauseMusic);
  AutoRegisterLua regResumeMusic("resumeMusic", ResumeMusic);
  AutoRegisterLua regSetMasterVolume("setMasterVolume", SetMasterVolume);
  AutoRegisterLua regSetMusicVolume("setMusicVolume", SetMusicVolume);

}
