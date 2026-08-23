#!/usr/bin/env python3
"""Generator for the crafted regression samples in Samples/fuzz/.

These are not real photos: each one is a minimal JPEG whose APP1/EXIF segment is
built to hit one specific out-of-bounds read that the parser used to perform.
They are checked in together with this generator so that the bytes stay
reviewable instead of being an opaque blob; re-run it to regenerate them:

    python3 Samples/fuzz/make_samples.py

Every sample is expected to parse without any sanitizer report and without
inventing fields. See Samples/fuzz/<name>.expected for the baseline output.

All samples use Motorola ("MM", big-endian) byte order so the crafted values
below read the same way they are written.
"""
import os
import struct

# JPEG/TIFF layout constants (see the block comment above parseFromEXIFSegment)
EXIF_ID = b'Exif\x00\x00'      # APP1 EXIF identifier
TIFF_HEADER_START = len(EXIF_ID)  # the TIFF header starts right after it
TIFF_HEADER = b'MM\x00\x2a'    # byte order + magic 42
IFD_ENTRY_SIZE = 12

# TIFF field format codes
FMT_ASCII = 2
FMT_SHORT = 3
FMT_LONG = 4
FMT_RATIONAL = 5
FMT_UNDEFINED = 7


def entry(tag, fmt, count, value):
	"""One 12-byte IFD entry; `value` is the raw 4-byte value/offset field."""
	assert len(value) == 4
	return struct.pack('>HHI', tag, fmt, count) + value


def ifd(entries):
	"""An IFD: entry count followed by the entries (no next-IFD offset)."""
	return struct.pack('>H', len(entries)) + b''.join(entries)


def exif_payload(body, first_ifd_offset=len(TIFF_HEADER) + 4):
	"""APP1 payload: "Exif\\0\\0", the TIFF header, then `body` (usually an IFD).

	`first_ifd_offset` is relative to the TIFF header start; the default (8)
	places the first IFD immediately after the header, as every real file does.
	"""
	return EXIF_ID + TIFF_HEADER + struct.pack('>I', first_ifd_offset) + body


def jpeg(payload):
	"""SOI + APP1(payload) + EOI -- the smallest container the parser accepts."""
	return (b'\xff\xd8' +
		b'\xff\xe1' + struct.pack('>H', len(payload) + 2) + payload +
		b'\xff\xd9')


def rational_oob():
	"""EntryParser::Fetch(double&): rational whose value offset is out of bounds.

	XResolution is a RATIONAL, so its 8-byte value lives at
	tiff_header_start + <offset>. The offset below points exactly one byte past
	the end of the APP1 payload, so the pre-patch parser read 8 bytes off the
	end of the buffer. This is the crash the security report described.
	"""
	body = ifd([entry(0x011a, FMT_RATIONAL, 1, b'\x00\x00\x00\x00')])
	payload_len = len(exif_payload(body))
	# make the rational start right at the first byte past the buffer
	offset = payload_len - TIFF_HEADER_START
	body = ifd([entry(0x011a, FMT_RATIONAL, 1, struct.pack('>I', offset))])
	return exif_payload(body)


def makernote_oob():
	"""EXIFInfo::parseIFDMakerNote(): entry count read from an unchecked offset.

	The MakerNote is only walked for DJI images, so Make must be set first.
	The pre-patch parser read the MakerNote's 2-byte entry count at
	tiff_header_start + <offset> before validating anything, and the length
	check that followed compared against the tag's own length field, not the
	buffer size.
	"""
	def build(offset):
		return exif_payload(ifd([
			entry(0x010f, FMT_ASCII, 4, b'DJI\x00'),
			entry(0x927c, FMT_UNDEFINED, 26, struct.pack('>I', offset)),
		]))
	# make the MakerNote header start right at the first byte past the buffer
	return build(len(build(0)) - TIFF_HEADER_START)


def subifd_offset_wrap():
	"""parseFromEXIFSegment(): "exif_sub_ifd_offset + 4 <= len" wraps around.

	GetSubIFD() is tiff_header_start + a fully attacker-controlled uint32, so it
	can be made to land just below 2^32. The pre-patch additive guard then
	wrapped to a small value, passed, and the parser read the sub-IFD entry
	count ~4GB past the buffer.
	"""
	offset = 0xffffffff - TIFF_HEADER_START - 2  # GetSubIFD() == 0xfffffffd
	return exif_payload(ifd([entry(0x8769, FMT_LONG, 1, struct.pack('>I', offset))]))


