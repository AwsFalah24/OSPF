#ifndef ROUTING_TABLE_H
#define ROUTING_TABLE_H

#include "graph.h"
#include "dijkstra.h"

typedef struct {
    char dest[MAX_NAME_LEN];
    char next_hop[MAX_NAME_LEN];
    int  cost;
    char path_str[256];         /* e.g. "R1 -> R2 -> R4" */
    bool reachable;
} RouteEntry;

typedef struct {
    char       owner[MAX_NAME_LEN];  /* router that owns this table */
    RouteEntry routes[MAX_ROUTERS];
    int        count;
} RoutingTable;

/* Build a forwarding table from an SPF tree */
void routing_table_build(const Topology *t, const SpfTree *tree, RoutingTable *rt);

/* Print in a Cisco-inspired columnar format */
void routing_table_print(const RoutingTable *rt);

/* Compute and print routing tables for every router in the topology */
void routing_tables_print_all(const Topology *t);

#endif /* ROUTING_TABLE_H */
