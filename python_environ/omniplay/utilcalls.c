#include <Python.h>

#include "util.h"      // we're adding python calls for functions in this header file

static int fd_spec; //can I do this??
static u_long *replay_clock; // something

static PyObject *
py_ptrace_syscall_begin(PyObject *self, PyObject *args) {
  int rtn, pid;

  if (fd_spec < 0) {
    return NULL;
  }

  if (!PyArg_ParseTuple(args, "i", &pid))
    return NULL;

  rtn = ptrace_syscall_begin(fd_spec, pid);
  return Py_BuildValue("i", rtn);
}

static PyObject *
py_ptrace_syscall_end(PyObject *self, PyObject *args) {
  int rtn, pid;
  int *unused_status, *unused_pstatus;

  assert (self);

  if (!PyArg_ParseTuple(args, "i", &pid))
    return NULL;

  rtn = ptrace_syscall_end(fd_spec, pid, &unused_status, &unused_pstatus);
  return Py_BuildValue("i", rtn);
}

static PyObject *
py_try_to_exit(PyObject *self, PyObject *args) {
  int rtn, pid;

  assert (self);

  if (!PyArg_ParseTuple(args, "i", &pid))
    return NULL;

  rtn = try_to_exit(fd_spec, pid);
  return Py_BuildValue("i", rtn);
}

static PyObject *
py_get_record_pid(PyObject *self, PyObject *args) {
  int rtn, pid;

  assert (self);

  if (!PyArg_ParseTuple(args, "i", &pid)) {
    fprintf(stderr, "Didn't find a pin in get_record_pid??");
    return NULL;
  }

  rtn = get_log_id(fd_spec, pid);
  return Py_BuildValue("i", rtn);
}

static PyObject *
py_get_replay_clock(PyObject *self, PyObject *args) {
  int pid;
  u_long rtn;

  if (fd_spec < 0) {
    return NULL;
  }

  if (!PyArg_ParseTuple(args, "i", &pid)) {
    fprintf(stderr, "Didn't find a pid in get_replay_clock??");
    return NULL;
  }

  if (!replay_clock) {
    replay_clock = map_other_clock(fd_spec, pid);
    assert (replay_clock);
  }
  rtn = *replay_clock;
  return Py_BuildValue("i", rtn);
}



static PyMethodDef UtilMethods[] = {
  {"ptrace_syscall_begin", py_ptrace_syscall_begin, METH_VARARGS, "calls ptrace_syscall_begin"},
  {"ptrace_syscall_end", py_ptrace_syscall_end, METH_VARARGS, "calls ptrace_syscall_end"},
  {"exit_replay", py_try_to_exit, METH_VARARGS, "calls exit_repay"},
  {"get_record_pid", py_get_record_pid, METH_VARARGS, "calls get_record_pid"},
  {"get_replay_clock", py_get_replay_clock, METH_VARARGS, "calls get_replay_clock"},
};


#ifndef PyMODINIT_FUNC
#define PyMODINIT_FUNC void
#endif

PyMODINIT_FUNC initutilcalls(void) {
  PyObject *m = Py_InitModule3("utilcalls", UtilMethods,
                               "bindings to call arnold's util methods from python");
  int rc;

  if (m == NULL) {
    return;
  }

  // try opening dev?
  rc = devspec_init(&fd_spec);
  if (fd_spec < 0 || rc < 0) {
    return;
  }

  // we would like to creat a shared clock, but we don't know what the current pid is!
  replay_clock = NULL;
}

