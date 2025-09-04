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
            build/resample.o

# Visualization objects
VIZ_OBJS = build/img_png.o \
           build/img_ppm.o \
           build/draw_axes.o

# Demodulation objects
DEMOD_OBJS = build/fm.o \
             build/am.o \
             build/ssb.o \
             build/wave.o \
             build/agc.o

# Converter objects
CONVERTER_OBJS = build/converter.o \
                 build/wav_converter.o \
                 build/iq_converter.o \
                 build/file_utils.o

# Tool executables
TOOLS = iqinfo file_converter generate_images iqls iqcut iqdemod-fm iqdemod-am iqdemod-ssb

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

# Demodulation compilation
build/fm.o: src/demod/fm.c src/demod/fm.h
	$(CC) $(CFLAGS) -c $< -o $@

build/am.o: src/demod/am.c src/demod/am.h
	$(CC) $(CFLAGS) -c $< -o $@

build/ssb.o: src/demod/ssb.c src/demod/ssb.h
	$(CC) $(CFLAGS) -c $< -o $@

build/wave.o: src/demod/wave.c src/demod/wave.h
	$(CC) $(CFLAGS) -c $< -o $@

build/agc.o: src/demod/agc.c src/demod/agc.h
	$(CC) $(CFLAGS) -c $< -o $@

# Core library compilation (additional)
build/resample.o: src/iq_core/resample.c src/iq_core/resample.h
	$(CC) $(CFLAGS) -c $< -o $@

# Tool compilation
iqinfo: tools/iqinfo.c $(CORE_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lm


file_converter: tools/file_converter.c $(CORE_OBJS) $(CONVERTER_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lm

generate_images: tools/generate_images.c $(CORE_OBJS) $(VIZ_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lm

# iqls tool
iqls: tools/iqls.c $(CORE_OBJS) $(VIZ_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lm

# iqdemod-fm tool
iqdemod-fm: tools/iqdemod-fm.c $(CORE_OBJS) $(DEMOD_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lm

# iqdemod-am tool
iqdemod-am: tools/iqdemod-am.c $(CORE_OBJS) $(DEMOD_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lm

# iqdemod-ssb tool
iqdemod-ssb: tools/iqdemod-ssb.c $(CORE_OBJS) $(DEMOD_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lm

# Integration tests
tests/integration/test_iqls_spectrum.exe: tests/integration/test_iqls_spectrum.c iqls
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

test-iqls: tests/integration/test_iqls_spectrum.exe
	./tests/integration/test_iqls_spectrum.exe

tests/integration/test_iqls_waterfall.exe: tests/integration/test_iqls_waterfall.c iqls
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

test-iqls-wf: tests/integration/test_iqls_waterfall.exe
	./tests/integration/test_iqls_waterfall.exe

iqcut: tools/iqcut.c $(CORE_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lm

tests/integration/test_iqcut_basic.exe: tests/integration/test_iqcut_basic.c iqcut
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

test-iqcut: tests/integration/test_iqcut_basic.exe
	./tests/integration/test_iqcut_basic.exe

tests/integration/test_iqls_synthetic_tone.exe: tests/integration/test_iqls_synthetic_tone.c iqls
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

test-iqls-acceptance: tests/integration/test_iqls_synthetic_tone.exe
	./tests/integration/test_iqls_synthetic_tone.exe

tests/integration/test_iqcut_acceptance.exe: tests/integration/test_iqcut_acceptance.c $(CORE_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lm

test-iqcut-acceptance: tests/integration/test_iqcut_acceptance.exe
	./tests/integration/test_iqcut_acceptance.exe

tests/integration/test_iqdemod_fm_basic.exe: tests/integration/test_iqdemod_fm_basic.c iqdemod-fm $(CORE_OBJS)
	$(CC) $(CFLAGS) $< $(CORE_OBJS) -o $@ $(LDFLAGS) -lm

test-iqdemod-fm: tests/integration/test_iqdemod_fm_basic.exe
	./tests/integration/test_iqdemod_fm_basic.exe

tests/integration/test_iqdemod_fm_acceptance.exe: tests/integration/test_iqdemod_fm_acceptance.c $(CORE_OBJS) $(DEMOD_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lm

test-iqdemod-fm-acceptance: tests/integration/test_iqdemod_fm_acceptance.exe
	./tests/integration/test_iqdemod_fm_acceptance.exe

tests/integration/test_iqdemod_am_basic.exe: tests/integration/test_iqdemod_am_basic.c iqdemod-am $(CORE_OBJS)
	$(CC) $(CFLAGS) $< $(CORE_OBJS) -o $@ $(LDFLAGS) -lm

test-iqdemod-am: tests/integration/test_iqdemod_am_basic.exe
	./tests/integration/test_iqdemod_am_basic.exe

tests/integration/test_iqdemod_am_acceptance.exe: tests/integration/test_iqdemod_am_acceptance.c $(CORE_OBJS) $(DEMOD_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lm

test-iqdemod-am-acceptance: tests/integration/test_iqdemod_am_acceptance.exe
	./tests/integration/test_iqdemod_am_acceptance.exe

tests/integration/test_iqdemod_ssb_basic.exe: tests/integration/test_iqdemod_ssb_basic.c iqdemod-ssb $(CORE_OBJS)
	$(CC) $(CFLAGS) $< $(CORE_OBJS) -o $@ $(LDFLAGS) -lm

test-iqdemod-ssb: tests/integration/test_iqdemod_ssb_basic.exe
	./tests/integration/test_iqdemod_ssb_basic.exe

tests/integration/test_iqdemod_ssb_acceptance.exe: tests/integration/test_iqdemod_ssb_acceptance.c $(CORE_OBJS) $(DEMOD_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lm

test-iqdemod-ssb-acceptance: tests/integration/test_iqdemod_ssb_acceptance.exe
	./tests/integration/test_iqdemod_ssb_acceptance.exe

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR) $(TOOLS)

# Development helpers
.PHONY: all clean dirs test
