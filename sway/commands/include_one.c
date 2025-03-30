#include <stdlib.h>
#include <string.h>
#include <wordexp.h>
#include <libgen.h> // For basename()
#include "sway/commands.h"
#include "sway/config.h"
#include "util.h"
#include "list.h"
#include "log.h"

// List to track already included files
static list_t *included_files = NULL;

// Function to free the included_files list (to be called on cleanup)
void free_included_files_list() {
    if (included_files) {
        for (int i = 0; i < included_files->length; i++) {
            free(included_files->items[i]);  // Free each filename string
        }
        list_free(included_files);  // Free the list itself
        included_files = NULL;
    }
}

// Comparator function for sorting file names
static int compare_strings(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

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
        free(files);
        return cmd_results_new(CMD_FAILURE, "include_one: Failed to expand file patterns");
    }

    if (p.we_wordc == 0) {
        wordfree(&p);
        free(files);
        return cmd_results_new(CMD_FAILURE, "include_one: No matching files found");
    }

    // Sort files for consistency
    qsort(p.we_wordv, p.we_wordc, sizeof(char *), compare_strings);

    sway_log(SWAY_DEBUG, "Processing include_one files:");

    for (size_t i = 0; i < p.we_wordc; i++) {
        char *file = p.we_wordv[i];

        // Use strdup to avoid modifying original strings
        char *file_name_dup = strdup(file);
        if (!file_name_dup) {
            wordfree(&p);
            free(files);
            return cmd_results_new(CMD_FAILURE, "include_one: Memory allocation failed");
        }

        char *file_name = basename(file_name_dup); // Extract just the filename

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
            free(file_name_dup); // Free memory for skipped files
            continue;
        }

        // Mark file as included
        list_add(included_files, strdup(file_name));

        // Load the configuration file
        load_include_configs(file, config, &config->swaynag_config_errors);
        sway_log(SWAY_DEBUG, "Included: %s", file);
       
        free(file_name_dup); // Free memory after processing
    }

    sway_log(SWAY_DEBUG, "Include_one process completed");

    wordfree(&p);
    free(files);
    return cmd_results_new(CMD_SUCCESS, NULL);
}