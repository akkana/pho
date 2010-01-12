/*
 * phoexif.c: glue between pho and the jhead exif code.
 *
 * Copyright 2002 by Akkana Peck.
 * You are free to use or modify this code under the Gnu Public License.
 */

#include "jhead.h"

#include "phoexif.h"

#include <stdio.h>

struct ExifTypes_s ExifLabels[] =
{
    { "Camera make", ExifString },
    { "Camera model", ExifString },
    { "Date", ExifString },
    { "Orientation", ExifInt },
    { "Color", ExifInt },
    { "Flash", ExifInt },
    { "Focal Length", ExifFloat },
    { "Exposure Time", ExifFloat },
    { "ApertureNumber", ExifFloat },
    { "Distance", ExifFloat },
    { "CCD Width", ExifFloat },
    { "Exposure Bias", ExifFloat },
    { "White Balance", ExifInt },
    { "Metering Mode", ExifInt },
    { "Exposure Program", ExifInt },
    { "ISO equivalent", ExifInt },
    { "Compression Level", ExifInt },
    { "EXIF Comments", ExifString },
    { "Thumbnail Size", ExifInt }
};

void ExifReadInfo(char* filename)
{
    ProcessFile(filename);
}

static char buf[BUFSIZ];

static char* ItoS(int i)
{
    sprintf(buf, "%d", i);
    return buf;
}

static char* FtoS(float f)
{
    sprintf(buf, "%f", f);
    return buf;
}

int HasExif()
{
    return (ImageInfo.FileName[0] != '\0');
}

const char* ExifGetString(ExifFields_e field)
{
    if (!HasExif()) {
        fprintf(stderr, "No info!\n");
        return 0;
    }

    switch (field)
    {
      case ExifCameraMake:
          return ImageInfo.CameraMake;
      case ExifCameraModel:
          return ImageInfo.CameraModel;
      case ExifDate:
          return ImageInfo.DateTime;
      case ExifOrientation:
          return OrientTab[ImageInfo.Orientation];
      case ExifColor:
          return ItoS(ImageInfo.IsColor);
      case ExifFlash:
          return ItoS(ImageInfo.FlashUsed);
      case ExifFocalLength:
          return FtoS(ImageInfo.FocalLength);
      case ExifExposureTime:
          return FtoS(ImageInfo.ExposureTime);
      case ExifAperture:
          return FtoS(ImageInfo.ApertureFNumber);
      case ExifDistance:
          return FtoS(ImageInfo.Distance);
      case ExifCCDWidth:
          return FtoS(ImageInfo.CCDWidth);
      case ExifExposureBias:
          return FtoS(ImageInfo.ExposureBias);
      case ExifWhiteBalance:
          return ItoS(ImageInfo.Whitebalance);
      case ExifMetering:
          return ItoS(ImageInfo.MeteringMode);
      case ExifExposureProgram:
          return ItoS(ImageInfo.ExposureProgram);
      case ExifISO:
          return ItoS(ImageInfo.ISOequivalent);
      case ExifCompression:
          return ItoS(ImageInfo.CompressionLevel);
      case ExifComments:
          return ImageInfo.Comments;
      case ExifThumbnailSize:
          return ItoS(ImageInfo.ThumbnailSize);
    }

    return 0;
}

int ExifGetInt(ExifFields_e field)
{
    if (!HasExif()) {
        fprintf(stderr, "No info!\n");
        return 0;
    }

    switch (field)
    {
      case ExifOrientation:
          return OrientRot[ImageInfo.Orientation];
      case ExifColor:
          return ImageInfo.IsColor;
      case ExifFlash:
          return ImageInfo.FlashUsed;
      case ExifFocalLength:
          return (int)ImageInfo.FocalLength;
      case ExifExposureTime:
          return (int)ImageInfo.ExposureTime;
      case ExifAperture:
          return (int)ImageInfo.ApertureFNumber;
      case ExifDistance:
          return (int)ImageInfo.Distance;
      case ExifCCDWidth:
          return (int)ImageInfo.CCDWidth;
      case ExifExposureBias:
          return (int)ImageInfo.ExposureBias;
      case ExifWhiteBalance:
          return ImageInfo.Whitebalance;
      case ExifMetering:
          return ImageInfo.MeteringMode;
      case ExifExposureProgram:
          return ImageInfo.ExposureProgram;
      case ExifISO:
          return ImageInfo.ISOequivalent;
      case ExifCompression:
          return ImageInfo.CompressionLevel;
      case ExifThumbnailSize:
          return ImageInfo.ThumbnailSize;
    }
    return 0;
}

float ExifGetFloat(ExifFields_e field)
{
    if (!HasExif()) {
        fprintf(stderr, "No info!\n");
        return 0;
    }

    switch (field)
    {
      case ExifOrientation:
          return (float)OrientRot[ImageInfo.Orientation];
      case ExifColor:
          return (float)ImageInfo.IsColor;
      case ExifFlash:
          return (float)ImageInfo.FlashUsed;
      case ExifFocalLength:
          return ImageInfo.FocalLength;
      case ExifExposureTime:
          return ImageInfo.ExposureTime;
      case ExifAperture:
          return ImageInfo.ApertureFNumber;
      case ExifDistance:
          return ImageInfo.Distance;
      case ExifCCDWidth:
          return ImageInfo.CCDWidth;
      case ExifExposureBias:
          return ImageInfo.ExposureBias;
      case ExifWhiteBalance:
          return (float)ImageInfo.Whitebalance;
      case ExifMetering:
          return (float)ImageInfo.MeteringMode;
      case ExifExposureProgram:
          return (float)ImageInfo.ExposureProgram;
      case ExifISO:
          return (float)ImageInfo.ISOequivalent;
      case ExifCompression:
          return (float)ImageInfo.CompressionLevel;
      case ExifThumbnailSize:
          return (float)ImageInfo.ThumbnailSize;
    }
    return 0;
}

