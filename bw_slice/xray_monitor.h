#ifndef XRAY_MONITOR_H
#define XRAY_MONITOR_H

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <regex.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <vector>

// A structure to hold fds
struct fd_struct {
    int fd;
    void* data;
};

/* A monitor is a struct that keeps track of fds that we want to monitor */
struct xray_monitor {
    std::vector<struct fd_struct> fds; 
};


// Prototypes
struct xray_monitor* new_xray_monitor();
int monitor_has_fd(struct xray_monitor*, int fd);
int monitor_add_fd(struct xray_monitor*, int fd, void* data);
int monitor_remove_fd(struct xray_monitor*, int fd);
int monitor_size(struct xray_monitor*);
void* monitor_get_fd_data(struct xray_monitor*, int fd);


struct xray_monitor* new_xray_monitor() {
    struct xray_monitor* monitor;
//    monitor = (struct xray_monitor*) malloc(sizeof(struct xray_monitor));
    monitor = new xray_monitor(); 

    return monitor;
}

int monitor_has_fd(struct xray_monitor* monitor, int fd) {
    for (auto &fds : monitor->fds) 
    {
	if (fds.fd == fd) {
	    return 1; 
	}
    }
    return 0;
}

/**
 * Add an fd to monitor.
 *
 * If the fd already exists in the list, we set the cloexec flag to the new cloexec flag.
 */
int monitor_add_fd(struct xray_monitor* monitor, int fd, void* data) {
    struct fd_struct fds;
    // if it's already in here, remove it
    if (monitor_has_fd(monitor, fd)) {
        fprintf(stderr, "WARN -- monitor already has fd %d\n", fd);
        monitor_remove_fd(monitor, fd);
    }

    // else add it
    fds.fd = fd;
    fds.data = data;

    monitor->fds.push_back(fds);
    return 0;
}

int monitor_remove_fd(struct xray_monitor* monitor, int fd) {

    for (std::vector<fd_struct>::iterator fds = monitor->fds.begin();
	 fds != monitor->fds.end(); 
	 ++fds)
    {
        if (fds->fd == fd) {
	    monitor->fds.erase(fds);
	    return 1;
	}
    }

    return 0;
}

int monitor_size(struct xray_monitor* monitor) {
    return monitor->fds.size();
}

void* monitor_get_fd_data(struct xray_monitor* xrm, int fd)
{
    for (auto fds : xrm->fds)
    {
        if (fds.fd == fd) {
            return fds.data;
        }
    }
    return NULL;

}

#endif // end include guard XRAY_MONITOR_H
