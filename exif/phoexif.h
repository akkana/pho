/*
 * phoexif.c: glue between pho and the jhead exif code.
 *
 * Copyright 2002 by Akkana Peck.
 * You are free to use or modify this code under the Gnu Public License.
 */

#ifndef PHOEXIF_H
#define PHOEXIF_H 1

typedef enum { ExifString, ExifInt, ExifFloat } ExifDataType;

struct ExifTypes_s {
    char* str;
    ExifDataType type;
};

/* Loop over ExifLabels[] in order to make a list of possible fields. */
extern struct ExifTypes_s ExifLabels[];

/* The fields of this enum must correspond with the positions
 * in ExifLabels[].
 */
typedef enum
{
    ExifCameraMake = 0,
    ExifCameraModel,
    ExifDate,
    ExifOrientation,
    ExifColor,
    ExifFlash,
    ExifFocalLength,
    ExifExposureTime,
    ExifAperture,
    ExifDistance,
    ExifCCDWidth,
    ExifExposureBias,
    ExifWhiteBalance,
    ExifMetering,
    ExifExposureProgram,
    ExifISO,
    ExifCompression,
    ExifComments,
    ExifThumbnailSize
} ExifFields_e;

/*#define NUM_EXIF_FIELDS  (sizeof ExifLabels / sizeof (*ExifLabels))*/
#define NUM_EXIF_FIELDS  19

/*
 * You must call ExifReadInfo() before you call ExifGet*()!
 */
extern void ExifReadInfo(char* filename);

/*
 * Do selected operations to one file at a time.
*/
extern void ProcessFile(const char * FileName);

/*
 * This tells us whether we have good EXIF data
 * on the current image.
 */
extern int HasExif();

/* ExifGetString() returns a pointer to a STATIC buffer.
 * Copy it if you want to keep it!
 * It will work even the type is not a string (will translate appropriately).
 * ExifGetInt() and ExifGetFloat() do the obvious, simpler, things.
 */
extern const char* ExifGetString(ExifFields_e field);
extern         int ExifGetInt(ExifFields_e field);
extern       float ExifGetFloat(ExifFields_e field);


#endif /* PHOEXIF_H */
    
