# TinyEXIF: Tiny ISO-compliant C++ EXIF and XMP parsing library for JPEG

## Introduction

TinyEXIF is a tiny, lightweight C++ library for parsing the metadata existing inside JPEG files. No third party dependencies are needed to parse EXIF data, however for accesing XMP data the [TinyXML2](https://github.com/leethomason/tinyxml2) library is needed. TinyEXIF is easy to use, simply copy the two source files in you project and pass the JPEG data to EXIFInfo class. Currently common information like the camera make/model, original resolution, timestamp, focal length, lens info, F-stop/exposure time, GPS information, etc, embedded in the EXIF/XMP metadata are fetched. It is easy though to extend it and add any missing or new EXIF/XMP fields.

## Usage example

```
#include "TinyEXIF.h"
#include <iostream> // std::cout
#include <fstream>  // std::ifstream
#include <vector>   // std::vector

int main(int argc, const char** argv) {
	if (argc != 2) {
		std::cout << "Usage: TinyEXIF <image_file>" << std::endl;
		return -1;
	}

	// read entire image file
	std::ifstream file(argv[1], std::ifstream::in|std::ifstream::binary);
	file.seekg(0,std::ios::end);
	std::streampos length = file.tellg();
	file.seekg(0,std::ios::beg);
	std::vector<uint8_t> data(length);
	file.read((char*)data.data(), length);

	// parse image EXIF and XMP metadata
	TinyEXIF::EXIFInfo imageEXIF;
	if (imageEXIF.parseFrom(data.data(), length) == TinyEXIF::PARSE_EXIF_SUCCESS)
		std::cout
			<< "Image Description " << imageEXIF.ImageDescription << "\n"
			<< "Image Resolution " << imageEXIF.ImageWidth << "x" << imageEXIF.ImageHeight << " pixels\n"
			<< "Camera Model " << imageEXIF.Make << " - " << imageEXIF.Model << "\n"
			<< "Focal Length " << imageEXIF.FocalLength << " mm" << std::endl;
	return 0;
}
```
See `main.cpp` for more details.
