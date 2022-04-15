#include <sys/types.h>
#include <sys/reg.h>
#include <sys/user.h>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <unordered_map>
#include <vector>

extern "C" {
#include <xed/xed-interface.h>
}

#include "log.hpp"
#include "gadget.hpp"

namespace Tracer {


void initStandardGadgets(void) {
  xed_tables_init();
};

Gadget<6>* makeMmapGadget() {

  // for encoding the call instruction
  xed_state_t state;
  xed_encoder_instruction_t encoderInst[MAX_INST];
  u_int nInst = 0;
  initState(&state);

  // The classic int 0x80 systemcall interface for simplicity:
  //   - arg order ebx, ecx, edx, esi, edi, ebp
  //   - syscall number in eax (192)
  xed_inst2(encoderInst + nInst, state, XED_ICLASS_MOV, 0, xed_reg(XED_REG_EAX), xed_imm0(192, 32));
  nInst++;

  // an int 0x80
  xed_inst1(encoderInst + nInst, state, XED_ICLASS_INT, 0, xed_imm0(0x80,8));
  nInst++;

  // a breakpoint
  xed_inst0(encoderInst + nInst, state, XED_ICLASS_INT3, 0);
  nInst++;

  return new Gadget<6>(encoderInst,
                       nInst,
                       &state,
                       STD_STACK_OFF,
                       {XED_REG_EBX, XED_REG_ECX, XED_REG_EDX, XED_REG_ESI, XED_REG_EDI, XED_REG_EBP});
}

Gadget<3>* makeMprotectGadget() {

  // for encoding the call instruction
  xed_state_t state;
  xed_encoder_instruction_t encoderInst[MAX_INST];
  u_int nInst = 0;
  initState(&state);

  // The classic int 0x80 systemcall interface for simplicity:
  //   - arg order ebx, ecx, edx, esi, edi, ebp
  //   - syscall number in eax (125 for mprotect)
  xed_inst2(encoderInst + nInst, state, XED_ICLASS_MOV, 0, xed_reg(XED_REG_EAX), xed_imm0(125, 32));
  nInst++;

  // an int 0x80
  xed_inst1(encoderInst + nInst, state, XED_ICLASS_INT, 0, xed_imm0(0x80,8));
  nInst++;

  // a breakpoint
  xed_inst0(encoderInst + nInst, state, XED_ICLASS_INT3, 0);
  nInst++;

  return new Gadget<3>(encoderInst,
                       nInst,
                       &state,
                       STD_STACK_OFF,
                       {XED_REG_EBX, XED_REG_ECX, XED_REG_EDX});
}


Gadget<2>* makeOpenGadget() {
  // for encoding the call instruction
  xed_state_t state;
  xed_encoder_instruction_t encoderInst[MAX_INST];
  u_int nInst = 0;

  //all params to mmap are constant (for now!)
  initState(&state);
  //push all of the arguments:
  //now push the call logic (indirect call):
  xed_inst2(encoderInst + nInst, state, XED_ICLASS_MOV, 0, xed_reg(XED_REG_EAX), xed_imm0(5,32));
  nInst++;
  // an int 0x80
  xed_inst1(encoderInst + nInst, state, XED_ICLASS_INT, 0, xed_imm0(0x80,8));
  nInst++;

  // a breakpoint
  xed_inst0(encoderInst + nInst, state, XED_ICLASS_INT3, 0);
  nInst++;

  return new Gadget<2>(encoderInst, nInst, &state, STD_STACK_OFF, {XED_REG_EBX, XED_REG_ECX});
}


Gadget<0>* makeInt3Gadget(int numBytes) {
  // for encoding the call instruction
  xed_state_t state;
  u_long nInst = 0;
  std::unique_ptr<xed_encoder_instruction_t[]> encoderInst(new xed_encoder_instruction_t[numBytes]);

  DEBUG_ALLOC("newInt3 {:x} rtn {}", sizeof(Gadget), (void*)gadget);

  initState(&state);
  for (int i = 0; i < numBytes; ++i) {
    xed_inst0(encoderInst.get() + nInst, state, XED_ICLASS_INT3, 0);
    nInst++;
  }

  return new Gadget<0>(encoderInst.get(), nInst, &state, STD_STACK_OFF);
}

Gadget<0>* makeJmpGadget(uintptr_t start, uintptr_t dest) {
  // for encoding the call instruction
  xed_state_t state;
  u_long nInst = 0;
  xed_encoder_instruction_t encoderInst[1];

  initState(&state);
  xed_inst1(encoderInst, state, XED_ICLASS_JMP, 0,  xed_relbr((int32_t)dest - (int32_t)(start + 5), 32));
  nInst++;
  return new Gadget<0>(encoderInst, nInst, &state, STD_STACK_OFF);
}

Gadget<0>* makeSegfaultGadget(uintptr_t segfault) {
  return makeFxnCallGadget<0>(segfault, STD_STACK_OFF);
}

Gadget<0>* makeTracerCall(uintptr_t tracer, uintptr_t begin, uintptr_t end, int stackalign) {
  // for encoding the call instruction
  xed_state_t state;
  xed_encoder_instruction_t encoderInst[MAX_INST];
  u_int nInst = 0;
  initState(&state);

  assert (stackalign > 4);
  //call begin
  xed_inst2(encoderInst + nInst, state, XED_ICLASS_MOV, 0, xed_reg(XED_REG_EAX), xed_imm0(begin,32));
  nInst++;
  xed_inst1(encoderInst + nInst, state, XED_ICLASS_CALL_NEAR, 0, xed_reg(XED_REG_EAX));
  nInst++;

  // call tracer (save return val in edi (it's callee saved!)
  xed_inst2(encoderInst + nInst, state, XED_ICLASS_MOV, 0, xed_reg(XED_REG_EAX), xed_imm0(tracer,32));
  nInst++;
  xed_inst1(encoderInst + nInst, state, XED_ICLASS_CALL_NEAR, 0,  xed_reg(XED_REG_EAX));
  nInst++;
  xed_inst2(encoderInst + nInst, state, XED_ICLASS_MOV, 0, xed_reg(XED_REG_EDI), xed_reg(XED_REG_EAX));
  nInst++;

  // call end. restore return val from the tracer into eax.
  xed_inst2(encoderInst + nInst, state, XED_ICLASS_MOV, 0, xed_reg(XED_REG_EAX), xed_imm0(end,32));
  nInst++;
  xed_inst1(encoderInst + nInst, state, XED_ICLASS_CALL_NEAR, 0, xed_reg(XED_REG_EAX));
  nInst++;
  xed_inst2(encoderInst + nInst, state, XED_ICLASS_MOV, 0, xed_reg(XED_REG_EAX), xed_reg(XED_REG_EDI));
  nInst++;

  //finally: end in an int3
  xed_inst0(encoderInst + nInst, state, XED_ICLASS_INT3, 0);
  nInst++;

  return new Gadget<0>(encoderInst, nInst, &state, stackalign);
}
} // end namespace Tracer
