// Defines the entry point for the console application.

#ifdef _MSC_VER
#include <windows.h>
#endif
#include "TinyEXIF.h"
#include <iostream> // std::cout
#include <fstream>  // std::ifstream
#include <vector>   // std::vector
#include <string>   // std::string
#include <iomanip>  // std::setprecision

// Compile-time check that TINYEXIF_VERSION works as documented in TinyEXIF.h:
// fail the build with a clear message rather than a confusing downstream
// error if this demo is ever paired with a pre-1.1.0 header.
#if TINYEXIF_VERSION < 10100
#error "This demo requires TinyEXIF.h 1.1.0 or later"
#endif
static_assert(sizeof(TINYEXIF_VERSION_STRING) > 1, "TINYEXIF_VERSION_STRING must be a non-empty string literal");

int main(int argc, const char** argv)
{
	// the field list is optional so that the output above stays the same for
	// everything that already parses it
	const bool listFields(argc == 3 && std::string(argv[2]) == "--fields");
	if (!listFields && argc != 2) {
		std::cout << "Usage: TinyEXIF <image_file> [--fields]\n";
		return -1;
	}

	// open a stream to read just the necessary parts of the image file
	std::ifstream stream(argv[1], std::ios::binary);
	if (!stream) {
		std::cout << "error: can not open '" << argv[1] << "'\n";
		return -2;
	}

	// parse image EXIF and XMP metadata
	TinyEXIF::EXIFInfo imageEXIF(stream);
	if (!imageEXIF.Fields) {
		std::cout << "error: no EXIF or XMP metadata\n";
		return -3;
	}

	// print extracted metadata
	if (imageEXIF.ImageWidth || imageEXIF.ImageHeight)
		std::cout << "ImageResolution " << imageEXIF.ImageWidth << "x" << imageEXIF.ImageHeight << " pixels" << "\n";
	if (imageEXIF.RelatedImageWidth || imageEXIF.RelatedImageHeight)
		std::cout << "RelatedImageResolution " << imageEXIF.RelatedImageWidth << "x" << imageEXIF.RelatedImageHeight << " pixels" << "\n";
	if (!imageEXIF.ImageDescription.empty())
		std::cout << "Description " << imageEXIF.ImageDescription << "\n";
	if (!imageEXIF.Make.empty() || !imageEXIF.Model.empty())
		std::cout << "CameraModel " << imageEXIF.Make << " - " << imageEXIF.Model << "\n";
	if (!imageEXIF.SerialNumber.empty())
		std::cout << "SerialNumber " << imageEXIF.SerialNumber << "\n";
	if (imageEXIF.Orientation)
		std::cout << "Orientation " << imageEXIF.Orientation << "\n";
	if (imageEXIF.XResolution || imageEXIF.YResolution || imageEXIF.ResolutionUnit)
		std::cout << "Resolution " << imageEXIF.XResolution << "x" << imageEXIF.YResolution << " (" << imageEXIF.ResolutionUnit << ")\n";
	if (imageEXIF.BitsPerSample)
		std::cout << "BitsPerSample " << imageEXIF.BitsPerSample << "\n";
	if (!imageEXIF.Software.empty())
		std::cout << "Software " << imageEXIF.Software << "\n";
	if (!imageEXIF.DateTime.empty())
		std::cout << "DateTime " << imageEXIF.DateTime << "\n";
	if (!imageEXIF.DateTimeOriginal.empty())
		std::cout << "DateTimeOriginal " << imageEXIF.DateTimeOriginal << "\n";
	if (!imageEXIF.DateTimeDigitized.empty())
		std::cout << "DateTimeDigitized " << imageEXIF.DateTimeDigitized << "\n";
	if (!imageEXIF.SubSecTimeOriginal.empty())
		std::cout << "SubSecTimeOriginal " << imageEXIF.SubSecTimeOriginal << "\n";
	if (!imageEXIF.Copyright.empty())
		std::cout << "Copyright " << imageEXIF.Copyright << "\n";
	std::cout << "ExposureTime " << std::setprecision(10) << imageEXIF.ExposureTime << " s" << "\n";
	std::cout << "FNumber " << imageEXIF.FNumber << "\n";
	std::cout << "ExposureProgram " << imageEXIF.ExposureProgram << "\n";
	std::cout << "ISOSpeed " << imageEXIF.ISOSpeedRatings << "\n";
	std::cout << "ShutterSpeedValue " << std::setprecision(10) << imageEXIF.ShutterSpeedValue << "\n";
	std::cout << "ApertureValue " << std::setprecision(10) << imageEXIF.ApertureValue << "\n";
	if (imageEXIF.HasField(TinyEXIF::FIELD_ID_MaxApertureValue))
		std::cout << "MaxApertureValue " << std::setprecision(10) << imageEXIF.MaxApertureValue << "\n";
	std::cout << "BrightnessValue " << std::setprecision(10) << imageEXIF.BrightnessValue << "\n";
	std::cout << "ExposureBiasValue " << imageEXIF.ExposureBiasValue << "\n";
	std::cout << "SubjectDistance " << imageEXIF.SubjectDistance << "\n";
	std::cout << "FocalLength " << imageEXIF.FocalLength << " mm" << "\n";
	std::cout << "Flash " << imageEXIF.Flash << "\n";
	if (!imageEXIF.SubjectArea.empty()) {
		std::cout << "SubjectArea";
		for (uint16_t val: imageEXIF.SubjectArea)
			std::cout << " " << val;
		std::cout << "\n";
	}
	std::cout << "MeteringMode " << imageEXIF.MeteringMode << "\n";
	std::cout << "LightSource " << imageEXIF.LightSource << "\n";
	std::cout << "ProjectionType " << imageEXIF.ProjectionType << "\n";
	if (imageEXIF.Calibration.FocalLength != 0)
		std::cout << "Calibration.FocalLength " << imageEXIF.Calibration.FocalLength << " pixels" << "\n";
	if (imageEXIF.Calibration.OpticalCenterX != 0)
		std::cout << "Calibration.OpticalCenterX " << imageEXIF.Calibration.OpticalCenterX << " pixels" << "\n";
	if (imageEXIF.Calibration.OpticalCenterY != 0)
		std::cout << "Calibration.OpticalCenterY " << imageEXIF.Calibration.OpticalCenterY << " pixels" << "\n";
	std::cout << "LensInfo.FStopMin " << imageEXIF.LensInfo.FStopMin << "\n";
	std::cout << "LensInfo.FStopMax " << imageEXIF.LensInfo.FStopMax << "\n";
	std::cout << "LensInfo.FocalLengthMin " << imageEXIF.LensInfo.FocalLengthMin << " mm" << "\n";
	std::cout << "LensInfo.FocalLengthMax " << imageEXIF.LensInfo.FocalLengthMax << " mm" << "\n";
	std::cout << "LensInfo.DigitalZoomRatio " << imageEXIF.LensInfo.DigitalZoomRatio << "\n";
	std::cout << "LensInfo.FocalLengthIn35mm " << imageEXIF.LensInfo.FocalLengthIn35mm << "\n";
	std::cout << "LensInfo.FocalPlaneXResolution " << std::setprecision(10) << imageEXIF.LensInfo.FocalPlaneXResolution << "\n";
	std::cout << "LensInfo.FocalPlaneYResolution " << std::setprecision(10) << imageEXIF.LensInfo.FocalPlaneYResolution << "\n";
	std::cout << "LensInfo.FocalPlaneResolutionUnit " << imageEXIF.LensInfo.FocalPlaneResolutionUnit << "\n";
	if (!imageEXIF.LensInfo.Make.empty() || !imageEXIF.LensInfo.Model.empty())
		std::cout << "LensInfo.Model " << imageEXIF.LensInfo.Make << " - " << imageEXIF.LensInfo.Model << "\n";
	if (imageEXIF.GeoLocation.hasLatLon()) {
		std::cout << "GeoLocation.Latitude " << std::setprecision(10) << imageEXIF.GeoLocation.Latitude << "\n";
		std::cout << "GeoLocation.Longitude " << std::setprecision(10) << imageEXIF.GeoLocation.Longitude << "\n";
	}
	if (imageEXIF.GeoLocation.hasAltitude()) {
		std::cout << "GeoLocation.Altitude " << imageEXIF.GeoLocation.Altitude << " m" << "\n";
		std::cout << "GeoLocation.AltitudeRef " << (int)imageEXIF.GeoLocation.AltitudeRef << "\n";
	}
	if (imageEXIF.GeoLocation.hasRelativeAltitude())
		std::cout << "GeoLocation.RelativeAltitude " << imageEXIF.GeoLocation.RelativeAltitude << " m" << "\n";
	if (imageEXIF.GeoLocation.hasOrientation()) {
		std::cout << "GeoLocation.RollDegree " << imageEXIF.GeoLocation.RollDegree << "\n";
		std::cout << "GeoLocation.PitchDegree " << imageEXIF.GeoLocation.PitchDegree << "\n";
		std::cout << "GeoLocation.YawDegree " << imageEXIF.GeoLocation.YawDegree << "\n";
	}
	if (imageEXIF.GeoLocation.hasSpeed()) {
		std::cout << "GeoLocation.SpeedX " << imageEXIF.GeoLocation.SpeedX << " m/s" << "\n";
		std::cout << "GeoLocation.SpeedY " << imageEXIF.GeoLocation.SpeedY << " m/s" << "\n";
		std::cout << "GeoLocation.SpeedZ " << imageEXIF.GeoLocation.SpeedZ << " m/s" << "\n";
	}
	if (imageEXIF.GeoLocation.AccuracyXY > 0 || imageEXIF.GeoLocation.AccuracyZ > 0)
		std::cout << "GeoLocation.GPSAccuracy XY " << imageEXIF.GeoLocation.AccuracyXY << " m" << " Z " << imageEXIF.GeoLocation.AccuracyZ << " m" << "\n";
	std::cout << "GeoLocation.GPSDOP " << imageEXIF.GeoLocation.GPSDOP << "\n";
	std::cout << "GeoLocation.GPSDifferential " << imageEXIF.GeoLocation.GPSDifferential << "\n";
	if (!imageEXIF.GeoLocation.GPSMapDatum.empty())
		std::cout << "GeoLocation.GPSMapDatum " << imageEXIF.GeoLocation.GPSMapDatum << "\n";
	if (!imageEXIF.GeoLocation.GPSTimeStamp.empty())
		std::cout << "GeoLocation.GPSTimeStamp " << imageEXIF.GeoLocation.GPSTimeStamp << "\n";
	if (!imageEXIF.GeoLocation.GPSDateStamp.empty())
		std::cout << "GeoLocation.GPSDateStamp " << imageEXIF.GeoLocation.GPSDateStamp << "\n";
	if (!imageEXIF.GPano.ProjectionType.empty()) {
		std::cout << "GPano.ProjectionType " << imageEXIF.GPano.ProjectionType << "\n";
		std::cout << "GPano.isEquirectangular " << (imageEXIF.GPano.isEquirectangular() ? "true" : "false") << "\n";
	}
	if (imageEXIF.GPano.hasPoseHeadingDegrees())
		std::cout << "GPano.PoseHeadingDegrees " << imageEXIF.GPano.PoseHeadingDegrees << "\n";
	if (imageEXIF.GPano.hasPosePitchDegrees())
		std::cout << "GPano.PosePitchDegrees " << imageEXIF.GPano.PosePitchDegrees << "\n";
	if (imageEXIF.GPano.hasPoseRollDegrees())
		std::cout << "GPano.PoseRollDegrees " << imageEXIF.GPano.PoseRollDegrees << "\n";
	if (imageEXIF.GPano.hasCroppedAreaImageWidthPixels())
		std::cout << "GPano.CroppedAreaImageWidthPixels " << imageEXIF.GPano.CroppedAreaImageWidthPixels << "\n";
	if (imageEXIF.GPano.hasCroppedAreaImageHeightPixels())
		std::cout << "GPano.CroppedAreaImageHeightPixels " << imageEXIF.GPano.CroppedAreaImageHeightPixels << "\n";
	if (imageEXIF.GPano.hasFullPanoWidthPixels())
		std::cout << "GPano.FullPanoWidthPixels " << imageEXIF.GPano.FullPanoWidthPixels << "\n";
	if (imageEXIF.GPano.hasFullPanoHeightPixels())
		std::cout << "GPano.FullPanoHeightPixels " << imageEXIF.GPano.FullPanoHeightPixels << "\n";
	if (imageEXIF.GPano.hasCroppedAreaLeftPixels())
		std::cout << "GPano.CroppedAreaLeftPixels " << imageEXIF.GPano.CroppedAreaLeftPixels << "\n";
	if (imageEXIF.GPano.hasCroppedAreaTopPixels())
		std::cout << "GPano.CroppedAreaTopPixels " << imageEXIF.GPano.CroppedAreaTopPixels << "\n";
	if (imageEXIF.Distortion.hasDewarpFlag())
		std::cout << "Distortion.DewarpFlag " << imageEXIF.Distortion.DewarpFlag << "\n";
	if (imageEXIF.Distortion.hasDistortion())
		std::cout << "Distortion [K1 K2 P1 P2 K3] " << std::setprecision(6)
		          << imageEXIF.Distortion.K1 << " " << imageEXIF.Distortion.K2 << " " << imageEXIF.Distortion.P1
		          << " " << imageEXIF.Distortion.P2 << " " << imageEXIF.Distortion.K3 << "\n";
	if (imageEXIF.MicroVideo.HasMicroVideo) {
		std::cout << "MicroVideo.MicroVideoVersion " << imageEXIF.MicroVideo.MicroVideoVersion << "\n";
		std::cout << "MicroVideo.MicroVideoOffset " << imageEXIF.MicroVideo.MicroVideoOffset << "\n";
	}
	if (imageEXIF.MicroVideo.HasMotionPhoto) {
		std::cout << "MicroVideo.MotionPhotoLength " << imageEXIF.MicroVideo.MotionPhotoLength << "\n";
		if (!imageEXIF.MicroVideo.MotionPhotoMime.empty())
			std::cout << "MicroVideo.MotionPhotoMime " << imageEXIF.MicroVideo.MotionPhotoMime << "\n";
	}
	// numbered from 1 like exiftool does; the first image is normally this one
	for (size_t i=0; i<imageEXIF.MPImages.size(); ++i) {
		const TinyEXIF::EXIFInfo::MPImage_t& image(imageEXIF.MPImages[i]);
		std::cout << "MPImage" << i+1
			<< " Type 0x" << std::hex << std::setfill('0') << std::setw(6) << image.Type << std::dec << std::setfill(' ')
			<< " Flags " << (unsigned)image.Flags << " Offset " << image.Offset << " Length " << image.Length
			<< (image.isLargeThumbnail() ? " (large thumbnail)" : "") << "\n";
	}

	// list which fields were actually present: the values printed above can not
	// tell a tag that was absent from a tag that was there and legitimately zero,
	// while HasField()/GetFields() can
	if (listFields) {
		const std::vector<TinyEXIF::FieldID> fields(imageEXIF.GetFields());
		std::cout << "Fields present: " << fields.size() << "\n";
		for (size_t i=0; i<fields.size(); ++i)
			std::cout << "  " << TinyEXIF::FieldName(fields[i]) << "\n";
		// ISOSpeedRatings is the example from issue #15: a value of 0 means
		// "absent" or "the camera really reported 0" and only this tells which
		std::cout << "ISOSpeedRatings " << imageEXIF.ISOSpeedRatings
			<< (imageEXIF.HasField(TinyEXIF::FIELD_ID_ISOSpeedRatings) ? " (present)" : " (absent)") << "\n";
	}
	return EXIT_SUCCESS;
}
