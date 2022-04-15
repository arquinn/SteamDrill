#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>

//char *socket_path = "./socket";
char *socket_path = "\0hidden";

int main(int argc, char *argv[]) {
	struct sockaddr_un addr;
	char buf[100];
	int fd,cl,rc;

	if (argc > 1) socket_path=argv[1];

	if ( (fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("socket error");
		exit(-1);
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path)-1);

	unlink(socket_path);

	if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1)
	{
		perror("bind error");
		exit(-1);
	}

	if (listen(fd, 5) == -1) {
		perror("listen error");
		exit(-1);
	}

	while (1) {
		if ( (cl = accept(fd, NULL, NULL)) == -1) {
			perror("accept error");
			continue;
		}

#ifdef USE_RDWR
		while ( (rc=read(cl,buf,sizeof(buf))) > 0) {
#else
		while ( (rc=recv(cl,buf,sizeof(buf), 0)) > 0) {
#endif
			printf("read %u bytes: %.*s\n", rc, rc, buf);
		}

		if (rc == -1) {
			perror("read");
			exit(-1);
		} else if (rc == 0) {
			printf("EOF\n");
			close(cl);
		}
	}

	return 0;
}
