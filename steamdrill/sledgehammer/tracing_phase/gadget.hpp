#include <sys/types.h>
#include <sys/reg.h>
#include <sys/user.h>


#include <functional>
#include <list>
#include <unordered_map>

extern "C" {
#include <xed/xed-interface.h>
}

#include "log.hpp"
#ifndef __GADGET_H_
#define __GADGET_H_

// max size of our instructions
#define ASM_SIZE 15
#define MAX_INST 32

using std::rbegin;
using std::rend;

namespace Tracer {
template<unsigned int N>
class Gadget {
 private:
  char *_buffer;
  u_long _buffLen;
  u_long _len;
  uintptr_t _location;
  uintptr_t _stackOffset;
  std::array<xed_reg_enum_t, N> _regs;

 public:
  Gadget(xed_encoder_instruction_t *insts,
         size_t nInsts,
         xed_state_t *state,
         uintptr_t stackOffset,
         std::array<xed_reg_enum_t, N> args = {}): _buffer(nullptr),
                                                   _buffLen(0),
                                                   _len(0),
                                                   _location(0),
                                                   _stackOffset(stackOffset),
                                                   _regs(args) {

    // for the gadget that we're creating
    xed_encoder_request_t req;

    for (int i = 0; i < nInsts; ++i) {
      char assembly[ASM_SIZE];
      u_int addedSize;
      xed_bool_t convert = false;

      // initialize the requester
      xed_encoder_request_zero_set_mode(&req, state);

      convert = xed_convert_to_encoder_request(&req, insts + i);
      if (!convert) {
        ERROR("xed converter not working!?");
      }
      else
      {
        xed_error_enum_t encodeError = xed_encode(&req,
                                                  (u_char*)assembly,
                                                  ASM_SIZE,
                                                  &addedSize);
        if (encodeError != XED_ERROR_NONE) {
          ERROR("xed_encode error {}", xed_error_enum_t2str(encodeError));
        }
        else {
          appendToGadget((char *)assembly, addedSize);
        }
      }
    }
  }
  ~Gadget() {
    if (_buffer) {
      delete [] _buffer;
    }
  }

  uintptr_t getStackOffset(void) {return _stackOffset;}

  void appendToGadget(char *text, u_long len) {
    if (_buffLen < _len + len) {
      _buffLen = std::max (len + _len, 2 * _buffLen);
      char *tmp = new char[_buffLen];
      memcpy(tmp, _buffer, _len);

      if (_buffer)  {
        delete [] _buffer;
      }
      _buffer = tmp;
    }

    memcpy(_buffer + _len, text, len);
    _len += len;
  }

  template <typename ... Args,
            std::enable_if_t<std::conjunction_v<std::is_integral<Args>...>, bool> = true>
  void fillRegs(struct user_regs_struct *regs, Args... passed) {
    static_assert(sizeof ...(Args) == N, "incorrect # of arguments");
    std::array<u_int, N> args = {static_cast<u_int>(passed)...}; //das work?

    auto reg = _regs.begin();
    for (auto a = args.begin(), end = args.end(); a != end; ++a) {
      switch(*reg) {
        case XED_REG_EAX:
          regs->eax = *a;
          break;
        case XED_REG_EBX:
          regs->ebx = *a;
          break;
        case XED_REG_ECX:
          regs->ecx = *a;
          break;
        case XED_REG_EDX:
          regs->edx = *a;
          break;
        case XED_REG_EDI:
          regs->edi = *a;
          break;
        case XED_REG_ESI:
          regs->esi = *a;
          break;
        case XED_REG_EBP:
          regs->ebp = *a;
          break;
        default:
          ERROR("somehow I'm out of arguments!");
          // THROW("too many arguments for {}!", N);
      }
      reg++;
    }
  }
  inline char * getBuffer(void) {
    return _buffer;
  };

  inline u_long getLen(void) {
    return _len;
  };

  inline uintptr_t getLocation(void) {
    return _location;
  }

  inline void setLocation(uintptr_t loc) {
    _location = loc;
  }
};


//helper fxns:
inline void initState(xed_state_t *state) {
  xed_state_zero(state);
  state->mmode = XED_MACHINE_MODE_LEGACY_32;
  state->stack_addr_width = XED_ADDRESS_WIDTH_32b;
}


#define STD_STACK_OFF 0x4

void initStandardGadgets(void);
Gadget<0>* makeInt3Gadget(int numBytes);
Gadget<0>* makeJmpGadget(uintptr_t start, uintptr_t dest);
Gadget<6>* makeMmapGadget();
Gadget<3>* makeMprotectGadget();
Gadget<2>* makeOpenGadget();
Gadget<0>* makeTracerCall(uintptr_t tracer, uintptr_t begin, uintptr_t end, int align);
Gadget<0>* makeSegfaultGadget(uintptr_t segfault);

template<unsigned int N>
Gadget<N>* makeFxnCallGadget(uintptr_t fxn, int align, std::initializer_list<u_long> args = {}) {

  static std::array<xed_reg_enum_t, 6> registers = {XED_REG_EAX,
                                                    XED_REG_EBX,
                                                    XED_REG_ECX,
                                                    XED_REG_EDX,
                                                    XED_REG_EDI,
                                                    XED_REG_ESI};

  int pushes = 4 * N;
  // extra -4 b/c the original stack is an upper bound
  int stack = (((pushes + 0x4) & align) + align) - pushes;

  NDEBUG("making fxn call {:x} {:x}", pushes, stack);

  xed_state_t state;
  xed_encoder_instruction_t encoderInst[MAX_INST];
  u_int nInst = 0;
  std::array<xed_reg_enum_t, N> myRegs;
  initState(&state);

  //push all of the arguments:
  auto reg = registers.begin();
  for (int i = 0; i < N; ++i) {
    xed_inst1(encoderInst + nInst, state, XED_ICLASS_PUSH, 0, xed_reg(*reg));
    myRegs[N - i - 1] = *reg;
    reg++;
    nInst++;
  }
  for (auto a = rbegin(args); a != rend(args); ++a) {
    xed_inst1(encoderInst + nInst, state, XED_ICLASS_PUSH, 0,  xed_imm0(*a, 32));
    nInst++;
  }

  //now push the call logic (indirect call):
  xed_inst2(encoderInst + nInst, state, XED_ICLASS_MOV, 0, xed_reg(XED_REG_EAX), xed_imm0(fxn,32));
  nInst++;
  xed_inst1(encoderInst + nInst, state, XED_ICLASS_CALL_NEAR, 0, xed_reg(XED_REG_EAX));
  nInst++;

  //finally: end in an int3
  xed_inst0(encoderInst + nInst, state, XED_ICLASS_INT3, 0);
  nInst++;

  return new Gadget<N>(encoderInst, nInst, &state, stack, myRegs);
}

} /* Namespace Tracer */

#endif /* __GADGET_H_*/
