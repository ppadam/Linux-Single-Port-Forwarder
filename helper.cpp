/*
 * Helper file containing several functions used by the port forwarder.
 */

#include "helper.h"
using namespace std;

extern map<int, forwardInfo> ipPortList;

/*
 * Given a filename and a two-dimensional array for ip:port 
 * combinations, the file is opened and parsed for ip:port pairs and
 * added to the array.
 */
int parseConfig(char *filename)
{
	char filein[BUF];
	int srcport = -1, destport = -1;
	char destip[16];
	
	struct forwardInfo tempList;
	
	FILE *fp = fopen(filename, "r");
	
	if(fp == NULL)
	{
		perror(filename);
		return 1;
	}
	
	while((fgets(filein, sizeof(filein), fp)) != NULL)
	{
		sscanf(filein, "%d %s %d \n", &srcport, destip, &destport);
		
		if((checkPort(srcport) == 1) && (validateIP(destip) == 1) && (checkPort(destport)) == 1)
		{
			memcpy(tempList.destip, destip, sizeof(destip));
			tempList.destport = destport;
			ipPortList[srcport] = tempList;
		}
		else
			continue;
	}
	fclose(fp);
	return 0;
}

/*
 * Checks a given ip if it is a valid IPv4 ip.
 */
int validateIP(char *ip)
{
	unsigned int octet1, octet2, octet3, octet4;
	
	if(sscanf(ip, "%3u.%3u.%3u.%3u", &octet1, &octet2, &octet3, &octet4) != 4)
		return 7; /* Invalid ip */
		
	if((octet1 || octet2 || octet3 || octet4) > 255)
		return 8; /* octets are not in valid range */
		
	return 1;
}

/*
 * Checks a given port for several conditions. If no conditions are 
 * triggered, the value is a valid port.
 */
int checkPort(int port)
{
	if(isdigit(port) != 0)
	{
		return 3; /* Value is not a digit */
	}
	if(port < 0)
	{
		return 4; /* No valid port specified */
	}
	else if(port > 65535)
	{
		return 5; /* More than 5 digits in port number */
	}
	else
		return 1;
}

int setSockOption(const int opt, int sd, int sdstate)
{
	if(setsockopt(sd, SOL_SOCKET, opt, &sdstate, sizeof(int)) == -1)
	{
		perror("Can't set sock option.\n");
		return -1;
	}
	return 0;
}

void displayMap()
{
	map<int, forwardInfo>::iterator iter;
	for(iter = ipPortList.begin(); iter != ipPortList.end(); ++iter )
	{
		cout << "Source Port: " << (*iter).first << " Forward IP:Port "
			<< (*iter).second.destip << ":" << (*iter).second.destport
			<< endl;
	}
}
