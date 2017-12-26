#include <afina/coroutine/Engine.h>

#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>

namespace Afina {
namespace Coroutine {

void Engine::Store(context &ctx) {
  char StackEndsHere;
  ptrdiff_t l = std::abs(this->StackBottom - &StackEndsHere);
  if (l >= std::get<1>(ctx.Stack)) {
    if (std::get<0>(ctx.Stack) != NULL) {
      delete[] std::get<0>(ctx.Stack);
    }
    std::get<0>(ctx.Stack) = new char[l];
  }
  std::get<1>(ctx.Stack) = l;
  if (this->StackBottom < &StackEndsHere) {
    memcpy(std::get<0>(ctx.Stack), this->StackBottom, l);
  } else {
    memcpy(std::get<0>(ctx.Stack), this->StackBottom - l + 1, l);
  }
}

void Engine::Restore(context &ctx) {
  char StackEndsHere;
  if (std::get<1>(ctx.Stack) > std::abs(this->StackBottom - &StackEndsHere)) {
    this->Restore(ctx);
  }
  if (this->StackBottom < &StackEndsHere) {
    memcpy(this->StackBottom, std::get<0>(ctx.Stack), std::get<1>(ctx.Stack));
  } else {
    memcpy(this->StackBottom - int32_t(std::get<1>(ctx.Stack)) + 1, std::get<0>(ctx.Stack), std::get<1>(ctx.Stack));
  }
  longjmp(ctx.Environment, 1); 
}


void Engine::yield() {
  if (this->alive) {
    context *pr = this->alive;
    this->alive = this->alive->next;
    sched(pr);
  } 
}



void Engine::sched(void *routine_) {
  context* ctx = static_cast<context*>(routine_);
  if (ctx == this->cur_routine) {
    return;
  }
  context* ind = ctx;
  // проверка связаны ли они в цепь, если да, то расцепляем первую связь и устанавливаем новую (из списка в дерево)
  while(ind != NULL) {
    if (ind == this->cur_routine) {
      ctx->caller->callee = NULL; // ctx->caller != NULL т.к. ctx != this->cur_routine то один раз цикл пройдет
    }
    ind = ind->caller;
  }

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
