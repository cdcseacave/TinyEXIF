# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
this project uses [Semantic Versioning](https://semver.org/).

## [Unreleased] - 1.1.0

### Added
- `HasField()` / `GetFields()` API to tell an absent tag apart from one that is
  legitimately zero, with one `FieldID` enumerator per data field and
  `FieldName()` to print it. Purely additive: existing fields, sentinels
  (`DBL_MAX`, `UINT32_MAX`) and `hasXxx()` accessors are unchanged. (#15)
- `GPano_t` extended with the rest of the Google spherical-metadata field
  set: `PoseHeadingDegrees`, `ProjectionType`, the four cropped/full pano
  pixel dimensions, `CroppedAreaLeftPixels`, `CroppedAreaTopPixels`, and
  `isEquirectangular()`.
- `GCamera:MotionPhoto` parsing into new `HasMotionPhoto`, `MotionPhotoLength`
  and `MotionPhotoMime` fields, kept separate from the pre-existing
  `MicroVideoOffset` since the two are not interchangeable; the container is
  only walked when the file actually declares one.
- `TINYEXIF_VERSION_STRING` / `TINYEXIF_VERSION` macros, derived from the
  major/minor/patch macros, for downstream feature-gating.
- A libFuzzer target (`BUILD_FUZZER`, Clang-only), an ASan+UBSan CI job
  running the full `Samples/` corpus under sanitizers, and `SECURITY.md`
  documenting the disclosure process.
- `LICENSE.easyexif`, reproducing easyexif's BSD-2-Clause notice, with
  pointers from `LICENSE` and `README.md` noting that portions of this code
  derive from easyexif and remain additionally subject to it.
- `TestSamples.py` rewritten from an ad hoc script into a real pass/fail
  regression runner (with a subprocess timeout), now the CI gate for both
  the sanitizer job and this release's parser changes.

### Changed
- **`GPSAltitudeRef` sign handling — parsed values change for `ref` 1 or 3.**
  Some writers (e.g. DroneDeploy) emit an already-negative `GPSAltitude` and
  still set `AltitudeRef = 1`; the old code re-negated it, flipping the sign
  back to positive. The magnitude is now taken before negating, and
  `AltitudeRef == 3` (negative sea-level reference) is now handled at all —
  it was previously ignored. `AltitudeRef` 0 and 2 are untouched. Downstream
  consumers of `GeoLocation.Altitude` should check whether their inputs use
  refs 1 or 3.
- Version is now single-sourced from `TinyEXIF.h`; `CMakeLists.txt` reads it
  from the header's macros and fails the configure step if `vcpkg.json`'s
  `version-string` disagrees, fixing drift between the two (the header read
  1.0.3 while CMake/vcpkg.json read 1.0.4).

### Fixed
- Unbounded allocation from an attacker-controlled `SubjectArea` component
  count: a single crafted 12-byte IFD entry could drive a multi-gigabyte
  `std::vector::resize()` from a file a few dozen bytes long. Found by this
  project's own new fuzzer.

### Security
- Bounds-checked every attacker-controlled buffer read reachable through
  `EntryParser::Fetch`, `ParseTag()`, MakerNote parsing, and the EXIF
  segment offset walk, closing a reported crash in `Fetch(double&)`, which
  previously had no bounds check at all. Reported by **doopal** (handle
  only — see note below).
- Fixed a heap buffer overflow in `EntryParser::Fetch` methods reachable via
  a crafted `SubjectArea` length. (#25, fixes #24)
- Fixed an integer overflow in the `parseString` bounds check that could
  pass validation on an attacker-controlled offset near `UINT32_MAX`. (#26,
  fixes #16)

> The "doopal" credit above uses the handle only, per the reporter's
> apparent preference — no email address is published here. The maintainer
> should confirm how they'd like to be credited before this entry ships.

## [1.0.4] - 2025-11-17

### Added
- DJI lens-distortion parsing. (#21)
- `LICENSE` (MIT); source file headers and README updated to match.

### Changed
- CMake project modernized. (#23)

### Fixed
- Robust parsing when an IFD offset is missing.

## [1.0.3] - 2025-01-01

### Added
- Optional XMP support: the library builds without tinyxml2 when XMP
  parsing isn't needed. (#10)
- `std::istream`-based constructor. (#11)
- Google Camera motion-photo metadata support. (#12)
- `GPano:PosePitchDegrees` / `GPano:PoseRollDegrees` parsing. (#8)
- Big-endian CPU support. (#14)

### Fixed
- MSVC++ UNICODE builds. (#9)
- Missing `<cstdint>` include. (#17)

## [1.0.2] - 2019-01-16

### Added
- XMP metadata support for Sentera, DJI, PARROT and senseFly cameras.
- XMP-stored-inside-EXIF parsing, GPS accuracy parsing, DJI MakerNote
  support and calibration-information extraction.
- Test images.

### Fixed
- Rational / signed-rational EXIF parsing.
- tinyxml2 namespace usage.

## [1.0.1] - 2017-12-09

### Added
- Generic input-stream interface, overridable for memory or file input.
- Fallback recovery of missing EXIF fields from XMP.

### Fixed
- Linux compile.

## [1.0.0] - 2017-11-17

Initial release, forked from easyexif.

### Added
- Projection-type parsing; `ExposureProgram`, `LightSource` and
  `SubjectArea` tags; `DigitalZoomRatio`; camera status and serial-number
  fields.
- CMake build support.
