#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <cstdint>

namespace Vulpis {
  namespace Tools {
    struct FrameStat {
      uint32_t timestamp;
      float dt;
      double ramMB;
      double layoutTimeMs;
      double renderTimeMs;
    };

    class StatsLogger {
      public:
        StatsLogger(const std::string& filename);
        ~StatsLogger();

        void log(uint32_t time, float dt, double layoutMs, double renderMs);

        static double GetCurrentRAMUsageMB();

      private:
        void flush();
        std::ofstream file;
        std::vector<FrameStat> buffer;
    };

  }
}
