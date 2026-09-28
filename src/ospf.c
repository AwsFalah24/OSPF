#include "ospf.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
static void pause_ms(int ms) { Sleep(ms); }
#else
#include <unistd.h>
static void pause_ms(int ms)
{
    usleep((useconds_t)ms * 1000);
}
#endif

static void log_event(OspfSim *sim, EventType type, const char *detail)
{
    if (sim->event_count >= MAX_EVENTS) {
        /* shift left to keep a rolling window */
        memmove(&sim->events[0], &sim->events[1],
                (MAX_EVENTS - 1) * sizeof(OspfEvent));
        sim->event_count = MAX_EVENTS - 1;
    }
    OspfEvent *e = &sim->events[sim->event_count++];
    e->type = type;
    strncpy(e->detail, detail, sizeof(e->detail) - 1);
    e->detail[sizeof(e->detail) - 1] = '\0';
}

static const char *event_name(EventType type)
{
    switch (type) {
    case EVT_LINK_DOWN:        return "LINK-DOWN";
    case EVT_LINK_UP:          return "LINK-UP";
    case EVT_COST_CHANGE:      return "COST-CHANGE";
    case EVT_ROUTER_ADDED:     return "ROUTER-ADD";
    case EVT_LINK_ADDED:       return "LINK-ADD";
    case EVT_TOPOLOGY_LOADED:  return "TOPO-LOAD";
    case EVT_SPF_RUN:          return "SPF-RUN";
    default:                   return "EVENT";
    }
}

void ospf_init(OspfSim *sim)
{
    memset(sim, 0, sizeof(*sim));
    topology_init(&sim->lsdb);
    sim->dirty = true;
}

void ospf_clear(OspfSim *sim)
{
    topology_clear(&sim->lsdb);
    sim->dirty = true;
    sim->event_count = 0;
    sim->spf_runs = 0;
}

bool ospf_load_topology(OspfSim *sim, const char *path)
{
    if (!topology_load_file(&sim->lsdb, path))
        return false;
    char buf[128];
    snprintf(buf, sizeof(buf), "Loaded topology from %s (%d routers)",
             path, sim->lsdb.count);
    log_event(sim, EVT_TOPOLOGY_LOADED, buf);
    sim->dirty = true;
    ospf_recompute_all(sim);
    return true;
}

bool ospf_add_router(OspfSim *sim, const char *name)
{
    if (topology_add_router(&sim->lsdb, name) < 0)
        return false;
    char buf[128];
    snprintf(buf, sizeof(buf), "Router %s added to LSDB", name);
    log_event(sim, EVT_ROUTER_ADDED, buf);
    sim->dirty = true;
    return true;
}

bool ospf_add_link(OspfSim *sim, const char *a, const char *b, int cost)
{
    if (!topology_add_link(&sim->lsdb, a, b, cost))
        return false;
    char buf[128];
    snprintf(buf, sizeof(buf), "Link %s--%s cost=%d installed", a, b, cost);
    log_event(sim, EVT_LINK_ADDED, buf);
    sim->dirty = true;
    return true;
}

bool ospf_fail_link(OspfSim *sim, const char *a, const char *b)
{
    if (!topology_set_link_state(&sim->lsdb, a, b, false))
        return false;
    char buf[128];
    snprintf(buf, sizeof(buf), "Link %s--%s FAILED — flooding LSA update", a, b);
    log_event(sim, EVT_LINK_DOWN, buf);
    sim->dirty = true;
    ospf_recompute_all(sim);
    return true;
}

bool ospf_restore_link(OspfSim *sim, const char *a, const char *b)
{
    if (!topology_set_link_state(&sim->lsdb, a, b, true))
        return false;
    char buf[128];
    snprintf(buf, sizeof(buf), "Link %s--%s RESTORED — flooding LSA update", a, b);
    log_event(sim, EVT_LINK_UP, buf);
    sim->dirty = true;
    ospf_recompute_all(sim);
    return true;
}

bool ospf_update_cost(OspfSim *sim, const char *a, const char *b, int cost)
{
    if (!topology_set_link_cost(&sim->lsdb, a, b, cost))
        return false;
    char buf[128];
    snprintf(buf, sizeof(buf), "Link %s--%s cost changed to %d", a, b, cost);
    log_event(sim, EVT_COST_CHANGE, buf);
    sim->dirty = true;
    ospf_recompute_all(sim);
    return true;
}

void ospf_recompute_all(OspfSim *sim)
{
    Topology *t = &sim->lsdb;
    for (int i = 0; i < t->count; i++) {
        dijkstra_spf(t, i, &sim->trees[i]);
        routing_table_build(t, &sim->trees[i], &sim->tables[i]);
    }
    sim->spf_runs++;
    sim->dirty = false;

    char buf[128];
    snprintf(buf, sizeof(buf),
             "SPF recalculated for all %d routers (run #%d)",
             t->count, sim->spf_runs);
    log_event(sim, EVT_SPF_RUN, buf);
}

void ospf_show_route(OspfSim *sim, const char *router)
{
    if (sim->dirty)
        ospf_recompute_all(sim);

    int idx = topology_find_router(&sim->lsdb, router);
    if (idx < 0) {
        printf("  %% Error: unknown router '%s'\n", router);
        return;
    }
    routing_table_print(&sim->tables[idx]);
}

void ospf_show_all_routes(OspfSim *sim)
{
    if (sim->dirty)
        ospf_recompute_all(sim);
    for (int i = 0; i < sim->lsdb.count; i++)
        routing_table_print(&sim->tables[i]);
}

