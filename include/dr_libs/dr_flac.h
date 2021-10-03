/*
FLAC audio decoder. Choice of public domain or MIT-0. See license statements at
the end of this file. dr_flac - v0.12.31 - 2021-08-16

David Reid - mackron@gmail.com

GitHub: https://github.com/mackron/dr_libs
*/

/*
RELEASE NOTES - v0.12.0
=======================
Version 0.12.0 has breaking API changes including changes to the existing API
and the removal of deprecated APIs.


Improved Client-Defined Memory Allocation
-----------------------------------------
The main change with this release is the addition of a more flexible way of
implementing custom memory allocation routines. The existing system of
DRFLAC_MALLOC, DRFLAC_REALLOC and DRFLAC_FREE are still in place and will be
used by default when no custom allocation callbacks are specified.

To use the new system, you pass in a pointer to a drflac_allocation_callbacks
object to drflac_open() and family, like this:

    void* my_malloc(size_t sz, void* pUserData)
    {
        return malloc(sz);
    }
    void* my_realloc(void* p, size_t sz, void* pUserData)
    {
        return realloc(p, sz);
    }
    void my_free(void* p, void* pUserData)
    {
        free(p);
    }

    ...

    drflac_allocation_callbacks allocationCallbacks;
    allocationCallbacks.pUserData = &myData;
    allocationCallbacks.onMalloc  = my_malloc;
    allocationCallbacks.onRealloc = my_realloc;
    allocationCallbacks.onFree    = my_free;
    drflac* pFlac = drflac_open_file("my_file.flac", &allocationCallbacks);

The advantage of this new system is that it allows you to specify user data
which will be passed in to the allocation routines.

Passing in null for the allocation callbacks object will cause dr_flac to use
defaults which is the same as DRFLAC_MALLOC, DRFLAC_REALLOC and DRFLAC_FREE and
the equivalent of how it worked in previous versions.

Every API that opens a drflac object now takes this extra parameter. These
include the following:

    drflac_open()
    drflac_open_relaxed()
    drflac_open_with_metadata()
    drflac_open_with_metadata_relaxed()
    drflac_open_file()
    drflac_open_file_with_metadata()
    drflac_open_memory()
    drflac_open_memory_with_metadata()
    drflac_open_and_read_pcm_frames_s32()
    drflac_open_and_read_pcm_frames_s16()
    drflac_open_and_read_pcm_frames_f32()
    drflac_open_file_and_read_pcm_frames_s32()
    drflac_open_file_and_read_pcm_frames_s16()
    drflac_open_file_and_read_pcm_frames_f32()
    drflac_open_memory_and_read_pcm_frames_s32()
    drflac_open_memory_and_read_pcm_frames_s16()
    drflac_open_memory_and_read_pcm_frames_f32()



Optimizations
-------------
Seeking performance has been greatly improved. A new binary search based seeking
algorithm has been introduced which significantly improves performance over the
brute force method which was used when no seek table was present. Seek table
based seeking also takes advantage of the new binary search seeking system to
further improve performance there as well. Note that this depends on CRC which
means it will be disabled when DR_FLAC_NO_CRC is used.

The SSE4.1 pipeline has been cleaned up and optimized. You should see some
improvements with decoding speed of 24-bit files in particular. 16-bit streams
should also see some improvement.

drflac_read_pcm_frames_s16() has been optimized. Previously this sat on top of
drflac_read_pcm_frames_s32() and performed it's s32 to s16 conversion in a
second pass. This is now all done in a single pass. This includes SSE2 and ARM
NEON optimized paths.

A minor optimization has been implemented for drflac_read_pcm_frames_s32(). This
will now use an SSE2 optimized pipeline for stereo channel reconstruction which
is the last part of the decoding process.

The ARM build has seen a few improvements. The CLZ (count leading zeroes) and
REV (byte swap) instructions are now used when compiling with GCC and Clang
which is achieved using inline assembly. The CLZ instruction requires ARM
architecture version 5 at compile time and the REV instruction requires ARM
architecture version 6.

An ARM NEON optimized pipeline has been implemented. To enable this you'll need
to add -mfpu=neon to the command line when compiling.


Removed APIs
------------
The following APIs were deprecated in version 0.11.0 and have been completely
removed in version 0.12.0:

    drflac_read_s32()                   -> drflac_read_pcm_frames_s32()
    drflac_read_s16()                   -> drflac_read_pcm_frames_s16()
    drflac_read_f32()                   -> drflac_read_pcm_frames_f32()
    drflac_seek_to_sample()             -> drflac_seek_to_pcm_frame()
    drflac_open_and_decode_s32()        -> drflac_open_and_read_pcm_frames_s32()
    drflac_open_and_decode_s16()        -> drflac_open_and_read_pcm_frames_s16()
    drflac_open_and_decode_f32()        -> drflac_open_and_read_pcm_frames_f32()
    drflac_open_and_decode_file_s32()   ->
drflac_open_file_and_read_pcm_frames_s32() drflac_open_and_decode_file_s16() ->
drflac_open_file_and_read_pcm_frames_s16() drflac_open_and_decode_file_f32() ->
drflac_open_file_and_read_pcm_frames_f32() drflac_open_and_decode_memory_s32()
-> drflac_open_memory_and_read_pcm_frames_s32()
    drflac_open_and_decode_memory_s16() ->
drflac_open_memory_and_read_pcm_frames_s16() drflac_open_and_decode_memory_f32()
-> drflac_open_memroy_and_read_pcm_frames_f32()

Prior versions of dr_flac operated on a per-sample basis whereas now it operates
on PCM frames. The removed APIs all relate to the old per-sample APIs. You now
need to use the "pcm_frame" versions.
*/

