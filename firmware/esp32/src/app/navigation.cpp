#include "navigation.h"

#include "navigation_stack.h"
#include "../core/logging.h"

namespace {

AppState nextState = STATE_CLOCK;
bool transitionPending = false;

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

bool isMainState(AppState state) {
    for (uint8_t i = 0; i < MAIN_STATE_COUNT; i++) {
        if (MAIN_STATES[i] == state) return true;
    }
    return false;
}

}

namespace AppNavigation {

AppState current(){
  return NavigationStack::peek();
}

AppState resumeAfterReminder(){
  return reminderResumeState;
}

void enter(AppState newState){
  if(current() != newState){
    nextState = newState;
    transitionPending = true;
  }
}

void setResumeAfterReminder(AppState state){
  reminderResumeState = state;
}

void rotateMainState(int step){
  int idx = mainStateIndex(current()) + step;
  if(idx < 0) idx = MAIN_STATE_COUNT - 1;
  if(idx >= MAIN_STATE_COUNT) idx = 0;
  nextState = MAIN_STATES[idx];
  transitionPending = true;
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

void commit(){
    if (!transitionPending) return;

    if (isMainState(nextState) && isMainState(current())) {
        NavigationStack::replace(nextState);
    } else if (nextState == NavigationStack::peek()) {
        NavigationStack::pop();
    } else {
        NavigationStack::push(nextState);
    }

    transitionPending = false;
    stateChanged = true;
}

void back(){
  if(!NavigationStack::isAtHome()){
    nextState = NavigationStack::peek(); 
    transitionPending = true;
  }
}

void goHome(){
  NavigationStack::reset();
  stateChanged = true;
  LOG_INFO(LogTag::APP, "[NAV] Going home");
}

bool isAtHome(){
  return NavigationStack::isAtHome();
}

uint8_t stackDepth(){
  return NavigationStack::depth();
}

}