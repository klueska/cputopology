#include <sys/queue.h>
 
#ifndef	_SCHEDULE_
#define	_SCHEDULE_

#define THREADS_PER_CORE 2
#define CORES_PER_SOCKET 2
#define SOCKETS_PER_NUMA 1
#define NUMAS_IN_SYSTEM 1
#define SOCKETS_IN_SYSTEM 1
#define CORES_IN_SYSTEM 2
#define THREADS_IN_SYSTEM 4

enum node_type { THREAD, CORE, SOCKET, NUMA };

struct node {
	int id;
	enum node_type type;
	CIRCLEQ_ENTRY(node) thread_link;
	CIRCLEQ_ENTRY(node) core_link;
	CIRCLEQ_ENTRY(node) socket_link;
	CIRCLEQ_ENTRY(node) numa_link;
	struct node *father;
	struct node **sons;
};

void print_val();

/*Those lists are our 4 circular lists. If a node 
(thread, core, socket or numa) is in the corresponding list that 
means it is available and can be given to the API. */
CIRCLEQ_HEAD(thread_list, node);	
CIRCLEQ_HEAD(core_list, node);	
CIRCLEQ_HEAD(socket_list, node);		
CIRCLEQ_HEAD(numa_list, node);

/*Here are our 4 arrays of numas, sockets, cores and threads.
The index is the id of the considerated array.
*/
struct node *numas_lookup[NUMAS_IN_SYSTEM];
struct node *sockets_lookup[SOCKETS_IN_SYSTEM];
struct node *cores_lookup[CORES_IN_SYSTEM];
struct node *threads_lookup[THREADS_IN_SYSTEM];

struct node *find_thread(int threadid);
struct node *find_core(int coreid);
struct node *find_socket(int socketid);
struct node *find_numa(int numaid);

#endif
