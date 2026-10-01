#!/usr/bin/env python3
"""Generator for the crafted regression samples in Samples/fuzz/.

These are not real photos: each one is a minimal JPEG whose APP1 EXIF and/or
XMP segments are built to pin one specific parser behavior. The poc-* samples
hit an out-of-bounds read, undefined-behavior conversion or non-finite value
that the parser used to perform or store; the others cover corners of the
format that real files reach (a GPS without a fix, mixed byte orders).
They are checked in together with this generator so that the bytes stay
reviewable instead of being an opaque blob; re-run it to regenerate them:

    python3 Samples/fuzz/make_samples.py

Every sample is expected to parse without any sanitizer report and without
inventing fields. See Samples/fuzz/<name>.expected for the baseline output.

One sample is not generated here: poc-y1bit-rational-oob.jpg is the original
190-byte input from y1bit's report of the EntryParser::Fetch(double&)
out-of-bounds read fixed in 1.1.0, checked in byte for byte as received.

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
FMT_SRATIONAL = 10
FMT_FLOAT = 11

XMP_ID = b'http://ns.adobe.com/xap/1.0/\x00'  # APP1 XMP identifier


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


def jpeg(*payloads):
	"""SOI + one APP1 per payload + EOI -- the smallest container the parser accepts."""
	return (b'\xff\xd8' +
		b''.join(b'\xff\xe1' + struct.pack('>H', len(p) + 2) + p for p in payloads) +
		b'\xff\xd9')


def rational(numerator, denominator=1):
	"""The 8 raw bytes of one RATIONAL."""
	return struct.pack('>II', numerator, denominator)


def ifd_at(offset, fields):
	"""An IFD placed at TIFF `offset`, followed by the values too large for an entry.

	`fields` are (tag, format, count, raw value bytes); a value of up to 4 bytes is
	stored in its entry, a longer one after the IFD with the entry pointing to it.
	Unlike ifd(), this ends the IFD with a (zero) next-IFD offset, as real files do.
	"""
	data_offset = offset + 2 + IFD_ENTRY_SIZE * len(fields) + 4
	entries, data = [], b''
	for tag, fmt, count, value in fields:
		if len(value) <= 4:
			entries.append(entry(tag, fmt, count, value.ljust(4, b'\x00')))
		else:
			entries.append(entry(tag, fmt, count, struct.pack('>I', data_offset + len(data))))
			data += value
	return ifd(entries) + struct.pack('>I', 0) + data


def exif_with_subifd(pointer_tag, fields):
	"""EXIF payload whose IFD0 holds only a pointer (`pointer_tag`) to one sub-IFD."""
	ifd0_offset = len(TIFF_HEADER) + 4
	sub_offset = ifd0_offset + 2 + IFD_ENTRY_SIZE + 4
	ifd0 = ifd_at(ifd0_offset, [(pointer_tag, FMT_LONG, 1, struct.pack('>I', sub_offset))])
	return exif_payload(ifd0 + ifd_at(sub_offset, fields))


def xmp(attributes):
	"""XMP payload: one rdf:Description carrying `attributes`, an XML attribute string."""
	return XMP_ID + (
		'<x:xmpmeta xmlns:x="adobe:ns:meta/">'
		'<rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#">'
		'<rdf:Description' + attributes + '/>'
		'</rdf:RDF></x:xmpmeta>'
	).encode('ascii')


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


def exposure_index(fmt, numerator):
	"""parseIFDExif(), tag 0xa215 (ExposureIndex): a double cast to uint16_t unchecked.

	ExposureIndex fills ISOSpeedRatings, a uint16_t, from a (S)RATIONAL; converting
	a double the target type can not represent is undefined behavior, which UBSan
	reports as float-cast-overflow. The rational is stored right after the IFD.
	"""
	value_offset = len(TIFF_HEADER) + 4 + len(ifd([b'\x00' * IFD_ENTRY_SIZE]))
	body = ifd([entry(0xa215, fmt, 1, struct.pack('>I', value_offset))])
	return exif_payload(body + struct.pack('>iI' if fmt == FMT_SRATIONAL else '>II', numerator, 1))


def exposure_index_overflow():
	"""ExposureIndex 100000, above uint16_t: clamped to 65535, as EXIF records ISO."""
	return exposure_index(FMT_RATIONAL, 100000)


def exposure_index_negative():
	"""ExposureIndex -1, below uint16_t: not a valid index, so ISOSpeedRatings stays absent."""
	return exposure_index(FMT_SRATIONAL, -1)


def apex_overflow():
	"""parseIFDExif(), tags 0x9201/0x9202: APEX conversions overflowing to inf.

	ShutterSpeedValue and ApertureValue are base-2 logarithms converted with exp():
	an SRATIONAL shutter speed of INT32_MIN makes 1/exp() divide by an underflowed
	zero, and a RATIONAL aperture of UINT32_MAX overflows exp(); both used to be
	stored as +inf. The two rationals are stored right after the IFD.
	"""
	values_offset = len(TIFF_HEADER) + 4 + len(ifd([b'\x00' * IFD_ENTRY_SIZE] * 2))
	body = ifd([
		entry(0x9201, FMT_SRATIONAL, 1, struct.pack('>I', values_offset)),
		entry(0x9202, FMT_RATIONAL, 1, struct.pack('>I', values_offset + 8)),
	])
	return exif_payload(body + struct.pack('>iI', -2**31, 1) + struct.pack('>II', 2**32 - 1, 1))


def makernote_float_nonfinite():
	"""parseIFDMakerNote(): DJI FLOAT entries carrying NaN and infinity bit patterns.

	A FLOAT is raw IEEE 754 bits, so a file can encode NaN or inf directly; they used
	to be stored in the GeoLocation speed and orientation fields, which then read back
	as present through their DBL_MAX sentinels. Only SpeedZ, Yaw and Roll are finite.
	"""
	floats = (
		(3, 0x7fc00000),  # SpeedX: NaN
		(4, 0x7f800000),  # SpeedY: +inf
		(5, 0x3f800000),  # SpeedZ: 1.0
		(9, 0xff800000),  # Pitch: -inf
		(10, 0x40000000), # Yaw: 2.0
		(11, 0x40400000), # Roll: 3.0
	)
	note = ifd([entry(1, FMT_ASCII, 4, b'DJI\x00')] +
		[entry(tag, FMT_FLOAT, 1, struct.pack('>I', bits)) for tag, bits in floats])
	note_offset = len(TIFF_HEADER) + 4 + len(ifd([b'\x00' * IFD_ENTRY_SIZE] * 2))
	body = ifd([
		entry(0x010f, FMT_ASCII, 4, b'DJI\x00'),
		entry(0x927c, FMT_UNDEFINED, len(note), struct.pack('>I', note_offset)),
	])
	return exif_payload(body + note)


def xmp_nonfinite():
	"""parseFromXMPSegmentXML(): XMP numbers that are not finite.

	XMP numbers are text, and strtod (like the sscanf behind tinyxml2's
	QueryDoubleAttribute) accepts "nan", "inf" and out-of-range literals, while a
	rational with a zero denominator divides to inf or NaN. Every one of them used to
	be stored and read back as present through the DBL_MAX sentinels. The one valid
	value, tiff:YResolution, is a well-formed rational that must still parse: 144/2
	is 72, where the old sscanf read stopped at the slash and returned 144.
	"""
	return xmp(
		' rdf:about="DJI Meta Data"'
		' xmlns:drone-dji="http://www.dji.com/drone-dji/1.0/"'
		' xmlns:tiff="http://ns.adobe.com/tiff/1.0/"'
		' xmlns:GPano="http://ns.google.com/photos/1.0/panorama/"'
		' drone-dji:AbsoluteAltitude="1/0"'
		' drone-dji:RelativeAltitude="0/0"'
		' drone-dji:GimbalRollDegree="nan"'
		' drone-dji:GimbalPitchDegree="-inf"'
		' drone-dji:GimbalYawDegree="1e999"'
		' drone-dji:CalibratedFocalLength="inf"'
		' drone-dji:DewarpData="2026-09-26;1,2,3,4,nan,0.1,0.2,0.3,0.4"'
		' tiff:XResolution="inf" tiff:YResolution="144/2" tiff:ResolutionUnit="2"'
		' GPano:PosePitchDegrees="nan" GPano:PoseRollDegrees="1/0"'
	)


# GPS IFD tags (EXIF 2.3, 4.6.6)
GPS_LATITUDE_REF, GPS_LATITUDE = 1, 2
GPS_LONGITUDE_REF, GPS_LONGITUDE = 3, 4
GPS_ALTITUDE_REF, GPS_ALTITUDE = 5, 6
GPS_STATUS = 9
GPS_MAP_DATUM = 18
GPS_IFD_POINTER = 0x8825
EXIF_IFD_POINTER = 0x8769


def gps_at_origin(status, altitude_ref):
	"""GPS fields of a receiver at 0/0/0 on the south/west side, like a camera
	with no fix writes them (DJI Osmo 360), with the given GPSStatus."""
	zero = rational(0) * 3
	return [
		(GPS_LATITUDE_REF, FMT_ASCII, 2, b'S\x00'),
		(GPS_LATITUDE, FMT_RATIONAL, 3, zero),
		(GPS_LONGITUDE_REF, FMT_ASCII, 2, b'W\x00'),
		(GPS_LONGITUDE, FMT_RATIONAL, 3, zero),
		(GPS_ALTITUDE_REF, 1, 1, bytes([altitude_ref])),
		(GPS_ALTITUDE, FMT_RATIONAL, 1, rational(0)),
		(GPS_STATUS, FMT_ASCII, 2, status + b'\x00'),
		(GPS_MAP_DATUM, FMT_ASCII, 7, b'WGS-84\x00'),
	]


def gps_void():
	"""GPSStatus 'V': the receiver had no fix, so its zero position is not one.

	Cameras without a fix still write the position tags, all zeros, and parsing
	them would place the image at 0N 0E. With the measurement void they must
	come back absent, along with the DJI XMP AbsoluteAltitude, which DJI marks
	invalid through drone-dji:GpsStatus the same way; the map datum and the
	barometric RelativeAltitude do not depend on a fix and stay.
	"""
	return (
		exif_with_subifd(GPS_IFD_POINTER, gps_at_origin(b'V', 0)),
		xmp(' rdf:about="DJI Meta Data"'
			' xmlns:drone-dji="http://www.dji.com/drone-dji/1.0/"'
			' drone-dji:GpsStatus="Invalid"'
			' drone-dji:AbsoluteAltitude="+0.000"'
			' drone-dji:RelativeAltitude="+1.500"'),
	)


def gps_signed_zero():
	"""GPSStatus 'A' at 0/0/0 with the south, west and below-sea-level refs.

	Negating a zero gives IEEE 754 -0.0, which prints as "-0"; a position on the
	equator, the prime meridian or at sea level must read back as plain 0.
	"""
	return exif_with_subifd(GPS_IFD_POINTER, gps_at_origin(b'A', 1))


def dji_speed_max_aperture():
	"""DJI XMP flight speed and the EXIF MaxApertureValue, both parsed since 1.2.0.

	MaxApertureValue is APEX like ApertureValue: 1.85 is f/1.9 (2^(1.85/2)).
	The speeds come from the XMP only, as cameras without the DJI MakerNote
	(e.g. the Osmo 360) write them nowhere else.
	"""
	return (
		exif_with_subifd(EXIF_IFD_POINTER, [(0x9205, FMT_RATIONAL, 1, rational(185, 100))]),
		xmp(' rdf:about="DJI Meta Data"'
			' xmlns:drone-dji="http://www.dji.com/drone-dji/1.0/"'
			' drone-dji:FlightXSpeed="+1.50"'
			' drone-dji:FlightYSpeed="-2.25"'
			' drone-dji:FlightZSpeed="0.5"'),
	)


def dji_makernote_little_endian():
	"""A little-endian DJI MakerNote inside Motorola (big-endian) EXIF.

	DJI writes its MakerNote little-endian; a tool that rewrites the EXIF in
	Motorola byte order copies that opaque blob unchanged (Samples/dji_phantom4_2
	is such a file). Read in the EXIF's byte order, the entry count came out as
	garbage and the whole MakerNote, speeds and camera angles, was skipped.
	"""
	floats = ((3, 1.5), (4, -2.0), (5, 0.25), (9, -45.0), (10, 90.0), (11, 1.0))
	note = struct.pack('<H', 1 + len(floats)) + struct.pack('<HHI', 1, FMT_ASCII, 4) + b'DJI\x00'
	note += b''.join(struct.pack('<HHIf', tag, FMT_FLOAT, 1, value) for tag, value in floats)
	return exif_payload(ifd_at(len(TIFF_HEADER) + 4, [
		(0x010f, FMT_ASCII, 4, b'DJI\x00'),
		(0x927c, FMT_UNDEFINED, len(note), note),
	]))


SAMPLES = (
	('poc-rational-oob.jpg', rational_oob),
	('poc-makernote-oob.jpg', makernote_oob),
	('poc-subifd-offset-wrap.jpg', subifd_offset_wrap),
	('poc-lensinfo-offset-wrap.jpg', lensinfo_offset_wrap),
	('poc-first-ifd-underflow.jpg', first_ifd_offset_underflow),
	('poc-subjectarea-alloc-dos.jpg', subjectarea_alloc_dos),
	('poc-subjectarea-count-wrap.jpg', subjectarea_count_wrap),
	('poc-exposureindex-overflow.jpg', exposure_index_overflow),
	('poc-exposureindex-negative.jpg', exposure_index_negative),
	('poc-apex-overflow.jpg', apex_overflow),
	('poc-makernote-float-nonfinite.jpg', makernote_float_nonfinite),
	('poc-xmp-nonfinite.jpg', xmp_nonfinite),
	('gps-void.jpg', gps_void),
	('gps-signed-zero.jpg', gps_signed_zero),
	('dji-speed-max-aperture.jpg', dji_speed_max_aperture),
	('dji-makernote-little-endian.jpg', dji_makernote_little_endian),
)


def main():
	outdir = os.path.dirname(os.path.abspath(__file__))
	for name, build in SAMPLES:
		# a builder returns one APP1 payload, or a tuple of them (e.g. EXIF + XMP)
		payloads = build()
		data = jpeg(*payloads) if isinstance(payloads, tuple) else jpeg(payloads)
		path = os.path.join(outdir, name)
		with open(path, 'wb') as fh:
			fh.write(data)
		print("wrote {} ({} bytes)".format(path, len(data)))


if __name__ == '__main__':
	main()
