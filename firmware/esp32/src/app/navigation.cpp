#include "navigation.h"

namespace {

AppState currentState = STATE_CLOCK;
AppState reminderResumeState = STATE_CLOCK;
bool stateChanged = true;

const AppState MAIN_STATES[] = {
  STATE_CLOCK,
  STATE_TIMER_VIEW,
  STATE_STOPWATCH,
  STATE_REMINDER_HOME,
  STATE_SETTINGS_HOME
};

const uint8_t MAIN_STATE_COUNT = sizeof(MAIN_STATES) / sizeof(MAIN_STATES[0]);

int mainStateIndex(AppState state){
  for(uint8_t i=0;i<MAIN_STATE_COUNT;i++){
    if(MAIN_STATES[i] == state) return i;
  }
  return 0;
}

}

namespace AppNavigation {

AppState current(){
  return currentState;
}

AppState resumeAfterReminder(){
  return reminderResumeState;
}

void enter(AppState nextState){
  if(currentState != nextState){
    currentState = nextState;
  }
  stateChanged = true;
}

void setResumeAfterReminder(AppState state){
  reminderResumeState = state;
}

void rotateMainState(int step){
  int idx = mainStateIndex(currentState) + step;
  if(idx < 0) idx = MAIN_STATE_COUNT - 1;
  if(idx >= MAIN_STATE_COUNT) idx = 0;
  enter(MAIN_STATES[idx]);
}

bool hasStateChanged(){
  return stateChanged;
}

void clearStateChanged(){
  stateChanged = false;
}

void markChanged(){
  stateChanged = true;
}

}
