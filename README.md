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

	// open a stream to read just the necessary parts of the image file
	std::ifstream istream(argv[1], std::ifstream::binary);

	// parse image EXIF and XMP metadata
	TinyEXIF::EXIFInfo imageEXIF(istream);
	if (imageEXIF.Fields)
		std::cout
			<< "Image Description " << imageEXIF.ImageDescription << "\n"
			<< "Image Resolution " << imageEXIF.ImageWidth << "x" << imageEXIF.ImageHeight << " pixels\n"
			<< "Camera Model " << imageEXIF.Make << " - " << imageEXIF.Model << "\n"
			<< "Focal Length " << imageEXIF.FocalLength << " mm" << std::endl;
	return 0;
}
```
See `main.cpp` for more details.

## License

MIT [License](https://github.com/cdcseacave/TinyEXIF/blob/master/LICENSE)

Copyright (c) 2025 cdcseacave

## Acknowledgments

Inspired by [easyexif](https://github.com/mayanklahiri/easyexif) library (2013 version) of Mayank Lahiri (mlahiri@gmail.com).