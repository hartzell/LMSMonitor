/*
 *	sliminfo.c
 *
 *	(c) 2015 László TÓTH
 *
 *	Todo:	Reconect to server
 *
 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 3 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	See <http://www.gnu.org/licenses/> to get a copy of the GNU General
 *	Public License.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <pthread.h>

#include "common.h"
#include "tagUtils.h"
#include "sliminfo.h"

int   LMSPort;
char *LMSHost  = NULL;

char playerID[BSIZE] = {0};
char query[BSIZE]    = {0};
char stb[BSIZE];

int sockFD = 0;
struct sockaddr_in  serv_addr;

tag 	    tagStore[MAXTAG_TYPES];
int         refreshRequ;
pthread_t   sliminfoThread;

int discoverPlayer(char *playerName) {
	char qBuffer[BSIZE];
	char aBuffer[BSIZE];
	int  bytes;

	// I've not found this feature in the CLI spec,
	//	but if you send the player name, the server answer with the player ID.
	if (playerName != NULL) {
		if(strlen(playerName) > (BSIZE/3))					 { abort("ERROR too long player name!"); }

		encode(playerName, aBuffer);
		sprintf(qBuffer, "%s\n", aBuffer);
		if (write(sockFD, qBuffer, strlen(qBuffer)) < 0)	 { abort("ERROR writing to socket!"); }
		if ((bytes = read(sockFD, aBuffer, BSIZE-1)) < 0)	 { abort("ERROR reading from socket!"); }
		aBuffer[bytes] = 0;

		if (strncmp(qBuffer, aBuffer, strlen(aBuffer)) == 0) { abort("Player not found!"); }
		decode(aBuffer, playerID);

	} else {
		return -1;
	}

	sprintf (stb, "PlayerName: %s, PlayerID: %s\n", playerName, playerID);
	putMSG  (stb, LL_INFO);

	return 0;
}

int setStaticServer(void) {
	LMSPort = 9090;
	LMSHost = NULL;		// autodiscovery
//	LMSHost = (char *)"127.0.0.1";
//	LMSHost = (char *)"192.168.1.252";
	return 0;
}
/*
 * LMS server discover
 *
 * code from: https://code.google.com/p/squeezelite/source/browse/slimproto.c
 *
 */
in_addr_t getServerAddress(void) {
	#define PORT 3483

	struct sockaddr_in d;
	struct sockaddr_in s;
	char   *buf;
	struct pollfd pollinfo;

	if (LMSHost != NULL) {
		// Static server address
		memset(&s, 0, sizeof(s));
		inet_pton(AF_INET, LMSHost, &(s.sin_addr));

	} else {
		// Discover server address
		int disc_sock = socket(AF_INET, SOCK_DGRAM, 0);

		socklen_t enable = 1;
		setsockopt(disc_sock, SOL_SOCKET, SO_BROADCAST, (const void *)&enable, sizeof(enable));

		buf = (char *)"e";

		memset(&d, 0, sizeof(d));
		d.sin_family = AF_INET;
		d.sin_port = htons(PORT);
		d.sin_addr.s_addr = htonl(INADDR_BROADCAST);

		pollinfo.fd = disc_sock;
		pollinfo.events = POLLIN;

		do {
			putMSG ("Sending discovery...\n", LL_INFO);
			memset(&s, 0, sizeof(s));

			if (sendto(disc_sock, buf, 1, 0, (struct sockaddr *)&d, sizeof(d)) < 0) {
				putMSG ("Error sending disovery\n", LL_INFO);
			}

			if (poll(&pollinfo, 1, 5000) == 1) {
				char readbuf[10];
				socklen_t slen = sizeof(s);
				recvfrom(disc_sock, readbuf, 10, 0, (struct sockaddr *)&s, &slen);
				sprintf(stb, "Got response from: %s:%d\n", inet_ntoa(s.sin_addr), ntohs(s.sin_port));
				putMSG (stb, LL_INFO);
			}
		} while (s.sin_addr.s_addr == 0);

		close(disc_sock);
	}

	return s.sin_addr.s_addr;
}

/*******************************************************************************
 *
 ******************************************************************************/
void askRefresh(void) {
	refreshRequ = true;
	}

/*******************************************************************************
 *
 ******************************************************************************/
int isRefreshed(void) {
	return !refreshRequ;
	}

