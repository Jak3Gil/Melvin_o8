# Makefile for Melvin System with optional GPU acceleration
# Hardware-aware: Auto-detects and uses GPU when available

CC = gcc
NVCC = nvcc
CFLAGS = -Wall -Wextra -O2 -std=c11
NVCCFLAGS = -arch=sm_75 -O2 -Xcompiler -Wall -Xcompiler -Wextra
LDFLAGS = 
CUDA_LDFLAGS = -lcudart

# Source files
CORE_SOURCES = melvin.c melvin_m.c melvin_gpu.c
CUDA_SOURCES = melvin_gpu_cuda.cu
OBJECTS = $(CORE_SOURCES:.c=.o)
CUDA_OBJECTS = $(CUDA_SOURCES:.cu=.o)

# Detect if CUDA is available
CUDA_AVAILABLE := $(shell command -v $(NVCC) >/dev/null 2>&1 && echo yes || echo no)

.PHONY: all clean cpu-only gpu

all: melvin_lib

# Build with GPU support if CUDA is available
ifeq ($(CUDA_AVAILABLE),yes)
melvin_lib: $(OBJECTS) $(CUDA_OBJECTS)
	@echo "Building with GPU acceleration (CUDA detected)"
	$(NVCC) -shared -o libmelvin.so $(OBJECTS) $(CUDA_OBJECTS) $(LDFLAGS) $(CUDA_LDFLAGS)
	@echo "Build complete with GPU acceleration"
else
melvin_lib: $(OBJECTS)
	@echo "Building CPU-only (CUDA not detected)"
	$(CC) -shared -o libmelvin.so $(OBJECTS) $(LDFLAGS)
	@echo "Build complete (CPU-only, GPU fallback enabled)"
endif

# CPU-only build (explicit)
cpu-only: CUDA_AVAILABLE = no
cpu-only: melvin_lib

# GPU build (explicit, fails if CUDA not available)
gpu:
	@if [ "$(CUDA_AVAILABLE)" != "yes" ]; then \
		echo "Error: CUDA compiler (nvcc) not found. Install CUDA toolkit."; \
		exit 1; \
	fi
	$(MAKE) CUDA_AVAILABLE=yes melvin_lib

# Compile C sources
%.o: %.c melvin.h melvin_gpu.h
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

# Compile CUDA sources (only if CUDA available)
ifeq ($(CUDA_AVAILABLE),yes)
%.o: %.cu melvin.h melvin_gpu.h
	$(NVCC) $(NVCCFLAGS) -c $< -o $@
endif

clean:
	rm -f $(OBJECTS) $(CUDA_OBJECTS) libmelvin.so *.a
	rm -f test/*.o test/*.m test/*.log

# Test target
test: melvin_lib
	cd test && $(MAKE)

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: NVCCFLAGS += -g -G
debug: melvin_lib

# Release build
release: CFLAGS += -O3 -DNDEBUG
release: NVCCFLAGS += -O3 -DNDEBUG
release: melvin_lib

