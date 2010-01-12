//--------------------------------------------------------------------------
// Program to pull the information out of various types of EXIF digital 
// camera files and show it in a reasonably consistent way
// Version 1.7
//
//
// Compiling under Unix:
// Use: cc -O3 -o jhead jhead.c exif.c -lm
//
// Compiling under Windows:  Use MSVC5 or MSVC6, from command line:
// cl -Ox jhead.c exif.c myglob.c
//
// Dec 1999 - May 2002
//
// by Matthias Wandel (mwandel@rim.net),
//--------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>

#define JHEAD_VERSION "1.7"

// This #define turns on features that are too very specific to 
// how I organize my photos.  Best to ignore everything inside #ifdef MATTHIAS
//#define MATTHIAS

#ifdef _WIN32
    #include <process.h>
    #include <io.h>
    #include <sys/utime.h>
#else
    #include <utime.h>
    #include <sys/types.h>
    #include <unistd.h>
    #include <errno.h>
    #include <limits.h>
#endif

#include "jhead.h"

static int FilesMatched;

static const char * CurrentFile;

//--------------------------------------------------------------------------
// Command line options flags
static int DoModify     = FALSE;
       int ShowTags     = FALSE;    // Do not show raw by default.
static char * ApplyCommand = NULL;  // Apply this command to all images.
static char * FilterModel = NULL;
static int    ExifOnly    = FALSE;

static char * ExifXferScrFile = NULL;// Extract Exif header from this file, and
                                    // put it into the jpegs processed.

static int SupressNonFatalErrors = TRUE; // Wether or not to pint warnings on recoverable errors


#ifdef MATTHIAS
    // This #ifdef to take out less than elegant stuff for editing
    // the comments in a jpeg.  The programs rdjpgcom and wrjpgcom
    // included with Linux distributions do a better job.

    static char * AddComment = NULL; // Add this tag.
    static char * RemComment = NULL; // Remove this tag
    static int AutoResize = FALSE;
#endif // MATTHIAS

//--------------------------------------------------------------------------
// Error exit handler
//--------------------------------------------------------------------------
void ErrFatal(char * msg)
{
    fprintf(stderr,"Error : %s\n", msg);
    if (CurrentFile) fprintf(stderr,"in file '%s'\n",CurrentFile);
    exit(EXIT_FAILURE);
} 

//--------------------------------------------------------------------------
// Report non fatal errors.  Now that microsoft.net modifies exif headers,
// there's corrupted ones, and there could be more in the future.
//--------------------------------------------------------------------------
void ErrNonfatal(char * msg, int a1, int a2)
{
    if (SupressNonFatalErrors) return;

    fprintf(stderr,"Nonfatal Error : ");
    if (CurrentFile) fprintf(stderr,"'%s' ",CurrentFile);
    fprintf(stderr, msg, a1, a2);
    fprintf(stderr, "\n");
} 

//--------------------------------------------------------------------------
// Apply the specified command to the jpeg file.
//--------------------------------------------------------------------------
static void DoCommand(const char * FileName)
{
    int a,e;
    char ExecString[400];
    char TempName[200];
    int TempUsed = FALSE;

    e = 0;

    // Make a temporary file in the destination directory by changing last char.
    strcpy(TempName, FileName);
    a = strlen(TempName)-1;
    TempName[a] = TempName[a] == 't' ? 'z' : 't';

    // Build the exec string.  &i and &o in the exec string get replaced by input and output files.
    for (a=0;;a++){
        if (ApplyCommand[a] == '&'){
            if (ApplyCommand[a+1] == 'i'){
                // Input file.
                if (strstr(FileName, " ")){
                    e += sprintf(ExecString+e, "\"%s\"",FileName);
                }else{
                    // No need for quoting (that way I can put a relative path in front)
                    e += sprintf(ExecString+e, "%s",FileName);
                }
                a += 1;
                continue;
            }
            if (ApplyCommand[a+1] == 'o'){
                // Needs an output file distinct from the input file.
                e += sprintf(ExecString+e, "\"%s\"",TempName);
                a += 1;
                TempUsed = TRUE;
                unlink(TempName);// Remove any pre-existing temp file
                continue;
            }
        }
        ExecString[e++] = ApplyCommand[a];
        if (ApplyCommand[a] == 0) break;
    }

    printf("Cmd:%s\n",ExecString);

    errno = 0;
    a = system(ExecString);

    if (a || errno){
        // A command can however fail without errno getting set or system returning an error.
        if (errno) perror("system");
        ErrFatal("Problem executing specified command");
    }

    if (TempUsed){
        // Don't delete original file until we know a new one was created by the command.
        struct stat dummy;
        if (stat(TempName, &dummy) == 0){
            unlink(FileName);
            rename(TempName, FileName);
        }else{
            ErrFatal("specified command did not produce expected output file");
        }
    }
}

