#include "StateMachine.h"

State::State(void (*enter)(), void (*update)(), void (*exit)())
: enter(enter),
  update(update),
  exit(exit)
{
}

StateMachine::StateMachine()
{
  _state = NULL;
}

StateMachine::~StateMachine()
{
}

void StateMachine::goToState(State *state)
{
  if (_state != NULL) {
    if (_state->exit != NULL)
      _state->exit();
  }

  _state = state;

  if (_state != NULL) {
    if (_state->enter != NULL)
      _state->enter();
  }
}

bool StateMachine::isCurrentState(State *state)
{
  if (state == _state) {
    return true;
  } else {
    return false;
  }
}

void StateMachine::update()
{
  if (_state != NULL) {
    if (_state->update != NULL)
      _state->update();
  }
}
