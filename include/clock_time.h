#pragma once

#include <cstddef>
#include <ctime>

class ClockTime {
 public:
  void begin();
  void formatTime(char* buffer, size_t size) const;
  void formatDate(char* buffer, size_t size) const;
  int second() const;

 private:
  void seedFromCompileTimeIfUnset();
};
