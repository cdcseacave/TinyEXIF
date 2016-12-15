/*
  TinyEXIF.h -- A simple ISO C++ library to parse basic EXIF and XMP
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

#ifndef __TINYEXIF_H__
#define __TINYEXIF_H__

#include <string>

namespace TinyEXIF {

enum ErrorCode {
	PARSE_EXIF_SUCCESS                 = 0, // Parse was successful
	PARSE_EXIF_ERROR_NO_JPEG           = 1, // No JPEG markers found in buffer, possibly invalid JPEG file
	PARSE_EXIF_ERROR_NO_EXIF           = 2, // No EXIF header found in JPEG file
	PARSE_EXIF_ERROR_NO_XMP            = 3, // No XMP header found in JPEG file
	PARSE_EXIF_ERROR_UNKNOWN_BYTEALIGN = 4, // Byte alignment specified in EXIF file was unknown (not Motorola or Intel)
	PARSE_EXIF_ERROR_CORRUPT           = 5, // EXIF header was found, but data was corrupted
};

// 
// Class responsible for storing and parsing EXIF information from a JPEG blob
//
class EXIFInfo {
public:
	EXIFInfo() { clear(); }

	// Parsing function for an entire JPEG image buffer.
	//
	// PARAM 'data': A pointer to a JPEG image.
	// PARAM 'length': The length of the JPEG image.
	// RETURN:  PARSE_EXIF_SUCCESS (0) on success with 'result' filled out
	//          error code otherwise, as defined by the PARSE_EXIF_ERROR_* macros
	int parseFrom(const uint8_t* data, unsigned length);
	int parseFrom(const std::string &data);

	// Parsing function for an EXIF segment. This is used internally by parseFrom()
	// but can be called for special cases where only the EXIF section is 
	// available (i.e., a blob starting with the bytes "Exif\0\0").
	int parseFromEXIFSegment(const uint8_t* buf, unsigned len);

	// Parsing function for an XMP segment. This is used internally by parseFrom()
	// but can be called for special cases where only the XMP section is 
	// available (i.e., a blob starting with the bytes "http://ns.adobe.com/xap/1.0/\0").
	int parseFromXMPSegment(const uint8_t* buf, unsigned len);

	// Set all data members to default values.
	void clear();

	// Return true if the memory alignment is Intel format.
	bool alignIntel() const { return this->ByteAlign != 0; }

	// Data fields filled out by parseFrom()
	int8_t ByteAlign;                   // 0 = Motorola byte alignment, 1 = Intel 
	std::string ImageDescription;       // Image description
	std::string Make;                   // Camera manufacturer's name
	std::string Model;                  // Camera model
	uint16_t Orientation;               // Image orientation, start of data corresponds to
									    // 0: unspecified in EXIF data
									    // 1: upper left of image
									    // 3: lower right of image
									    // 6: upper right of image
									    // 8: lower left of image
									    // 9: undefined
	double XResolution;                 // Number of pixels per ResolutionUnit in the ImageWidth direction
	double YResolution;                 // Number of pixels per ResolutionUnit in the ImageLength direction
	uint16_t ResolutionUnit;            // Unit of measurement for XResolution and YResolution
									    // 1: no absolute unit of measurement. Used for images that may have a non-square aspect ratio, but no meaningful absolute dimensions
									    // 2: inch
									    // 3: centimeter
	uint16_t BitsPerSample;             // Number of bits per component
	std::string Software;               // Software used
	std::string DateTime;               // File change date and time
	std::string DateTimeOriginal;       // Original file date and time (may not exist)
	std::string DateTimeDigitized;      // Digitization date and time (may not exist)
	std::string SubSecTimeOriginal;     // Sub-second time that original picture was taken
	std::string Copyright;              // File copyright information
	double ExposureTime;                // Exposure time in seconds
	double FNumber;                     // F/stop
	uint16_t ISOSpeedRatings;           // ISO speed
	double ShutterSpeedValue;           // Shutter speed (reciprocal of exposure time)
	double ExposureBiasValue;           // Exposure bias value in EV
	double SubjectDistance;             // Distance to focus point in meters
	double FocalLength;                 // Focal length of lens in millimeters
	int8_t Flash;                       // 0 = no flash, 1 = flash used
	uint16_t MeteringMode;              // Metering mode
									    // 1: average
									    // 2: center weighted average
									    // 3: spot
									    // 4: multi-spot
									    // 5: multi-segment
	uint32_t ImageWidth;                // Image width reported in EXIF data
	uint32_t ImageHeight;               // Image height reported in EXIF data
	struct LensInfo_t {                 // Lens information
		double FStopMin;                // Min aperture (f-stop)
		double FStopMax;                // Max aperture (f-stop)
		double FocalLengthMin;          // Min focal length (mm)
		double FocalLengthMax;          // Max focal length (mm)
		double FocalLengthIn35mm;       // Focal length in 35mm film
		double FocalPlaneXResolution;   // Indicates the number of pixels in the image width (X) direction per FocalPlaneResolutionUnit on the camera focal plane (may not exist)
		double FocalPlaneYResolution;   // Indicates the number of pixels in the image width (Y) direction per FocalPlaneResolutionUnit on the camera focal plane (may not exist)
		uint16_t FocalPlaneResolutionUnit;// Indicates the unit for measuring FocalPlaneXResolution and FocalPlaneYResolution (may not exist)
										// 0: unspecified in EXIF data
										// 1: no absolute unit of measurement
										// 2: inch
										// 3: centimeter
		std::string Make;               // Lens manufacturer
		std::string Model;              // Lens model
	} LensInfo;
	struct Geolocation_t {              // GPS information embedded in file
		double Latitude;                // Image latitude expressed as decimal
		double Longitude;               // Image longitude expressed as decimal
		double Altitude;                // Altitude in meters, relative to sea level
		int8_t AltitudeRef;             // 0 = above sea level, -1 = below sea level
		double RelativeAltitude;        // Relative altitude in meters
		double RollDegree;              // Flight roll in degrees
		double PitchDegree;             // Flight pitch in degrees
		double YawDegree;               // Flight yaw in degrees
		double GPSDOP;                  // Indicates the GPS DOP (data degree of precision)
		uint16_t GPSDifferential;       // Indicates whether differential correction is applied to the GPS receiver (may not exist)
										// 0: measurement without differential correction
										// 1: differential correction applied 
		std::string GPSMapDatum;        // Indicates the geodetic survey data (may not exist)
		std::string GPSTimeStamp;       // Indicates the time as UTC (Coordinated Universal Time) (may not exist)
		std::string GPSDateStamp;       // A character string recording date and time information relative to UTC (Coordinated Universal Time) YYYY:MM:DD (may not exist)
		struct Coord_t {
			double degrees;
			double minutes;
			double seconds;
			int8_t direction;
		} LatComponents, LonComponents; // Latitude/Longitude expressed in deg/min/sec
		void parseCoords();             // Convert Latitude/Longitude from deg/min/sec to decimal
		bool hasLatLon() const;         // Return true if (lat,lon) is available
		bool hasAltitude() const;       // Return true if (alt) is available
		bool hasRelativeAltitude()const;// Return true if (rel_alt) is available
		bool hasOrientation() const;    // Return true if (roll,yaw,pitch) is available
	} GeoLocation;
};

} // namespace TinyEXIF

#endif // __TINYEXIF_H__
