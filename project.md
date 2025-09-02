# PRD — IQ Lab (SIGINT / Radioamateur Toolkit)
---

## 0. Summary

* **Goal:** Offline, C-first toolkit to read IQ files and produce auditable SIGINT artifacts: spectra, waterfalls, audio, detections, and structured event logs.
* **Users:** Military analyst, SIGINT expert, radioamateur.
* **Principles:** Small composable CLI bricks, deterministic runs, SigMF metadata, minimal deps, correctness before speed.
* **MVP path:** `iqinfo` → `iqls` → `iqcut` → `iqdemod-*` → `iqdetect` → `iqchan` → (optional) `iqtdoa`.

---

## 1. Objectives

1. **Ingest IQ reliably**: s8/s16 interleaved, SigMF metadata.
2. **Visualize**: spectrum + waterfall images with correct axes.
3. **Listen**: FM/AM/SSB demod to WAV.
4. **Detect**: CFAR-based signal events with timestamps, bandwidth, SNR.
5. **Scale**: channelization + batch jobs (YAML) + reproducible manifests.
6. **Extend**: optional TDoA (simulation-first), calibration, protocol decoders.

---

## 2. Non-Functional Requirements

* **Determinism:** Same inputs/params → byte-identical outputs.
* **Auditability:** Run manifest with versions, hashes, params.
* **Standards:** SigMF read/write for metadata.
* **Performance (baseline):**

  * Process ≥ 8 Msps offline on a 4-core laptop (scalar; SIMD later).
  * Memory usage stable under stream processing (fixed-size blocks).
* **Portability:** C11, Linux + Windows.
* **Security:** No network use; all operations on local files.

---

## 3. Inputs & Outputs

### 3.1 IQ Input

* **Formats:** `s8` or `s16`, interleaved `[I0,Q0,I1,Q1,...]`, little-endian.
* **Metadata:** SigMF `.sigmf-meta` (JSON).

**Minimal SigMF fields (required):**

```json
{
  "global": {
    "core:datatype": "ci8" | "ci16",
    "core:sample_rate": 2400000,
    "core:version": "1.2.0",
    "core:description": "Capture description",
    "core:author": "user"
  },
  "captures": [{
    "core:sample_start": 0,
    "core:frequency": 100000000,
    "core:datetime": "2025-09-01T10:00:00Z"
  }]
}
```

### 3.2 Artifacts (Outputs)

* **Images:** `spectrum.png|ppm`, `waterfall.png|ppm` (8-bit grayscale).
* **Audio:** `*.wav` (PCM16, mono, 48 kHz default).
* **Events:** `events.csv` or `events.jsonl`.
* **Sub-IQ cutouts:** `event_###.iq` + updated `event_###.sigmf-meta`.
* **Run manifest:** `run.json` (see §9.3).

**Events schema (CSV header):**

```
t_start_s,t_end_s,f_center_Hz,bw_Hz,snr_dB,peak_dBFS,modulation_guess,confidence_0_1,tags
```

---

## 4. Use Cases

### 4.1 Military Analyst

* **Batch triage** of many hours of IQ: produce an **events timeline** with confidence.
* **Reproducible reports**: manifests + fixed params per run.

### 4.2 SIGINT Expert

* **Evaluate detectors** with labeled synthetic sets (ROC curves).
* **Measure demod quality** (audio SNR vs input SNR).

### 4.3 Radioamateur

* **Quick visualize** spectrum/waterfall.
* **Tune & listen** to WFM/NFM/AM/SSB.
* **Slice sub-bands** from large IQ for sharing.

---

## 5. Feature Roadmap (Phases & Acceptance)

### Phase 0 — Foundations (Week 1)

**Tools:** `iqinfo`, core libs, SigMF I/O, PNG writer, WAV writer.

* **iqinfo**

  * **Inputs:** `--in <file.iq> --format {s8|s16} --rate <Hz> [--meta <file.sigmf-meta>]`
  * **Outputs (stdout JSON):** duration\_s, rms\_dBFS, dc\_offset\_IQ, top\_freq\_bins\_Hz, est\_noise\_floor\_dBFS.
  * **Acceptance:** RMS error ≤ ±0.2 dB vs reference; duration within ±1 sample.

