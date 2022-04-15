#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <errno.h>

#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>

#include <iostream>
#include <iomanip>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#ifndef __TRACER_OBJECT_CONFIG__
#define __TRACER_OBJECT_CONFIG__

#define TRACER_INIT_FXN "tracerLibInit"
#define TRACER_DESTROY_FXN "tracerLibDestruct"
#define TRACER_BEGIN_FXN "tracerBegin"
#define TRACER_END_FXN "tracerEnd"
#define TRACER_LCLOCK_FXN "tracerLogicalclock"
#define TRACER_ADDLOG_FXN "tracerAddLog"

#define TASK_INIT_FXN "tracerLibThreadInit"

#endif /*__PTRACE_FUNCTIONS__*/
