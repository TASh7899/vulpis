#include "audio.h"
#include <iostream>
#include <lua.h>
#include <unordered_map>
#include <utility>
#include <vector>
#include <zip.h>
#include <fstream>

#define MINIAUDIO_IMPLEMENTATION
#include "../../../third_party/miniaudio/miniaudio.h"
#include "../../scripting/regsitry.h"
#include "../../components/system/pathUtils.h"

namespace Audio {
  ma_engine engine;
  bool isInitialized = false;

  const int MAX_OVERLAPPING_SOUNDS = 5;

  static std::vector<unsigned char> ReadAudioFile(const std::string& path) {
    std::vector<unsigned char> buffer;
    std::string relativePath = path;
    if (relativePath.find("assets/") == 0) relativePath = relativePath.substr(7);
    else if (relativePath.find("./assets/") == 0) relativePath = relativePath.substr(9);

    std::string vpakPath = Vulpis::getProjectRoot() + "app.vpak";
    int zipErr = 0;
    zip_t* vpak = zip_open(vpakPath.c_str(), ZIP_RDONLY, &zipErr);

    if (vpak) {
      std::string vfsPath = "assets/" + relativePath;
      zip_stat_t stat;
      if (zip_stat(vpak, vfsPath.c_str(), 0, &stat) == 0) {
        zip_file_t* f = zip_fopen(vpak, vfsPath.c_str(), 0);
        if (f) {
          buffer.resize(stat.size);
          zip_fread(f, buffer.data(), stat.size);
          zip_fclose(f);
        }
      }
      zip_close(vpak);
    }

    if (buffer.empty()) {
      std::string fullPath = Vulpis::getAssetPath(relativePath);
      std::ifstream file(fullPath, std::ios::binary | std::ios::ate);
      if (file) {
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        buffer.resize(size);
        file.read(reinterpret_cast<char*>(buffer.data()), size);
      }
    }
    return buffer;
  }

  struct SoundCache {
    std::vector<unsigned char> buffer;
    std::vector<ma_decoder> decoders;
    std::vector<ma_sound> instances;
    int currentIndex = 0;
    bool loaded = false;
  };

  struct MusicTrack {
    std::vector<unsigned char> buffer;
    ma_decoder decoder;
    ma_sound sound;
    bool loaded = false;
  };

  std::unordered_map<std::string, SoundCache> soundRegistry;
  std::unordered_map<std::string, MusicTrack> activeMusicTracks;

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
          for (int i = 0; i < MAX_OVERLAPPING_SOUNDS; i++) {
            ma_sound_uninit(&pair.second.instances[i]);
            ma_decoder_uninit(&pair.second.decoders[i]);
          }
        }
      }
      soundRegistry.clear();

      for (auto& pair : activeMusicTracks) {
        if (pair.second.loaded) {
          ma_sound_uninit(&pair.second.sound);
          ma_decoder_uninit(&pair.second.decoder);
        }
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
        cache.buffer = ReadAudioFile(filepath);

        if (!cache.buffer.empty()) {
          cache.loaded = true;
          cache.instances.resize(MAX_OVERLAPPING_SOUNDS);
          cache.decoders.resize(MAX_OVERLAPPING_SOUNDS);

          for (int i = 0; i < MAX_OVERLAPPING_SOUNDS; i++) {
            ma_result dr = ma_decoder_init_memory(cache.buffer.data(), cache.buffer.size(), NULL, &cache.decoders[i]);
            if (dr == MA_SUCCESS) {
              ma_sound_init_from_data_source(&engine, &cache.decoders[i], 0, NULL, &cache.instances[i]);
            }
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
      ma_sound_stop(&it->second.sound);
      ma_sound_uninit(&it->second.sound);
      ma_decoder_uninit(&it->second.decoder);
      activeMusicTracks.erase(it);
    }

    MusicTrack& track = activeMusicTracks[trackId];
    track.buffer = ReadAudioFile(filepath);

    if (!track.buffer.empty()) {
      ma_result dr = ma_decoder_init_memory(track.buffer.data(), track.buffer.size(), NULL, &track.decoder);
      if (dr == MA_SUCCESS) {
        ma_result result = ma_sound_init_from_data_source(&engine, &track.decoder, MA_SOUND_FLAG_STREAM, NULL, &track.sound);
        if (result == MA_SUCCESS) {
          track.loaded = true;
          ma_sound_set_looping(&track.sound, loop ? MA_TRUE : MA_FALSE);

          if (fadeMs > 0) {
            ma_sound_set_fade_in_milliseconds(&track.sound, 0.0f, 1.0f, fadeMs);
          }

          ma_sound_start(&track.sound);
        } else {
          std::cerr << "[Audio] Failed to init sound from data source: " << filepath << std::endl;
          activeMusicTracks.erase(trackId);
        }
      } else {
        std::cerr << "[Audio] Failed to decode music: " << filepath << std::endl;
        activeMusicTracks.erase(trackId);
      }
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
        float currentVol = ma_sound_get_volume(&it->second.sound);
        ma_sound_set_fade_in_milliseconds(&it->second.sound, currentVol, 0.0f, fadeMs);
      } else {
        ma_sound_stop(&it->second.sound);
        ma_sound_uninit(&it->second.sound);
        ma_decoder_uninit(&it->second.decoder);
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
      ma_sound_stop(&it->second.sound);
    }
    return 0;
  }

  int ResumeMusic(lua_State* L) {
    if (!isInitialized) return 0;
    std::string trackId = lua_isstring(L, 1) ? lua_tostring(L, 1) : "default";
    auto it = activeMusicTracks.find(trackId);
    if (it != activeMusicTracks.end()) {
      ma_sound_start(&it->second.sound);
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
      ma_sound_set_volume(&it->second.sound, volume);
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
