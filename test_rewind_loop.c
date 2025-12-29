#include <stdio.h>
#include <stdbool.h>

int main() {
    FILE *f = fopen("test_dataset_1gb.txt", "rb");
    if (!f) {
        printf("Failed to open\n");
        return 1;
    }
    
    char buffer[4096];
    int chunks = 0;
    bool loop = true;
    
    printf("Testing continuous read with rewind...\n\n");
    
    while (loop && chunks < 20) {
        size_t n = fread(buffer, 1, 4096, f);
        
        if (n > 0) {
            chunks++;
            printf("Chunk %d: %zu bytes\n", chunks, n);
        } else {
            if (feof(f)) {
                printf("EOF reached at chunk %d\n", chunks);
                printf("Rewinding...\n");
                rewind(f);
                clearerr(f);
                printf("After rewind: feof() = %d\n", feof(f));
                
                // Try reading again
                n = fread(buffer, 1, 4096, f);
                if (n > 0) {
                    chunks++;
                    printf("After rewind: Chunk %d: %zu bytes\n", chunks, n);
                } else {
                    printf("After rewind: Still 0 bytes! feof() = %d\n", feof(f));
                    break;
                }
            } else {
                printf("Error or empty read\n");
                break;
            }
        }
    }
    
    fclose(f);
    printf("\nTotal chunks read: %d\n", chunks);
    return 0;
}
