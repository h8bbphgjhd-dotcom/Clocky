#pragma once

#include <cstddef>
#include <cstring>
#include <strings.h>

// Shared supported zones for the clock, API, and touchscreen.
struct TzEntry {
  const char* name;
  const char* posix;
};

static const TzEntry kTimezoneTable[] = {
  {"UTC", "UTC0"},
  {"GMT", "GMT0"},
  {"America/Denver", "MST7MDT,M3.2.0,M11.1.0"},
  {"America/Phoenix", "MST7"},
  {"America/Los_Angeles", "PST8PDT,M3.2.0,M11.1.0"},
  {"America/Chicago", "CST6CDT,M3.2.0,M11.1.0"},
  {"America/New_York", "EST5EDT,M3.2.0,M11.1.0"},
  {"America/Anchorage", "AKST9AKDT,M3.2.0,M11.1.0"},
  {"Pacific/Honolulu", "HST10"},
  {"America/Toronto", "EST5EDT,M3.2.0,M11.1.0"},
  {"America/Vancouver", "PST8PDT,M3.2.0,M11.1.0"},
  {"America/Mexico_City", "CST6"},
  {"America/Sao_Paulo", "<-03>3"},
  {"America/Argentina/Buenos_Aires", "<-03>3"},
  {"America/Lima", "PET5"},
  {"America/Bogota", "COT5"},
  {"America/Caracas", "<-04>4"},
  {"Europe/London", "GMT0BST,M3.5.0/1,M10.5.0"},
  {"Europe/Paris", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Berlin", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Rome", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Madrid", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Amsterdam", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Stockholm", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Oslo", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Copenhagen", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Helsinki", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
  {"Europe/Warsaw", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Budapest", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Prague", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Vienna", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Zurich", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Athens", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
  {"Europe/Istanbul", "TRT-3"},
  {"Asia/Dubai", "GST-4"},
  {"Asia/Kolkata", "IST-5:30"},
  {"Asia/Bangkok", "ICT-7"},
  {"Asia/Singapore", "SGT-8"},
  {"Asia/Hong_Kong", "HKT-8"},
  {"Asia/Shanghai", "CST-8"},
  {"Asia/Tokyo", "JST-9"},
  {"Asia/Seoul", "KST-9"},
  {"Australia/Sydney", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
  {"Australia/Melbourne", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
  {"Australia/Brisbane", "AEST-10"},
  {"Australia/Perth", "AWST-8"},
  {"Australia/Adelaide", "ACST-9:30ACDT,M10.1.0,M4.1.0/3"},
  {"Pacific/Auckland", "NZST-12NZDT,M9.5.0,M4.1.0/3"},
  {"Pacific/Fiji", "FJT-12"},
  {"Pacific/Guam", "ChST-10"},
  {"Africa/Cairo", "EET-2EEST,M4.5.5/0,M10.5.4/24"},
  {"Africa/Johannesburg", "SAST-2"},
  {"Africa/Lagos", "WAT-1"},
  {"Africa/Nairobi", "EAT-3"}
};

constexpr size_t kTimezoneCount = sizeof(kTimezoneTable) / sizeof(kTimezoneTable[0]);

inline int timezoneIndex(const char* name) {
  if (!name) return -1;
  for (size_t i = 0; i < kTimezoneCount; ++i) {
    if (strcasecmp(name, kTimezoneTable[i].name) == 0) return static_cast<int>(i);
  }
  return -1;
}
