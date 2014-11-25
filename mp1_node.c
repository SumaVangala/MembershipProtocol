/**********************
*
* Progam Name: MP1. Membership Protocol
* 
* Code authors: Suma Vangala
*
* Current file: mp1_node.c
* About this file: Member Node Implementation
* 
***********************/

#include "mp1_node.h"
#include "emulnet.h"
#include "MPtemplate.h"
#include "log.h"


/*
 *
 * Routines for introducer and current time.
 *
 */

char NULLADDR[] = {0,0,0,0,0,0};
int isnulladdr( address *addr){
    return (memcmp(addr, NULLADDR, 6)==0?1:0);
}

/* 
Return the address of the introducer member. 
*/
address getjoinaddr(void){

    address joinaddr;

    memset(&joinaddr, 0, sizeof(address));
    *(int *)(&joinaddr.addr)=1;
    *(short *)(&joinaddr.addr[4])=0;

    return joinaddr;
}

/*
 *
 * Message Processing routines.
 *
 */

/* 
Received a JOINREQ (joinrequest) message.
*/
void Process_joinreq(void *env, char *data, int size)
{
	member *intro_node = (member *) env;
	address *new_node_addr = (address *) data;
    
	// Creating entry to be added to the membership list
	membership_list *new_node;
	new_node = malloc(sizeof(membership_list));
	new_node->heartbeat = 0;
	new_node->timestamp = 0;
	memcpy(&new_node->addr, new_node_addr->addr, sizeof(address));

	// Adding nodes to the membership list(linked list)
	if(intro_node->head == NULL){
		// Adding introducer node to the membership list
		membership_list *temp_node;
		temp_node = malloc(sizeof(membership_list));
		temp_node->heartbeat = 0;
		temp_node->timestamp = 0;
		memcpy(&temp_node -> addr, &intro_node->addr, sizeof(address));
		temp_node->next = NULL;

		new_node->next = temp_node;
		intro_node->head = new_node;
		logNodeAdd(&intro_node->addr, &intro_node->addr);
	}
	else{
		new_node->next = intro_node->head;
		intro_node->head = new_node;
	}

	// Sending reply to each node along with the membership list
	// which has requested introducer to join the group
	messagehdr *msg;
	size_t msgsize = sizeof(messagehdr) + sizeof(membership_list);
	msg=malloc(msgsize);
	msg->msgtype=JOINREP;
	memcpy((char *)(msg+1), intro_node->head, sizeof(membership_list));
	MPp2psend(&intro_node->addr, new_node_addr, (char *)msg, msgsize);
	logNodeAdd(&intro_node->addr, new_node_addr);

	return;
}

/* 
Received a JOINREP (joinreply) message. 
*/
void Process_joinrep(void *env, char *data, int size)
{
	membership_list *pktdata = (membership_list *)data;
	member *node = (member *)env;
	// Creating membership list for this node from the head node received from introducer
	membership_list *node_head = create_membershiplist(&node->addr, pktdata);

	// Incrementing the node's sequence number before sending it to its membership list
	node->head = node_head;
	node->ingroup = 1;
	node->heartbeat += 1;
	node->timestamp = getcurrtime();

	membership_list *temp_node = node_head;

	// Sending heart beats to all the members in its membership list
	while(temp_node!=NULL){
		messagehdr *msg;
		size_t msgsize = sizeof(messagehdr) + sizeof(address) + sizeof(int);
		msg = malloc(msgsize);
		msg->msgtype = HEARTBEAT;
		memcpy((int *)(msg+1), &node->heartbeat, sizeof(int));
		memcpy((char *)(msg+2), &node->addr, sizeof(address));

		if(!(memcmp(&node->addr, &temp_node->addr, sizeof(address)) == 0))
			MPp2psend(&node->addr, &temp_node->addr, (char *)msg, msgsize);

		temp_node = temp_node->next;
	}

	return;
}

membership_list *create_membershiplist(address *env, membership_list *start_node)
{
	membership_list *new_head = NULL, *previous = NULL;

	// Iterate through each node sent by the introducer and create
	// a new linked list(membership list) for this node
	// and return the head of the list

	while(start_node != NULL)
	{
		membership_list * temp_node = (membership_list *) malloc (sizeof(membership_list));
		temp_node->heartbeat = start_node->heartbeat;
		temp_node->timestamp = start_node->timestamp;
		temp_node->addr = start_node->addr;
		temp_node->next = NULL;

		if(new_head == NULL)
		{
			new_head = temp_node;
			previous = temp_node;
		}
		else
		{
			previous->next = temp_node;
			previous = temp_node;
		}

		logNodeAdd(env, &start_node->addr);
		start_node = start_node->next;
	}

	return new_head;
}

