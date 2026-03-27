#include "stats_logger.h"
#include <iostream>
#include <string>
#include <cstring>
#include <thread>

#if defined(_WIN32)
    #include <windows.h>
    #include <psapi.h>
#elif defined(__APPLE__)
    #include <mach/mach.h>
#elif defined(__linux__)
#include <unistd.h>
#include <cstdio>
#endif

namespace Vulpis {
  namespace Tools {
    StatsLogger::StatsLogger(const std::string& filename) {
      file.open(filename);
      if (file.is_open()) {
        file << "Time(ms),DeltaTime(s),FPS,RAM(MB),ScriptTime(ms),LayoutTime(ms),RenderTime(ms),TotalCPULoad(%)\n";
      }
      buffer.reserve(1000);
    }

    StatsLogger::~StatsLogger() {
      flush();
      if (file.is_open()) file.close();
    }

    void StatsLogger::log(uint32_t time, float dt, double scriptMs, double layoutMs, double renderMs) {      // Static variables to throttle the expensive OS-level RAM check
      static int frameCounter = 0;
      static double lastRamCache = 0.0;

      // Only query the OS once every 60 frames (~1 second at 60fps)
      if (frameCounter % 60 == 0) {
        lastRamCache = GetCurrentRAMUsageMB();
      }
      frameCounter++;

      buffer.push_back({time, dt, lastRamCache, layoutMs, renderMs});

      if (buffer.size() >= 1000) {
        flush();
      }
    }

    void StatsLogger::flush() {
      if (!file.is_open() || buffer.empty()) return;

      // Portable detection of logical cores
      unsigned int n = std::thread::hardware_concurrency();
      const double numCores = (n > 0) ? static_cast<double>(n) : 1.0; 

      for (const auto& stat : buffer) {
        float fps = stat.dt > 0.0f ? (1.0f / stat.dt) : 0.0f;

        double totalCpuLoad = 0.0;
        if (stat.dt > 0.0f) {
          totalCpuLoad = ((stat.scriptTimeMs + stat.layoutTimeMs + stat.renderTimeMs) / (stat.dt * 1000.0)) / numCores * 100.0;
        }

        file << stat.timestamp << "," 
          << stat.dt << "," 
          << fps << "," 
          << stat.ramMB << "," 
          << stat.scriptTimeMs << ","
          << stat.layoutTimeMs << ","
          << stat.renderTimeMs << ","
          << totalCpuLoad << "\n";
      }
      buffer.clear();
    }

    double StatsLogger::GetCurrentRAMUsageMB() {
#if defined(_WIN32)
      PROCESS_MEMORY_COUNTERS_EX pmc;
      if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        return (double)pmc.WorkingSetSize / (1024.0 * 1024.0);
      }
      return 0.0;

#elif defined(__APPLE__)
      struct mach_task_basic_info info;
      mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
      if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &infoCount) != KERN_SUCCESS) {
        return 0.0;
      }
      return (double)info.resident_size / (1024.0 * 1024.0);

#elif defined(__linux__)
      FILE* fp = fopen("/proc/self/status", "r");
      if (!fp) return 0.0;

      char line[128];
      long rss_kb = 0;

      // Scan line by line until we find the "VmRSS:" tag
      while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
          // Parse the integer value right after the tag
          sscanf(line + 6, "%ld", &rss_kb);
          break;
        }
      }
      fclose(fp);

      // VmRSS is explicitly reported in Kilobytes (kB). Divide by 1024 for MB.
      return (double)rss_kb / 1024.0;

#else
      return 0.0;
#endif
    }

  }
}
