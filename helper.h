#ifndef HELPER_H
#define HELPER_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <map>
#include <iostream>
#include <string>

#define FILENAMEBUF	100
#define BUF 100

struct forwardInfo
{
	int destport;
	char destip[16];
};

int parseConfig(char *filename);
int checkPort(int port);
int validateIP(char *ip);
int setSockOption(const int opt, int sd, int sdstate);
void displayMap();

#endif
