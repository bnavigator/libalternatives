/*  libalternatives - update-alternatives alternative
 *  Copyright (C) 2021  SUSE LLC
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#define _GNU_SOURCE
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include "../src/libalternatives.h"

const char binname[] = "alts";

extern int printInstalledBinariesAndTheirOverrideStates(const char *program);

static int setProgramOverride(const char *program, int priority, int is_system, int is_user)
{
	if (!is_user && !is_system) {
		if (geteuid() == 0)
			is_system = 1;
		else
			is_user = 1;
	}

	if (is_system && is_user) {
		fprintf(stderr, "Trying to override both system and user priorities. Decide on one.\n");
		return -1;
	}

	const char *config_fn = (is_user ? get_user_config_path() : get_system_config_path());
	struct AlternativeLink *alts = NULL;
	int group_priority = priority;
	if (group_priority == 0) {
		group_priority = read_binary_configured_priority_from_file(program, config_fn);
		if (group_priority < 0) {
			fprintf(stderr, "Failed to load current state from the config file for binary: %s\n", program);
			return -1;
		}
	}
	if (group_priority > 0 && load_exact_priority_binary_alternatives(program, group_priority, &alts) != 0) {
		fprintf(stderr, "Failed to load config file for binary: %s\n", program);
		return -1;
	}

	int ret = write_binary_configured_priority_to_file(program, priority, config_fn);
	if (ret < 0) {
		perror(config_fn);
		fprintf(stderr, "Error updating override file\n");
		return -1;
	}

	if (group_priority > 0) {
		for (const struct AlternativeLink *p = alts; p->type != ALTLINK_EOL; p++) {
			if (p->type == ALTLINK_GROUP) {
				if (write_binary_configured_priority_to_file(p->target, priority, config_fn) < 0) {
					perror(config_fn);
					fprintf(stderr, "Error updating override file for group member: %s (orig binary: %s)\n", p->target, program);
					ret = -2;
				}
			}
		}
	}

	free_alternatives_ptr(&alts);
	return ret;
}

static void printHelp()
{
	puts(
		"\n\n"
		"  libalternatives (C) 2021  SUSE LLC\n"
		"\n"
		"    alts -h         --- this help screen\n"
		"    alts -l[name]   --- list programs or just one with given name\n"
		"    alts [-u] [-s] -n <program> [-p <alt_priority>]\n"
		"       sets an override with a given priority as default\n"
		"       if priority is not set, then resets to default by removing override\n"
		"       -u -- user override, default for non-root users\n"
		"       -s -- system overrude, default for root users\n"
		"       -n -- program to override with a given priority alternative\n"
		"\n\n"
	);
}

static void setFirstCommandOrError(int *command, int new_command)
{
	if (*command == 0)
		*command = new_command;
	else
		*command = -1;
}

static int processOptions(int argc, char *argv[])
{
	int opt;

	int command = 0;
	const char *program = NULL;
	int priority = 0;
	int is_system = 0, is_user = 0;

	optind = 1; // reset since we call this multiple times in unit tests
	while ((opt = getopt(argc, argv, ":hn:p:l::us")) != -1) {
		switch(opt) {
			case 'h':
			case 'r':
				setFirstCommandOrError(&command, opt);
				break;
			case 'u':
				is_user = 1;
				break;
			case 's':
				is_system = 1;
				break;
			case 'l':
			case 'n':
				setFirstCommandOrError(&command, opt);
				program = optarg;
				if (!optarg && optind < argc && argv[optind] != NULL && argv[optind][0] != '-') {
					program = argv[optind++];
				}
				break;
			case 'p': {
				char *ptr;
				errno = 0;
				priority = strtol(optarg, &ptr, 10);
				if (errno != 0 || *ptr != '\x00') {
					command = -1;
				}
				break;
			}
			default:
				fprintf(stderr, "unexpected return from getopt: %d\n", opt);
				setFirstCommandOrError(&command, opt);
		}
	}

	switch (command) {
		case 'h':
			printHelp();
			break;
		case '?':
		case -1:
		case 0:
			printHelp();
			return -1;
		case 'l':
			return printInstalledBinariesAndTheirOverrideStates(program);
		case 'n':
			return setProgramOverride(program, priority, is_system, is_user);
		default:
			printf("unimplemented command %c %d\n", command, (int)command);
			return 10;
	}

	return 0;
}

#ifdef UNITTESTS
int alternative_app_main(int argc, char *argv[])
{
#else
int main(int argc, char *argv[])
{
	if (strcmp(binname, basename(argv[0])) != 0)
		return exec_default(argv);
#endif
	return processOptions(argc, argv);
}
