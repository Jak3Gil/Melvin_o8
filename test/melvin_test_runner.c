/*
 * Melvin Test Runner
 * 
 * Unified interface for testing .m files with multiple inputs
 * - Reuses same .m file across multiple tests (persistent learning)
 * - Automatic output analysis
 * - No code needed - just feed inputs and analyze outputs
 * 
 * Usage:
 *   melvin_test_runner <mfile.m> [test_file.txt]
 *   melvin_test_runner <mfile.m> -i "input1" -i "input2" ...
 *   melvin_test_runner <mfile.m> -f test_cases.txt
 */

#include "melvin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Forward declarations for logging */
bool melvin_log_output(const char *mfile_name, const char *test_name,
                       const uint8_t *input, size_t input_size,
                       const uint8_t *output, size_t output_size,
                       MelvinMFile *mfile);
void melvin_analyze_logs(const char *mfile_name);

typedef struct {
    char *input;
    size_t input_size;
    char *expected_output;  /* Optional - for comparison */
    size_t expected_size;
    char *description;
} TestCase;

typedef struct {
    TestCase *cases;
    size_t count;
    size_t capacity;
} TestSuite;

void print_hex(const char *label, const uint8_t *data, size_t size, size_t max_display) {
    printf("%s (%zu bytes): ", label, size);
    size_t display = (size < max_display) ? size : max_display;
    for (size_t i = 0; i < display; i++) {
        printf("%02X ", data[i]);
    }
    if (size > max_display) printf("...");
    printf("\n");
}

void print_ascii(const char *label, const uint8_t *data, size_t size, size_t max_display) {
    printf("%s (%zu bytes): \"", label, size);
    size_t display = (size < max_display) ? size : max_display;
    for (size_t i = 0; i < display; i++) {
        if (data[i] >= 32 && data[i] < 127) {
            printf("%c", data[i]);
        } else {
            printf(".");
        }
    }
    if (size > max_display) printf("...");
    printf("\"\n");
}

void analyze_output(const uint8_t *output, size_t output_size, 
                    const uint8_t *input, size_t input_size,
                    const char *test_name) {
    printf("\n  === Analysis: %s ===\n", test_name ? test_name : "Output");
    
    if (output_size == 0) {
        printf("  No output generated\n");
        return;
    }
    
    printf("  Output size: %zu bytes\n", output_size);
    printf("  Input size: %zu bytes\n", input_size);
    
    if (input_size > 0) {
        printf("  Output/Input ratio: %.2f\n", (float)output_size / (float)input_size);
    }
    
    /* Compare with input */
    if (input && input_size > 0) {
        size_t match_count = 0;
        size_t compare_len = (output_size < input_size) ? output_size : input_size;
        
        for (size_t i = 0; i < compare_len; i++) {
            if (output[i] == input[i]) match_count++;
        }
        
        float similarity = compare_len > 0 ? (float)match_count / (float)compare_len : 0.0f;
        printf("  Input similarity: %.2f%% (%zu/%zu bytes match)\n", 
               similarity * 100.0f, match_count, compare_len);
        
        if (output_size == input_size && similarity > 0.95f) {
            printf("  → Echo/pass-through behavior\n");
        } else if (output_size > input_size) {
            printf("  → Extended output (learned continuation: +%zu bytes)\n", 
                   output_size - input_size);
        } else if (output_size < input_size) {
            printf("  → Compressed output (abstraction: -%zu bytes)\n", 
                   input_size - output_size);
        }
    }
    
    /* Byte distribution */
    int byte_counts[256] = {0};
    for (size_t i = 0; i < output_size; i++) {
        byte_counts[output[i]]++;
    }
    
    int unique_bytes = 0;
    int max_count = 0;
    uint8_t most_frequent = 0;
    for (int i = 0; i < 256; i++) {
        if (byte_counts[i] > 0) unique_bytes++;
        if (byte_counts[i] > max_count) {
            max_count = byte_counts[i];
            most_frequent = i;
        }
    }
    
    printf("  Unique byte values: %d/256\n", unique_bytes);
    if (max_count > 0) {
        printf("  Most frequent byte: 0x%02X (appears %d times)\n", most_frequent, max_count);
    }
}

void print_graph_state(MelvinMFile *mfile, const char *label) {
    if (!mfile || !mfile->graph) return;
    printf("  %s: Nodes=%zu, Edges=%zu, Adaptations=%llu\n",
           label,
           mfile->graph->node_count,
           mfile->graph->edge_count,
           (unsigned long long)melvin_m_get_adaptation_count(mfile));
}

int hex_char_to_int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

size_t hex_string_to_bytes(const char *hex_str, uint8_t *buffer, size_t buffer_size) {
    size_t len = strlen(hex_str);
    size_t bytes = 0;
    
    for (size_t i = 0; i < len && bytes < buffer_size; i += 2) {
        if (i + 1 >= len) break;
        int high = hex_char_to_int(hex_str[i]);
        int low = hex_char_to_int(hex_str[i + 1]);
        if (high < 0 || low < 0) break;
        buffer[bytes++] = (high << 4) | low;
    }
    
    return bytes;
}

