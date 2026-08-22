# Fuzzing TinyEXIF

`fuzz_exif.cpp` is a [libFuzzer](https://llvm.org/docs/LibFuzzer.html) harness over
`TinyEXIF::EXIFInfo::parseFrom(const uint8_t* data, unsigned length)` -- the same entry point
an attacker-supplied JPEG reaches. Three memory-safety reports against this parser (#25, #26,
and the 1.1.0 bounds-check pass) were all the same bug class: an attacker-controlled offset or
length trusted without validation. This harness is how the next one gets caught before it
reaches an issue tracker.

## Building

Requires a Clang toolchain that ships compiler-rt's fuzzer runtime. Upstream Clang does, so any
recent Linux distribution's `clang` works, as does Homebrew's `llvm@18`; Xcode's **AppleClang**
does **not**, so on macOS point CMake at a Homebrew LLVM instead of the system compiler.
`BUILD_FUZZER` is off by default and does not affect the normal library/demo build.

```
cmake -S . -B build-fuzz -DBUILD_FUZZER=ON -DBUILD_SHARED_LIBS=OFF \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build-fuzz -j8
```

This produces `build-fuzz/TinyEXIFfuzzer`. On macOS with Homebrew's LLVM, substitute the full
paths, e.g. `-DCMAKE_CXX_COMPILER=$(brew --prefix llvm@18)/bin/clang++`.

`BUILD_FUZZER=ON` also compiles the `TinyEXIF` library target itself with
`-fsanitize=fuzzer-no-link`, and that is a requirement rather than a nicety. libFuzzer is
coverage-guided, and the coverage it steers by comes from SanitizerCoverage callbacks the
compiler inserts into the code under test. Instrument only the harness and the fuzzer sees the
same ten-line trace on every input, gets no signal out of the parser, and degenerates into blind
mutation of the seed corpus. `-fsanitize=fuzzer-no-link` inserts those callbacks without
libFuzzer's `main()`, which belongs to the executable alone. Any other way of building this
harness has to instrument the library the same way.

### Combining with AddressSanitizer

For deeper bug-finding you normally want `-fsanitize=fuzzer,address` instead of `-fsanitize=fuzzer`
alone, which the CMake target does not enable by default (add it via
`target_compile_options`/`target_link_options` overrides, or build with `CXXFLAGS` containing
`-fsanitize=address` in your own tree). Whichever route you take, make sure `-fsanitize=address`
reaches the `TinyEXIF` target and not just the harness -- same reasoning as the coverage flag
above: a sanitizer only reports on the code it instrumented, and the parser is the code under
test. **Known issue on this project's macOS development host:** an ASan-instrumented binary hangs
at process startup on Apple Silicon macOS (both Apple Clang and Homebrew LLVM), independent of
TinyEXIF. The `ASan+UBSan (ubuntu-latest)` job in `.github/workflows/build.yml` runs the corpus
under sanitizers on Linux for that reason, and is the supported way to get sanitizer coverage of
this project. Plain `-fsanitize=fuzzer` (no ASan) runs fine natively on macOS and still catches
crashes (SIGSEGV, SIGABRT, out-of-memory) even without ASan's finer-grained instrumentation.

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
(`SubjectArea`) was handled in `EXIFInfo::parseIFDExif()` as:

```cpp
if (parser.IsShort() && parser.GetLength() > 1) {
	SubjectArea.resize(parser.GetLength());
	...
```

`parser.GetLength()` is the IFD entry's raw "component count" field, and it went straight into
the `resize()` with no check against the actual buffer size. A single crafted 12-byte IFD entry
(`tag=0x9214, format=3 (SHORT), count=0xdfdfdfdf`) was enough on its own -- the value/offset
field is never read -- and drove `std::vector<uint16_t>::resize()` to attempt to allocate
~7.5 GB, a genuine denial-of-service primitive against anything that parses untrusted JPEGs with
this library.

**This was fixed in 1.1.0** (see the Security section of `CHANGELOG.md`): the entry's whole array
must now be shown to fit inside the EXIF buffer before the vector is sized to match it.
`Samples/fuzz/poc-subjectarea-alloc-dos.jpg` is the regression fixture for it, and
`Samples/fuzz/poc-subjectarea-count-wrap.jpg` pins the overflow guard on the same field. Both are
part of the `TestSamples.py` corpus gate, so a regression fails CI.
