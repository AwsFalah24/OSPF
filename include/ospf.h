#ifndef OSPF_H
#define OSPF_H

#include "graph.h"
#include "dijkstra.h"
#include "routing_table.h"

/*
 * OSPF simulation engine.
 * Maintains the LSDB (link-state database = topology),
 * runs SPF on demand, and tracks change events.
 */

#define MAX_EVENTS 128

typedef enum {
    EVT_LINK_DOWN,
    EVT_LINK_UP,
    EVT_COST_CHANGE,
    EVT_ROUTER_ADDED,
    EVT_LINK_ADDED,
    EVT_TOPOLOGY_LOADED,
    EVT_SPF_RUN
} EventType;

typedef struct {
    EventType type;
    char      detail[128];
} OspfEvent;

typedef struct {
    Topology      lsdb;                 /* Link-State Database */
    SpfTree       trees[MAX_ROUTERS];   /* cached SPF trees per router */
    RoutingTable  tables[MAX_ROUTERS];  /* cached forwarding tables */
    bool          dirty;                /* true when SPF needs recompute */
    OspfEvent     events[MAX_EVENTS];
    int           event_count;
    int           spf_runs;             /* how many SPF recalculations */
} OspfSim;

void ospf_init(OspfSim *sim);
void ospf_clear(OspfSim *sim);

/* Topology mutations (mark LSDB dirty and log events) */
bool ospf_load_topology(OspfSim *sim, const char *path);
bool ospf_add_router(OspfSim *sim, const char *name);
bool ospf_add_link(OspfSim *sim, const char *a, const char *b, int cost);
bool ospf_fail_link(OspfSim *sim, const char *a, const char *b);
bool ospf_restore_link(OspfSim *sim, const char *a, const char *b);
bool ospf_update_cost(OspfSim *sim, const char *a, const char *b, int cost);

/* SPF / forwarding */
void ospf_recompute_all(OspfSim *sim);
void ospf_show_route(OspfSim *sim, const char *router);
void ospf_show_all_routes(OspfSim *sim);
void ospf_show_spf(OspfSim *sim, const char *router);
void ospf_show_topology(OspfSim *sim);
void ospf_show_events(OspfSim *sim);
void ospf_show_path(OspfSim *sim, const char *src, const char *dst);

/* Guided demo that exercises failure / cost / recovery scenarios */
void ospf_run_demo(OspfSim *sim);

#endif /* OSPF_H */
