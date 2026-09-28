#include "ospf.h"
#include "cli.h"

#include <stdio.h>
#include <string.h>

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage:\n"
            "  %s                  Interactive CLI\n"
            "  %s --demo           Run guided demo then exit\n"
            "  %s --load <file>    Load topology then enter CLI\n"
            "  %s --help           Show this message\n",
            prog, prog, prog, prog);
}

int main(int argc, char **argv)
{
    OspfSim sim;
    ospf_init(&sim);

    if (argc >= 2) {
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            usage(argv[0]);
            return 0;
        }
        if (strcmp(argv[1], "--demo") == 0) {
            ospf_run_demo(&sim);
            ospf_clear(&sim);
            return 0;
        }
        if (strcmp(argv[1], "--load") == 0) {
            if (argc < 3) {
                usage(argv[0]);
                return 1;
            }
            if (!ospf_load_topology(&sim, argv[2])) {
                fprintf(stderr, "Failed to load topology: %s\n", argv[2]);
                return 1;
            }
            printf("Loaded %s (%d routers). Entering interactive mode.\n",
                   argv[2], sim.lsdb.count);
            cli_run(&sim);
            ospf_clear(&sim);
            return 0;
        }
        fprintf(stderr, "Unknown option: %s\n", argv[1]);
        usage(argv[0]);
        return 1;
    }

    cli_run(&sim);
    ospf_clear(&sim);
    return 0;
}
