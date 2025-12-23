/*
 * Example: Using .m file format
 * 
 * Demonstrates creating, using, and adapting .m files
 */

#include "melvin_m.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    printf("Melvin .m File Format Example\n");
    printf("==============================\n\n");
    
    /* Create a new .m file */
    MelvinMFile *mfile = melvin_m_create("example.m");
    if (!mfile) {
        fprintf(stderr, "Failed to create .m file\n");
        return 1;
    }
    
    printf("Created new .m file: example.m\n");
    
    /* Add some nodes using melvin.c rules */
    uint8_t payload1[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F};  /* "Hello" */
    Node *node1 = melvin_m_add_node(mfile, payload1, 5);
    
    uint8_t payload2[] = {0x57, 0x6F, 0x72, 0x6C, 0x64};  /* "World" */
    Node *node2 = melvin_m_add_node(mfile, payload2, 5);
    
    printf("Added %zu nodes to graph\n", mfile->graph->node_count);
    
    /* Add an edge */
    Edge *edge1 = melvin_m_add_edge(mfile, node1, node2, true);
    if (edge1) {
        printf("Added edge: %s -> %s\n", edge1->from_id, edge1->to_id);
    }
    
    /* Add ports (external interfaces) */
    melvin_m_add_port(mfile, "input1", node1->id, 1, true, 256);
    melvin_m_add_port(mfile, "output1", node2->id, 1, false, 256);
    
    printf("Added 2 ports (input1, output1)\n");
    
    /* Write to universal input */
    uint8_t input_data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    melvin_m_universal_input_write(mfile, input_data, sizeof(input_data));
    printf("Wrote %zu bytes to universal input\n", sizeof(input_data));
    
    /* Write to specific port */
    uint8_t port_data[] = {0xAA, 0xBB, 0xCC};
    melvin_m_port_write(mfile, "input1", port_data, sizeof(port_data));
    printf("Wrote %zu bytes to port 'input1'\n", sizeof(port_data));
    
    /* Save the .m file (adaptive write) */
    if (melvin_m_save(mfile)) {
        printf("Saved .m file (adaptation count: %llu)\n", 
               (unsigned long long)melvin_m_get_adaptation_count(mfile));
    } else {
        fprintf(stderr, "Failed to save .m file\n");
    }
    
    /* Close file */
    melvin_m_close(mfile);
    
    printf("\nReopening .m file...\n");
    
    /* Reopen the file */
    mfile = melvin_m_open("example.m");
    if (!mfile) {
        fprintf(stderr, "Failed to open .m file\n");
        return 1;
    }
    
    printf("Opened .m file\n");
    printf("  Nodes: %llu\n", (unsigned long long)mfile->header.node_count);
    printf("  Edges: %llu\n", (unsigned long long)mfile->header.edge_count);
    printf("  Ports: %llu\n", (unsigned long long)mfile->header.port_count);
    printf("  Universal input size: %llu bytes\n", 
           (unsigned long long)mfile->header.universal_input_size);
    
    /* Read from universal input */
    uint8_t read_buffer[256];
    size_t read_size = melvin_m_universal_input_read(mfile, read_buffer, sizeof(read_buffer));
    printf("Read %zu bytes from universal input: ", read_size);
    for (size_t i = 0; i < read_size && i < 10; i++) {
        printf("%02X ", read_buffer[i]);
    }
    printf("\n");
    
    /* List ports */
    size_t port_count;
    MelvinPort *ports = melvin_m_get_ports(mfile, &port_count);
    printf("Ports (%zu):\n", port_count);
    for (size_t i = 0; i < port_count; i++) {
        if (ports[i].port_id[0] != '\0') {
            printf("  %s -> node %s (%s, type=%u)\n",
                   ports[i].port_id,
                   ports[i].node_id,
                   ports[i].is_input ? "input" : "output",
                   ports[i].port_type);
        }
    }
    
    /* Process input (adaptive operation) */
    melvin_m_process_input(mfile);
    printf("Processed universal input through graph\n");
    
    /* Save again (file adapts) */
    melvin_m_save(mfile);
    printf("Saved again (adaptation count: %llu)\n",
           (unsigned long long)melvin_m_get_adaptation_count(mfile));
    
    melvin_m_close(mfile);
    
    printf("\nExample complete!\n");
    return 0;
}

