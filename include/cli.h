#ifndef CLI_H
#define CLI_H

#include "ospf.h"

void cli_print_banner(void);
void cli_print_help(void);
void cli_run(OspfSim *sim);

#endif /* CLI_H */
