/*
 * The unwinder for C++ exceptions on 16-bit DOS and Windows: setjmp/longjmp
 * style ("SjLj") function contexts, as LLVM's SjLjEHPrepare pass makes
 * them. A function with landing pads registers a context in its frame at
 * entry and unregisters it on return; the contexts form a list from the
 * innermost function out. To throw, libc++abi calls
 * _Unwind_SjLj_RaiseException, which asks each context's personality
 * routine (__gxx_personality_sj0) for a handler, then jumps to the
 * dispatch code of each function with cleanups or the handler, which picks
 * the landing pad by the call site the personality chose.
 *
 * The layout of a context is fixed by the compiler (X86ISelLowering.cpp):
 *
 *   0   near pointer to the previous context
 *   2   call site (32 bits): set before each call, then by the personality
 *   6   four 32-bit data words: the exception pointer and the selector
 *   22  the personality routine: a near or far code pointer
 *   +   near pointer to the LSDA (the exception table, in DGROUP)
 *   +   jump buffer: BP, the dispatch's offset, SP, the dispatch's segment
 *       (with far code), one spare word
 *
 * Contexts are on the stack, in DGROUP (SS = DS), so near pointers reach
 * them in every memory model. The landing pads restore their registers
 * themselves, so jumping to one only sets BP and SP.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */
#include <stdint.h>
#include <stdlib.h>
#include <unwind.h>

#ifndef __USING_SJLJ_EXCEPTIONS__
#error "build with -fexceptions: the unwinder needs the SjLj exception model"
#endif

#if defined(__MEDIUM__) || defined(__LARGE__) || defined(__HUGE__)
#define FAR_CODE 1
#endif
/* A near pointer into DGROUP. The tiny model has only near pointers. */
#ifdef __TINY__
#define NEAR
#else
#define NEAR __near
#endif

struct fc {
  struct fc NEAR *prev;
  uint32_t call_site;
  uint32_t data[4];
  _Unwind_Personality_Fn personality;
  const unsigned char NEAR *lsda;
  uint16_t jbuf[5];
};

typedef struct fc NEAR *fc_t;

static fc_t top;

static fc_t to_fc(struct _Unwind_Context *context) {
  return (fc_t)context;
}

static struct _Unwind_Context *to_context(fc_t c) {
  return (struct _Unwind_Context *)c;
}

/* unwind.h declares these with a default pointer, which is far in the far
 * data models; the compiler passes a near one. */
void sjlj_register(fc_t c) __asm__("__Unwind_SjLj_Register");
void sjlj_unregister(fc_t c) __asm__("__Unwind_SjLj_Unregister");

void sjlj_register(fc_t c) {
  c->prev = top;
  top = c;
}

void sjlj_unregister(fc_t c) { top = c->prev; }

/* Jump to the dispatch code of the function that registered c. */
static __attribute__((noreturn)) void install(fc_t c) {
  top = c;
#ifdef FAR_CODE
  __asm__ volatile("movw 6(%%bx), %%ax\n\t"
                   "movw 2(%%bx), %%dx\n\t"
                   "movw 0(%%bx), %%bp\n\t"
                   "movw 4(%%bx), %%sp\n\t"
                   "pushw %%ax\n\t"
                   "pushw %%dx\n\t"
                   "lretw"
                   :
                   : "b"(c->jbuf)
                   : "memory");
#else
  __asm__ volatile("movw 2(%%bx), %%dx\n\t"
                   "movw 0(%%bx), %%bp\n\t"
                   "movw 4(%%bx), %%sp\n\t"
                   "jmpw *%%dx"
                   :
                   : "b"(c->jbuf)
                   : "memory");
#endif
  __builtin_unreachable();
}

static _Unwind_Reason_Code phase1(struct _Unwind_Exception *e) {
  for (fc_t c = top; c; c = c->prev) {
    if (!c->personality)
      continue;
    _Unwind_Reason_Code r = c->personality(1, _UA_SEARCH_PHASE,
                                           e->exception_class, e,
                                           to_context(c));
    if (r == _URC_HANDLER_FOUND) {
      e->private_2 = (uintptr_t)(uint16_t)c;
      return _URC_NO_REASON;
    }
    if (r != _URC_CONTINUE_UNWIND)
      return _URC_FATAL_PHASE1_ERROR;
  }
  return _URC_END_OF_STACK;
}

static _Unwind_Reason_Code phase2(struct _Unwind_Exception *e) {
  for (fc_t c = top; c; c = c->prev) {
    if (!c->personality)
      continue;
    _Unwind_Action action = _UA_CLEANUP_PHASE;
    if ((uintptr_t)(uint16_t)c == e->private_2)
      action = (_Unwind_Action)(_UA_CLEANUP_PHASE | _UA_HANDLER_FRAME);
    _Unwind_Reason_Code r =
        c->personality(1, action, e->exception_class, e, to_context(c));
    if (r == _URC_INSTALL_CONTEXT)
      install(c);
    if (r != _URC_CONTINUE_UNWIND || (action & _UA_HANDLER_FRAME))
      return _URC_FATAL_PHASE2_ERROR;
  }
  return _URC_END_OF_STACK;
}

_Unwind_Reason_Code _Unwind_SjLj_RaiseException(struct _Unwind_Exception *e) {
  e->private_1 = 0;
  e->private_2 = 0;
  _Unwind_Reason_Code r = phase1(e);
  if (r != _URC_NO_REASON)
    return r;
  return phase2(e);
}

/* Called by a landing pad that only ran cleanups, to go on unwinding. */
void _Unwind_SjLj_Resume(struct _Unwind_Exception *e) {
  phase2(e);
  abort();
}

/* Called by __cxa_rethrow. */
_Unwind_Reason_Code
_Unwind_SjLj_Resume_or_Rethrow(struct _Unwind_Exception *e) {
  return _Unwind_SjLj_RaiseException(e);
}

void *_Unwind_GetLanguageSpecificData(struct _Unwind_Context *context) {
  /* A near pointer into DGROUP, made a default (far or huge) pointer. */
  return (void *)(const unsigned char *)to_fc(context)->lsda;
}

_Unwind_Word _Unwind_GetGR(struct _Unwind_Context *context, int index) {
  return to_fc(context)->data[index];
}

void _Unwind_SetGR(struct _Unwind_Context *context, int index,
                   _Unwind_Word value) {
  to_fc(context)->data[index] = value;
}

/* The "instruction pointer" is the call site number, plus one. */
_Unwind_Word _Unwind_GetIP(struct _Unwind_Context *context) {
  return to_fc(context)->call_site + 1;
}

_Unwind_Word _Unwind_GetIPInfo(struct _Unwind_Context *context, int *before) {
  *before = 0;
  return to_fc(context)->call_site + 1;
}

void _Unwind_SetIP(struct _Unwind_Context *context, _Unwind_Word value) {
  to_fc(context)->call_site = value - 1;
}

_Unwind_Ptr _Unwind_GetRegionStart(struct _Unwind_Context *context) {
  (void)context;
  return 0;
}

_Unwind_Word _Unwind_GetCFA(struct _Unwind_Context *context) {
  return context ? to_fc(context)->jbuf[2] : 0;
}

void _Unwind_DeleteException(struct _Unwind_Exception *e) {
  if (e->exception_cleanup)
    e->exception_cleanup(_URC_FOREIGN_EXCEPTION_CAUGHT, e);
}
