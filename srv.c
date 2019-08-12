#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>     /* struct msghdr */
#include <unistd.h>
#include <sys/un.h>

/* open socket to nfq instead of file */
#define NFNL
#ifdef NFNL
#include <netlink/netlink.h>
#include <netlink/netfilter/queue.h>
#endif
/* size of control buffer to send/recv one file descriptor */
#define CONTROLLEN  CMSG_LEN(sizeof(int))
#define BUFSIZE (32 * 1024 * 1024)

static struct cmsghdr   *cmptr = NULL;  /* malloc'ed first time */
char *socket_path = "./socket";

int server() {
	struct sockaddr_un addr;
	char buf[100];
	int fd,cl,rc;

	if ( (fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("socket error");
		exit(-1);
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path)-1);
	unlink(socket_path);

	if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		perror("bind error");
		exit(-1);
	}

	if (listen(fd, 5) == -1) {
		perror("listen error");
		exit(-1);
	}
	if ( (cl = accept(fd, NULL, NULL)) == -1) {
		perror("accept error");
	}

	return cl;
}

/*
 * Pass a file descriptor to another process.
 * If fd<0, then -fd is sent back instead as the error status.
 */
int send_fd(int fd, int fd_to_send) {
	int ret;
	struct iovec    iov[1];
	struct msghdr   msg;
	char            buf[2]; /* send_fd()/recv_fd() 2-byte protocol */

	iov[0].iov_base = buf;
	iov[0].iov_len  = 2;
	msg.msg_iov     = iov;
	msg.msg_iovlen  = 1;
	msg.msg_name    = NULL;
	msg.msg_namelen = 0;

	if (fd_to_send < 0) {
		msg.msg_control    = NULL;
		msg.msg_controllen = 0;
		buf[1] = -fd_to_send;   /* nonzero status means error */
		if (buf[1] == 0)
			buf[1] = 1; /* -256, etc. would screw up protocol */
	} else {
		if (cmptr == NULL && (cmptr = malloc(CONTROLLEN)) == NULL) {
			printf("Malloc failed!\n");
			exit(127);
		}
		cmptr->cmsg_level  = SOL_SOCKET;
		cmptr->cmsg_type   = SCM_RIGHTS;
		cmptr->cmsg_len    = CONTROLLEN;
		msg.msg_control    = cmptr;
		msg.msg_controllen = CONTROLLEN;
		*(int *)CMSG_DATA(cmptr) = fd_to_send;     /* the fd to pass */
		buf[1] = 0;          /* zero status means OK */
	}
	buf[0] = 0;              /* null byte flag to recv_fd() */
	if (sendmsg(fd, &msg, 0) != 2) {
		perror("sendmsg error");
		exit(127);
	}

	printf("fd(%d) is sent via socket\n", fd_to_send);
	return(0);
}

int main(int argc, char *argv[])
{
	int fd;
	size_t bufsize = BUFSIZE;
	struct nl_sock* ct_sock = nfnl_queue_socket_alloc();
	if (!ct_sock) {
		perror("nfnl_queue_socket_alloc() failed");
	}

	nl_join_groups(ct_sock, NF_NETLINK_CONNTRACK_NEW | NF_NETLINK_CONNTRACK_UPDATE | NF_NETLINK_CONNTRACK_DESTROY);
	nl_socket_disable_seq_check(ct_sock);

	if (nl_connect(ct_sock, NETLINK_NETFILTER) < 0) {
		perror("Unable to connect netlink socket");
	}

	fd = nl_socket_get_fd(ct_sock);
	printf("fd : %d\n", fd);

	int sockfd = server();
	send_fd(sockfd, fd);
	printf("Done 1/2\n");

	struct nl_sock *q_sock = nfnl_queue_socket_alloc();
	nl_socket_disable_seq_check(q_sock);
	if(nl_connect(q_sock, NETLINK_NETFILTER)) {
		perror("Unable to connect netlink socket");
	}
	fd = nl_socket_get_fd(q_sock);
	send_fd(sockfd, fd);

	printf("Done 2/2\n");

	// After chatting close the socket
	close(sockfd);
	return 0;
}

