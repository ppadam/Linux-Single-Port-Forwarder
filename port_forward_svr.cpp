#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <strings.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include "helper.cpp"
#include "helper.h"

#define TRUE 		1
#define FALSE 		0
#define EPOLL_QUEUE_LEN	256
#define BUFLEN		16000
#define FILENAMEBUF	100
#define BUF 100
using namespace std;

/* Globals */
int serviceSocket;

/*
 * Create map for srcport, destip:destport configuration.
 */
map<int, forwardInfo> ipPortList;

/**
 * Create a map for connections. serviceSocket,clientSocket.
 */
map<int, int> connections;

/* Function prototypes */
static void SystemFatal(const char* );
static int ClearSocket (int fd);
void close_fd (int);

/*---------------------------------------------------------------------------------------
--	FUNCTION:  main
--
--	DATE:  March 30th 2010
--
--	REVISIONS:  	
--
--	DESIGNERS:  Pardeep Padam
--
--	PROGRAMMER:  Pardeep Padam
--
--	INTERFACE:  int main(int argc, char* argv[])
--
--	RETURNS:  int
--
--	NOTES:  Main program of the port forwarder application.
--	
---------------------------------------------------------------------------------------*/
int main (int argc, char* argv[]) 
{
	int i, servicePort, serviceSocket, serverSocket; 
	int num_fds, fd_new, epoll_fd;
	static struct epoll_event events[EPOLL_QUEUE_LEN], event;
	struct sockaddr_in addr, remote_addr;
	struct sockaddr_in remote_server;
	struct hostent	*hp;
	socklen_t addr_size = sizeof(struct sockaddr_in);
	struct sigaction act;
	char *destHostIP;
	int destHostPort;
	
	/*
	 * configuration file name
	 */
	char filename[FILENAMEBUF] = "config.txt";

		/* set up the signal handler to close the server socket when CTRL-c is received */
	act.sa_handler = close_fd;
	act.sa_flags = 0;
	if ((sigemptyset (&act.sa_mask) == -1 || sigaction (SIGINT, &act, NULL) == -1))
	{
			SystemFatal ("Failed to set SIGINT handler");
			exit (EXIT_FAILURE);
	}
	
	/* Create the epoll file descriptor */
	epoll_fd = epoll_create(EPOLL_QUEUE_LEN);
	if (epoll_fd == -1) 
		SystemFatal("epoll_create");
		
	/*
	 * Read in the configuration file and put it into a two dimensional
	 * array.
	 */
	parseConfig(filename);
	
	/*
	 * Display all entries in the map.
	 */
	displayMap();
	
	/*
	 * Create listen socket for each service and add it to the epoll event loop
	 */
	map<int, forwardInfo>::iterator iter;
	for(iter = ipPortList.begin(); iter != ipPortList.end(); ++iter)
	{
		/*
		 * Get the listening port for the service.
		 */
		servicePort = (*iter).first;
		
		/*
		 * Get the ip for the destination host.
		 */
		destHostIP = (*iter).second.destip;
		
		/*
		 * Get the port for the destination port.
		 */
		destHostPort = (*iter).second.destport;
		
		/*
		 * Create the socket for the service that will be listening.
		 */
		serviceSocket = socket (AF_INET, SOCK_STREAM, 0);
		if (serviceSocket == -1) 
		{
			perror("socket");
			return -1;
		}

		/*
		 * Set the socket options.
		 */
		if(setSockOption(SO_REUSEADDR, serviceSocket, 1) == -1)
		{
			perror("setsockopt");
		}

		/*
		 * Make the server listening socket non-blocking.
		 */
		if (fcntl (serviceSocket, F_SETFL, O_NONBLOCK | fcntl (serviceSocket, F_GETFL, 0)) == -1) 
		{
			perror("fcntl");
		}

		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
		addr.sin_port = htons(servicePort);

		/*
		 * Bind the socket.
		 */
		if (bind (serviceSocket, (struct sockaddr*)&addr, sizeof(addr)) == -1) 
		{
			perror("bind");
		}
		else
		{		
			/* Listen for fd_news; SOMAXCONN is 128 by default */
			if (listen (serviceSocket, 65535) == -1) 
				SystemFatal("listen");
			else
				printf("Listening for connections\n\n");
			
			event.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLET;
			event.data.fd = serviceSocket;
			if (epoll_ctl (epoll_fd, EPOLL_CTL_ADD, serviceSocket, &event) == -1)
			{
				SystemFatal("epoll_ctl");
			}
		}
	}
    	
	/* Execute the epoll event loop */
	while (TRUE) 
	{
		num_fds = epoll_wait (epoll_fd, events, EPOLL_QUEUE_LEN, -1);
		if (num_fds < 0) 
			SystemFatal ("Error in epoll_wait!");

		for (i = 0; i < num_fds; i++) 
		{
			/* Case 1: Error condition */
			if (events[i].events & (EPOLLHUP | EPOLLERR)) 
			{
				fputs("epoll: EPOLLERR\n", stderr);
				close(events[i].data.fd);
				continue;
			}
			assert (events[i].events & EPOLLIN);

			/* Case 2: Server is receiving a connection request */
			if (events[i].data.fd == serviceSocket) 
			{
				while((fd_new = accept (serviceSocket, (struct sockaddr*) &remote_addr, &addr_size)) != -1)
				{
					/* 
					 * Create the socket to connect from forwarder to the server.
					 */
					if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
					{
						perror("Cannot create socket");
						exit(1);
					}
					bzero((char *)&remote_server, sizeof(struct sockaddr_in));
					remote_server.sin_family = AF_INET;
					remote_server.sin_port = htons(destHostPort);
					if ((hp = gethostbyname(destHostIP)) == NULL)
					{
						fprintf(stderr, "Unknown server address\n");
						exit(1);
					}
					bcopy(hp->h_addr, (char *)&remote_server.sin_addr, hp->h_length);

					// Connecting to the server
					if (connect (serverSocket, (struct sockaddr *)&remote_server, sizeof(addr)) == -1)
					{
						fprintf(stderr, "Can't connect to server\n");
						perror("connect");
						exit(1);
					}
					
					/*
					 * Next two ifs for the accepted socket.
					 */
					/* Make the fd_new non-blocking */
					if (fcntl (fd_new, F_SETFL, O_NONBLOCK | fcntl(fd_new, F_GETFL, 0)) == -1) 
						SystemFatal("fcntl");
					/* Add the new socket descriptor to the epoll loop */
					event.data.fd = fd_new;
					if (epoll_ctl (epoll_fd, EPOLL_CTL_ADD, fd_new, &event) == -1) 
						SystemFatal ("epoll_ctl");
						
					/*
					 * Next two ifs for the forwarder->server socket.
					 */
					/* Make the fd_new non-blocking */
					if (fcntl (serverSocket, F_SETFL, O_NONBLOCK | fcntl(fd_new, F_GETFL, 0)) == -1) 
						SystemFatal("fcntl");
					/* Add the new socket descriptor to the epoll loop */
					event.data.fd = serverSocket;
					if (epoll_ctl (epoll_fd, EPOLL_CTL_ADD, serverSocket, &event) == -1) 
						SystemFatal ("epoll_ctl");
					
					/* 
					 * Adds the new connection as a value pair with the serviceSocket.					 * 
					 */
					connections[serverSocket] = fd_new;
					connections[fd_new] = serverSocket;
		
					printf("New connection from %s. Socket descriptor: %d.\n", inet_ntoa(remote_addr.sin_addr), fd_new);
					continue;
				}
				if (fd_new == -1) 
				{
					if (errno != EAGAIN && errno != EWOULDBLOCK) 
					{
						SystemFatal("accept");
					}
					continue;
				}
			}
			
			/* Case 3: One of the sockets has read data */
			if (!ClearSocket(events[i].data.fd)) 
			{
				/* epoll will remove the fd from its set
				 * automatically when the fd is closed */		
				close (events[i].data.fd);
				printf("Closed socket descriptor: %d.\n", events[i].data.fd);
				
				int sock;
				
				map<int, int>::iterator it;
				
				/*
				 * Iterate through the connections map and find the fd
				 * that no longer has data and close the server forward
				 * socket related to fd.
				 */
				it = connections.find(events[i].data.fd);
				if(it != connections.end())
				{
					sock = it->second;
					
					close(sock);
					
					/*
					 * Remove the connection pairs from the connection map.
					 */
					connections.erase(connections.find(events[i].data.fd));
					connections.erase(connections.find(sock));
				}
				else
				{
					cout << "Key not in map.\n";
				}
			}
		}
	}
	close(serviceSocket);
	exit (EXIT_SUCCESS);
}

