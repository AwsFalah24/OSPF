#include "graph.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void topology_init(Topology *t)
{
    memset(t, 0, sizeof(*t));
}

void topology_clear(Topology *t)
{
    for (int i = 0; i < t->count; i++) {
        free(t->routers[i].adj);
        t->routers[i].adj = NULL;
        t->routers[i].adj_count = 0;
        t->routers[i].adj_cap = 0;
    }
    t->count = 0;
}

int topology_find_router(const Topology *t, const char *name)
{
    for (int i = 0; i < t->count; i++) {
        if (strcmp(t->routers[i].name, name) == 0)
            return i;
    }
    return -1;
}

int topology_add_router(Topology *t, const char *name)
{
    if (!name || !*name)
        return -1;
    int existing = topology_find_router(t, name);
    if (existing >= 0)
        return existing;
    if (t->count >= MAX_ROUTERS)
        return -1;

    Router *r = &t->routers[t->count];
    strncpy(r->name, name, MAX_NAME_LEN - 1);
    r->name[MAX_NAME_LEN - 1] = '\0';
    r->adj = NULL;
    r->adj_count = 0;
    r->adj_cap = 0;
    return t->count++;
}

static bool ensure_adj_cap(Router *r)
{
    if (r->adj_count < r->adj_cap)
        return true;
    int new_cap = r->adj_cap == 0 ? 4 : r->adj_cap * 2;
    Link *tmp = realloc(r->adj, (size_t)new_cap * sizeof(Link));
    if (!tmp)
        return false;
    r->adj = tmp;
    r->adj_cap = new_cap;
    return true;
}

static Link *find_link(Router *r, int to)
{
    for (int i = 0; i < r->adj_count; i++) {
        if (r->adj[i].to == to)
            return &r->adj[i];
    }
    return NULL;
}

static const Link *find_link_const(const Router *r, int to)
{
    for (int i = 0; i < r->adj_count; i++) {
        if (r->adj[i].to == to)
            return &r->adj[i];
    }
    return NULL;
}

bool topology_add_link(Topology *t, const char *a, const char *b, int cost)
{
    if (cost <= 0 || strcmp(a, b) == 0)
        return false;

    int ia = topology_add_router(t, a);
    int ib = topology_add_router(t, b);
    if (ia < 0 || ib < 0)
        return false;

    Router *ra = &t->routers[ia];
    Router *rb = &t->routers[ib];

    Link *ab = find_link(ra, ib);
    Link *ba = find_link(rb, ia);

    if (ab) {
        ab->cost = cost;
        ab->up = true;
    } else {
        if (!ensure_adj_cap(ra))
            return false;
        ra->adj[ra->adj_count++] = (Link){ .to = ib, .cost = cost, .up = true };
    }

    if (ba) {
        ba->cost = cost;
        ba->up = true;
    } else {
        if (!ensure_adj_cap(rb))
            return false;
        rb->adj[rb->adj_count++] = (Link){ .to = ia, .cost = cost, .up = true };
    }
    return true;
}

bool topology_set_link_cost(Topology *t, const char *a, const char *b, int cost)
{
    if (cost <= 0)
        return false;
    int ia = topology_find_router(t, a);
    int ib = topology_find_router(t, b);
    if (ia < 0 || ib < 0)
        return false;

    Link *ab = find_link(&t->routers[ia], ib);
    Link *ba = find_link(&t->routers[ib], ia);
    if (!ab || !ba)
        return false;

    ab->cost = cost;
    ba->cost = cost;
    return true;
}

bool topology_set_link_state(Topology *t, const char *a, const char *b, bool up)
{
    int ia = topology_find_router(t, a);
    int ib = topology_find_router(t, b);
    if (ia < 0 || ib < 0)
        return false;

    Link *ab = find_link(&t->routers[ia], ib);
    Link *ba = find_link(&t->routers[ib], ia);
    if (!ab || !ba)
        return false;

    ab->up = up;
    ba->up = up;
    return true;
}

