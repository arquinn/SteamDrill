#ifndef __TRACE_STATE_H
#define __TRACE_STATE_H

namespace TracerGlobals
{
    // checkpointed and saved for each tracer
    struct user_regs_struct savedRegs;

    // shared by all gadgets
    char *savedRegion = NULL;
    u_long gadgetStart = 0;

//    bool separateGadgetArea = false;
    // for the fork gadget
    u_char  *forkGadget = NULL;
    u_long forkGadgetLen = 0;
    u_long forkBreak = 0;

    // for the waitpid gadget
    char  *waitpidGadget = NULL;
    u_long waitpidAddr = 0;
    u_long waitpidGadgetLen = 0;

    // for the map gadget
    u_long dlopenAddr = 0;
    u_long dlsymAddr = 0;

    // for calling syscalls
    u_long syscallAddr = 0;

    u_char  *syscallGadget = NULL;
    u_long syscallGadgetLen = 0;


    // for the dlerror gadget
    u_char *dlerrorGadget = NULL;
    u_long dlerrorGadgetLen = 0;
    u_long dlerrorAddr = 0;
}


#endif /*TRACE_STAT_H*/