def lensinfo_offset_wrap():
	"""EntryParser::Fetch(double&, idx): GetSubIFD() itself wraps around.

	tiff_header_start + 0xffffffff wraps back to 5, which is inside the buffer, so
	the pre-patch parser happily decoded LensInfo rationals out of the "Exif\\0\\0"
	marker and the TIFF header -- an offset the file never pointed at. Padding puts
	four rationals (8 bytes each, starting at 5) inside the buffer so that more than
	one of them is fabricated.
	"""
	body = ifd([entry(0xa432, FMT_RATIONAL, 4, b'\xff\xff\xff\xff')])
	payload = exif_payload(body)
	wrapped = (TIFF_HEADER_START + 0xffffffff) & 0xffffffff
	return payload.ljust(wrapped + 4 * 8, b'\x00')


def first_ifd_offset_underflow():
	"""parseFromEXIFSegment(): "offs += first_ifd_offset - 4" underflows.

	With first_ifd_offset < 4 the subtraction wraps and offs lands back inside
	the buffer, on an IFD the file never declared. Here first_ifd_offset == 2
	makes the parser read its entry count from the TIFF magic (0x002a == 42) and
	then walk 42 entries over the raw header bytes -- entry 1 below is picked up
	as a bogus Orientation. The buffer is padded so the entries fit, otherwise
	the size check rejects the file before the bogus IFD is walked.
	"""
	first_ifd_offset = 2
	offs = TIFF_HEADER_START + first_ifd_offset      # 8: where the bogus IFD starts
	num_entries = 0x2a                               # read out of the TIFF magic
	payload_len = offs + 2 + IFD_ENTRY_SIZE * num_entries
	payload = bytearray(exif_payload(b'', first_ifd_offset).ljust(payload_len, b'\x00'))
	bogus = entry(0x0112, FMT_SHORT, 1, b'\x00\x07\x00\x00')  # Orientation = 7
	start = offs + 2 + IFD_ENTRY_SIZE                # entry 1 (entry 0 overlaps the header)
	payload[start:start + IFD_ENTRY_SIZE] = bogus
	return bytes(payload)


def subjectarea_alloc_dos():
	"""parseIFDExif(), tag 0x9214 (SubjectArea): unbounded allocation, not an OOB read.

	SubjectArea's component count is a fully attacker-controlled uint32 taken
	straight from the entry, with no check against the buffer before the
	pre-patch parser did `SubjectArea.resize(parser.GetLength())`. The value/
	offset field is never even read -- a single 12-byte IFD entry is enough.
	0xdfdfdfdf components as SHORT (2 bytes each) is a ~7.5 GB allocation
	attempt from a file that is a few dozen bytes.
	"""
	body = ifd([entry(0x9214, FMT_SHORT, 0xdfdfdfdf, b'\x00\x00\x00\x00')])
	return exif_payload(body)


def subjectarea_count_wrap():
	"""parseIFDExif(), tag 0x9214 (SubjectArea): the count that wraps to zero.

	Companion to subjectarea_alloc_dos(). That one uses 0xdfdfdfdf, which is
	large enough that `count * 2` exceeds UINT32_MAX and the `dataSize <=
	UINT32_MAX` guard short-circuits before InBounds() is ever consulted --
	so it does not actually pin the overflow arithmetic it was written for.
	Deleting that guard leaves it passing.

	0x80000000 is the case that does pin it: doubled it is exactly 0x100000000,
	which truncates to 0 in 32 bits. A parser that narrows before checking asks
	InBounds(offset, 0), which reduces to `offset <= len` and is accepted -- and
	then resizes to 2147483648 elements, a 4 GB allocation. The fix promotes to
	64-bit before the multiply, so the guard sees the true 0x100000000 and
	rejects it.
	"""
	body = ifd([entry(0x9214, FMT_SHORT, 0x80000000, b'\x00\x00\x00\x00')])
	return exif_payload(body)


SAMPLES = (
	('poc-rational-oob.jpg', rational_oob),
	('poc-makernote-oob.jpg', makernote_oob),
	('poc-subifd-offset-wrap.jpg', subifd_offset_wrap),
	('poc-lensinfo-offset-wrap.jpg', lensinfo_offset_wrap),
	('poc-first-ifd-underflow.jpg', first_ifd_offset_underflow),
	('poc-subjectarea-alloc-dos.jpg', subjectarea_alloc_dos),
	('poc-subjectarea-count-wrap.jpg', subjectarea_count_wrap),
)


def main():
	outdir = os.path.dirname(os.path.abspath(__file__))
	for name, build in SAMPLES:
		data = jpeg(build())
		path = os.path.join(outdir, name)
		with open(path, 'wb') as fh:
			fh.write(data)
		print("wrote {} ({} bytes)".format(path, len(data)))


if __name__ == '__main__':
	main()
