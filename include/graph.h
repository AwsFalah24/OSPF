#ifndef GRAPH_H
#define GRAPH_H

#include <stddef.h>
#include <stdbool.h>

#define MAX_ROUTERS   64
#define MAX_NAME_LEN  32
#define INF_COST      0x3FFFFFFF

typedef struct {
    int  to;          /* neighbor router index */
    int  cost;        /* OSPF link cost (metric) */
    bool up;          /* link operational state */
} Link;

typedef struct {
    char  name[MAX_NAME_LEN];
    Link *adj;        /* dynamic adjacency list */
    int   adj_count;
    int   adj_cap;
} Router;

typedef struct {
    Router routers[MAX_ROUTERS];
    int    count;
} Topology;

/* Lifecycle */
void topology_init(Topology *t);
void topology_clear(Topology *t);

/* Router / link management */
int  topology_add_router(Topology *t, const char *name);
int  topology_find_router(const Topology *t, const char *name);
bool topology_add_link(Topology *t, const char *a, const char *b, int cost);
bool topology_set_link_cost(Topology *t, const char *a, const char *b, int cost);
bool topology_set_link_state(Topology *t, const char *a, const char *b, bool up);
bool topology_remove_link(Topology *t, const char *a, const char *b);

/* Queries */
int  topology_get_cost(const Topology *t, int from, int to);
bool topology_link_is_up(const Topology *t, int from, int to);

/* I/O */
bool topology_load_file(Topology *t, const char *path);
void topology_print(const Topology *t);
void topology_print_adjacency(const Topology *t);

#endif /* GRAPH_H */
