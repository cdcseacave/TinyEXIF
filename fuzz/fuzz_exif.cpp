// fuzz_exif.cpp -- libFuzzer harness for TinyEXIF::EXIFInfo::parseFrom().
//
// This exercises the exact entry point that the third memory-safety report
// against this parser went through: attacker-controlled JPEG bytes handed
// straight to the EXIF/XMP parser, with no assumptions about well-formedness.
// See fuzz/README.md for how to build, seed, and run it.
//
// This file is only compiled when BUILD_FUZZER is turned on (see
// CMakeLists.txt); it is not part of the library and is not built by
// default.
#include "TinyEXIF.h"
#include <cstddef>
#include <cstdint>
#include <limits>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	// EXIFInfo::parseFrom() takes an 'unsigned' length; guard the narrowing
	// conversion from libFuzzer's size_t instead of assuming it never fires.
	if (size > (std::numeric_limits<unsigned>::max)())
		return 0;

	TinyEXIF::EXIFInfo info;
	info.parseFrom(data, static_cast<unsigned>(size));
	return 0;
}