/*
Received a HEARTBEAT message
*/
void Process_heartbeat(void *env, char *data, int size)
{
	member *member_node = (member *)env;

	// unpacking the data
	int *heartbeat_value = (int *)data;
	address *nodeaddr = (address *)(heartbeat_value + 1);

	membership_list *temp_node = member_node->head;
	int node_found = 0;

	// Find the node in the membership list and update the heart beat along with time stamp
	while(temp_node != NULL){
		if(memcmp(&temp_node->addr, nodeaddr, sizeof(address)) == 0){
			node_found = 1;
			temp_node->heartbeat = *heartbeat_value;
			temp_node->timestamp = getcurrtime();
			break;
		}
		temp_node = temp_node->next;
	}

	// If the node is not found in the membership list, insert it
	if(!node_found){
		membership_list *new_node = malloc(sizeof(membership_list));
		new_node->heartbeat = *heartbeat_value;
		new_node->timestamp = getcurrtime();
		memcpy(&new_node->addr, nodeaddr->addr, sizeof(address));
		new_node->next = member_node->head;
		member_node->head = new_node;
		logNodeAdd(&member_node->addr, nodeaddr);
	}

	return;
}

/* 
Array of Message handlers. 
*/
void ( ( * MsgHandler [20] ) STDCLLBKARGS )={
/* Message processing operations at the P2P layer. */
    Process_joinreq, 
    Process_joinrep,
    Process_heartbeat
};

/* 
Called from nodeloop() on each received packet dequeue()-ed from node->inmsgq. 
Parse the packet, extract information and process. 
env is member *node, data is 'messagehdr'. 
*/
int recv_callback(void *env, char *data, int size){

    member *node = (member *) env;
    messagehdr *msghdr = (messagehdr *)data;
    char *pktdata = (char *)(msghdr+1);

    if(size < sizeof(messagehdr)){
#ifdef DEBUGLOG
        LOG(&((member *)env)->addr, "Faulty packet received - ignoring");
#endif
        return -1;
    }

#ifdef DEBUGLOG
    LOG(&((member *)env)->addr, "Received msg type %d with %d B payload", msghdr->msgtype, size - sizeof(messagehdr));
#endif

    if((node->ingroup && msghdr->msgtype >= 0 && msghdr->msgtype <= HEARTBEAT)
        || (!node->ingroup && msghdr->msgtype==JOINREP))            
            /* if not yet in group, accept only JOINREPs */
        MsgHandler[msghdr->msgtype](env, pktdata, size-sizeof(messagehdr));
    /* else ignore (garbled message) */
    free(data);

    return 0;

}

/*
 *
 * Initialization and cleanup routines.
 *
 */

/* 
Find out who I am, and start up. 
*/
int init_thisnode(member *thisnode, address *joinaddr){
    
    if(MPinit(&thisnode->addr, PORTNUM, (char *)joinaddr)== NULL){ /* Calls ENInit */
#ifdef DEBUGLOG
        LOG(&thisnode->addr, "MPInit failed");
#endif
        exit(1);
    }
#ifdef DEBUGLOG
    else LOG(&thisnode->addr, "MPInit succeeded. Hello.");
#endif

    thisnode->bfailed=0;
    thisnode->inited=1;
    thisnode->ingroup=0;
    /* node is up! */

    return 0;
}


/* 
Clean up this node. 
*/
int finishup_thisnode(member *node){
	membership_list *node_head = node->head;
	membership_list *temp_node = NULL;

	// Freeing the memory of all the nodes in the membership list
	while(node_head != NULL){
		temp_node = node_head;
		node_head = node_head->next;
		free(temp_node);
	}
    return 0;
}


/* 
 *
 * Main code for a node 
 *
 */

