#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>     /* struct msghdr */

/* size of control buffer to send/recv one file descriptor */
#define CONTROLLEN  CMSG_LEN(sizeof(int))
#define MAXLINE 100

static struct cmsghdr   *cmptr = NULL;      /* malloc'ed first time */

#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define NFNL
#ifdef NFNL
#include <netlink/netlink.h>
#include <netlink/netfilter/queue.h>
#include <netlink/socket.h>
#endif

char *socket_path = "./socket";

int client() {
	struct sockaddr_un addr;
	char buf[100];
	int fd,rc;

	if ( (fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("socket error");
		exit(-1);
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path)-1);

	if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		perror("connect error");
		exit(-1);
	}

	return fd;
}

/*
 * Receive a file descriptor from a server process.  Also, any data
 * received is passed to (*userfunc)(STDERR_FILENO, buf, nbytes).
 * We have a 2-byte protocol for receiving the fd from send_fd().
 */
int recv_fd(int fd, ssize_t (*userfunc)(int, const void *, size_t)) {
	int             newfd, nr, status;
	char            *ptr;
	char            buf[MAXLINE];
	struct iovec    iov[1];
	struct msghdr   msg;

	status = -1;
	while(1) {
		iov[0].iov_base = buf;
		iov[0].iov_len  = sizeof(buf);
		msg.msg_iov     = iov;
		msg.msg_iovlen  = 1;
		msg.msg_name    = NULL;
		msg.msg_namelen = 0;
		if (cmptr == NULL && (cmptr = malloc(CONTROLLEN)) == NULL)
			return(0);
		msg.msg_control    = cmptr;
		msg.msg_controllen = CONTROLLEN;
		if ((nr = recvmsg(fd, &msg, 0)) < 0) {
			perror("recvmsg error");
		} else if (nr == 0) {
			printf("connection closed by server");
			return(0);
		}
		/*
		 * See if this is the final data with null & status.  Null
		 * is next to last byte of buffer; status byte is last byte.
		 * Zero status means there is a file descriptor to receive.
		 */
		for (ptr = buf; ptr < &buf[nr]; ) {
			if (*ptr++ == 0) {
				if (ptr != &buf[nr-1])
					printf("message format error");
				status = *ptr & 0xFF;  /* prevent sign extension */
				if (status == 0) {
					if (msg.msg_controllen != CONTROLLEN)
    					printf("status = 0 but no fd");
					newfd = *(int *)CMSG_DATA(cmptr);
				} else {
					newfd = -status;
				}
				nr -= 2;
			}
		}
		if (nr > 0 && (*userfunc)(STDERR_FILENO, buf, nr) != nr) {
			printf("Message is not succesfully recived.\n");
			return(-1);
		}
		if (status >= 0)    /* final data has arrived */
			return(newfd);  /* descriptor, or -status */
	}
}

int main(int argc, char *argv[])
{
	int fd;
	int sockfd = client();
	fd = recv_fd(sockfd, write);
#if 1
	struct nl_sock* socket = nfnl_queue_socket_alloc();
	if (!socket) {
		perror("nfnl_queue_socket_alloc() failed");
	}

	int ret = nl_socket_set_fd(socket, -1, fd);

	struct nfnl_queue* q = nfnl_queue_alloc();
	if (!q) {
		perror("nfnl_queue_alloc() failed");
	}
#else
	printf("Accepted fd(%d) via socket\n", fd);
	FILE* fp = fdopen(fd, "w");
	fputs("What is your name?\n", fp);
#endif

	// close the socket
	close(sockfd);
	return 0;
}