size_t read_file_to_buffer(const char *filename, uint8_t **buffer) {
    FILE *f = fopen(filename, "rb");
    if (!f) return 0;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size < 0) {
        fclose(f);
        return 0;
    }
    
    *buffer = (uint8_t*)malloc(size);
    if (!*buffer) {
        fclose(f);
        return 0;
    }
    
    size_t read_size = fread(*buffer, 1, size, f);
    fclose(f);
    
    return read_size;
}

/* Parse test file format:
 *   # comment
 *   test_name: input_data
 *   test_name: hex:48656C6C6F
 *   test_name: file:input.bin
 */
TestSuite* parse_test_file(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) return NULL;
    
    TestSuite *suite = (TestSuite*)calloc(1, sizeof(TestSuite));
    if (!suite) {
        fclose(f);
        return NULL;
    }
    
    suite->capacity = 16;
    suite->cases = (TestCase*)calloc(suite->capacity, sizeof(TestCase));
    if (!suite->cases) {
        free(suite);
        fclose(f);
        return NULL;
    }
    
    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        /* Skip comments and empty lines */
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        
        /* Remove newline */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
        }
        
        if (len == 0) continue;
        
        /* Parse: name: input */
        char *colon = strchr(line, ':');
        if (!colon) continue;
        
        *colon = '\0';
        char *name = line;
        char *input_spec = colon + 1;
        
        /* Skip leading whitespace */
        while (*input_spec == ' ' || *input_spec == '\t') input_spec++;
        
        if (suite->count >= suite->capacity) {
            suite->capacity *= 2;
            suite->cases = (TestCase*)realloc(suite->cases, suite->capacity * sizeof(TestCase));
            if (!suite->cases) break;
        }
        
        TestCase *tc = &suite->cases[suite->count];
        
        /* Initialize all fields */
        tc->description = NULL;
        tc->input = NULL;
        tc->input_size = 0;
        tc->expected_output = NULL;
        tc->expected_size = 0;
        
        tc->description = strdup(name);
        if (!tc->description) continue;  /* Skip if allocation fails */
        
        /* Parse input spec */
        if (strncmp(input_spec, "hex:", 4) == 0) {
            uint8_t *buffer = (uint8_t*)malloc(4096);
            if (buffer) {
                tc->input_size = hex_string_to_bytes(input_spec + 4, buffer, 4096);
                tc->input = (char*)buffer;
            } else {
                free(tc->description);
                continue;  /* Skip if allocation fails */
            }
        } else if (strncmp(input_spec, "file:", 5) == 0) {
            uint8_t *buffer = NULL;
            tc->input_size = read_file_to_buffer(input_spec + 5, &buffer);
            tc->input = (char*)buffer;
            if (!tc->input || tc->input_size == 0) {
                free(tc->description);
                continue;  /* Skip if file read fails */
            }
        } else {
            /* Plain ASCII string */
            tc->input_size = strlen(input_spec);
            tc->input = strdup(input_spec);
            if (!tc->input) {
                free(tc->description);
                continue;  /* Skip if allocation fails */
            }
        }
        
        suite->count++;
    }
    
    fclose(f);
    return suite;
}

