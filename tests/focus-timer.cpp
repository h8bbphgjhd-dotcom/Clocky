#include "focus_timer.h"
#include <cassert>
#include <iostream>
int main() {
  FocusTimer timer;
  assert(timer.snapshot(0).remainingSeconds == 1500);
  timer.start(1500, 100);
  assert(timer.snapshot(1100).remainingSeconds == 1499);
  timer.pause(1600);
  assert(timer.snapshot(100000).remainingSeconds == 1499);
  timer.resume(100000);
  assert(timer.snapshot(100500).remainingSeconds == 1498);
  timer.reset(100500);
  assert(!timer.snapshot(100500).running);
  timer.start(60, 0xfffffff0);
  assert(timer.snapshot(984).remainingSeconds == 59); // millis rollover
  auto completed = timer.snapshot(59984);
  assert(completed.completed && !completed.running && completed.remainingSeconds == 0);
  assert(completed.completedSessions == 1);
  assert(timer.snapshot(100000).completedSessions == 1);
  timer.resume(100000);
  assert(!timer.snapshot(100000).running);
  timer.start(0, 100000); // invalid requests cannot replace the timer
  assert(timer.snapshot(100000).durationSeconds == 60);
  timer.start(5401, 100000);
  assert(timer.snapshot(100000).durationSeconds == 60);
  timer.start(3000, 100000);
  assert(timer.snapshot(100000).remainingSeconds == 3000);
  std::cout << "PASS: pause/resume, completion, reset, bounds, and millis rollover\n";
}
