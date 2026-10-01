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
That includes any floating point value that is NaN or infinite, or converts to one, so every
floating point field holds a finite number or is absent.
Existing fields, sentinel values (`DBL_MAX`, `UINT32_MAX`) and `hasXxx()` accessors are
unchanged, so this is purely additive.

Run the demo with `TinyEXIFdemo <image_file> --fields` to print the list for a file.

This API was added in 1.1.0 and can be feature-gated with the `TINYEXIF_VERSION` macro:

```
	#if TINYEXIF_VERSION >= 10100
	// TinyEXIF::EXIFInfo::HasField() is available
	#endif
```

## Images stored after the main one

A JPEG can carry more images after its own, listed by a Multi-Picture Format (MPF, CIPA DC-007)
index: the preview of a large panorama, the second view of a stereo (`.MPO`) image, an HDR gain
map. `MPImages` lists them in the index order, with the type, offset and length of each.
The first entry is normally the image itself. TinyEXIF only locates them and never reads them,
so their `Offset` and `Length` come straight from the file and must be checked against its
size before use:

```
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	const uint64_t fileSize = (uint64_t)file.tellg();
	file.seekg(0);
	TinyEXIF::EXIFInfo imageEXIF(file);
	for (const TinyEXIF::EXIFInfo::MPImage_t& image: imageEXIF.MPImages) {
		if (!image.isLargeThumbnail() || image.Offset > fileSize || image.Length > fileSize - image.Offset)
			continue;
		std::vector<uint8_t> preview(image.Length);
		file.clear();
		file.seekg((std::streamoff)image.Offset);
		if (file.read((char*)preview.data(), preview.size())) {
			// a complete JPEG, which may carry metadata of its own
			TinyEXIF::EXIFInfo previewEXIF(preview.data(), image.Length);
		}
	}
```

## License

MIT [License](https://github.com/cdcseacave/TinyEXIF/blob/master/LICENSE)

Copyright (c) 2025 cdcseacave

## Acknowledgments

Forked from [easyexif](https://github.com/mayanklahiri/easyexif) library (2013 version) of Mayank Lahiri (mlahiri@gmail.com); see [LICENSE.easyexif](https://github.com/cdcseacave/TinyEXIF/blob/master/LICENSE.easyexif) for its terms.