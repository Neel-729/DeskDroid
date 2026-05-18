#include "stopwatch.h"

namespace {
unsigned long stopwatchStartTime=0;
unsigned long stopwatchElapsed=0;
bool stopwatchRunning=false;
}

namespace StopwatchFeature {

void toggle(unsigned long now){
  stopwatchRunning=!stopwatchRunning;
  if(stopwatchRunning) stopwatchStartTime=now-stopwatchElapsed;
}

void reset(){
  stopwatchRunning=false;
  stopwatchElapsed=0;
}

void update(unsigned long now){
  if(stopwatchRunning) stopwatchElapsed = (unsigned long)(now - stopwatchStartTime);
}

bool isRunning(){
  return stopwatchRunning;
}

unsigned long elapsed(){
  return stopwatchElapsed;
}

}

