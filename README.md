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

## Absent tags vs tags that are legitimately zero

All data fields are zero-initialised, so a value of `0` on its own does not say whether the tag
was missing from the file or whether the camera really wrote a `0`. `HasField()` answers that,
and `GetFields()` lists everything that was found:

```
	TinyEXIF::EXIFInfo imageEXIF(istream);

	// ISOSpeedRatings == 0 alone is ambiguous, this is not
	if (imageEXIF.HasField(TinyEXIF::FIELD_ID_ISOSpeedRatings))
		std::cout << "ISO " << imageEXIF.ISOSpeedRatings << "\n";
	else
		std::cout << "ISO not recorded by the camera\n";

	// list every tag that was present
	for (TinyEXIF::FieldID id: imageEXIF.GetFields())
		std::cout << TinyEXIF::FieldName(id) << "\n";
```

There is one `FieldID` enumerator per data field, named `FIELD_ID_` followed by the path of the
member it fills, e.g. `FIELD_ID_GeoLocation_Altitude` for `GeoLocation.Altitude`; `FieldName()`
returns that path as a string. A field counts as present when the tag carrying it was parsed
successfully, from EXIF or from XMP; a tag that is present but malformed does not count.
Existing fields, sentinel values (`DBL_MAX`, `UINT32_MAX`) and `hasXxx()` accessors are
unchanged, so this is purely additive.

Run the demo with `TinyEXIFdemo <image_file> --fields` to print the list for a file.

This API was added in 1.1.0 and can be feature-gated with the `TINYEXIF_VERSION` macro:

```
	#if TINYEXIF_VERSION >= 10100
	// TinyEXIF::EXIFInfo::HasField() is available
	#endif
```

## License

MIT [License](https://github.com/cdcseacave/TinyEXIF/blob/master/LICENSE)

Copyright (c) 2025 cdcseacave

## Acknowledgments

Forked from [easyexif](https://github.com/mayanklahiri/easyexif) library (2013 version) of Mayank Lahiri (mlahiri@gmail.com); see [LICENSE.easyexif](https://github.com/cdcseacave/TinyEXIF/blob/master/LICENSE.easyexif) for its terms.