/* 
Introduce self to group. 
*/
int introduceselftogroup(member *node, address *joinaddr){
    
    messagehdr *msg;
#ifdef DEBUGLOG
    static char s[1024];
#endif

    if(memcmp(&node->addr, joinaddr, 4*sizeof(char)) == 0){
        /* I am the group booter (first process to join the group). Boot up the group. */
#ifdef DEBUGLOG
        LOG(&node->addr, "Starting up group...");
#endif

        node->ingroup = 1;
    }
    else{
        size_t msgsize = sizeof(messagehdr) + sizeof(address);
        msg=malloc(msgsize);

    /* create JOINREQ message: format of data is {struct address myaddr} */
        msg->msgtype=JOINREQ;
        memcpy((char *)(msg+1), &node->addr, sizeof(address));

#ifdef DEBUGLOG
        sprintf(s, "Trying to join...");
        LOG(&node->addr, s);
#endif

    /* send JOINREQ message to introducer member. */
        MPp2psend(&node->addr, joinaddr, (char *)msg, msgsize);
        
        free(msg);
    }

    return 1;

}

/* 
Called from nodeloop(). 
*/
void checkmsgs(member *node){
    void *data;
    int size;

    /* Dequeue waiting messages from node->inmsgq and process them. */
	
    while((data = dequeue(&node->inmsgq, &size)) != NULL) {
        recv_callback((void *)node, data, size); 
    }
    return;
}


/* 
Executed periodically for each member. 
Performs necessary periodic operations. 
Called by nodeloop(). 
*/
void nodeloopops(member *node){
	membership_list *node_head = node->head;
	membership_list *previous = NULL, *temp_node = NULL;

	// Removing a failed node from the group and freeing its membership list
	if(node->bfailed){
		node->ingroup = 0;
		while(node_head != NULL){
			temp_node = node_head;
			node_head = node_head->next;
			free(temp_node);
		}
	}
	else{
		node->heartbeat += 1;
		node->timestamp = getcurrtime();

		while(node_head != NULL){
			// Removing the node from the membership list from which
			// heart beat was not received from a time more than timeout
			if((getcurrtime() - (node_head->timestamp)) > TIMEOUT){
				if(previous == NULL){
					temp_node = node_head;
					node->head = node_head->next;
					node_head = node_head->next;
					logNodeRemove(&node->addr, &temp_node->addr);
					free(temp_node);
				}
				else{
					temp_node = node_head;
					previous->next = node_head->next;
					node_head = node_head->next;
					logNodeRemove(&node->addr, &temp_node->addr);
					free(temp_node);
				}
			}
			else{ // Sending heart beat to the node which is up and running
				messagehdr *msg;
				size_t msgsize = sizeof(messagehdr) + sizeof(int) + sizeof(address);
				msg = malloc(msgsize);
				msg->msgtype = HEARTBEAT;
				memcpy((int *)(msg+1), &node->heartbeat, sizeof(int));
				memcpy((char *)(msg+2), &node->addr, sizeof(address));
				MPp2psend(&node->addr, &node_head->addr, (char *)msg, msgsize);

				previous = node_head;
				node_head = node_head->next;
			}
		}
	}
	return;
}

/* 
Executed periodically at each member. Called from app.c.
*/
void nodeloop(member *node){
    if (node->bfailed) return;

    checkmsgs(node);

    /* Wait until you're in the group... */
    if(!node->ingroup) return ;

    /* ...then jump in and share your responsibilites! */
    nodeloopops(node);
    
    return;
}

/* 
All initialization routines for a member. Called by app.c. 
*/
void nodestart(member *node, char *servaddrstr, short servport){

    address joinaddr=getjoinaddr();

    /* Self booting routines */
    if(init_thisnode(node, &joinaddr) == -1){

#ifdef DEBUGLOG
        LOG(&node->addr, "init_thisnode failed. Exit.");
#endif
        exit(1);
    }

    if(!introduceselftogroup(node, &joinaddr)){
        finishup_thisnode(node);
#ifdef DEBUGLOG
        LOG(&node->addr, "Unable to join self to group. Exiting.");
#endif
        exit(1);
    }

    return;
}

/* 
Enqueue a message (buff) onto the queue env. 
*/
int enqueue_wrppr(void *env, char *buff, int size){    return enqueue((queue *)env, buff, size);}

/* 
Called by a member to receive messages currently waiting for it. 
*/
int recvloop(member *node){
    if (node->bfailed) return -1;
    else return MPrecv(&(node->addr), enqueue_wrppr, NULL, 1, &node->inmsgq); 
    /* Fourth parameter specifies number of times to 'loop'. */
}

