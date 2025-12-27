#include "melvin_m.h"
#include <stdio.h>
int main() {
    MelvinMFile *m = melvin_m_open("example.m");
    if (!m) {
        printf("Failed to open\n");
        return 1;
    }
    printf("Opened successfully\n");
    melvin_m_close(m);
    return 0;
}