/*---------------------------------------------------------------------------------------
--	FUNCTION:  ClearSocket
--
--	DATE:  March 30th 2010
--
--	REVISIONS:  	
--
--	DESIGNERS:  Pardeep Padam
--
--	PROGRAMMER:  Pardeep Padam
--
--	INTERFACE:  static int ClearSocket(int fd)
--
--	RETURNS:  int
--
--	NOTES:  Tterate through the connections map and find the fd that no longer has data and close the server forward socket related to the fd.
--	
---------------------------------------------------------------------------------------*/
static int ClearSocket (int fd) 
{
	int n = -1, temp = -1;
	char rbuf[BUFLEN];
	
	int fromFD = fd;
	int toFD;
				
	map<int, int>::iterator itData;
	
	/*
	 * Iterate through the connections map and find the fd
	 * that no longer has data and close the server forward
	 * socket related to the fd.
	 */
	itData = connections.find(fromFD);
	if(itData != connections.end())
	{
		toFD = itData->second;
	}
	
	while(TRUE)
	{
		/*clear buffer */
		memset(rbuf, '\0', sizeof(rbuf) );
		
		if((n = recv(fromFD, rbuf, BUFLEN, 0)) == -1 )
		{	
			perror("error reading");
			close(fromFD);
			return 0;
		}
		else if(n > 0)
		{	
			/*printf("data: %s socket: %d\n", rbuf, fromFD);*/
			/* echo */
			if((temp = send(toFD, rbuf, n, 0)) < 0)
			{
				perror("error sending");
				return 1;
			}
			else
			{
				/* successfull send. data sent. */
				/*printf("sent: %s socket: %d\n", rbuf, toFD);*/
				return 1;
			}
		}
		else
		{
			close(fd);
			return 0;
		}
	}
	return 0;
}

/* Prints the error stored in errno and aborts the program. */
static void SystemFatal(const char* message) 
{
    perror (message);
    exit (EXIT_FAILURE);
}

/* close fd */
void close_fd (int signo)
{
	close(serviceSocket);
	exit (EXIT_SUCCESS);
}