* **Core libs delivered:** `io_iq`, `io_sigmf`, `window`, `fft` (kissfft or radix-2), `fir`, `resampler_pffir`, `img_png` (tiny), `img_ppm`, `wav_writer`, `stats`, `manifest`.

### Phase 1 — Visualize (Week 1–2)

**Tools:** `iqls`, `iqcut`.

* **iqls**

  * **Inputs:** `--fft 4096 --hop 1024 --avg 20 --logmag --waterfall`
  * **Outputs:** `spectrum.png`, `waterfall.png`, optional `peaks.csv`.
  * **Acceptance:** Peak bin error ≤ ±1 bin on synthetic tone; axis labels match center\_freq & sample\_rate.

* **iqcut**

  * **Function:** Frequency-translate (digital LO), LPF, decimate, and cut time window.
  * **Inputs:** `--f_center <Hz> --bw <Hz> --t_start <s> --t_end <s>`
  * **Outputs:** `sub.iq` + `sub.sigmf-meta`.
  * **Acceptance:** LO leak ≤ −40 dBc on synthetic; passband ripple ≤ 1 dB.

### Phase 2 — Listen (Week 2–3)

**Tools:** `iqdemod-fm`, `iqdemod-am`, `iqdemod-ssb`, `iqtune` (headless tuner).

* **iqdemod-fm**

  * **Algo:** Δphase discriminator, deemphasis (50/75 μs), optional decim to 48 kHz.
  * **Acceptance:** Audio SNR ≥ 25 dB @ input SNR 30 dB (synthetic FM).

* **iqdemod-am / iqdemod-ssb**

  * **Algo:** AM (envelope or product), SSB (BFO + BPF + downmix).
  * **Acceptance:** AM envelope distortion < 5% on synthetic; SSB intelligible at SNR ≥ 10 dB.

* **iqtune**

  * **CLI:** `--mode {wfm|nfm|am|usb|lsb} --f_offset <Hz> --bw <Hz> --audio out.wav`
  * **Output:** small `tune_spectrum.png` snapshot + WAV.

### Phase 3 — Detect (Week 4–5)

**Tools:** `iqdetect`.

* **iqdetect**

  * **Algo:** OS-CFAR in frequency domain (configurable `pfa`), temporal hysteresis, clustering → events.
  * **Inputs:** `--fft 4096 --hop 1024 --pfa 1e-3 --min_dur_ms 50`
  * **Outputs:** `events.csv|jsonl` + optional `--cut` to create `event_*.iq`.
  * **Acceptance (synthetic labeled set):** Pd ≥ 0.90 @ SNR ≥ 8 dB, Pfa ≈ configured ±20%; time/freq localization error ≤ (hop/2, bin/2).

### Phase 4 — Scale (Month 2)

**Tools:** `iqchan`, `iqjob` (batch runner).

* **iqchan**

  * **Algo:** Polyphase filter bank (PFB) channelizer into N fixed-width bands.
  * **Inputs:** `--channels 8 --bandwidth 200e3`
  * **Outputs:** `chan_k/chan_k.iq` + per-channel spectra.
  * **Acceptance:** Channel isolation ≥ 55 dB on synthetic multi-tone.

* **iqjob** (YAML batch runner)

  * **Inputs:** YAML pipeline (see §8.2).
  * **Outputs:** Consolidated report folder + `run.json`.
  * **Acceptance:** Deterministic outputs across runs (hash-stable).

### Phase 5 — Advanced (Month 3+)

**Tools:** `iqtdoa` (simulation-first), `iqcal` (ppm/beacon).

* **iqtdoa**

  * **Algo:** Cross-correlation Δt with sub-sample parabolic interp; least-squares hyperbola intersection; heatmap.
  * **Acceptance (synthetic):** Δt error < 0.2 sample @ SNR ≥ 15 dB; position error < 2% of baseline geometry.

* **iqcal**

  * **Function:** Frequency calibration vs known beacon; estimate ppm and correct FFT axis.
  * **Acceptance:** Residual frequency error < 0.2 ppm post-cal on synthetic drift.