bool topology_remove_link(Topology *t, const char *a, const char *b)
{
    int ia = topology_find_router(t, a);
    int ib = topology_find_router(t, b);
    if (ia < 0 || ib < 0)
        return false;

    /* Soft-remove: mark as down (keeps history in LSDB for demo clarity) */
    return topology_set_link_state(t, a, b, false);
}

int topology_get_cost(const Topology *t, int from, int to)
{
    if (from < 0 || to < 0 || from >= t->count || to >= t->count)
        return INF_COST;
    const Link *l = find_link_const(&t->routers[from], to);
    if (!l || !l->up)
        return INF_COST;
    return l->cost;
}

bool topology_link_is_up(const Topology *t, int from, int to)
{
    if (from < 0 || to < 0 || from >= t->count || to >= t->count)
        return false;
    const Link *l = find_link_const(&t->routers[from], to);
    return l && l->up;
}

/* Load topology file:
 *   # comments
 *   router R1
 *   link R1 R2 10
 */
bool topology_load_file(Topology *t, const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp)
        return false;

    topology_clear(t);

    char line[256];
    int lineno = 0;
    while (fgets(line, sizeof(line), fp)) {
        lineno++;
        /* strip newline and comments */
        char *hash = strchr(line, '#');
        if (hash)
            *hash = '\0';
        /* trim */
        char *p = line;
        while (*p && isspace((unsigned char)*p))
            p++;
        char *end = p + strlen(p);
        while (end > p && isspace((unsigned char)end[-1]))
            end--;
        *end = '\0';
        if (*p == '\0')
            continue;

        char cmd[32], a[MAX_NAME_LEN], b[MAX_NAME_LEN];
        int cost = 0;

        if (sscanf(p, "%31s %31s %31s %d", cmd, a, b, &cost) >= 2) {
            if (strcmp(cmd, "router") == 0) {
                if (topology_add_router(t, a) < 0) {
                    fprintf(stderr, "warn: line %d: cannot add router %s\n", lineno, a);
                }
            } else if (strcmp(cmd, "link") == 0) {
                if (cost <= 0) {
                    fprintf(stderr, "warn: line %d: invalid cost\n", lineno);
                    continue;
                }
                if (!topology_add_link(t, a, b, cost)) {
                    fprintf(stderr, "warn: line %d: cannot add link %s-%s\n", lineno, a, b);
                }
            } else {
                fprintf(stderr, "warn: line %d: unknown directive '%s'\n", lineno, cmd);
            }
        }
    }

    fclose(fp);
    return t->count > 0;
}

void topology_print(const Topology *t)
{
    printf("\n");
    printf("  +--------------------------------------------------+\n");
    printf("  |           OSPF Link-State Database               |\n");
    printf("  +--------------------------------------------------+\n");
    printf("  Routers: %d\n\n", t->count);

    if (t->count == 0) {
        printf("  (empty topology)\n\n");
        return;
    }

    /* ASCII adjacency matrix of costs */
    printf("  Cost matrix (* = down, - = no link):\n\n      ");
    for (int j = 0; j < t->count; j++)
        printf("%6s", t->routers[j].name);
    printf("\n");

    for (int i = 0; i < t->count; i++) {
        printf("  %4s", t->routers[i].name);
        for (int j = 0; j < t->count; j++) {
            if (i == j) {
                printf("%6s", "0");
                continue;
            }
            const Link *l = find_link_const(&t->routers[i], j);
            if (!l)
                printf("%6s", "-");
            else if (!l->up)
                printf("%6s", "*");
            else
                printf("%6d", l->cost);
        }
        printf("\n");
    }
    printf("\n");
}

void topology_print_adjacency(const Topology *t)
{
    printf("\n  Adjacency lists:\n");
    for (int i = 0; i < t->count; i++) {
        const Router *r = &t->routers[i];
        printf("    %-8s -> ", r->name);
        if (r->adj_count == 0) {
            printf("(none)\n");
            continue;
        }
        for (int k = 0; k < r->adj_count; k++) {
            printf("%s(cost=%d%s)%s",
                   t->routers[r->adj[k].to].name,
                   r->adj[k].cost,
                   r->adj[k].up ? "" : ",DOWN",
                   k + 1 < r->adj_count ? ", " : "\n");
        }
    }
    printf("\n");
}
