#ifndef StateMachine_h
#define StateMachine_h

#include "Arduino.h"

// ---- Effect

struct State
{
  State(void (*onEnter)(), void (*onState)(), void (*onExit)());
  void (*onEnter)();
  void (*onState)();
  void (*onExit)();
};

class StateMachine
{
public:
  StateMachine(State *initialState);
  ~StateMachine();

  void addTransition(State* stateFrom, State* stateTo, int event, void (*onTransition)());

  void trigger(int event);
  void runMachine();

private:
  struct Transition
  {
    State* stateFrom;
    State* stateTo;
    int event;
    void (*onTransition)();
  };

  static Transition createTransition(State* stateFrom, State* stateTo, int event, void (*onTransition)());

  void makeTransition(Transition* transition);

private:
  State* mCurrentState;
  Transition* mTransitions;
  int mNumTransitions;

  bool mInitialized;
};

#endif