/**********************
*
* Progam Name: MP1. Membership Protocol.
* 
* Code authors: Suma Vangala
*
* Current file: mp2_node.h
* About this file: Header file.
* 
***********************/

#ifndef _NODE_H_
#define _NODE_H_

#include "stdincludes.h"
#include "params.h"
#include "queue.h"
#include "requests.h"
#include "emulnet.h"

/* Configuration Parameters */
char JOINADDR[30];                    /* address for introduction into the group. */
extern char *DEF_SERVADDR;            /* server address. */
extern short PORTNUM;                /* standard portnum of server to contact. */

#define TIMEOUT 100

/* Miscellaneous Parameters */
extern char *STDSTRING;
//int TIMEOUT = 100;     // Timeout period

typedef struct member{            
        struct address addr;            // my address
        int inited;                     // boolean indicating if this member is up
        int ingroup;                    // boolean indiciating if this member is in the group

        queue inmsgq;                   // queue for incoming messages

        int bfailed;                    // boolean indicating if this member has failed

        int heartbeat;					// Sequence number
        int timestamp;					// Time stamp when the sequence number is last updated
        struct membership_list *head;	// Pointer pointing to the head of the membership list(linked list)
} member;

// Structure of the membership list node
typedef struct membership_list{
	int heartbeat;
	int timestamp;
	struct address addr;
	struct membership_list *next;
} membership_list;

/* Message types */
/* Meaning of different message types
  JOINREQ - request to join the group
  JOINREP - replyto JOINREQ
  HEARTBEAT - Gossip message
*/
enum Msgtypes{
		JOINREQ,			
		JOINREP,
		HEARTBEAT
};

/* Generic message template. */
typedef struct messagehdr{ 	
	enum Msgtypes msgtype;
} messagehdr;


/* Functions in mp2_node.c */

/* Message processing routines. */
STDCLLBKRET Process_joinreq STDCLLBKARGS;
STDCLLBKRET Process_joinrep STDCLLBKARGS;
STDCLLBKRET Process_heartbeat STDCLLBKARGS;
/*
int recv_callback(void *env, char *data, int size);
int init_thisnode(member *thisnode, address *joinaddr);
*/

/*
Other routines.
*/

void nodestart(member *node, char *servaddrstr, short servport);
void nodeloop(member *node);
int recvloop(member *node);
int finishup_thisnode(member *node);
membership_list *create_membershiplist(address *env, membership_list *start1);
#endif /* _NODE_H_ */

