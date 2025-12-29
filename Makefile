# Makefile for Melvin System with optional GPU acceleration
# Hardware-aware: Auto-detects and uses GPU when available

CC = gcc
NVCC = nvcc
CFLAGS = -Wall -Wextra -O2 -std=c11
NVCCFLAGS = -arch=sm_75 -O2 -Xcompiler -Wall -Xcompiler -Wextra
LDFLAGS = -lcurl
CUDA_LDFLAGS = -lcudart -lcurl

# Source files
CORE_SOURCES = melvin.c melvin_m.c melvin_gpu.c melvin_ports.c
MAC_PORT_SOURCES = melvin_port_mac_audio.c melvin_port_usb_can.c melvin_port_file.c melvin_port_http.c
MAC_CAMERA_SOURCE = melvin_port_mac_camera.mm
CUDA_SOURCES = melvin_gpu_cuda.cu
OBJECTS = $(CORE_SOURCES:.c=.o) $(MAC_PORT_SOURCES:.c=.o)
CUDA_OBJECTS = $(CUDA_SOURCES:.cu=.o)

# Mac-specific: Objective-C++ camera source
ifeq ($(shell uname),Darwin)
OBJECTS += melvin_port_mac_camera.o
endif

# Detect if CUDA is available
CUDA_AVAILABLE := $(shell command -v $(NVCC) >/dev/null 2>&1 && echo yes || echo no)

.PHONY: all clean cpu-only gpu

all: melvin_lib

# Build with GPU support if CUDA is available
ifeq ($(CUDA_AVAILABLE),yes)
melvin_lib: CFLAGS += -DCUDA_AVAILABLE
melvin_lib: $(OBJECTS) $(CUDA_OBJECTS)
	@echo "Building with GPU acceleration (CUDA detected)"
ifeq ($(shell uname),Darwin)
	$(NVCC) -shared -o libmelvin.dylib $(OBJECTS) $(CUDA_OBJECTS) $(LDFLAGS) $(CUDA_LDFLAGS) $(FRAMEWORKS)
else
	$(NVCC) -shared -o libmelvin.so $(OBJECTS) $(CUDA_OBJECTS) $(LDFLAGS) $(CUDA_LDFLAGS)
endif
	@echo "Build complete with GPU acceleration"
else
melvin_lib: $(OBJECTS)
	@echo "Building CPU-only (CUDA not detected)"
ifeq ($(shell uname),Darwin)
	$(CC) -shared -o libmelvin.dylib $(OBJECTS) $(LDFLAGS) $(FRAMEWORKS)
else
	$(CC) -shared -o libmelvin.so $(OBJECTS) $(LDFLAGS)
endif
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
%.o: %.c melvin.h melvin_gpu.h melvin_ports.h
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

# Mac-specific: Compile audio port with framework flags
ifeq ($(shell uname),Darwin)
CC_OBJC = clang++
CFLAGS_OBJC = -Wall -Wextra -O2 -std=c++11 -fPIC -fobjc-arc
FRAMEWORKS = -framework AVFoundation -framework CoreVideo -framework CoreMedia -framework Foundation -framework AudioToolbox -framework CoreAudio
else
FRAMEWORKS =
endif

ifeq ($(shell uname),Darwin)
melvin_port_mac_audio.o: melvin_port_mac_audio.c melvin_ports.h
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

# Compile Objective-C++ camera implementation (disable ARC for manual memory management)
melvin_port_mac_camera.o: melvin_port_mac_camera.mm melvin_ports.h
	$(CC_OBJC) -Wall -Wextra -O2 -std=c++11 -fPIC -fno-objc-arc -c $< -o $@
endif

# Compile CUDA sources (only if CUDA available)
ifeq ($(CUDA_AVAILABLE),yes)
%.o: %.cu melvin.h melvin_gpu.h
	$(NVCC) $(NVCCFLAGS) -c $< -o $@
endif

clean:
	rm -f $(OBJECTS) $(CUDA_OBJECTS) libmelvin.so libmelvin.dylib *.a
	rm -f melvin_port_mac_camera.o
	rm -f test/*.o test/*.m test/*.log test/example_ports test/test_ports test/test_ports_simple test/test_dataset_port test/test_http_range test/test_production pipeline

# Example port program (legacy test)
example_ports: melvin_lib test/example_ports.c
ifeq ($(shell uname),Darwin)
	$(CC) $(CFLAGS) -o test/example_ports test/example_ports.c -L. -lmelvin -lm $(FRAMEWORKS) -I.
else
	$(CC) $(CFLAGS) -o test/example_ports test/example_ports.c -L. -lmelvin -lm -I.
endif

# Production pipeline
pipeline: melvin_lib test/example_ports.c
ifeq ($(shell uname),Darwin)
	$(CC) $(CFLAGS) -o pipeline test/example_ports.c -L. -lmelvin -lm $(FRAMEWORKS) -I.
else
	$(CC) $(CFLAGS) -o pipeline test/example_ports.c -L. -lmelvin -lm -I.
endif

# Test port program
test_ports: melvin_lib test/test_ports.c
ifeq ($(shell uname),Darwin)
	$(CC) $(CFLAGS) -o test/test_ports test/test_ports.c -L. -lmelvin -lm $(FRAMEWORKS) -I.
else
	$(CC) $(CFLAGS) -o test/test_ports test/test_ports.c -L. -lmelvin -lm -I.
endif

# Dataset port test program (production-ready)
test_dataset_port: melvin_lib test/test_dataset_port.c
ifeq ($(shell uname),Darwin)
	$(CC) $(CFLAGS) -o test/test_dataset_port test/test_dataset_port.c -L. -lmelvin -lm $(FRAMEWORKS) -I.
else
	$(CC) $(CFLAGS) -o test/test_dataset_port test/test_dataset_port.c -L. -lmelvin -lm -I.
endif

# HTTP range port test program
test_http_range: melvin_lib test/test_http_range.c
ifeq ($(shell uname),Darwin)
	$(CC) $(CFLAGS) -o test/test_http_range test/test_http_range.c -L. -lmelvin -lm -lcurl $(FRAMEWORKS) -I.
else
	$(CC) $(CFLAGS) -o test/test_http_range test/test_http_range.c -L. -lmelvin -lm -lcurl -I.
endif

# Live learning test (HTTP range requests with live stats)
test_live_learning: melvin_lib test_live_learning.c
ifeq ($(shell uname),Darwin)
	$(CC) $(CFLAGS) -o test_live_learning test_live_learning.c -L. -lmelvin -lm -lcurl $(FRAMEWORKS) -I.
else
	$(CC) $(CFLAGS) -o test_live_learning test_live_learning.c -L. -lmelvin -lm -lcurl -I.
endif

# Production test suite
test_production: melvin_lib test/test_production_suite.c
ifeq ($(shell uname),Darwin)
	$(CC) $(CFLAGS) -o test/test_production test/test_production_suite.c -L. -lmelvin -lm $(FRAMEWORKS) -I.
else
	$(CC) $(CFLAGS) -o test/test_production test/test_production_suite.c -L. -lmelvin -lm -I.
endif


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