//--------------------------------------------------------------------------
// check if this file should be skipped based on contents.
//--------------------------------------------------------------------------
static int CheckFileSkip(void)
{
    // I sometimes add code here to only process images based on certain
    // criteria - for example, only to convert non progressive jpegs to progressives, etc..

    if (FilterModel){
        // Filtering processing by camera model.
        if (strstr(ImageInfo.CameraModel, FilterModel) == NULL){
            // Skip.
            return TRUE;

        }
    }

    if (ExifOnly){
        // Filtering by EXIF only.  Skip all files that have no Exif.
        if (FindSection(M_EXIF) == NULL){
            return TRUE;
        }
    }

    return FALSE;
}

//--------------------------------------------------------------------------
// Subsititute original name for '&i' if present in specified name.
// This to allow specifying relative names when manipulating multiple files.
//--------------------------------------------------------------------------
static void RelativeName(char * OutFileName, const char * NamePattern, const char * OrigName)
{
    // If the filename contains substring "&i", then substitute the 
    // filename for that.  This gives flexibility in terms of processing
    // multiple files at a time.
    char * Subst;
    Subst = strstr(NamePattern, "&i");
    if (Subst){
        strncpy(OutFileName, NamePattern, Subst-NamePattern);
        OutFileName[Subst-NamePattern] = 0;
        strncat(OutFileName, OrigName, PATH_MAX);
        strncat(OutFileName, Subst+2, PATH_MAX);
    }else{
        strcpy(OutFileName, NamePattern); 
    }
}

