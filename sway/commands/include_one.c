#include <stdlib.h>
#include <string.h>
#include <wordexp.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "util.h"
#include "list.h"
#include "log.h"
#include <libgen.h> // For basename()

// List to track already included files
static list_t *included_files = NULL;

struct cmd_results *cmd_include_one(int argc, char **argv) {
    struct cmd_results *error = NULL;
    if ((error = checkarg(argc, "include_one", EXPECTED_AT_LEAST, 1))) {
        return error;
    }

    // Initialize the list if not already done
    if (!included_files) {
        included_files = create_list();
    }

    char *files = join_args(argv, argc);

    // Use wordexp to expand wildcards
    wordexp_t p;
    if (wordexp(files, &p, 0) != 0) {
        return cmd_results_new(CMD_FAILURE, "include_one: Failed to expand file patterns");
    }

    if (p.we_wordc == 0) {
        wordfree(&p);
        return cmd_results_new(CMD_FAILURE, "include_one: No matching files found");
    }

    sway_log(SWAY_DEBUG, "Files to include:");
    for (size_t i = 0; i < p.we_wordc; i++) {
        char *file_name = basename(p.we_wordv[i]); // Extract just the filename
        sway_log(SWAY_DEBUG, " - %s", file_name);
    }

    for (size_t i = 0; i < p.we_wordc; i++) {
        char *file = p.we_wordv[i];
        char *file_name = basename(file); // Extract just the filename

        // Check if the file was already included
        bool already_included = false;
        for (int j = 0; j < included_files->length; j++) {
            if (strcmp(included_files->items[j], file_name) == 0) {
                already_included = true;
                break;
            }
        }

        if (already_included) {
            sway_log(SWAY_DEBUG, "Skipping duplicate include: %s", file_name);
            continue;
        }

        // Mark file as included
        list_add(included_files, strdup(file_name));

        // Load the configuration file
        load_include_configs(file, config, &config->swaynag_config_errors);
    }

    sway_log(SWAY_DEBUG, "Include process completed");

    wordfree(&p);
    return cmd_results_new(CMD_SUCCESS, NULL);
}