void free_test_suite(TestSuite *suite) {
    if (!suite) return;
    for (size_t i = 0; i < suite->count; i++) {
        if (suite->cases[i].description) free(suite->cases[i].description);
        if (suite->cases[i].input) free(suite->cases[i].input);
        if (suite->cases[i].expected_output) free(suite->cases[i].expected_output);
    }
    if (suite->cases) free(suite->cases);
    free(suite);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <mfile.m> [options]\n", argv[0]);
        fprintf(stderr, "  Options:\n");
        fprintf(stderr, "    -i <input>        - Process single input (ASCII string)\n");
        fprintf(stderr, "    -h <hex_string>   - Process single input (hex string)\n");
        fprintf(stderr, "    -f <test_file>    - Process multiple inputs from test file\n");
        fprintf(stderr, "    -a                 - Analyze current state (no new inputs)\n");
        fprintf(stderr, "    -a logs            - Analyze output logs over time\n");
        return 1;
    }
    
    const char *mfile_name = argv[1];
    
    printf("========================================\n");
    printf("Melvin Test Runner\n");
    printf("========================================\n\n");
    
    /* Open or create .m file (reused across tests) */
    MelvinMFile *mfile = melvin_m_open(mfile_name);
    bool created = false;
    
    if (!mfile) {
        printf("Creating new .m file: %s\n", mfile_name);
        mfile = melvin_bootstrap(mfile_name);
        created = true;
        if (!mfile) {
            fprintf(stderr, "Failed to create .m file\n");
            return 1;
        }
    } else {
        printf("Opened existing .m file: %s\n", mfile_name);
    }
    
    print_graph_state(mfile, "Initial state");
    printf("\n");
    
    /* Parse test inputs */
    TestSuite *suite = NULL;
    
    if (argc >= 4 && strcmp(argv[2], "-f") == 0) {
        /* Test file mode */
        suite = parse_test_file(argv[3]);
        if (!suite) {
            fprintf(stderr, "Failed to parse test file: %s\n", argv[3]);
            melvin_m_close(mfile);
            return 1;
        }
        printf("Loaded %zu test cases from %s\n\n", suite->count, argv[3]);
    } else if (argc >= 3 && strcmp(argv[2], "-a") == 0) {
        /* Analyze only mode */
        if (argc >= 4 && strcmp(argv[3], "logs") == 0) {
            /* Analyze log history */
            melvin_m_close(mfile);
            melvin_analyze_logs(mfile_name);
            return 0;
        } else {
            /* Show current state */
            printf("Analysis mode - showing current state\n\n");
            if (melvin_m_universal_output_size(mfile) > 0) {
                uint8_t *output = (uint8_t*)malloc(melvin_m_universal_output_size(mfile));
                if (output) {
                    size_t size = melvin_m_universal_output_read(mfile, output, melvin_m_universal_output_size(mfile));
                    print_hex("Current output", output, size, 64);
                    print_ascii("Current output", output, size, 64);
                    analyze_output(output, size, NULL, 0, "Current State");
                    free(output);
                }
            }
            melvin_m_close(mfile);
            return 0;
        }
    } else {
        /* Single input mode */
        suite = (TestSuite*)calloc(1, sizeof(TestSuite));
        if (!suite) {
            melvin_m_close(mfile);
            return 1;
        }
        suite->capacity = 1;
        suite->cases = (TestCase*)calloc(1, sizeof(TestCase));
        if (!suite->cases) {
            free(suite);
            melvin_m_close(mfile);
            return 1;
        }
        
        TestCase *tc = &suite->cases[0];
        
        if (argc >= 4 && strcmp(argv[2], "-i") == 0) {
            tc->description = strdup("Test 1");
            tc->input = strdup(argv[3]);
            tc->input_size = strlen(argv[3]);
        } else if (argc >= 4 && strcmp(argv[2], "-h") == 0) {
            uint8_t *buffer = (uint8_t*)malloc(4096);
            if (buffer) {
                tc->description = strdup("Test 1");
                tc->input_size = hex_string_to_bytes(argv[3], buffer, 4096);
                tc->input = (char*)buffer;
            }
        } else {
            fprintf(stderr, "Invalid arguments\n");
            free(suite);
            melvin_m_close(mfile);
            return 1;
        }
        
        suite->count = 1;
    }
    
    /* Run tests */
    for (size_t test_idx = 0; test_idx < suite->count; test_idx++) {
        TestCase *tc = &suite->cases[test_idx];
        
        printf("----------------------------------------\n");
        printf("Test %zu: %s\n", test_idx + 1, tc->description ? tc->description : "Unnamed");
        printf("----------------------------------------\n");
        
        print_graph_state(mfile, "Before");
        
        /* Write input */
        print_hex("Input", (uint8_t*)tc->input, tc->input_size, 32);
        print_ascii("Input", (uint8_t*)tc->input, tc->input_size, 32);
        
        melvin_m_universal_input_write(mfile, (uint8_t*)tc->input, tc->input_size);
        
        /* Process (auto-saves) */
        printf("\n  Processing...\n");
        bool success = melvin_m_process_input(mfile);
        if (!success) {
            fprintf(stderr, "  ERROR: Processing failed\n");
            continue;
        }
        
        /* Read output */
        size_t output_size = melvin_m_universal_output_size(mfile);
        uint8_t *output = NULL;
        
        if (output_size > 0) {
            output = (uint8_t*)malloc(output_size);
            if (output) {
                melvin_m_universal_output_read(mfile, output, output_size);
            }
        }
        
        /* Display output */
        printf("\n  Output:\n");
        if (output && output_size > 0) {
            print_hex("    ", output, output_size, 32);
            print_ascii("    ", output, output_size, 32);
        } else {
            printf("    (empty)\n");
        }
        
        /* Analyze */
        analyze_output(output, output_size, (uint8_t*)tc->input, tc->input_size, tc->description);
        
        /* Log output for time-series analysis */
        melvin_log_output(mfile_name, tc->description, 
                         (uint8_t*)tc->input, tc->input_size,
                         output, output_size, mfile);
        
        print_graph_state(mfile, "After");
        
        if (output) free(output);
        printf("\n");
    }
    
    /* Final state */
    printf("========================================\n");
    printf("Final State\n");
    printf("========================================\n");
    print_graph_state(mfile, "Final");
    printf("Total tests run: %zu\n", suite->count);
    printf("Total adaptations: %llu\n", 
           (unsigned long long)melvin_m_get_adaptation_count(mfile));
    
    /* Cleanup */
    free_test_suite(suite);
    melvin_m_close(mfile);
    
    printf("\nTest run complete!\n");
    return 0;
}

