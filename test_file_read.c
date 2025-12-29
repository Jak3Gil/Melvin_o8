#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    FILE *f = fopen("test_dataset_1gb.txt", "rb");
    if (!f) {
        printf("Failed to open file\n");
        return 1;
    }
    
    char buffer[4096];
    size_t total = 0;
    int chunks = 0;
    
    printf("Testing file read with rewind...\n\n");
    
    // Read first few chunks
    for (int i = 0; i < 5; i++) {
        size_t n = fread(buffer, 1, 4096, f);
        if (n > 0) {
            total += n;
            chunks++;
            printf("Chunk %d: read %zu bytes (total: %zu)\n", i+1, n, total);
        } else {
            printf("Chunk %d: EOF reached\n", i+1);
            break;
        }
    }
    
    printf("\nAt EOF: feof() = %d\n", feof(f));
    
    // Rewind and try again
    printf("\nRewinding...\n");
    rewind(f);
    clearerr(f);
    printf("After rewind: feof() = %d\n", feof(f));
    
    // Try reading again
    size_t n = fread(buffer, 1, 4096, f);
    printf("After rewind read: %zu bytes, feof() = %d\n", n, feof(f));
    
    if (n > 0) {
        printf("SUCCESS: Can read after rewind!\n");
    } else {
        printf("FAILED: Cannot read after rewind\n");
    }
    
    fclose(f);
    return 0;
}
