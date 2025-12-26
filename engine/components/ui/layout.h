#pragma once

#include <string>

struct LayoutProperties {
  
  int minW = 0;
  int maxW = 0;  
  int minH = 0;
  int maxH = 0;  

 
  bool fillWidth = false;
  bool fillHeight = false;

  
  std::string justifyContent = "start"; 
  std::string alignItems = "start";     
};

