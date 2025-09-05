# IQ Lab Build System
# C11 with strict warnings and error handling

CC=gcc
CFLAGS=-std=c11 -Wall -Wextra -Werror -O2 -g
LDFLAGS=

# Source directories
SRC_DIRS = src/iq_core src/viz src/demod src/detect src/chan src/jobs src/ui
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

# Detection objects
DETECT_OBJS = build/cfar_os.o \
              build/cluster.o \
              build/features.o

# Channelization objects
CHAN_OBJS = build/pfb.o \
            build/scheduler.o

# Job orchestration objects
JOB_OBJS = build/yaml_parse.o \
           build/pipeline.o

# UI objects
UI_OBJS = build/ui.o

# Converter objects
CONVERTER_OBJS = build/converter.o \
                 build/wav_converter.o \
                 build/iq_converter.o \
                 build/file_utils.o

# Tool executables
TOOLS = iqinfo file_converter generate_images iqls iqcut iqdemod-fm iqdemod-am iqdemod-ssb iqdetect iqchan iqjob iq_ui

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

# Detection compilation
build/cfar_os.o: src/detect/cfar_os.c src/detect/cfar_os.h
	$(CC) $(CFLAGS) -c $< -o $@

build/cluster.o: src/detect/cluster.c src/detect/cluster.h
	$(CC) $(CFLAGS) -c $< -o $@

build/features.o: src/detect/features.c src/detect/features.h
	$(CC) $(CFLAGS) -c $< -o $@

# Channelization compilation
build/pfb.o: src/chan/pfb.c src/chan/pfb.h
	$(CC) $(CFLAGS) -c $< -o $@ -lm

build/scheduler.o: src/chan/scheduler.c src/chan/scheduler.h
	$(CC) $(CFLAGS) -c $< -o $@

# Job orchestration compilation
build/yaml_parse.o: src/jobs/yaml_parse.c src/jobs/yaml_parse.h
	$(CC) $(CFLAGS) -c $< -o $@

build/pipeline.o: src/jobs/pipeline.c src/jobs/pipeline.h
	$(CC) $(CFLAGS) -c $< -o $@

# UI compilation
build/ui.o: src/ui/ui.c src/ui/ui.h
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

# iqdetect object compilation
build/iqdetect.o: tools/iqdetect.c
	$(CC) $(CFLAGS) -c $< -o $@

# iqdetect tool
iqdetect: tools/iqdetect.c $(CORE_OBJS) $(DETECT_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lm

# iqchan tool
iqchan: tools/iqchan.c $(CORE_OBJS) $(CHAN_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lm

# iqjob tool
iqjob: tools/iqjob.c $(CORE_OBJS) $(JOB_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lm

# iq_ui tool (Windows only)
iq_ui: tools/iq_ui.c $(UI_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lgdi32 -luser32 -lkernel32

# Integration tests
tests/integration/test_iqdetect_basic.exe: tests/integration/test_iqdetect_basic.c build/io_iq.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lm

tests/integration/test_iqdetect_comprehensive.exe: tests/integration/test_iqdetect_comprehensive.c build/io_iq.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lm

tests/integration/test_iqdetect_accuracy.exe: tests/integration/test_iqdetect_accuracy.c build/io_iq.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lm

tests/integration/test_iqdetect_debug.exe: tests/integration/test_iqdetect_debug.c build/io_iq.o build/fft.o build/cfar_os.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lm

tests/integration/test_iqdetect_acceptance.exe: tests/integration/test_iqdetect_acceptance.c build/io_iq.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lm

tests/integration/test_iqchan_basic.exe: tests/integration/test_iqchan_basic.c build/io_iq.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lm

tests/integration/test_iqjob_basic.exe: tests/integration/test_iqjob_basic.c build/io_iq.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lm

tests/integration/test_iqls_spectrum.exe: tests/integration/test_iqls_spectrum.c iqls
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

test-iqdetect: tests/integration/test_iqdetect_basic.exe
	./tests/integration/test_iqdetect_basic.exe

test-iqdetect-comprehensive: tests/integration/test_iqdetect_comprehensive.exe
	./tests/integration/test_iqdetect_comprehensive.exe

test-iqdetect-accuracy: tests/integration/test_iqdetect_accuracy.exe
	./tests/integration/test_iqdetect_accuracy.exe

test-iqdetect-debug: tests/integration/test_iqdetect_debug.exe
	./tests/integration/test_iqdetect_debug.exe

test-iqdetect-acceptance: tests/integration/test_iqdetect_acceptance.exe
	./tests/integration/test_iqdetect_acceptance.exe

test-iqchan: tests/integration/test_iqchan_basic.exe
	./tests/integration/test_iqchan_basic.exe

test-iqjob: tests/integration/test_iqjob_basic.exe
	./tests/integration/test_iqjob_basic.exe

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

# Unit test compilation
tests/unit/test_cfar_os.exe: tests/unit/test_cfar_os.c $(DETECT_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lm

test-cfar-os: tests/unit/test_cfar_os.exe
	./tests/unit/test_cfar_os.exe

tests/unit/test_cluster.exe: tests/unit/test_cluster.c $(DETECT_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lm

test-cluster: tests/unit/test_cluster.exe
	./tests/unit/test_cluster.exe

tests/unit/test_features.exe: tests/unit/test_features.c $(DETECT_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lm

test-features: tests/unit/test_features.exe
	./tests/unit/test_features.exe

tests/unit/test_yaml_parse.exe: tests/unit/test_yaml_parse.c build/yaml_parse.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

test-yaml-parse: tests/unit/test_yaml_parse.exe
	./tests/unit/test_yaml_parse.exe

# Clean build artifacts
# Test runner targets
test: test-comprehensive
	@echo "✅ All tests completed successfully!"

test-comprehensive:
	@echo "🧪 Running comprehensive test suite..."
	@if command -v bash >/dev/null 2>&1 && [ -f tests/test_runner.sh ]; then \
		bash tests/test_runner.sh; \
	else \
		echo "🪟 Running Windows test suite..."; \
		cmd /c "tests\\test_runner.bat"; \
	fi

test-quick: all
	@echo "⚡ Running quick test suite..."
	@make test-iqchan
	@make test-iqdetect-acceptance
	@make test-iqdemod-fm-acceptance
	@echo "✅ Quick tests completed!"

test-unit: test-yaml-parse test-cfar-os test-features
	@echo "✅ Unit tests completed!"

test-integration: test-iqchan test-iqjob test-iqdetect-acceptance
	@echo "✅ Integration tests completed!"

test-acceptance: test-iqdetect-acceptance test-iqdemod-fm-acceptance test-iqdemod-am-acceptance test-iqdemod-ssb-acceptance
	@echo "✅ Acceptance tests completed!"

# Cross-platform test runner
test-cross-platform:
	@echo "🌐 Running cross-platform tests..."
	@if [ "$$OS" = "Windows_NT" ]; then \
		if exist tests\test_runner.bat ( \
			tests\test_runner.bat \
		) else ( \
			echo "Windows test runner not found"; \
			exit 1; \
		); \
	else \
		if [ -f tests/test_runner.sh ]; then \
			./tests/test_runner.sh; \
		else \
			echo "Unix test runner not found"; \
			exit 1; \
		fi; \
	fi

clean:
	rm -rf $(BUILD_DIR) $(TOOLS)

# Development helpers
.PHONY: all clean dirs test test-comprehensive test-quick test-unit test-integration test-acceptance test-cross-platform
