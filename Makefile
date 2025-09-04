# IQ Lab Build System
# C11 with strict warnings and error handling

CC=gcc
CFLAGS=-std=c11 -Wall -Wextra -Werror -O2 -g
LDFLAGS=

# Source directories
SRC_DIRS = src/iq_core src/viz src/demod src/detect src/chan src/jobs
BUILD_DIR = build

# Core library objects (IQ-only)
CORE_OBJS = build/io_iq.o \
            build/io_sigmf.o \
            build/fft.o \
            build/window.o

# Visualization objects
VIZ_OBJS = build/img_png.o \
           build/img_ppm.o \
           build/draw_axes.o

# Converter objects
CONVERTER_OBJS = build/converter.o \
                 build/wav_converter.o \
                 build/iq_converter.o \
                 build/file_utils.o

# Tool executables
TOOLS = iqinfo file_converter generate_images

# Default target
all: dirs $(TOOLS)

# Create build directories
dirs:
	@mkdir -p $(BUILD_DIR)

# Core library compilation
build/io_iq.o: src/iq_core/io_iq.c src/iq_core/io_iq.h
	$(CC) $(CFLAGS) -c $< -o $@

build/io_sigmf.o: src/iq_core/io_sigmf.c src/iq_core/io_sigmf.h
	$(CC) $(CFLAGS) -c $< -o $@

build/fft.o: src/iq_core/fft.c src/iq_core/fft.h
	$(CC) $(CFLAGS) -c $< -o $@

build/window.o: src/iq_core/window.c src/iq_core/window.h
	$(CC) $(CFLAGS) -c $< -o $@

# Visualization compilation
build/img_png.o: src/viz/img_png.c src/viz/img_png.h src/viz/stb_image_write.h
	$(CC) $(CFLAGS) -DSTB_IMAGE_WRITE_IMPLEMENTATION -c $< -o $@

build/img_ppm.o: src/viz/img_ppm.c src/viz/img_ppm.h
	$(CC) $(CFLAGS) -c $< -o $@

build/draw_axes.o: src/viz/draw_axes.c src/viz/draw_axes.h
	$(CC) $(CFLAGS) -c $< -o $@


# Converter compilation
build/converter.o: src/converter/converter.c src/converter/converter.h
	$(CC) $(CFLAGS) -c $< -o $@

build/wav_converter.o: src/converter/formats/wav_converter.c src/converter/formats/wav_converter.h
	$(CC) $(CFLAGS) -c $< -o $@

build/iq_converter.o: src/converter/formats/iq_converter.c src/converter/formats/iq_converter.h
	$(CC) $(CFLAGS) -c $< -o $@

build/file_utils.o: src/converter/utils/file_utils.c src/converter/utils/file_utils.h
	$(CC) $(CFLAGS) -c $< -o $@

# Tool compilation
iqinfo: tools/iqinfo.c $(CORE_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lm


file_converter: tools/file_converter.c $(CORE_OBJS) $(CONVERTER_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lm

generate_images: tools/generate_images.c $(CORE_OBJS) $(VIZ_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lm

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR) $(TOOLS)

# Development helpers
.PHONY: all clean dirs test
