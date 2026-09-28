#include "cli.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void cli_print_banner(void)
{
    printf("\n");
    printf("  ================================================================\n");
    printf("   OSPF / Dijkstra Routing Simulator\n");
    printf("   Link-State Routing  |  SPF (Dijkstra)  |  Forwarding Tables\n");
    printf("  ================================================================\n");
    printf("   Type 'help' for commands, 'demo' for a guided walkthrough.\n");
    printf("  ================================================================\n\n");
}

void cli_print_help(void)
{
    printf("\n");
    printf("  Commands (Cisco-inspired):\n");
    printf("  ----------------------------------------------------------------\n");
    printf("  load <file>                  Load topology file into LSDB\n");
    printf("  add router <name>            Add a router to the topology\n");
    printf("  add link <A> <B> <cost>      Add bidirectional link with cost\n");
    printf("  fail <A> <B>                 Simulate link failure (LSA flood)\n");
    printf("  restore <A> <B>              Restore a failed link\n");
    printf("  cost <A> <B> <newcost>       Update link metric / trigger SPF\n");
    printf("  show topology                Display LSDB cost matrix + adj\n");
    printf("  show ip route [router]       Routing table (all or one router)\n");
    printf("  show spf <router>            SPF tree rooted at router\n");
    printf("  show path <src> <dst>        Lowest-cost path between routers\n");
    printf("  show events                  LSA / SPF event log\n");
    printf("  recompute                    Force SPF recalculation\n");
    printf("  demo                         Run guided failure/recovery demo\n");
    printf("  help                         Show this help\n");
    printf("  quit / exit                  Leave the simulator\n");
    printf("  ----------------------------------------------------------------\n\n");
}

static void strip_newline(char *s)
{
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r'))
        s[--n] = '\0';
}

static int tokenize(char *line, char **argv, int max)
{
    int argc = 0;
    char *p = line;
    while (*p && argc < max) {
        while (*p && isspace((unsigned char)*p))
            p++;
        if (!*p)
            break;
        argv[argc++] = p;
        while (*p && !isspace((unsigned char)*p))
            p++;
        if (*p)
            *p++ = '\0';
    }
    return argc;
}

void cli_run(OspfSim *sim)
{
    char line[512];
    char *argv[16];

    cli_print_banner();

    for (;;) {
        printf("ospf-sim# ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }
        strip_newline(line);
        int argc = tokenize(line, argv, 16);
        if (argc == 0)
            continue;

        if (strcmp(argv[0], "quit") == 0 || strcmp(argv[0], "exit") == 0) {
            printf("  Session ended.\n");
            break;
        }

        if (strcmp(argv[0], "help") == 0 || strcmp(argv[0], "?") == 0) {
            cli_print_help();
            continue;
        }

        if (strcmp(argv[0], "demo") == 0) {
            ospf_run_demo(sim);
            continue;
        }

        if (strcmp(argv[0], "load") == 0) {
            if (argc < 2) {
                printf("  %% Usage: load <file>\n");
                continue;
            }
            if (ospf_load_topology(sim, argv[1]))
                printf("  Topology loaded. SPF computed for %d routers.\n",
                       sim->lsdb.count);
            else
                printf("  %% Error: could not load '%s'\n", argv[1]);
            continue;
        }

        if (strcmp(argv[0], "add") == 0) {
            if (argc >= 3 && strcmp(argv[1], "router") == 0) {
                if (ospf_add_router(sim, argv[2]))
                    printf("  Router %s added.\n", argv[2]);
                else
                    printf("  %% Error: could not add router\n");
            } else if (argc >= 5 && strcmp(argv[1], "link") == 0) {
                int cost = atoi(argv[4]);
                if (ospf_add_link(sim, argv[2], argv[3], cost)) {
                    printf("  Link %s--%s cost=%d added. Run 'recompute' or\n",
                           argv[2], argv[3], cost);
                    printf("  any fail/restore/cost command to refresh SPF.\n");
                    ospf_recompute_all(sim);
                } else {
                    printf("  %% Error: could not add link\n");
                }
            } else {
                printf("  %% Usage: add router <name> | add link <A> <B> <cost>\n");
            }
            continue;
        }

        if (strcmp(argv[0], "fail") == 0) {
            if (argc < 3) {
                printf("  %% Usage: fail <A> <B>\n");
                continue;
            }
            if (ospf_fail_link(sim, argv[1], argv[2]))
                printf("  Link %s--%s DOWN. SPF recomputed.\n", argv[1], argv[2]);
            else
                printf("  %% Error: link not found\n");
            continue;
        }

        if (strcmp(argv[0], "restore") == 0) {
            if (argc < 3) {
                printf("  %% Usage: restore <A> <B>\n");
                continue;
            }
            if (ospf_restore_link(sim, argv[1], argv[2]))
                printf("  Link %s--%s UP. SPF recomputed.\n", argv[1], argv[2]);
            else
                printf("  %% Error: link not found\n");
            continue;
        }

        if (strcmp(argv[0], "cost") == 0) {
            if (argc < 4) {
                printf("  %% Usage: cost <A> <B> <newcost>\n");
                continue;
            }
            int c = atoi(argv[3]);
            if (ospf_update_cost(sim, argv[1], argv[2], c))
                printf("  Cost %s--%s set to %d. SPF recomputed.\n",
                       argv[1], argv[2], c);
            else
                printf("  %% Error: could not update cost\n");
            continue;
        }

        if (strcmp(argv[0], "recompute") == 0) {
            ospf_recompute_all(sim);
            printf("  SPF recalculated for all routers.\n");
            continue;
        }

        if (strcmp(argv[0], "show") == 0) {
            if (argc < 2) {
                printf("  %% Usage: show topology|ip route|spf|path|events\n");
                continue;
            }
            if (strcmp(argv[1], "topology") == 0) {
                ospf_show_topology(sim);
            } else if (strcmp(argv[1], "ip") == 0 && argc >= 3 &&
                       strcmp(argv[2], "route") == 0) {
                if (argc >= 4)
                    ospf_show_route(sim, argv[3]);
                else
                    ospf_show_all_routes(sim);
            } else if (strcmp(argv[1], "route") == 0) {
                /* shorthand: show route [router] */
                if (argc >= 3)
                    ospf_show_route(sim, argv[2]);
                else
                    ospf_show_all_routes(sim);
            } else if (strcmp(argv[1], "spf") == 0) {
                if (argc < 3)
                    printf("  %% Usage: show spf <router>\n");
                else
                    ospf_show_spf(sim, argv[2]);
            } else if (strcmp(argv[1], "path") == 0) {
                if (argc < 4)
                    printf("  %% Usage: show path <src> <dst>\n");
                else
                    ospf_show_path(sim, argv[2], argv[3]);
            } else if (strcmp(argv[1], "events") == 0) {
                ospf_show_events(sim);
            } else {
                printf("  %% Unknown show target. Try 'help'.\n");
            }
            continue;
        }

        printf("  %% Unknown command '%s'. Type 'help'.\n", argv[0]);
    }
}
