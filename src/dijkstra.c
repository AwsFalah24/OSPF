#include "dijkstra.h"

#include <stdio.h>
#include <string.h>
#include <limits.h>

/*
 * Classic Dijkstra SPF used by OSPF for intra-area route calculation.
 *
 * Complexity: O(V^2) with a simple linear scan for the min-distance node —
 * fine for educational topologies (V <= 64). A binary heap would yield
 * O((V+E) log V) for larger graphs.
 */
bool dijkstra_spf(const Topology *t, int source, SpfTree *out)
{
    if (!t || !out || source < 0 || source >= t->count)
        return false;

    int n = t->count;
    int dist[MAX_ROUTERS];
    int prev[MAX_ROUTERS];
    bool visited[MAX_ROUTERS];

    for (int i = 0; i < n; i++) {
        dist[i] = INF_COST;
        prev[i] = -1;
        visited[i] = false;
    }
    dist[source] = 0;

    for (int iter = 0; iter < n; iter++) {
        int u = -1;
        int best = INF_COST;
        for (int i = 0; i < n; i++) {
            if (!visited[i] && dist[i] < best) {
                best = dist[i];
                u = i;
            }
        }
        if (u < 0)
            break; /* remaining nodes unreachable */

        visited[u] = true;

        const Router *ru = &t->routers[u];
        for (int k = 0; k < ru->adj_count; k++) {
            const Link *l = &ru->adj[k];
            if (!l->up)
                continue;
            int v = l->to;
            if (visited[v])
                continue;
            /* Guard against overflow */
            if (dist[u] > INF_COST - l->cost)
                continue;
            int alt = dist[u] + l->cost;
            if (alt < dist[v]) {
                dist[v] = alt;
                prev[v] = u;
            }
        }
    }

    memset(out, 0, sizeof(*out));
    out->source = source;
    out->entry_count = n;

    for (int dest = 0; dest < n; dest++) {
        SpfEntry *e = &out->entries[dest];
        e->dest = dest;
        e->cost = dist[dest];
        e->reachable = (dist[dest] < INF_COST);
        e->next_hop = -1;
        e->path_len = 0;

        if (!e->reachable)
            continue;

        /* Reconstruct path dest <- ... <- source */
        int stack[MAX_PATH_LEN];
        int sp = 0;
        int cur = dest;
        while (cur != -1 && sp < MAX_PATH_LEN) {
            stack[sp++] = cur;
            if (cur == source)
                break;
            cur = prev[cur];
        }

        /* Reverse into path[] */
        for (int i = 0; i < sp; i++)
            e->path[i] = stack[sp - 1 - i];
        e->path_len = sp;

        /* Next-hop is the first hop after source (or dest itself if adjacent) */
        if (dest == source)
            e->next_hop = source;
        else if (sp >= 2)
            e->next_hop = e->path[1];
    }

    return true;
}

void spf_print(const Topology *t, const SpfTree *tree)
{
    if (!t || !tree)
        return;

    const char *src = t->routers[tree->source].name;
    printf("\n");
    printf("  +----------------------------------------------------------+\n");
    printf("  |  SPF Tree (Dijkstra) rooted at %-24s |\n", src);
    printf("  +----------------------------------------------------------+\n");
    printf("  %-10s %-8s %-12s %s\n", "DEST", "COST", "NEXT-HOP", "PATH");
    printf("  %-10s %-8s %-12s %s\n", "----------", "--------", "------------", "----------------");

    for (int i = 0; i < tree->entry_count; i++) {
        const SpfEntry *e = &tree->entries[i];
        const char *dname = t->routers[e->dest].name;

        if (!e->reachable) {
            printf("  %-10s %-8s %-12s %s\n", dname, "unreach", "-", "-");
            continue;
        }

        const char *nh = (e->dest == tree->source)
                             ? "-"
                             : t->routers[e->next_hop].name;

        char pathbuf[256];
        pathbuf[0] = '\0';
        for (int p = 0; p < e->path_len; p++) {
            if (p > 0)
                strncat(pathbuf, " -> ", sizeof(pathbuf) - strlen(pathbuf) - 1);
            strncat(pathbuf, t->routers[e->path[p]].name,
                    sizeof(pathbuf) - strlen(pathbuf) - 1);
        }

        char costbuf[16];
        snprintf(costbuf, sizeof(costbuf), "%d", e->cost);
        printf("  %-10s %-8s %-12s %s\n", dname, costbuf, nh, pathbuf);
    }
    printf("\n");
}
