/*
  TinyEXIF.cpp -- A simple ISO C++ library to parse basic EXIF and XMP
                  information from a JPEG file.

  Copyright (c) 2015-2016 Seacave
  cdc.seacave@gmail.com
  All rights reserved.

  Based on the easyexif library (2013 version)
    https://github.com/mayanklahiri/easyexif
  of Mayank Lahiri (mlahiri@gmail.com).
  
  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions are met:

  -- Redistributions of source code must retain the above copyright notice, 
     this list of conditions and the following disclaimer.
  -- Redistributions in binary form must reproduce the above copyright notice, 
     this list of conditions and the following disclaimer in the documentation 
   and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY EXPRESS 
   OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES 
   OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN 
   NO EVENT SHALL THE FREEBSD PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
   INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
   BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
   OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, 
   EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "TinyEXIF.h"
#include "tinyxml2.h"
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>

#ifdef _MSC_VER
#include <tchar.h>
#else
#include <strings.h>
#include <float.h>
#define _tcsncmp         	strncmp
#define _tcsicmp         	strcasecmp
#define _T
#endif


namespace TinyEXIF {

enum JPEG_MARKERS {
	JM_START = 0xFF,
	JM_SOF0  = 0xC0,
	JM_SOF1  = 0xC1,
	JM_SOF2  = 0xC2,
	JM_SOF3  = 0xC3,
	JM_DHT   = 0xC4,
	JM_SOF5  = 0xC5,
	JM_SOF6  = 0xC6,
	JM_SOF7  = 0xC7,
	JM_JPG   = 0xC8,
	JM_SOF9  = 0xC9,
	JM_SOF10 = 0xCA,
	JM_SOF11 = 0xCB,
	JM_DAC   = 0xCC,
	JM_SOF13 = 0xCD,
	JM_SOF14 = 0xCE,
	JM_SOF15 = 0xCF,
	JM_RST0  = 0xD0,
	JM_RST1  = 0xD1,
	JM_RST2  = 0xD2,
	JM_RST3  = 0xD3,
	JM_RST4  = 0xD4,
	JM_RST5  = 0xD5,
	JM_RST6  = 0xD6,
	JM_RST7  = 0xD7,
	JM_SOI   = 0xD8,
	JM_EOI   = 0xD9,
	JM_SOS   = 0xDA,
	JM_DQT	 = 0xDB,
	JM_DNL	 = 0xDC,
	JM_DRI	 = 0xDD,
	JM_DHP	 = 0xDE,
	JM_EXP	 = 0xDF,
	JM_APP0	 = 0xE0,
	JM_APP1  = 0xE1, // EXIF and XMP
	JM_APP2  = 0xE2,
	JM_APP3  = 0xE3,
	JM_APP4  = 0xE4,
	JM_APP5  = 0xE5,
	JM_APP6  = 0xE6,
	JM_APP7  = 0xE7,
	JM_APP8  = 0xE8,
	JM_APP9  = 0xE9,
	JM_APP10 = 0xEA,
	JM_APP11 = 0xEB,
	JM_APP12 = 0xEC,
	JM_APP13 = 0xED, // IPTC
	JM_APP14 = 0xEE,
	JM_APP15 = 0xEF,
	JM_JPG0	 = 0xF0,
	JM_JPG1	 = 0xF1,
	JM_JPG2	 = 0xF2,
	JM_JPG3	 = 0xF3,
	JM_JPG4	 = 0xF4,
	JM_JPG5	 = 0xF5,
	JM_JPG6	 = 0xF6,
	JM_JPG7	 = 0xF7,
	JM_JPG8	 = 0xF8,
	JM_JPG9	 = 0xF9,
	JM_JPG10 = 0xFA,
	JM_JPG11 = 0xFB,
	JM_JPG12 = 0xFC,
	JM_JPG13 = 0xFD,
	JM_COM   = 0xFE
};


// Parser helper
class EntryParser {
private:
	const uint8_t* buf;
	const unsigned len;
	const unsigned tiff_header_start;
	const bool alignIntel; // byte alignment (defined in EXIF header)
	unsigned offs; // current offset into buffer
	uint16_t tag, format;
	uint32_t length;

public:
	EntryParser(const uint8_t* _buf, unsigned _len, unsigned _tiff_header_start, bool _alignIntel)
		: buf(_buf), len(_len), tiff_header_start(_tiff_header_start), alignIntel(_alignIntel), offs(0) {}

	void Init(unsigned _offs) {
		offs = _offs - 12;
	}

	void ParseTag() {
		offs  += 12;
		tag    = parse16(buf + offs, alignIntel);
		format = parse16(buf + offs + 2, alignIntel);
		length = parse32(buf + offs + 4, alignIntel);
	}

	uint16_t GetTag() const { return tag; }
	uint32_t GetLength() const { return length; }
	uint32_t GetData() const { return parse32(buf + offs + 8, alignIntel); }
	uint32_t GetSubIFD() const { return tiff_header_start + GetData(); }

	bool IsShort() const { return format == 3; }
	bool IsLong() const { return format == 4; }
	bool IsRational() const { return format == 5 || format == 10; }

	bool Fetch(std::string& val) const {
		if (format != 2 || length == 0)
			return false;
		val = parseEXIFString(buf, length, GetData(), tiff_header_start, len, alignIntel);
		return true;
	}
	bool Fetch(uint8_t& val) const {
		if ((format != 1 && format != 2 && format != 6) || length == 0)
			return false;
		val = parse8(buf + offs + 8);
		return true;
	}
	bool Fetch(uint16_t& val) const {
		if (!IsShort() || length == 0)
			return false;
		val = parse16(buf + offs + 8, alignIntel);
		return true;
	}
	bool Fetch(uint16_t& val, uint32_t idx) const {
		if (!IsShort() || length <= idx)
			return false;
		val = parse16(buf + GetSubIFD() + idx*2, alignIntel);
		return true;
	}
	bool Fetch(uint32_t& val) const {
		if (!IsLong() || length == 0)
			return false;
		val = parse32(buf + offs + 8, alignIntel);
		return true;
	}
	bool Fetch(double& val) const {
		if (!IsRational() || length == 0)
			return false;
		val = parseEXIFRational(buf + GetSubIFD(), alignIntel);
		return true;
	}
	bool Fetch(double& val, uint32_t idx) const {
		if (!IsRational() || length <= idx)
			return false;
		val = parseEXIFRational(buf + GetSubIFD() + idx*8, alignIntel);
		return true;
	}

public:
	static uint8_t parse8(const uint8_t* buf) {
		return buf[0];
	}
	static uint16_t parse16(const uint8_t* buf, bool intel) {
		if (intel)
			return ((uint16_t)buf[1]<<8) | buf[0];
		return ((uint16_t)buf[0]<<8) | buf[1];
	}
	static uint32_t parse32(const uint8_t* buf, bool intel) {
		if (intel)
			return ((uint32_t)buf[3]<<24) |
				((uint32_t)buf[2]<<16) |
				((uint32_t)buf[1]<<8)  |
				buf[0];
		return ((uint32_t)buf[0]<<24) |
			((uint32_t)buf[1]<<16) |
			((uint32_t)buf[2]<<8)  |
			buf[3];
	}
	static double parseEXIFRational(const uint8_t* buf, bool intel) {
		const uint32_t denominator = parse32(buf+4, intel);
		if (denominator == 0)
			return 0.0;
		const uint32_t numerator = parse32(buf, intel);
		return (double)(int32_t)numerator/(double)(int32_t)denominator;
	}
	static std::string parseEXIFString(const uint8_t* buf,
		unsigned num_components,
		unsigned data,
		unsigned base,
		unsigned len,
		bool intel)
	{
		std::string value;
		if (num_components <= 4) {
			value.resize(num_components);
			char j = intel ? 0 : 24;
			char j_m = intel ? -8 : 8;
			for (unsigned i=0; i<num_components; ++i, j -= j_m)
				value[i] = data >> j & 0xff;
			if (value[num_components-1] == '\0')
				value.resize(num_components-1);
		} else
		if (base+data+num_components <= len) {
			const char* const sz((const char*)buf+base+data);
			unsigned num(0);
			while (num < num_components && sz[num] != '\0')
				++num;
			value.assign(sz, num);
		}
		return value;
	}
};


// Parse tag as Image IFD
void EXIFInfo::parseIFDImage(EntryParser& parser, unsigned& exif_sub_ifd_offset, unsigned& gps_sub_ifd_offset) {
	switch (parser.GetTag()) {
	case 0x0102:
		// Bits per sample
		parser.Fetch(BitsPerSample);
		break;

	case 0x010e:
		// Image description
		parser.Fetch(ImageDescription);
		break;

	case 0x010f:
		// Camera maker
		parser.Fetch(Make);
		break;

	case 0x0110:
		// Camera model
		parser.Fetch(Model);
		break;

	case 0x0112:
		// Orientation of image
		parser.Fetch(Orientation);
		break;

	case 0x011a:
		// XResolution 
		parser.Fetch(XResolution);
		break;

	case 0x011b:
		// YResolution 
		parser.Fetch(YResolution);
		break;

	case 0x0128:
		// Resolution Unit
		parser.Fetch(ResolutionUnit);
		break;

	case 0x0131:
		// Software used for image
		parser.Fetch(Software);
		break;

	case 0x0132:
		// EXIF/TIFF date/time of image modification
		parser.Fetch(DateTime);
		break;

	case 0x1001:
		// Original Image width
		if (!parser.Fetch(RelatedImageWidth)) {
			uint16_t _RelatedImageWidth;
			if (parser.Fetch(_RelatedImageWidth))
				RelatedImageWidth = _RelatedImageWidth;
		}
		break;

	case 0x1002:
		// Original Image height
		if (!parser.Fetch(RelatedImageHeight)) {
			uint16_t _RelatedImageHeight;
			if (parser.Fetch(_RelatedImageHeight))
				RelatedImageHeight = _RelatedImageHeight;
		}
		break;

	case 0x8298:
		// Copyright information
		parser.Fetch(Copyright);
		break;

	case 0x8769:
		// EXIF SubIFD offset
		exif_sub_ifd_offset = parser.GetSubIFD();
		break;

	case 0x8825:
		// GPS IFS offset
		gps_sub_ifd_offset = parser.GetSubIFD();
		break;

	default:
		// Try to parse as EXIF tag, as some images store them in here
		parseIFDExif(parser);
		break;
	}
}

// Parse tag as Exif IFD
void EXIFInfo::parseIFDExif(EntryParser& parser) {
	switch (parser.GetTag()) {
	case 0x829a:
		// Exposure time in seconds
		parser.Fetch(ExposureTime);
		break;

	case 0x829d:
		// FNumber
		parser.Fetch(FNumber);
		break;

	case 0x8822:
		// Exposure Program
		parser.Fetch(ExposureProgram);
		break;

	case 0x8827:
		// ISO Speed Rating
		parser.Fetch(ISOSpeedRatings);
		break;

	case 0x9003:
		// Original date and time
		parser.Fetch(DateTimeOriginal);
		break;

	case 0x9004:
		// Digitization date and time
		parser.Fetch(DateTimeDigitized);
		break;

	case 0x9201:
		// Shutter speed value
		parser.Fetch(ShutterSpeedValue);
		ShutterSpeedValue = 1.0/exp(ShutterSpeedValue*log(2));
		break;

	case 0x9202:
		// Aperture value
		parser.Fetch(ApertureValue);
		ApertureValue = exp(ApertureValue*log(2)*0.5);
		break;

	case 0x9203:
		// Brightness value
		parser.Fetch(BrightnessValue);
		break;

	case 0x9204:
		// Exposure bias value 
		parser.Fetch(ExposureBiasValue);
		break;

	case 0x9206:
		// Subject distance
		parser.Fetch(SubjectDistance);
		break;

	case 0x9207:
		// Metering mode
		parser.Fetch(MeteringMode);
		break;

	case 0x9208:
		// Light source
		parser.Fetch(LightSource);
		break;

	case 0x9209:
		// Flash info
		parser.Fetch(Flash);
		break;

	case 0x920a:
		// Focal length
		parser.Fetch(FocalLength);
		break;

	case 0x9214:
		// Subject area
		if (parser.IsShort() && parser.GetLength() > 1) {
			SubjectArea.resize(parser.GetLength());
			for (uint32_t i=0; i<parser.GetLength(); ++i)
				parser.Fetch(SubjectArea[i], i);
		}
		break;

	case 0x9291:
		// Fractions of seconds for DateTimeOriginal
		parser.Fetch(SubSecTimeOriginal);
		break;

	case 0xa002:
		// EXIF Image width
		if (!parser.Fetch(ImageWidth)) {
			uint16_t _ImageWidth;
			if (parser.Fetch(_ImageWidth))
				ImageWidth = _ImageWidth;
		}
		break;

	case 0xa003:
		// EXIF Image height
		if (!parser.Fetch(ImageHeight)) {
			uint16_t _ImageHeight;
			if (parser.Fetch(_ImageHeight))
				ImageHeight = _ImageHeight;
		}
		break;

	case 0xa20e:
		// Focal plane X resolution
		parser.Fetch(LensInfo.FocalPlaneXResolution);
		break;

	case 0xa20f:
		// Focal plane Y resolution
		parser.Fetch(LensInfo.FocalPlaneYResolution);
		break;

	case 0xa210:
		// Focal plane resolution unit
		parser.Fetch(LensInfo.FocalPlaneResolutionUnit);
		break;

	case 0xa215:
		// Exposure Index and ISO Speed Rating are often used interchangeably
		if (ISOSpeedRatings == 0) {
			double ExposureIndex;
			if (parser.Fetch(ExposureIndex))
				ISOSpeedRatings = (uint16_t)ExposureIndex;
		}
		break;

	case 0xa404:
		// Digital Zoom Ratio
		parser.Fetch(LensInfo.DigitalZoomRatio);
		break;

	case 0xa405:
		// Focal length in 35mm film
		if (!parser.Fetch(LensInfo.FocalLengthIn35mm)) {
			uint16_t _FocalLengthIn35mm;
			if (parser.Fetch(_FocalLengthIn35mm))
				LensInfo.FocalLengthIn35mm = (double)_FocalLengthIn35mm;
		}
		break;

	case 0xa432:
		// Focal length and FStop.
		if (parser.Fetch(LensInfo.FocalLengthMin, 0))
			if (parser.Fetch(LensInfo.FocalLengthMax, 1))
				if (parser.Fetch(LensInfo.FStopMin, 2))
					parser.Fetch(LensInfo.FStopMax, 3);
		break;

	case 0xa433:
		// Lens make.
		parser.Fetch(LensInfo.Make);
		break;

	case 0xa434:
		// Lens model.
		parser.Fetch(LensInfo.Model);
		break;
	}
}

// Parse tag as GPS IFD
void EXIFInfo::parseIFDGPS(EntryParser& parser) {
	switch (parser.GetTag()) {
	case 1:
		// GPS north or south
		parser.Fetch(GeoLocation.LatComponents.direction);
		break;

	case 2:
		// GPS latitude
		if (parser.IsRational() && parser.GetLength() == 3) {
			parser.Fetch(GeoLocation.LatComponents.degrees, 0);
			parser.Fetch(GeoLocation.LatComponents.minutes, 1);
			parser.Fetch(GeoLocation.LatComponents.seconds, 2);
		}
		break;

	case 3:
		// GPS east or west
		parser.Fetch(GeoLocation.LonComponents.direction);
		break;

	case 4:
		// GPS longitude
		if (parser.IsRational() && parser.GetLength() == 3) {
			parser.Fetch(GeoLocation.LonComponents.degrees, 0);
			parser.Fetch(GeoLocation.LonComponents.minutes, 1);
			parser.Fetch(GeoLocation.LonComponents.seconds, 2);
		}
		break;

	case 5:
		// GPS altitude reference (below or above sea level)
		parser.Fetch((uint8_t&)GeoLocation.AltitudeRef);
		break;

	case 6:
		// GPS altitude
		parser.Fetch(GeoLocation.Altitude);
		break;

	case 7:
		// GPS timestamp
		if (parser.IsRational() && parser.GetLength() == 3) {
			double h,m,s;
			parser.Fetch(h, 0);
			parser.Fetch(m, 1);
			parser.Fetch(s, 2);
			char buffer[256];
			snprintf(buffer, 256, "%g %g %g", h, m, s);
			GeoLocation.GPSTimeStamp = buffer;
		}
		break;

	case 11:
		// Indicates the GPS DOP (data degree of precision)
		parser.Fetch(GeoLocation.GPSDOP);
		break;

	case 18:
		// GPS geodetic survey data
		parser.Fetch(GeoLocation.GPSMapDatum);
		break;

	case 29:
		// GPS date-stamp
		parser.Fetch(GeoLocation.GPSDateStamp);
		break;

	case 30:
		// GPS differential indicates whether differential correction is applied to the GPS receiver
		parser.Fetch(GeoLocation.GPSDifferential);
		break;
	}
}


//
// Locates the JM_APP1 segment and parses it using
// parseFromEXIFSegment() or parseFromXMPSegment()
//
int EXIFInfo::parseFrom(const uint8_t* buf, unsigned len) {
	// Sanity check: all JPEG files start with 0xFFD8 and end with 0xFFD9
	// This check also ensures that the user has supplied a correct value for len.
	if (!buf || len < 16)
		return PARSE_ERROR_NO_EXIF; 
	if (buf[0] != JM_START || buf[1] != JM_SOI)
		return PARSE_EXIF_ERROR_NO_JPEG;
	// not always valid, sometimes 0xFF is added for padding
	//if (buf[len-2] != JM_START || buf[len-1] != JM_EOI)
	//  return PARSE_EXIF_ERROR_NO_JPEG;

	// Scan for JM_APP1 header (bytes 0xFF 0xE1) and parse its length.
	// Exit if both EXIF and XMP sections were parsed.
	enum {
		APP1_NA = 0,
		APP1_EXIF = 1,
		APP1_XMP = 2,
		APP1_ALL = APP1_EXIF|APP1_XMP
	};
	struct APP1S {
		uint32_t val;
		inline APP1S() : val(APP1_NA) {}
		inline operator uint32_t () const { return val; }
		inline operator uint32_t& () { return val; }
        inline int operator () (int code=PARSE_NO_EXIF_XMP) const { return (val==0)?code:((val==3)?PARSE_EXIF_XMP_SUCCESS:((val==1)?PARSE_EXIF_SUCCESS:PARSE_XMP_SUCCESS)); }
	} app1s;
	for (unsigned pos=2; pos<len; ) {
		// find next marker
		uint8_t marker, prev(0);
		do {
			marker = buf[pos++];
			if (marker != JM_START && prev == JM_START)
				break;
			prev = marker;
		} while (pos<len);
		// select marker
		switch (marker) {
		case 0x00:
		case 0x01:
		case JM_RST0:
		case JM_RST1:
		case JM_RST2:
		case JM_RST3:
		case JM_RST4:
		case JM_RST5:
		case JM_RST6:
		case JM_RST7:
		case JM_SOI:
			break;
		case JM_SOS: // start of stream: and we're done
		case JM_EOI: // no data? not good
            return app1s();
		case JM_APP1: {
			const uint16_t section_length(EntryParser::parse16(buf + pos, false));
			int ret;
			switch (ret=parseFromEXIFSegment(buf + pos + 2, section_length - 2)) {
			case PARSE_ERROR_NO_EXIF:
				switch (ret=parseFromXMPSegment(buf + pos + 2, section_length - 2)) {
				case PARSE_ERROR_NO_XMP:
					break;
				case PARSE_XMP_SUCCESS:
					if ((app1s|=APP1_XMP) == APP1_ALL)
						return PARSE_EXIF_XMP_SUCCESS;
					break;
				default:
					return app1s(ret); // some error
				}
				break;
			case PARSE_EXIF_SUCCESS:
				if ((app1s|=APP1_EXIF) == APP1_ALL)
					return PARSE_EXIF_XMP_SUCCESS;
				break;
			default:
				return app1s(ret); // some error
			}
		}
		default: {
			// read section length
			const uint16_t section_length(EntryParser::parse16(buf + pos, false));
			if (pos + section_length > len)
				return app1s(PARSE_EXIF_ERROR_NO_JPEG);
			// skip the section
			pos += section_length;
		}
		}
	}
	return app1s();
}

int EXIFInfo::parseFrom(const std::string& data) {
	return parseFrom((const uint8_t*)data.data(), (unsigned)data.length());
}

//
// Main parsing function for an EXIF segment.
// Do a sanity check by looking for bytes "Exif\0\0".
// The marker has to contain at least the TIFF header, otherwise the
// JM_APP1 data is corrupt. So the minimum length specified here has to be:
//   6 bytes: "Exif\0\0" string
//   2 bytes: TIFF header (either "II" or "MM" string)
//   2 bytes: TIFF magic (short 0x2a00 in Motorola byte order)
//   4 bytes: Offset to first IFD
// =========
//  14 bytes
//
// PARAM: 'buf' start of the EXIF TIFF, which must be the bytes "Exif\0\0".
// PARAM: 'len' length of buffer
//
int EXIFInfo::parseFromEXIFSegment(const uint8_t* buf, unsigned len) {
	unsigned offs = 0;        // current offset into buffer
	if (!buf || len < 6)
		return PARSE_ERROR_NO_EXIF;

	if (!std::equal(buf, buf+6, "Exif\0\0"))
		return PARSE_ERROR_NO_EXIF;
	offs += 6;

	// Now parsing the TIFF header. The first two bytes are either "II" or
	// "MM" for Intel or Motorola byte alignment. Sanity check by parsing
	// the uint16_t that follows, making sure it equals 0x2a. The
	// last 4 bytes are an offset into the first IFD, which are added to 
	// the global offset counter. For this block, we expect the following
	// minimum size:
	//  2 bytes: 'II' or 'MM'
	//  2 bytes: 0x002a
	//  4 bytes: offset to first IDF
	// -----------------------------
	//  8 bytes
	if (offs + 8 > len)
		return PARSE_EXIF_ERROR_CORRUPT;
	bool alignIntel;
	if (buf[offs] == 'I' && buf[offs+1] == 'I')
		alignIntel = true; // 1: Intel byte alignment
	else
	if (buf[offs] == 'M' && buf[offs+1] == 'M')
		alignIntel = false; // 0: Motorola byte alignment
	else
		return PARSE_EXIF_ERROR_UNKNOWN_BYTEALIGN;
	EntryParser parser(buf, len, offs, alignIntel);
	offs += 2;
	if (0x2a != EntryParser::parse16(buf + offs, alignIntel))
		return PARSE_EXIF_ERROR_CORRUPT;
	offs += 2;
	const unsigned first_ifd_offset = EntryParser::parse32(buf + offs, alignIntel);
	offs += first_ifd_offset - 4;
	if (offs >= len)
		return PARSE_EXIF_ERROR_CORRUPT;

	// Now parsing the first Image File Directory (IFD0, for the main image).
	// An IFD consists of a variable number of 12-byte directory entries. The
	// first two bytes of the IFD section contain the number of directory
	// entries in the section. The last 4 bytes of the IFD contain an offset
	// to the next IFD, which means this IFD must contain exactly 6 + 12 * num
	// bytes of data.
	if (offs + 2 > len)
		return PARSE_EXIF_ERROR_CORRUPT;
	int num_entries = EntryParser::parse16(buf + offs, alignIntel);
	if (offs + 6 + 12 * num_entries > len)
		return PARSE_EXIF_ERROR_CORRUPT;
	unsigned exif_sub_ifd_offset = len;
	unsigned gps_sub_ifd_offset  = len;
	parser.Init(offs+2);
	while (--num_entries >= 0) {
		parser.ParseTag();
		parseIFDImage(parser, exif_sub_ifd_offset, gps_sub_ifd_offset);
	}

	// Jump to the EXIF SubIFD if it exists and parse all the information
	// there. Note that it's possible that the EXIF SubIFD doesn't exist.
	// The EXIF SubIFD contains most of the interesting information that a
	// typical user might want.
    
	if (exif_sub_ifd_offset + 4 <= len) {
		offs = exif_sub_ifd_offset;
		int num_entries = EntryParser::parse16(buf + offs, alignIntel);
		if (offs + 6 + 12 * num_entries > len)
			return PARSE_EXIF_ERROR_CORRUPT;
		parser.Init(offs+2);
		while (--num_entries >= 0) {
			parser.ParseTag();
			parseIFDExif(parser);
		}
	}

	// Jump to the GPS SubIFD if it exists and parse all the information
	// there. Note that it's possible that the GPS SubIFD doesn't exist.
	if (gps_sub_ifd_offset + 4 <= len) {
		offs = gps_sub_ifd_offset;
		int num_entries = EntryParser::parse16(buf + offs, alignIntel);
		if (offs + 6 + 12 * num_entries > len)
			return PARSE_EXIF_ERROR_CORRUPT;
		parser.Init(offs+2);
		while (--num_entries >= 0) {
			parser.ParseTag();
			parseIFDGPS(parser);
		}
		GeoLocation.parseCoords();
	}

	return PARSE_EXIF_SUCCESS;
}

//
// Main parsing function for a XMP segment.
// Do a sanity check by looking for bytes "http://ns.adobe.com/xap/1.0/\0".
// So the minimum length specified here has to be:
//  29 bytes: "http://ns.adobe.com/xap/1.0/\0" string
//
// PARAM: 'buf' start of the XMP header, which must be the bytes "http://ns.adobe.com/xap/1.0/\0".
// PARAM: 'len' length of buffer
//
int EXIFInfo::parseFromXMPSegment(const uint8_t* buf, unsigned len) {
	struct Tools {
		static const char* strrnstr(const char* haystack, const char* needle, size_t len) {
			const size_t needle_len(strlen(needle));
			if (0 == needle_len)
				return haystack;
			if (len <= needle_len)
				return NULL;
			for (size_t i=len-needle_len; i-- > 0; ) {
				if (haystack[0] == needle[0] &&
					0 == _tcsncmp(haystack, needle, needle_len))
					return haystack;
				haystack++;
			}
			return NULL;
		}
	};

	unsigned offs = 0; // current offset into buffer
	if (!buf || len < 29)
		return PARSE_ERROR_NO_XMP;

	if (!std::equal(buf, buf+29, "http://ns.adobe.com/xap/1.0/\0"))
		return PARSE_ERROR_NO_XMP;
	offs += 29;
	if (offs >= len)
		return PARSE_XMP_ERROR_CORRUPT;
	len -= offs;

	// Skip xpacket end section so that tinyxml2 lib parses the section correctly.
	const char* const strXMP((const char*)(buf + offs)), *strEnd;
	if ((strEnd=Tools::strrnstr(strXMP, "<?xpacket end=", len)) != NULL)
		len = (unsigned)(strEnd - strXMP);

	// Now try parsing the XML packet.
	tinyxml2::XMLDocument doc;
	const tinyxml2::XMLElement* document;
	if (doc.Parse(strXMP, len) != tinyxml2::XML_SUCCESS ||
		((document=doc.FirstChildElement(_T("x:xmpmeta"))) == NULL && (document=doc.FirstChildElement(_T("xmp:xmpmeta"))) == NULL) ||
		(document=document->FirstChildElement(_T("rdf:RDF"))) == NULL ||
		(document=document->FirstChildElement(_T("rdf:Description"))) == NULL)
		return PARSE_ERROR_NO_XMP;

	// Now try parsing the XMP content for projection type.
	{
	const tinyxml2::XMLElement* const element(document->FirstChildElement(_T("GPano:ProjectionType")));
	if (element != NULL) {
		const char* const szProjectionType(element->GetText());
		if (szProjectionType != NULL) {
			if (0 == _tcsicmp(szProjectionType, _T("perspective")))
				ProjectionType = 1;
			else if (0 == _tcsicmp(szProjectionType, _T("equirectangular")) ||
					 0 == _tcsicmp(szProjectionType, _T("spherical")))
				ProjectionType = 2;
		}
	}
	}

	// Now try parsing the XMP content for DJI info.
	document->QueryDoubleAttribute("drone-dji:AbsoluteAltitude", &GeoLocation.Altitude);
	document->QueryDoubleAttribute("drone-dji:RelativeAltitude", &GeoLocation.RelativeAltitude);
	document->QueryDoubleAttribute("drone-dji:FlightRollDegree", &GeoLocation.RollDegree);
	document->QueryDoubleAttribute("drone-dji:FlightPitchDegree", &GeoLocation.PitchDegree);
	document->QueryDoubleAttribute("drone-dji:FlightYawDegree", &GeoLocation.YawDegree);

	return PARSE_XMP_SUCCESS;
}


void EXIFInfo::Geolocation_t::parseCoords() {
	// convert GPS latitude
	if (LatComponents.degrees != DBL_MAX ||
		LatComponents.minutes != 0 ||
		LatComponents.seconds != 0) {
		Latitude =
			LatComponents.degrees +
			LatComponents.minutes / 60 +
			LatComponents.seconds / 3600;
		if ('S' == LatComponents.direction)
			Latitude = -Latitude;
	}
	// convert GPS longitude
	if (LonComponents.degrees != DBL_MAX ||
		LonComponents.minutes != 0 ||
		LonComponents.seconds != 0) {
		Longitude =
			LonComponents.degrees +
			LonComponents.minutes / 60 +
			LonComponents.seconds / 3600;
		if ('W' == LonComponents.direction)
			Longitude = -Longitude;
	}
	// convert GPS altitude
	if (hasAltitude() &&
		AltitudeRef == 1) {
		Altitude = -Altitude;
	}
}

bool EXIFInfo::Geolocation_t::hasLatLon() const {
	return Latitude != DBL_MAX && Longitude != DBL_MAX;
}
bool EXIFInfo::Geolocation_t::hasAltitude() const {
	return Altitude != DBL_MAX;
}
bool EXIFInfo::Geolocation_t::hasRelativeAltitude() const {
	return RelativeAltitude != DBL_MAX;
}
bool EXIFInfo::Geolocation_t::hasOrientation() const {
	return RollDegree != DBL_MAX && PitchDegree != DBL_MAX && YawDegree != DBL_MAX;
}


void EXIFInfo::clear() {
	// Strings
	ImageDescription  = "";
	Make              = "";
	Model             = "";
	Software          = "";
	DateTime          = "";
	DateTimeOriginal  = "";
	DateTimeDigitized = "";
	SubSecTimeOriginal= "";
	Copyright         = "";

	// Shorts / unsigned / double
	ImageWidth        = 0;
	ImageHeight       = 0;
	RelatedImageWidth = 0;
	RelatedImageHeight= 0;
	Orientation       = 0;
	BitsPerSample     = 0;
	ExposureTime      = 0;
	FNumber           = 0;
	ExposureProgram   = 0;
	ISOSpeedRatings   = 0;
	ShutterSpeedValue = 0;
	ApertureValue     = 0;
	BrightnessValue   = 0;
	ExposureBiasValue = 0;
	SubjectDistance   = 0;
	FocalLength       = 0;
	Flash             = 0;
	MeteringMode      = 0;
	LightSource       = 0;
	ProjectionType    = 0;
	SubjectArea.clear();

	// LensInfo
	LensInfo.FocalLengthMax = 0;
	LensInfo.FocalLengthMin = 0;
	LensInfo.FStopMax = 0;
	LensInfo.FStopMin = 0;
	LensInfo.DigitalZoomRatio = 0;
	LensInfo.FocalLengthIn35mm = 0;
	LensInfo.FocalPlaneXResolution = 0;
	LensInfo.FocalPlaneYResolution = 0;
	LensInfo.FocalPlaneResolutionUnit = 0;
	LensInfo.Make = "";
	LensInfo.Model = "";

	// Geolocation
	GeoLocation.Latitude                = DBL_MAX;
	GeoLocation.Longitude               = DBL_MAX;
	GeoLocation.Altitude                = DBL_MAX;
	GeoLocation.AltitudeRef             = 0;
	GeoLocation.RelativeAltitude        = DBL_MAX;
	GeoLocation.RollDegree              = DBL_MAX;
	GeoLocation.PitchDegree             = DBL_MAX;
	GeoLocation.YawDegree               = DBL_MAX;
	GeoLocation.GPSDOP                  = 0;
	GeoLocation.GPSDifferential         = 0;
	GeoLocation.GPSMapDatum             = "";
	GeoLocation.GPSTimeStamp            = "";
	GeoLocation.GPSDateStamp            = "";
	GeoLocation.LatComponents.degrees   = DBL_MAX;
	GeoLocation.LatComponents.minutes   = 0;
	GeoLocation.LatComponents.seconds   = 0;
	GeoLocation.LatComponents.direction = 0;
	GeoLocation.LonComponents.degrees   = DBL_MAX;
	GeoLocation.LonComponents.minutes   = 0;
	GeoLocation.LonComponents.seconds   = 0;
	GeoLocation.LonComponents.direction = 0;
}

} // namespace TinyEXIF