//--------------------------------------------------------------------------
// Do selected operations to one file at a time.
//--------------------------------------------------------------------------
void ProcessFile(const char * FileName)
{
    int Modified = FALSE;
    ReadMode_t ReadMode = READ_EXIF;

    CurrentFile = FileName;

    ResetJpgfile();

    // Start with an empty image information structure.
    memset(&ImageInfo, 0, sizeof(ImageInfo));
    ImageInfo.FlashUsed = -1;
    ImageInfo.MeteringMode = -1;

    // Store file date/time.
    {
        struct stat st;
        if (stat(FileName, &st) >= 0){
            ImageInfo.FileDateTime = st.st_mtime;
            ImageInfo.FileSize = st.st_size;
        }else{
            ErrFatal("No such file");
        }
    }

    strncpy(ImageInfo.FileName, FileName, PATH_MAX);

    if (ApplyCommand){
        // Applying a command is special - the headers from the file have to be
        // pre-read, then the command executed, and then the image part of the file read.

        if (!ReadJpegFile(FileName, READ_EXIF)) return;

        #ifdef MATTHIAS
            if (AutoResize){
                // Automatic resize computation - to customize for each run...
                if (AutoResizeCmdStuff() == 0){
                    DiscardData();
                    return;
                }
            }
        #endif // MATTHIAS

        if (CheckFileSkip()){
            DiscardData();
            return;
        }

        DiscardAllButExif();

        DoCommand(FileName);
        Modified = TRUE;

        ReadMode = READ_IMAGE;   // Don't re-read exif section again on next read.
    }else if (ExifXferScrFile){
        char RelativeExifName[PATH_MAX+1];

        // Make a relative name.
        RelativeName(RelativeExifName, ExifXferScrFile, FileName);

        if(!ReadJpegFile(RelativeExifName, READ_EXIF)) return;

        DiscardAllButExif();    // Don't re-read exif section again on next read.

        Modified = TRUE;
        ReadMode = READ_IMAGE;
    }

    FilesMatched += 1;

    FilesMatched = TRUE; // Turns off complaining that nothing matched.

    if (DoModify){
        ReadMode |= READ_IMAGE;
    }

    if (!ReadJpegFile(FileName, ReadMode)) return;

#ifdef VERBOSE
    if (CheckFileSkip()){
        DiscardData();
        return;
    }

    if (ShowConcise){
        ShowConciseImageInfo();
    }else{
        if (!(DoModify || DoReadAction) || ShowTags){
            ShowImageInfo();
        }
    }

    if (ThumbnailName){
        if (ImageInfo.ThumbnailPointer){
            FILE * ThumbnailFile;
            char OutFileName[PATH_MAX+1];

            // Make a relative name.
            RelativeName(OutFileName, ThumbnailName, FileName);

#ifndef _WIN32
            if (strcmp(ThumbnailName, "-") == 0){
                // A filename of '-' indicates thumbnail goes to stdout.
                // This doesn't make much sense under Windows, so this feature is unix only.
                ThumbnailFile = stdout;
            }else
#endif
            {
                ThumbnailFile = fopen(OutFileName,"wb");
            }

            if (ThumbnailFile){
                fwrite(ImageInfo.ThumbnailPointer, ImageInfo.ThumbnailSize ,1, ThumbnailFile);
                fclose(ThumbnailFile);
                if (ThumbnailFile != stdout){
                    printf("Created: '%s'\n", OutFileName);
                }else{
                    // No point in printing to stdout when that is where the thumbnail goes!
                }
            }else{
                ErrFatal("Could not write thumbnail file");
            }
        }else{
            printf("Image '%s' contains no thumbnail\n",FileName);
        }
    }

#ifdef MATTHIAS
    if (AddComment || RemComment|| EditComment){
#else
    if (EditComment){
#endif
        Section_t * CommentSec;
        char Comment[1000];
        int CommentSize;

        CommentSec = FindSection(M_COM);

        if (CommentSec == NULL){
            unsigned char * DummyData;

            DummyData = (uchar *) malloc(3);
            DummyData[0] = 0;
            DummyData[1] = 2;
            DummyData[2] = 0;
            CommentSec = CreateSection(M_COM, DummyData, 2);
        }

        CommentSize = CommentSec->Size-2;

#ifdef MATTHIAS
        if (ModifyDescriptComment(Comment, (char *)CommentSec->Data+2)){
            Modified = TRUE;
            CommentSize = strlen(Comment);
        }
        if (EditComment)
#else
        memcpy(Comment, (char *)CommentSec->Data+2, CommentSize);
#endif
        {
            char EditFileName[PATH_MAX+4];
            strcpy(EditFileName, FileName);
            strcat(EditFileName, ".txt");

            CommentSize = FileEditComment(EditFileName, Comment, CommentSize);
        }

        if (strcmp(Comment, (char *)CommentSec->Data+2)){
            // Discard old comment section and put a new one in.
            int size;
            size = CommentSize+2;
            free(CommentSec->Data);
            CommentSec->Size = size;
            CommentSec->Data = malloc(size);
            CommentSec->Data[0] = (uchar)(size >> 8);
            CommentSec->Data[1] = (uchar)(size);
            memcpy((CommentSec->Data)+2, Comment, size-2);
            Modified = TRUE;
        }
        if (!Modified){
            printf("Comment not modified\n");
        }
    }

    if (ExifTimeAdjust || ExifTimeSet){
        if (ImageInfo.DatePointer){
            struct tm tm;
            time_t UnixTime;
            char TempBuf[50];

            if (ExifTimeSet){
                // A time to set was specified.
                UnixTime = ExifTimeSet;
            }else{
                // A time offset to adjust by was specified.
                if (!Exif2tm(&tm, ImageInfo.DateTime)) goto badtime;

                // Convert to unix 32 bit time value, add offset, and convert back.
                UnixTime = mktime(&tm);
                if ((int)UnixTime == -1) goto badtime;
                UnixTime += ExifTimeAdjust;
            }
            tm = *localtime(&UnixTime);

            // Print to temp buffer first to avoid putting null termination in destination.
            // snprintf() would do the trick ,but not available everywhere (like FreeBSD 4.4)
            sprintf(TempBuf, "%04d:%02d:%02d %02d:%02d:%02d",
                tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
                tm.tm_hour, tm.tm_min, tm.tm_sec);

            memcpy(ImageInfo.DatePointer, TempBuf, 19);

            Modified = TRUE;
        }else{
            printf("File '%s' contains no Exif timestamp to change\n", FileName);
        }
    }

    if (TrimExif){
        if (TrimExifFunc()) Modified = TRUE;
    }
    
    if (DeleteComments){
        if (RemoveSectionType(M_COM)) Modified = TRUE;
    }
    if (DeleteExif){
        if (RemoveSectionType(M_EXIF)) Modified = TRUE;
    }


    if (Modified){
        char BackupName[400];
        printf("Modified: %s\n",FileName);

        strcpy(BackupName, FileName);
        strcat(BackupName, ".t");

        // Remove any .old file name that may pre-exist
        unlink(BackupName);

        // Rename the old file.
        rename(FileName, BackupName);

        // Write the new file.
        WriteJpegFile(FileName);

        // Now that we are done, remove original file.
        unlink(BackupName);
    }


    if (Exif2FileTime){
        // Set the file date to the date from the exif header.
        if (ImageInfo.DateTime[0]){
            // Converte the file date to Unix time.
            struct tm tm;
            time_t UnixTime;
            struct utimbuf mtime;
            if (!Exif2tm(&tm, ImageInfo.DateTime)) goto badtime;

            UnixTime = mktime(&tm);
            if ((int)UnixTime == -1){
                goto badtime;
            }

            mtime.actime = UnixTime;
            mtime.modtime = UnixTime;

            if (utime(FileName, &mtime) != 0){
                printf("Error: Could not change time of file '%s'\n",FileName);
            }else{
                printf("%s\n",FileName);
            }
        }else{
            printf("File '%s' contains no Exif timestamp\n", FileName);
        }
    }

    // Feature to rename image according to date and time from camera.
    // I use this feature to put images from multiple digicams in sequence.

    if (RenameToDate){
        int NumAlpha = 0;
        int NumDigit = 0;
        int PrefixPart = 0;
        
        for (a=0;FileName[a];a++){
            if (FileName[a] == '/' || FileName[a] == '\\'){
                // Don't count path compoenent.
                NumAlpha = 0;
                NumDigit = 0;
                PrefixPart = a+1;
            }
            if (isalpha(FileName[a])) NumAlpha += 1;
            if (isdigit(FileName[a])) NumDigit += 1;
        }

        if ((NumAlpha <= 8 && NumDigit >= 2) || RenameToDate > 1){
            if (ImageInfo.DateTime[0]){
                struct tm tm;
                if (Exif2tm(&tm, ImageInfo.DateTime)){
                    char NewBaseName[PATH_MAX*2];

                    strcpy(NewBaseName, FileName); // Get path component of name.

                    if (strftime_args){
                        // Complicated scheme for flexibility.  Just pass the args to strftime.
                        time_t UnixTime;

                        // Call mktime to get weekday and such filled in.
                        UnixTime = mktime(&tm);
                        if ((int)UnixTime == -1) goto badtime;
                        strftime(NewBaseName+PrefixPart, 99, strftime_args, &tm);
                    }else{
                        // My favourite scheme.
                        sprintf(NewBaseName+PrefixPart, "%02d%02d-%02d%02d%02d",
                             tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
                    }

                    for (a=0;;a++){
                        char NewName[120];
                        char NameExtra[3];
                        struct stat dummy;

                        if (a){
                            // Generate a suffix for the file name if previous choice of names is taken.
                            // depending on wether the name ends in a letter or digit, pick the opposite to separate
                            // it.  This to avoid using a separator character - this because any good separator
                            // is before the '.' in ascii, and so sorting the names would put the later name before
                            // the name without suffix, causing the pictures to more likely be out of order.
                            if (isdigit(NewBaseName[strlen(NewBaseName)-1])){
                                NameExtra[0] = 'a'-1+a; // Try a,b,c,d... for suffix if it ends in a letter.
                            }else{
                                NameExtra[0] = '0'-1+a; // Try 1,2,3,4... for suffix if it ends in a char.
                            }
                            NameExtra[1] = 0;
                        }else{
                            NameExtra[0] = 0;
                        }

                        sprintf(NewName, "%s%s.jpg", NewBaseName, NameExtra);

                        if (!strcmp(FileName, NewName)) break; // Skip if its already this name.

                        if (stat(NewName, &dummy)){
                            // This name does not pre-exist.
                            if (rename(FileName, NewName) == 0){
                                printf("%s --> %s\n",FileName, NewName);
                            }else{
                                printf("Error: Couldn't rename '%s' to '%s'\n",FileName, NewName);
                            }
                            break;
                        }

                        if (a >= 9){
                            printf("Dest name for '%s' already exists\n",FileName);
                            break;
                        }
                    }
                }else{
                    printf("File '%s' contains no Exif timestamp\n", FileName);
                }
            }
        }
    }
    if(0){
        badtime:
        printf("Error: Time '%s': cannot convert to Unix time\n",ImageInfo.DateTime);
    }
    DiscardData();
#endif /* VERBOSE */
}

