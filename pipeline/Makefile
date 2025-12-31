# Makefile for Melvin System with optional GPU acceleration
# Hardware-aware: Auto-detects and uses GPU when available

CC = gcc
NVCC = nvcc
# CPU OPTIMIZATION: -O3 enables auto-vectorization, -march=native uses CPU-specific instructions
CFLAGS = -Wall -Wextra -O3 -march=native -mtune=native -std=c11
CFLAGS += -ffast-math -funroll-loops -ftree-vectorize -flto -pthread
NVCCFLAGS = -arch=sm_75 -O3 -Xcompiler -Wall -Xcompiler -Wextra -Xcompiler -march=native
LDFLAGS = -lcurl -pthread
CUDA_LDFLAGS = -lcudart -lcurl

# Source files
CORE_SOURCES = melvin.c melvin_m.c melvin_gpu.c melvin_ports.c melvin_threads.c
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
	rm -f dataset_port http_range performance show_brain analyze_mfile

# Production Applications

# Production pipeline (continuous processing with mic/camera/speaker)
pipeline: melvin_lib example_ports.c
ifeq ($(shell uname),Darwin)
	$(CC) $(CFLAGS) -o pipeline example_ports.c -L. -lmelvin -lm $(FRAMEWORKS) -I.
else
	$(CC) $(CFLAGS) -o pipeline example_ports.c -L. -lmelvin -lm -I.
endif

# Dataset port (production dataset processor)
dataset_port: melvin_lib test_dataset_port.c
ifeq ($(shell uname),Darwin)
	$(CC) $(CFLAGS) -o dataset_port test_dataset_port.c -L. -lmelvin -lm $(FRAMEWORKS) -I.
else
	$(CC) $(CFLAGS) -o dataset_port test_dataset_port.c -L. -lmelvin -lm -I.
endif

# HTTP range port (production HTTP data source)
http_range: melvin_lib test_http_range.c
ifeq ($(shell uname),Darwin)
	$(CC) $(CFLAGS) -o http_range test_http_range.c -L. -lmelvin -lm -lcurl $(FRAMEWORKS) -I.
else
	$(CC) $(CFLAGS) -o http_range test_http_range.c -L. -lmelvin -lm -lcurl -I.
endif

# Performance monitor (measures operations per byte and throughput)
performance: melvin_lib test_performance.c
ifeq ($(shell uname),Darwin)
	$(CC) $(CFLAGS) -o performance test_performance.c -L. -lmelvin -lm $(FRAMEWORKS) -I.
else
	$(CC) $(CFLAGS) -o performance test_performance.c -L. -lmelvin -lm -I.
endif

# .m file analysis tool
analyze_mfile: melvin_lib analyze_mfile.c
ifeq ($(shell uname),Darwin)
	$(CC) $(CFLAGS) -o analyze_mfile analyze_mfile.c -L. -lmelvin -lm $(FRAMEWORKS) -I.
else
	$(CC) $(CFLAGS) -o analyze_mfile analyze_mfile.c -L. -lmelvin -lm -I.
endif

# Show brain stats utility
show_brain: melvin_lib show_brain.c
ifeq ($(shell uname),Darwin)
	$(CC) $(CFLAGS) -o show_brain show_brain.c -L. -lmelvin -lm $(FRAMEWORKS) -I.
else
	$(CC) $(CFLAGS) -o show_brain show_brain.c -L. -lmelvin -lm -I.
endif


# Build all production applications
all-apps: pipeline dataset_port http_range performance show_brain analyze_mfile
	@echo "All production applications built"

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: NVCCFLAGS += -g -G
debug: melvin_lib

# Release build
release: CFLAGS += -O3 -DNDEBUG
release: NVCCFLAGS += -O3 -DNDEBUG
release: melvin_lib

