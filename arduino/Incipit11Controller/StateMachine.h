#ifndef StateMachine_h
#define StateMachine_h

#include "Arduino.h"

// ---- Effect

struct State
{
  State(void (*enter)(), void (*update)(), void (*exit)());
  void (*enter)();
  void (*update)();
  void (*exit)();
};

class StateMachine
{
public:
  StateMachine();
  ~StateMachine();

  void goToState(State* state);
  bool isCurrentState(State* state);

  void update();

private:
  State* _state;
};

#endif