#ifndef dr_flac_h
#define dr_flac_h

#ifdef __cplusplus
extern "C" {
#endif

#define DRFLAC_STRINGIFY(x) #x
#define DRFLAC_XSTRINGIFY(x) DRFLAC_STRINGIFY(x)

#define DRFLAC_VERSION_MAJOR 0
#define DRFLAC_VERSION_MINOR 12
#define DRFLAC_VERSION_REVISION 31
#define DRFLAC_VERSION_STRING                                                                      \
  DRFLAC_XSTRINGIFY(DRFLAC_VERSION_MAJOR)                                                          \
  "." DRFLAC_XSTRINGIFY(DRFLAC_VERSION_MINOR) "." DRFLAC_XSTRINGIFY(DRFLAC_VERSION_REVISION)

#include <dr_libs/dr_common.h>
#include <stddef.h> /* For size_t. */
#include <stdint.h> /* For sized types. */
#include <stdbool.h> /* For bool */

#define DRFLAC_TRUE 1
#define DRFLAC_FALSE 0

#if !defined(DRFLAC_API)
#if defined(DRFLAC_DLL)
#if defined(_WIN32)
#define DRFLAC_DLL_IMPORT __declspec(dllimport)
#define DRFLAC_DLL_EXPORT __declspec(dllexport)
#define DRFLAC_DLL_PRIVATE static
#else
#if defined(__GNUC__) && __GNUC__ >= 4
#define DRFLAC_DLL_IMPORT __attribute__((visibility("default")))
#define DRFLAC_DLL_EXPORT __attribute__((visibility("default")))
#define DRFLAC_DLL_PRIVATE __attribute__((visibility("hidden")))
#else
#define DRFLAC_DLL_IMPORT
#define DRFLAC_DLL_EXPORT
#define DRFLAC_DLL_PRIVATE static
#endif
#endif

#if defined(DR_FLAC_IMPLEMENTATION) || defined(DRFLAC_IMPLEMENTATION)
#define DRFLAC_API DRFLAC_DLL_EXPORT
#else
#define DRFLAC_API DRFLAC_DLL_IMPORT
#endif
#define DRFLAC_PRIVATE DRFLAC_DLL_PRIVATE
#else
#define DRFLAC_API extern
#define DRFLAC_PRIVATE static
#endif
#endif

#if defined(_MSC_VER) && _MSC_VER >= 1700 /* Visual Studio 2012 */
#define DRFLAC_DEPRECATED __declspec(deprecated)
#elif (defined(__GNUC__) && __GNUC__ >= 4) /* GCC 4 */
#define DRFLAC_DEPRECATED __attribute__((deprecated))
#elif defined(__has_feature) /* Clang */
#if __has_feature(attribute_deprecated)
#define DRFLAC_DEPRECATED __attribute__((deprecated))
#else
#define DRFLAC_DEPRECATED
#endif
#else
#define DRFLAC_DEPRECATED
#endif

DRFLAC_API void drflac_version(uint32_t* pMajor, uint32_t* pMinor,
                               uint32_t* pRevision);
DRFLAC_API const char* drflac_version_string(void);

/*
As data is read from the client it is placed into an internal buffer for fast
access. This controls the size of that buffer. Larger values means more speed,
but also more memory. In my testing there is diminishing returns after about
4KB, but you can fiddle with this to suit your own needs. Must be a multiple
of 8.
*/
#ifndef DR_FLAC_BUFFER_SIZE
#define DR_FLAC_BUFFER_SIZE 4096
#endif

/* Check if we can enable 64-bit optimizations. */
#if defined(_WIN64) || defined(_LP64) || defined(__LP64__)
#define DRFLAC_64BIT
#endif

#ifdef DRFLAC_64BIT
typedef uint64_t drflac_cache_t;
#else
typedef uint32_t drflac_cache_t;
#endif

/* The various metadata block types. */
#define DRFLAC_METADATA_BLOCK_TYPE_STREAMINFO 0
#define DRFLAC_METADATA_BLOCK_TYPE_PADDING 1
#define DRFLAC_METADATA_BLOCK_TYPE_APPLICATION 2
#define DRFLAC_METADATA_BLOCK_TYPE_SEEKTABLE 3
#define DRFLAC_METADATA_BLOCK_TYPE_VORBIS_COMMENT 4
#define DRFLAC_METADATA_BLOCK_TYPE_CUESHEET 5
#define DRFLAC_METADATA_BLOCK_TYPE_PICTURE 6
#define DRFLAC_METADATA_BLOCK_TYPE_INVALID 127

/* The various picture types specified in the PICTURE block. */
#define DRFLAC_PICTURE_TYPE_OTHER 0
#define DRFLAC_PICTURE_TYPE_FILE_ICON 1
#define DRFLAC_PICTURE_TYPE_OTHER_FILE_ICON 2
#define DRFLAC_PICTURE_TYPE_COVER_FRONT 3
#define DRFLAC_PICTURE_TYPE_COVER_BACK 4
#define DRFLAC_PICTURE_TYPE_LEAFLET_PAGE 5
#define DRFLAC_PICTURE_TYPE_MEDIA 6
#define DRFLAC_PICTURE_TYPE_LEAD_ARTIST 7
#define DRFLAC_PICTURE_TYPE_ARTIST 8
#define DRFLAC_PICTURE_TYPE_CONDUCTOR 9
#define DRFLAC_PICTURE_TYPE_BAND 10
#define DRFLAC_PICTURE_TYPE_COMPOSER 11
#define DRFLAC_PICTURE_TYPE_LYRICIST 12
#define DRFLAC_PICTURE_TYPE_RECORDING_LOCATION 13
#define DRFLAC_PICTURE_TYPE_DURING_RECORDING 14
#define DRFLAC_PICTURE_TYPE_DURING_PERFORMANCE 15
#define DRFLAC_PICTURE_TYPE_SCREEN_CAPTURE 16
#define DRFLAC_PICTURE_TYPE_BRIGHT_COLORED_FISH 17
#define DRFLAC_PICTURE_TYPE_ILLUSTRATION 18
#define DRFLAC_PICTURE_TYPE_BAND_LOGOTYPE 19
#define DRFLAC_PICTURE_TYPE_PUBLISHER_LOGOTYPE 20

typedef enum {
  drflac_container_native,
  drflac_container_ogg,
  drflac_container_unknown
} drflac_container;

typedef enum { drflac_seek_origin_start, drflac_seek_origin_current } drflac_seek_origin;

/* Packing is important on this structure because we map this directly to the
 * raw data within the SEEKTABLE metadata block. */
#pragma pack(2)
typedef struct {
  uint64_t firstPCMFrame;
  uint64_t flacFrameOffset; /* The offset from the first byte of the header
                                    of the first frame. */
  uint16_t pcmFrameCount;
} drflac_seekpoint;
#pragma pack()

typedef struct {
  uint16_t minBlockSizeInPCMFrames;
  uint16_t maxBlockSizeInPCMFrames;
  uint32_t minFrameSizeInPCMFrames;
  uint32_t maxFrameSizeInPCMFrames;
  uint32_t sampleRate;
  uint8_t channels;
  uint8_t bitsPerSample;
  uint64_t totalPCMFrameCount;
  uint8_t md5[16];
} drflac_streaminfo;

typedef struct {
  /*
  The metadata type. Use this to know how to interpret the data below. Will be
  set to one of the DRFLAC_METADATA_BLOCK_TYPE_* tokens.
  */
  uint32_t type;

  /*
  A pointer to the raw data. This points to a temporary buffer so don't hold on
  to it. It's best to not modify the contents of this buffer. Use the structures
  below for more meaningful and structured information about the metadata. It's
  possible for this to be null.
  */
  const void* pRawData;

  /* The size in bytes of the block and the buffer pointed to by pRawData if
   * it's non-NULL. */
  uint32_t rawDataSize;

  union {
    drflac_streaminfo streaminfo;

    struct {
      int unused;
    } padding;

    struct {
      uint32_t id;
      const void* pData;
      uint32_t dataSize;
    } application;

    struct {
      uint32_t seekpointCount;
      const drflac_seekpoint* pSeekpoints;
    } seektable;

    struct {
      uint32_t vendorLength;
      const char* vendor;
      uint32_t commentCount;
      const void* pComments;
    } vorbis_comment;

    struct {
      char catalog[128];
      uint64_t leadInSampleCount;
      bool isCD;
      uint8_t trackCount;
      const void* pTrackData;
    } cuesheet;

    struct {
      uint32_t type;
      uint32_t mimeLength;
      const char* mime;
      uint32_t descriptionLength;
      const char* description;
      uint32_t width;
      uint32_t height;
      uint32_t colorDepth;
      uint32_t indexColorCount;
      uint32_t pictureDataSize;
      const uint8_t* pPictureData;
    } picture;
  } data;
} drflac_metadata;

/*
Callback for when data needs to be read from the client.


Parameters
----------
pUserData (in)
    The user data that was passed to drflac_open() and family.

pBufferOut (out)
    The output buffer.

bytesToRead (in)
    The number of bytes to read.


Return Value
------------
The number of bytes actually read.


Remarks
-------
A return value of less than bytesToRead indicates the end of the stream. Do
_not_ return from this callback until either the entire bytesToRead is filled or
you have reached the end of the stream.
*/
typedef size_t (*drflac_read_proc)(void* pUserData, void* pBufferOut, size_t bytesToRead);

/*
Callback for when data needs to be seeked.


Parameters
----------
pUserData (in)
    The user data that was passed to drflac_open() and family.

offset (in)
    The number of bytes to move, relative to the origin. Will never be negative.

origin (in)
    The origin of the seek - the current position or the start of the stream.


Return Value
------------
Whether or not the seek was successful.


Remarks
-------
The offset will never be negative. Whether or not it is relative to the
beginning or current position is determined by the "origin" parameter which will
be either drflac_seek_origin_start or drflac_seek_origin_current.

When seeking to a PCM frame using drflac_seek_to_pcm_frame(), dr_flac may call
this with an offset beyond the end of the FLAC stream. This needs to be detected
and handled by returning DRFLAC_FALSE.
*/
typedef bool (*drflac_seek_proc)(void* pUserData, int offset, drflac_seek_origin origin);

/*
Callback for when a metadata block is read.


Parameters
----------
pUserData (in)
    The user data that was passed to drflac_open() and family.

pMetadata (in)
    A pointer to a structure containing the data of the metadata block.


Remarks
-------
Use pMetadata->type to determine which metadata block is being handled and how
to read the data. This will be set to one of the DRFLAC_METADATA_BLOCK_TYPE_*
tokens.
*/
typedef void (*drflac_meta_proc)(void* pUserData, drflac_metadata* pMetadata);

/* Structure for internal use. Only used for decoders opened with
 * drflac_open_memory. */
typedef struct {
  const uint8_t* data;
  size_t dataSize;
  size_t currentReadPos;
} drflac__memory_stream;

/* Structure for internal use. Used for bit streaming. */
typedef struct {
  /* The function to call when more data needs to be read. */
  drflac_read_proc onRead;

  /* The function to call when the current read position needs to be moved. */
  drflac_seek_proc onSeek;

  /* The user data to pass around to onRead and onSeek. */
  void* pUserData;

  /*
  The number of unaligned bytes in the L2 cache. This will always be 0 until the
  end of the stream is hit. At the end of the stream there will be a number of
  bytes that don't cleanly fit in an L1 cache line, so we use this variable to
  know whether or not the bistreamer needs to run on a slower path to read those
  last bytes. This will never be more than sizeof(drflac_cache_t).
  */
  size_t unalignedByteCount;

  /* The content of the unaligned bytes. */
  drflac_cache_t unalignedCache;

  /* The index of the next valid cache line in the "L2" cache. */
  uint32_t nextL2Line;

  /* The number of bits that have been consumed by the cache. This is used to
   * determine how many valid bits are remaining. */
  uint32_t consumedBits;

  /*
  The cached data which was most recently read from the client. There are two
  levels of cache. Data flows as such: Client -> L2 -> L1. The L2 -> L1 movement
  is aligned and runs on a fast path in just a few instructions.
  */
  drflac_cache_t cacheL2[DR_FLAC_BUFFER_SIZE / sizeof(drflac_cache_t)];
  drflac_cache_t cache;

  /*
  CRC-16. This is updated whenever bits are read from the bit stream. Manually
  set this to 0 to reset the CRC. For FLAC, this is reset to 0 at the beginning
  of each frame.
  */
  uint16_t crc16;
  drflac_cache_t crc16Cache;            /* A cache for optimizing CRC calculations. This is
                                           filled when when the L1 cache is reloaded. */
  uint32_t crc16CacheIgnoredBytes; /* The number of bytes to ignore when updating the
                                           CRC-16 from the CRC-16 cache. */
} drflac_bs;

typedef struct {
  /* The type of the subframe: SUBFRAME_CONSTANT, SUBFRAME_VERBATIM,
   * SUBFRAME_FIXED or SUBFRAME_LPC. */
  uint8_t subframeType;

  /* The number of wasted bits per sample as specified by the sub-frame header.
   */
  uint8_t wastedBitsPerSample;

  /* The order to use for the prediction stage for SUBFRAME_FIXED and
   * SUBFRAME_LPC. */
  uint8_t lpcOrder;

  /* A pointer to the buffer containing the decoded samples in the subframe.
   * This pointer is an offset from drflac::pExtraData. */
  int32_t* pSamplesS32;
} drflac_subframe;

typedef struct {
  /*
  If the stream uses variable block sizes, this will be set to the index of the
  first PCM frame. If fixed block sizes are used, this will always be set to 0.
  This is 64-bit because the decoded PCM frame number will be 36 bits.
  */
  uint64_t pcmFrameNumber;

  /*
  If the stream uses fixed block sizes, this will be set to the frame number. If
  variable block sizes are used, this will always be 0. This is 32-bit because
  in fixed block sizes, the maximum frame number will be 31 bits.
  */
  uint32_t flacFrameNumber;

  /* The sample rate of this frame. */
  uint32_t sampleRate;

  /* The number of PCM frames in each sub-frame within this frame. */
  uint16_t blockSizeInPCMFrames;

  /*
  The channel assignment of this frame. This is not always set to the channel
  count. If interchannel decorrelation is being used this will be set to
  DRFLAC_CHANNEL_ASSIGNMENT_LEFT_SIDE, DRFLAC_CHANNEL_ASSIGNMENT_RIGHT_SIDE or
  DRFLAC_CHANNEL_ASSIGNMENT_MID_SIDE.
  */
  uint8_t channelAssignment;

  /* The number of bits per sample within this frame. */
  uint8_t bitsPerSample;

  /* The frame's CRC. */
  uint8_t crc8;
} drflac_frame_header;

typedef struct {
  /* The header. */
  drflac_frame_header header;

  /*
  The number of PCM frames left to be read in this FLAC frame. This is initially
  set to the block size. As PCM frames are read, this will be decremented. When
  it reaches 0, the decoder will see this frame as fully consumed and load the
  next frame.
  */
  uint32_t pcmFramesRemaining;

  /* The list of sub-frames within the frame. There is one sub-frame for each
   * channel, and there's a maximum of 8 channels. */
  drflac_subframe subframes[8];
} drflac_frame;

typedef struct {
  /* The function to call when a metadata block is read. */
  drflac_meta_proc onMeta;

  /* The user data posted to the metadata callback function. */
  void* pUserDataMD;

  /* Memory allocation callbacks. */
  drlibs_allocation_callbacks allocationCallbacks;

  /* The sample rate. Will be set to something like 44100. */
  uint32_t sampleRate;

  /*
  The number of channels. This will be set to 1 for monaural streams, 2 for
  stereo, etc. Maximum 8. This is set based on the value specified in the
  STREAMINFO block.
  */
  uint8_t channels;

  /* The bits per sample. Will be set to something like 16, 24, etc. */
  uint8_t bitsPerSample;

  /* The maximum block size, in samples. This number represents the number of
   * samples in each channel (not combined). */
  uint16_t maxBlockSizeInPCMFrames;

  /*
  The total number of PCM Frames making up the stream. Can be 0 in which case
  it's still a valid stream, but just means the total PCM frame count is
  unknown. Likely the case with streams like internet radio.
  */
  uint64_t totalPCMFrameCount;

  /* The container type. This is set based on whether or not the decoder was
   * opened from a native or Ogg stream. */
  drflac_container container;

  /* The number of seekpoints in the seektable. */
  uint32_t seekpointCount;

  /* Information about the frame the decoder is currently sitting on. */
  drflac_frame currentFLACFrame;

  /* The index of the PCM frame the decoder is currently sitting on. This is
   * only used for seeking. */
  uint64_t currentPCMFrame;

  /* The position of the first FLAC frame in the stream. This is only ever used
   * for seeking. */
  uint64_t firstFLACFramePosInBytes;

  /* A hack to avoid a malloc() when opening a decoder with
   * drflac_open_memory(). */
  drflac__memory_stream memoryStream;

  /* A pointer to the decoded sample data. This is an offset of pExtraData. */
  int32_t* pDecodedSamples;

  /* A pointer to the seek table. This is an offset of pExtraData, or NULL if
   * there is no seek table. */
  drflac_seekpoint* pSeekpoints;

  /* Internal use only. Only used with Ogg containers. Points to a drflac_oggbs
   * object. This is an offset of pExtraData. */
  void* _oggbs;

  /* Internal use only. Used for profiling and testing different seeking modes.
   */
  bool _noSeekTableSeek : 1;
  bool _noBinarySearchSeek : 1;
  bool _noBruteForceSeek : 1;

  /* The bit streamer. The raw FLAC data is fed through this object. */
  drflac_bs bs;

  /* Variable length extra data. We attach this to the end of the object so we
   * can avoid unnecessary mallocs. */
  uint8_t pExtraData[1];
} drflac;

/*
Opens a FLAC decoder.


Parameters
----------
onRead (in)
    The function to call when data needs to be read from the client.

onSeek (in)
    The function to call when the read position of the client data needs to
move.

pUserData (in, optional)
    A pointer to application defined data that will be passed to onRead and
onSeek.

pAllocationCallbacks (in, optional)
    A pointer to application defined callbacks for managing memory allocations.


Return Value
------------
Returns a pointer to an object representing the decoder.


Remarks
-------
Close the decoder with `drflac_close()`.

`pAllocationCallbacks` can be NULL in which case it will use `DRFLAC_MALLOC`,
`DRFLAC_REALLOC` and `DRFLAC_FREE`.

This function will automatically detect whether or not you are attempting to
open a native or Ogg encapsulated FLAC, both of which should work seamlessly
without any manual intervention. Ogg encapsulation also works with multiplexed
streams which basically means it can play FLAC encoded audio tracks in videos.

This is the lowest level function for opening a FLAC stream. You can also use
`drflac_open_file()` and `drflac_open_memory()` to open the stream from a file
or from a block of memory respectively.

The STREAMINFO block must be present for this to succeed. Use
`drflac_open_relaxed()` to open a FLAC stream where the header may not be
present.

Use `drflac_open_with_metadata()` if you need access to metadata.


Seek Also
---------
drflac_open_file()
drflac_open_memory()
drflac_open_with_metadata()
drflac_close()
*/
DRFLAC_API drflac* drflac_open(drflac_read_proc onRead, drflac_seek_proc onSeek, void* pUserData,
                               const drlibs_allocation_callbacks* pAllocationCallbacks);

/*
Opens a FLAC stream with relaxed validation of the header block.


Parameters
----------
onRead (in)
    The function to call when data needs to be read from the client.

onSeek (in)
    The function to call when the read position of the client data needs to
move.

container (in)
    Whether or not the FLAC stream is encapsulated using standard FLAC
encapsulation or Ogg encapsulation.

pUserData (in, optional)
    A pointer to application defined data that will be passed to onRead and
onSeek.

pAllocationCallbacks (in, optional)
    A pointer to application defined callbacks for managing memory allocations.


Return Value
------------
A pointer to an object representing the decoder.


Remarks
-------
The same as drflac_open(), except attempts to open the stream even when a header
block is not present.

Because the header is not necessarily available, the caller must explicitly
define the container (Native or Ogg). Do not set this to
`drflac_container_unknown` as that is for internal use only.

Opening in relaxed mode will continue reading data from onRead until it finds a
valid frame. If a frame is never found it will continue forever. To abort, force
your `onRead` callback to return 0, which dr_flac will use as an indicator that
the end of the stream was found.

Use `drflac_open_with_metadata_relaxed()` if you need access to metadata.
*/
DRFLAC_API drflac* drflac_open_relaxed(drflac_read_proc onRead, drflac_seek_proc onSeek,
                                       drflac_container container, void* pUserData,
                                       const drlibs_allocation_callbacks* pAllocationCallbacks);

/*
Opens a FLAC decoder and notifies the caller of the metadata chunks (album art,
etc.).


Parameters
----------
onRead (in)
    The function to call when data needs to be read from the client.

onSeek (in)
    The function to call when the read position of the client data needs to
move.

onMeta (in)
    The function to call for every metadata block.

pUserData (in, optional)
    A pointer to application defined data that will be passed to onRead, onSeek
and onMeta.

pAllocationCallbacks (in, optional)
    A pointer to application defined callbacks for managing memory allocations.


Return Value
------------
A pointer to an object representing the decoder.


Remarks
-------
Close the decoder with `drflac_close()`.

`pAllocationCallbacks` can be NULL in which case it will use `DRFLAC_MALLOC`,
`DRFLAC_REALLOC` and `DRFLAC_FREE`.

This is slower than `drflac_open()`, so avoid this one if you don't need
metadata. Internally, this will allocate and free memory on the heap for every
metadata block except for STREAMINFO and PADDING blocks.

The caller is notified of the metadata via the `onMeta` callback. All metadata
blocks will be handled before the function returns. This callback takes a
pointer to a `drflac_metadata` object which is a union containing the data of
all relevant metadata blocks. Use the `type` member to discriminate against the
different metadata types.

The STREAMINFO block must be present for this to succeed. Use
`drflac_open_with_metadata_relaxed()` to open a FLAC stream where the header may
not be present.

Note that this will behave inconsistently with `drflac_open()` if the stream is
an Ogg encapsulated stream and a metadata block is corrupted. This is due to the
way the Ogg stream recovers from corrupted pages. When
`drflac_open_with_metadata()` is being used, the open routine will try to read
the contents of the metadata block, whereas `drflac_open()` will simply seek
past it (for the sake of efficiency). This inconsistency can result in different
samples being returned depending on whether or not the stream is being opened
with metadata.


Seek Also
---------
drflac_open_file_with_metadata()
drflac_open_memory_with_metadata()
drflac_open()
drflac_close()
*/
DRFLAC_API drflac*
drflac_open_with_metadata(drflac_read_proc onRead, drflac_seek_proc onSeek, drflac_meta_proc onMeta,
                          void* pUserData, const drlibs_allocation_callbacks* pAllocationCallbacks);

/*
The same as drflac_open_with_metadata(), except attempts to open the stream even
when a header block is not present.

See Also
--------
drflac_open_with_metadata()
drflac_open_relaxed()
*/
DRFLAC_API drflac*
drflac_open_with_metadata_relaxed(drflac_read_proc onRead, drflac_seek_proc onSeek,
                                  drflac_meta_proc onMeta, drflac_container container,
                                  void* pUserData,
                                  const drlibs_allocation_callbacks* pAllocationCallbacks);

/*
Closes the given FLAC decoder.


Parameters
----------
pFlac (in)
    The decoder to close.


Remarks
-------
This will destroy the decoder object.


See Also
--------
drflac_open()
drflac_open_with_metadata()
drflac_open_file()
drflac_open_file_w()
drflac_open_file_with_metadata()
drflac_open_file_with_metadata_w()
drflac_open_memory()
drflac_open_memory_with_metadata()
*/
DRFLAC_API void drflac_close(drflac* pFlac);

/*
Reads sample data from the given FLAC decoder, output as interleaved signed
32-bit PCM.


Parameters
----------
pFlac (in)
    The decoder.

framesToRead (in)
    The number of PCM frames to read.

pBufferOut (out, optional)
    A pointer to the buffer that will receive the decoded samples.


Return Value
------------
Returns the number of PCM frames actually read. If the return value is less than
`framesToRead` it has reached the end.


Remarks
-------
pBufferOut can be null, in which case the call will act as a seek, and the
return value will be the number of frames seeked.
*/
DRFLAC_API uint64_t drflac_read_pcm_frames_s32(drflac* pFlac, uint64_t framesToRead,
                                                    int32_t* pBufferOut);

/*
Reads sample data from the given FLAC decoder, output as interleaved signed
16-bit PCM.


Parameters
----------
pFlac (in)
    The decoder.

framesToRead (in)
    The number of PCM frames to read.

pBufferOut (out, optional)
    A pointer to the buffer that will receive the decoded samples.


Return Value
------------
Returns the number of PCM frames actually read. If the return value is less than
`framesToRead` it has reached the end.


Remarks
-------
pBufferOut can be null, in which case the call will act as a seek, and the
return value will be the number of frames seeked.

Note that this is lossy for streams where the bits per sample is larger than 16.
*/
DRFLAC_API uint64_t drflac_read_pcm_frames_s16(drflac* pFlac, uint64_t framesToRead,
                                                    int16_t* pBufferOut);

/*
Reads sample data from the given FLAC decoder, output as interleaved 32-bit
floating point PCM.


Parameters
----------
pFlac (in)
    The decoder.

framesToRead (in)
    The number of PCM frames to read.

pBufferOut (out, optional)
    A pointer to the buffer that will receive the decoded samples.


Return Value
------------
Returns the number of PCM frames actually read. If the return value is less than
`framesToRead` it has reached the end.


Remarks
-------
pBufferOut can be null, in which case the call will act as a seek, and the
return value will be the number of frames seeked.

Note that this should be considered lossy due to the nature of floating point
numbers not being able to exactly represent every possible number.
*/
DRFLAC_API uint64_t drflac_read_pcm_frames_f32(drflac* pFlac, uint64_t framesToRead,
                                                    float* pBufferOut);

/*
Seeks to the PCM frame at the given index.


Parameters
----------
pFlac (in)
    The decoder.

pcmFrameIndex (in)
    The index of the PCM frame to seek to. See notes below.


Return Value
-------------
`DRFLAC_TRUE` if successful; `DRFLAC_FALSE` otherwise.
*/
DRFLAC_API bool drflac_seek_to_pcm_frame(drflac* pFlac, uint64_t pcmFrameIndex);

#ifndef DR_FLAC_NO_STDIO
/*
Opens a FLAC decoder from the file at the given path.


Parameters
----------
pFileName (in)
    The path of the file to open, either absolute or relative to the current
directory.

pAllocationCallbacks (in, optional)
    A pointer to application defined callbacks for managing memory allocations.


Return Value
------------
A pointer to an object representing the decoder.


Remarks
-------
Close the decoder with drflac_close().


Remarks
-------
This will hold a handle to the file until the decoder is closed with
drflac_close(). Some platforms will restrict the number of files a process can
have open at any given time, so keep this mind if you have many decoders open at
the same time.


See Also
--------
drflac_open_file_with_metadata()
drflac_open()
drflac_close()
*/
DRFLAC_API drflac* drflac_open_file(const char* pFileName,
                                    const drlibs_allocation_callbacks* pAllocationCallbacks);
DRFLAC_API drflac* drflac_open_file_w(const wchar_t* pFileName,
                                      const drlibs_allocation_callbacks* pAllocationCallbacks);

/*
Opens a FLAC decoder from the file at the given path and notifies the caller of
the metadata chunks (album art, etc.)


Parameters
----------
pFileName (in)
    The path of the file to open, either absolute or relative to the current
directory.

pAllocationCallbacks (in, optional)
    A pointer to application defined callbacks for managing memory allocations.

onMeta (in)
    The callback to fire for each metadata block.

pUserData (in)
    A pointer to the user data to pass to the metadata callback.

pAllocationCallbacks (in)
    A pointer to application defined callbacks for managing memory allocations.


Remarks
-------
Look at the documentation for drflac_open_with_metadata() for more information
on how metadata is handled.


See Also
--------
drflac_open_with_metadata()
drflac_open()
drflac_close()
*/
DRFLAC_API drflac*
drflac_open_file_with_metadata(const char* pFileName, drflac_meta_proc onMeta, void* pUserData,
                               const drlibs_allocation_callbacks* pAllocationCallbacks);
DRFLAC_API drflac*
drflac_open_file_with_metadata_w(const wchar_t* pFileName, drflac_meta_proc onMeta, void* pUserData,
                                 const drlibs_allocation_callbacks* pAllocationCallbacks);
#endif

/*
Opens a FLAC decoder from a pre-allocated block of memory


Parameters
----------
pData (in)
    A pointer to the raw encoded FLAC data.

dataSize (in)
    The size in bytes of `data`.

pAllocationCallbacks (in)
    A pointer to application defined callbacks for managing memory allocations.


Return Value
------------
A pointer to an object representing the decoder.


Remarks
-------
This does not create a copy of the data. It is up to the application to ensure
the buffer remains valid for the lifetime of the decoder.


See Also
--------
drflac_open()
drflac_close()
*/
DRFLAC_API drflac* drflac_open_memory(const void* pData, size_t dataSize,
                                      const drlibs_allocation_callbacks* pAllocationCallbacks);

/*
Opens a FLAC decoder from a pre-allocated block of memory and notifies the
caller of the metadata chunks (album art, etc.)


Parameters
----------
pData (in)
    A pointer to the raw encoded FLAC data.

dataSize (in)
    The size in bytes of `data`.

onMeta (in)
    The callback to fire for each metadata block.

pUserData (in)
    A pointer to the user data to pass to the metadata callback.

pAllocationCallbacks (in)
    A pointer to application defined callbacks for managing memory allocations.


Remarks
-------
Look at the documentation for drflac_open_with_metadata() for more information
on how metadata is handled.


See Also
-------
drflac_open_with_metadata()
drflac_open()
drflac_close()
*/
DRFLAC_API drflac*
drflac_open_memory_with_metadata(const void* pData, size_t dataSize, drflac_meta_proc onMeta,
                                 void* pUserData,
                                 const drlibs_allocation_callbacks* pAllocationCallbacks);

/* High Level APIs */

/*
Opens a FLAC stream from the given callbacks and fully decodes it in a single
operation. The return value is a pointer to the sample data as interleaved
signed 32-bit PCM. The returned data must be freed with drflac_free().

You can pass in custom memory allocation callbacks via the pAllocationCallbacks
parameter. This can be NULL in which case it will use DRFLAC_MALLOC,
DRFLAC_REALLOC and DRFLAC_FREE.

Sometimes a FLAC file won't keep track of the total sample count. In this
situation the function will continuously read samples into a dynamically sized
buffer on the heap until no samples are left.

Do not call this function on a broadcast type of stream (like internet radio
streams and whatnot).
*/
DRFLAC_API int32_t*
drflac_open_and_read_pcm_frames_s32(drflac_read_proc onRead, drflac_seek_proc onSeek,
                                    void* pUserData, unsigned int* channels,
                                    unsigned int* sampleRate, uint64_t* totalPCMFrameCount,
                                    const drlibs_allocation_callbacks* pAllocationCallbacks);

/* Same as drflac_open_and_read_pcm_frames_s32(), except returns signed 16-bit
 * integer samples. */
DRFLAC_API int16_t*
drflac_open_and_read_pcm_frames_s16(drflac_read_proc onRead, drflac_seek_proc onSeek,
                                    void* pUserData, unsigned int* channels,
                                    unsigned int* sampleRate, uint64_t* totalPCMFrameCount,
                                    const drlibs_allocation_callbacks* pAllocationCallbacks);

/* Same as drflac_open_and_read_pcm_frames_s32(), except returns 32-bit
 * floating-point samples. */
DRFLAC_API float*
drflac_open_and_read_pcm_frames_f32(drflac_read_proc onRead, drflac_seek_proc onSeek,
                                    void* pUserData, unsigned int* channels,
                                    unsigned int* sampleRate, uint64_t* totalPCMFrameCount,
                                    const drlibs_allocation_callbacks* pAllocationCallbacks);

#ifndef DR_FLAC_NO_STDIO
/* Same as drflac_open_and_read_pcm_frames_s32() except opens the decoder from a
 * file. */
DRFLAC_API int32_t* drflac_open_file_and_read_pcm_frames_s32(
    const char* filename, unsigned int* channels, unsigned int* sampleRate,
    uint64_t* totalPCMFrameCount, const drlibs_allocation_callbacks* pAllocationCallbacks);

/* Same as drflac_open_file_and_read_pcm_frames_s32(), except returns signed
 * 16-bit integer samples. */
DRFLAC_API int16_t* drflac_open_file_and_read_pcm_frames_s16(
    const char* filename, unsigned int* channels, unsigned int* sampleRate,
    uint64_t* totalPCMFrameCount, const drlibs_allocation_callbacks* pAllocationCallbacks);

/* Same as drflac_open_file_and_read_pcm_frames_s32(), except returns 32-bit
 * floating-point samples. */
DRFLAC_API float* drflac_open_file_and_read_pcm_frames_f32(
    const char* filename, unsigned int* channels, unsigned int* sampleRate,
    uint64_t* totalPCMFrameCount, const drlibs_allocation_callbacks* pAllocationCallbacks);
#endif

/* Same as drflac_open_and_read_pcm_frames_s32() except opens the decoder from a
 * block of memory. */
DRFLAC_API int32_t* drflac_open_memory_and_read_pcm_frames_s32(
    const void* data, size_t dataSize, unsigned int* channels, unsigned int* sampleRate,
    uint64_t* totalPCMFrameCount, const drlibs_allocation_callbacks* pAllocationCallbacks);

/* Same as drflac_open_memory_and_read_pcm_frames_s32(), except returns signed
 * 16-bit integer samples. */
DRFLAC_API int16_t* drflac_open_memory_and_read_pcm_frames_s16(
    const void* data, size_t dataSize, unsigned int* channels, unsigned int* sampleRate,
    uint64_t* totalPCMFrameCount, const drlibs_allocation_callbacks* pAllocationCallbacks);

/* Same as drflac_open_memory_and_read_pcm_frames_s32(), except returns 32-bit
 * floating-point samples. */
DRFLAC_API float* drflac_open_memory_and_read_pcm_frames_f32(
    const void* data, size_t dataSize, unsigned int* channels, unsigned int* sampleRate,
    uint64_t* totalPCMFrameCount, const drlibs_allocation_callbacks* pAllocationCallbacks);

/*
Frees memory that was allocated internally by dr_flac.

Set pAllocationCallbacks to the same object that was passed to
drflac_open_*_and_read_pcm_frames_*(). If you originally passed in NULL, pass in
NULL for this.
*/
DRFLAC_API void drflac_free(void* p, const drlibs_allocation_callbacks* pAllocationCallbacks);

/* Structure representing an iterator for vorbis comments in a VORBIS_COMMENT
 * metadata block. */
typedef struct {
  uint32_t countRemaining;
  const char* pRunningData;
} drflac_vorbis_comment_iterator;

/*
Initializes a vorbis comment iterator. This can be used for iterating over the
vorbis comments in a VORBIS_COMMENT metadata block.
*/
DRFLAC_API void drflac_init_vorbis_comment_iterator(drflac_vorbis_comment_iterator* pIter,
                                                    uint32_t commentCount,
                                                    const void* pComments);

/*
Goes to the next vorbis comment in the given iterator. If null is returned it
means there are no more comments. The returned string is NOT null terminated.
*/
DRFLAC_API const char* drflac_next_vorbis_comment(drflac_vorbis_comment_iterator* pIter,
                                                  uint32_t* pCommentLengthOut);

/* Structure representing an iterator for cuesheet tracks in a CUESHEET metadata
 * block. */
typedef struct {
  uint32_t countRemaining;
  const char* pRunningData;
} drflac_cuesheet_track_iterator;

/* Packing is important on this structure because we map this directly to the
 * raw data within the CUESHEET metadata block. */
#pragma pack(4)
typedef struct {
  uint64_t offset;
  uint8_t index;
  uint8_t reserved[3];
} drflac_cuesheet_track_index;
#pragma pack()

typedef struct {
  uint64_t offset;
  uint8_t trackNumber;
  char ISRC[12];
  bool isAudio;
  bool preEmphasis;
  uint8_t indexCount;
  const drflac_cuesheet_track_index* pIndexPoints;
} drflac_cuesheet_track;

/*
Initializes a cuesheet track iterator. This can be used for iterating over the
cuesheet tracks in a CUESHEET metadata block.
*/
DRFLAC_API void drflac_init_cuesheet_track_iterator(drflac_cuesheet_track_iterator* pIter,
                                                    uint32_t trackCount,
                                                    const void* pTrackData);

/* Goes to the next cuesheet track in the given iterator. If DRFLAC_FALSE is
 * returned it means there are no more comments. */
DRFLAC_API bool drflac_next_cuesheet_track(drflac_cuesheet_track_iterator* pIter,
                                                    drflac_cuesheet_track* pCuesheetTrack);

#ifdef __cplusplus
}
#endif
#endif /* dr_flac_h */

/*
This software is available as a choice of the following licenses. Choose
whichever you prefer.

===============================================================================
ALTERNATIVE 1 - Public Domain (www.unlicense.org)
===============================================================================
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.

In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>

===============================================================================
ALTERNATIVE 2 - MIT No Attribution
===============================================================================
Copyright 2020 David Reid

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
