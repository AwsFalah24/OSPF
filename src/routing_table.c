#include "routing_table.h"

#include <stdio.h>
#include <string.h>

void routing_table_build(const Topology *t, const SpfTree *tree, RoutingTable *rt)
{
    memset(rt, 0, sizeof(*rt));
    strncpy(rt->owner, t->routers[tree->source].name, MAX_NAME_LEN - 1);

    for (int i = 0; i < tree->entry_count; i++) {
        const SpfEntry *e = &tree->entries[i];
        /* Skip self */
        if (e->dest == tree->source)
            continue;

        RouteEntry *r = &rt->routes[rt->count++];
        strncpy(r->dest, t->routers[e->dest].name, MAX_NAME_LEN - 1);
        r->reachable = e->reachable;
        r->cost = e->cost;

        if (!e->reachable) {
            strncpy(r->next_hop, "-", MAX_NAME_LEN - 1);
            strncpy(r->path_str, "unreachable", sizeof(r->path_str) - 1);
            continue;
        }

        strncpy(r->next_hop, t->routers[e->next_hop].name, MAX_NAME_LEN - 1);

        r->path_str[0] = '\0';
        for (int p = 0; p < e->path_len; p++) {
            if (p > 0)
                strncat(r->path_str, " -> ", sizeof(r->path_str) - strlen(r->path_str) - 1);
            strncat(r->path_str, t->routers[e->path[p]].name,
                    sizeof(r->path_str) - strlen(r->path_str) - 1);
        }
    }
}

void routing_table_print(const RoutingTable *rt)
{
    printf("\n");
    printf("  +================================================================+\n");
    printf("  |  Routing Table — Router %-36s |\n", rt->owner);
    printf("  +================================================================+\n");
    printf("  %-12s %-10s %-12s %s\n", "DESTINATION", "COST", "NEXT-HOP", "FORWARDING PATH");
    printf("  %-12s %-10s %-12s %s\n",
           "------------", "----------", "------------", "---------------------------");

    if (rt->count == 0) {
        printf("  (no destinations)\n\n");
        return;
    }

    for (int i = 0; i < rt->count; i++) {
        const RouteEntry *r = &rt->routes[i];
        if (!r->reachable) {
            printf("  %-12s %-10s %-12s %s\n", r->dest, "*", "*", "unreachable");
        } else {
            char costbuf[16];
            snprintf(costbuf, sizeof(costbuf), "%d", r->cost);
            printf("  %-12s %-10s %-12s %s\n", r->dest, costbuf, r->next_hop, r->path_str);
        }
    }
    printf("  +================================================================+\n\n");
}

void routing_tables_print_all(const Topology *t)
{
    SpfTree tree;
    RoutingTable rt;

    for (int i = 0; i < t->count; i++) {
        if (!dijkstra_spf(t, i, &tree))
            continue;
        routing_table_build(t, &tree, &rt);
        routing_table_print(&rt);
    }
}
