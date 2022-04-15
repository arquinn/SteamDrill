
"""
Module for calling into various util functions from omnitable
"""

import utilcalls

def ignore_sys_begin(pid):
    """
    Start ignoreing system calls made by a process
    """
    utilcalls.ptrace_syscall_begin(pid)

def ignore_sys_end(pid):
    """
    Stop ignoreing system calls made by a process
    """
    utilcalls.ptrace_syscall_end(pid)

def exit_replay(pid):
    """
    Stop ignoreing system calls made by a process
    """
    utilcalls.exit_replay(pid)

def get_record_pid(pid):
    """
    Get the record pid associated with pid
    """
    return utilcalls.get_record_pid(pid)

def get_replay_clock(pid):
    """
    Get the shared replay clock at pid
    """
    return utilcalls.get_replay_clock(pid)
