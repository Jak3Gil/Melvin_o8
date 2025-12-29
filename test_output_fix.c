#include "melvin_m.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int main() {
    printf("Testing output with pure sequential edges (no similarity boosts)...\n\n");
    
    /* Create brain */
    MelvinMFile *brain = melvin_m_create("test_output.m");
    assert(brain != NULL);
    
    /* Train on "hello" 10 times to build strong sequential edges */
    const char *pattern = "hello";
    for (int i = 0; i < 10; i++) {
        melvin_m_universal_input_write(brain, (const uint8_t*)pattern, strlen(pattern));
        melvin_m_process_input(brain);
        printf("Training iteration %d: input='%s'\n", i+1, pattern);
    }
    
    printf("\nGraph stats: %zu nodes, %zu edges\n", 
           brain->graph->node_count, brain->graph->edge_count);
    
    /* Now test output with partial input */
    printf("\n--- Testing Output ---\n");
    
    /* Input: "hel" (first 3 chars) */
    const char *test_input = "hel";
    melvin_m_universal_input_write(brain, (const uint8_t*)test_input, strlen(test_input));
    melvin_m_process_input(brain);
    
    /* Check output */
    size_t output_size = melvin_m_universal_output_size(brain);
    printf("Input: '%s'\n", test_input);
    printf("Output size: %zu bytes\n", output_size);
    
    if (output_size > 0) {
        uint8_t *output = (uint8_t*)malloc(output_size + 1);
        memcpy(output, brain->universal_output, output_size);
        output[output_size] = '\0';
        printf("Output: '%s'\n", (char*)output);
        
        /* Expected: "lo" (completing "hello") */
        if (strncmp((char*)output, "lo", 2) == 0) {
            printf("✓ SUCCESS: Output correctly completes the pattern!\n");
        } else {
            printf("✗ Output doesn't match expected 'lo'\n");
        }
        free(output);
    } else {
        printf("(no output - system not confident yet)\n");
    }
    
    /* Test with "wor" after training on "world" */
    printf("\n--- Training on 'world' ---\n");
    const char *pattern2 = "world";
    for (int i = 0; i < 10; i++) {
        melvin_m_universal_input_write(brain, (const uint8_t*)pattern2, strlen(pattern2));
        melvin_m_process_input(brain);
    }
    
    printf("\n--- Testing 'wor' -> should output 'ld' ---\n");
    const char *test_input2 = "wor";
    melvin_m_universal_input_write(brain, (const uint8_t*)test_input2, strlen(test_input2));
    melvin_m_process_input(brain);
    
    output_size = melvin_m_universal_output_size(brain);
    printf("Input: '%s'\n", test_input2);
    printf("Output size: %zu bytes\n", output_size);
    
    if (output_size > 0) {
        uint8_t *output = (uint8_t*)malloc(output_size + 1);
        memcpy(output, brain->universal_output, output_size);
        output[output_size] = '\0';
        printf("Output: '%s'\n", (char*)output);
        
        if (strncmp((char*)output, "ld", 2) == 0) {
            printf("✓ SUCCESS: Output correctly completes 'world'!\n");
        } else {
            printf("✗ Output doesn't match expected 'ld'\n");
        }
        free(output);
    }
    
    melvin_m_close(brain);
    printf("\nTest complete!\n");
    return 0;
}
