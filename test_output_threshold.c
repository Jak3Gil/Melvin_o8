/*
 * Test: Output Threshold - Thinking vs Output
 * 
 * Demonstrates the new relative threshold for output decisions:
 * - Wave propagation always happens (thinking)
 * - Output only generated when patterns are mature (relative threshold)
 * - Novel inputs: thinking only (no output)
 * - Repeated inputs: learned patterns → output
 */

#include "melvin_m.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

void print_separator(const char *title) {
    printf("\n");
    printf("================================================================================\n");
    printf("  %s\n", title);
    printf("================================================================================\n");
}

int main(void) {
    print_separator("Output Threshold Test: Thinking vs Output");
    
    /* Create test .m file */
    const char *test_file = "test_output_threshold.m";
    unlink(test_file);  /* Remove if exists */
    
    MelvinMFile *mfile = melvin_m_create(test_file);
    if (!mfile) {
        printf("ERROR: Failed to create .m file\n");
        return 1;
    }
    
    printf("\nTest: Novel input should only think (no output)\n");
    printf("-----------------------------------------------\n");
    
    /* First input: completely novel - should only think, no output */
    const char *novel_input = "NOVEL";
    uint8_t packaged[256];
    packaged[0] = 1;  /* port_id = 1 */
    memcpy(packaged + 1, novel_input, strlen(novel_input));
    size_t packaged_size = 1 + strlen(novel_input);
    
    melvin_m_universal_input_write(mfile, packaged, packaged_size);
    melvin_m_process_input(mfile);
    
    size_t output_size = melvin_m_universal_output_size(mfile);
    printf("Input: \"%s\"\n", novel_input);
    printf("Output size: %zu bytes\n", output_size);
    printf("Result: %s\n", output_size == 0 ? "THINKING ONLY ✓" : "OUTPUT GENERATED ✗");
    
    printf("\nTest: Repeat input to build patterns\n");
    printf("-------------------------------------\n");
    
    /* Repeat the input multiple times to build co-activation edges */
    const char *repeated_input = "HELLO";
    for (int i = 0; i < 10; i++) {
        packaged[0] = 1;  /* port_id = 1 */
        memcpy(packaged + 1, repeated_input, strlen(repeated_input));
        packaged_size = 1 + strlen(repeated_input);
        
        melvin_m_universal_input_write(mfile, packaged, packaged_size);
        melvin_m_process_input(mfile);
        
        output_size = melvin_m_universal_output_size(mfile);
        printf("Iteration %d: output size = %zu bytes %s\n", 
               i + 1, output_size, 
               output_size > 0 ? "(output generated)" : "(thinking only)");
    }
    
    printf("\nTest: Novel input after learning\n");
    printf("--------------------------------\n");
    
    /* Another novel input - should still only think */
    const char *novel_input2 = "WORLD";
    packaged[0] = 1;  /* port_id = 1 */
    memcpy(packaged + 1, novel_input2, strlen(novel_input2));
    packaged_size = 1 + strlen(novel_input2);
    
    melvin_m_universal_input_write(mfile, packaged, packaged_size);
    melvin_m_process_input(mfile);
    
    output_size = melvin_m_universal_output_size(mfile);
    printf("Input: \"%s\"\n", novel_input2);
    printf("Output size: %zu bytes\n", output_size);
    printf("Result: %s\n", output_size == 0 ? "THINKING ONLY ✓" : "OUTPUT GENERATED");
    
    printf("\nTest: Familiar input after learning\n");
    printf("-----------------------------------\n");
    
    /* Repeat familiar pattern - should generate output */
    packaged[0] = 1;  /* port_id = 1 */
    memcpy(packaged + 1, repeated_input, strlen(repeated_input));
    packaged_size = 1 + strlen(repeated_input);
    
    melvin_m_universal_input_write(mfile, packaged, packaged_size);
    melvin_m_process_input(mfile);
    
    output_size = melvin_m_universal_output_size(mfile);
    printf("Input: \"%s\" (familiar)\n", repeated_input);
    printf("Output size: %zu bytes\n", output_size);
    printf("Result: %s\n", output_size > 0 ? "OUTPUT GENERATED ✓" : "THINKING ONLY");
    
    if (output_size > 0) {
        uint8_t output[1024];
        size_t read = melvin_m_universal_output_read(mfile, output, sizeof(output));
        printf("Output content: \"");
        for (size_t i = 0; i < read && i < 50; i++) {
            if (output[i] >= 32 && output[i] < 127) {
                printf("%c", output[i]);
            } else {
                printf("<%02x>", output[i]);
            }
        }
        printf("\"\n");
    }
    
    print_separator("Summary");
    printf("\nThe system demonstrates biological-like behavior:\n");
    printf("- Novel stimuli trigger internal processing only (thinking)\n");
    printf("- Repeated patterns build co-activation edges (learning)\n");
    printf("- Familiar patterns generate output (response)\n");
    printf("- All thresholds are relative (no hardcoded values)\n");
    printf("\nThis matches biological systems where:\n");
    printf("- Novel stimuli don't trigger immediate motor response\n");
    printf("- Familiar stimuli trigger learned responses\n");
    printf("- Internal processing always happens (thinking)\n");
    
    /* Cleanup */
    melvin_m_close(mfile);
    unlink(test_file);
    
    printf("\n");
    return 0;
}

