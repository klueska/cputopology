#include <sys/queue.h>
 
#ifndef	_SCHEDULE_
#define	_SCHEDULE_

#define NUMAS_IN_SYSTEM 16
#define SOCKETS_IN_SYSTEM 16
#define CHIPS_IN_SYSTEM 64
#define CORES_IN_SYSTEM 128

enum node_type { CORE, CHIP, SOCKET, NUMA };

struct node {
	int id;
	enum node_type type;
	CIRCLEQ_ENTRY(node) core_link;
	CIRCLEQ_ENTRY(node) chip_link;
	CIRCLEQ_ENTRY(node) socket_link;
	CIRCLEQ_ENTRY(node) numa_link;
	struct node *father;
	struct node **sons;
};

void print_available_ressources();
void build_structure_ressources(int nb_numas, int sockets_per_numa,
		     int chips_per_socket, int cores_per_chip);

/*Those lists are our 4 circular lists. If a node 
(core, chip, socket or numa) is in the corresponding list that 
means it is available and can be given to the API. */
CIRCLEQ_HEAD(core_list, node);	
CIRCLEQ_HEAD(chip_list, node);	
CIRCLEQ_HEAD(socket_list, node);		
CIRCLEQ_HEAD(numa_list, node);

/*Here are our 4 arrays of numas, sockets, chips and cores.
The index is the id of the considerated array.
*/
struct node *numas_lookup[NUMAS_IN_SYSTEM];
struct node *sockets_lookup[SOCKETS_IN_SYSTEM];
struct node *chips_lookup[CHIPS_IN_SYSTEM];
struct node *cores_lookup[CORES_IN_SYSTEM];

struct node *find_core(int coreid);
struct node *find_chip(int chipid);
struct node *find_socket(int socketid);
struct node *find_numa(int numaid);

#endif
