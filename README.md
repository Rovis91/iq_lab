# IQ Lab - SIGINT/Radioamateur Toolkit

A high-performance, offline toolkit for analyzing IQ (In-phase/Quadrature) data from Software Defined Radio captures. Built in pure C11 for maximum performance and portability.

## 🎯 Overview

IQ Lab processes raw IQ data files to produce auditable SIGINT artifacts:
- **Spectrum analysis** with waterfall visualizations
- **Signal detection** using CFAR algorithms
- **Audio demodulation** (FM/AM/SSB)
- **Frequency channelization** and batch processing
- **Structured event logs** with timestamps and metadata

### Key Features
- ✅ **Offline-first**: No network dependencies, all processing local
- ✅ **Deterministic**: Same inputs produce identical outputs
- ✅ **Standards-compliant**: SigMF metadata support
- ✅ **High-performance**: C11 optimized for real-time processing
- ✅ **Cross-platform**: Linux, Windows, macOS
- ✅ **Minimal dependencies**: Only standard C libraries

## 🚀 Quick Start

### Prerequisites
- **C11 compiler** (GCC 7+, Clang 6+, or MSVC 2019+)
- **Make** build system
- **Standard C libraries** (stdlib, math, etc.)

#### Optional Prerequisites (for KiwiSDR recording)
- **Python 3.6+** (for KiwiSDR recording tool only)
- **mod_pywebsocket** (for KiwiSDR WebSocket connections)
- **PyYAML** (optional, for advanced KiwiSDR configuration)

### Build
```bash
# Clone and build
git clone https://github.com/Rovis91/iq_lab
cd iq_lab
make all
```

### Basic Usage
```bash
# Analyze IQ file statistics
./iqinfo --in capture.iq --format s16 --rate 2400000

# Generate spectrum and waterfall
./iqls --in capture.iq --fft 4096 --hop 1024 --waterfall

# Generate PNG spectrograms and waterfalls from IQ data
./generate_images

# Detect signals
./iqdetect --in capture.iq --pfa 1e-3 --min_dur_ms 50
```

## 📁 Project Structure

```
iq_lab/
├── src/
│   ├── iq_core/          # Core IQ processing libraries
│   ├── converter/        # Universal file converter
│   ├── viz/              # Visualization (PNG/PPM)
│   ├── demod/            # Demodulation (FM/AM/SSB)
│   ├── detect/           # Signal detection (CFAR)
│   ├── chan/             # Channelization (PFB)
│   └── jobs/             # Batch processing (YAML)
├── tools/                # CLI tools
├── data/                 # Sample data and tests
├── tests/                # Unit and integration tests
└── docs/                 # Documentation
```

## 🛠️ Tools

### Core Tools (Phase 0)
- **`iqinfo`** - IQ file statistics and metadata
- **`iqls`** - Spectrum analysis and waterfall generation
- **`iqcut`** - Extract time/frequency segments
- **`generate_images`** - Generate PNG spectrograms and waterfalls from IQ data

### Demodulation Tools (Phase 1)
- **`iqdemod-fm`** - FM demodulation to audio
- **`iqdemod-am`** - AM demodulation to audio
- **`iqdemod-ssb`** - SSB demodulation to audio
- **`iqtune`** - Quick tuner with spectrum snapshot

### Advanced Tools (Phase 2+)
- **`iqdetect`** - Signal detection with CFAR algorithm
- **`iqchan`** - Polyphase filter bank channelization
- **`iqjob`** - YAML-based batch processing pipeline
- **`iqtdoa`** - Time Difference of Arrival (TDoA) positioning
- **`iqcal`** - Frequency calibration

### 🎛️ Optional: KiwiSDR Recording Tool

> **Note**: Optional script using external [kiwiclient](https://github.com/jks-prv/kiwiclient) project for capturing IQ data from KiwiSDR servers.

- **`record_iq_from_kiwi.ps1`** - PowerShell script to record IQ data

```powershell
# Setup: Clone kiwiclient and install deps
git clone https://github.com/jks-prv/kiwiclient.git ../kiwiclient
pip install mod-pywebsocket pyyaml

# Record from KiwiSDR (outputs .wav files)
.\tools\record_iq_from_kiwi.ps1 -Server wessex.zapto.org -Freq 7100.0 -Duration 30
```

## 📊 Input/Output Formats

### IQ Input Formats
- **Raw IQ**: `s8` or `s16` interleaved `[I0,Q0,I1,Q1,...]` (little-endian)
- **WAV IQ**: Standard WAV files containing IQ data
- **RF64**: Extended WAV format for large files (>4GB)
- **Metadata**: SigMF JSON sidecar files (`.sigmf-meta`)

### Output Formats
- **Images**: PNG spectrograms and waterfalls with calibrated axes
  - Spectrograms: Frequency (x-axis) vs Power (y-axis in dB)
  - Waterfalls: Frequency (x-axis) vs Time (y-axis, newest at top)
  - Color-coded power levels with automatic scaling
- **Audio**: WAV files (PCM16, mono, 48kHz default)
- **Events**: CSV or JSONL structured event logs
- **IQ Cutouts**: Extracted IQ segments with metadata
- **Reports**: JSON run manifests with hashes and parameters

### Events Schema
```csv
t_start_s,t_end_s,f_center_Hz,bw_Hz,snr_dB,peak_dBFS,modulation_guess,confidence_0_1,tags
```

## 🔧 Build System

### Requirements
- **C11 standard** compilation
- **GCC/Clang/MSVC** with optimization flags
- **Make** for build automation

### Build Targets
```bash
make all          # Build all tools
make iqinfo       # Build specific tool
make test         # Run test suite
make clean        # Clean build artifacts
make install      # Install to system (optional)
```

## 🧪 Testing

IQ Lab includes a comprehensive test suite to ensure code quality and mathematical accuracy.

### Running Tests

```bash
# Build and run all tests
./tests/run_tests.exe --build
./tests/run_tests.exe --run

# Or run individual test suites
./tests/unit/test_sigmf.exe       # SigMF metadata tests
./tests/unit/test_fft.exe         # FFT algorithm tests
./tests/unit/test_window.exe      # Window function tests
./tests/integration/test_pipeline.exe  # Complete pipeline tests
```

### Test Coverage

- **SigMF Tests**: Metadata I/O, parsing, validation, edge cases
- **FFT Tests**: Mathematical accuracy, reconstruction, frequency detection
- **Window Tests**: All 6 window types, coefficient accuracy, application
- **Integration Tests**: Complete signal processing pipeline validation

### Test Results

Expected successful test run:
```
========================================
IQ Lab - Complete Test Suite
========================================

🎉 All tests PASSED!
✅ Complete signal processing pipeline validated
```

### Test Documentation

See `tests/README.md` for detailed test documentation including:
- Test structure and organization
- Coverage metrics and edge cases
- Performance benchmarks
- Debugging failed tests
- Adding new tests

### Quality Metrics

- **Mathematical Accuracy**: FFT reconstruction error < 1e-12
- **Edge Case Coverage**: Division by zero, invalid inputs, memory limits
- **Performance**: Real-time FFT processing capability
- **Integration**: Complete SigMF → FFT → Window pipeline validation

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.


**Made in France 🇫🇷**
