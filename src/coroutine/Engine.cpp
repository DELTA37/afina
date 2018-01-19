#include <afina/coroutine/Engine.h>

#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>

namespace Afina {
namespace Coroutine {

void Engine::Store(context &ctx) {
  char StackEndsHere;
  ctx.Low = std::min(&StackEndsHere, this->StackBottom);
  ctx.High = std::max(&StackEndsHere, this->StackBottom);
  
  ptrdiff_t l = ctx.High - ctx.Low;
  if (l > std::get<1>(ctx.Stack)) {
    delete[] std::get<0>(ctx.Stack);
    std::get<0>(ctx.Stack) = new char[l];
  }
 
  std::get<1>(ctx.Stack) = l;
  memcpy(std::get<0>(ctx.Stack), ctx.Low, l);
}

void Engine::Restore(context &ctx) {
  char StackEndsHere;
  if ((ctx.Low < &StackEndsHere) && (&StackEndsHere < ctx.High)) {
    this->Restore(ctx);
  }

  memcpy(ctx.Low, std::get<0>(ctx.Stack), std::get<1>(ctx.Stack));
  longjmp(ctx.Environment, 1); 
}


void Engine::yield(void) {
  if (!this->alive) {
    return;
  }
  context* target = this->alive;
  if (this->alive == this->cur_routine) {
    target = this->alive->next;
  }
  if (target->next != NULL) {
    target->next->prev = target->prev;
  }
  if (target->prev != NULL) {
    target->prev->next = target->next;
  }
  force_sched(target);
}

void Engine::sched(void *routine_) {
  context* ctx = static_cast<context*>(routine_);
  if (ctx == this->cur_routine) {
    return;
  }

  context* ind = ctx;
  while(ind != NULL) {
    if (ind == this->cur_routine) {
      if (ctx->caller != NULL) {
        ctx->caller->callee = NULL; 
      }
    }
    ind = ind->caller;
  }
  
  force_sched(ctx);
}

void Engine::force_sched(void* routine_) {
  context* ctx = static_cast<context*>(routine_);
  if (this->cur_routine != NULL) {
    cur_routine->callee = ctx; 
    Store(*cur_routine); 
    if (setjmp(cur_routine->Environment) != 0) {
      return;
    }
  }
  this->cur_routine = ctx;
  this->Restore(*ctx);
}

} // namespace Coroutine
} // namespace Afina
