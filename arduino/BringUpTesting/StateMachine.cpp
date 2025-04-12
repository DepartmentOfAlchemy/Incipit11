#include "StateMachine.h"

State::State(void (*onEnter)(), void (*onState)(), void (*onExit)())
: onEnter(onEnter),
  onState(onState),
  onExit(onExit)
{
}

StateMachine::StateMachine(State* initialState)
: mCurrentState(initialState),
  mTransitions(NULL),
  mNumTransitions(0),
  mInitialized(false)
{
}

StateMachine::~StateMachine()
{
  free(mTransitions);
  mTransitions = NULL;
}

void StateMachine::addTransition(State* stateFrom, State* stateTo, int event, void (*onTransition)())
{
  if (stateFrom == NULL || stateTo == NULL)
    return;

  Transition transition = StateMachine::createTransition(stateFrom, stateTo, event, onTransition);
  mTransitions = (Transition*) realloc(mTransitions, (mNumTransitions * 1) * sizeof(Transition));
  mTransitions[mNumTransitions] = transition;
  mNumTransitions++;
}

StateMachine::Transition StateMachine::createTransition(State* stateFrom, State* stateTo, int event, void (*onTransition)())
{
  Transition t;
  t.stateFrom = stateFrom;
  t.stateTo = stateTo;
  t.event = event;
  t.onTransition = onTransition;

  return t;
}

void StateMachine::trigger(int event)
{
  if (mInitialized)
  {
    // Find the transition with the current state and given event
    for (int i = 0; i < mNumTransitions; ++i)
    {
      if (mTransitions[i].stateFrom == mCurrentState &&
          mTransitions[i].event == event)
      {
        StateMachine::makeTransition(&(mTransitions[i]));
        return;
      }
    }
  }
}

void StateMachine::runMachine()
{
  // first run must evec first state "onEnter"
  if (!mInitialized)
  {
    mInitialized = true;
    if (mCurrentState->onEnter != NULL)
      mCurrentState->onEnter();
  }

  if (mCurrentState->onState != NULL)
    mCurrentState->onState();
}

void StateMachine::makeTransition(Transition* transition)
{
  // Execute the handlers in the correct order
  if (transition->stateFrom->onExit != NULL)
    transition->stateFrom->onExit();

  if (transition->onTransition != NULL)
    transition->onTransition();

  if (transition->stateTo->onEnter != NULL)
    transition->stateTo->onEnter();
}