---

## 6. System Architecture

### 6.1 Modules (Key Root Components)

```
/src/iq_core/
  io_iq.{c,h}           // s8/s16 streaming reader, interleaved → float [-1,1]
  io_sigmf.{c,h}        // read/write SigMF JSON
  window.{c,h}          // Hann/Hamming/Blackman
  fft.{c,h}             // radix-2 or kissfft adapter
  fir.{c,h}             // FIR, decim, polyphase bank
  resampler_pffir.{c,h} // rational resampler
  stats.{c,h}           // RMS, dBFS, peak bins, noise floor (median)
  iq_corr.{c,h}         // DC removal, IQ balance
  cfo_track.{c,h}       // coarse CFO estimation/tracking
  rng.{c,h}             // deterministic seeds for synth
  manifest.{c,h}        // run.json writer (hashes, args, versions)
/src/viz/
  img_ppm.{c,h}; img_png.{c,h}   // 8-bit grayscale
  draw_axes.{c,h}                 // annotate Hz bins, labels
/src/demod/
  fm.{c,h}; am.{c,h}; ssb.{c,h}; agc.{c,h}; wave.{c,h}
/src/detect/
  cfar_os.{c,h}; cluster.{c,h}; features.{c,h}
/src/chan/
  pfb.{c,h}; scheduler.{c,h}
/src/jobs/
  yaml_parse.{c,h}; pipeline.{c,h}; logger.{c,h}
```

### 6.2 CLI Tools

* `iqinfo`, `iqls`, `iqcut`, `iqdemod-fm`, `iqdemod-am`, `iqdemod-ssb`, `iqtune`, `iqdetect`, `iqchan`, `iqjob`, (optional) `iqtdoa`, `iqcal`.

---

## 7. Interfaces

### 7.1 Common CLI pattern

```
<tool> --in <path.iq> [--meta <path.sigmf-meta>] --format {s8|s16} --rate <Hz> [tool-specific options] --out <prefix|file>
```

### 7.2 Events JSONL record (one per line)

```json
{
  "t_start_s": 12.384,
  "t_end_s": 12.612,
  "f_center_Hz": 100050000.0,
  "bw_Hz": 12500.0,
  "snr_dB": 14.2,
  "peak_dBFS": -22.7,
  "modulation_guess": "fm",
  "confidence_0_1": 0.78,
  "tags": ["burst","candidate"]
}
```

### 7.3 Run Manifest (`run.json`)

```json
{
  "tool": "iqdetect",
  "version": "0.3.1",
  "git_commit": "abcdef1",
  "args": "--in data/cap.iq --fft 4096 --pfa 1e-3",
  "inputs": [{
    "path": "data/cap.iq",
    "bytes": 123456789,
    "sha256": "..."
  },{
    "path": "data/cap.sigmf-meta",
    "sha256": "..."
  }],
  "env": {"os": "linux", "cpu": "x86_64"},
  "datetime_utc": "2025-09-01T10:00:00Z"
}
```

### 7.4 YAML Job (for `iqjob`)

```yaml
inputs:
  - file: data/capture.iq
    meta: data/capture.sigmf-meta
pipeline:
  - iqls:   { fft: 4096, hop: 1024, avg: 20, logmag: true }
  - iqdetect: { fft: 4096, hop: 1024, pfa: 1e-3, min_dur_ms: 50, cut: true }
  - iqdemod-fm: { on_events_mod: "fm", deemphasis_us: 50, audio_rate: 48000 }
report:
  consolidate: { events: true, spectra: true, audio_snippets: true }
```

---

## 8. Data & Testing

### 8.1 Synthetic Generators (bundled)

* `gen_tone`: tone @ f0 Hz, SNR dB, duration s.
* `gen_fm`: FM with small WAV as message; set deviation.
* `gen_bursts`: on/off carriers, variable widths/durations.
* `gen_chirp`: linear FM chirp (for detector robustness).

### 8.2 Labeled Sets

* JSON labels per file:

```json
{
  "file": "mix.iq",
  "sample_rate": 2000000,
  "center_freq": 100000000,
  "events": [
    {"t_start_s": 2.0, "t_end_s": 2.2, "f_center_Hz": 100020000, "bw_Hz": 10000, "label": "burst"}
  ]
}
```

### 8.3 Acceptance Tests (per phase)

* **Phase 1:** peak bin location; RMS/PSD error margins.
* **Phase 2:** audio SNR and distortion thresholds.
* **Phase 3:** detector Pd/Pfa and localization error vs labels.
* **Phase 4:** channel isolation (dB), throughput benchmark.
* **Phase 5:** Δt and geolocation error on synthetic.

---

## 9. Calibration & Corrections

* **DC removal / IQ balance:** per-block mean removal; optional I/Q gain/phase corr.
* **CFO tracking:** coarse peak tracking + frequency re-grid for long waterfalls.
* **Frequency calibration (`iqcal`):** estimate ppm via beacon; apply to axes.
* **Time normalization:** prefer SigMF `captures[].core:datetime`, else user-provided offsets.

---

## 10. Reporting

* **Artifacts directory layout:**

```
runs/<timestamp>_<tool>/
  run.json
  *.png|ppm
  *.wav
  events.csv|jsonl
  event_###.iq + event_###.sigmf-meta
  logs.txt
```

* **Optional HTML summary:** single-page report from CSV/PNGs.

---

## 11. Risks & Mitigations

* **Format ambiguity:** Enforce SigMF; provide converter CLI.
* **Detector tuning sensitivity:** Use Pfa-driven OS-CFAR; provide labeled synthetic ROC evaluation.
* **Performance bottlenecks:** Start scalar; later add AVX2/NEON kernels for FIR/mag²; double-buffered I/O.
* **Clock/LO drift:** CFO tracker + optional calibration step.

---

## 12. Dependencies & Licensing

* **Language:** C11.
* **Optional libs:** `kissfft` (BSD), minimal YAML parser (MIT), tiny PNG writer (public-domain/MIT).
* **License:** MIT or BSD-2 for project code.

---

## 13. Directory Structure

```
/src
  /iq_core /viz /demod /detect /chan /jobs
/tools
  iqinfo iqls iqcut iqdemod-fm iqdemod-am iqdemod-ssb iqtune iqdetect iqchan iqjob iqtdoa iqcal
/data
  /synthetic (generated IQ + labels)  /real (small public samples)
/tests
  unit/ integration/ golden/
/docs
  PRD.md  CLI.md  DEV_NOTES.md
Makefile or CMakeLists.txt
```

---

## 14. Backlog (Prioritized)

1. Core libraries + `iqinfo` + SigMF I/O + PNG/WAV + manifest.
2. `iqls` + `iqcut` + acceptance tests.
3. `iqdemod-fm`, `iqdemod-am`, `iqdemod-ssb`, `iqtune`.
4. `iqdetect` (OS-CFAR + clustering + events schema + cutouts).
5. `iqchan` + `iqjob` (YAML runner) + consolidated reports.
6. (Optional) `iqtdoa` + `iqcal`.
7. Stretch: RDS/ADS-B decoders, SIMD kernels, HTML report.

---

## 15. Open Questions

* **SigMF fields:** do we require geotags (`core:geolocation`) from day 1?
* **PNG vs PPM:** keep both or standardize on PNG once tiny encoder is stable?
* **Detector defaults:** preferred `pfa` default (e.g., `1e-3`) across bands?
* **SSB defaults:** recommended BW per band (e.g., 2.4–2.8 kHz)?
* **Batch reports:** do we ship an HTML reporter in MVP-4 or later?

---

## 16. Success Criteria (Definition of Done)

* **MVP complete** when:

  * Phases 0–3 acceptance tests pass on CI with synthetic and one small real sample.
  * Events schema, manifests, and artifacts are reproducible (hash-stable).
  * Documentation for each CLI includes example commands and expected outputs.
* **Project success** when:

  * Analysts can run a YAML job over hours of IQ and obtain a consolidated events report with spectra and selected audio, reproducibly.
  * Radioamateurs can visualize, tune, and export audio within minutes on a commodity laptop.