void ospf_show_spf(OspfSim *sim, const char *router)
{
    if (sim->dirty)
        ospf_recompute_all(sim);

    int idx = topology_find_router(&sim->lsdb, router);
    if (idx < 0) {
        printf("  %% Error: unknown router '%s'\n", router);
        return;
    }
    spf_print(&sim->lsdb, &sim->trees[idx]);
}

void ospf_show_topology(OspfSim *sim)
{
    topology_print(&sim->lsdb);
    topology_print_adjacency(&sim->lsdb);
    printf("  SPF runs so far: %d | LSDB dirty: %s\n\n",
           sim->spf_runs, sim->dirty ? "yes" : "no");
}

void ospf_show_events(OspfSim *sim)
{
    printf("\n  +----------------------------------------------------------+\n");
    printf("  |              OSPF Event Log (LSA / SPF)                  |\n");
    printf("  +----------------------------------------------------------+\n");
    if (sim->event_count == 0) {
        printf("  (no events yet)\n\n");
        return;
    }
    for (int i = 0; i < sim->event_count; i++) {
        printf("  [%02d] %-12s  %s\n", i + 1,
               event_name(sim->events[i].type),
               sim->events[i].detail);
    }
    printf("\n");
}

void ospf_show_path(OspfSim *sim, const char *src, const char *dst)
{
    if (sim->dirty)
        ospf_recompute_all(sim);

    int si = topology_find_router(&sim->lsdb, src);
    int di = topology_find_router(&sim->lsdb, dst);
    if (si < 0 || di < 0) {
        printf("  %% Error: unknown router\n");
        return;
    }

    const SpfEntry *e = &sim->trees[si].entries[di];
    printf("\n  Shortest path %s -> %s:\n", src, dst);
    if (!e->reachable) {
        printf("    UNREACHABLE (no SPF path in current LSDB)\n\n");
        return;
    }
    printf("    Cost : %d\n", e->cost);
    printf("    Path : ");
    for (int p = 0; p < e->path_len; p++) {
        printf("%s", sim->lsdb.routers[e->path[p]].name);
        if (p + 1 < e->path_len)
            printf(" -> ");
    }
    printf("\n");
    if (e->dest != e->path[0])
        printf("    Next-hop from %s: %s\n", src,
               sim->lsdb.routers[e->next_hop].name);
    printf("\n");
}

/* ---------- Guided demo ---------- */

static void demo_step(const char *title)
{
    printf("\n");
    printf("  ############################################################\n");
    printf("  #  %s\n", title);
    printf("  ############################################################\n");
}

void ospf_run_demo(OspfSim *sim)
{
    ospf_clear(sim);

    demo_step("STEP 1 — Build enterprise campus topology");
    /*
     * Topology (OSPF costs = reference bandwidth / interface bandwidth style):
     *
     *          [Core]
     *       R1 ----10---- R2
     *       | \          / |
     *      20  15      15  20
     *       |   \      /   |
     *       R3 ---5--- R4
     *       |          |
     *      30         30
     *       |          |
     *       R5 ---10-- R6
     *
     * R1/R2 = core, R3/R4 = distribution, R5/R6 = access
     */
    ospf_add_link(sim, "R1", "R2", 10);
    ospf_add_link(sim, "R1", "R3", 20);
    ospf_add_link(sim, "R1", "R4", 15);
    ospf_add_link(sim, "R2", "R3", 15);
    ospf_add_link(sim, "R2", "R4", 20);
    ospf_add_link(sim, "R3", "R4", 5);
    ospf_add_link(sim, "R3", "R5", 30);
    ospf_add_link(sim, "R4", "R6", 30);
    ospf_add_link(sim, "R5", "R6", 10);

    ospf_recompute_all(sim);
    ospf_show_topology(sim);
    pause_ms(400);

    demo_step("STEP 2 — Initial SPF / routing tables (steady state)");
    printf("  Focusing on R1 (core) and R5 (access):\n");
    ospf_show_route(sim, "R1");
    ospf_show_path(sim, "R1", "R6");
    ospf_show_path(sim, "R5", "R2");
    pause_ms(400);

    demo_step("STEP 3 — Link failure: R1--R4 goes DOWN");
    printf("  Simulating fiber cut between core R1 and distribution R4...\n");
    ospf_fail_link(sim, "R1", "R4");
    printf("  SPF automatically recomputed. New path R1 -> R6:\n");
    ospf_show_path(sim, "R1", "R6");
    ospf_show_route(sim, "R1");
    pause_ms(400);

    demo_step("STEP 4 — Cost update: prefer alternate core path");
    printf("  Raising R2--R4 cost from 20 to 50 (congested uplink)...\n");
    ospf_update_cost(sim, "R2", "R4", 50);
    printf("  Path R2 -> R5 after cost change:\n");
    ospf_show_path(sim, "R2", "R5");
    ospf_show_spf(sim, "R2");
    pause_ms(400);

    demo_step("STEP 5 — Cascading failure: R3--R5 also fails");
    printf("  Access uplink R3--R5 fails — R5 must reroute via R6...\n");
    ospf_fail_link(sim, "R3", "R5");
    ospf_show_path(sim, "R5", "R1");
    ospf_show_route(sim, "R5");
    pause_ms(400);

    demo_step("STEP 6 — Recovery: restore failed links");
    ospf_restore_link(sim, "R1", "R4");
    ospf_restore_link(sim, "R3", "R5");
    ospf_update_cost(sim, "R2", "R4", 20);
    printf("  Topology restored to baseline. Verifying R1 -> R6:\n");
    ospf_show_path(sim, "R1", "R6");
    ospf_show_topology(sim);

    demo_step("STEP 7 — Event log summary");
    ospf_show_events(sim);

    printf("  Demo complete. Type 'help' for interactive commands, or 'quit' to exit.\n\n");
}
