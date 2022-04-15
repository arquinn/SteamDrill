#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>

#include <atomic>
#include <iostream>

using namespace std;

#include "streamserver.h"
#include "streamnw.h"

long init_socket (int port)
{
   int c = socket (AF_INET, SOCK_STREAM, 0);
    if (c < 0) {
	fprintf (stderr, "Cannot create socket, errno=%d\n", errno);
	return c;
    }

    int on = 1;
    long rc = setsockopt (c, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if (rc < 0) {
	fprintf (stderr, "Cannot set socket option, errno=%d\n", errno);
	return rc;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    rc = ::bind (c, (struct sockaddr *) &addr, sizeof(addr));
    if (rc < 0) {
	fprintf (stderr, "Cannot bind socket, errno=%d\n", errno);
	return rc;
    }

    rc = listen (c, 5);
    if (rc < 0) {
	fprintf (stderr, "Cannot listen on socket, errno=%d\n", errno);
	return rc;
    }
    
    int s = accept (c, NULL, NULL);
    if (s < 0) {
	fprintf (stderr, "Cannot accept connection, errno=%d\n", errno);
	return s;
    }

    close (c);
    return s;
}

long connect_to_host(const char *hostname, int port)
{
    // Open a connection to the 64-bit consumer process
    long s;
    int rc; 

    struct hostent* hp = gethostbyname (hostname);
    if (hp == NULL) {
	fprintf (stderr, "Invalid host %s, errno=%d\n", hostname, h_errno);
	return -1;
    }

    s = socket (AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
	fprintf (stderr, "Cannot create socket, errno=%d\n", errno);
	return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy (&addr.sin_addr, hp->h_addr, hp->h_length);

    int tries = 0;
    do {
	rc = connect (s, (struct sockaddr *) &addr, sizeof(addr));
	if (rc < 0) {
	    tries++;
	    usleep(1000);
	}
    } while (rc < 0 && tries <=100); //whats the punishment for increasing tries here? 

    if (rc < 0) {
	fprintf (stderr, "Cannot connect to socket (host %s, port %d), errno=%d\n", hostname, port, errno);
	return -1;
    }

    return s;
}

long safe_read (int s, void* buf, u_long size) 
{
    long bytes_read = 0;
    
    while (bytes_read < (long) size) {
	long rc = read (s, (char *) buf+bytes_read, size-bytes_read);	
	if (rc <= 0) return rc;
	bytes_read += rc;
    }
    return bytes_read;
}

long safe_write (int s, void* buf, u_long size)
{
    long bytes_written = 0;
    
    while (bytes_written < (long) size) {
	long rc = write (s, (char *) buf+bytes_written, size-bytes_written);	
	if (rc <= 0) return rc;
	bytes_written += rc;
    }
    return bytes_written;
}

long send_file (int s, const char* pathname, const char* filename)
{
    char buf[1024*1024];
    char sendfilename[FILENAME_LEN];
    struct stat st;
    int rc;

    // Get the filename
    int fd = open (pathname, O_RDONLY);
    if (fd < 0) {
	fprintf (stderr, "send_file: cannot open %s, rc=%d, errno=%d\n", pathname, fd, errno);
	return fd;
    }

    // Send the filename
    strcpy (sendfilename, filename);
    rc = write (s, sendfilename, sizeof(sendfilename));
    if (rc != sizeof(sendfilename)) {
	fprintf (stderr, "send_file: cannot write filename %s, rc=%d, errno=%d\n", filename,rc, errno);
	return rc;
    }

    // Send the file stats
    rc = fstat (fd, &st);
    if (rc < 0) {
	fprintf (stderr, 
		 "send_file: cannot stat %s, rc=%d, errno=%d\n", 
		 filename, 
		 rc, 
		 errno);
	return rc;
    }
    rc = write (s, &st, sizeof(st));
    if (rc != sizeof(st)) {
	fprintf (stderr, 
		 "send_file: cannot write file %s stats, rc=%d, errno=%d\n", 
		 filename, 
		 rc,
		 errno);
	return rc;
    }

	
    // Send file data
    u_long bytes_written = 0;
    while (bytes_written < (u_long) st.st_size) {
	size_t to_write = sizeof(buf);
	if (to_write > st.st_size - bytes_written)
	    to_write = st.st_size - bytes_written;
	    
      	rc = read (fd, buf, to_write);
	if (rc <= 0) {
	    fprintf (stderr, "send_file: read of %s returns %d, errno=%d\n", 
		     filename, 
		     rc, 
		     errno);
	    break;
	}
	long wrc = safe_write(s, buf, rc);
	if (wrc != rc) {
	    fprintf (stderr,
		     "send_file: write of %s returns %ld (not %d), errno=%d\n", 
		     filename, 
		     wrc, 
		     rc, 
		     errno);
	    break;
	}
	bytes_written += rc;
    }

    close (fd);

    return rc;
}

long fetch_file (int s, const char* output_dir, const char *output_postfix)
{
    char buf[1024*1024];
    char filename[FILENAME_LEN];
    struct stat st;
    u_long bytes_read;
    int rc;

    // Get the filename
    rc = safe_read (s, filename, sizeof(filename));
    if (rc != sizeof(filename)) {
	fprintf (stderr, "fetch_file: cannot read filename, rc=%d, errno=%d\n", rc, errno);
	return rc;
    }

    //std::cout << "fetch_file filename:" << filename << std::endl;

    // Get the file stats
    rc = safe_read (s, (char *) &st, sizeof(st));
    if (rc != sizeof(st)) {
	fprintf (stderr, "fetch_file: cannot read file %s stats, rc=%d, errno=%d\n", filename, rc, errno);
	return rc;
    }
	
    // Open the new file
    char pathname[FILENAME_LEN];
    if (output_postfix != NULL)
	sprintf (pathname, "%s/%s%s", output_dir, filename, output_postfix);
    else 
	sprintf (pathname, "%s/%s", output_dir, filename);
	
    int fd = open (pathname, O_CREAT|O_WRONLY|O_TRUNC, st.st_mode);
    if (fd < 0) {
	fprintf (stderr, "fetch_file: cannot create %s, rc=%d, errno=%d\n", pathname, rc, errno);
	return rc;
    }

    // Get the file data and write it out
    bytes_read = 0;
    while (bytes_read < (u_long) st.st_size) 
    {
	size_t to_read = sizeof(buf);
	if (to_read > st.st_size - bytes_read)
	    to_read = st.st_size - bytes_read;

	rc = read (s, buf, to_read);
	if (rc <= 0) {
	    fprintf (stderr, 
		     "fetch_file: read of %s returns %d, errno=%d\n", 
		     pathname, 
		     rc, 
		     errno);
	    fprintf (stderr, 
		     "already written %lu of %lu\n", 
		     bytes_read, 
		     st.st_size);
	    break;
	}
	long wrc = write(fd, buf, rc);
	if (wrc != rc) {
	    fprintf (stderr, 
		     "fetch_file: write of %s returns %ld (not %d), errno=%d\n", 
		     filename, 
		     wrc,
		     rc, 
		     errno);
	    break;
	}
	bytes_read += rc;
    }

    struct timespec times[2];
    times[0].tv_sec = st.st_mtim.tv_sec;
    times[0].tv_nsec = st.st_mtim.tv_nsec;
    times[1].tv_sec = st.st_mtim.tv_sec;
    times[1].tv_nsec = st.st_mtim.tv_nsec;
    rc = futimens (fd, times);
    if (rc < 0) {
	fprintf (stderr, "utimensat returns %d for file %s, errno=%d\n", rc, filename, errno);
    }

    close (fd);
    return rc;
}

