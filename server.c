#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <resolv.h>
#include <pthread.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#define DEFAULT_PORT 80
#define BUFSIZE 32765

struct connection {
	int sd;
	SLIST_ENTRY(connection) next;
};

SLIST_HEAD(connhead, connection) head = SLIST_HEAD_INITIALIZER(head);

char voidmsg[BUFSIZE] = {0};
int listensock = -1;

void sigh(int sig)
{
	struct connection *conn, *tmp;
	fprintf(stderr, "Shutting down server\n");
	shutdown(listensock, SHUT_RDWR);
	close(listensock);
	while ((conn = SLIST_FIRST(&head))) {
		SLIST_REMOVE_HEAD(&head, next);
		shutdown(conn->sd, SHUT_RDWR);
		close(conn->sd);
		free(conn);
	}
	exit(-1);
}

static inline void diep(const char *msg)
{
	perror(msg);
	signal(SIGTERM, sigh);
}

void *srvthread(void *arg)
{
	struct connection *conn = arg;
	char s[20];
	fprintf(stderr, "Connection thread started\n");
	while (send(conn->sd, voidmsg, BUFSIZE, 0) > 0) {
		/*Do nothing*/
	}
	fprintf(stderr, "client connection lost\n");
	perror("send()");
	signal(SIGTERM, sigh);
	return 0;
}

void *stat_poller(void *arg)
{
	struct connection *conn;
	struct tcp_info tcpinfo;
	int tcpinfolen = sizeof(struct tcp_info);
	unsigned int *delay = arg;

	printf("Polling at %u usec intervals\n", *delay);
	while (1) {
		SLIST_FOREACH(conn, &head, next) {
			if (getsockopt(conn->sd, SOL_TCP, TCP_INFO,
				       (void*)&tcpinfo, &tcpinfolen) != 0) {
				fprintf(stderr, "Failed to read TCP info\n");
				signal(SIGTERM, sigh);
			}
			printf("%u %u %u %u %u %u %u %u %u %u %u %u\n",
			tcpinfo.tcpi_last_data_sent,
			tcpinfo.tcpi_last_data_recv,
			tcpinfo.tcpi_snd_cwnd,
			tcpinfo.tcpi_snd_ssthresh,
			tcpinfo.tcpi_rcv_ssthresh,
			tcpinfo.tcpi_rtt,
			tcpinfo.tcpi_rttvar,
			tcpinfo.tcpi_unacked,
			tcpinfo.tcpi_sacked,
			tcpinfo.tcpi_lost,
			tcpinfo.tcpi_retrans,
			tcpinfo.tcpi_fackets);
		}
		
		usleep(*delay);
	}
}

int main(int argc, char *argv[])
{
	struct sockaddr_in addr = {0};
	int listensock;
	int client;
	char c;
	int port;
	int sample_interval = 1000000;
	pthread_t child;
	pthread_t poller;

	char srvip[INET_ADDRSTRLEN];

        addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(80);

	while ((c = getopt(argc, argv, "p:t:")) != -1) {
		switch (c) {
		case 'p':
			addr.sin_port = htons(atoi(optarg));
		break;
		case 't':
			sample_interval = atoi(optarg);
		break;
		default:
			exit(0);
		break;
		}
	}
	signal(SIGINT, sigh);
	inet_ntop(AF_INET, &addr.sin_addr, srvip, INET_ADDRSTRLEN);
	printf("Server accepting connections on %s:%d\n", srvip, ntohs(addr.sin_port));

	printf("Starting stat poller\n");
	pthread_create(&poller, 0, stat_poller, &sample_interval);
	pthread_detach(poller);

	listensock = socket(PF_INET, SOCK_STREAM, 0);
	if (listensock < 0)
		diep("socket()");

	if (bind(listensock, (struct sockaddr*)&addr, sizeof(addr)) != 0)
		diep("bind()");

	if (listen(listensock, 1000) != 0)
		diep("listen()");

	struct connection *nc;
	while (1)
	{
		if (!(nc = malloc(sizeof(struct connection))))
			diep("malloc()");
		
		if ((nc->sd = accept(listensock, 0, 0)) <= 0)
			diep("accept()");
		SLIST_INSERT_HEAD(&head, nc, next);
		pthread_create(&child, 0, srvthread, nc);
		pthread_detach(child);
	}
}
