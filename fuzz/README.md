# Fuzzing TinyEXIF

`fuzz_exif.cpp` is a [libFuzzer](https://llvm.org/docs/LibFuzzer.html) harness over
`TinyEXIF::EXIFInfo::parseFrom(const uint8_t* data, unsigned length)` -- the same entry point
an attacker-supplied JPEG reaches. Three memory-safety reports against this parser (#25, #26,
and the bounds-check pass in this branch) were all the same bug class: an attacker-controlled
offset or length trusted without validation. This harness is how the next one gets caught before
it reaches an issue tracker.

## Building

Requires a Clang toolchain with `-fsanitize=fuzzer` support (upstream Clang or Xcode's
AppleClang do **not** ship the fuzzer runtime on their own; Homebrew's `llvm@18` does, and any
recent Linux Clang does too). `BUILD_FUZZER` is off by default and does not affect the normal
library/demo build.

```
cmake -S . -B build-fuzz -DBUILD_FUZZER=ON -DBUILD_SHARED_LIBS=OFF \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build-fuzz -j8
```

This produces `build-fuzz/TinyEXIFfuzzer`. On macOS with Homebrew's LLVM, substitute the full
paths, e.g. `-DCMAKE_CXX_COMPILER=$(brew --prefix llvm@18)/bin/clang++`.

### Combining with AddressSanitizer

For deeper bug-finding you normally want `-fsanitize=fuzzer,address` instead of `-fsanitize=fuzzer`
alone, which the CMake target does not enable by default (add it via
`target_compile_options`/`target_link_options` overrides, or build with `CXXFLAGS` containing
`-fsanitize=address` in your own tree). **Known issue on this project's macOS development
host:** an ASan-instrumented binary hangs at process startup on Apple Silicon macOS (both Apple
Clang and Homebrew LLVM), independent of TinyEXIF -- see the sanitizer CI job below and its
container-based verification for the working combination. Plain `-fsanitize=fuzzer` (no ASan)
runs fine natively on macOS and still catches crashes (SIGSEGV, SIGABRT, out-of-memory) even
without ASan's finer-grained instrumentation.

## Seeding and running

Point it at a corpus directory seeded from the existing regression samples -- both the real-world
photos under `Samples/` and the crafted out-of-bounds regression inputs under `Samples/fuzz/`:

```
mkdir -p /tmp/tinyexif-corpus
cp Samples/*.jpg Samples/fuzz/*.jpg /tmp/tinyexif-corpus/

./build-fuzz/TinyEXIFfuzzer -max_total_time=60 -print_final_stats=1 /tmp/tinyexif-corpus
```

Use a directory outside the repository (not `Samples/` itself): libFuzzer treats its first
positional argument as both the seed corpus and the place it writes newly-discovered inputs back
to, and those generated files should not end up tracked in git.

Always bound a run with `-max_total_time=<seconds>` (or `-runs=<N>`); an unbounded
`./TinyEXIFfuzzer /tmp/tinyexif-corpus` runs until it finds a crash or is killed. Crash/OOM/timeout
artifacts are written to the current directory by default -- pass `-artifact_prefix=<dir>/` to
put them somewhere deliberate.

## What it found (2026-08-22 run)

A three-minute run against the seed corpus above found a real, reproducible bug within the first
minute, on two independent runs: an **unbounded attacker-controlled allocation**. Tag `0x9214`
(`SubjectArea`) is handled in `EXIFInfo::parseIFDExif()` as:

```cpp
if (parser.IsShort() && parser.GetLength() > 1) {
	SubjectArea.resize(parser.GetLength());
	...
```

`parser.GetLength()` is the IFD entry's raw "component count" field, taken directly from the
file with no check against the actual buffer size before the `resize()`. A single crafted
12-byte IFD entry (`tag=0x9214, format=3 (SHORT), count=0xdfdfdfdf`) is sufficient on its own --
the value/offset field is never read -- and drives `std::vector<uint16_t>::resize()` to attempt
to allocate ~7.5 GB, which is a genuine denial-of-service primitive against anything that parses
untrusted JPEGs with this library. This is **not fixed by this task**; see
`.superpowers/sdd/IMPLEMENTATION_PLAN/task-8-report.md` for the full writeup and reproduction
recipe. Do not treat this README's mention of it as resolved.