/*******************************************************************************
 *
 ******************************************************************************/
void refreshed(void) {
	refreshRequ = false;
	}

/*******************************************************************************
 *
 ******************************************************************************/
int connectServer(void) {
	int sfd;

	if (setStaticServer() < 0) {
		abort("Failed to find LMS server!");
	}

	if ((sfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		abort("Failed to opening socket!");
	}

	memset(&serv_addr, 0, sizeof(serv_addr));

//	memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);

	serv_addr.sin_family	  = AF_INET;
	serv_addr.sin_addr.s_addr = getServerAddress();
	serv_addr.sin_port 		  = htons(LMSPort);

	if (connect(sfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		fprintf(stderr, "No such host: %s\n", inet_ntoa(serv_addr.sin_addr));
		close(sfd);
		return -1;
	}

	return sfd;
}

/*******************************************************************************
 *
 ******************************************************************************/
void closeSliminfo(void) {
	if (sockFD > 0) {
		close(sockFD);
	}

	for(int i = 0; i < MAXTAG_TYPES; i++) {
		if(tagStore[i].tagData != NULL) {
			free(tagStore[i].tagData);
		}
	}
}

/*******************************************************************************
 *
 ******************************************************************************/
tag *initTagStore(void) {
	tagStore[SAMPLESIZE].name  = "samplesize";
	tagStore[SAMPLERATE].name  = "samplerate";
	tagStore[TIME].name 	   = "time";
	tagStore[DURATION].name	   = "duration";
	tagStore[TITLE].name 	   = "title";
	tagStore[ALBUM].name 	   = "album";
	tagStore[ARTIST].name 	   = "artist";
	tagStore[ALBUMARTIST].name = "albumartist";
	tagStore[COMPOSER].name	   = "composer";
	tagStore[CONDUCTOR].name   = "conductor";
	tagStore[MODE].name 	   = "mode";
	tagStore[VOLUME].name 	   = "mixer%20volume";

	for(int i = 0; i < MAXTAG_TYPES; i++) {
		if ((tagStore[i].tagData = (char *)malloc(MAXTAG_DATA * sizeof(char))) == NULL) {
			closeSliminfo();
			return NULL;
		} else {
			tagStore[i].displayName = "";
			tagStore[i].valid	= false;
			tagStore[i].changed	= false;
		}
	}
	return tagStore;
}

void *serverPolling(void *x_voidptr){
	char buffer[BSIZE];
	char tagData[BSIZE];
	int  rbytes;

	while (true) {
		if (!isRefreshed()) {
			if (write(sockFD, query, strlen(query)) < 0)		{ abort("ERROR writing to socket"); }
			if ((rbytes = read(sockFD, buffer, BSIZE-1)) < 0)	{ abort("ERROR reading from socket"); }
			buffer[rbytes] = 0;

			for(int i = 0; i < MAXTAG_TYPES; i++) {
				if ((getTag((char *)tagStore[i].name, buffer, tagData, BSIZE)) != NULL) {
					if (strcmp(tagData, tagStore[i].tagData) != 0) {
						strncpy(tagStore[i].tagData, tagData, MAXTAG_DATA);
						tagStore[i].changed = true;
					}
					tagStore[i].valid = true;
				} else {
					tagStore[i].valid = false;
				}
			}

//		pTime	= getMinute("time", buffer);
//		duration = getMinute("duration", buffer);

//		printf("%3ld:%02ld %s %ld:%02ld\n", pTime/60, pTime%60, isPlaying(buffer) ? ">" : "II",  duration/60, duration%60);

//		printf("\n");
			refreshed();
			sleep(1);
		}
	}
	return NULL;
}


tag *initSliminfo(char *playerName) {
	if (setStaticServer() < 0)			{ return NULL; }
	if ((sockFD = connectServer()) < 0)	{ return NULL; }
	if (discoverPlayer(playerName) < 0)	{ return NULL; }

	sprintf(query, "%s status - 1 tags:aAlCIT\n", playerID); // alrTy
	int x = 0;

	if (initTagStore() != NULL) {
 		askRefresh();
	   	if (pthread_create(&sliminfoThread, NULL, serverPolling, &x) != 0) {
			closeSliminfo();
			abort("Failed to create sliminfo thread!");
		}
	} else {return NULL;}

	return tagStore;
}
