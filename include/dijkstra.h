#ifndef DIJKSTRA_H
#define DIJKSTRA_H

#include "graph.h"

#define MAX_PATH_LEN  MAX_ROUTERS

/* Per-destination SPF result from a single source */
typedef struct {
    int  dest;
    int  cost;
    int  next_hop;              /* first hop toward dest (-1 if dest==src or unreachable) */
    int  path[MAX_PATH_LEN];    /* full hop-by-hop path including src and dest */
    int  path_len;
    bool reachable;
} SpfEntry;

/* Complete SPF tree rooted at one router */
typedef struct {
    int      source;
    SpfEntry entries[MAX_ROUTERS];
    int      entry_count;
} SpfTree;

/*
 * Run Dijkstra's algorithm (OSPF SPF) from `source`.
 * Only considers links that are currently up.
 * Returns true on success.
 */
bool dijkstra_spf(const Topology *t, int source, SpfTree *out);

/* Pretty-print an SPF tree */
void spf_print(const Topology *t, const SpfTree *tree);

#endif /* DIJKSTRA_H */
