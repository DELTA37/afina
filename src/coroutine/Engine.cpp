#include <afina/coroutine/Engine.h>

#include <setjmp.h>
#include <stdio.h>
#include <string.h>

namespace Afina {
namespace Coroutine {

void Engine::Store(context &ctx) volatile {
  char StackEndsHere;
  size_t l = this->StackBottom - &StackEndsHere;
  std::get<1>(ctx.Stack) = l;
  delete[] std::get<0>(ctx.Stack);
  std::get<0>(ctx.Stack) = new char[l];
  memcpy(std::get<0>(ctx.Stack), &StackEndsHere, l);
}

void Engine::Restore(context &ctx) volatile {
  char StackEndsHere;
  size_t l = std::get<1>(ctx.Stack);
  if (&StackEndsHere + std::get<1>(ctx.Stack) > this->StackBottom ) {
    char data[100];
    this->Restore(ctx);
  }
  memcpy(StackBottom - std::get<1>(ctx.Stack), std::get<0>(ctx.Stack), std::get<1>(ctx.Stack));
  longjmp(ctx.Environment, 1); 
}


void Engine::yield() volatile {
  if (this->alive) {
    context *pr = this->alive;
    this->alive->prev = nullptr;
    this->alive = this->alive->next;
    sched(pr);
  }
}



void Engine::sched(void *routine_) volatile {
  context* ctx = static_cast<context*>(routine_);
  while (ctx->caller != NULL) {
    ctx = ctx->caller;
  }
  if (ctx == this->cur_routine) {
    return;
  }
  ctx->caller = this->cur_routine;
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
