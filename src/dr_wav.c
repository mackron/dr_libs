#include <dr_libs/dr_wav.h>

#include <limits.h> /* For INT_MAX */
#include <stdlib.h>
#include <string.h> /* For memcpy(), memset() */

#ifndef DR_WAV_NO_STDIO
#include <stdio.h>
#include <wchar.h>
#endif

/* Standard library stuff. */
#ifndef DRWAV_ASSERT
#include <assert.h>
#define DRWAV_ASSERT(expression) assert(expression)
#endif
#ifndef DRWAV_MALLOC
#define DRWAV_MALLOC(sz) malloc((sz))
#endif
#ifndef DRWAV_REALLOC
#define DRWAV_REALLOC(p, sz) realloc((p), (sz))
#endif
#ifndef DRWAV_FREE
#define DRWAV_FREE(p) free((p))
#endif
#ifndef DRWAV_COPY_MEMORY
#define DRWAV_COPY_MEMORY(dst, src, sz) memcpy((dst), (src), (sz))
#endif
#ifndef DRWAV_ZERO_MEMORY
#define DRWAV_ZERO_MEMORY(p, sz) memset((p), 0, (sz))
#endif
#ifndef DRWAV_ZERO_OBJECT
#define DRWAV_ZERO_OBJECT(p) DRWAV_ZERO_MEMORY((p), sizeof(*p))
#endif

#define drwav_countof(x) (sizeof(x) / sizeof(x[0]))
#define drwav_align(x, a) ((((x) + (a)-1) / (a)) * (a))
#define drwav_min(a, b) (((a) < (b)) ? (a) : (b))
#define drwav_max(a, b) (((a) > (b)) ? (a) : (b))
#define drwav_clamp(x, lo, hi) (drwav_max((lo), drwav_min((hi), (x))))

#define DRWAV_MAX_SIMD_VECTOR_SIZE 64 /* 64 for AVX-512 in the future. */

/* CPU architecture. */
#if defined(__x86_64__) || defined(_M_X64)
#define DRWAV_X64
#elif defined(__i386) || defined(_M_IX86)
#define DRWAV_X86
#elif defined(__arm__) || defined(_M_ARM)
#define DRWAV_ARM
#endif

#ifdef _MSC_VER
#define DRWAV_INLINE __forceinline
#elif defined(__GNUC__)
/*
I've had a bug report where GCC is emitting warnings about functions possibly
not being inlineable. This warning happens when the
__attribute__((always_inline)) attribute is defined without an "inline"
statement. I think therefore there must be some case where "__inline__" is not
always defined, thus the compiler emitting these warnings. When using -std=c89
or -ansi on the command line, we cannot use the "inline" keyword and instead
need to use "__inline__". In an attempt to work around this issue I am using
"__inline__" only when we're compiling in strict ANSI mode.
*/
#if defined(__STRICT_ANSI__)
#define DRWAV_INLINE __inline__ __attribute__((always_inline))
#else
#define DRWAV_INLINE inline __attribute__((always_inline))
#endif
#elif defined(__WATCOMC__)
#define DRWAV_INLINE __inline
#else
#define DRWAV_INLINE
#endif

#if defined(SIZE_MAX)
#define DRWAV_SIZE_MAX SIZE_MAX
#else
#if defined(_WIN64) || defined(_LP64) || defined(__LP64__)
#define DRWAV_SIZE_MAX ((uint64_t)0xFFFFFFFFFFFFFFFF)
#else
#define DRWAV_SIZE_MAX 0xFFFFFFFF
#endif
#endif

#if defined(_MSC_VER) && _MSC_VER >= 1400
#define DRWAV_HAS_BYTESWAP16_INTRINSIC
#define DRWAV_HAS_BYTESWAP32_INTRINSIC
#define DRWAV_HAS_BYTESWAP64_INTRINSIC
#elif defined(__clang__)
#if defined(__has_builtin)
#if __has_builtin(__builtin_bswap16)
#define DRWAV_HAS_BYTESWAP16_INTRINSIC
#endif
#if __has_builtin(__builtin_bswap32)
#define DRWAV_HAS_BYTESWAP32_INTRINSIC
#endif
#if __has_builtin(__builtin_bswap64)
#define DRWAV_HAS_BYTESWAP64_INTRINSIC
#endif
#endif
#elif defined(__GNUC__)
#if ((__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3))
#define DRWAV_HAS_BYTESWAP32_INTRINSIC
#define DRWAV_HAS_BYTESWAP64_INTRINSIC
#endif
#if ((__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8))
#define DRWAV_HAS_BYTESWAP16_INTRINSIC
#endif
#endif

DRWAV_API void drwav_version(uint32_t* pMajor, uint32_t* pMinor, uint32_t* pRevision) {
  if (pMajor) { *pMajor = DRWAV_VERSION_MAJOR; }

  if (pMinor) { *pMinor = DRWAV_VERSION_MINOR; }

  if (pRevision) { *pRevision = DRWAV_VERSION_REVISION; }
}

DRWAV_API const char* drwav_version_string(void) { return DRWAV_VERSION_STRING; }

/*
These limits are used for basic validation when initializing the decoder. If you
exceed these limits, first of all: what on Earth are you doing?! (Let me know,
I'd be curious!) Second, you can adjust these by #define-ing them before the
dr_wav implementation.
*/
#ifndef DRWAV_MAX_SAMPLE_RATE
#define DRWAV_MAX_SAMPLE_RATE 384000
#endif
#ifndef DRWAV_MAX_CHANNELS
#define DRWAV_MAX_CHANNELS 256
#endif
#ifndef DRWAV_MAX_BITS_PER_SAMPLE
#define DRWAV_MAX_BITS_PER_SAMPLE 64
#endif

static const uint8_t drwavGUID_W64_RIFF[16] = {
    0x72, 0x69, 0x66, 0x66, 0x2E, 0x91, 0xCF, 0x11, 0xA5,
    0xD6, 0x28, 0xDB, 0x04, 0xC1, 0x00, 0x00}; /* 66666972-912E-11CF-A5D6-28DB04C10000
                                                */
static const uint8_t drwavGUID_W64_WAVE[16] = {
    0x77, 0x61, 0x76, 0x65, 0xF3, 0xAC, 0xD3, 0x11, 0x8C,
    0xD1, 0x00, 0xC0, 0x4F, 0x8E, 0xDB, 0x8A}; /* 65766177-ACF3-11D3-8CD1-00C04F8EDB8A
                                                */
/*static const uint8_t drwavGUID_W64_JUNK[16] = {0x6A,0x75,0x6E,0x6B,
 * 0xF3,0xAC, 0xD3,0x11, 0x8C,0xD1, 0x00,0xC0,0x4F,0x8E,0xDB,0x8A};*/    /* 6B6E756A-ACF3-11D3-8CD1-00C04F8EDB8A */
static const uint8_t drwavGUID_W64_FMT[16] = {
    0x66, 0x6D, 0x74, 0x20, 0xF3, 0xAC, 0xD3, 0x11, 0x8C,
    0xD1, 0x00, 0xC0, 0x4F, 0x8E, 0xDB, 0x8A}; /* 20746D66-ACF3-11D3-8CD1-00C04F8EDB8A
                                                */
static const uint8_t drwavGUID_W64_FACT[16] = {
    0x66, 0x61, 0x63, 0x74, 0xF3, 0xAC, 0xD3, 0x11, 0x8C,
    0xD1, 0x00, 0xC0, 0x4F, 0x8E, 0xDB, 0x8A}; /* 74636166-ACF3-11D3-8CD1-00C04F8EDB8A
                                                */
static const uint8_t drwavGUID_W64_DATA[16] = {
    0x64, 0x61, 0x74, 0x61, 0xF3, 0xAC, 0xD3, 0x11, 0x8C,
    0xD1, 0x00, 0xC0, 0x4F, 0x8E, 0xDB, 0x8A}; /* 61746164-ACF3-11D3-8CD1-00C04F8EDB8A
                                                */
/*static const uint8_t drwavGUID_W64_SMPL[16] = {0x73,0x6D,0x70,0x6C,
 * 0xF3,0xAC, 0xD3,0x11, 0x8C,0xD1, 0x00,0xC0,0x4F,0x8E,0xDB,0x8A};*/    /* 6C706D73-ACF3-11D3-8CD1-00C04F8EDB8A */

static DRWAV_INLINE int drwav__is_little_endian(void) {
#if defined(DRWAV_X86) || defined(DRWAV_X64)
  return true;
#elif defined(__BYTE_ORDER) && defined(__LITTLE_ENDIAN) && __BYTE_ORDER == __LITTLE_ENDIAN
  return true;
#else
  int n = 1;
  return (*(char*)&n) == 1;
#endif
}

static DRWAV_INLINE void drwav_bytes_to_guid(const uint8_t* data, uint8_t* guid) {
  int i;
  for (i = 0; i < 16; ++i) { guid[i] = data[i]; }
}

static DRWAV_INLINE uint16_t drwav__bswap16(uint16_t n) {
#ifdef DRWAV_HAS_BYTESWAP16_INTRINSIC
#if defined(_MSC_VER)
  return _byteswap_ushort(n);
#elif defined(__GNUC__) || defined(__clang__)
  return __builtin_bswap16(n);
#else
#error "This compiler does not support the byte swap intrinsic."
#endif
#else
  return ((n & 0xFF00) >> 8) | ((n & 0x00FF) << 8);
#endif
}

static DRWAV_INLINE uint32_t drwav__bswap32(uint32_t n) {
#ifdef DRWAV_HAS_BYTESWAP32_INTRINSIC
#if defined(_MSC_VER)
  return _byteswap_ulong(n);
#elif defined(__GNUC__) || defined(__clang__)
#if defined(DRWAV_ARM) && (defined(__ARM_ARCH) && __ARM_ARCH >= 6) &&                              \
    !defined(DRWAV_64BIT) /* <-- 64-bit inline assembly has not been tested,                       \
                             so disabling for now. */
  /* Inline assembly optimized implementation for ARM. In my testing, GCC does
   * not generate optimized code with __builtin_bswap32(). */
  uint32_t r;
  __asm__ __volatile__(
#if defined(DRWAV_64BIT)
      "rev %w[out], %w[in]"
      : [out] "=r"(r)
      : [in] "r"(n) /* <-- This is untested. If someone in the community could
                       test this, that would be appreciated! */
#else
      "rev %[out], %[in]"
      : [out] "=r"(r)
      : [in] "r"(n)
#endif
  );
  return r;
#else
  return __builtin_bswap32(n);
#endif
#else
#error "This compiler does not support the byte swap intrinsic."
#endif
#else
  return ((n & 0xFF000000) >> 24) | ((n & 0x00FF0000) >> 8) | ((n & 0x0000FF00) << 8) |
         ((n & 0x000000FF) << 24);
#endif
}

static DRWAV_INLINE uint64_t drwav__bswap64(uint64_t n) {
#ifdef DRWAV_HAS_BYTESWAP64_INTRINSIC
#if defined(_MSC_VER)
  return _byteswap_uint64(n);
#elif defined(__GNUC__) || defined(__clang__)
  return __builtin_bswap64(n);
#else
#error "This compiler does not support the byte swap intrinsic."
#endif
#else
  /* Weird "<< 32" bitshift is required for C89 because it doesn't support
   * 64-bit constants. Should be optimized out by a good compiler. */
  return ((n & ((uint64_t)0xFF000000 << 32)) >> 56) | ((n & ((uint64_t)0x00FF0000 << 32)) >> 40) |
         ((n & ((uint64_t)0x0000FF00 << 32)) >> 24) | ((n & ((uint64_t)0x000000FF << 32)) >> 8) |
         ((n & ((uint64_t)0xFF000000)) << 8) | ((n & ((uint64_t)0x00FF0000)) << 24) |
         ((n & ((uint64_t)0x0000FF00)) << 40) | ((n & ((uint64_t)0x000000FF)) << 56);
#endif
}

static DRWAV_INLINE int16_t drwav__bswap_s16(int16_t n) {
  return (int16_t)drwav__bswap16((uint16_t)n);
}

static DRWAV_INLINE void drwav__bswap_samples_s16(int16_t* pSamples, uint64_t sampleCount) {
  uint64_t iSample;
  for (iSample = 0; iSample < sampleCount; iSample += 1) {
    pSamples[iSample] = drwav__bswap_s16(pSamples[iSample]);
  }
}

static DRWAV_INLINE void drwav__bswap_s24(uint8_t* p) {
  uint8_t t;
  t = p[0];
  p[0] = p[2];
  p[2] = t;
}

static DRWAV_INLINE void drwav__bswap_samples_s24(uint8_t* pSamples, uint64_t sampleCount) {
  uint64_t iSample;
  for (iSample = 0; iSample < sampleCount; iSample += 1) {
    uint8_t* pSample = pSamples + (iSample * 3);
    drwav__bswap_s24(pSample);
  }
}

static DRWAV_INLINE int32_t drwav__bswap_s32(int32_t n) {
  return (int32_t)drwav__bswap32((uint32_t)n);
}

static DRWAV_INLINE void drwav__bswap_samples_s32(int32_t* pSamples, uint64_t sampleCount) {
  uint64_t iSample;
  for (iSample = 0; iSample < sampleCount; iSample += 1) {
    pSamples[iSample] = drwav__bswap_s32(pSamples[iSample]);
  }
}

static DRWAV_INLINE float drwav__bswap_f32(float n) {
  union {
    uint32_t i;
    float f;
  } x;
  x.f = n;
  x.i = drwav__bswap32(x.i);

  return x.f;
}

static DRWAV_INLINE void drwav__bswap_samples_f32(float* pSamples, uint64_t sampleCount) {
  uint64_t iSample;
  for (iSample = 0; iSample < sampleCount; iSample += 1) {
    pSamples[iSample] = drwav__bswap_f32(pSamples[iSample]);
  }
}

static DRWAV_INLINE double drwav__bswap_f64(double n) {
  union {
    uint64_t i;
    double f;
  } x;
  x.f = n;
  x.i = drwav__bswap64(x.i);

  return x.f;
}

static DRWAV_INLINE void drwav__bswap_samples_f64(double* pSamples, uint64_t sampleCount) {
  uint64_t iSample;
  for (iSample = 0; iSample < sampleCount; iSample += 1) {
    pSamples[iSample] = drwav__bswap_f64(pSamples[iSample]);
  }
}

static DRWAV_INLINE void drwav__bswap_samples_pcm(void* pSamples, uint64_t sampleCount,
                                                  uint32_t bytesPerSample) {
  /* Assumes integer PCM. Floating point PCM is done in
   * drwav__bswap_samples_ieee(). */
  switch (bytesPerSample) {
  case 2: /* s16, s12 (loosely packed) */
  {
    drwav__bswap_samples_s16((int16_t*)pSamples, sampleCount);
  } break;
  case 3: /* s24 */
  {
    drwav__bswap_samples_s24((uint8_t*)pSamples, sampleCount);
  } break;
  case 4: /* s32 */
  {
    drwav__bswap_samples_s32((int32_t*)pSamples, sampleCount);
  } break;
  default: {
    /* Unsupported format. */
    DRWAV_ASSERT(false);
  } break;
  }
}

static DRWAV_INLINE void drwav__bswap_samples_ieee(void* pSamples, uint64_t sampleCount,
                                                   uint32_t bytesPerSample) {
  switch (bytesPerSample) {
#if 0 /* Contributions welcome for f16 support. */
        case 2: /* f16 */
        {
            drwav__bswap_samples_f16((drwav_float16*)pSamples, sampleCount);
        } break;
#endif
  case 4: /* f32 */
  {
    drwav__bswap_samples_f32((float*)pSamples, sampleCount);
  } break;
  case 8: /* f64 */
  {
    drwav__bswap_samples_f64((double*)pSamples, sampleCount);
  } break;
  default: {
    /* Unsupported format. */
    DRWAV_ASSERT(false);
  } break;
  }
}

static DRWAV_INLINE void drwav__bswap_samples(void* pSamples, uint64_t sampleCount,
                                              uint32_t bytesPerSample, uint16_t format) {
  switch (format) {
  case DR_WAVE_FORMAT_PCM: {
    drwav__bswap_samples_pcm(pSamples, sampleCount, bytesPerSample);
  } break;

  case DR_WAVE_FORMAT_IEEE_FLOAT: {
    drwav__bswap_samples_ieee(pSamples, sampleCount, bytesPerSample);
  } break;

  case DR_WAVE_FORMAT_ALAW:
  case DR_WAVE_FORMAT_MULAW: {
    drwav__bswap_samples_s16((int16_t*)pSamples, sampleCount);
  } break;

  case DR_WAVE_FORMAT_ADPCM:
  case DR_WAVE_FORMAT_DVI_ADPCM:
  default: {
    /* Unsupported format. */
    DRWAV_ASSERT(false);
  } break;
  }
}

DRWAV_PRIVATE void* drwav__malloc_default(size_t sz, void* pUserData) {
  (void)pUserData;
  return DRWAV_MALLOC(sz);
}

DRWAV_PRIVATE void* drwav__realloc_default(void* p, size_t sz, void* pUserData) {
  (void)pUserData;
  return DRWAV_REALLOC(p, sz);
}

DRWAV_PRIVATE void drwav__free_default(void* p, void* pUserData) {
  (void)pUserData;
  DRWAV_FREE(p);
}

DRWAV_PRIVATE void*
drwav__malloc_from_callbacks(size_t sz, const drwav_allocation_callbacks* pAllocationCallbacks) {
  if (pAllocationCallbacks == NULL) { return NULL; }

  if (pAllocationCallbacks->onMalloc != NULL) {
    return pAllocationCallbacks->onMalloc(sz, pAllocationCallbacks->pUserData);
  }

  /* Try using realloc(). */
  if (pAllocationCallbacks->onRealloc != NULL) {
    return pAllocationCallbacks->onRealloc(NULL, sz, pAllocationCallbacks->pUserData);
  }

  return NULL;
}

DRWAV_PRIVATE void*
drwav__realloc_from_callbacks(void* p, size_t szNew, size_t szOld,
                              const drwav_allocation_callbacks* pAllocationCallbacks) {
  if (pAllocationCallbacks == NULL) { return NULL; }

  if (pAllocationCallbacks->onRealloc != NULL) {
    return pAllocationCallbacks->onRealloc(p, szNew, pAllocationCallbacks->pUserData);
  }

  /* Try emulating realloc() in terms of malloc()/free(). */
  if (pAllocationCallbacks->onMalloc != NULL && pAllocationCallbacks->onFree != NULL) {
    void* p2;

    p2 = pAllocationCallbacks->onMalloc(szNew, pAllocationCallbacks->pUserData);
    if (p2 == NULL) { return NULL; }

    if (p != NULL) {
      DRWAV_COPY_MEMORY(p2, p, szOld);
      pAllocationCallbacks->onFree(p, pAllocationCallbacks->pUserData);
    }

    return p2;
  }

  return NULL;
}

DRWAV_PRIVATE void
drwav__free_from_callbacks(void* p, const drwav_allocation_callbacks* pAllocationCallbacks) {
  if (p == NULL || pAllocationCallbacks == NULL) { return; }

  if (pAllocationCallbacks->onFree != NULL) {
    pAllocationCallbacks->onFree(p, pAllocationCallbacks->pUserData);
  }
}

DRWAV_PRIVATE drwav_allocation_callbacks drwav_copy_allocation_callbacks_or_defaults(
    const drwav_allocation_callbacks* pAllocationCallbacks) {
  if (pAllocationCallbacks != NULL) {
    /* Copy. */
    return *pAllocationCallbacks;
  } else {
    /* Defaults. */
    drwav_allocation_callbacks allocationCallbacks;
    allocationCallbacks.pUserData = NULL;
    allocationCallbacks.onMalloc = drwav__malloc_default;
    allocationCallbacks.onRealloc = drwav__realloc_default;
    allocationCallbacks.onFree = drwav__free_default;
    return allocationCallbacks;
  }
}

static DRWAV_INLINE bool drwav__is_compressed_format_tag(uint16_t formatTag) {
  return formatTag == DR_WAVE_FORMAT_ADPCM || formatTag == DR_WAVE_FORMAT_DVI_ADPCM;
}

DRWAV_PRIVATE unsigned int drwav__chunk_padding_size_riff(uint64_t chunkSize) {
  return (unsigned int)(chunkSize % 2);
}

DRWAV_PRIVATE unsigned int drwav__chunk_padding_size_w64(uint64_t chunkSize) {
  return (unsigned int)(chunkSize % 8);
}

DRWAV_PRIVATE uint64_t drwav_read_pcm_frames_s16__msadpcm(drwav* pWav, uint64_t samplesToRead,
                                                          int16_t* pBufferOut);
DRWAV_PRIVATE uint64_t drwav_read_pcm_frames_s16__ima(drwav* pWav, uint64_t samplesToRead,
                                                      int16_t* pBufferOut);
DRWAV_PRIVATE bool drwav_init_write__internal(drwav* pWav, const drwav_data_format* pFormat,
                                              uint64_t totalSampleCount);

DRWAV_PRIVATE drwav_result drwav__read_chunk_header(drwav_read_proc onRead, void* pUserData,
                                                    drwav_container container,
                                                    uint64_t* pRunningBytesReadOut,
                                                    drwav_chunk_header* pHeaderOut) {
  if (container == drwav_container_riff || container == drwav_container_rf64) {
    uint8_t sizeInBytes[4];

    if (onRead(pUserData, pHeaderOut->id.fourcc, 4) != 4) { return DRWAV_AT_END; }

    if (onRead(pUserData, sizeInBytes, 4) != 4) { return DRWAV_INVALID_FILE; }

    pHeaderOut->sizeInBytes = drwav_bytes_to_u32(sizeInBytes);
    pHeaderOut->paddingSize = drwav__chunk_padding_size_riff(pHeaderOut->sizeInBytes);
    *pRunningBytesReadOut += 8;
  } else {
    uint8_t sizeInBytes[8];

    if (onRead(pUserData, pHeaderOut->id.guid, 16) != 16) { return DRWAV_AT_END; }

    if (onRead(pUserData, sizeInBytes, 8) != 8) { return DRWAV_INVALID_FILE; }

    pHeaderOut->sizeInBytes = drwav_bytes_to_u64(sizeInBytes) -
                              24; /* <-- Subtract 24 because w64 includes the size of the header. */
    pHeaderOut->paddingSize = drwav__chunk_padding_size_w64(pHeaderOut->sizeInBytes);
    *pRunningBytesReadOut += 24;
  }

  return DRWAV_SUCCESS;
}

DRWAV_PRIVATE bool drwav__seek_forward(drwav_seek_proc onSeek, uint64_t offset, void* pUserData) {
  uint64_t bytesRemainingToSeek = offset;
  while (bytesRemainingToSeek > 0) {
    if (bytesRemainingToSeek > 0x7FFFFFFF) {
      if (!onSeek(pUserData, 0x7FFFFFFF, drwav_seek_origin_current)) { return false; }
      bytesRemainingToSeek -= 0x7FFFFFFF;
    } else {
      if (!onSeek(pUserData, (int)bytesRemainingToSeek, drwav_seek_origin_current)) {
        return false;
      }
      bytesRemainingToSeek = 0;
    }
  }

  return true;
}

DRWAV_PRIVATE bool drwav__seek_from_start(drwav_seek_proc onSeek, uint64_t offset,
                                          void* pUserData) {
  if (offset <= 0x7FFFFFFF) { return onSeek(pUserData, (int)offset, drwav_seek_origin_start); }

  /* Larger than 32-bit seek. */
  if (!onSeek(pUserData, 0x7FFFFFFF, drwav_seek_origin_start)) { return false; }
  offset -= 0x7FFFFFFF;

  for (;;) {
    if (offset <= 0x7FFFFFFF) { return onSeek(pUserData, (int)offset, drwav_seek_origin_current); }

    if (!onSeek(pUserData, 0x7FFFFFFF, drwav_seek_origin_current)) { return false; }
    offset -= 0x7FFFFFFF;
  }

  /* Should never get here. */
  /*return true; */
}

DRWAV_PRIVATE bool drwav__read_fmt(drwav_read_proc onRead, drwav_seek_proc onSeek, void* pUserData,
                                   drwav_container container, uint64_t* pRunningBytesReadOut,
                                   drwav_fmt* fmtOut) {
  drwav_chunk_header header;
  uint8_t fmt[16];

  if (drwav__read_chunk_header(onRead, pUserData, container, pRunningBytesReadOut, &header) !=
      DRWAV_SUCCESS) {
    return false;
  }

  /* Skip non-fmt chunks. */
  while (
      ((container == drwav_container_riff || container == drwav_container_rf64) &&
       !drwav_fourcc_equal(header.id.fourcc, "fmt ")) ||
      (container == drwav_container_w64 && !drwav_guid_equal(header.id.guid, drwavGUID_W64_FMT))) {
    if (!drwav__seek_forward(onSeek, header.sizeInBytes + header.paddingSize, pUserData)) {
      return false;
    }
    *pRunningBytesReadOut += header.sizeInBytes + header.paddingSize;

    /* Try the next header. */
    if (drwav__read_chunk_header(onRead, pUserData, container, pRunningBytesReadOut, &header) !=
        DRWAV_SUCCESS) {
      return false;
    }
  }

  /* Validation. */
  if (container == drwav_container_riff || container == drwav_container_rf64) {
    if (!drwav_fourcc_equal(header.id.fourcc, "fmt ")) { return false; }
  } else {
    if (!drwav_guid_equal(header.id.guid, drwavGUID_W64_FMT)) { return false; }
  }

  if (onRead(pUserData, fmt, sizeof(fmt)) != sizeof(fmt)) { return false; }
  *pRunningBytesReadOut += sizeof(fmt);

  fmtOut->formatTag = drwav_bytes_to_u16(fmt + 0);
  fmtOut->channels = drwav_bytes_to_u16(fmt + 2);
  fmtOut->sampleRate = drwav_bytes_to_u32(fmt + 4);
  fmtOut->avgBytesPerSec = drwav_bytes_to_u32(fmt + 8);
  fmtOut->blockAlign = drwav_bytes_to_u16(fmt + 12);
  fmtOut->bitsPerSample = drwav_bytes_to_u16(fmt + 14);

  fmtOut->extendedSize = 0;
  fmtOut->validBitsPerSample = 0;
  fmtOut->channelMask = 0;
  memset(fmtOut->subFormat, 0, sizeof(fmtOut->subFormat));

  if (header.sizeInBytes > 16) {
    uint8_t fmt_cbSize[2];
    int bytesReadSoFar = 0;

    if (onRead(pUserData, fmt_cbSize, sizeof(fmt_cbSize)) != sizeof(fmt_cbSize)) {
      return false; /* Expecting more data. */
    }
    *pRunningBytesReadOut += sizeof(fmt_cbSize);

    bytesReadSoFar = 18;

    fmtOut->extendedSize = drwav_bytes_to_u16(fmt_cbSize);
    if (fmtOut->extendedSize > 0) {
      /* Simple validation. */
      if (fmtOut->formatTag == DR_WAVE_FORMAT_EXTENSIBLE) {
        if (fmtOut->extendedSize != 22) { return false; }
      }

      if (fmtOut->formatTag == DR_WAVE_FORMAT_EXTENSIBLE) {
        uint8_t fmtext[22];
        if (onRead(pUserData, fmtext, fmtOut->extendedSize) != fmtOut->extendedSize) {
          return false; /* Expecting more data. */
        }

        fmtOut->validBitsPerSample = drwav_bytes_to_u16(fmtext + 0);
        fmtOut->channelMask = drwav_bytes_to_u32(fmtext + 2);
        drwav_bytes_to_guid(fmtext + 6, fmtOut->subFormat);
      } else {
        if (!onSeek(pUserData, fmtOut->extendedSize, drwav_seek_origin_current)) { return false; }
      }
      *pRunningBytesReadOut += fmtOut->extendedSize;

      bytesReadSoFar += fmtOut->extendedSize;
    }

    /* Seek past any leftover bytes. For w64 the leftover will be defined based
     * on the chunk size. */
    if (!onSeek(pUserData, (int)(header.sizeInBytes - bytesReadSoFar), drwav_seek_origin_current)) {
      return false;
    }
    *pRunningBytesReadOut += (header.sizeInBytes - bytesReadSoFar);
  }

  if (header.paddingSize > 0) {
    if (!onSeek(pUserData, header.paddingSize, drwav_seek_origin_current)) { return false; }
    *pRunningBytesReadOut += header.paddingSize;
  }

  return true;
}

DRWAV_PRIVATE size_t drwav__on_read(drwav_read_proc onRead, void* pUserData, void* pBufferOut,
                                    size_t bytesToRead, uint64_t* pCursor) {
  size_t bytesRead;

  DRWAV_ASSERT(onRead != NULL);
  DRWAV_ASSERT(pCursor != NULL);

  bytesRead = onRead(pUserData, pBufferOut, bytesToRead);
  *pCursor += bytesRead;
  return bytesRead;
}

#if 0
DRWAV_PRIVATE bool drwav__on_seek(drwav_seek_proc onSeek, void* pUserData, int offset, drwav_seek_origin origin, uint64_t* pCursor)
{
    DRWAV_ASSERT(onSeek != NULL);
    DRWAV_ASSERT(pCursor != NULL);

    if (!onSeek(pUserData, offset, origin)) {
        return false;
    }

    if (origin == drwav_seek_origin_start) {
        *pCursor = offset;
    } else {
        *pCursor += offset;
    }

    return true;
}
#endif

#define DRWAV_SMPL_BYTES 36
#define DRWAV_SMPL_LOOP_BYTES 24
#define DRWAV_INST_BYTES 7
#define DRWAV_ACID_BYTES 24
#define DRWAV_CUE_BYTES 4
#define DRWAV_BEXT_BYTES 602
#define DRWAV_BEXT_DESCRIPTION_BYTES 256
#define DRWAV_BEXT_ORIGINATOR_NAME_BYTES 32
#define DRWAV_BEXT_ORIGINATOR_REF_BYTES 32
#define DRWAV_BEXT_RESERVED_BYTES 180
#define DRWAV_BEXT_UMID_BYTES 64
#define DRWAV_CUE_POINT_BYTES 24
#define DRWAV_LIST_LABEL_OR_NOTE_BYTES 4
#define DRWAV_LIST_LABELLED_TEXT_BYTES 20

#define DRWAV_METADATA_ALIGNMENT 8

typedef enum {
  drwav__metadata_parser_stage_count,
  drwav__metadata_parser_stage_read
} drwav__metadata_parser_stage;

typedef struct {
  drwav_read_proc onRead;
  drwav_seek_proc onSeek;
  void* pReadSeekUserData;
  drwav__metadata_parser_stage stage;
  drwav_metadata* pMetadata;
  uint32_t metadataCount;
  uint8_t* pData;
  uint8_t* pDataCursor;
  uint64_t metadataCursor;
  uint64_t extraCapacity;
} drwav__metadata_parser;

DRWAV_PRIVATE size_t drwav__metadata_memory_capacity(drwav__metadata_parser* pParser) {
  uint64_t cap = sizeof(drwav_metadata) * (uint64_t)pParser->metadataCount + pParser->extraCapacity;
  if (cap > DRWAV_SIZE_MAX) { return 0; /* Too big. */ }

  return (size_t)cap; /* Safe cast thanks to the check above. */
}

DRWAV_PRIVATE uint8_t* drwav__metadata_get_memory(drwav__metadata_parser* pParser, size_t size,
                                                  size_t align) {
  uint8_t* pResult;

  if (align) {
    uintptr_t modulo = (uintptr_t)pParser->pDataCursor % align;
    if (modulo != 0) { pParser->pDataCursor += align - modulo; }
  }

  pResult = pParser->pDataCursor;

  /*
  Getting to the point where this function is called means there should always
  be memory available. Out of memory checks should have been done at an earlier
  stage.
  */
  DRWAV_ASSERT((pResult + size) <= (pParser->pData + drwav__metadata_memory_capacity(pParser)));

  pParser->pDataCursor += size;
  return pResult;
}

DRWAV_PRIVATE void drwav__metadata_request_extra_memory_for_stage_2(drwav__metadata_parser* pParser,
                                                                    size_t bytes, size_t align) {
  size_t extra = bytes + (align ? (align - 1) : 0);
  pParser->extraCapacity += extra;
}

DRWAV_PRIVATE drwav_result drwav__metadata_alloc(drwav__metadata_parser* pParser,
                                                 drwav_allocation_callbacks* pAllocationCallbacks) {
  if (pParser->extraCapacity != 0 || pParser->metadataCount != 0) {
    free(pParser->pData);

    pParser->pData = (uint8_t*)pAllocationCallbacks->onMalloc(
        drwav__metadata_memory_capacity(pParser), pAllocationCallbacks->pUserData);
    pParser->pDataCursor = pParser->pData;

    if (pParser->pData == NULL) { return DRWAV_OUT_OF_MEMORY; }

    /*
    We don't need to worry about specifying an alignment here because malloc
    always returns something of suitable alignment. This also means than
    pParser->pMetadata is all that we need to store in order for us to free when
    we are done.
    */
    pParser->pMetadata = (drwav_metadata*)drwav__metadata_get_memory(
        pParser, sizeof(drwav_metadata) * pParser->metadataCount, 1);
    pParser->metadataCursor = 0;
  }

  return DRWAV_SUCCESS;
}

DRWAV_PRIVATE size_t drwav__metadata_parser_read(drwav__metadata_parser* pParser, void* pBufferOut,
                                                 size_t bytesToRead, uint64_t* pCursor) {
  if (pCursor != NULL) {
    return drwav__on_read(pParser->onRead, pParser->pReadSeekUserData, pBufferOut, bytesToRead,
                          pCursor);
  } else {
    return pParser->onRead(pParser->pReadSeekUserData, pBufferOut, bytesToRead);
  }
}

DRWAV_PRIVATE uint64_t drwav__read_smpl_to_metadata_obj(drwav__metadata_parser* pParser,
                                                        drwav_metadata* pMetadata) {
  uint8_t smplHeaderData[DRWAV_SMPL_BYTES];
  uint64_t totalBytesRead = 0;
  size_t bytesJustRead =
      drwav__metadata_parser_read(pParser, smplHeaderData, sizeof(smplHeaderData), &totalBytesRead);

  DRWAV_ASSERT(pParser->stage == drwav__metadata_parser_stage_read);

  if (bytesJustRead == sizeof(smplHeaderData)) {
    uint32_t iSampleLoop;

    pMetadata->type = drwav_metadata_type_smpl;
    pMetadata->data.smpl.manufacturerId = drwav_bytes_to_u32(smplHeaderData + 0);
    pMetadata->data.smpl.productId = drwav_bytes_to_u32(smplHeaderData + 4);
    pMetadata->data.smpl.samplePeriodNanoseconds = drwav_bytes_to_u32(smplHeaderData + 8);
    pMetadata->data.smpl.midiUnityNote = drwav_bytes_to_u32(smplHeaderData + 12);
    pMetadata->data.smpl.midiPitchFraction = drwav_bytes_to_u32(smplHeaderData + 16);
    pMetadata->data.smpl.smpteFormat = drwav_bytes_to_u32(smplHeaderData + 20);
    pMetadata->data.smpl.smpteOffset = drwav_bytes_to_u32(smplHeaderData + 24);
    pMetadata->data.smpl.sampleLoopCount = drwav_bytes_to_u32(smplHeaderData + 28);
    pMetadata->data.smpl.samplerSpecificDataSizeInBytes = drwav_bytes_to_u32(smplHeaderData + 32);
    pMetadata->data.smpl.pLoops = (drwav_smpl_loop*)drwav__metadata_get_memory(
        pParser, sizeof(drwav_smpl_loop) * pMetadata->data.smpl.sampleLoopCount,
        DRWAV_METADATA_ALIGNMENT);

    for (iSampleLoop = 0; iSampleLoop < pMetadata->data.smpl.sampleLoopCount; ++iSampleLoop) {
      uint8_t smplLoopData[DRWAV_SMPL_LOOP_BYTES];
      bytesJustRead =
          drwav__metadata_parser_read(pParser, smplLoopData, sizeof(smplLoopData), &totalBytesRead);

      if (bytesJustRead == sizeof(smplLoopData)) {
        pMetadata->data.smpl.pLoops[iSampleLoop].cuePointId = drwav_bytes_to_u32(smplLoopData + 0);
        pMetadata->data.smpl.pLoops[iSampleLoop].type = drwav_bytes_to_u32(smplLoopData + 4);
        pMetadata->data.smpl.pLoops[iSampleLoop].firstSampleByteOffset =
            drwav_bytes_to_u32(smplLoopData + 8);
        pMetadata->data.smpl.pLoops[iSampleLoop].lastSampleByteOffset =
            drwav_bytes_to_u32(smplLoopData + 12);
        pMetadata->data.smpl.pLoops[iSampleLoop].sampleFraction =
            drwav_bytes_to_u32(smplLoopData + 16);
        pMetadata->data.smpl.pLoops[iSampleLoop].playCount = drwav_bytes_to_u32(smplLoopData + 20);
      } else {
        break;
      }
    }

    if (pMetadata->data.smpl.samplerSpecificDataSizeInBytes > 0) {
      pMetadata->data.smpl.pSamplerSpecificData = drwav__metadata_get_memory(
          pParser, pMetadata->data.smpl.samplerSpecificDataSizeInBytes, 1);
      DRWAV_ASSERT(pMetadata->data.smpl.pSamplerSpecificData != NULL);

      bytesJustRead = drwav__metadata_parser_read(
          pParser, pMetadata->data.smpl.pSamplerSpecificData,
          pMetadata->data.smpl.samplerSpecificDataSizeInBytes, &totalBytesRead);
    }
  }

  return totalBytesRead;
}

DRWAV_PRIVATE uint64_t drwav__read_cue_to_metadata_obj(drwav__metadata_parser* pParser,
                                                       drwav_metadata* pMetadata) {
  uint8_t cueHeaderSectionData[DRWAV_CUE_BYTES];
  uint64_t totalBytesRead = 0;
  size_t bytesJustRead = drwav__metadata_parser_read(pParser, cueHeaderSectionData,
                                                     sizeof(cueHeaderSectionData), &totalBytesRead);

  DRWAV_ASSERT(pParser->stage == drwav__metadata_parser_stage_read);

  if (bytesJustRead == sizeof(cueHeaderSectionData)) {
    pMetadata->type = drwav_metadata_type_cue;
    pMetadata->data.cue.cuePointCount = drwav_bytes_to_u32(cueHeaderSectionData);
    pMetadata->data.cue.pCuePoints = (drwav_cue_point*)drwav__metadata_get_memory(
        pParser, sizeof(drwav_cue_point) * pMetadata->data.cue.cuePointCount,
        DRWAV_METADATA_ALIGNMENT);
    DRWAV_ASSERT(pMetadata->data.cue.pCuePoints != NULL);

    if (pMetadata->data.cue.cuePointCount > 0) {
      uint32_t iCuePoint;

      for (iCuePoint = 0; iCuePoint < pMetadata->data.cue.cuePointCount; ++iCuePoint) {
        uint8_t cuePointData[DRWAV_CUE_POINT_BYTES];
        bytesJustRead = drwav__metadata_parser_read(pParser, cuePointData, sizeof(cuePointData),
                                                    &totalBytesRead);

        if (bytesJustRead == sizeof(cuePointData)) {
          pMetadata->data.cue.pCuePoints[iCuePoint].id = drwav_bytes_to_u32(cuePointData + 0);
          pMetadata->data.cue.pCuePoints[iCuePoint].playOrderPosition =
              drwav_bytes_to_u32(cuePointData + 4);
          pMetadata->data.cue.pCuePoints[iCuePoint].dataChunkId[0] = cuePointData[8];
          pMetadata->data.cue.pCuePoints[iCuePoint].dataChunkId[1] = cuePointData[9];
          pMetadata->data.cue.pCuePoints[iCuePoint].dataChunkId[2] = cuePointData[10];
          pMetadata->data.cue.pCuePoints[iCuePoint].dataChunkId[3] = cuePointData[11];
          pMetadata->data.cue.pCuePoints[iCuePoint].chunkStart =
              drwav_bytes_to_u32(cuePointData + 12);
          pMetadata->data.cue.pCuePoints[iCuePoint].blockStart =
              drwav_bytes_to_u32(cuePointData + 16);
          pMetadata->data.cue.pCuePoints[iCuePoint].sampleByteOffset =
              drwav_bytes_to_u32(cuePointData + 20);
        } else {
          break;
        }
      }
    }
  }

  return totalBytesRead;
}

DRWAV_PRIVATE uint64_t drwav__read_inst_to_metadata_obj(drwav__metadata_parser* pParser,
                                                        drwav_metadata* pMetadata) {
  uint8_t instData[DRWAV_INST_BYTES];
  uint64_t bytesRead = drwav__metadata_parser_read(pParser, instData, sizeof(instData), NULL);

  DRWAV_ASSERT(pParser->stage == drwav__metadata_parser_stage_read);

  if (bytesRead == sizeof(instData)) {
    pMetadata->type = drwav_metadata_type_inst;
    pMetadata->data.inst.midiUnityNote = (int8_t)instData[0];
    pMetadata->data.inst.fineTuneCents = (int8_t)instData[1];
    pMetadata->data.inst.gainDecibels = (int8_t)instData[2];
    pMetadata->data.inst.lowNote = (int8_t)instData[3];
    pMetadata->data.inst.highNote = (int8_t)instData[4];
    pMetadata->data.inst.lowVelocity = (int8_t)instData[5];
    pMetadata->data.inst.highVelocity = (int8_t)instData[6];
  }

  return bytesRead;
}

DRWAV_PRIVATE uint64_t drwav__read_acid_to_metadata_obj(drwav__metadata_parser* pParser,
                                                        drwav_metadata* pMetadata) {
  uint8_t acidData[DRWAV_ACID_BYTES];
  uint64_t bytesRead = drwav__metadata_parser_read(pParser, acidData, sizeof(acidData), NULL);

  DRWAV_ASSERT(pParser->stage == drwav__metadata_parser_stage_read);

  if (bytesRead == sizeof(acidData)) {
    pMetadata->type = drwav_metadata_type_acid;
    pMetadata->data.acid.flags = drwav_bytes_to_u32(acidData + 0);
    pMetadata->data.acid.midiUnityNote = drwav_bytes_to_u16(acidData + 4);
    pMetadata->data.acid.reserved1 = drwav_bytes_to_u16(acidData + 6);
    pMetadata->data.acid.reserved2 = drwav_bytes_to_f32(acidData + 8);
    pMetadata->data.acid.numBeats = drwav_bytes_to_u32(acidData + 12);
    pMetadata->data.acid.meterDenominator = drwav_bytes_to_u16(acidData + 16);
    pMetadata->data.acid.meterNumerator = drwav_bytes_to_u16(acidData + 18);
    pMetadata->data.acid.tempo = drwav_bytes_to_f32(acidData + 20);
  }

  return bytesRead;
}

DRWAV_PRIVATE size_t drwav__strlen_clamped(char* str, size_t maxToRead) {
  size_t result = 0;

  while (*str++ && result < maxToRead) { result += 1; }

  return result;
}

DRWAV_PRIVATE char* drwav__metadata_copy_string(drwav__metadata_parser* pParser, char* str,
                                                size_t maxToRead) {
  size_t len = drwav__strlen_clamped(str, maxToRead);

  if (len) {
    char* result = (char*)drwav__metadata_get_memory(pParser, len + 1, 1);
    DRWAV_ASSERT(result != NULL);

    memcpy(result, str, len);
    result[len] = '\0';

    return result;
  } else {
    return NULL;
  }
}

DRWAV_PRIVATE uint64_t drwav__read_bext_to_metadata_obj(drwav__metadata_parser* pParser,
                                                        drwav_metadata* pMetadata,
                                                        uint64_t chunkSize) {
  uint8_t bextData[DRWAV_BEXT_BYTES];
  uint64_t bytesRead = drwav__metadata_parser_read(pParser, bextData, sizeof(bextData), NULL);

  DRWAV_ASSERT(pParser->stage == drwav__metadata_parser_stage_read);

  if (bytesRead == sizeof(bextData)) {
    uint8_t* pReadPointer;
    uint32_t timeReferenceLow;
    uint32_t timeReferenceHigh;
    size_t extraBytes;

    pMetadata->type = drwav_metadata_type_bext;

    pReadPointer = bextData;
    pMetadata->data.bext.pDescription =
        drwav__metadata_copy_string(pParser, (char*)(pReadPointer), DRWAV_BEXT_DESCRIPTION_BYTES);
    pReadPointer += DRWAV_BEXT_DESCRIPTION_BYTES;

    pMetadata->data.bext.pOriginatorName = drwav__metadata_copy_string(
        pParser, (char*)(pReadPointer), DRWAV_BEXT_ORIGINATOR_NAME_BYTES);
    pReadPointer += DRWAV_BEXT_ORIGINATOR_NAME_BYTES;

    pMetadata->data.bext.pOriginatorReference = drwav__metadata_copy_string(
        pParser, (char*)(pReadPointer), DRWAV_BEXT_ORIGINATOR_REF_BYTES);
    pReadPointer += DRWAV_BEXT_ORIGINATOR_REF_BYTES;

    memcpy(pReadPointer, pMetadata->data.bext.pOriginationDate,
           sizeof(pMetadata->data.bext.pOriginationDate));
    pReadPointer += sizeof(pMetadata->data.bext.pOriginationDate);

    memcpy(pReadPointer, pMetadata->data.bext.pOriginationTime,
           sizeof(pMetadata->data.bext.pOriginationTime));
    pReadPointer += sizeof(pMetadata->data.bext.pOriginationTime);

    timeReferenceLow = drwav_bytes_to_u32(pReadPointer);
    pReadPointer += sizeof(uint32_t);
    timeReferenceHigh = drwav_bytes_to_u32(pReadPointer);
    pReadPointer += sizeof(uint32_t);
    pMetadata->data.bext.timeReference = ((uint64_t)timeReferenceHigh << 32) + timeReferenceLow;

    pMetadata->data.bext.version = drwav_bytes_to_u16(pReadPointer);
    pReadPointer += sizeof(uint16_t);

    pMetadata->data.bext.pUMID = drwav__metadata_get_memory(pParser, DRWAV_BEXT_UMID_BYTES, 1);
    memcpy(pMetadata->data.bext.pUMID, pReadPointer, DRWAV_BEXT_UMID_BYTES);
    pReadPointer += DRWAV_BEXT_UMID_BYTES;

    pMetadata->data.bext.loudnessValue = drwav_bytes_to_u16(pReadPointer);
    pReadPointer += sizeof(uint16_t);

    pMetadata->data.bext.loudnessRange = drwav_bytes_to_u16(pReadPointer);
    pReadPointer += sizeof(uint16_t);

    pMetadata->data.bext.maxTruePeakLevel = drwav_bytes_to_u16(pReadPointer);
    pReadPointer += sizeof(uint16_t);

    pMetadata->data.bext.maxMomentaryLoudness = drwav_bytes_to_u16(pReadPointer);
    pReadPointer += sizeof(uint16_t);

    pMetadata->data.bext.maxShortTermLoudness = drwav_bytes_to_u16(pReadPointer);
    pReadPointer += sizeof(uint16_t);

    DRWAV_ASSERT((pReadPointer + DRWAV_BEXT_RESERVED_BYTES) == (bextData + DRWAV_BEXT_BYTES));

    extraBytes = (size_t)(chunkSize - DRWAV_BEXT_BYTES);
    if (extraBytes > 0) {
      pMetadata->data.bext.pCodingHistory =
          (char*)drwav__metadata_get_memory(pParser, extraBytes + 1, 1);
      DRWAV_ASSERT(pMetadata->data.bext.pCodingHistory != NULL);

      bytesRead += drwav__metadata_parser_read(pParser, pMetadata->data.bext.pCodingHistory,
                                               extraBytes, NULL);
      pMetadata->data.bext.codingHistorySize =
          (uint32_t)strlen(pMetadata->data.bext.pCodingHistory);
    } else {
      pMetadata->data.bext.pCodingHistory = NULL;
      pMetadata->data.bext.codingHistorySize = 0;
    }
  }

  return bytesRead;
}

DRWAV_PRIVATE uint64_t drwav__read_list_label_or_note_to_metadata_obj(
    drwav__metadata_parser* pParser, drwav_metadata* pMetadata, uint64_t chunkSize,
    drwav_metadata_type type) {
  uint8_t cueIDBuffer[DRWAV_LIST_LABEL_OR_NOTE_BYTES];
  uint64_t totalBytesRead = 0;
  size_t bytesJustRead =
      drwav__metadata_parser_read(pParser, cueIDBuffer, sizeof(cueIDBuffer), &totalBytesRead);

  DRWAV_ASSERT(pParser->stage == drwav__metadata_parser_stage_read);

  if (bytesJustRead == sizeof(cueIDBuffer)) {
    uint32_t sizeIncludingNullTerminator;

    pMetadata->type = type;
    pMetadata->data.labelOrNote.cuePointId = drwav_bytes_to_u32(cueIDBuffer);

    sizeIncludingNullTerminator = (uint32_t)chunkSize - DRWAV_LIST_LABEL_OR_NOTE_BYTES;
    if (sizeIncludingNullTerminator > 0) {
      pMetadata->data.labelOrNote.stringLength = sizeIncludingNullTerminator - 1;
      pMetadata->data.labelOrNote.pString =
          (char*)drwav__metadata_get_memory(pParser, sizeIncludingNullTerminator, 1);
      DRWAV_ASSERT(pMetadata->data.labelOrNote.pString != NULL);

      bytesJustRead = drwav__metadata_parser_read(pParser, pMetadata->data.labelOrNote.pString,
                                                  sizeIncludingNullTerminator, &totalBytesRead);
    } else {
      pMetadata->data.labelOrNote.stringLength = 0;
      pMetadata->data.labelOrNote.pString = NULL;
    }
  }

  return totalBytesRead;
}

DRWAV_PRIVATE uint64_t drwav__read_list_labelled_cue_region_to_metadata_obj(
    drwav__metadata_parser* pParser, drwav_metadata* pMetadata, uint64_t chunkSize) {
  uint8_t buffer[DRWAV_LIST_LABELLED_TEXT_BYTES];
  uint64_t totalBytesRead = 0;
  size_t bytesJustRead =
      drwav__metadata_parser_read(pParser, buffer, sizeof(buffer), &totalBytesRead);

  DRWAV_ASSERT(pParser->stage == drwav__metadata_parser_stage_read);

  if (bytesJustRead == sizeof(buffer)) {
    uint32_t sizeIncludingNullTerminator;

    pMetadata->type = drwav_metadata_type_list_labelled_cue_region;
    pMetadata->data.labelledCueRegion.cuePointId = drwav_bytes_to_u32(buffer + 0);
    pMetadata->data.labelledCueRegion.sampleLength = drwav_bytes_to_u32(buffer + 4);
    pMetadata->data.labelledCueRegion.purposeId[0] = buffer[8];
    pMetadata->data.labelledCueRegion.purposeId[1] = buffer[9];
    pMetadata->data.labelledCueRegion.purposeId[2] = buffer[10];
    pMetadata->data.labelledCueRegion.purposeId[3] = buffer[11];
    pMetadata->data.labelledCueRegion.country = drwav_bytes_to_u16(buffer + 12);
    pMetadata->data.labelledCueRegion.language = drwav_bytes_to_u16(buffer + 14);
    pMetadata->data.labelledCueRegion.dialect = drwav_bytes_to_u16(buffer + 16);
    pMetadata->data.labelledCueRegion.codePage = drwav_bytes_to_u16(buffer + 18);

    sizeIncludingNullTerminator = (uint32_t)chunkSize - DRWAV_LIST_LABELLED_TEXT_BYTES;
    if (sizeIncludingNullTerminator > 0) {
      pMetadata->data.labelledCueRegion.stringLength = sizeIncludingNullTerminator - 1;
      pMetadata->data.labelledCueRegion.pString =
          (char*)drwav__metadata_get_memory(pParser, sizeIncludingNullTerminator, 1);
      DRWAV_ASSERT(pMetadata->data.labelledCueRegion.pString != NULL);

      bytesJustRead =
          drwav__metadata_parser_read(pParser, pMetadata->data.labelledCueRegion.pString,
                                      sizeIncludingNullTerminator, &totalBytesRead);
    } else {
      pMetadata->data.labelledCueRegion.stringLength = 0;
      pMetadata->data.labelledCueRegion.pString = NULL;
    }
  }

  return totalBytesRead;
}

DRWAV_PRIVATE uint64_t drwav__metadata_process_info_text_chunk(drwav__metadata_parser* pParser,
                                                               uint64_t chunkSize,
                                                               drwav_metadata_type type) {
  uint64_t bytesRead = 0;
  uint32_t stringSizeWithNullTerminator = (uint32_t)chunkSize;

  if (pParser->stage == drwav__metadata_parser_stage_count) {
    pParser->metadataCount += 1;
    drwav__metadata_request_extra_memory_for_stage_2(pParser, stringSizeWithNullTerminator, 1);
  } else {
    drwav_metadata* pMetadata = &pParser->pMetadata[pParser->metadataCursor];
    pMetadata->type = type;
    if (stringSizeWithNullTerminator > 0) {
      pMetadata->data.infoText.stringLength = stringSizeWithNullTerminator - 1;
      pMetadata->data.infoText.pString =
          (char*)drwav__metadata_get_memory(pParser, stringSizeWithNullTerminator, 1);
      DRWAV_ASSERT(pMetadata->data.infoText.pString != NULL);

      bytesRead = drwav__metadata_parser_read(pParser, pMetadata->data.infoText.pString,
                                              (size_t)stringSizeWithNullTerminator, NULL);
      if (bytesRead == chunkSize) {
        pParser->metadataCursor += 1;
      } else {
        /* Failed to parse. */
      }
    } else {
      pMetadata->data.infoText.stringLength = 0;
      pMetadata->data.infoText.pString = NULL;
      pParser->metadataCursor += 1;
    }
  }

  return bytesRead;
}

DRWAV_PRIVATE uint64_t drwav__metadata_process_unknown_chunk(drwav__metadata_parser* pParser,
                                                             const uint8_t* pChunkId,
                                                             uint64_t chunkSize,
                                                             drwav_metadata_location location) {
  uint64_t bytesRead = 0;

  if (location == drwav_metadata_location_invalid) { return 0; }

  if (drwav_fourcc_equal(pChunkId, "data") || drwav_fourcc_equal(pChunkId, "fmt") ||
      drwav_fourcc_equal(pChunkId, "fact")) {
    return 0;
  }

  if (pParser->stage == drwav__metadata_parser_stage_count) {
    pParser->metadataCount += 1;
    drwav__metadata_request_extra_memory_for_stage_2(pParser, (size_t)chunkSize, 1);
  } else {
    drwav_metadata* pMetadata = &pParser->pMetadata[pParser->metadataCursor];
    pMetadata->type = drwav_metadata_type_unknown;
    pMetadata->data.unknown.chunkLocation = location;
    pMetadata->data.unknown.id[0] = pChunkId[0];
    pMetadata->data.unknown.id[1] = pChunkId[1];
    pMetadata->data.unknown.id[2] = pChunkId[2];
    pMetadata->data.unknown.id[3] = pChunkId[3];
    pMetadata->data.unknown.dataSizeInBytes = (uint32_t)chunkSize;
    pMetadata->data.unknown.pData =
        (uint8_t*)drwav__metadata_get_memory(pParser, (size_t)chunkSize, 1);
    DRWAV_ASSERT(pMetadata->data.unknown.pData != NULL);

    bytesRead = drwav__metadata_parser_read(pParser, pMetadata->data.unknown.pData,
                                            pMetadata->data.unknown.dataSizeInBytes, NULL);
    if (bytesRead == pMetadata->data.unknown.dataSizeInBytes) {
      pParser->metadataCursor += 1;
    } else {
      /* Failed to read. */
    }
  }

  return bytesRead;
}

DRWAV_PRIVATE bool drwav__chunk_matches(uint64_t allowedMetadataTypes, const uint8_t* pChunkID,
                                        drwav_metadata_type type, const char* pID) {
  return (allowedMetadataTypes & type) && drwav_fourcc_equal(pChunkID, pID);
}

DRWAV_PRIVATE uint64_t drwav__metadata_process_chunk(drwav__metadata_parser* pParser,
                                                     const drwav_chunk_header* pChunkHeader,
                                                     uint64_t allowedMetadataTypes) {
  const uint8_t* pChunkID = pChunkHeader->id.fourcc;
  uint64_t bytesRead = 0;

  if (drwav__chunk_matches(allowedMetadataTypes, pChunkID, drwav_metadata_type_smpl, "smpl")) {
    if (pChunkHeader->sizeInBytes >= DRWAV_SMPL_BYTES) {
      if (pParser->stage == drwav__metadata_parser_stage_count) {
        uint8_t buffer[4];
        size_t bytesJustRead;

        if (!pParser->onSeek(pParser->pReadSeekUserData, 28, drwav_seek_origin_current)) {
          return bytesRead;
        }
        bytesRead += 28;

        bytesJustRead = drwav__metadata_parser_read(pParser, buffer, sizeof(buffer), &bytesRead);
        if (bytesJustRead == sizeof(buffer)) {
          uint32_t loopCount = drwav_bytes_to_u32(buffer);

          bytesJustRead = drwav__metadata_parser_read(pParser, buffer, sizeof(buffer), &bytesRead);
          if (bytesJustRead == sizeof(buffer)) {
            uint32_t samplerSpecificDataSizeInBytes = drwav_bytes_to_u32(buffer);

            pParser->metadataCount += 1;
            drwav__metadata_request_extra_memory_for_stage_2(
                pParser, sizeof(drwav_smpl_loop) * loopCount, DRWAV_METADATA_ALIGNMENT);
            drwav__metadata_request_extra_memory_for_stage_2(pParser,
                                                             samplerSpecificDataSizeInBytes, 1);
          }
        }
      } else {
        bytesRead =
            drwav__read_smpl_to_metadata_obj(pParser, &pParser->pMetadata[pParser->metadataCursor]);
        if (bytesRead == pChunkHeader->sizeInBytes) {
          pParser->metadataCursor += 1;
        } else {
          /* Failed to parse. */
        }
      }
    } else {
      /* Incorrectly formed chunk. */
    }
  } else if (drwav__chunk_matches(allowedMetadataTypes, pChunkID, drwav_metadata_type_inst,
                                  "inst")) {
    if (pChunkHeader->sizeInBytes == DRWAV_INST_BYTES) {
      if (pParser->stage == drwav__metadata_parser_stage_count) {
        pParser->metadataCount += 1;
      } else {
        bytesRead =
            drwav__read_inst_to_metadata_obj(pParser, &pParser->pMetadata[pParser->metadataCursor]);
        if (bytesRead == pChunkHeader->sizeInBytes) {
          pParser->metadataCursor += 1;
        } else {
          /* Failed to parse. */
        }
      }
    } else {
      /* Incorrectly formed chunk. */
    }
  } else if (drwav__chunk_matches(allowedMetadataTypes, pChunkID, drwav_metadata_type_acid,
                                  "acid")) {
    if (pChunkHeader->sizeInBytes == DRWAV_ACID_BYTES) {
      if (pParser->stage == drwav__metadata_parser_stage_count) {
        pParser->metadataCount += 1;
      } else {
        bytesRead =
            drwav__read_acid_to_metadata_obj(pParser, &pParser->pMetadata[pParser->metadataCursor]);
        if (bytesRead == pChunkHeader->sizeInBytes) {
          pParser->metadataCursor += 1;
        } else {
          /* Failed to parse. */
        }
      }
    } else {
      /* Incorrectly formed chunk. */
    }
  } else if (drwav__chunk_matches(allowedMetadataTypes, pChunkID, drwav_metadata_type_cue,
                                  "cue ")) {
    if (pChunkHeader->sizeInBytes >= DRWAV_CUE_BYTES) {
      if (pParser->stage == drwav__metadata_parser_stage_count) {
        size_t cueCount;

        pParser->metadataCount += 1;
        cueCount = (size_t)(pChunkHeader->sizeInBytes - DRWAV_CUE_BYTES) / DRWAV_CUE_POINT_BYTES;
        drwav__metadata_request_extra_memory_for_stage_2(
            pParser, sizeof(drwav_cue_point) * cueCount, DRWAV_METADATA_ALIGNMENT);
      } else {
        bytesRead =
            drwav__read_cue_to_metadata_obj(pParser, &pParser->pMetadata[pParser->metadataCursor]);
        if (bytesRead == pChunkHeader->sizeInBytes) {
          pParser->metadataCursor += 1;
        } else {
          /* Failed to parse. */
        }
      }
    } else {
      /* Incorrectly formed chunk. */
    }
  } else if (drwav__chunk_matches(allowedMetadataTypes, pChunkID, drwav_metadata_type_bext,
                                  "bext")) {
    if (pChunkHeader->sizeInBytes >= DRWAV_BEXT_BYTES) {
      if (pParser->stage == drwav__metadata_parser_stage_count) {
        /* The description field is the largest one in a bext chunk, so that is
         * the max size of this temporary buffer. */
        char buffer[DRWAV_BEXT_DESCRIPTION_BYTES + 1];
        size_t allocSizeNeeded = DRWAV_BEXT_UMID_BYTES; /* We know we will need SMPTE umid size. */
        size_t bytesJustRead;

        buffer[DRWAV_BEXT_DESCRIPTION_BYTES] = '\0';
        bytesJustRead =
            drwav__metadata_parser_read(pParser, buffer, DRWAV_BEXT_DESCRIPTION_BYTES, &bytesRead);
        if (bytesJustRead != DRWAV_BEXT_DESCRIPTION_BYTES) { return bytesRead; }
        allocSizeNeeded += strlen(buffer) + 1;

        buffer[DRWAV_BEXT_ORIGINATOR_NAME_BYTES] = '\0';
        bytesJustRead = drwav__metadata_parser_read(pParser, buffer,
                                                    DRWAV_BEXT_ORIGINATOR_NAME_BYTES, &bytesRead);
        if (bytesJustRead != DRWAV_BEXT_ORIGINATOR_NAME_BYTES) { return bytesRead; }
        allocSizeNeeded += strlen(buffer) + 1;

        buffer[DRWAV_BEXT_ORIGINATOR_REF_BYTES] = '\0';
        bytesJustRead = drwav__metadata_parser_read(pParser, buffer,
                                                    DRWAV_BEXT_ORIGINATOR_REF_BYTES, &bytesRead);
        if (bytesJustRead != DRWAV_BEXT_ORIGINATOR_REF_BYTES) { return bytesRead; }
        allocSizeNeeded += strlen(buffer) + 1;
        allocSizeNeeded +=
            (size_t)pChunkHeader->sizeInBytes - DRWAV_BEXT_BYTES; /* Coding history. */

        drwav__metadata_request_extra_memory_for_stage_2(pParser, allocSizeNeeded, 1);

        pParser->metadataCount += 1;
      } else {
        bytesRead = drwav__read_bext_to_metadata_obj(
            pParser, &pParser->pMetadata[pParser->metadataCursor], pChunkHeader->sizeInBytes);
        if (bytesRead == pChunkHeader->sizeInBytes) {
          pParser->metadataCursor += 1;
        } else {
          /* Failed to parse. */
        }
      }
    } else {
      /* Incorrectly formed chunk. */
    }
  } else if (drwav_fourcc_equal(pChunkID, "LIST") || drwav_fourcc_equal(pChunkID, "list")) {
    drwav_metadata_location listType = drwav_metadata_location_invalid;
    while (bytesRead < pChunkHeader->sizeInBytes) {
      uint8_t subchunkId[4];
      uint8_t subchunkSizeBuffer[4];
      uint64_t subchunkDataSize;
      uint64_t subchunkBytesRead = 0;
      uint64_t bytesJustRead =
          drwav__metadata_parser_read(pParser, subchunkId, sizeof(subchunkId), &bytesRead);
      if (bytesJustRead != sizeof(subchunkId)) { break; }

      /*
      The first thing in a list chunk should be "adtl" or "INFO".

        - adtl means this list is a Associated Data List Chunk and will contain
      labels, notes or labelled cue regions.
        - INFO means this list is an Info List Chunk containing info text chunks
      such as IPRD which would specifies the album of this wav file.

      No data follows the adtl or INFO id so we just make note of what type this
      list is and continue.
      */
      if (drwav_fourcc_equal(subchunkId, "adtl")) {
        listType = drwav_metadata_location_inside_adtl_list;
        continue;
      } else if (drwav_fourcc_equal(subchunkId, "INFO")) {
        listType = drwav_metadata_location_inside_info_list;
        continue;
      }

      bytesJustRead = drwav__metadata_parser_read(pParser, subchunkSizeBuffer,
                                                  sizeof(subchunkSizeBuffer), &bytesRead);
      if (bytesJustRead != sizeof(subchunkSizeBuffer)) { break; }
      subchunkDataSize = drwav_bytes_to_u32(subchunkSizeBuffer);

      if (drwav__chunk_matches(allowedMetadataTypes, subchunkId, drwav_metadata_type_list_label,
                               "labl") ||
          drwav__chunk_matches(allowedMetadataTypes, subchunkId, drwav_metadata_type_list_note,
                               "note")) {
        if (subchunkDataSize >= DRWAV_LIST_LABEL_OR_NOTE_BYTES) {
          uint64_t stringSizeWithNullTerm = subchunkDataSize - DRWAV_LIST_LABEL_OR_NOTE_BYTES;
          if (pParser->stage == drwav__metadata_parser_stage_count) {
            pParser->metadataCount += 1;
            drwav__metadata_request_extra_memory_for_stage_2(pParser,
                                                             (size_t)stringSizeWithNullTerm, 1);
          } else {
            subchunkBytesRead = drwav__read_list_label_or_note_to_metadata_obj(
                pParser, &pParser->pMetadata[pParser->metadataCursor], subchunkDataSize,
                drwav_fourcc_equal(subchunkId, "labl") ? drwav_metadata_type_list_label
                                                       : drwav_metadata_type_list_note);
            if (subchunkBytesRead == subchunkDataSize) {
              pParser->metadataCursor += 1;
            } else {
              /* Failed to parse. */
            }
          }
        } else {
          /* Incorrectly formed chunk. */
        }
      } else if (drwav__chunk_matches(allowedMetadataTypes, subchunkId,
                                      drwav_metadata_type_list_labelled_cue_region, "ltxt")) {
        if (subchunkDataSize >= DRWAV_LIST_LABELLED_TEXT_BYTES) {
          uint64_t stringSizeWithNullTerminator = subchunkDataSize - DRWAV_LIST_LABELLED_TEXT_BYTES;
          if (pParser->stage == drwav__metadata_parser_stage_count) {
            pParser->metadataCount += 1;
            drwav__metadata_request_extra_memory_for_stage_2(
                pParser, (size_t)stringSizeWithNullTerminator, 1);
          } else {
            subchunkBytesRead = drwav__read_list_labelled_cue_region_to_metadata_obj(
                pParser, &pParser->pMetadata[pParser->metadataCursor], subchunkDataSize);
            if (subchunkBytesRead == subchunkDataSize) {
              pParser->metadataCursor += 1;
            } else {
              /* Failed to parse. */
            }
          }
        } else {
          /* Incorrectly formed chunk. */
        }
      } else if (drwav__chunk_matches(allowedMetadataTypes, subchunkId,
                                      drwav_metadata_type_list_info_software, "ISFT")) {
        subchunkBytesRead = drwav__metadata_process_info_text_chunk(
            pParser, subchunkDataSize, drwav_metadata_type_list_info_software);
      } else if (drwav__chunk_matches(allowedMetadataTypes, subchunkId,
                                      drwav_metadata_type_list_info_copyright, "ICOP")) {
        subchunkBytesRead = drwav__metadata_process_info_text_chunk(
            pParser, subchunkDataSize, drwav_metadata_type_list_info_copyright);
      } else if (drwav__chunk_matches(allowedMetadataTypes, subchunkId,
                                      drwav_metadata_type_list_info_title, "INAM")) {
        subchunkBytesRead = drwav__metadata_process_info_text_chunk(
            pParser, subchunkDataSize, drwav_metadata_type_list_info_title);
      } else if (drwav__chunk_matches(allowedMetadataTypes, subchunkId,
                                      drwav_metadata_type_list_info_artist, "IART")) {
        subchunkBytesRead = drwav__metadata_process_info_text_chunk(
            pParser, subchunkDataSize, drwav_metadata_type_list_info_artist);
      } else if (drwav__chunk_matches(allowedMetadataTypes, subchunkId,
                                      drwav_metadata_type_list_info_comment, "ICMT")) {
        subchunkBytesRead = drwav__metadata_process_info_text_chunk(
            pParser, subchunkDataSize, drwav_metadata_type_list_info_comment);
      } else if (drwav__chunk_matches(allowedMetadataTypes, subchunkId,
                                      drwav_metadata_type_list_info_date, "ICRD")) {
        subchunkBytesRead = drwav__metadata_process_info_text_chunk(
            pParser, subchunkDataSize, drwav_metadata_type_list_info_date);
      } else if (drwav__chunk_matches(allowedMetadataTypes, subchunkId,
                                      drwav_metadata_type_list_info_genre, "IGNR")) {
        subchunkBytesRead = drwav__metadata_process_info_text_chunk(
            pParser, subchunkDataSize, drwav_metadata_type_list_info_genre);
      } else if (drwav__chunk_matches(allowedMetadataTypes, subchunkId,
                                      drwav_metadata_type_list_info_album, "IPRD")) {
        subchunkBytesRead = drwav__metadata_process_info_text_chunk(
            pParser, subchunkDataSize, drwav_metadata_type_list_info_album);
      } else if (drwav__chunk_matches(allowedMetadataTypes, subchunkId,
                                      drwav_metadata_type_list_info_tracknumber, "ITRK")) {
        subchunkBytesRead = drwav__metadata_process_info_text_chunk(
            pParser, subchunkDataSize, drwav_metadata_type_list_info_tracknumber);
      } else if (allowedMetadataTypes & drwav_metadata_type_unknown) {
        subchunkBytesRead =
            drwav__metadata_process_unknown_chunk(pParser, subchunkId, subchunkDataSize, listType);
      }

      bytesRead += subchunkBytesRead;
      DRWAV_ASSERT(subchunkBytesRead <= subchunkDataSize);

      if (subchunkBytesRead < subchunkDataSize) {
        uint64_t bytesToSeek = subchunkDataSize - subchunkBytesRead;

        if (!pParser->onSeek(pParser->pReadSeekUserData, (int)bytesToSeek,
                             drwav_seek_origin_current)) {
          break;
        }
        bytesRead += bytesToSeek;
      }

      if ((subchunkDataSize % 2) == 1) {
        if (!pParser->onSeek(pParser->pReadSeekUserData, 1, drwav_seek_origin_current)) { break; }
        bytesRead += 1;
      }
    }
  } else if (allowedMetadataTypes & drwav_metadata_type_unknown) {
    bytesRead = drwav__metadata_process_unknown_chunk(pParser, pChunkID, pChunkHeader->sizeInBytes,
                                                      drwav_metadata_location_top_level);
  }

  return bytesRead;
}

DRWAV_PRIVATE uint32_t drwav_get_bytes_per_pcm_frame(drwav* pWav) {
  uint32_t bytesPerFrame;

  /*
  The bytes per frame is a bit ambiguous. It can be either be based on the bits
  per sample, or the block align. The way I'm doing it here is that if the bits
  per sample is a multiple of 8, use floor(bitsPerSample*channels/8), otherwise
  fall back to the block align.
  */
  if ((pWav->bitsPerSample & 0x7) == 0) {
    /* Bits per sample is a multiple of 8. */
    bytesPerFrame = (pWav->bitsPerSample * pWav->fmt.channels) >> 3;
  } else {
    bytesPerFrame = pWav->fmt.blockAlign;
  }

  /* Validation for known formats. a-law and mu-law should be 1 byte per
   * channel. If it's not, it's not decodable. */
  if (pWav->translatedFormatTag == DR_WAVE_FORMAT_ALAW ||
      pWav->translatedFormatTag == DR_WAVE_FORMAT_MULAW) {
    if (bytesPerFrame != pWav->fmt.channels) { return 0; /* Invalid file. */ }
  }

  return bytesPerFrame;
}

DRWAV_API uint16_t drwav_fmt_get_format(const drwav_fmt* pFMT) {
  if (pFMT == NULL) { return 0; }

  if (pFMT->formatTag != DR_WAVE_FORMAT_EXTENSIBLE) {
    return pFMT->formatTag;
  } else {
    return drwav_bytes_to_u16(pFMT->subFormat); /* Only the first two bytes are required. */
  }
}

DRWAV_PRIVATE bool drwav_preinit(drwav* pWav, drwav_read_proc onRead, drwav_seek_proc onSeek,
                                 void* pReadSeekUserData,
                                 const drwav_allocation_callbacks* pAllocationCallbacks) {
  if (pWav == NULL || onRead == NULL || onSeek == NULL) { return false; }

  DRWAV_ZERO_MEMORY(pWav, sizeof(*pWav));
  pWav->onRead = onRead;
  pWav->onSeek = onSeek;
  pWav->pUserData = pReadSeekUserData;
  pWav->allocationCallbacks = drwav_copy_allocation_callbacks_or_defaults(pAllocationCallbacks);

  if (pWav->allocationCallbacks.onFree == NULL ||
      (pWav->allocationCallbacks.onMalloc == NULL && pWav->allocationCallbacks.onRealloc == NULL)) {
    return false; /* Invalid allocation callbacks. */
  }

  return true;
}

DRWAV_PRIVATE bool drwav_init__internal(drwav* pWav, drwav_chunk_proc onChunk, void* pChunkUserData,
                                        uint32_t flags) {
  /* This function assumes drwav_preinit() has been called beforehand. */

  uint64_t cursor; /* <-- Keeps track of the byte position so we can seek to
                          specific locations. */
  bool sequential;
  uint8_t riff[4];
  drwav_fmt fmt;
  unsigned short translatedFormatTag;
  bool foundDataChunk;
  uint64_t dataChunkSize = 0; /* <-- Important! Don't explicitly set this to 0 anywhere else.
                                     Calculation of the size of the data chunk is performed in
                                     different paths depending on the container. */
  uint64_t sampleCountFromFactChunk = 0; /* Same as dataChunkSize - make sure this is the only
                                                place this is initialized to 0. */
  uint64_t chunkSize;
  drwav__metadata_parser metadataParser;

  cursor = 0;
  sequential = (flags & DRWAV_SEQUENTIAL) != 0;

  /* The first 4 bytes should be the RIFF identifier. */
  if (drwav__on_read(pWav->onRead, pWav->pUserData, riff, sizeof(riff), &cursor) != sizeof(riff)) {
    return false;
  }

  /*
  The first 4 bytes can be used to identify the container. For RIFF files it
  will start with "RIFF" and for w64 it will start with "riff".
  */
  if (drwav_fourcc_equal(riff, "RIFF")) {
    pWav->container = drwav_container_riff;
  } else if (drwav_fourcc_equal(riff, "riff")) {
    int i;
    uint8_t riff2[12];

    pWav->container = drwav_container_w64;

    /* Check the rest of the GUID for validity. */
    if (drwav__on_read(pWav->onRead, pWav->pUserData, riff2, sizeof(riff2), &cursor) !=
        sizeof(riff2)) {
      return false;
    }

    for (i = 0; i < 12; ++i) {
      if (riff2[i] != drwavGUID_W64_RIFF[i + 4]) { return false; }
    }
  } else if (drwav_fourcc_equal(riff, "RF64")) {
    pWav->container = drwav_container_rf64;
  } else {
    return false; /* Unknown or unsupported container. */
  }

  if (pWav->container == drwav_container_riff || pWav->container == drwav_container_rf64) {
    uint8_t chunkSizeBytes[4];
    uint8_t wave[4];

    /* RIFF/WAVE */
    if (drwav__on_read(pWav->onRead, pWav->pUserData, chunkSizeBytes, sizeof(chunkSizeBytes),
                       &cursor) != sizeof(chunkSizeBytes)) {
      return false;
    }

    if (pWav->container == drwav_container_riff) {
      if (drwav_bytes_to_u32(chunkSizeBytes) < 36) {
        return false; /* Chunk size should always be at least 36 bytes. */
      }
    } else {
      if (drwav_bytes_to_u32(chunkSizeBytes) != 0xFFFFFFFF) {
        return false; /* Chunk size should always be set to -1/0xFFFFFFFF
                               for RF64. The actual size is retrieved later. */
      }
    }

    if (drwav__on_read(pWav->onRead, pWav->pUserData, wave, sizeof(wave), &cursor) !=
        sizeof(wave)) {
      return false;
    }

    if (!drwav_fourcc_equal(wave, "WAVE")) { return false; /* Expecting "WAVE". */ }
  } else {
    uint8_t chunkSizeBytes[8];
    uint8_t wave[16];

    /* W64 */
    if (drwav__on_read(pWav->onRead, pWav->pUserData, chunkSizeBytes, sizeof(chunkSizeBytes),
                       &cursor) != sizeof(chunkSizeBytes)) {
      return false;
    }

    if (drwav_bytes_to_u64(chunkSizeBytes) < 80) { return false; }

    if (drwav__on_read(pWav->onRead, pWav->pUserData, wave, sizeof(wave), &cursor) !=
        sizeof(wave)) {
      return false;
    }

    if (!drwav_guid_equal(wave, drwavGUID_W64_WAVE)) { return false; }
  }

  /* For RF64, the "ds64" chunk must come next, before the "fmt " chunk. */
  if (pWav->container == drwav_container_rf64) {
    uint8_t sizeBytes[8];
    uint64_t bytesRemainingInChunk;
    drwav_chunk_header header;
    drwav_result result =
        drwav__read_chunk_header(pWav->onRead, pWav->pUserData, pWav->container, &cursor, &header);
    if (result != DRWAV_SUCCESS) { return false; }

    if (!drwav_fourcc_equal(header.id.fourcc, "ds64")) { return false; /* Expecting "ds64". */ }

    bytesRemainingInChunk = header.sizeInBytes + header.paddingSize;

    /* We don't care about the size of the RIFF chunk - skip it. */
    if (!drwav__seek_forward(pWav->onSeek, 8, pWav->pUserData)) { return false; }
    bytesRemainingInChunk -= 8;
    cursor += 8;

    /* Next 8 bytes is the size of the "data" chunk. */
    if (drwav__on_read(pWav->onRead, pWav->pUserData, sizeBytes, sizeof(sizeBytes), &cursor) !=
        sizeof(sizeBytes)) {
      return false;
    }
    bytesRemainingInChunk -= 8;
    dataChunkSize = drwav_bytes_to_u64(sizeBytes);

    /* Next 8 bytes is the same count which we would usually derived from the
     * FACT chunk if it was available. */
    if (drwav__on_read(pWav->onRead, pWav->pUserData, sizeBytes, sizeof(sizeBytes), &cursor) !=
        sizeof(sizeBytes)) {
      return false;
    }
    bytesRemainingInChunk -= 8;
    sampleCountFromFactChunk = drwav_bytes_to_u64(sizeBytes);

    /* Skip over everything else. */
    if (!drwav__seek_forward(pWav->onSeek, bytesRemainingInChunk, pWav->pUserData)) {
      return false;
    }
    cursor += bytesRemainingInChunk;
  }

  /* The next bytes should be the "fmt " chunk. */
  if (!drwav__read_fmt(pWav->onRead, pWav->onSeek, pWav->pUserData, pWav->container, &cursor,
                       &fmt)) {
    return false; /* Failed to read the "fmt " chunk. */
  }

  /* Basic validation. */
  if ((fmt.sampleRate == 0 || fmt.sampleRate > DRWAV_MAX_SAMPLE_RATE) ||
      (fmt.channels == 0 || fmt.channels > DRWAV_MAX_CHANNELS) ||
      (fmt.bitsPerSample == 0 || fmt.bitsPerSample > DRWAV_MAX_BITS_PER_SAMPLE) ||
      fmt.blockAlign == 0) {
    return false; /* Probably an invalid WAV file. */
  }

  /* Translate the internal format. */
  translatedFormatTag = fmt.formatTag;
  if (translatedFormatTag == DR_WAVE_FORMAT_EXTENSIBLE) {
    translatedFormatTag = drwav_bytes_to_u16(fmt.subFormat + 0);
  }

  memset(&metadataParser, 0, sizeof(metadataParser));

  /* Not tested on W64. */
  if (!sequential && pWav->allowedMetadataTypes != drwav_metadata_type_none &&
      (pWav->container == drwav_container_riff || pWav->container == drwav_container_rf64)) {
    uint64_t cursorForMetadata = cursor;

    metadataParser.onRead = pWav->onRead;
    metadataParser.onSeek = pWav->onSeek;
    metadataParser.pReadSeekUserData = pWav->pUserData;
    metadataParser.stage = drwav__metadata_parser_stage_count;

    for (;;) {
      drwav_result result;
      uint64_t bytesRead;
      uint64_t remainingBytes;
      drwav_chunk_header header;

      result = drwav__read_chunk_header(pWav->onRead, pWav->pUserData, pWav->container,
                                        &cursorForMetadata, &header);
      if (result != DRWAV_SUCCESS) { break; }

      bytesRead =
          drwav__metadata_process_chunk(&metadataParser, &header, pWav->allowedMetadataTypes);
      DRWAV_ASSERT(bytesRead <= header.sizeInBytes);

      remainingBytes = header.sizeInBytes - bytesRead + header.paddingSize;
      if (!drwav__seek_forward(pWav->onSeek, remainingBytes, pWav->pUserData)) { break; }
      cursorForMetadata += remainingBytes;
    }

    if (!drwav__seek_from_start(pWav->onSeek, cursor, pWav->pUserData)) { return false; }

    drwav__metadata_alloc(&metadataParser, &pWav->allocationCallbacks);
    metadataParser.stage = drwav__metadata_parser_stage_read;
  }

  /*
  We need to enumerate over each chunk for two reasons:
    1) The "data" chunk may not be the next one
    2) We may want to report each chunk back to the client

  In order to correctly report each chunk back to the client we will need to
  keep looping until the end of the file.
  */
  foundDataChunk = false;

  /* The next chunk we care about is the "data" chunk. This is not necessarily
   * the next chunk so we'll need to loop. */
  for (;;) {
    drwav_chunk_header header;
    drwav_result result =
        drwav__read_chunk_header(pWav->onRead, pWav->pUserData, pWav->container, &cursor, &header);
    if (result != DRWAV_SUCCESS) {
      if (!foundDataChunk) {
        return false;
      } else {
        break; /* Probably at the end of the file. Get out of the loop. */
      }
    }

    /* Tell the client about this chunk. */
    if (!sequential && onChunk != NULL) {
      uint64_t callbackBytesRead = onChunk(pChunkUserData, pWav->onRead, pWav->onSeek,
                                           pWav->pUserData, &header, pWav->container, &fmt);

      /*
      dr_wav may need to read the contents of the chunk, so we now need to seek
      back to the position before we called the callback.
      */
      if (callbackBytesRead > 0) {
        if (!drwav__seek_from_start(pWav->onSeek, cursor, pWav->pUserData)) { return false; }
      }
    }

    if (!sequential && pWav->allowedMetadataTypes != drwav_metadata_type_none &&
        (pWav->container == drwav_container_riff || pWav->container == drwav_container_rf64)) {
      uint64_t bytesRead =
          drwav__metadata_process_chunk(&metadataParser, &header, pWav->allowedMetadataTypes);

      if (bytesRead > 0) {
        if (!drwav__seek_from_start(pWav->onSeek, cursor, pWav->pUserData)) { return false; }
      }
    }

    if (!foundDataChunk) { pWav->dataChunkDataPos = cursor; }

    chunkSize = header.sizeInBytes;
    if (pWav->container == drwav_container_riff || pWav->container == drwav_container_rf64) {
      if (drwav_fourcc_equal(header.id.fourcc, "data")) {
        foundDataChunk = true;
        if (pWav->container != drwav_container_rf64) { /* The data chunk size for RF64 will always
                                                          be set to 0xFFFFFFFF here. It was set to
                                                          it's true value earlier. */
          dataChunkSize = chunkSize;
        }
      }
    } else {
      if (drwav_guid_equal(header.id.guid, drwavGUID_W64_DATA)) {
        foundDataChunk = true;
        dataChunkSize = chunkSize;
      }
    }

    /*
    If at this point we have found the data chunk and we're running in
    sequential mode, we need to break out of this loop. The reason for this is
    that we would otherwise require a backwards seek which sequential mode
    forbids.
    */
    if (foundDataChunk && sequential) { break; }

    /* Optional. Get the total sample count from the FACT chunk. This is useful
     * for compressed formats. */
    if (pWav->container == drwav_container_riff) {
      if (drwav_fourcc_equal(header.id.fourcc, "fact")) {
        uint32_t sampleCount;
        if (drwav__on_read(pWav->onRead, pWav->pUserData, &sampleCount, 4, &cursor) != 4) {
          return false;
        }
        chunkSize -= 4;

        if (!foundDataChunk) { pWav->dataChunkDataPos = cursor; }

        /*
        The sample count in the "fact" chunk is either unreliable, or I'm not
        understanding it properly. For now I am only enabling this for Microsoft
        ADPCM formats.
        */
        if (pWav->translatedFormatTag == DR_WAVE_FORMAT_ADPCM) {
          sampleCountFromFactChunk = sampleCount;
        } else {
          sampleCountFromFactChunk = 0;
        }
      }
    } else if (pWav->container == drwav_container_w64) {
      if (drwav_guid_equal(header.id.guid, drwavGUID_W64_FACT)) {
        if (drwav__on_read(pWav->onRead, pWav->pUserData, &sampleCountFromFactChunk, 8, &cursor) !=
            8) {
          return false;
        }
        chunkSize -= 8;

        if (!foundDataChunk) { pWav->dataChunkDataPos = cursor; }
      }
    } else if (pWav->container == drwav_container_rf64) {
      /* We retrieved the sample count from the ds64 chunk earlier so no need to
       * do that here. */
    }

    /* Make sure we seek past the padding. */
    chunkSize += header.paddingSize;
    if (!drwav__seek_forward(pWav->onSeek, chunkSize, pWav->pUserData)) { break; }
    cursor += chunkSize;

    if (!foundDataChunk) { pWav->dataChunkDataPos = cursor; }
  }

  pWav->pMetadata = metadataParser.pMetadata;
  pWav->metadataCount = metadataParser.metadataCount;

  /* If we haven't found a data chunk, return an error. */
  if (!foundDataChunk) { return false; }

  /* We may have moved passed the data chunk. If so we need to move back. If
   * running in sequential mode we can assume we are already sitting on the data
   * chunk. */
  if (!sequential) {
    if (!drwav__seek_from_start(pWav->onSeek, pWav->dataChunkDataPos, pWav->pUserData)) {
      return false;
    }
    cursor = pWav->dataChunkDataPos;
  }

  /* At this point we should be sitting on the first byte of the raw audio data.
   */

  pWav->fmt = fmt;
  pWav->sampleRate = fmt.sampleRate;
  pWav->channels = fmt.channels;
  pWav->bitsPerSample = fmt.bitsPerSample;
  pWav->bytesRemaining = dataChunkSize;
  pWav->translatedFormatTag = translatedFormatTag;
  pWav->dataChunkDataSize = dataChunkSize;

  if (sampleCountFromFactChunk != 0) {
    pWav->totalPCMFrameCount = sampleCountFromFactChunk;
  } else {
    pWav->totalPCMFrameCount = dataChunkSize / drwav_get_bytes_per_pcm_frame(pWav);

    if (pWav->translatedFormatTag == DR_WAVE_FORMAT_ADPCM) {
      uint64_t totalBlockHeaderSizeInBytes;
      uint64_t blockCount = dataChunkSize / fmt.blockAlign;

      /* Make sure any trailing partial block is accounted for. */
      if ((blockCount * fmt.blockAlign) < dataChunkSize) { blockCount += 1; }

      /* We decode two samples per byte. There will be blockCount headers in the
       * data chunk. This is enough to know how to calculate the total PCM frame
       * count. */
      totalBlockHeaderSizeInBytes = blockCount * (6 * fmt.channels);
      pWav->totalPCMFrameCount = ((dataChunkSize - totalBlockHeaderSizeInBytes) * 2) / fmt.channels;
    }
    if (pWav->translatedFormatTag == DR_WAVE_FORMAT_DVI_ADPCM) {
      uint64_t totalBlockHeaderSizeInBytes;
      uint64_t blockCount = dataChunkSize / fmt.blockAlign;

      /* Make sure any trailing partial block is accounted for. */
      if ((blockCount * fmt.blockAlign) < dataChunkSize) { blockCount += 1; }

      /* We decode two samples per byte. There will be blockCount headers in the
       * data chunk. This is enough to know how to calculate the total PCM frame
       * count. */
      totalBlockHeaderSizeInBytes = blockCount * (4 * fmt.channels);
      pWav->totalPCMFrameCount = ((dataChunkSize - totalBlockHeaderSizeInBytes) * 2) / fmt.channels;

      /* The header includes a decoded sample for each channel which acts as the
       * initial predictor sample. */
      pWav->totalPCMFrameCount += blockCount;
    }
  }

  /* Some formats only support a certain number of channels. */
  if (pWav->translatedFormatTag == DR_WAVE_FORMAT_ADPCM ||
      pWav->translatedFormatTag == DR_WAVE_FORMAT_DVI_ADPCM) {
    if (pWav->channels > 2) { return false; }
  }

  /* The number of bytes per frame must be known. If not, it's an invalid file
   * and not decodable. */
  if (drwav_get_bytes_per_pcm_frame(pWav) == 0) { return false; }

#ifdef DR_WAV_LIBSNDFILE_COMPAT
  /*
  I use libsndfile as a benchmark for testing, however in the version I'm using
  (from the Windows installer on the libsndfile website), it appears the total
  sample count libsndfile uses for MS-ADPCM is incorrect. It would seem they are
  computing the total sample count from the number of blocks, however this
  results in the inclusion of extra silent samples at the end of the last block.
  The correct way to know the total sample count is to inspect the "fact" chunk,
  which should always be present for compressed formats, and should always
  include the sample count. This little block of code below is only used to
  emulate the libsndfile logic so I can properly run my correctness tests
  against libsndfile, and is disabled by default.
  */
  if (pWav->translatedFormatTag == DR_WAVE_FORMAT_ADPCM) {
    uint64_t blockCount = dataChunkSize / fmt.blockAlign;
    pWav->totalPCMFrameCount = (((blockCount * (fmt.blockAlign - (6 * pWav->channels))) * 2)) /
                               fmt.channels; /* x2 because two samples per byte. */
  }
  if (pWav->translatedFormatTag == DR_WAVE_FORMAT_DVI_ADPCM) {
    uint64_t blockCount = dataChunkSize / fmt.blockAlign;
    pWav->totalPCMFrameCount = (((blockCount * (fmt.blockAlign - (4 * pWav->channels))) * 2) +
                                (blockCount * pWav->channels)) /
                               fmt.channels;
  }
#endif

  return true;
}

DRWAV_API drwav* drwav_init(drwav_read_proc onRead, drwav_seek_proc onSeek, void* pUserData,
                            const drwav_allocation_callbacks* pAllocationCallbacks) {
  return drwav_init_ex(onRead, onSeek, NULL, pUserData, NULL, 0, pAllocationCallbacks);
}

DRWAV_API drwav* drwav_init_ex(drwav_read_proc onRead, drwav_seek_proc onSeek,
                               drwav_chunk_proc onChunk, void* pReadSeekUserData,
                               void* pChunkUserData, uint32_t flags,
                               const drwav_allocation_callbacks* pAllocationCallbacks) {
  drwav* pWav = DRWAV_MALLOC(sizeof(drwav));

  if (!drwav_preinit(pWav, onRead, onSeek, pReadSeekUserData, pAllocationCallbacks)) {
    drwav_free(pWav, pAllocationCallbacks);
    return NULL;
  }

  if (!drwav_init__internal(pWav, onChunk, pChunkUserData, flags)) {
    drwav_free(pWav, pAllocationCallbacks);
    return NULL;
  }

  return pWav;
}

DRWAV_API drwav* drwav_init_with_metadata(drwav_read_proc onRead, drwav_seek_proc onSeek,
                                          void* pUserData, uint32_t flags,
                                          const drwav_allocation_callbacks* pAllocationCallbacks) {
  drwav* pWav = DRWAV_MALLOC(sizeof(drwav));

  if (!drwav_preinit(pWav, onRead, onSeek, pUserData, pAllocationCallbacks)) {
    drwav_free(pWav, pAllocationCallbacks);
    return NULL;
  }

  pWav->allowedMetadataTypes =
      drwav_metadata_type_all_including_unknown; /* <-- Needs to be set to tell
                                                    drwav_init_ex() that we need
                                                    to process metadata. */
  if (!drwav_init__internal(pWav, NULL, NULL, flags)) {
    drwav_free(pWav, pAllocationCallbacks);
    return NULL;
  }

  return pWav;
}

DRWAV_API drwav_metadata* drwav_take_ownership_of_metadata(drwav* pWav) {
  drwav_metadata* result = pWav->pMetadata;

  pWav->pMetadata = NULL;
  pWav->metadataCount = 0;

  return result;
}

DRWAV_PRIVATE size_t drwav__write(drwav* pWav, const void* pData, size_t dataSize) {
  DRWAV_ASSERT(pWav != NULL);
  DRWAV_ASSERT(pWav->onWrite != NULL);

  /* Generic write. Assumes no byte reordering required. */
  return pWav->onWrite(pWav->pUserData, pData, dataSize);
}

DRWAV_PRIVATE size_t drwav__write_byte(drwav* pWav, uint8_t byte) {
  DRWAV_ASSERT(pWav != NULL);
  DRWAV_ASSERT(pWav->onWrite != NULL);

  return pWav->onWrite(pWav->pUserData, &byte, 1);
}

DRWAV_PRIVATE size_t drwav__write_u16ne_to_le(drwav* pWav, uint16_t value) {
  DRWAV_ASSERT(pWav != NULL);
  DRWAV_ASSERT(pWav->onWrite != NULL);

  if (!drwav__is_little_endian()) { value = drwav__bswap16(value); }

  return drwav__write(pWav, &value, 2);
}

DRWAV_PRIVATE size_t drwav__write_u32ne_to_le(drwav* pWav, uint32_t value) {
  DRWAV_ASSERT(pWav != NULL);
  DRWAV_ASSERT(pWav->onWrite != NULL);

  if (!drwav__is_little_endian()) { value = drwav__bswap32(value); }

  return drwav__write(pWav, &value, 4);
}

DRWAV_PRIVATE size_t drwav__write_u64ne_to_le(drwav* pWav, uint64_t value) {
  DRWAV_ASSERT(pWav != NULL);
  DRWAV_ASSERT(pWav->onWrite != NULL);

  if (!drwav__is_little_endian()) { value = drwav__bswap64(value); }

  return drwav__write(pWav, &value, 8);
}

DRWAV_PRIVATE size_t drwav__write_f32ne_to_le(drwav* pWav, float value) {
  union {
    uint32_t u32;
    float f32;
  } u;

  DRWAV_ASSERT(pWav != NULL);
  DRWAV_ASSERT(pWav->onWrite != NULL);

  u.f32 = value;

  if (!drwav__is_little_endian()) { u.u32 = drwav__bswap32(u.u32); }

  return drwav__write(pWav, &u.u32, 4);
}

DRWAV_PRIVATE size_t drwav__write_or_count(drwav* pWav, const void* pData, size_t dataSize) {
  if (pWav == NULL) { return dataSize; }

  return drwav__write(pWav, pData, dataSize);
}

DRWAV_PRIVATE size_t drwav__write_or_count_byte(drwav* pWav, uint8_t byte) {
  if (pWav == NULL) { return 1; }

  return drwav__write_byte(pWav, byte);
}

DRWAV_PRIVATE size_t drwav__write_or_count_u16ne_to_le(drwav* pWav, uint16_t value) {
  if (pWav == NULL) { return 2; }

  return drwav__write_u16ne_to_le(pWav, value);
}

DRWAV_PRIVATE size_t drwav__write_or_count_u32ne_to_le(drwav* pWav, uint32_t value) {
  if (pWav == NULL) { return 4; }

  return drwav__write_u32ne_to_le(pWav, value);
}

#if 0 /* Unused for now. */
DRWAV_PRIVATE size_t drwav__write_or_count_u64ne_to_le(drwav* pWav, uint64_t value)
{
    if (pWav == NULL) {
        return 8;
    }

    return drwav__write_u64ne_to_le(pWav, value);
}
#endif

DRWAV_PRIVATE size_t drwav__write_or_count_f32ne_to_le(drwav* pWav, float value) {
  if (pWav == NULL) { return 4; }

  return drwav__write_f32ne_to_le(pWav, value);
}

DRWAV_PRIVATE size_t drwav__write_or_count_string_to_fixed_size_buf(drwav* pWav, char* str,
                                                                    size_t bufFixedSize) {
  size_t len;

  if (pWav == NULL) { return bufFixedSize; }

  len = drwav__strlen_clamped(str, bufFixedSize);
  drwav__write_or_count(pWav, str, len);

  if (len < bufFixedSize) {
    size_t i;
    for (i = 0; i < bufFixedSize - len; ++i) { drwav__write_byte(pWav, 0); }
  }

  return bufFixedSize;
}

/* pWav can be NULL meaning just count the bytes that would be written. */
DRWAV_PRIVATE size_t drwav__write_or_count_metadata(drwav* pWav, drwav_metadata* pMetadatas,
                                                    uint32_t metadataCount) {
  size_t bytesWritten = 0;
  bool hasListAdtl = false;
  bool hasListInfo = false;
  uint32_t iMetadata;

  if (pMetadatas == NULL || metadataCount == 0) { return 0; }

  for (iMetadata = 0; iMetadata < metadataCount; ++iMetadata) {
    drwav_metadata* pMetadata = &pMetadatas[iMetadata];
    uint32_t chunkSize = 0;

    if ((pMetadata->type & drwav_metadata_type_list_all_info_strings) ||
        (pMetadata->type == drwav_metadata_type_unknown &&
         pMetadata->data.unknown.chunkLocation == drwav_metadata_location_inside_info_list)) {
      hasListInfo = true;
    }

    if ((pMetadata->type & drwav_metadata_type_list_all_adtl) ||
        (pMetadata->type == drwav_metadata_type_unknown &&
         pMetadata->data.unknown.chunkLocation == drwav_metadata_location_inside_adtl_list)) {
      hasListAdtl = true;
    }

    switch (pMetadata->type) {
    case drwav_metadata_type_smpl: {
      uint32_t iLoop;

      chunkSize = DRWAV_SMPL_BYTES + DRWAV_SMPL_LOOP_BYTES * pMetadata->data.smpl.sampleLoopCount +
                  pMetadata->data.smpl.samplerSpecificDataSizeInBytes;

      bytesWritten += drwav__write_or_count(pWav, "smpl", 4);
      bytesWritten += drwav__write_or_count_u32ne_to_le(pWav, chunkSize);

      bytesWritten += drwav__write_or_count_u32ne_to_le(pWav, pMetadata->data.smpl.manufacturerId);
      bytesWritten += drwav__write_or_count_u32ne_to_le(pWav, pMetadata->data.smpl.productId);
      bytesWritten +=
          drwav__write_or_count_u32ne_to_le(pWav, pMetadata->data.smpl.samplePeriodNanoseconds);
      bytesWritten += drwav__write_or_count_u32ne_to_le(pWav, pMetadata->data.smpl.midiUnityNote);
      bytesWritten +=
          drwav__write_or_count_u32ne_to_le(pWav, pMetadata->data.smpl.midiPitchFraction);
      bytesWritten += drwav__write_or_count_u32ne_to_le(pWav, pMetadata->data.smpl.smpteFormat);
      bytesWritten += drwav__write_or_count_u32ne_to_le(pWav, pMetadata->data.smpl.smpteOffset);
      bytesWritten += drwav__write_or_count_u32ne_to_le(pWav, pMetadata->data.smpl.sampleLoopCount);
      bytesWritten += drwav__write_or_count_u32ne_to_le(
          pWav, pMetadata->data.smpl.samplerSpecificDataSizeInBytes);

      for (iLoop = 0; iLoop < pMetadata->data.smpl.sampleLoopCount; ++iLoop) {
        bytesWritten +=
            drwav__write_or_count_u32ne_to_le(pWav, pMetadata->data.smpl.pLoops[iLoop].cuePointId);
        bytesWritten +=
            drwav__write_or_count_u32ne_to_le(pWav, pMetadata->data.smpl.pLoops[iLoop].type);
        bytesWritten += drwav__write_or_count_u32ne_to_le(
            pWav, pMetadata->data.smpl.pLoops[iLoop].firstSampleByteOffset);
        bytesWritten += drwav__write_or_count_u32ne_to_le(
            pWav, pMetadata->data.smpl.pLoops[iLoop].lastSampleByteOffset);
        bytesWritten += drwav__write_or_count_u32ne_to_le(
            pWav, pMetadata->data.smpl.pLoops[iLoop].sampleFraction);
        bytesWritten +=
            drwav__write_or_count_u32ne_to_le(pWav, pMetadata->data.smpl.pLoops[iLoop].playCount);
      }

      if (pMetadata->data.smpl.samplerSpecificDataSizeInBytes > 0) {
        bytesWritten += drwav__write(pWav, pMetadata->data.smpl.pSamplerSpecificData,
                                     pMetadata->data.smpl.samplerSpecificDataSizeInBytes);
      }
    } break;

    case drwav_metadata_type_inst: {
      chunkSize = DRWAV_INST_BYTES;

      bytesWritten += drwav__write_or_count(pWav, "inst", 4);
      bytesWritten += drwav__write_or_count_u32ne_to_le(pWav, chunkSize);
      bytesWritten += drwav__write_or_count(pWav, &pMetadata->data.inst.midiUnityNote, 1);
      bytesWritten += drwav__write_or_count(pWav, &pMetadata->data.inst.fineTuneCents, 1);
      bytesWritten += drwav__write_or_count(pWav, &pMetadata->data.inst.gainDecibels, 1);
      bytesWritten += drwav__write_or_count(pWav, &pMetadata->data.inst.lowNote, 1);
      bytesWritten += drwav__write_or_count(pWav, &pMetadata->data.inst.highNote, 1);
      bytesWritten += drwav__write_or_count(pWav, &pMetadata->data.inst.lowVelocity, 1);
      bytesWritten += drwav__write_or_count(pWav, &pMetadata->data.inst.highVelocity, 1);
    } break;

    case drwav_metadata_type_cue: {
      uint32_t iCuePoint;

      chunkSize = DRWAV_CUE_BYTES + DRWAV_CUE_POINT_BYTES * pMetadata->data.cue.cuePointCount;

      bytesWritten += drwav__write_or_count(pWav, "cue ", 4);
      bytesWritten += drwav__write_or_count_u32ne_to_le(pWav, chunkSize);
      bytesWritten += drwav__write_or_count_u32ne_to_le(pWav, pMetadata->data.cue.cuePointCount);
      for (iCuePoint = 0; iCuePoint < pMetadata->data.cue.cuePointCount; ++iCuePoint) {
        bytesWritten +=
            drwav__write_or_count_u32ne_to_le(pWav, pMetadata->data.cue.pCuePoints[iCuePoint].id);
        bytesWritten += drwav__write_or_count_u32ne_to_le(
            pWav, pMetadata->data.cue.pCuePoints[iCuePoint].playOrderPosition);
        bytesWritten +=
            drwav__write_or_count(pWav, pMetadata->data.cue.pCuePoints[iCuePoint].dataChunkId, 4);
        bytesWritten += drwav__write_or_count_u32ne_to_le(
            pWav, pMetadata->data.cue.pCuePoints[iCuePoint].chunkStart);
        bytesWritten += drwav__write_or_count_u32ne_to_le(
            pWav, pMetadata->data.cue.pCuePoints[iCuePoint].blockStart);
        bytesWritten += drwav__write_or_count_u32ne_to_le(
            pWav, pMetadata->data.cue.pCuePoints[iCuePoint].sampleByteOffset);
      }
    } break;

    case drwav_metadata_type_acid: {
      chunkSize = DRWAV_ACID_BYTES;

      bytesWritten += drwav__write_or_count(pWav, "acid", 4);
      bytesWritten += drwav__write_or_count_u32ne_to_le(pWav, chunkSize);
      bytesWritten += drwav__write_or_count_u32ne_to_le(pWav, pMetadata->data.acid.flags);
      bytesWritten += drwav__write_or_count_u16ne_to_le(pWav, pMetadata->data.acid.midiUnityNote);
      bytesWritten += drwav__write_or_count_u16ne_to_le(pWav, pMetadata->data.acid.reserved1);
      bytesWritten += drwav__write_or_count_f32ne_to_le(pWav, pMetadata->data.acid.reserved2);
      bytesWritten += drwav__write_or_count_u32ne_to_le(pWav, pMetadata->data.acid.numBeats);
      bytesWritten +=
          drwav__write_or_count_u16ne_to_le(pWav, pMetadata->data.acid.meterDenominator);
      bytesWritten += drwav__write_or_count_u16ne_to_le(pWav, pMetadata->data.acid.meterNumerator);
      bytesWritten += drwav__write_or_count_f32ne_to_le(pWav, pMetadata->data.acid.tempo);
    } break;

    case drwav_metadata_type_bext: {
      char reservedBuf[DRWAV_BEXT_RESERVED_BYTES];
      uint32_t timeReferenceLow;
      uint32_t timeReferenceHigh;

      chunkSize = DRWAV_BEXT_BYTES + pMetadata->data.bext.codingHistorySize;

      bytesWritten += drwav__write_or_count(pWav, "bext", 4);
      bytesWritten += drwav__write_or_count_u32ne_to_le(pWav, chunkSize);

      bytesWritten += drwav__write_or_count_string_to_fixed_size_buf(
          pWav, pMetadata->data.bext.pDescription, DRWAV_BEXT_DESCRIPTION_BYTES);
      bytesWritten += drwav__write_or_count_string_to_fixed_size_buf(
          pWav, pMetadata->data.bext.pOriginatorName, DRWAV_BEXT_ORIGINATOR_NAME_BYTES);
      bytesWritten += drwav__write_or_count_string_to_fixed_size_buf(
          pWav, pMetadata->data.bext.pOriginatorReference, DRWAV_BEXT_ORIGINATOR_REF_BYTES);
      bytesWritten += drwav__write_or_count(pWav, pMetadata->data.bext.pOriginationDate,
                                            sizeof(pMetadata->data.bext.pOriginationDate));
      bytesWritten += drwav__write_or_count(pWav, pMetadata->data.bext.pOriginationTime,
                                            sizeof(pMetadata->data.bext.pOriginationTime));

      timeReferenceLow = (uint32_t)(pMetadata->data.bext.timeReference & 0xFFFFFFFF);
      timeReferenceHigh = (uint32_t)(pMetadata->data.bext.timeReference >> 32);
      bytesWritten += drwav__write_or_count_u32ne_to_le(pWav, timeReferenceLow);
      bytesWritten += drwav__write_or_count_u32ne_to_le(pWav, timeReferenceHigh);

      bytesWritten += drwav__write_or_count_u16ne_to_le(pWav, pMetadata->data.bext.version);
      bytesWritten +=
          drwav__write_or_count(pWav, pMetadata->data.bext.pUMID, DRWAV_BEXT_UMID_BYTES);
      bytesWritten += drwav__write_or_count_u16ne_to_le(pWav, pMetadata->data.bext.loudnessValue);
      bytesWritten += drwav__write_or_count_u16ne_to_le(pWav, pMetadata->data.bext.loudnessRange);
      bytesWritten +=
          drwav__write_or_count_u16ne_to_le(pWav, pMetadata->data.bext.maxTruePeakLevel);
      bytesWritten +=
          drwav__write_or_count_u16ne_to_le(pWav, pMetadata->data.bext.maxMomentaryLoudness);
      bytesWritten +=
          drwav__write_or_count_u16ne_to_le(pWav, pMetadata->data.bext.maxShortTermLoudness);

      memset(reservedBuf, 0, sizeof(reservedBuf));
      bytesWritten += drwav__write_or_count(pWav, reservedBuf, sizeof(reservedBuf));

      if (pMetadata->data.bext.codingHistorySize > 0) {
        bytesWritten += drwav__write_or_count(pWav, pMetadata->data.bext.pCodingHistory,
                                              pMetadata->data.bext.codingHistorySize);
      }
    } break;

    case drwav_metadata_type_unknown: {
      if (pMetadata->data.unknown.chunkLocation == drwav_metadata_location_top_level) {
        chunkSize = pMetadata->data.unknown.dataSizeInBytes;

        bytesWritten += drwav__write_or_count(pWav, pMetadata->data.unknown.id, 4);
        bytesWritten += drwav__write_or_count_u32ne_to_le(pWav, chunkSize);
        bytesWritten += drwav__write_or_count(pWav, pMetadata->data.unknown.pData,
                                              pMetadata->data.unknown.dataSizeInBytes);
      }
    } break;

    default: break;
    }
    if ((chunkSize % 2) != 0) { bytesWritten += drwav__write_or_count_byte(pWav, 0); }
  }

  if (hasListInfo) {
    uint32_t chunkSize = 4; /* Start with 4 bytes for "INFO". */
    for (iMetadata = 0; iMetadata < metadataCount; ++iMetadata) {
      drwav_metadata* pMetadata = &pMetadatas[iMetadata];

      if ((pMetadata->type & drwav_metadata_type_list_all_info_strings)) {
        chunkSize += 8;                                         /* For id and string size. */
        chunkSize += pMetadata->data.infoText.stringLength + 1; /* Include null terminator. */
      } else if (pMetadata->type == drwav_metadata_type_unknown &&
                 pMetadata->data.unknown.chunkLocation ==
                     drwav_metadata_location_inside_info_list) {
        chunkSize += 8; /* For id string size. */
        chunkSize += pMetadata->data.unknown.dataSizeInBytes;
      }

      if ((chunkSize % 2) != 0) { chunkSize += 1; }
    }

    bytesWritten += drwav__write_or_count(pWav, "LIST", 4);
    bytesWritten += drwav__write_or_count_u32ne_to_le(pWav, chunkSize);
    bytesWritten += drwav__write_or_count(pWav, "INFO", 4);

    for (iMetadata = 0; iMetadata < metadataCount; ++iMetadata) {
      drwav_metadata* pMetadata = &pMetadatas[iMetadata];
      uint32_t subchunkSize = 0;

      if (pMetadata->type & drwav_metadata_type_list_all_info_strings) {
        const char* pID = NULL;

        switch (pMetadata->type) {
        case drwav_metadata_type_list_info_software: pID = "ISFT"; break;
        case drwav_metadata_type_list_info_copyright: pID = "ICOP"; break;
        case drwav_metadata_type_list_info_title: pID = "INAM"; break;
        case drwav_metadata_type_list_info_artist: pID = "IART"; break;
        case drwav_metadata_type_list_info_comment: pID = "ICMT"; break;
        case drwav_metadata_type_list_info_date: pID = "ICRD"; break;
        case drwav_metadata_type_list_info_genre: pID = "IGNR"; break;
        case drwav_metadata_type_list_info_album: pID = "IPRD"; break;
        case drwav_metadata_type_list_info_tracknumber: pID = "ITRK"; break;
        default: break;
        }

        DRWAV_ASSERT(pID != NULL);

        if (pMetadata->data.infoText.stringLength) {
          subchunkSize = pMetadata->data.infoText.stringLength + 1;
          bytesWritten += drwav__write_or_count(pWav, pID, 4);
          bytesWritten += drwav__write_or_count_u32ne_to_le(pWav, subchunkSize);
          bytesWritten += drwav__write_or_count(pWav, pMetadata->data.infoText.pString,
                                                pMetadata->data.infoText.stringLength);
          bytesWritten += drwav__write_or_count_byte(pWav, '\0');
        }
      } else if (pMetadata->type == drwav_metadata_type_unknown &&
                 pMetadata->data.unknown.chunkLocation ==
                     drwav_metadata_location_inside_info_list) {
        if (pMetadata->data.unknown.dataSizeInBytes) {
          subchunkSize = pMetadata->data.unknown.dataSizeInBytes;

          bytesWritten += drwav__write_or_count(pWav, pMetadata->data.unknown.id, 4);
          bytesWritten +=
              drwav__write_or_count_u32ne_to_le(pWav, pMetadata->data.unknown.dataSizeInBytes);
          bytesWritten += drwav__write_or_count(pWav, pMetadata->data.unknown.pData, subchunkSize);
        }
      }

      if ((subchunkSize % 2) != 0) { bytesWritten += drwav__write_or_count_byte(pWav, 0); }
    }
  }

  if (hasListAdtl) {
    uint32_t chunkSize = 4; /* start with 4 bytes for "adtl" */

    for (iMetadata = 0; iMetadata < metadataCount; ++iMetadata) {
      drwav_metadata* pMetadata = &pMetadatas[iMetadata];

      switch (pMetadata->type) {
      case drwav_metadata_type_list_label:
      case drwav_metadata_type_list_note: {
        chunkSize += 8; /* for id and chunk size */
        chunkSize += DRWAV_LIST_LABEL_OR_NOTE_BYTES;

        if (pMetadata->data.labelOrNote.stringLength > 0) {
          chunkSize += pMetadata->data.labelOrNote.stringLength + 1;
        }
      } break;

      case drwav_metadata_type_list_labelled_cue_region: {
        chunkSize += 8; /* for id and chunk size */
        chunkSize += DRWAV_LIST_LABELLED_TEXT_BYTES;

        if (pMetadata->data.labelledCueRegion.stringLength > 0) {
          chunkSize += pMetadata->data.labelledCueRegion.stringLength + 1;
        }
      } break;

      case drwav_metadata_type_unknown: {
        if (pMetadata->data.unknown.chunkLocation == drwav_metadata_location_inside_adtl_list) {
          chunkSize += 8; /* for id and chunk size */
          chunkSize += pMetadata->data.unknown.dataSizeInBytes;
        }
      } break;

      default: break;
      }

      if ((chunkSize % 2) != 0) { chunkSize += 1; }
    }

    bytesWritten += drwav__write_or_count(pWav, "LIST", 4);
    bytesWritten += drwav__write_or_count_u32ne_to_le(pWav, chunkSize);
    bytesWritten += drwav__write_or_count(pWav, "adtl", 4);

    for (iMetadata = 0; iMetadata < metadataCount; ++iMetadata) {
      drwav_metadata* pMetadata = &pMetadatas[iMetadata];
      uint32_t subchunkSize = 0;

      switch (pMetadata->type) {
      case drwav_metadata_type_list_label:
      case drwav_metadata_type_list_note: {
        if (pMetadata->data.labelOrNote.stringLength > 0) {
          const char* pID = NULL;

          if (pMetadata->type == drwav_metadata_type_list_label) {
            pID = "labl";
          } else if (pMetadata->type == drwav_metadata_type_list_note) {
            pID = "note";
          }

          DRWAV_ASSERT(pID != NULL);
          DRWAV_ASSERT(pMetadata->data.labelOrNote.pString != NULL);

          subchunkSize = DRWAV_LIST_LABEL_OR_NOTE_BYTES;

          bytesWritten += drwav__write_or_count(pWav, pID, 4);
          subchunkSize += pMetadata->data.labelOrNote.stringLength + 1;
          bytesWritten += drwav__write_or_count_u32ne_to_le(pWav, subchunkSize);

          bytesWritten +=
              drwav__write_or_count_u32ne_to_le(pWav, pMetadata->data.labelOrNote.cuePointId);
          bytesWritten += drwav__write_or_count(pWav, pMetadata->data.labelOrNote.pString,
                                                pMetadata->data.labelOrNote.stringLength);
          bytesWritten += drwav__write_or_count_byte(pWav, '\0');
        }
      } break;

      case drwav_metadata_type_list_labelled_cue_region: {
        subchunkSize = DRWAV_LIST_LABELLED_TEXT_BYTES;

        bytesWritten += drwav__write_or_count(pWav, "ltxt", 4);
        if (pMetadata->data.labelledCueRegion.stringLength > 0) {
          subchunkSize += pMetadata->data.labelledCueRegion.stringLength + 1;
        }
        bytesWritten += drwav__write_or_count_u32ne_to_le(pWav, subchunkSize);
        bytesWritten +=
            drwav__write_or_count_u32ne_to_le(pWav, pMetadata->data.labelledCueRegion.cuePointId);
        bytesWritten +=
            drwav__write_or_count_u32ne_to_le(pWav, pMetadata->data.labelledCueRegion.sampleLength);
        bytesWritten += drwav__write_or_count(pWav, pMetadata->data.labelledCueRegion.purposeId, 4);
        bytesWritten +=
            drwav__write_or_count_u16ne_to_le(pWav, pMetadata->data.labelledCueRegion.country);
        bytesWritten +=
            drwav__write_or_count_u16ne_to_le(pWav, pMetadata->data.labelledCueRegion.language);
        bytesWritten +=
            drwav__write_or_count_u16ne_to_le(pWav, pMetadata->data.labelledCueRegion.dialect);
        bytesWritten +=
            drwav__write_or_count_u16ne_to_le(pWav, pMetadata->data.labelledCueRegion.codePage);

        if (pMetadata->data.labelledCueRegion.stringLength > 0) {
          DRWAV_ASSERT(pMetadata->data.labelledCueRegion.pString != NULL);

          bytesWritten += drwav__write_or_count(pWav, pMetadata->data.labelledCueRegion.pString,
                                                pMetadata->data.labelledCueRegion.stringLength);
          bytesWritten += drwav__write_or_count_byte(pWav, '\0');
        }
      } break;

      case drwav_metadata_type_unknown: {
        if (pMetadata->data.unknown.chunkLocation == drwav_metadata_location_inside_adtl_list) {
          subchunkSize = pMetadata->data.unknown.dataSizeInBytes;

          DRWAV_ASSERT(pMetadata->data.unknown.pData != NULL);
          bytesWritten += drwav__write_or_count(pWav, pMetadata->data.unknown.id, 4);
          bytesWritten += drwav__write_or_count_u32ne_to_le(pWav, subchunkSize);
          bytesWritten += drwav__write_or_count(pWav, pMetadata->data.unknown.pData, subchunkSize);
        }
      } break;

      default: break;
      }

      if ((subchunkSize % 2) != 0) { bytesWritten += drwav__write_or_count_byte(pWav, 0); }
    }
  }

  DRWAV_ASSERT((bytesWritten % 2) == 0);

  return bytesWritten;
}

DRWAV_PRIVATE uint32_t drwav__riff_chunk_size_riff(uint64_t dataChunkSize,
                                                   drwav_metadata* pMetadata,
                                                   uint32_t metadataCount) {
  uint64_t chunkSize =
      4 + 24 + (uint64_t)drwav__write_or_count_metadata(NULL, pMetadata, metadataCount) + 8 +
      dataChunkSize +
      drwav__chunk_padding_size_riff(dataChunkSize); /* 4 = "WAVE". 24 = "fmt " chunk.
                                                        8 = "data" + u32 data size. */
  if (chunkSize > 0xFFFFFFFFUL) { chunkSize = 0xFFFFFFFFUL; }

  return (uint32_t)chunkSize; /* Safe cast due to the clamp above. */
}

DRWAV_PRIVATE uint32_t drwav__data_chunk_size_riff(uint64_t dataChunkSize) {
  if (dataChunkSize <= 0xFFFFFFFFUL) {
    return (uint32_t)dataChunkSize;
  } else {
    return 0xFFFFFFFFUL;
  }
}

DRWAV_PRIVATE uint64_t drwav__riff_chunk_size_w64(uint64_t dataChunkSize) {
  uint64_t dataSubchunkPaddingSize = drwav__chunk_padding_size_w64(dataChunkSize);

  return 80 + 24 + dataChunkSize + dataSubchunkPaddingSize; /* +24 because W64 includes the size of
                                                               the GUID and size fields. */
}

DRWAV_PRIVATE uint64_t drwav__data_chunk_size_w64(uint64_t dataChunkSize) {
  return 24 + dataChunkSize; /* +24 because W64 includes the size of the GUID
                                and size fields. */
}

DRWAV_PRIVATE uint64_t drwav__riff_chunk_size_rf64(uint64_t dataChunkSize, drwav_metadata* metadata,
                                                   uint32_t numMetadata) {
  uint64_t chunkSize =
      4 + 36 + 24 + (uint64_t)drwav__write_or_count_metadata(NULL, metadata, numMetadata) + 8 +
      dataChunkSize +
      drwav__chunk_padding_size_riff(dataChunkSize); /* 4 = "WAVE". 36 = "ds64" chunk. 24 = "fmt "
                                                        chunk. 8 = "data" + u32 data size. */
  if (chunkSize > 0xFFFFFFFFUL) { chunkSize = 0xFFFFFFFFUL; }

  return chunkSize;
}

DRWAV_PRIVATE uint64_t drwav__data_chunk_size_rf64(uint64_t dataChunkSize) { return dataChunkSize; }

DRWAV_PRIVATE bool drwav_preinit_write(drwav* pWav, const drwav_data_format* pFormat,
                                       bool isSequential, drwav_write_proc onWrite,
                                       drwav_seek_proc onSeek, void* pUserData,
                                       const drwav_allocation_callbacks* pAllocationCallbacks) {
  if (pWav == NULL || onWrite == NULL) { return false; }

  if (!isSequential && onSeek == NULL) {
    return false; /* <-- onSeek is required when in non-sequential mode.
                   */
  }

  /* Not currently supporting compressed formats. Will need to add support for
   * the "fact" chunk before we enable this. */
  if (pFormat->format == DR_WAVE_FORMAT_EXTENSIBLE) { return false; }
  if (pFormat->format == DR_WAVE_FORMAT_ADPCM || pFormat->format == DR_WAVE_FORMAT_DVI_ADPCM) {
    return false;
  }

  DRWAV_ZERO_MEMORY(pWav, sizeof(*pWav));
  pWav->onWrite = onWrite;
  pWav->onSeek = onSeek;
  pWav->pUserData = pUserData;
  pWav->allocationCallbacks = drwav_copy_allocation_callbacks_or_defaults(pAllocationCallbacks);

  if (pWav->allocationCallbacks.onFree == NULL ||
      (pWav->allocationCallbacks.onMalloc == NULL && pWav->allocationCallbacks.onRealloc == NULL)) {
    return false; /* Invalid allocation callbacks. */
  }

  pWav->fmt.formatTag = (uint16_t)pFormat->format;
  pWav->fmt.channels = (uint16_t)pFormat->channels;
  pWav->fmt.sampleRate = pFormat->sampleRate;
  pWav->fmt.avgBytesPerSec =
      (uint32_t)((pFormat->bitsPerSample * pFormat->sampleRate * pFormat->channels) / 8);
  pWav->fmt.blockAlign = (uint16_t)((pFormat->channels * pFormat->bitsPerSample) / 8);
  pWav->fmt.bitsPerSample = (uint16_t)pFormat->bitsPerSample;
  pWav->fmt.extendedSize = 0;
  pWav->isSequentialWrite = isSequential;

  return true;
}

DRWAV_PRIVATE bool drwav_init_write__internal(drwav* pWav, const drwav_data_format* pFormat,
                                              uint64_t totalSampleCount) {
  /* The function assumes drwav_preinit_write() was called beforehand. */

  size_t runningPos = 0;
  uint64_t initialDataChunkSize = 0;
  uint64_t chunkSizeFMT;

  /*
  The initial values for the "RIFF" and "data" chunks depends on whether or not
  we are initializing in sequential mode or not. In sequential mode we set this
  to its final values straight away since they can be calculated from the total
  sample count. In non- sequential mode we initialize it all to zero and fill it
  out in drwav_uninit() using a backwards seek.
  */
  if (pWav->isSequentialWrite) {
    initialDataChunkSize = (totalSampleCount * pWav->fmt.bitsPerSample) / 8;

    /*
    The RIFF container has a limit on the number of samples. drwav is not
    allowing this. There's no practical limits for Wave64 so for the sake of
    simplicity I'm not doing any validation for that.
    */
    if (pFormat->container == drwav_container_riff) {
      if (initialDataChunkSize > (0xFFFFFFFFUL - 36)) {
        return false; /* Not enough room to store every sample. */
      }
    }
  }

  pWav->dataChunkDataSizeTargetWrite = initialDataChunkSize;

  /* "RIFF" chunk. */
  if (pFormat->container == drwav_container_riff) {
    uint32_t chunkSizeRIFF =
        28 + (uint32_t)initialDataChunkSize; /* +28 = "WAVE" + [sizeof "fmt " chunk] */
    runningPos += drwav__write(pWav, "RIFF", 4);
    runningPos += drwav__write_u32ne_to_le(pWav, chunkSizeRIFF);
    runningPos += drwav__write(pWav, "WAVE", 4);
  } else if (pFormat->container == drwav_container_w64) {
    uint64_t chunkSizeRIFF = 80 + 24 + initialDataChunkSize; /* +24 because W64 includes the size of
                                                                the GUID and size fields. */
    runningPos += drwav__write(pWav, drwavGUID_W64_RIFF, 16);
    runningPos += drwav__write_u64ne_to_le(pWav, chunkSizeRIFF);
    runningPos += drwav__write(pWav, drwavGUID_W64_WAVE, 16);
  } else if (pFormat->container == drwav_container_rf64) {
    runningPos += drwav__write(pWav, "RF64", 4);
    runningPos +=
        drwav__write_u32ne_to_le(pWav, 0xFFFFFFFF); /* Always 0xFFFFFFFF for RF64. Set to a proper
                                                       value in the "ds64" chunk. */
    runningPos += drwav__write(pWav, "WAVE", 4);
  }

  /* "ds64" chunk (RF64 only). */
  if (pFormat->container == drwav_container_rf64) {
    uint32_t initialds64ChunkSize = 28; /* 28 = [Size of RIFF (8 bytes)] + [Size of DATA (8
                                               bytes)] + [Sample Count (8 bytes)] + [Table Length (4
                                               bytes)]. Table length always set to 0. */
    uint64_t initialRiffChunkSize =
        8 + initialds64ChunkSize + initialDataChunkSize; /* +8 for the ds64 header. */

    runningPos += drwav__write(pWav, "ds64", 4);
    runningPos += drwav__write_u32ne_to_le(pWav, initialds64ChunkSize); /* Size of ds64. */
    runningPos += drwav__write_u64ne_to_le(
        pWav, initialRiffChunkSize); /* Size of RIFF. Set to true value at the end. */
    runningPos += drwav__write_u64ne_to_le(
        pWav, initialDataChunkSize); /* Size of DATA. Set to true value at the end. */
    runningPos += drwav__write_u64ne_to_le(pWav, totalSampleCount); /* Sample count. */
    runningPos +=
        drwav__write_u32ne_to_le(pWav, 0); /* Table length. Always set to zero in our case since
                                              we're not doing any other chunks than "DATA". */
  }

  /* "fmt " chunk. */
  if (pFormat->container == drwav_container_riff || pFormat->container == drwav_container_rf64) {
    chunkSizeFMT = 16;
    runningPos += drwav__write(pWav, "fmt ", 4);
    runningPos += drwav__write_u32ne_to_le(pWav, (uint32_t)chunkSizeFMT);
  } else if (pFormat->container == drwav_container_w64) {
    chunkSizeFMT = 40;
    runningPos += drwav__write(pWav, drwavGUID_W64_FMT, 16);
    runningPos += drwav__write_u64ne_to_le(pWav, chunkSizeFMT);
  }

  runningPos += drwav__write_u16ne_to_le(pWav, pWav->fmt.formatTag);
  runningPos += drwav__write_u16ne_to_le(pWav, pWav->fmt.channels);
  runningPos += drwav__write_u32ne_to_le(pWav, pWav->fmt.sampleRate);
  runningPos += drwav__write_u32ne_to_le(pWav, pWav->fmt.avgBytesPerSec);
  runningPos += drwav__write_u16ne_to_le(pWav, pWav->fmt.blockAlign);
  runningPos += drwav__write_u16ne_to_le(pWav, pWav->fmt.bitsPerSample);

  /* TODO: is a 'fact' chunk required for DR_WAVE_FORMAT_IEEE_FLOAT? */

  if (!pWav->isSequentialWrite && pWav->pMetadata != NULL && pWav->metadataCount > 0 &&
      (pFormat->container == drwav_container_riff || pFormat->container == drwav_container_rf64)) {
    runningPos += drwav__write_or_count_metadata(pWav, pWav->pMetadata, pWav->metadataCount);
  }

  pWav->dataChunkDataPos = runningPos;

  /* "data" chunk. */
  if (pFormat->container == drwav_container_riff) {
    uint32_t chunkSizeDATA = (uint32_t)initialDataChunkSize;
    runningPos += drwav__write(pWav, "data", 4);
    runningPos += drwav__write_u32ne_to_le(pWav, chunkSizeDATA);
  } else if (pFormat->container == drwav_container_w64) {
    uint64_t chunkSizeDATA = 24 + initialDataChunkSize; /* +24 because W64 includes the size of
                                                               the GUID and size fields. */
    runningPos += drwav__write(pWav, drwavGUID_W64_DATA, 16);
    runningPos += drwav__write_u64ne_to_le(pWav, chunkSizeDATA);
  } else if (pFormat->container == drwav_container_rf64) {
    runningPos += drwav__write(pWav, "data", 4);
    runningPos += drwav__write_u32ne_to_le(
        pWav, 0xFFFFFFFF); /* Always set to 0xFFFFFFFF for RF64. The true size of the
                              data chunk is specified in the ds64 chunk. */
  }

  /* Set some properties for the client's convenience. */
  pWav->container = pFormat->container;
  pWav->channels = (uint16_t)pFormat->channels;
  pWav->sampleRate = pFormat->sampleRate;
  pWav->bitsPerSample = (uint16_t)pFormat->bitsPerSample;
  pWav->translatedFormatTag = (uint16_t)pFormat->format;
  pWav->dataChunkDataPos = runningPos;

  return true;
}

DRWAV_API drwav* drwav_init_write(const drwav_data_format* pFormat, drwav_write_proc onWrite,
                                  drwav_seek_proc onSeek, void* pUserData,
                                  const drwav_allocation_callbacks* pAllocationCallbacks) {
  drwav* pWav = DRWAV_MALLOC(sizeof(drwav));
  if (!drwav_preinit_write(pWav, pFormat, false, onWrite, onSeek, pUserData,
                           pAllocationCallbacks)) {
    drwav_free(pWav, pAllocationCallbacks);
    return NULL;
  }

  if (!drwav_init_write__internal(pWav, pFormat, 0)) { /* false = Not Sequential */
    drwav_free(pWav, pAllocationCallbacks);
    return NULL;
  }

  return pWav;
}

DRWAV_API drwav*
drwav_init_write_sequential(const drwav_data_format* pFormat, uint64_t totalSampleCount,
                            drwav_write_proc onWrite, void* pUserData,
                            const drwav_allocation_callbacks* pAllocationCallbacks) {
  drwav* pWav = DRWAV_MALLOC(sizeof(drwav));

  if (!drwav_preinit_write(pWav, pFormat, true, onWrite, NULL, pUserData, pAllocationCallbacks)) {
    drwav_free(pWav, pAllocationCallbacks);
    return NULL;
  }

  if (!drwav_init_write__internal(pWav, pFormat, totalSampleCount)) { /* true = Sequential */
    drwav_free(pWav, pAllocationCallbacks);
    return NULL;
  }

  return pWav;
}

DRWAV_API drwav* drwav_init_write_sequential_pcm_frames(
    const drwav_data_format* pFormat, uint64_t totalPCMFrameCount, drwav_write_proc onWrite,
    void* pUserData, const drwav_allocation_callbacks* pAllocationCallbacks) {
  if (pFormat == NULL) { return NULL; }

  return drwav_init_write_sequential(pFormat, totalPCMFrameCount * pFormat->channels, onWrite,
                                     pUserData, pAllocationCallbacks);
}

DRWAV_API drwav*
drwav_init_write_with_metadata(const drwav_data_format* pFormat, drwav_write_proc onWrite,
                               drwav_seek_proc onSeek, void* pUserData,
                               const drwav_allocation_callbacks* pAllocationCallbacks,
                               drwav_metadata* pMetadata, uint32_t metadataCount) {
  drwav* pWav = DRWAV_MALLOC(sizeof(drwav));
  if (!drwav_preinit_write(pWav, pFormat, false, onWrite, onSeek, pUserData,
                           pAllocationCallbacks)) {
    drwav_free(pWav, pAllocationCallbacks);
    return NULL;
  }

  pWav->pMetadata = pMetadata;
  pWav->metadataCount = metadataCount;

  if (!drwav_init_write__internal(pWav, pFormat, 0)) { return NULL; }

  return pWav;
}

DRWAV_API uint64_t drwav_target_write_size_bytes(const drwav_data_format* pFormat,
                                                 uint64_t totalFrameCount,
                                                 drwav_metadata* pMetadata,
                                                 uint32_t metadataCount) {
  /* Casting totalFrameCount to int64_t for VC6 compatibility. No issues in
   * practice because nobody is going to exhaust the whole 63 bits. */
  uint64_t targetDataSizeBytes =
      (uint64_t)((int64_t)totalFrameCount * pFormat->channels * pFormat->bitsPerSample / 8.0);
  uint64_t riffChunkSizeBytes;
  uint64_t fileSizeBytes = 0;

  if (pFormat->container == drwav_container_riff) {
    riffChunkSizeBytes = drwav__riff_chunk_size_riff(targetDataSizeBytes, pMetadata, metadataCount);
    fileSizeBytes = (8 + riffChunkSizeBytes); /* +8 because WAV doesn't include the size of
                                                 the ChunkID and ChunkSize fields. */
  } else if (pFormat->container == drwav_container_w64) {
    riffChunkSizeBytes = drwav__riff_chunk_size_w64(targetDataSizeBytes);
    fileSizeBytes = riffChunkSizeBytes;
  } else if (pFormat->container == drwav_container_rf64) {
    riffChunkSizeBytes = drwav__riff_chunk_size_rf64(targetDataSizeBytes, pMetadata, metadataCount);
    fileSizeBytes = (8 + riffChunkSizeBytes); /* +8 because WAV doesn't include the size of
                                                 the ChunkID and ChunkSize fields. */
  }

  return fileSizeBytes;
}

#ifndef DR_WAV_NO_STDIO

/* drwav_result_from_errno() is only used for fopen() and wfopen() so putting it
 * inside DR_WAV_NO_STDIO for now. If something else needs this later we can
 * move it out. */
#include <errno.h>
DRWAV_PRIVATE drwav_result drwav_result_from_errno(int e) {
  switch (e) {
  case 0: return DRWAV_SUCCESS;
#ifdef EPERM
  case EPERM: return DRWAV_INVALID_OPERATION;
#endif
#ifdef ENOENT
  case ENOENT: return DRWAV_DOES_NOT_EXIST;
#endif
#ifdef ESRCH
  case ESRCH: return DRWAV_DOES_NOT_EXIST;
#endif
#ifdef EINTR
  case EINTR: return DRWAV_INTERRUPT;
#endif
#ifdef EIO
  case EIO: return DRWAV_IO_ERROR;
#endif
#ifdef ENXIO
  case ENXIO: return DRWAV_DOES_NOT_EXIST;
#endif
#ifdef E2BIG
  case E2BIG: return DRWAV_INVALID_ARGS;
#endif
#ifdef ENOEXEC
  case ENOEXEC: return DRWAV_INVALID_FILE;
#endif
#ifdef EBADF
  case EBADF: return DRWAV_INVALID_FILE;
#endif
#ifdef ECHILD
  case ECHILD: return DRWAV_ERROR;
#endif
#ifdef EAGAIN
  case EAGAIN: return DRWAV_UNAVAILABLE;
#endif
#ifdef ENOMEM
  case ENOMEM: return DRWAV_OUT_OF_MEMORY;
#endif
#ifdef EACCES
  case EACCES: return DRWAV_ACCESS_DENIED;
#endif
#ifdef EFAULT
  case EFAULT: return DRWAV_BAD_ADDRESS;
#endif
#ifdef ENOTBLK
  case ENOTBLK: return DRWAV_ERROR;
#endif
#ifdef EBUSY
  case EBUSY: return DRWAV_BUSY;
#endif
#ifdef EEXIST
  case EEXIST: return DRWAV_ALREADY_EXISTS;
#endif
#ifdef EXDEV
  case EXDEV: return DRWAV_ERROR;
#endif
#ifdef ENODEV
  case ENODEV: return DRWAV_DOES_NOT_EXIST;
#endif
#ifdef ENOTDIR
  case ENOTDIR: return DRWAV_NOT_DIRECTORY;
#endif
#ifdef EISDIR
  case EISDIR: return DRWAV_IS_DIRECTORY;
#endif
#ifdef EINVAL
  case EINVAL: return DRWAV_INVALID_ARGS;
#endif
#ifdef ENFILE
  case ENFILE: return DRWAV_TOO_MANY_OPEN_FILES;
#endif
#ifdef EMFILE
  case EMFILE: return DRWAV_TOO_MANY_OPEN_FILES;
#endif
#ifdef ENOTTY
  case ENOTTY: return DRWAV_INVALID_OPERATION;
#endif
#ifdef ETXTBSY
  case ETXTBSY: return DRWAV_BUSY;
#endif
#ifdef EFBIG
  case EFBIG: return DRWAV_TOO_BIG;
#endif
#ifdef ENOSPC
  case ENOSPC: return DRWAV_NO_SPACE;
#endif
#ifdef ESPIPE
  case ESPIPE: return DRWAV_BAD_SEEK;
#endif
#ifdef EROFS
  case EROFS: return DRWAV_ACCESS_DENIED;
#endif
#ifdef EMLINK
  case EMLINK: return DRWAV_TOO_MANY_LINKS;
#endif
#ifdef EPIPE
  case EPIPE: return DRWAV_BAD_PIPE;
#endif
#ifdef EDOM
  case EDOM: return DRWAV_OUT_OF_RANGE;
#endif
#ifdef ERANGE
  case ERANGE: return DRWAV_OUT_OF_RANGE;
#endif
#ifdef EDEADLK
  case EDEADLK: return DRWAV_DEADLOCK;
#endif
#ifdef ENAMETOOLONG
  case ENAMETOOLONG: return DRWAV_PATH_TOO_LONG;
#endif
#ifdef ENOLCK
  case ENOLCK: return DRWAV_ERROR;
#endif
#ifdef ENOSYS
  case ENOSYS: return DRWAV_NOT_IMPLEMENTED;
#endif
#ifdef ENOTEMPTY
  case ENOTEMPTY: return DRWAV_DIRECTORY_NOT_EMPTY;
#endif
#ifdef ELOOP
  case ELOOP: return DRWAV_TOO_MANY_LINKS;
#endif
#ifdef ENOMSG
  case ENOMSG: return DRWAV_NO_MESSAGE;
#endif
#ifdef EIDRM
  case EIDRM: return DRWAV_ERROR;
#endif
#ifdef ECHRNG
  case ECHRNG: return DRWAV_ERROR;
#endif
#ifdef EL2NSYNC
  case EL2NSYNC: return DRWAV_ERROR;
#endif
#ifdef EL3HLT
  case EL3HLT: return DRWAV_ERROR;
#endif
#ifdef EL3RST
  case EL3RST: return DRWAV_ERROR;
#endif
#ifdef ELNRNG
  case ELNRNG: return DRWAV_OUT_OF_RANGE;
#endif
#ifdef EUNATCH
  case EUNATCH: return DRWAV_ERROR;
#endif
#ifdef ENOCSI
  case ENOCSI: return DRWAV_ERROR;
#endif
#ifdef EL2HLT
  case EL2HLT: return DRWAV_ERROR;
#endif
#ifdef EBADE
  case EBADE: return DRWAV_ERROR;
#endif
#ifdef EBADR
  case EBADR: return DRWAV_ERROR;
#endif
#ifdef EXFULL
  case EXFULL: return DRWAV_ERROR;
#endif
#ifdef ENOANO
  case ENOANO: return DRWAV_ERROR;
#endif
#ifdef EBADRQC
  case EBADRQC: return DRWAV_ERROR;
#endif
#ifdef EBADSLT
  case EBADSLT: return DRWAV_ERROR;
#endif
#ifdef EBFONT
  case EBFONT: return DRWAV_INVALID_FILE;
#endif
#ifdef ENOSTR
  case ENOSTR: return DRWAV_ERROR;
#endif
#ifdef ENODATA
  case ENODATA: return DRWAV_NO_DATA_AVAILABLE;
#endif
#ifdef ETIME
  case ETIME: return DRWAV_TIMEOUT;
#endif
#ifdef ENOSR
  case ENOSR: return DRWAV_NO_DATA_AVAILABLE;
#endif
#ifdef ENONET
  case ENONET: return DRWAV_NO_NETWORK;
#endif
#ifdef ENOPKG
  case ENOPKG: return DRWAV_ERROR;
#endif
#ifdef EREMOTE
  case EREMOTE: return DRWAV_ERROR;
#endif
#ifdef ENOLINK
  case ENOLINK: return DRWAV_ERROR;
#endif
#ifdef EADV
  case EADV: return DRWAV_ERROR;
#endif
#ifdef ESRMNT
  case ESRMNT: return DRWAV_ERROR;
#endif
#ifdef ECOMM
  case ECOMM: return DRWAV_ERROR;
#endif
#ifdef EPROTO
  case EPROTO: return DRWAV_ERROR;
#endif
#ifdef EMULTIHOP
  case EMULTIHOP: return DRWAV_ERROR;
#endif
#ifdef EDOTDOT
  case EDOTDOT: return DRWAV_ERROR;
#endif
#ifdef EBADMSG
  case EBADMSG: return DRWAV_BAD_MESSAGE;
#endif
#ifdef EOVERFLOW
  case EOVERFLOW: return DRWAV_TOO_BIG;
#endif
#ifdef ENOTUNIQ
  case ENOTUNIQ: return DRWAV_NOT_UNIQUE;
#endif
#ifdef EBADFD
  case EBADFD: return DRWAV_ERROR;
#endif
#ifdef EREMCHG
  case EREMCHG: return DRWAV_ERROR;
#endif
#ifdef ELIBACC
  case ELIBACC: return DRWAV_ACCESS_DENIED;
#endif
#ifdef ELIBBAD
  case ELIBBAD: return DRWAV_INVALID_FILE;
#endif
#ifdef ELIBSCN
  case ELIBSCN: return DRWAV_INVALID_FILE;
#endif
#ifdef ELIBMAX
  case ELIBMAX: return DRWAV_ERROR;
#endif
#ifdef ELIBEXEC
  case ELIBEXEC: return DRWAV_ERROR;
#endif
#ifdef EILSEQ
  case EILSEQ: return DRWAV_INVALID_DATA;
#endif
#ifdef ERESTART
  case ERESTART: return DRWAV_ERROR;
#endif
#ifdef ESTRPIPE
  case ESTRPIPE: return DRWAV_ERROR;
#endif
#ifdef EUSERS
  case EUSERS: return DRWAV_ERROR;
#endif
#ifdef ENOTSOCK
  case ENOTSOCK: return DRWAV_NOT_SOCKET;
#endif
#ifdef EDESTADDRREQ
  case EDESTADDRREQ: return DRWAV_NO_ADDRESS;
#endif
#ifdef EMSGSIZE
  case EMSGSIZE: return DRWAV_TOO_BIG;
#endif
#ifdef EPROTOTYPE
  case EPROTOTYPE: return DRWAV_BAD_PROTOCOL;
#endif
#ifdef ENOPROTOOPT
  case ENOPROTOOPT: return DRWAV_PROTOCOL_UNAVAILABLE;
#endif
#ifdef EPROTONOSUPPORT
  case EPROTONOSUPPORT: return DRWAV_PROTOCOL_NOT_SUPPORTED;
#endif
#ifdef ESOCKTNOSUPPORT
  case ESOCKTNOSUPPORT: return DRWAV_SOCKET_NOT_SUPPORTED;
#endif
#ifdef EOPNOTSUPP
  case EOPNOTSUPP: return DRWAV_INVALID_OPERATION;
#endif
#ifdef EPFNOSUPPORT
  case EPFNOSUPPORT: return DRWAV_PROTOCOL_FAMILY_NOT_SUPPORTED;
#endif
#ifdef EAFNOSUPPORT
  case EAFNOSUPPORT: return DRWAV_ADDRESS_FAMILY_NOT_SUPPORTED;
#endif
#ifdef EADDRINUSE
  case EADDRINUSE: return DRWAV_ALREADY_IN_USE;
#endif
#ifdef EADDRNOTAVAIL
  case EADDRNOTAVAIL: return DRWAV_ERROR;
#endif
#ifdef ENETDOWN
  case ENETDOWN: return DRWAV_NO_NETWORK;
#endif
#ifdef ENETUNREACH
  case ENETUNREACH: return DRWAV_NO_NETWORK;
#endif
#ifdef ENETRESET
  case ENETRESET: return DRWAV_NO_NETWORK;
#endif
#ifdef ECONNABORTED
  case ECONNABORTED: return DRWAV_NO_NETWORK;
#endif
#ifdef ECONNRESET
  case ECONNRESET: return DRWAV_CONNECTION_RESET;
#endif
#ifdef ENOBUFS
  case ENOBUFS: return DRWAV_NO_SPACE;
#endif
#ifdef EISCONN
  case EISCONN: return DRWAV_ALREADY_CONNECTED;
#endif
#ifdef ENOTCONN
  case ENOTCONN: return DRWAV_NOT_CONNECTED;
#endif
#ifdef ESHUTDOWN
  case ESHUTDOWN: return DRWAV_ERROR;
#endif
#ifdef ETOOMANYREFS
  case ETOOMANYREFS: return DRWAV_ERROR;
#endif
#ifdef ETIMEDOUT
  case ETIMEDOUT: return DRWAV_TIMEOUT;
#endif
#ifdef ECONNREFUSED
  case ECONNREFUSED: return DRWAV_CONNECTION_REFUSED;
#endif
#ifdef EHOSTDOWN
  case EHOSTDOWN: return DRWAV_NO_HOST;
#endif
#ifdef EHOSTUNREACH
  case EHOSTUNREACH: return DRWAV_NO_HOST;
#endif
#ifdef EALREADY
  case EALREADY: return DRWAV_IN_PROGRESS;
#endif
#ifdef EINPROGRESS
  case EINPROGRESS: return DRWAV_IN_PROGRESS;
#endif
#ifdef ESTALE
  case ESTALE: return DRWAV_INVALID_FILE;
#endif
#ifdef EUCLEAN
  case EUCLEAN: return DRWAV_ERROR;
#endif
#ifdef ENOTNAM
  case ENOTNAM: return DRWAV_ERROR;
#endif
#ifdef ENAVAIL
  case ENAVAIL: return DRWAV_ERROR;
#endif
#ifdef EISNAM
  case EISNAM: return DRWAV_ERROR;
#endif
#ifdef EREMOTEIO
  case EREMOTEIO: return DRWAV_IO_ERROR;
#endif
#ifdef EDQUOT
  case EDQUOT: return DRWAV_NO_SPACE;
#endif
#ifdef ENOMEDIUM
  case ENOMEDIUM: return DRWAV_DOES_NOT_EXIST;
#endif
#ifdef EMEDIUMTYPE
  case EMEDIUMTYPE: return DRWAV_ERROR;
#endif
#ifdef ECANCELED
  case ECANCELED: return DRWAV_CANCELLED;
#endif
#ifdef ENOKEY
  case ENOKEY: return DRWAV_ERROR;
#endif
#ifdef EKEYEXPIRED
  case EKEYEXPIRED: return DRWAV_ERROR;
#endif
#ifdef EKEYREVOKED
  case EKEYREVOKED: return DRWAV_ERROR;
#endif
#ifdef EKEYREJECTED
  case EKEYREJECTED: return DRWAV_ERROR;
#endif
#ifdef EOWNERDEAD
  case EOWNERDEAD: return DRWAV_ERROR;
#endif
#ifdef ENOTRECOVERABLE
  case ENOTRECOVERABLE: return DRWAV_ERROR;
#endif
#ifdef ERFKILL
  case ERFKILL: return DRWAV_ERROR;
#endif
#ifdef EHWPOISON
  case EHWPOISON: return DRWAV_ERROR;
#endif
  default: return DRWAV_ERROR;
  }
}

DRWAV_PRIVATE drwav_result drwav_fopen(FILE** ppFile, const char* pFilePath,
                                       const char* pOpenMode) {
#if defined(_MSC_VER) && _MSC_VER >= 1400
  errno_t err;
#endif

  if (ppFile != NULL) { *ppFile = NULL; /* Safety. */ }

  if (pFilePath == NULL || pOpenMode == NULL || ppFile == NULL) { return DRWAV_INVALID_ARGS; }

#if defined(_MSC_VER) && _MSC_VER >= 1400
  err = fopen_s(ppFile, pFilePath, pOpenMode);
  if (err != 0) { return drwav_result_from_errno(err); }
#else
#if defined(_WIN32) || defined(__APPLE__)
  *ppFile = fopen(pFilePath, pOpenMode);
#else
#if defined(_FILE_OFFSET_BITS) && _FILE_OFFSET_BITS == 64 && defined(_LARGEFILE64_SOURCE)
  *ppFile = fopen64(pFilePath, pOpenMode);
#else
  *ppFile = fopen(pFilePath, pOpenMode);
#endif
#endif
  if (*ppFile == NULL) {
    drwav_result result = drwav_result_from_errno(errno);
    if (result == DRWAV_SUCCESS) {
      result = DRWAV_ERROR; /* Just a safety check to make sure we never ever
                               return success when pFile == NULL. */
    }

    return result;
  }
#endif

  return DRWAV_SUCCESS;
}

/*
_wfopen() isn't always available in all compilation environments.

    * Windows only.
    * MSVC seems to support it universally as far back as VC6 from what I can
tell (haven't checked further back).
    * MinGW-64 (both 32- and 64-bit) seems to support it.
    * MinGW wraps it in !defined(__STRICT_ANSI__).
    * OpenWatcom wraps it in !defined(_NO_EXT_KEYS).

This can be reviewed as compatibility issues arise. The preference is to use
_wfopen_s() and _wfopen() as opposed to the wcsrtombs() fallback, so if you
notice your compiler not detecting this properly I'm happy to look at adding
support.
*/
#if defined(_WIN32)
#if defined(_MSC_VER) || defined(__MINGW64__) ||                                                   \
    (!defined(__STRICT_ANSI__) && !defined(_NO_EXT_KEYS))
#define DRWAV_HAS_WFOPEN
#endif
#endif

DRWAV_PRIVATE drwav_result drwav_wfopen(FILE** ppFile, const wchar_t* pFilePath,
                                        const wchar_t* pOpenMode,
                                        const drwav_allocation_callbacks* pAllocationCallbacks) {
  if (ppFile != NULL) { *ppFile = NULL; /* Safety. */ }

  if (pFilePath == NULL || pOpenMode == NULL || ppFile == NULL) { return DRWAV_INVALID_ARGS; }

#if defined(DRWAV_HAS_WFOPEN)
  {
    /* Use _wfopen() on Windows. */
#if defined(_MSC_VER) && _MSC_VER >= 1400
    errno_t err = _wfopen_s(ppFile, pFilePath, pOpenMode);
    if (err != 0) { return drwav_result_from_errno(err); }
#else
    *ppFile = _wfopen(pFilePath, pOpenMode);
    if (*ppFile == NULL) { return drwav_result_from_errno(errno); }
#endif
    (void)pAllocationCallbacks;
  }
#else
  /*
  Use fopen() on anything other than Windows. Requires a conversion. This is
  annoying because fopen() is locale specific. The only real way I can think of
  to do this is with wcsrtombs(). Note that wcstombs() is apparently not
  thread-safe because it uses a static global mbstate_t object for maintaining
  state. I've checked this with -std=c89 and it works, but if somebody get's a
  compiler error I'll look into improving compatibility.
  */
  {
    mbstate_t mbs;
    size_t lenMB;
    const wchar_t* pFilePathTemp = pFilePath;
    char* pFilePathMB = NULL;
    char pOpenModeMB[32] = {0};

    /* Get the length first. */
    DRWAV_ZERO_OBJECT(&mbs);
    lenMB = wcsrtombs(NULL, &pFilePathTemp, 0, &mbs);
    if (lenMB == (size_t)-1) { return drwav_result_from_errno(errno); }

    pFilePathMB = (char*)drwav__malloc_from_callbacks(lenMB + 1, pAllocationCallbacks);
    if (pFilePathMB == NULL) { return DRWAV_OUT_OF_MEMORY; }

    pFilePathTemp = pFilePath;
    DRWAV_ZERO_OBJECT(&mbs);
    wcsrtombs(pFilePathMB, &pFilePathTemp, lenMB + 1, &mbs);

    /* The open mode should always consist of ASCII characters so we should be
     * able to do a trivial conversion. */
    {
      size_t i = 0;
      for (;;) {
        if (pOpenMode[i] == 0) {
          pOpenModeMB[i] = '\0';
          break;
        }

        pOpenModeMB[i] = (char)pOpenMode[i];
        i += 1;
      }
    }

    *ppFile = fopen(pFilePathMB, pOpenModeMB);

    drwav__free_from_callbacks(pFilePathMB, pAllocationCallbacks);
  }

  if (*ppFile == NULL) { return DRWAV_ERROR; }
#endif

  return DRWAV_SUCCESS;
}

DRWAV_PRIVATE size_t drwav__on_read_stdio(void* pUserData, void* pBufferOut, size_t bytesToRead) {
  return fread(pBufferOut, 1, bytesToRead, (FILE*)pUserData);
}

DRWAV_PRIVATE size_t drwav__on_write_stdio(void* pUserData, const void* pData,
                                           size_t bytesToWrite) {
  return fwrite(pData, 1, bytesToWrite, (FILE*)pUserData);
}

DRWAV_PRIVATE bool drwav__on_seek_stdio(void* pUserData, int offset, drwav_seek_origin origin) {
  return fseek((FILE*)pUserData, offset,
               (origin == drwav_seek_origin_current) ? SEEK_CUR : SEEK_SET) == 0;
}

DRWAV_API drwav* drwav_init_file(const char* filename,
                                 const drwav_allocation_callbacks* pAllocationCallbacks) {
  return drwav_init_file_ex(filename, NULL, NULL, 0, pAllocationCallbacks);
}

DRWAV_PRIVATE bool
drwav_init_file__internal_FILE(drwav* pWav, FILE* pFile, drwav_chunk_proc onChunk,
                               void* pChunkUserData, uint32_t flags,
                               drwav_metadata_type allowedMetadataTypes,
                               const drwav_allocation_callbacks* pAllocationCallbacks) {
  bool result;

  result = drwav_preinit(pWav, drwav__on_read_stdio, drwav__on_seek_stdio, (void*)pFile,
                         pAllocationCallbacks);
  if (result != true) {
    fclose(pFile);
    return result;
  }

  pWav->allowedMetadataTypes = allowedMetadataTypes;

  result = drwav_init__internal(pWav, onChunk, pChunkUserData, flags);
  if (result != true) {
    fclose(pFile);
    return result;
  }

  return true;
}

DRWAV_API drwav* drwav_init_file_ex(const char* filename, drwav_chunk_proc onChunk,
                                    void* pChunkUserData, uint32_t flags,
                                    const drwav_allocation_callbacks* pAllocationCallbacks) {
  FILE* pFile;
  if (drwav_fopen(&pFile, filename, "rb") != DRWAV_SUCCESS) { return false; }

  drwav* pWav = DRWAV_MALLOC(sizeof(drwav));
  /* This takes ownership of the FILE* object. */
  if (!drwav_init_file__internal_FILE(pWav, pFile, onChunk, pChunkUserData, flags,
                                      drwav_metadata_type_none, pAllocationCallbacks)) {
    drwav_free(pWav, pAllocationCallbacks);
    return NULL;
  }

  return pWav;
}

DRWAV_API drwav* drwav_init_file_w(const wchar_t* filename,
                                   const drwav_allocation_callbacks* pAllocationCallbacks) {
  return drwav_init_file_ex_w(filename, NULL, NULL, 0, pAllocationCallbacks);
}

DRWAV_API drwav* drwav_init_file_ex_w(const wchar_t* filename, drwav_chunk_proc onChunk,
                                      void* pChunkUserData, uint32_t flags,
                                      const drwav_allocation_callbacks* pAllocationCallbacks) {
  FILE* pFile;
  if (drwav_wfopen(&pFile, filename, L"rb", pAllocationCallbacks) != DRWAV_SUCCESS) {
    return NULL;
  }

  drwav* pWav = DRWAV_MALLOC(sizeof(drwav));

  /* This takes ownership of the FILE* object. */
  if (!drwav_init_file__internal_FILE(pWav, pFile, onChunk, pChunkUserData, flags,
                                      drwav_metadata_type_none, pAllocationCallbacks)) {
    drwav_free(pWav, pAllocationCallbacks);
    return NULL;
  }

  return pWav;
}

DRWAV_API drwav*
drwav_init_file_with_metadata(const char* filename, uint32_t flags,
                              const drwav_allocation_callbacks* pAllocationCallbacks) {
  FILE* pFile;
  if (drwav_fopen(&pFile, filename, "rb") != DRWAV_SUCCESS) { return NULL; }

  drwav* pWav = DRWAV_MALLOC(sizeof(drwav));

  /* This takes ownership of the FILE* object. */
  if (!drwav_init_file__internal_FILE(pWav, pFile, NULL, NULL, flags,
                                      drwav_metadata_type_all_including_unknown,
                                      pAllocationCallbacks)) {
    drwav_free(pWav, pAllocationCallbacks);
    return NULL;
  }

  return pWav;
}

DRWAV_API drwav*
drwav_init_file_with_metadata_w(const wchar_t* filename, uint32_t flags,
                                const drwav_allocation_callbacks* pAllocationCallbacks) {
  FILE* pFile;
  if (drwav_wfopen(&pFile, filename, L"rb", pAllocationCallbacks) != DRWAV_SUCCESS) {
    return NULL;
  }

  drwav* pWav = DRWAV_MALLOC(sizeof(drwav));

  /* This takes ownership of the FILE* object. */
  if (!drwav_init_file__internal_FILE(pWav, pFile, NULL, NULL, flags,
                                      drwav_metadata_type_all_including_unknown,
                                      pAllocationCallbacks)) {
    drwav_free(pWav, pAllocationCallbacks);
    return NULL;
  }

  return pWav;
}

DRWAV_PRIVATE bool
drwav_init_file_write__internal_FILE(drwav* pWav, FILE* pFile, const drwav_data_format* pFormat,
                                     uint64_t totalSampleCount, bool isSequential,
                                     const drwav_allocation_callbacks* pAllocationCallbacks) {
  bool result;

  result = drwav_preinit_write(pWav, pFormat, isSequential, drwav__on_write_stdio,
                               drwav__on_seek_stdio, (void*)pFile, pAllocationCallbacks);
  if (result != true) {
    fclose(pFile);
    return result;
  }

  result = drwav_init_write__internal(pWav, pFormat, totalSampleCount);
  if (result != true) {
    fclose(pFile);
    return result;
  }

  return true;
}

DRWAV_PRIVATE bool
drwav_init_file_write__internal(drwav* pWav, const char* filename, const drwav_data_format* pFormat,
                                uint64_t totalSampleCount, bool isSequential,
                                const drwav_allocation_callbacks* pAllocationCallbacks) {
  FILE* pFile;
  if (drwav_fopen(&pFile, filename, "wb") != DRWAV_SUCCESS) { return false; }

  /* This takes ownership of the FILE* object. */
  return drwav_init_file_write__internal_FILE(pWav, pFile, pFormat, totalSampleCount, isSequential,
                                              pAllocationCallbacks);
}

DRWAV_PRIVATE bool
drwav_init_file_write_w__internal(drwav* pWav, const wchar_t* filename,
                                  const drwav_data_format* pFormat, uint64_t totalSampleCount,
                                  bool isSequential,
                                  const drwav_allocation_callbacks* pAllocationCallbacks) {
  FILE* pFile;
  if (drwav_wfopen(&pFile, filename, L"wb", pAllocationCallbacks) != DRWAV_SUCCESS) {
    return false;
  }

  /* This takes ownership of the FILE* object. */
  return drwav_init_file_write__internal_FILE(pWav, pFile, pFormat, totalSampleCount, isSequential,
                                              pAllocationCallbacks);
}

DRWAV_API drwav* drwav_init_file_write(const char* filename, const drwav_data_format* pFormat,
                                       const drwav_allocation_callbacks* pAllocationCallbacks) {
  drwav* pWav = DRWAV_MALLOC(sizeof(drwav));

  if (!drwav_init_file_write__internal(pWav, filename, pFormat, 0, false, pAllocationCallbacks)) {
    drwav_free(pWav, pAllocationCallbacks);
    return NULL;
  }

  return pWav;
}

DRWAV_API drwav*
drwav_init_file_write_sequential(const char* filename, const drwav_data_format* pFormat,
                                 uint64_t totalSampleCount,
                                 const drwav_allocation_callbacks* pAllocationCallbacks) {
  drwav* pWav = DRWAV_MALLOC(sizeof(drwav));

  if (!drwav_init_file_write__internal(pWav, filename, pFormat, totalSampleCount, true,
                                       pAllocationCallbacks)) {
    drwav_free(pWav, pAllocationCallbacks);
    return NULL;
  }

  return pWav;
}

DRWAV_API drwav* drwav_init_file_write_sequential_pcm_frames(
    const char* filename, const drwav_data_format* pFormat, uint64_t totalPCMFrameCount,
    const drwav_allocation_callbacks* pAllocationCallbacks) {
  if (pFormat == NULL) { return false; }

  return drwav_init_file_write_sequential(filename, pFormat, totalPCMFrameCount * pFormat->channels,
                                          pAllocationCallbacks);
}

DRWAV_API drwav* drwav_init_file_write_w(const wchar_t* filename, const drwav_data_format* pFormat,
                                         const drwav_allocation_callbacks* pAllocationCallbacks) {
  drwav* pWav = DRWAV_MALLOC(sizeof(drwav));
  if (!drwav_init_file_write_w__internal(pWav, filename, pFormat, 0, false, pAllocationCallbacks)) {
    drwav_free(pWav, pAllocationCallbacks);
    return NULL;
  }

  return pWav;
}

DRWAV_API drwav*
drwav_init_file_write_sequential_w(const wchar_t* filename, const drwav_data_format* pFormat,
                                   uint64_t totalSampleCount,
                                   const drwav_allocation_callbacks* pAllocationCallbacks) {
  drwav* pWav = DRWAV_MALLOC(sizeof(drwav));
  if (!drwav_init_file_write_w__internal(pWav, filename, pFormat, totalSampleCount, true,
                                         pAllocationCallbacks)) {
    drwav_free(pWav, pAllocationCallbacks);
    return NULL;
  }

  return pWav;
}

DRWAV_API drwav* drwav_init_file_write_sequential_pcm_frames_w(
    const wchar_t* filename, const drwav_data_format* pFormat, uint64_t totalPCMFrameCount,
    const drwav_allocation_callbacks* pAllocationCallbacks) {
  if (pFormat == NULL) { return NULL; }

  return drwav_init_file_write_sequential_w(
      filename, pFormat, totalPCMFrameCount * pFormat->channels, pAllocationCallbacks);
}
#endif /* DR_WAV_NO_STDIO */

DRWAV_PRIVATE size_t drwav__on_read_memory(void* pUserData, void* pBufferOut, size_t bytesToRead) {
  drwav* pWav = (drwav*)pUserData;
  size_t bytesRemaining;

  DRWAV_ASSERT(pWav != NULL);
  DRWAV_ASSERT(pWav->memoryStream.dataSize >= pWav->memoryStream.currentReadPos);

  bytesRemaining = pWav->memoryStream.dataSize - pWav->memoryStream.currentReadPos;
  if (bytesToRead > bytesRemaining) { bytesToRead = bytesRemaining; }

  if (bytesToRead > 0) {
    DRWAV_COPY_MEMORY(pBufferOut, pWav->memoryStream.data + pWav->memoryStream.currentReadPos,
                      bytesToRead);
    pWav->memoryStream.currentReadPos += bytesToRead;
  }

  return bytesToRead;
}

DRWAV_PRIVATE bool drwav__on_seek_memory(void* pUserData, int offset, drwav_seek_origin origin) {
  drwav* pWav = (drwav*)pUserData;
  DRWAV_ASSERT(pWav != NULL);

  if (origin == drwav_seek_origin_current) {
    if (offset > 0) {
      if (pWav->memoryStream.currentReadPos + offset > pWav->memoryStream.dataSize) {
        return false; /* Trying to seek too far forward. */
      }
    } else {
      if (pWav->memoryStream.currentReadPos < (size_t)-offset) {
        return false; /* Trying to seek too far backwards. */
      }
    }

    /* This will never underflow thanks to the clamps above. */
    pWav->memoryStream.currentReadPos += offset;
  } else {
    if ((uint32_t)offset <= pWav->memoryStream.dataSize) {
      pWav->memoryStream.currentReadPos = offset;
    } else {
      return false; /* Trying to seek too far forward. */
    }
  }

  return true;
}

DRWAV_PRIVATE size_t drwav__on_write_memory(void* pUserData, const void* pDataIn,
                                            size_t bytesToWrite) {
  drwav* pWav = (drwav*)pUserData;
  size_t bytesRemaining;

  DRWAV_ASSERT(pWav != NULL);
  DRWAV_ASSERT(pWav->memoryStreamWrite.dataCapacity >= pWav->memoryStreamWrite.currentWritePos);

  bytesRemaining = pWav->memoryStreamWrite.dataCapacity - pWav->memoryStreamWrite.currentWritePos;
  if (bytesRemaining < bytesToWrite) {
    /* Need to reallocate. */
    void* pNewData;
    size_t newDataCapacity = (pWav->memoryStreamWrite.dataCapacity == 0)
                                 ? 256
                                 : pWav->memoryStreamWrite.dataCapacity * 2;

    /* If doubling wasn't enough, just make it the minimum required size to
     * write the data. */
    if ((newDataCapacity - pWav->memoryStreamWrite.currentWritePos) < bytesToWrite) {
      newDataCapacity = pWav->memoryStreamWrite.currentWritePos + bytesToWrite;
    }

    pNewData = drwav__realloc_from_callbacks(*pWav->memoryStreamWrite.ppData, newDataCapacity,
                                             pWav->memoryStreamWrite.dataCapacity,
                                             &pWav->allocationCallbacks);
    if (pNewData == NULL) { return 0; }

    *pWav->memoryStreamWrite.ppData = pNewData;
    pWav->memoryStreamWrite.dataCapacity = newDataCapacity;
  }

  DRWAV_COPY_MEMORY(((uint8_t*)(*pWav->memoryStreamWrite.ppData)) +
                        pWav->memoryStreamWrite.currentWritePos,
                    pDataIn, bytesToWrite);

  pWav->memoryStreamWrite.currentWritePos += bytesToWrite;
  if (pWav->memoryStreamWrite.dataSize < pWav->memoryStreamWrite.currentWritePos) {
    pWav->memoryStreamWrite.dataSize = pWav->memoryStreamWrite.currentWritePos;
  }

  *pWav->memoryStreamWrite.pDataSize = pWav->memoryStreamWrite.dataSize;

  return bytesToWrite;
}

DRWAV_PRIVATE bool drwav__on_seek_memory_write(void* pUserData, int offset,
                                               drwav_seek_origin origin) {
  drwav* pWav = (drwav*)pUserData;
  DRWAV_ASSERT(pWav != NULL);

  if (origin == drwav_seek_origin_current) {
    if (offset > 0) {
      if (pWav->memoryStreamWrite.currentWritePos + offset > pWav->memoryStreamWrite.dataSize) {
        offset =
            (int)(pWav->memoryStreamWrite.dataSize -
                  pWav->memoryStreamWrite.currentWritePos); /* Trying to seek too far forward. */
      }
    } else {
      if (pWav->memoryStreamWrite.currentWritePos < (size_t)-offset) {
        offset =
            -(int)pWav->memoryStreamWrite.currentWritePos; /* Trying to seek too far backwards. */
      }
    }

    /* This will never underflow thanks to the clamps above. */
    pWav->memoryStreamWrite.currentWritePos += offset;
  } else {
    if ((uint32_t)offset <= pWav->memoryStreamWrite.dataSize) {
      pWav->memoryStreamWrite.currentWritePos = offset;
    } else {
      pWav->memoryStreamWrite.currentWritePos =
          pWav->memoryStreamWrite.dataSize; /* Trying to seek too far forward. */
    }
  }

  return true;
}

DRWAV_API drwav* drwav_init_memory(const void* data, size_t dataSize,
                                   const drwav_allocation_callbacks* pAllocationCallbacks) {
  return drwav_init_memory_ex(data, dataSize, NULL, NULL, 0, pAllocationCallbacks);
}

DRWAV_API drwav* drwav_init_memory_ex(const void* data, size_t dataSize, drwav_chunk_proc onChunk,
                                      void* pChunkUserData, uint32_t flags,
                                      const drwav_allocation_callbacks* pAllocationCallbacks) {
  if (data == NULL || dataSize == 0) { return false; }

  drwav* pWav = DRWAV_MALLOC(sizeof(drwav));

  if (!drwav_preinit(pWav, drwav__on_read_memory, drwav__on_seek_memory, pWav,
                     pAllocationCallbacks)) {
    drwav_free(pWav, pAllocationCallbacks);
    return NULL;
  }

  pWav->memoryStream.data = (const uint8_t*)data;
  pWav->memoryStream.dataSize = dataSize;
  pWav->memoryStream.currentReadPos = 0;

  if (!drwav_init__internal(pWav, onChunk, pChunkUserData, flags)) {
    drwav_free(pWav, pAllocationCallbacks);
    return NULL;
  }

  return pWav;
}

DRWAV_API drwav*
drwav_init_memory_with_metadata(const void* data, size_t dataSize, uint32_t flags,
                                const drwav_allocation_callbacks* pAllocationCallbacks) {
  if (data == NULL || dataSize == 0) { return false; }

  drwav* pWav = DRWAV_MALLOC(sizeof(drwav));

  if (!drwav_preinit(pWav, drwav__on_read_memory, drwav__on_seek_memory, pWav,
                     pAllocationCallbacks)) {
    drwav_free(pWav, pAllocationCallbacks);
    return NULL;
  }

  pWav->memoryStream.data = (const uint8_t*)data;
  pWav->memoryStream.dataSize = dataSize;
  pWav->memoryStream.currentReadPos = 0;

  pWav->allowedMetadataTypes = drwav_metadata_type_all_including_unknown;

  if (!drwav_init__internal(pWav, NULL, NULL, flags)) {
    drwav_free(pWav, pAllocationCallbacks);
    return NULL;
  }

  return pWav;
}

DRWAV_PRIVATE bool
drwav_init_memory_write__internal(drwav* pWav, void** ppData, size_t* pDataSize,
                                  const drwav_data_format* pFormat, uint64_t totalSampleCount,
                                  bool isSequential,
                                  const drwav_allocation_callbacks* pAllocationCallbacks) {
  if (ppData == NULL || pDataSize == NULL) { return false; }

  *ppData = NULL; /* Important because we're using realloc()! */
  *pDataSize = 0;

  if (!drwav_preinit_write(pWav, pFormat, isSequential, drwav__on_write_memory,
                           drwav__on_seek_memory_write, pWav, pAllocationCallbacks)) {
    return false;
  }

  pWav->memoryStreamWrite.ppData = ppData;
  pWav->memoryStreamWrite.pDataSize = pDataSize;
  pWav->memoryStreamWrite.dataSize = 0;
  pWav->memoryStreamWrite.dataCapacity = 0;
  pWav->memoryStreamWrite.currentWritePos = 0;

  return drwav_init_write__internal(pWav, pFormat, totalSampleCount);
}

DRWAV_API drwav* drwav_init_memory_write(void** ppData, size_t* pDataSize,
                                         const drwav_data_format* pFormat,
                                         const drwav_allocation_callbacks* pAllocationCallbacks) {
  drwav* pWav = DRWAV_MALLOC(sizeof(drwav));
  if (!drwav_init_memory_write__internal(pWav, ppData, pDataSize, pFormat, 0, false,
                                         pAllocationCallbacks)) {
    drwav_free(pWav, pAllocationCallbacks);
    return NULL;
  }

  return pWav;
}

DRWAV_API drwav*
drwav_init_memory_write_sequential(void** ppData, size_t* pDataSize,
                                   const drwav_data_format* pFormat, uint64_t totalSampleCount,
                                   const drwav_allocation_callbacks* pAllocationCallbacks) {
  drwav* pWav = DRWAV_MALLOC(sizeof(drwav));
  if (!drwav_init_memory_write__internal(pWav, ppData, pDataSize, pFormat, totalSampleCount, true,
                                         pAllocationCallbacks)) {
    drwav_free(pWav, pAllocationCallbacks);
    return NULL;
  }

  return pWav;
}

DRWAV_API drwav* drwav_init_memory_write_sequential_pcm_frames(
    void** ppData, size_t* pDataSize, const drwav_data_format* pFormat, uint64_t totalPCMFrameCount,
    const drwav_allocation_callbacks* pAllocationCallbacks) {
  if (pFormat == NULL) { return false; }

  return drwav_init_memory_write_sequential(
      ppData, pDataSize, pFormat, totalPCMFrameCount * pFormat->channels, pAllocationCallbacks);
}

DRWAV_API drwav_result drwav_uninit(drwav* pWav) {
  drwav_result result = DRWAV_SUCCESS;

  if (pWav == NULL) { return DRWAV_INVALID_ARGS; }

  /*
  If the drwav object was opened in write mode we'll need to finalize a few
  things:
    - Make sure the "data" chunk is aligned to 16-bits for RIFF containers, or
  64 bits for W64 containers.
    - Set the size of the "data" chunk.
  */
  if (pWav->onWrite != NULL) {
    uint32_t paddingSize = 0;

    /* Padding. Do not adjust pWav->dataChunkDataSize - this should not include
     * the padding. */
    if (pWav->container == drwav_container_riff || pWav->container == drwav_container_rf64) {
      paddingSize = drwav__chunk_padding_size_riff(pWav->dataChunkDataSize);
    } else {
      paddingSize = drwav__chunk_padding_size_w64(pWav->dataChunkDataSize);
    }

    if (paddingSize > 0) {
      uint64_t paddingData = 0;
      drwav__write(pWav, &paddingData, paddingSize); /* Byte order does not matter for this. */
    }

    /*
    Chunk sizes. When using sequential mode, these will have been filled in at
    initialization time. We only need to do this when using non-sequential mode.
    */
    if (pWav->onSeek && !pWav->isSequentialWrite) {
      if (pWav->container == drwav_container_riff) {
        /* The "RIFF" chunk size. */
        if (pWav->onSeek(pWav->pUserData, 4, drwav_seek_origin_start)) {
          uint32_t riffChunkSize = drwav__riff_chunk_size_riff(
              pWav->dataChunkDataSize, pWav->pMetadata, pWav->metadataCount);
          drwav__write_u32ne_to_le(pWav, riffChunkSize);
        }

        /* The "data" chunk size. */
        if (pWav->onSeek(pWav->pUserData, (int)pWav->dataChunkDataPos - 4,
                         drwav_seek_origin_start)) {
          uint32_t dataChunkSize = drwav__data_chunk_size_riff(pWav->dataChunkDataSize);
          drwav__write_u32ne_to_le(pWav, dataChunkSize);
        }
      } else if (pWav->container == drwav_container_w64) {
        /* The "RIFF" chunk size. */
        if (pWav->onSeek(pWav->pUserData, 16, drwav_seek_origin_start)) {
          uint64_t riffChunkSize = drwav__riff_chunk_size_w64(pWav->dataChunkDataSize);
          drwav__write_u64ne_to_le(pWav, riffChunkSize);
        }

        /* The "data" chunk size. */
        if (pWav->onSeek(pWav->pUserData, (int)pWav->dataChunkDataPos - 8,
                         drwav_seek_origin_start)) {
          uint64_t dataChunkSize = drwav__data_chunk_size_w64(pWav->dataChunkDataSize);
          drwav__write_u64ne_to_le(pWav, dataChunkSize);
        }
      } else if (pWav->container == drwav_container_rf64) {
        /* We only need to update the ds64 chunk. The "RIFF" and "data" chunks
         * always have their sizes set to 0xFFFFFFFF for RF64. */
        int ds64BodyPos = 12 + 8;

        /* The "RIFF" chunk size. */
        if (pWav->onSeek(pWav->pUserData, ds64BodyPos + 0, drwav_seek_origin_start)) {
          uint64_t riffChunkSize = drwav__riff_chunk_size_rf64(
              pWav->dataChunkDataSize, pWav->pMetadata, pWav->metadataCount);
          drwav__write_u64ne_to_le(pWav, riffChunkSize);
        }

        /* The "data" chunk size. */
        if (pWav->onSeek(pWav->pUserData, ds64BodyPos + 8, drwav_seek_origin_start)) {
          uint64_t dataChunkSize = drwav__data_chunk_size_rf64(pWav->dataChunkDataSize);
          drwav__write_u64ne_to_le(pWav, dataChunkSize);
        }
      }
    }

    /* Validation for sequential mode. */
    if (pWav->isSequentialWrite) {
      if (pWav->dataChunkDataSize != pWav->dataChunkDataSizeTargetWrite) {
        result = DRWAV_INVALID_FILE;
      }
    }
  } else {
    if (pWav->pMetadata != NULL) {
      pWav->allocationCallbacks.onFree(pWav->pMetadata, pWav->allocationCallbacks.pUserData);
    }
  }

#ifndef DR_WAV_NO_STDIO
  /*
  If we opened the file with drwav_open_file() we will want to close the file
  handle. We can know whether or not drwav_open_file() was used by looking at
  the onRead and onSeek callbacks.
  */
  if (pWav->onRead == drwav__on_read_stdio || pWav->onWrite == drwav__on_write_stdio) {
    fclose((FILE*)pWav->pUserData);
  }
#endif

  return result;
}

DRWAV_API size_t drwav_read_raw(drwav* pWav, size_t bytesToRead, void* pBufferOut) {
  size_t bytesRead;

  if (pWav == NULL || bytesToRead == 0) { return 0; /* Invalid args. */ }

  if (bytesToRead > pWav->bytesRemaining) { bytesToRead = (size_t)pWav->bytesRemaining; }

  if (bytesToRead == 0) { return 0; /* At end. */ }

  if (pBufferOut != NULL) {
    bytesRead = pWav->onRead(pWav->pUserData, pBufferOut, bytesToRead);
  } else {
    /* We need to seek. If we fail, we need to read-and-discard to make sure we
     * get a good byte count. */
    bytesRead = 0;
    while (bytesRead < bytesToRead) {
      size_t bytesToSeek = (bytesToRead - bytesRead);
      if (bytesToSeek > 0x7FFFFFFF) { bytesToSeek = 0x7FFFFFFF; }

      if (pWav->onSeek(pWav->pUserData, (int)bytesToSeek, drwav_seek_origin_current) == false) {
        break;
      }

      bytesRead += bytesToSeek;
    }

    /* When we get here we may need to read-and-discard some data. */
    while (bytesRead < bytesToRead) {
      uint8_t buffer[4096];
      size_t bytesSeeked;
      size_t bytesToSeek = (bytesToRead - bytesRead);
      if (bytesToSeek > sizeof(buffer)) { bytesToSeek = sizeof(buffer); }

      bytesSeeked = pWav->onRead(pWav->pUserData, buffer, bytesToSeek);
      bytesRead += bytesSeeked;

      if (bytesSeeked < bytesToSeek) { break; /* Reached the end. */ }
    }
  }

  pWav->readCursorInPCMFrames += bytesRead / drwav_get_bytes_per_pcm_frame(pWav);

  pWav->bytesRemaining -= bytesRead;
  return bytesRead;
}

DRWAV_API uint64_t drwav_read_pcm_frames_le(drwav* pWav, uint64_t framesToRead, void* pBufferOut) {
  uint32_t bytesPerFrame;
  uint64_t bytesToRead; /* Intentionally uint64 instead of size_t so we can do a
                               check that we're not reading too much on 32-bit builds. */

  if (pWav == NULL || framesToRead == 0) { return 0; }

  /* Cannot use this function for compressed formats. */
  if (drwav__is_compressed_format_tag(pWav->translatedFormatTag)) { return 0; }

  bytesPerFrame = drwav_get_bytes_per_pcm_frame(pWav);
  if (bytesPerFrame == 0) { return 0; }

  /* Don't try to read more samples than can potentially fit in the output
   * buffer. */
  bytesToRead = framesToRead * bytesPerFrame;
  if (bytesToRead > DRWAV_SIZE_MAX) {
    bytesToRead =
        (DRWAV_SIZE_MAX / bytesPerFrame) * bytesPerFrame; /* Round the number of bytes to read to a
                                                             clean frame boundary. */
  }

  /*
  Doing an explicit check here just to make it clear that we don't want to be
  attempt to read anything if there's no bytes to read. There *could* be a time
  where it evaluates to 0 due to overflowing.
  */
  if (bytesToRead == 0) { return 0; }

  return drwav_read_raw(pWav, (size_t)bytesToRead, pBufferOut) / bytesPerFrame;
}

DRWAV_API uint64_t drwav_read_pcm_frames_be(drwav* pWav, uint64_t framesToRead, void* pBufferOut) {
  uint64_t framesRead = drwav_read_pcm_frames_le(pWav, framesToRead, pBufferOut);

  if (pBufferOut != NULL) {
    drwav__bswap_samples(pBufferOut, framesRead * pWav->channels,
                         drwav_get_bytes_per_pcm_frame(pWav) / pWav->channels,
                         pWav->translatedFormatTag);
  }

  return framesRead;
}

DRWAV_API uint64_t drwav_read_pcm_frames(drwav* pWav, uint64_t framesToRead, void* pBufferOut) {
  if (drwav__is_little_endian()) {
    return drwav_read_pcm_frames_le(pWav, framesToRead, pBufferOut);
  } else {
    return drwav_read_pcm_frames_be(pWav, framesToRead, pBufferOut);
  }
}

DRWAV_PRIVATE bool drwav_seek_to_first_pcm_frame(drwav* pWav) {
  if (pWav->onWrite != NULL) { return false; /* No seeking in write mode. */ }

  if (!pWav->onSeek(pWav->pUserData, (int)pWav->dataChunkDataPos, drwav_seek_origin_start)) {
    return false;
  }

  if (drwav__is_compressed_format_tag(pWav->translatedFormatTag)) {
    /* Cached data needs to be cleared for compressed formats. */
    if (pWav->translatedFormatTag == DR_WAVE_FORMAT_ADPCM) {
      DRWAV_ZERO_OBJECT(&pWav->msadpcm);
    } else if (pWav->translatedFormatTag == DR_WAVE_FORMAT_DVI_ADPCM) {
      DRWAV_ZERO_OBJECT(&pWav->ima);
    } else {
      DRWAV_ASSERT(false); /* If this assertion is triggered it means I've
                                    implemented a new compressed format but
                                    forgot to add a branch for it here. */
    }
  }

  pWav->readCursorInPCMFrames = 0;
  pWav->bytesRemaining = pWav->dataChunkDataSize;

  return true;
}

DRWAV_API bool drwav_seek_to_pcm_frame(drwav* pWav, uint64_t targetFrameIndex) {
  /* Seeking should be compatible with wave files > 2GB. */

  if (pWav == NULL || pWav->onSeek == NULL) { return false; }

  /* No seeking in write mode. */
  if (pWav->onWrite != NULL) { return false; }

  /* If there are no samples, just return true without doing anything. */
  if (pWav->totalPCMFrameCount == 0) { return true; }

  /* Make sure the sample is clamped. */
  if (targetFrameIndex >= pWav->totalPCMFrameCount) {
    targetFrameIndex = pWav->totalPCMFrameCount - 1;
  }

  /*
  For compressed formats we just use a slow generic seek. If we are seeking
  forward we just seek forward. If we are going backwards we need to seek back
  to the start.
  */
  if (drwav__is_compressed_format_tag(pWav->translatedFormatTag)) {
    /* TODO: This can be optimized. */

    /*
    If we're seeking forward it's simple - just keep reading samples until we
    hit the sample we're requesting. If we're seeking backwards, we first need
    to seek back to the start and then just do the same thing as a forward seek.
    */
    if (targetFrameIndex < pWav->readCursorInPCMFrames) {
      if (!drwav_seek_to_first_pcm_frame(pWav)) { return false; }
    }

    if (targetFrameIndex > pWav->readCursorInPCMFrames) {
      uint64_t offsetInFrames = targetFrameIndex - pWav->readCursorInPCMFrames;

      int16_t devnull[2048];
      while (offsetInFrames > 0) {
        uint64_t framesRead = 0;
        uint64_t framesToRead = offsetInFrames;
        if (framesToRead > drwav_countof(devnull) / pWav->channels) {
          framesToRead = drwav_countof(devnull) / pWav->channels;
        }

        if (pWav->translatedFormatTag == DR_WAVE_FORMAT_ADPCM) {
          framesRead = drwav_read_pcm_frames_s16__msadpcm(pWav, framesToRead, devnull);
        } else if (pWav->translatedFormatTag == DR_WAVE_FORMAT_DVI_ADPCM) {
          framesRead = drwav_read_pcm_frames_s16__ima(pWav, framesToRead, devnull);
        } else {
          DRWAV_ASSERT(false); /* If this assertion is triggered it means I've
                                        implemented a new compressed format but forgot to
                                        add a branch for it here. */
        }

        if (framesRead != framesToRead) { return false; }

        offsetInFrames -= framesRead;
      }
    }
  } else {
    uint64_t totalSizeInBytes;
    uint64_t currentBytePos;
    uint64_t targetBytePos;
    uint64_t offset;

    totalSizeInBytes = pWav->totalPCMFrameCount * drwav_get_bytes_per_pcm_frame(pWav);
    DRWAV_ASSERT(totalSizeInBytes >= pWav->bytesRemaining);

    currentBytePos = totalSizeInBytes - pWav->bytesRemaining;
    targetBytePos = targetFrameIndex * drwav_get_bytes_per_pcm_frame(pWav);

    if (currentBytePos < targetBytePos) {
      /* Offset forwards. */
      offset = (targetBytePos - currentBytePos);
    } else {
      /* Offset backwards. */
      if (!drwav_seek_to_first_pcm_frame(pWav)) { return false; }
      offset = targetBytePos;
    }

    while (offset > 0) {
      int offset32 = ((offset > INT_MAX) ? INT_MAX : (int)offset);
      if (!pWav->onSeek(pWav->pUserData, offset32, drwav_seek_origin_current)) { return false; }

      pWav->readCursorInPCMFrames += offset32 / drwav_get_bytes_per_pcm_frame(pWav);
      pWav->bytesRemaining -= offset32;
      offset -= offset32;
    }
  }

  return true;
}

DRWAV_API drwav_result drwav_get_cursor_in_pcm_frames(drwav* pWav, uint64_t* pCursor) {
  if (pCursor == NULL) { return DRWAV_INVALID_ARGS; }

  *pCursor = 0; /* Safety. */

  if (pWav == NULL) { return DRWAV_INVALID_ARGS; }

  *pCursor = pWav->readCursorInPCMFrames;

  return DRWAV_SUCCESS;
}

DRWAV_API drwav_result drwav_get_length_in_pcm_frames(drwav* pWav, uint64_t* pLength) {
  if (pLength == NULL) { return DRWAV_INVALID_ARGS; }

  *pLength = 0; /* Safety. */

  if (pWav == NULL) { return DRWAV_INVALID_ARGS; }

  *pLength = pWav->totalPCMFrameCount;

  return DRWAV_SUCCESS;
}

DRWAV_API size_t drwav_write_raw(drwav* pWav, size_t bytesToWrite, const void* pData) {
  size_t bytesWritten;

  if (pWav == NULL || bytesToWrite == 0 || pData == NULL) { return 0; }

  bytesWritten = pWav->onWrite(pWav->pUserData, pData, bytesToWrite);
  pWav->dataChunkDataSize += bytesWritten;

  return bytesWritten;
}

DRWAV_API uint64_t drwav_write_pcm_frames_le(drwav* pWav, uint64_t framesToWrite,
                                             const void* pData) {
  uint64_t bytesToWrite;
  uint64_t bytesWritten;
  const uint8_t* pRunningData;

  if (pWav == NULL || framesToWrite == 0 || pData == NULL) { return 0; }

  bytesToWrite = ((framesToWrite * pWav->channels * pWav->bitsPerSample) / 8);
  if (bytesToWrite > DRWAV_SIZE_MAX) { return 0; }

  bytesWritten = 0;
  pRunningData = (const uint8_t*)pData;

  while (bytesToWrite > 0) {
    size_t bytesJustWritten;
    uint64_t bytesToWriteThisIteration;

    bytesToWriteThisIteration = bytesToWrite;
    DRWAV_ASSERT(bytesToWriteThisIteration <= DRWAV_SIZE_MAX); /* <-- This is checked above. */

    bytesJustWritten = drwav_write_raw(pWav, (size_t)bytesToWriteThisIteration, pRunningData);
    if (bytesJustWritten == 0) { break; }

    bytesToWrite -= bytesJustWritten;
    bytesWritten += bytesJustWritten;
    pRunningData += bytesJustWritten;
  }

  return (bytesWritten * 8) / pWav->bitsPerSample / pWav->channels;
}

DRWAV_API uint64_t drwav_write_pcm_frames_be(drwav* pWav, uint64_t framesToWrite,
                                             const void* pData) {
  uint64_t bytesToWrite;
  uint64_t bytesWritten;
  uint32_t bytesPerSample;
  const uint8_t* pRunningData;

  if (pWav == NULL || framesToWrite == 0 || pData == NULL) { return 0; }

  bytesToWrite = ((framesToWrite * pWav->channels * pWav->bitsPerSample) / 8);
  if (bytesToWrite > DRWAV_SIZE_MAX) { return 0; }

  bytesWritten = 0;
  pRunningData = (const uint8_t*)pData;

  bytesPerSample = drwav_get_bytes_per_pcm_frame(pWav) / pWav->channels;

  while (bytesToWrite > 0) {
    uint8_t temp[4096];
    uint32_t sampleCount;
    size_t bytesJustWritten;
    uint64_t bytesToWriteThisIteration;

    bytesToWriteThisIteration = bytesToWrite;
    DRWAV_ASSERT(bytesToWriteThisIteration <= DRWAV_SIZE_MAX); /* <-- This is checked above. */

    /*
    WAV files are always little-endian. We need to byte swap on big-endian
    architectures. Since our input buffer is read-only we need to use an
    intermediary buffer for the conversion.
    */
    sampleCount = sizeof(temp) / bytesPerSample;

    if (bytesToWriteThisIteration > ((uint64_t)sampleCount) * bytesPerSample) {
      bytesToWriteThisIteration = ((uint64_t)sampleCount) * bytesPerSample;
    }

    DRWAV_COPY_MEMORY(temp, pRunningData, (size_t)bytesToWriteThisIteration);
    drwav__bswap_samples(temp, sampleCount, bytesPerSample, pWav->translatedFormatTag);

    bytesJustWritten = drwav_write_raw(pWav, (size_t)bytesToWriteThisIteration, temp);
    if (bytesJustWritten == 0) { break; }

    bytesToWrite -= bytesJustWritten;
    bytesWritten += bytesJustWritten;
    pRunningData += bytesJustWritten;
  }

  return (bytesWritten * 8) / pWav->bitsPerSample / pWav->channels;
}

DRWAV_API uint64_t drwav_write_pcm_frames(drwav* pWav, uint64_t framesToWrite, const void* pData) {
  if (drwav__is_little_endian()) {
    return drwav_write_pcm_frames_le(pWav, framesToWrite, pData);
  } else {
    return drwav_write_pcm_frames_be(pWav, framesToWrite, pData);
  }
}

DRWAV_PRIVATE uint64_t drwav_read_pcm_frames_s16__msadpcm(drwav* pWav, uint64_t framesToRead,
                                                          int16_t* pBufferOut) {
  uint64_t totalFramesRead = 0;

  DRWAV_ASSERT(pWav != NULL);
  DRWAV_ASSERT(framesToRead > 0);

  /* TODO: Lots of room for optimization here. */

  while (pWav->readCursorInPCMFrames < pWav->totalPCMFrameCount) {
    DRWAV_ASSERT(framesToRead > 0); /* This loop iteration will never get hit with framesToRead
                                       == 0 because it's asserted at the top, and we check for
                                       0 inside the loop just below. */

    /* If there are no cached frames we need to load a new block. */
    if (pWav->msadpcm.cachedFrameCount == 0 && pWav->msadpcm.bytesRemainingInBlock == 0) {
      if (pWav->channels == 1) {
        /* Mono. */
        uint8_t header[7];
        if (pWav->onRead(pWav->pUserData, header, sizeof(header)) != sizeof(header)) {
          return totalFramesRead;
        }
        pWav->msadpcm.bytesRemainingInBlock = pWav->fmt.blockAlign - sizeof(header);

        pWav->msadpcm.predictor[0] = header[0];
        pWav->msadpcm.delta[0] = drwav_bytes_to_s16(header + 1);
        pWav->msadpcm.prevFrames[0][1] = (int32_t)drwav_bytes_to_s16(header + 3);
        pWav->msadpcm.prevFrames[0][0] = (int32_t)drwav_bytes_to_s16(header + 5);
        pWav->msadpcm.cachedFrames[2] = pWav->msadpcm.prevFrames[0][0];
        pWav->msadpcm.cachedFrames[3] = pWav->msadpcm.prevFrames[0][1];
        pWav->msadpcm.cachedFrameCount = 2;
      } else {
        /* Stereo. */
        uint8_t header[14];
        if (pWav->onRead(pWav->pUserData, header, sizeof(header)) != sizeof(header)) {
          return totalFramesRead;
        }
        pWav->msadpcm.bytesRemainingInBlock = pWav->fmt.blockAlign - sizeof(header);

        pWav->msadpcm.predictor[0] = header[0];
        pWav->msadpcm.predictor[1] = header[1];
        pWav->msadpcm.delta[0] = drwav_bytes_to_s16(header + 2);
        pWav->msadpcm.delta[1] = drwav_bytes_to_s16(header + 4);
        pWav->msadpcm.prevFrames[0][1] = (int32_t)drwav_bytes_to_s16(header + 6);
        pWav->msadpcm.prevFrames[1][1] = (int32_t)drwav_bytes_to_s16(header + 8);
        pWav->msadpcm.prevFrames[0][0] = (int32_t)drwav_bytes_to_s16(header + 10);
        pWav->msadpcm.prevFrames[1][0] = (int32_t)drwav_bytes_to_s16(header + 12);

        pWav->msadpcm.cachedFrames[0] = pWav->msadpcm.prevFrames[0][0];
        pWav->msadpcm.cachedFrames[1] = pWav->msadpcm.prevFrames[1][0];
        pWav->msadpcm.cachedFrames[2] = pWav->msadpcm.prevFrames[0][1];
        pWav->msadpcm.cachedFrames[3] = pWav->msadpcm.prevFrames[1][1];
        pWav->msadpcm.cachedFrameCount = 2;
      }
    }

    /* Output anything that's cached. */
    while (framesToRead > 0 && pWav->msadpcm.cachedFrameCount > 0 &&
           pWav->readCursorInPCMFrames < pWav->totalPCMFrameCount) {
      if (pBufferOut != NULL) {
        uint32_t iSample = 0;
        for (iSample = 0; iSample < pWav->channels; iSample += 1) {
          pBufferOut[iSample] =
              (int16_t)
                  pWav->msadpcm.cachedFrames[(drwav_countof(pWav->msadpcm.cachedFrames) -
                                              (pWav->msadpcm.cachedFrameCount * pWav->channels)) +
                                             iSample];
        }

        pBufferOut += pWav->channels;
      }

      framesToRead -= 1;
      totalFramesRead += 1;
      pWav->readCursorInPCMFrames += 1;
      pWav->msadpcm.cachedFrameCount -= 1;
    }

    if (framesToRead == 0) { break; }

    /*
    If there's nothing left in the cache, just go ahead and load more. If
    there's nothing left to load in the current block we just continue to the
    next loop iteration which will trigger the loading of a new block.
    */
    if (pWav->msadpcm.cachedFrameCount == 0) {
      if (pWav->msadpcm.bytesRemainingInBlock == 0) {
        continue;
      } else {
        static int32_t adaptationTable[] = {230, 230, 230, 230, 307, 409, 512, 614,
                                            768, 614, 512, 409, 307, 230, 230, 230};
        static int32_t coeff1Table[] = {256, 512, 0, 192, 240, 460, 392};
        static int32_t coeff2Table[] = {0, -256, 0, 64, 0, -208, -232};

        uint8_t nibbles;
        int32_t nibble0;
        int32_t nibble1;

        if (pWav->onRead(pWav->pUserData, &nibbles, 1) != 1) { return totalFramesRead; }
        pWav->msadpcm.bytesRemainingInBlock -= 1;

        /* TODO: Optimize away these if statements. */
        nibble0 = ((nibbles & 0xF0) >> 4);
        if ((nibbles & 0x80)) { nibble0 |= 0xFFFFFFF0UL; }
        nibble1 = ((nibbles & 0x0F) >> 0);
        if ((nibbles & 0x08)) { nibble1 |= 0xFFFFFFF0UL; }

        if (pWav->channels == 1) {
          /* Mono. */
          int32_t newSample0;
          int32_t newSample1;

          newSample0 =
              ((pWav->msadpcm.prevFrames[0][1] * coeff1Table[pWav->msadpcm.predictor[0]]) +
               (pWav->msadpcm.prevFrames[0][0] * coeff2Table[pWav->msadpcm.predictor[0]])) >>
              8;
          newSample0 += nibble0 * pWav->msadpcm.delta[0];
          newSample0 = drwav_clamp(newSample0, -32768, 32767);

          pWav->msadpcm.delta[0] =
              (adaptationTable[((nibbles & 0xF0) >> 4)] * pWav->msadpcm.delta[0]) >> 8;
          if (pWav->msadpcm.delta[0] < 16) { pWav->msadpcm.delta[0] = 16; }

          pWav->msadpcm.prevFrames[0][0] = pWav->msadpcm.prevFrames[0][1];
          pWav->msadpcm.prevFrames[0][1] = newSample0;

          newSample1 =
              ((pWav->msadpcm.prevFrames[0][1] * coeff1Table[pWav->msadpcm.predictor[0]]) +
               (pWav->msadpcm.prevFrames[0][0] * coeff2Table[pWav->msadpcm.predictor[0]])) >>
              8;
          newSample1 += nibble1 * pWav->msadpcm.delta[0];
          newSample1 = drwav_clamp(newSample1, -32768, 32767);

          pWav->msadpcm.delta[0] =
              (adaptationTable[((nibbles & 0x0F) >> 0)] * pWav->msadpcm.delta[0]) >> 8;
          if (pWav->msadpcm.delta[0] < 16) { pWav->msadpcm.delta[0] = 16; }

          pWav->msadpcm.prevFrames[0][0] = pWav->msadpcm.prevFrames[0][1];
          pWav->msadpcm.prevFrames[0][1] = newSample1;

          pWav->msadpcm.cachedFrames[2] = newSample0;
          pWav->msadpcm.cachedFrames[3] = newSample1;
          pWav->msadpcm.cachedFrameCount = 2;
        } else {
          /* Stereo. */
          int32_t newSample0;
          int32_t newSample1;

          /* Left. */
          newSample0 =
              ((pWav->msadpcm.prevFrames[0][1] * coeff1Table[pWav->msadpcm.predictor[0]]) +
               (pWav->msadpcm.prevFrames[0][0] * coeff2Table[pWav->msadpcm.predictor[0]])) >>
              8;
          newSample0 += nibble0 * pWav->msadpcm.delta[0];
          newSample0 = drwav_clamp(newSample0, -32768, 32767);

          pWav->msadpcm.delta[0] =
              (adaptationTable[((nibbles & 0xF0) >> 4)] * pWav->msadpcm.delta[0]) >> 8;
          if (pWav->msadpcm.delta[0] < 16) { pWav->msadpcm.delta[0] = 16; }

          pWav->msadpcm.prevFrames[0][0] = pWav->msadpcm.prevFrames[0][1];
          pWav->msadpcm.prevFrames[0][1] = newSample0;

          /* Right. */
          newSample1 =
              ((pWav->msadpcm.prevFrames[1][1] * coeff1Table[pWav->msadpcm.predictor[1]]) +
               (pWav->msadpcm.prevFrames[1][0] * coeff2Table[pWav->msadpcm.predictor[1]])) >>
              8;
          newSample1 += nibble1 * pWav->msadpcm.delta[1];
          newSample1 = drwav_clamp(newSample1, -32768, 32767);

          pWav->msadpcm.delta[1] =
              (adaptationTable[((nibbles & 0x0F) >> 0)] * pWav->msadpcm.delta[1]) >> 8;
          if (pWav->msadpcm.delta[1] < 16) { pWav->msadpcm.delta[1] = 16; }

          pWav->msadpcm.prevFrames[1][0] = pWav->msadpcm.prevFrames[1][1];
          pWav->msadpcm.prevFrames[1][1] = newSample1;

          pWav->msadpcm.cachedFrames[2] = newSample0;
          pWav->msadpcm.cachedFrames[3] = newSample1;
          pWav->msadpcm.cachedFrameCount = 1;
        }
      }
    }
  }

  return totalFramesRead;
}

DRWAV_PRIVATE uint64_t drwav_read_pcm_frames_s16__ima(drwav* pWav, uint64_t framesToRead,
                                                      int16_t* pBufferOut) {
  uint64_t totalFramesRead = 0;
  uint32_t iChannel;

  static int32_t indexTable[16] = {-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8};

  static int32_t stepTable[89] = {
      7,     8,     9,     10,    11,    12,    13,    14,    16,    17,    19,   21,    23,
      25,    28,    31,    34,    37,    41,    45,    50,    55,    60,    66,   73,    80,
      88,    97,    107,   118,   130,   143,   157,   173,   190,   209,   230,  253,   279,
      307,   337,   371,   408,   449,   494,   544,   598,   658,   724,   796,  876,   963,
      1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066,  2272,  2499,  2749, 3024,  3327,
      3660,  4026,  4428,  4871,  5358,  5894,  6484,  7132,  7845,  8630,  9493, 10442, 11487,
      12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};

  DRWAV_ASSERT(pWav != NULL);
  DRWAV_ASSERT(framesToRead > 0);

  /* TODO: Lots of room for optimization here. */

  while (pWav->readCursorInPCMFrames < pWav->totalPCMFrameCount) {
    DRWAV_ASSERT(framesToRead > 0); /* This loop iteration will never get hit with framesToRead
                                       == 0 because it's asserted at the top, and we check for
                                       0 inside the loop just below. */

    /* If there are no cached samples we need to load a new block. */
    if (pWav->ima.cachedFrameCount == 0 && pWav->ima.bytesRemainingInBlock == 0) {
      if (pWav->channels == 1) {
        /* Mono. */
        uint8_t header[4];
        if (pWav->onRead(pWav->pUserData, header, sizeof(header)) != sizeof(header)) {
          return totalFramesRead;
        }
        pWav->ima.bytesRemainingInBlock = pWav->fmt.blockAlign - sizeof(header);

        if (header[2] >= drwav_countof(stepTable)) {
          pWav->onSeek(pWav->pUserData, pWav->ima.bytesRemainingInBlock, drwav_seek_origin_current);
          pWav->ima.bytesRemainingInBlock = 0;
          return totalFramesRead; /* Invalid data. */
        }

        pWav->ima.predictor[0] = drwav_bytes_to_s16(header + 0);
        pWav->ima.stepIndex[0] = header[2];
        pWav->ima.cachedFrames[drwav_countof(pWav->ima.cachedFrames) - 1] = pWav->ima.predictor[0];
        pWav->ima.cachedFrameCount = 1;
      } else {
        /* Stereo. */
        uint8_t header[8];
        if (pWav->onRead(pWav->pUserData, header, sizeof(header)) != sizeof(header)) {
          return totalFramesRead;
        }
        pWav->ima.bytesRemainingInBlock = pWav->fmt.blockAlign - sizeof(header);

        if (header[2] >= drwav_countof(stepTable) || header[6] >= drwav_countof(stepTable)) {
          pWav->onSeek(pWav->pUserData, pWav->ima.bytesRemainingInBlock, drwav_seek_origin_current);
          pWav->ima.bytesRemainingInBlock = 0;
          return totalFramesRead; /* Invalid data. */
        }

        pWav->ima.predictor[0] = drwav_bytes_to_s16(header + 0);
        pWav->ima.stepIndex[0] = header[2];
        pWav->ima.predictor[1] = drwav_bytes_to_s16(header + 4);
        pWav->ima.stepIndex[1] = header[6];

        pWav->ima.cachedFrames[drwav_countof(pWav->ima.cachedFrames) - 2] = pWav->ima.predictor[0];
        pWav->ima.cachedFrames[drwav_countof(pWav->ima.cachedFrames) - 1] = pWav->ima.predictor[1];
        pWav->ima.cachedFrameCount = 1;
      }
    }

    /* Output anything that's cached. */
    while (framesToRead > 0 && pWav->ima.cachedFrameCount > 0 &&
           pWav->readCursorInPCMFrames < pWav->totalPCMFrameCount) {
      if (pBufferOut != NULL) {
        uint32_t iSample;
        for (iSample = 0; iSample < pWav->channels; iSample += 1) {
          pBufferOut[iSample] =
              (int16_t)pWav->ima.cachedFrames[(drwav_countof(pWav->ima.cachedFrames) -
                                               (pWav->ima.cachedFrameCount * pWav->channels)) +
                                              iSample];
        }
        pBufferOut += pWav->channels;
      }

      framesToRead -= 1;
      totalFramesRead += 1;
      pWav->readCursorInPCMFrames += 1;
      pWav->ima.cachedFrameCount -= 1;
    }

    if (framesToRead == 0) { break; }

    /*
    If there's nothing left in the cache, just go ahead and load more. If
    there's nothing left to load in the current block we just continue to the
    next loop iteration which will trigger the loading of a new block.
    */
    if (pWav->ima.cachedFrameCount == 0) {
      if (pWav->ima.bytesRemainingInBlock == 0) {
        continue;
      } else {
        /*
        From what I can tell with stereo streams, it looks like every 4 bytes (8
        samples) is for one channel. So it goes 4 bytes for the left channel, 4
        bytes for the right channel.
        */
        pWav->ima.cachedFrameCount = 8;
        for (iChannel = 0; iChannel < pWav->channels; ++iChannel) {
          uint32_t iByte;
          uint8_t nibbles[4];
          if (pWav->onRead(pWav->pUserData, &nibbles, 4) != 4) {
            pWav->ima.cachedFrameCount = 0;
            return totalFramesRead;
          }
          pWav->ima.bytesRemainingInBlock -= 4;

          for (iByte = 0; iByte < 4; ++iByte) {
            uint8_t nibble0 = ((nibbles[iByte] & 0x0F) >> 0);
            uint8_t nibble1 = ((nibbles[iByte] & 0xF0) >> 4);

            int32_t step = stepTable[pWav->ima.stepIndex[iChannel]];
            int32_t predictor = pWav->ima.predictor[iChannel];

            int32_t diff = step >> 3;
            if (nibble0 & 1) diff += step >> 2;
            if (nibble0 & 2) diff += step >> 1;
            if (nibble0 & 4) diff += step;
            if (nibble0 & 8) diff = -diff;

            predictor = drwav_clamp(predictor + diff, -32768, 32767);
            pWav->ima.predictor[iChannel] = predictor;
            pWav->ima.stepIndex[iChannel] =
                drwav_clamp(pWav->ima.stepIndex[iChannel] + indexTable[nibble0], 0,
                            (int32_t)drwav_countof(stepTable) - 1);
            pWav->ima.cachedFrames[(drwav_countof(pWav->ima.cachedFrames) -
                                    (pWav->ima.cachedFrameCount * pWav->channels)) +
                                   (iByte * 2 + 0) * pWav->channels + iChannel] = predictor;

            step = stepTable[pWav->ima.stepIndex[iChannel]];
            predictor = pWav->ima.predictor[iChannel];

            diff = step >> 3;
            if (nibble1 & 1) diff += step >> 2;
            if (nibble1 & 2) diff += step >> 1;
            if (nibble1 & 4) diff += step;
            if (nibble1 & 8) diff = -diff;

            predictor = drwav_clamp(predictor + diff, -32768, 32767);
            pWav->ima.predictor[iChannel] = predictor;
            pWav->ima.stepIndex[iChannel] =
                drwav_clamp(pWav->ima.stepIndex[iChannel] + indexTable[nibble1], 0,
                            (int32_t)drwav_countof(stepTable) - 1);
            pWav->ima.cachedFrames[(drwav_countof(pWav->ima.cachedFrames) -
                                    (pWav->ima.cachedFrameCount * pWav->channels)) +
                                   (iByte * 2 + 1) * pWav->channels + iChannel] = predictor;
          }
        }
      }
    }
  }

  return totalFramesRead;
}

#ifndef DR_WAV_NO_CONVERSION_API
static unsigned short g_drwavAlawTable[256] = {
    0xEA80, 0xEB80, 0xE880, 0xE980, 0xEE80, 0xEF80, 0xEC80, 0xED80, 0xE280, 0xE380, 0xE080, 0xE180,
    0xE680, 0xE780, 0xE480, 0xE580, 0xF540, 0xF5C0, 0xF440, 0xF4C0, 0xF740, 0xF7C0, 0xF640, 0xF6C0,
    0xF140, 0xF1C0, 0xF040, 0xF0C0, 0xF340, 0xF3C0, 0xF240, 0xF2C0, 0xAA00, 0xAE00, 0xA200, 0xA600,
    0xBA00, 0xBE00, 0xB200, 0xB600, 0x8A00, 0x8E00, 0x8200, 0x8600, 0x9A00, 0x9E00, 0x9200, 0x9600,
    0xD500, 0xD700, 0xD100, 0xD300, 0xDD00, 0xDF00, 0xD900, 0xDB00, 0xC500, 0xC700, 0xC100, 0xC300,
    0xCD00, 0xCF00, 0xC900, 0xCB00, 0xFEA8, 0xFEB8, 0xFE88, 0xFE98, 0xFEE8, 0xFEF8, 0xFEC8, 0xFED8,
    0xFE28, 0xFE38, 0xFE08, 0xFE18, 0xFE68, 0xFE78, 0xFE48, 0xFE58, 0xFFA8, 0xFFB8, 0xFF88, 0xFF98,
    0xFFE8, 0xFFF8, 0xFFC8, 0xFFD8, 0xFF28, 0xFF38, 0xFF08, 0xFF18, 0xFF68, 0xFF78, 0xFF48, 0xFF58,
    0xFAA0, 0xFAE0, 0xFA20, 0xFA60, 0xFBA0, 0xFBE0, 0xFB20, 0xFB60, 0xF8A0, 0xF8E0, 0xF820, 0xF860,
    0xF9A0, 0xF9E0, 0xF920, 0xF960, 0xFD50, 0xFD70, 0xFD10, 0xFD30, 0xFDD0, 0xFDF0, 0xFD90, 0xFDB0,
    0xFC50, 0xFC70, 0xFC10, 0xFC30, 0xFCD0, 0xFCF0, 0xFC90, 0xFCB0, 0x1580, 0x1480, 0x1780, 0x1680,
    0x1180, 0x1080, 0x1380, 0x1280, 0x1D80, 0x1C80, 0x1F80, 0x1E80, 0x1980, 0x1880, 0x1B80, 0x1A80,
    0x0AC0, 0x0A40, 0x0BC0, 0x0B40, 0x08C0, 0x0840, 0x09C0, 0x0940, 0x0EC0, 0x0E40, 0x0FC0, 0x0F40,
    0x0CC0, 0x0C40, 0x0DC0, 0x0D40, 0x5600, 0x5200, 0x5E00, 0x5A00, 0x4600, 0x4200, 0x4E00, 0x4A00,
    0x7600, 0x7200, 0x7E00, 0x7A00, 0x6600, 0x6200, 0x6E00, 0x6A00, 0x2B00, 0x2900, 0x2F00, 0x2D00,
    0x2300, 0x2100, 0x2700, 0x2500, 0x3B00, 0x3900, 0x3F00, 0x3D00, 0x3300, 0x3100, 0x3700, 0x3500,
    0x0158, 0x0148, 0x0178, 0x0168, 0x0118, 0x0108, 0x0138, 0x0128, 0x01D8, 0x01C8, 0x01F8, 0x01E8,
    0x0198, 0x0188, 0x01B8, 0x01A8, 0x0058, 0x0048, 0x0078, 0x0068, 0x0018, 0x0008, 0x0038, 0x0028,
    0x00D8, 0x00C8, 0x00F8, 0x00E8, 0x0098, 0x0088, 0x00B8, 0x00A8, 0x0560, 0x0520, 0x05E0, 0x05A0,
    0x0460, 0x0420, 0x04E0, 0x04A0, 0x0760, 0x0720, 0x07E0, 0x07A0, 0x0660, 0x0620, 0x06E0, 0x06A0,
    0x02B0, 0x0290, 0x02F0, 0x02D0, 0x0230, 0x0210, 0x0270, 0x0250, 0x03B0, 0x0390, 0x03F0, 0x03D0,
    0x0330, 0x0310, 0x0370, 0x0350};

static unsigned short g_drwavMulawTable[256] = {
    0x8284, 0x8684, 0x8A84, 0x8E84, 0x9284, 0x9684, 0x9A84, 0x9E84, 0xA284, 0xA684, 0xAA84, 0xAE84,
    0xB284, 0xB684, 0xBA84, 0xBE84, 0xC184, 0xC384, 0xC584, 0xC784, 0xC984, 0xCB84, 0xCD84, 0xCF84,
    0xD184, 0xD384, 0xD584, 0xD784, 0xD984, 0xDB84, 0xDD84, 0xDF84, 0xE104, 0xE204, 0xE304, 0xE404,
    0xE504, 0xE604, 0xE704, 0xE804, 0xE904, 0xEA04, 0xEB04, 0xEC04, 0xED04, 0xEE04, 0xEF04, 0xF004,
    0xF0C4, 0xF144, 0xF1C4, 0xF244, 0xF2C4, 0xF344, 0xF3C4, 0xF444, 0xF4C4, 0xF544, 0xF5C4, 0xF644,
    0xF6C4, 0xF744, 0xF7C4, 0xF844, 0xF8A4, 0xF8E4, 0xF924, 0xF964, 0xF9A4, 0xF9E4, 0xFA24, 0xFA64,
    0xFAA4, 0xFAE4, 0xFB24, 0xFB64, 0xFBA4, 0xFBE4, 0xFC24, 0xFC64, 0xFC94, 0xFCB4, 0xFCD4, 0xFCF4,
    0xFD14, 0xFD34, 0xFD54, 0xFD74, 0xFD94, 0xFDB4, 0xFDD4, 0xFDF4, 0xFE14, 0xFE34, 0xFE54, 0xFE74,
    0xFE8C, 0xFE9C, 0xFEAC, 0xFEBC, 0xFECC, 0xFEDC, 0xFEEC, 0xFEFC, 0xFF0C, 0xFF1C, 0xFF2C, 0xFF3C,
    0xFF4C, 0xFF5C, 0xFF6C, 0xFF7C, 0xFF88, 0xFF90, 0xFF98, 0xFFA0, 0xFFA8, 0xFFB0, 0xFFB8, 0xFFC0,
    0xFFC8, 0xFFD0, 0xFFD8, 0xFFE0, 0xFFE8, 0xFFF0, 0xFFF8, 0x0000, 0x7D7C, 0x797C, 0x757C, 0x717C,
    0x6D7C, 0x697C, 0x657C, 0x617C, 0x5D7C, 0x597C, 0x557C, 0x517C, 0x4D7C, 0x497C, 0x457C, 0x417C,
    0x3E7C, 0x3C7C, 0x3A7C, 0x387C, 0x367C, 0x347C, 0x327C, 0x307C, 0x2E7C, 0x2C7C, 0x2A7C, 0x287C,
    0x267C, 0x247C, 0x227C, 0x207C, 0x1EFC, 0x1DFC, 0x1CFC, 0x1BFC, 0x1AFC, 0x19FC, 0x18FC, 0x17FC,
    0x16FC, 0x15FC, 0x14FC, 0x13FC, 0x12FC, 0x11FC, 0x10FC, 0x0FFC, 0x0F3C, 0x0EBC, 0x0E3C, 0x0DBC,
    0x0D3C, 0x0CBC, 0x0C3C, 0x0BBC, 0x0B3C, 0x0ABC, 0x0A3C, 0x09BC, 0x093C, 0x08BC, 0x083C, 0x07BC,
    0x075C, 0x071C, 0x06DC, 0x069C, 0x065C, 0x061C, 0x05DC, 0x059C, 0x055C, 0x051C, 0x04DC, 0x049C,
    0x045C, 0x041C, 0x03DC, 0x039C, 0x036C, 0x034C, 0x032C, 0x030C, 0x02EC, 0x02CC, 0x02AC, 0x028C,
    0x026C, 0x024C, 0x022C, 0x020C, 0x01EC, 0x01CC, 0x01AC, 0x018C, 0x0174, 0x0164, 0x0154, 0x0144,
    0x0134, 0x0124, 0x0114, 0x0104, 0x00F4, 0x00E4, 0x00D4, 0x00C4, 0x00B4, 0x00A4, 0x0094, 0x0084,
    0x0078, 0x0070, 0x0068, 0x0060, 0x0058, 0x0050, 0x0048, 0x0040, 0x0038, 0x0030, 0x0028, 0x0020,
    0x0018, 0x0010, 0x0008, 0x0000};

static DRWAV_INLINE int16_t drwav__alaw_to_s16(uint8_t sampleIn) {
  return (short)g_drwavAlawTable[sampleIn];
}

static DRWAV_INLINE int16_t drwav__mulaw_to_s16(uint8_t sampleIn) {
  return (short)g_drwavMulawTable[sampleIn];
}

DRWAV_PRIVATE void drwav__pcm_to_s16(int16_t* pOut, const uint8_t* pIn, size_t totalSampleCount,
                                     unsigned int bytesPerSample) {
  unsigned int i;

  /* Special case for 8-bit sample data because it's treated as unsigned. */
  if (bytesPerSample == 1) {
    drwav_u8_to_s16(pOut, pIn, totalSampleCount);
    return;
  }

  /* Slightly more optimal implementation for common formats. */
  if (bytesPerSample == 2) {
    for (i = 0; i < totalSampleCount; ++i) { *pOut++ = ((const int16_t*)pIn)[i]; }
    return;
  }
  if (bytesPerSample == 3) {
    drwav_s24_to_s16(pOut, pIn, totalSampleCount);
    return;
  }
  if (bytesPerSample == 4) {
    drwav_s32_to_s16(pOut, (const int32_t*)pIn, totalSampleCount);
    return;
  }

  /* Anything more than 64 bits per sample is not supported. */
  if (bytesPerSample > 8) {
    DRWAV_ZERO_MEMORY(pOut, totalSampleCount * sizeof(*pOut));
    return;
  }

  /* Generic, slow converter. */
  for (i = 0; i < totalSampleCount; ++i) {
    uint64_t sample = 0;
    unsigned int shift = (8 - bytesPerSample) * 8;

    unsigned int j;
    for (j = 0; j < bytesPerSample; j += 1) {
      DRWAV_ASSERT(j < 8);
      sample |= (uint64_t)(pIn[j]) << shift;
      shift += 8;
    }

    pIn += j;
    *pOut++ = (int16_t)((int64_t)sample >> 48);
  }
}

DRWAV_PRIVATE void drwav__ieee_to_s16(int16_t* pOut, const uint8_t* pIn, size_t totalSampleCount,
                                      unsigned int bytesPerSample) {
  if (bytesPerSample == 4) {
    drwav_f32_to_s16(pOut, (const float*)pIn, totalSampleCount);
    return;
  } else if (bytesPerSample == 8) {
    drwav_f64_to_s16(pOut, (const double*)pIn, totalSampleCount);
    return;
  } else {
    /* Only supporting 32- and 64-bit float. Output silence in all other cases.
     * Contributions welcome for 16-bit float. */
    DRWAV_ZERO_MEMORY(pOut, totalSampleCount * sizeof(*pOut));
    return;
  }
}

DRWAV_PRIVATE uint64_t drwav_read_pcm_frames_s16__pcm(drwav* pWav, uint64_t framesToRead,
                                                      int16_t* pBufferOut) {
  uint64_t totalFramesRead;
  uint8_t sampleData[4096];
  uint32_t bytesPerFrame;

  /* Fast path. */
  if ((pWav->translatedFormatTag == DR_WAVE_FORMAT_PCM && pWav->bitsPerSample == 16) ||
      pBufferOut == NULL) {
    return drwav_read_pcm_frames(pWav, framesToRead, pBufferOut);
  }

  bytesPerFrame = drwav_get_bytes_per_pcm_frame(pWav);
  if (bytesPerFrame == 0) { return 0; }

  totalFramesRead = 0;

  while (framesToRead > 0) {
    uint64_t framesRead = drwav_read_pcm_frames(
        pWav, drwav_min(framesToRead, sizeof(sampleData) / bytesPerFrame), sampleData);
    if (framesRead == 0) { break; }

    drwav__pcm_to_s16(pBufferOut, sampleData, (size_t)(framesRead * pWav->channels),
                      bytesPerFrame / pWav->channels);

    pBufferOut += framesRead * pWav->channels;
    framesToRead -= framesRead;
    totalFramesRead += framesRead;
  }

  return totalFramesRead;
}

DRWAV_PRIVATE uint64_t drwav_read_pcm_frames_s16__ieee(drwav* pWav, uint64_t framesToRead,
                                                       int16_t* pBufferOut) {
  uint64_t totalFramesRead;
  uint8_t sampleData[4096];
  uint32_t bytesPerFrame;

  if (pBufferOut == NULL) { return drwav_read_pcm_frames(pWav, framesToRead, NULL); }

  bytesPerFrame = drwav_get_bytes_per_pcm_frame(pWav);
  if (bytesPerFrame == 0) { return 0; }

  totalFramesRead = 0;

  while (framesToRead > 0) {
    uint64_t framesRead = drwav_read_pcm_frames(
        pWav, drwav_min(framesToRead, sizeof(sampleData) / bytesPerFrame), sampleData);
    if (framesRead == 0) { break; }

    drwav__ieee_to_s16(pBufferOut, sampleData, (size_t)(framesRead * pWav->channels),
                       bytesPerFrame / pWav->channels);

    pBufferOut += framesRead * pWav->channels;
    framesToRead -= framesRead;
    totalFramesRead += framesRead;
  }

  return totalFramesRead;
}

DRWAV_PRIVATE uint64_t drwav_read_pcm_frames_s16__alaw(drwav* pWav, uint64_t framesToRead,
                                                       int16_t* pBufferOut) {
  uint64_t totalFramesRead;
  uint8_t sampleData[4096];
  uint32_t bytesPerFrame;

  if (pBufferOut == NULL) { return drwav_read_pcm_frames(pWav, framesToRead, NULL); }

  bytesPerFrame = drwav_get_bytes_per_pcm_frame(pWav);
  if (bytesPerFrame == 0) { return 0; }

  totalFramesRead = 0;

  while (framesToRead > 0) {
    uint64_t framesRead = drwav_read_pcm_frames(
        pWav, drwav_min(framesToRead, sizeof(sampleData) / bytesPerFrame), sampleData);
    if (framesRead == 0) { break; }

    drwav_alaw_to_s16(pBufferOut, sampleData, (size_t)(framesRead * pWav->channels));

    pBufferOut += framesRead * pWav->channels;
    framesToRead -= framesRead;
    totalFramesRead += framesRead;
  }

  return totalFramesRead;
}

DRWAV_PRIVATE uint64_t drwav_read_pcm_frames_s16__mulaw(drwav* pWav, uint64_t framesToRead,
                                                        int16_t* pBufferOut) {
  uint64_t totalFramesRead;
  uint8_t sampleData[4096];
  uint32_t bytesPerFrame;

  if (pBufferOut == NULL) { return drwav_read_pcm_frames(pWav, framesToRead, NULL); }

  bytesPerFrame = drwav_get_bytes_per_pcm_frame(pWav);
  if (bytesPerFrame == 0) { return 0; }

  totalFramesRead = 0;

  while (framesToRead > 0) {
    uint64_t framesRead = drwav_read_pcm_frames(
        pWav, drwav_min(framesToRead, sizeof(sampleData) / bytesPerFrame), sampleData);
    if (framesRead == 0) { break; }

    drwav_mulaw_to_s16(pBufferOut, sampleData, (size_t)(framesRead * pWav->channels));

    pBufferOut += framesRead * pWav->channels;
    framesToRead -= framesRead;
    totalFramesRead += framesRead;
  }

  return totalFramesRead;
}

DRWAV_API uint64_t drwav_read_pcm_frames_s16(drwav* pWav, uint64_t framesToRead,
                                             int16_t* pBufferOut) {
  if (pWav == NULL || framesToRead == 0) { return 0; }

  if (pBufferOut == NULL) { return drwav_read_pcm_frames(pWav, framesToRead, NULL); }

  /* Don't try to read more samples than can potentially fit in the output
   * buffer. */
  if (framesToRead * pWav->channels * sizeof(int16_t) > DRWAV_SIZE_MAX) {
    framesToRead = DRWAV_SIZE_MAX / sizeof(int16_t) / pWav->channels;
  }

  if (pWav->translatedFormatTag == DR_WAVE_FORMAT_PCM) {
    return drwav_read_pcm_frames_s16__pcm(pWav, framesToRead, pBufferOut);
  }

  if (pWav->translatedFormatTag == DR_WAVE_FORMAT_IEEE_FLOAT) {
    return drwav_read_pcm_frames_s16__ieee(pWav, framesToRead, pBufferOut);
  }

  if (pWav->translatedFormatTag == DR_WAVE_FORMAT_ALAW) {
    return drwav_read_pcm_frames_s16__alaw(pWav, framesToRead, pBufferOut);
  }

  if (pWav->translatedFormatTag == DR_WAVE_FORMAT_MULAW) {
    return drwav_read_pcm_frames_s16__mulaw(pWav, framesToRead, pBufferOut);
  }

  if (pWav->translatedFormatTag == DR_WAVE_FORMAT_ADPCM) {
    return drwav_read_pcm_frames_s16__msadpcm(pWav, framesToRead, pBufferOut);
  }

  if (pWav->translatedFormatTag == DR_WAVE_FORMAT_DVI_ADPCM) {
    return drwav_read_pcm_frames_s16__ima(pWav, framesToRead, pBufferOut);
  }

  return 0;
}

DRWAV_API uint64_t drwav_read_pcm_frames_s16le(drwav* pWav, uint64_t framesToRead,
                                               int16_t* pBufferOut) {
  uint64_t framesRead = drwav_read_pcm_frames_s16(pWav, framesToRead, pBufferOut);
  if (pBufferOut != NULL && drwav__is_little_endian() == false) {
    drwav__bswap_samples_s16(pBufferOut, framesRead * pWav->channels);
  }

  return framesRead;
}

DRWAV_API uint64_t drwav_read_pcm_frames_s16be(drwav* pWav, uint64_t framesToRead,
                                               int16_t* pBufferOut) {
  uint64_t framesRead = drwav_read_pcm_frames_s16(pWav, framesToRead, pBufferOut);
  if (pBufferOut != NULL && drwav__is_little_endian() == true) {
    drwav__bswap_samples_s16(pBufferOut, framesRead * pWav->channels);
  }

  return framesRead;
}

DRWAV_API void drwav_u8_to_s16(int16_t* pOut, const uint8_t* pIn, size_t sampleCount) {
  int r;
  size_t i;
  for (i = 0; i < sampleCount; ++i) {
    int x = pIn[i];
    r = x << 8;
    r = r - 32768;
    pOut[i] = (short)r;
  }
}

DRWAV_API void drwav_s24_to_s16(int16_t* pOut, const uint8_t* pIn, size_t sampleCount) {
  int r;
  size_t i;
  for (i = 0; i < sampleCount; ++i) {
    int x = ((int)(((unsigned int)(((const uint8_t*)pIn)[i * 3 + 0]) << 8) |
                   ((unsigned int)(((const uint8_t*)pIn)[i * 3 + 1]) << 16) |
                   ((unsigned int)(((const uint8_t*)pIn)[i * 3 + 2])) << 24)) >>
            8;
    r = x >> 8;
    pOut[i] = (short)r;
  }
}

DRWAV_API void drwav_s32_to_s16(int16_t* pOut, const int32_t* pIn, size_t sampleCount) {
  int r;
  size_t i;
  for (i = 0; i < sampleCount; ++i) {
    int x = pIn[i];
    r = x >> 16;
    pOut[i] = (short)r;
  }
}

DRWAV_API void drwav_f32_to_s16(int16_t* pOut, const float* pIn, size_t sampleCount) {
  int r;
  size_t i;
  for (i = 0; i < sampleCount; ++i) {
    float x = pIn[i];
    float c;
    c = ((x < -1) ? -1 : ((x > 1) ? 1 : x));
    c = c + 1;
    r = (int)(c * 32767.5f);
    r = r - 32768;
    pOut[i] = (short)r;
  }
}

DRWAV_API void drwav_f64_to_s16(int16_t* pOut, const double* pIn, size_t sampleCount) {
  int r;
  size_t i;
  for (i = 0; i < sampleCount; ++i) {
    double x = pIn[i];
    double c;
    c = ((x < -1) ? -1 : ((x > 1) ? 1 : x));
    c = c + 1;
    r = (int)(c * 32767.5);
    r = r - 32768;
    pOut[i] = (short)r;
  }
}

DRWAV_API void drwav_alaw_to_s16(int16_t* pOut, const uint8_t* pIn, size_t sampleCount) {
  size_t i;
  for (i = 0; i < sampleCount; ++i) { pOut[i] = drwav__alaw_to_s16(pIn[i]); }
}

DRWAV_API void drwav_mulaw_to_s16(int16_t* pOut, const uint8_t* pIn, size_t sampleCount) {
  size_t i;
  for (i = 0; i < sampleCount; ++i) { pOut[i] = drwav__mulaw_to_s16(pIn[i]); }
}

DRWAV_PRIVATE void drwav__pcm_to_f32(float* pOut, const uint8_t* pIn, size_t sampleCount,
                                     unsigned int bytesPerSample) {
  unsigned int i;

  /* Special case for 8-bit sample data because it's treated as unsigned. */
  if (bytesPerSample == 1) {
    drwav_u8_to_f32(pOut, pIn, sampleCount);
    return;
  }

  /* Slightly more optimal implementation for common formats. */
  if (bytesPerSample == 2) {
    drwav_s16_to_f32(pOut, (const int16_t*)pIn, sampleCount);
    return;
  }
  if (bytesPerSample == 3) {
    drwav_s24_to_f32(pOut, pIn, sampleCount);
    return;
  }
  if (bytesPerSample == 4) {
    drwav_s32_to_f32(pOut, (const int32_t*)pIn, sampleCount);
    return;
  }

  /* Anything more than 64 bits per sample is not supported. */
  if (bytesPerSample > 8) {
    DRWAV_ZERO_MEMORY(pOut, sampleCount * sizeof(*pOut));
    return;
  }

  /* Generic, slow converter. */
  for (i = 0; i < sampleCount; ++i) {
    uint64_t sample = 0;
    unsigned int shift = (8 - bytesPerSample) * 8;

    unsigned int j;
    for (j = 0; j < bytesPerSample; j += 1) {
      DRWAV_ASSERT(j < 8);
      sample |= (uint64_t)(pIn[j]) << shift;
      shift += 8;
    }

    pIn += j;
    *pOut++ = (float)((int64_t)sample / 9223372036854775807.0);
  }
}

DRWAV_PRIVATE void drwav__ieee_to_f32(float* pOut, const uint8_t* pIn, size_t sampleCount,
                                      unsigned int bytesPerSample) {
  if (bytesPerSample == 4) {
    unsigned int i;
    for (i = 0; i < sampleCount; ++i) { *pOut++ = ((const float*)pIn)[i]; }
    return;
  } else if (bytesPerSample == 8) {
    drwav_f64_to_f32(pOut, (const double*)pIn, sampleCount);
    return;
  } else {
    /* Only supporting 32- and 64-bit float. Output silence in all other cases.
     * Contributions welcome for 16-bit float. */
    DRWAV_ZERO_MEMORY(pOut, sampleCount * sizeof(*pOut));
    return;
  }
}

DRWAV_PRIVATE uint64_t drwav_read_pcm_frames_f32__pcm(drwav* pWav, uint64_t framesToRead,
                                                      float* pBufferOut) {
  uint64_t totalFramesRead;
  uint8_t sampleData[4096];
  uint32_t bytesPerFrame = drwav_get_bytes_per_pcm_frame(pWav);

  if (bytesPerFrame == 0) { return 0; }

  totalFramesRead = 0;

  while (framesToRead > 0) {
    uint64_t framesRead = drwav_read_pcm_frames(
        pWav, drwav_min(framesToRead, sizeof(sampleData) / bytesPerFrame), sampleData);
    if (framesRead == 0) { break; }

    drwav__pcm_to_f32(pBufferOut, sampleData, (size_t)framesRead * pWav->channels,
                      bytesPerFrame / pWav->channels);

    pBufferOut += framesRead * pWav->channels;
    framesToRead -= framesRead;
    totalFramesRead += framesRead;
  }

  return totalFramesRead;
}

DRWAV_PRIVATE uint64_t drwav_read_pcm_frames_f32__msadpcm(drwav* pWav, uint64_t framesToRead,
                                                          float* pBufferOut) {
  /*
  We're just going to borrow the implementation from the drwav_read_s16() since
  ADPCM is a little bit more complicated than other formats and I don't want to
  duplicate that code.
  */
  uint64_t totalFramesRead = 0;
  int16_t samples16[2048];

  while (framesToRead > 0) {
    uint64_t framesRead = drwav_read_pcm_frames_s16(
        pWav, drwav_min(framesToRead, drwav_countof(samples16) / pWav->channels), samples16);
    if (framesRead == 0) { break; }

    drwav_s16_to_f32(pBufferOut, samples16,
                     (size_t)(framesRead * pWav->channels)); /* <-- Safe cast because we're
                                                                clamping to 2048. */

    pBufferOut += framesRead * pWav->channels;
    framesToRead -= framesRead;
    totalFramesRead += framesRead;
  }

  return totalFramesRead;
}

DRWAV_PRIVATE uint64_t drwav_read_pcm_frames_f32__ima(drwav* pWav, uint64_t framesToRead,
                                                      float* pBufferOut) {
  /*
  We're just going to borrow the implementation from the drwav_read_s16() since
  IMA-ADPCM is a little bit more complicated than other formats and I don't want
  to duplicate that code.
  */
  uint64_t totalFramesRead = 0;
  int16_t samples16[2048];

  while (framesToRead > 0) {
    uint64_t framesRead = drwav_read_pcm_frames_s16(
        pWav, drwav_min(framesToRead, drwav_countof(samples16) / pWav->channels), samples16);
    if (framesRead == 0) { break; }

    drwav_s16_to_f32(pBufferOut, samples16,
                     (size_t)(framesRead * pWav->channels)); /* <-- Safe cast because we're
                                                                clamping to 2048. */

    pBufferOut += framesRead * pWav->channels;
    framesToRead -= framesRead;
    totalFramesRead += framesRead;
  }

  return totalFramesRead;
}

DRWAV_PRIVATE uint64_t drwav_read_pcm_frames_f32__ieee(drwav* pWav, uint64_t framesToRead,
                                                       float* pBufferOut) {
  uint64_t totalFramesRead;
  uint8_t sampleData[4096];
  uint32_t bytesPerFrame;

  /* Fast path. */
  if (pWav->translatedFormatTag == DR_WAVE_FORMAT_IEEE_FLOAT && pWav->bitsPerSample == 32) {
    return drwav_read_pcm_frames(pWav, framesToRead, pBufferOut);
  }

  bytesPerFrame = drwav_get_bytes_per_pcm_frame(pWav);
  if (bytesPerFrame == 0) { return 0; }

  totalFramesRead = 0;

  while (framesToRead > 0) {
    uint64_t framesRead = drwav_read_pcm_frames(
        pWav, drwav_min(framesToRead, sizeof(sampleData) / bytesPerFrame), sampleData);
    if (framesRead == 0) { break; }

    drwav__ieee_to_f32(pBufferOut, sampleData, (size_t)(framesRead * pWav->channels),
                       bytesPerFrame / pWav->channels);

    pBufferOut += framesRead * pWav->channels;
    framesToRead -= framesRead;
    totalFramesRead += framesRead;
  }

  return totalFramesRead;
}

DRWAV_PRIVATE uint64_t drwav_read_pcm_frames_f32__alaw(drwav* pWav, uint64_t framesToRead,
                                                       float* pBufferOut) {
  uint64_t totalFramesRead;
  uint8_t sampleData[4096];
  uint32_t bytesPerFrame = drwav_get_bytes_per_pcm_frame(pWav);

  if (bytesPerFrame == 0) { return 0; }

  totalFramesRead = 0;

  while (framesToRead > 0) {
    uint64_t framesRead = drwav_read_pcm_frames(
        pWav, drwav_min(framesToRead, sizeof(sampleData) / bytesPerFrame), sampleData);
    if (framesRead == 0) { break; }

    drwav_alaw_to_f32(pBufferOut, sampleData, (size_t)(framesRead * pWav->channels));

    pBufferOut += framesRead * pWav->channels;
    framesToRead -= framesRead;
    totalFramesRead += framesRead;
  }

  return totalFramesRead;
}

DRWAV_PRIVATE uint64_t drwav_read_pcm_frames_f32__mulaw(drwav* pWav, uint64_t framesToRead,
                                                        float* pBufferOut) {
  uint64_t totalFramesRead;
  uint8_t sampleData[4096];
  uint32_t bytesPerFrame = drwav_get_bytes_per_pcm_frame(pWav);

  if (bytesPerFrame == 0) { return 0; }

  totalFramesRead = 0;

  while (framesToRead > 0) {
    uint64_t framesRead = drwav_read_pcm_frames(
        pWav, drwav_min(framesToRead, sizeof(sampleData) / bytesPerFrame), sampleData);
    if (framesRead == 0) { break; }

    drwav_mulaw_to_f32(pBufferOut, sampleData, (size_t)(framesRead * pWav->channels));

    pBufferOut += framesRead * pWav->channels;
    framesToRead -= framesRead;
    totalFramesRead += framesRead;
  }

  return totalFramesRead;
}

DRWAV_API uint64_t drwav_read_pcm_frames_f32(drwav* pWav, uint64_t framesToRead,
                                             float* pBufferOut) {
  if (pWav == NULL || framesToRead == 0) { return 0; }

  if (pBufferOut == NULL) { return drwav_read_pcm_frames(pWav, framesToRead, NULL); }

  /* Don't try to read more samples than can potentially fit in the output
   * buffer. */
  if (framesToRead * pWav->channels * sizeof(float) > DRWAV_SIZE_MAX) {
    framesToRead = DRWAV_SIZE_MAX / sizeof(float) / pWav->channels;
  }

  if (pWav->translatedFormatTag == DR_WAVE_FORMAT_PCM) {
    return drwav_read_pcm_frames_f32__pcm(pWav, framesToRead, pBufferOut);
  }

  if (pWav->translatedFormatTag == DR_WAVE_FORMAT_ADPCM) {
    return drwav_read_pcm_frames_f32__msadpcm(pWav, framesToRead, pBufferOut);
  }

  if (pWav->translatedFormatTag == DR_WAVE_FORMAT_IEEE_FLOAT) {
    return drwav_read_pcm_frames_f32__ieee(pWav, framesToRead, pBufferOut);
  }

  if (pWav->translatedFormatTag == DR_WAVE_FORMAT_ALAW) {
    return drwav_read_pcm_frames_f32__alaw(pWav, framesToRead, pBufferOut);
  }

  if (pWav->translatedFormatTag == DR_WAVE_FORMAT_MULAW) {
    return drwav_read_pcm_frames_f32__mulaw(pWav, framesToRead, pBufferOut);
  }

  if (pWav->translatedFormatTag == DR_WAVE_FORMAT_DVI_ADPCM) {
    return drwav_read_pcm_frames_f32__ima(pWav, framesToRead, pBufferOut);
  }

  return 0;
}

DRWAV_API uint64_t drwav_read_pcm_frames_f32le(drwav* pWav, uint64_t framesToRead,
                                               float* pBufferOut) {
  uint64_t framesRead = drwav_read_pcm_frames_f32(pWav, framesToRead, pBufferOut);
  if (pBufferOut != NULL && drwav__is_little_endian() == false) {
    drwav__bswap_samples_f32(pBufferOut, framesRead * pWav->channels);
  }

  return framesRead;
}

DRWAV_API uint64_t drwav_read_pcm_frames_f32be(drwav* pWav, uint64_t framesToRead,
                                               float* pBufferOut) {
  uint64_t framesRead = drwav_read_pcm_frames_f32(pWav, framesToRead, pBufferOut);
  if (pBufferOut != NULL && drwav__is_little_endian() == true) {
    drwav__bswap_samples_f32(pBufferOut, framesRead * pWav->channels);
  }

  return framesRead;
}

DRWAV_API void drwav_u8_to_f32(float* pOut, const uint8_t* pIn, size_t sampleCount) {
  size_t i;

  if (pOut == NULL || pIn == NULL) { return; }

#ifdef DR_WAV_LIBSNDFILE_COMPAT
  /*
  It appears libsndfile uses slightly different logic for the u8 -> f32
  conversion to dr_wav, which in my opinion is incorrect. It appears libsndfile
  performs the conversion something like "f32 = (u8 / 256) * 2 - 1", however I
  think it should be "f32 = (u8 / 255) * 2 - 1" (note the divisor of 256 vs
  255). I use libsndfile as a benchmark for testing, so I'm therefore leaving
  this block here just for my automated correctness testing. This is disabled by
  default.
  */
  for (i = 0; i < sampleCount; ++i) { *pOut++ = (pIn[i] / 256.0f) * 2 - 1; }
#else
  for (i = 0; i < sampleCount; ++i) {
    float x = pIn[i];
    x = x * 0.00784313725490196078f; /* 0..255 to 0..2 */
    x = x - 1;                       /* 0..2 to -1..1 */

    *pOut++ = x;
  }
#endif
}

DRWAV_API void drwav_s16_to_f32(float* pOut, const int16_t* pIn, size_t sampleCount) {
  size_t i;

  if (pOut == NULL || pIn == NULL) { return; }

  for (i = 0; i < sampleCount; ++i) { *pOut++ = pIn[i] * 0.000030517578125f; }
}

DRWAV_API void drwav_s24_to_f32(float* pOut, const uint8_t* pIn, size_t sampleCount) {
  size_t i;

  if (pOut == NULL || pIn == NULL) { return; }

  for (i = 0; i < sampleCount; ++i) {
    double x;
    uint32_t a = ((uint32_t)(pIn[i * 3 + 0]) << 8);
    uint32_t b = ((uint32_t)(pIn[i * 3 + 1]) << 16);
    uint32_t c = ((uint32_t)(pIn[i * 3 + 2]) << 24);

    x = (double)((int32_t)(a | b | c) >> 8);
    *pOut++ = (float)(x * 0.00000011920928955078125);
  }
}

DRWAV_API void drwav_s32_to_f32(float* pOut, const int32_t* pIn, size_t sampleCount) {
  size_t i;
  if (pOut == NULL || pIn == NULL) { return; }

  for (i = 0; i < sampleCount; ++i) { *pOut++ = (float)(pIn[i] / 2147483648.0); }
}

DRWAV_API void drwav_f64_to_f32(float* pOut, const double* pIn, size_t sampleCount) {
  size_t i;

  if (pOut == NULL || pIn == NULL) { return; }

  for (i = 0; i < sampleCount; ++i) { *pOut++ = (float)pIn[i]; }
}

DRWAV_API void drwav_alaw_to_f32(float* pOut, const uint8_t* pIn, size_t sampleCount) {
  size_t i;

  if (pOut == NULL || pIn == NULL) { return; }

  for (i = 0; i < sampleCount; ++i) { *pOut++ = drwav__alaw_to_s16(pIn[i]) / 32768.0f; }
}

DRWAV_API void drwav_mulaw_to_f32(float* pOut, const uint8_t* pIn, size_t sampleCount) {
  size_t i;

  if (pOut == NULL || pIn == NULL) { return; }

  for (i = 0; i < sampleCount; ++i) { *pOut++ = drwav__mulaw_to_s16(pIn[i]) / 32768.0f; }
}

DRWAV_PRIVATE void drwav__pcm_to_s32(int32_t* pOut, const uint8_t* pIn, size_t totalSampleCount,
                                     unsigned int bytesPerSample) {
  unsigned int i;

  /* Special case for 8-bit sample data because it's treated as unsigned. */
  if (bytesPerSample == 1) {
    drwav_u8_to_s32(pOut, pIn, totalSampleCount);
    return;
  }

  /* Slightly more optimal implementation for common formats. */
  if (bytesPerSample == 2) {
    drwav_s16_to_s32(pOut, (const int16_t*)pIn, totalSampleCount);
    return;
  }
  if (bytesPerSample == 3) {
    drwav_s24_to_s32(pOut, pIn, totalSampleCount);
    return;
  }
  if (bytesPerSample == 4) {
    for (i = 0; i < totalSampleCount; ++i) { *pOut++ = ((const int32_t*)pIn)[i]; }
    return;
  }

  /* Anything more than 64 bits per sample is not supported. */
  if (bytesPerSample > 8) {
    DRWAV_ZERO_MEMORY(pOut, totalSampleCount * sizeof(*pOut));
    return;
  }

  /* Generic, slow converter. */
  for (i = 0; i < totalSampleCount; ++i) {
    uint64_t sample = 0;
    unsigned int shift = (8 - bytesPerSample) * 8;

    unsigned int j;
    for (j = 0; j < bytesPerSample; j += 1) {
      DRWAV_ASSERT(j < 8);
      sample |= (uint64_t)(pIn[j]) << shift;
      shift += 8;
    }

    pIn += j;
    *pOut++ = (int32_t)((int64_t)sample >> 32);
  }
}

DRWAV_PRIVATE void drwav__ieee_to_s32(int32_t* pOut, const uint8_t* pIn, size_t totalSampleCount,
                                      unsigned int bytesPerSample) {
  if (bytesPerSample == 4) {
    drwav_f32_to_s32(pOut, (const float*)pIn, totalSampleCount);
    return;
  } else if (bytesPerSample == 8) {
    drwav_f64_to_s32(pOut, (const double*)pIn, totalSampleCount);
    return;
  } else {
    /* Only supporting 32- and 64-bit float. Output silence in all other cases.
     * Contributions welcome for 16-bit float. */
    DRWAV_ZERO_MEMORY(pOut, totalSampleCount * sizeof(*pOut));
    return;
  }
}

DRWAV_PRIVATE uint64_t drwav_read_pcm_frames_s32__pcm(drwav* pWav, uint64_t framesToRead,
                                                      int32_t* pBufferOut) {
  uint64_t totalFramesRead;
  uint8_t sampleData[4096];
  uint32_t bytesPerFrame;

  /* Fast path. */
  if (pWav->translatedFormatTag == DR_WAVE_FORMAT_PCM && pWav->bitsPerSample == 32) {
    return drwav_read_pcm_frames(pWav, framesToRead, pBufferOut);
  }

  bytesPerFrame = drwav_get_bytes_per_pcm_frame(pWav);
  if (bytesPerFrame == 0) { return 0; }

  totalFramesRead = 0;

  while (framesToRead > 0) {
    uint64_t framesRead = drwav_read_pcm_frames(
        pWav, drwav_min(framesToRead, sizeof(sampleData) / bytesPerFrame), sampleData);
    if (framesRead == 0) { break; }

    drwav__pcm_to_s32(pBufferOut, sampleData, (size_t)(framesRead * pWav->channels),
                      bytesPerFrame / pWav->channels);

    pBufferOut += framesRead * pWav->channels;
    framesToRead -= framesRead;
    totalFramesRead += framesRead;
  }

  return totalFramesRead;
}

DRWAV_PRIVATE uint64_t drwav_read_pcm_frames_s32__msadpcm(drwav* pWav, uint64_t framesToRead,
                                                          int32_t* pBufferOut) {
  /*
  We're just going to borrow the implementation from the drwav_read_s16() since
  ADPCM is a little bit more complicated than other formats and I don't want to
  duplicate that code.
  */
  uint64_t totalFramesRead = 0;
  int16_t samples16[2048];

  while (framesToRead > 0) {
    uint64_t framesRead = drwav_read_pcm_frames_s16(
        pWav, drwav_min(framesToRead, drwav_countof(samples16) / pWav->channels), samples16);
    if (framesRead == 0) { break; }

    drwav_s16_to_s32(pBufferOut, samples16,
                     (size_t)(framesRead * pWav->channels)); /* <-- Safe cast because we're
                                                                clamping to 2048. */

    pBufferOut += framesRead * pWav->channels;
    framesToRead -= framesRead;
    totalFramesRead += framesRead;
  }

  return totalFramesRead;
}

DRWAV_PRIVATE uint64_t drwav_read_pcm_frames_s32__ima(drwav* pWav, uint64_t framesToRead,
                                                      int32_t* pBufferOut) {
  /*
  We're just going to borrow the implementation from the drwav_read_s16() since
  IMA-ADPCM is a little bit more complicated than other formats and I don't want
  to duplicate that code.
  */
  uint64_t totalFramesRead = 0;
  int16_t samples16[2048];

  while (framesToRead > 0) {
    uint64_t framesRead = drwav_read_pcm_frames_s16(
        pWav, drwav_min(framesToRead, drwav_countof(samples16) / pWav->channels), samples16);
    if (framesRead == 0) { break; }

    drwav_s16_to_s32(pBufferOut, samples16,
                     (size_t)(framesRead * pWav->channels)); /* <-- Safe cast because we're
                                                                clamping to 2048. */

    pBufferOut += framesRead * pWav->channels;
    framesToRead -= framesRead;
    totalFramesRead += framesRead;
  }

  return totalFramesRead;
}

DRWAV_PRIVATE uint64_t drwav_read_pcm_frames_s32__ieee(drwav* pWav, uint64_t framesToRead,
                                                       int32_t* pBufferOut) {
  uint64_t totalFramesRead;
  uint8_t sampleData[4096];
  uint32_t bytesPerFrame = drwav_get_bytes_per_pcm_frame(pWav);

  if (bytesPerFrame == 0) { return 0; }

  totalFramesRead = 0;

  while (framesToRead > 0) {
    uint64_t framesRead = drwav_read_pcm_frames(
        pWav, drwav_min(framesToRead, sizeof(sampleData) / bytesPerFrame), sampleData);
    if (framesRead == 0) { break; }

    drwav__ieee_to_s32(pBufferOut, sampleData, (size_t)(framesRead * pWav->channels),
                       bytesPerFrame / pWav->channels);

    pBufferOut += framesRead * pWav->channels;
    framesToRead -= framesRead;
    totalFramesRead += framesRead;
  }

  return totalFramesRead;
}

DRWAV_PRIVATE uint64_t drwav_read_pcm_frames_s32__alaw(drwav* pWav, uint64_t framesToRead,
                                                       int32_t* pBufferOut) {
  uint64_t totalFramesRead;
  uint8_t sampleData[4096];
  uint32_t bytesPerFrame = drwav_get_bytes_per_pcm_frame(pWav);

  if (bytesPerFrame == 0) { return 0; }

  totalFramesRead = 0;

  while (framesToRead > 0) {
    uint64_t framesRead = drwav_read_pcm_frames(
        pWav, drwav_min(framesToRead, sizeof(sampleData) / bytesPerFrame), sampleData);
    if (framesRead == 0) { break; }

    drwav_alaw_to_s32(pBufferOut, sampleData, (size_t)(framesRead * pWav->channels));

    pBufferOut += framesRead * pWav->channels;
    framesToRead -= framesRead;
    totalFramesRead += framesRead;
  }

  return totalFramesRead;
}

DRWAV_PRIVATE uint64_t drwav_read_pcm_frames_s32__mulaw(drwav* pWav, uint64_t framesToRead,
                                                        int32_t* pBufferOut) {
  uint64_t totalFramesRead;
  uint8_t sampleData[4096];
  uint32_t bytesPerFrame = drwav_get_bytes_per_pcm_frame(pWav);

  if (bytesPerFrame == 0) { return 0; }

  totalFramesRead = 0;

  while (framesToRead > 0) {
    uint64_t framesRead = drwav_read_pcm_frames(
        pWav, drwav_min(framesToRead, sizeof(sampleData) / bytesPerFrame), sampleData);
    if (framesRead == 0) { break; }

    drwav_mulaw_to_s32(pBufferOut, sampleData, (size_t)(framesRead * pWav->channels));

    pBufferOut += framesRead * pWav->channels;
    framesToRead -= framesRead;
    totalFramesRead += framesRead;
  }

  return totalFramesRead;
}

DRWAV_API uint64_t drwav_read_pcm_frames_s32(drwav* pWav, uint64_t framesToRead,
                                             int32_t* pBufferOut) {
  if (pWav == NULL || framesToRead == 0) { return 0; }

  if (pBufferOut == NULL) { return drwav_read_pcm_frames(pWav, framesToRead, NULL); }

  /* Don't try to read more samples than can potentially fit in the output
   * buffer. */
  if (framesToRead * pWav->channels * sizeof(int32_t) > DRWAV_SIZE_MAX) {
    framesToRead = DRWAV_SIZE_MAX / sizeof(int32_t) / pWav->channels;
  }

  if (pWav->translatedFormatTag == DR_WAVE_FORMAT_PCM) {
    return drwav_read_pcm_frames_s32__pcm(pWav, framesToRead, pBufferOut);
  }

  if (pWav->translatedFormatTag == DR_WAVE_FORMAT_ADPCM) {
    return drwav_read_pcm_frames_s32__msadpcm(pWav, framesToRead, pBufferOut);
  }

  if (pWav->translatedFormatTag == DR_WAVE_FORMAT_IEEE_FLOAT) {
    return drwav_read_pcm_frames_s32__ieee(pWav, framesToRead, pBufferOut);
  }

  if (pWav->translatedFormatTag == DR_WAVE_FORMAT_ALAW) {
    return drwav_read_pcm_frames_s32__alaw(pWav, framesToRead, pBufferOut);
  }

  if (pWav->translatedFormatTag == DR_WAVE_FORMAT_MULAW) {
    return drwav_read_pcm_frames_s32__mulaw(pWav, framesToRead, pBufferOut);
  }

  if (pWav->translatedFormatTag == DR_WAVE_FORMAT_DVI_ADPCM) {
    return drwav_read_pcm_frames_s32__ima(pWav, framesToRead, pBufferOut);
  }

  return 0;
}

DRWAV_API uint64_t drwav_read_pcm_frames_s32le(drwav* pWav, uint64_t framesToRead,
                                               int32_t* pBufferOut) {
  uint64_t framesRead = drwav_read_pcm_frames_s32(pWav, framesToRead, pBufferOut);
  if (pBufferOut != NULL && drwav__is_little_endian() == false) {
    drwav__bswap_samples_s32(pBufferOut, framesRead * pWav->channels);
  }

  return framesRead;
}

DRWAV_API uint64_t drwav_read_pcm_frames_s32be(drwav* pWav, uint64_t framesToRead,
                                               int32_t* pBufferOut) {
  uint64_t framesRead = drwav_read_pcm_frames_s32(pWav, framesToRead, pBufferOut);
  if (pBufferOut != NULL && drwav__is_little_endian() == true) {
    drwav__bswap_samples_s32(pBufferOut, framesRead * pWav->channels);
  }

  return framesRead;
}

DRWAV_API void drwav_u8_to_s32(int32_t* pOut, const uint8_t* pIn, size_t sampleCount) {
  size_t i;

  if (pOut == NULL || pIn == NULL) { return; }

  for (i = 0; i < sampleCount; ++i) { *pOut++ = ((int)pIn[i] - 128) << 24; }
}

DRWAV_API void drwav_s16_to_s32(int32_t* pOut, const int16_t* pIn, size_t sampleCount) {
  size_t i;

  if (pOut == NULL || pIn == NULL) { return; }

  for (i = 0; i < sampleCount; ++i) { *pOut++ = pIn[i] << 16; }
}

DRWAV_API void drwav_s24_to_s32(int32_t* pOut, const uint8_t* pIn, size_t sampleCount) {
  size_t i;

  if (pOut == NULL || pIn == NULL) { return; }

  for (i = 0; i < sampleCount; ++i) {
    unsigned int s0 = pIn[i * 3 + 0];
    unsigned int s1 = pIn[i * 3 + 1];
    unsigned int s2 = pIn[i * 3 + 2];

    int32_t sample32 = (int32_t)((s0 << 8) | (s1 << 16) | (s2 << 24));
    *pOut++ = sample32;
  }
}

DRWAV_API void drwav_f32_to_s32(int32_t* pOut, const float* pIn, size_t sampleCount) {
  size_t i;

  if (pOut == NULL || pIn == NULL) { return; }

  for (i = 0; i < sampleCount; ++i) { *pOut++ = (int32_t)(2147483648.0 * pIn[i]); }
}

DRWAV_API void drwav_f64_to_s32(int32_t* pOut, const double* pIn, size_t sampleCount) {
  size_t i;

  if (pOut == NULL || pIn == NULL) { return; }

  for (i = 0; i < sampleCount; ++i) { *pOut++ = (int32_t)(2147483648.0 * pIn[i]); }
}

DRWAV_API void drwav_alaw_to_s32(int32_t* pOut, const uint8_t* pIn, size_t sampleCount) {
  size_t i;

  if (pOut == NULL || pIn == NULL) { return; }

  for (i = 0; i < sampleCount; ++i) { *pOut++ = ((int32_t)drwav__alaw_to_s16(pIn[i])) << 16; }
}

DRWAV_API void drwav_mulaw_to_s32(int32_t* pOut, const uint8_t* pIn, size_t sampleCount) {
  size_t i;

  if (pOut == NULL || pIn == NULL) { return; }

  for (i = 0; i < sampleCount; ++i) { *pOut++ = ((int32_t)drwav__mulaw_to_s16(pIn[i])) << 16; }
}

DRWAV_PRIVATE int16_t* drwav__read_pcm_frames_and_close_s16(drwav* pWav, unsigned int* channels,
                                                            unsigned int* sampleRate,
                                                            uint64_t* totalFrameCount) {
  uint64_t sampleDataSize;
  int16_t* pSampleData;
  uint64_t framesRead;

  DRWAV_ASSERT(pWav != NULL);

  sampleDataSize = pWav->totalPCMFrameCount * pWav->channels * sizeof(int16_t);
  if (sampleDataSize > DRWAV_SIZE_MAX) {
    drwav_uninit(pWav);
    return NULL; /* File's too big. */
  }

  pSampleData = (int16_t*)drwav__malloc_from_callbacks(
      (size_t)sampleDataSize,
      &pWav->allocationCallbacks); /* <-- Safe cast due to the check above. */
  if (pSampleData == NULL) {
    drwav_uninit(pWav);
    return NULL; /* Failed to allocate memory. */
  }

  framesRead = drwav_read_pcm_frames_s16(pWav, (size_t)pWav->totalPCMFrameCount, pSampleData);
  if (framesRead != pWav->totalPCMFrameCount) {
    drwav__free_from_callbacks(pSampleData, &pWav->allocationCallbacks);
    drwav_uninit(pWav);
    return NULL; /* There was an error reading the samples. */
  }

  drwav_uninit(pWav);

  if (sampleRate) { *sampleRate = pWav->sampleRate; }
  if (channels) { *channels = pWav->channels; }
  if (totalFrameCount) { *totalFrameCount = pWav->totalPCMFrameCount; }

  return pSampleData;
}

DRWAV_PRIVATE float* drwav__read_pcm_frames_and_close_f32(drwav* pWav, unsigned int* channels,
                                                          unsigned int* sampleRate,
                                                          uint64_t* totalFrameCount) {
  uint64_t sampleDataSize;
  float* pSampleData;
  uint64_t framesRead;

  DRWAV_ASSERT(pWav != NULL);

  sampleDataSize = pWav->totalPCMFrameCount * pWav->channels * sizeof(float);
  if (sampleDataSize > DRWAV_SIZE_MAX) {
    drwav_uninit(pWav);
    return NULL; /* File's too big. */
  }

  pSampleData = (float*)drwav__malloc_from_callbacks(
      (size_t)sampleDataSize,
      &pWav->allocationCallbacks); /* <-- Safe cast due to the check above. */
  if (pSampleData == NULL) {
    drwav_uninit(pWav);
    return NULL; /* Failed to allocate memory. */
  }

  framesRead = drwav_read_pcm_frames_f32(pWav, (size_t)pWav->totalPCMFrameCount, pSampleData);
  if (framesRead != pWav->totalPCMFrameCount) {
    drwav__free_from_callbacks(pSampleData, &pWav->allocationCallbacks);
    drwav_uninit(pWav);
    return NULL; /* There was an error reading the samples. */
  }

  drwav_uninit(pWav);

  if (sampleRate) { *sampleRate = pWav->sampleRate; }
  if (channels) { *channels = pWav->channels; }
  if (totalFrameCount) { *totalFrameCount = pWav->totalPCMFrameCount; }

  return pSampleData;
}

DRWAV_PRIVATE int32_t* drwav__read_pcm_frames_and_close_s32(drwav* pWav, unsigned int* channels,
                                                            unsigned int* sampleRate,
                                                            uint64_t* totalFrameCount) {
  uint64_t sampleDataSize;
  int32_t* pSampleData;
  uint64_t framesRead;

  DRWAV_ASSERT(pWav != NULL);

  sampleDataSize = pWav->totalPCMFrameCount * pWav->channels * sizeof(int32_t);
  if (sampleDataSize > DRWAV_SIZE_MAX) {
    drwav_uninit(pWav);
    return NULL; /* File's too big. */
  }

  pSampleData = (int32_t*)drwav__malloc_from_callbacks(
      (size_t)sampleDataSize,
      &pWav->allocationCallbacks); /* <-- Safe cast due to the check above. */
  if (pSampleData == NULL) {
    drwav_uninit(pWav);
    return NULL; /* Failed to allocate memory. */
  }

  framesRead = drwav_read_pcm_frames_s32(pWav, (size_t)pWav->totalPCMFrameCount, pSampleData);
  if (framesRead != pWav->totalPCMFrameCount) {
    drwav__free_from_callbacks(pSampleData, &pWav->allocationCallbacks);
    drwav_uninit(pWav);
    return NULL; /* There was an error reading the samples. */
  }

  drwav_uninit(pWav);

  if (sampleRate) { *sampleRate = pWav->sampleRate; }
  if (channels) { *channels = pWav->channels; }
  if (totalFrameCount) { *totalFrameCount = pWav->totalPCMFrameCount; }

  return pSampleData;
}

DRWAV_API int16_t*
drwav_open_and_read_pcm_frames_s16(drwav_read_proc onRead, drwav_seek_proc onSeek, void* pUserData,
                                   unsigned int* channelsOut, unsigned int* sampleRateOut,
                                   uint64_t* totalFrameCountOut,
                                   const drwav_allocation_callbacks* pAllocationCallbacks) {
  if (channelsOut) { *channelsOut = 0; }
  if (sampleRateOut) { *sampleRateOut = 0; }
  if (totalFrameCountOut) { *totalFrameCountOut = 0; }

  drwav* wav = drwav_init(onRead, onSeek, pUserData, pAllocationCallbacks);
  if (!wav) {
    drwav_free(wav, pAllocationCallbacks);
    return NULL;
  }

  int16_t* ret = drwav__read_pcm_frames_and_close_s16(wav, channelsOut, sampleRateOut, totalFrameCountOut);
  drwav_free(wav, pAllocationCallbacks);
  return ret;
}

DRWAV_API float*
drwav_open_and_read_pcm_frames_f32(drwav_read_proc onRead, drwav_seek_proc onSeek, void* pUserData,
                                   unsigned int* channelsOut, unsigned int* sampleRateOut,
                                   uint64_t* totalFrameCountOut,
                                   const drwav_allocation_callbacks* pAllocationCallbacks) {
  if (channelsOut) { *channelsOut = 0; }
  if (sampleRateOut) { *sampleRateOut = 0; }
  if (totalFrameCountOut) { *totalFrameCountOut = 0; }

  drwav* wav = drwav_init(onRead, onSeek, pUserData, pAllocationCallbacks);
  if (!wav) {
    drwav_free(wav, pAllocationCallbacks);
    return NULL;
  }

  float* ret = drwav__read_pcm_frames_and_close_f32(wav, channelsOut, sampleRateOut, totalFrameCountOut);
  drwav_free(wav, pAllocationCallbacks);
  return ret;
}

DRWAV_API int32_t*
drwav_open_and_read_pcm_frames_s32(drwav_read_proc onRead, drwav_seek_proc onSeek, void* pUserData,
                                   unsigned int* channelsOut, unsigned int* sampleRateOut,
                                   uint64_t* totalFrameCountOut,
                                   const drwav_allocation_callbacks* pAllocationCallbacks) {

  if (channelsOut) { *channelsOut = 0; }
  if (sampleRateOut) { *sampleRateOut = 0; }
  if (totalFrameCountOut) { *totalFrameCountOut = 0; }

  drwav* wav = drwav_init(onRead, onSeek, pUserData, pAllocationCallbacks);
  if (!wav) {
    drwav_free(wav, pAllocationCallbacks);
    return NULL;
  }

  int32_t* ret = drwav__read_pcm_frames_and_close_s32(wav, channelsOut, sampleRateOut, totalFrameCountOut);
  drwav_free(wav, pAllocationCallbacks);
  return ret;
}

#ifndef DR_WAV_NO_STDIO
DRWAV_API int16_t*
drwav_open_file_and_read_pcm_frames_s16(const char* filename, unsigned int* channelsOut,
                                        unsigned int* sampleRateOut, uint64_t* totalFrameCountOut,
                                        const drwav_allocation_callbacks* pAllocationCallbacks) {

  if (channelsOut) { *channelsOut = 0; }
  if (sampleRateOut) { *sampleRateOut = 0; }
  if (totalFrameCountOut) { *totalFrameCountOut = 0; }

  drwav* wav = drwav_init_file(filename, pAllocationCallbacks);
  if (!wav) {
    drwav_free(wav, pAllocationCallbacks);
    return NULL;
  }

  int16_t* ret = drwav__read_pcm_frames_and_close_s16(wav, channelsOut, sampleRateOut, totalFrameCountOut);
  drwav_free(wav, pAllocationCallbacks);
  return ret;
}

DRWAV_API float*
drwav_open_file_and_read_pcm_frames_f32(const char* filename, unsigned int* channelsOut,
                                        unsigned int* sampleRateOut, uint64_t* totalFrameCountOut,
                                        const drwav_allocation_callbacks* pAllocationCallbacks) {
  if (channelsOut) { *channelsOut = 0; }
  if (sampleRateOut) { *sampleRateOut = 0; }
  if (totalFrameCountOut) { *totalFrameCountOut = 0; }

  drwav* wav = drwav_init_file(filename, pAllocationCallbacks);
  if (!wav) {
    drwav_free(wav, pAllocationCallbacks);
    return NULL;
  }

  float* ret = drwav__read_pcm_frames_and_close_f32(wav, channelsOut, sampleRateOut, totalFrameCountOut);
  drwav_free(wav, pAllocationCallbacks);
  return ret;
}

DRWAV_API int32_t*
drwav_open_file_and_read_pcm_frames_s32(const char* filename, unsigned int* channelsOut,
                                        unsigned int* sampleRateOut, uint64_t* totalFrameCountOut,
                                        const drwav_allocation_callbacks* pAllocationCallbacks) {
  if (channelsOut) { *channelsOut = 0; }
  if (sampleRateOut) { *sampleRateOut = 0; }
  if (totalFrameCountOut) { *totalFrameCountOut = 0; }

  drwav* wav = drwav_init_file(filename, pAllocationCallbacks);
  if (!wav) {
    drwav_free(wav, pAllocationCallbacks);
    return NULL;
  }

  int32_t* ret = drwav__read_pcm_frames_and_close_s32(wav, channelsOut, sampleRateOut, totalFrameCountOut);
  drwav_free(wav, pAllocationCallbacks);
  return ret;
}

DRWAV_API int16_t*
drwav_open_file_and_read_pcm_frames_s16_w(const wchar_t* filename, unsigned int* channelsOut,
                                          unsigned int* sampleRateOut, uint64_t* totalFrameCountOut,
                                          const drwav_allocation_callbacks* pAllocationCallbacks) {
  if (sampleRateOut) { *sampleRateOut = 0; }
  if (channelsOut) { *channelsOut = 0; }
  if (totalFrameCountOut) { *totalFrameCountOut = 0; }

  drwav* wav = drwav_init_file_w(filename, pAllocationCallbacks);
  if (!wav) {
    drwav_free(wav, pAllocationCallbacks);
    return NULL;
  }

  int16_t* ret =
      drwav__read_pcm_frames_and_close_s16(wav, channelsOut, sampleRateOut, totalFrameCountOut);
  drwav_free(wav, pAllocationCallbacks);
  return ret;
}

DRWAV_API float*
drwav_open_file_and_read_pcm_frames_f32_w(const wchar_t* filename, unsigned int* channelsOut,
                                          unsigned int* sampleRateOut, uint64_t* totalFrameCountOut,
                                          const drwav_allocation_callbacks* pAllocationCallbacks) {
  if (sampleRateOut) { *sampleRateOut = 0; }
  if (channelsOut) { *channelsOut = 0; }
  if (totalFrameCountOut) { *totalFrameCountOut = 0; }

  drwav* wav = drwav_init_file_w(filename, pAllocationCallbacks);
  if (!wav) {
    drwav_free(wav, pAllocationCallbacks);
    return NULL;
  }

  float* ret =
      drwav__read_pcm_frames_and_close_f32(wav, channelsOut, sampleRateOut, totalFrameCountOut);
  drwav_free(wav, pAllocationCallbacks);
  return ret;
}

DRWAV_API int32_t*
drwav_open_file_and_read_pcm_frames_s32_w(const wchar_t* filename, unsigned int* channelsOut,
                                          unsigned int* sampleRateOut, uint64_t* totalFrameCountOut,
                                          const drwav_allocation_callbacks* pAllocationCallbacks) {
  if (sampleRateOut) { *sampleRateOut = 0; }
  if (channelsOut) { *channelsOut = 0; }
  if (totalFrameCountOut) { *totalFrameCountOut = 0; }

  drwav* wav = drwav_init_file_w(filename, pAllocationCallbacks);

  if (!wav) { return NULL; }

  int32_t* ret =
      drwav__read_pcm_frames_and_close_s32(wav, channelsOut, sampleRateOut, totalFrameCountOut);
  drwav_free(wav, pAllocationCallbacks);

  return ret;
}
#endif

DRWAV_API int16_t* drwav_open_memory_and_read_pcm_frames_s16(
    const void* data, size_t dataSize, unsigned int* channelsOut, unsigned int* sampleRateOut,
    uint64_t* totalFrameCountOut, const drwav_allocation_callbacks* pAllocationCallbacks) {
  if (channelsOut) { *channelsOut = 0; }
  if (sampleRateOut) { *sampleRateOut = 0; }
  if (totalFrameCountOut) { *totalFrameCountOut = 0; }

  drwav* wav = drwav_init_memory(data, dataSize, pAllocationCallbacks);
  if (!wav) { return NULL; }

  int16_t* ret =
      drwav__read_pcm_frames_and_close_s16(wav, channelsOut, sampleRateOut, totalFrameCountOut);
  drwav_free(wav, pAllocationCallbacks);
  return ret;
}

DRWAV_API float* drwav_open_memory_and_read_pcm_frames_f32(
    const void* data, size_t dataSize, unsigned int* channelsOut, unsigned int* sampleRateOut,
    uint64_t* totalFrameCountOut, const drwav_allocation_callbacks* pAllocationCallbacks) {
  if (channelsOut) { *channelsOut = 0; }
  if (sampleRateOut) { *sampleRateOut = 0; }
  if (totalFrameCountOut) { *totalFrameCountOut = 0; }

  drwav* wav = drwav_init_memory(data, dataSize, pAllocationCallbacks);

  if (!wav) { return NULL; }

  float* ret =
      drwav__read_pcm_frames_and_close_f32(wav, channelsOut, sampleRateOut, totalFrameCountOut);
  drwav_free(wav, pAllocationCallbacks);
  return ret;
}

DRWAV_API int32_t* drwav_open_memory_and_read_pcm_frames_s32(
    const void* data, size_t dataSize, unsigned int* channelsOut, unsigned int* sampleRateOut,
    uint64_t* totalFrameCountOut, const drwav_allocation_callbacks* pAllocationCallbacks) {
  if (channelsOut) { *channelsOut = 0; }
  if (sampleRateOut) { *sampleRateOut = 0; }
  if (totalFrameCountOut) { *totalFrameCountOut = 0; }

  drwav* wav = drwav_init_memory(data, dataSize, pAllocationCallbacks);
  if (!wav) { return NULL; }

  int32_t* ret =
      drwav__read_pcm_frames_and_close_s32(wav, channelsOut, sampleRateOut, totalFrameCountOut);
  drwav_free(wav, pAllocationCallbacks);

  return ret;
}
#endif /* DR_WAV_NO_CONVERSION_API */

DRWAV_API void drwav_free(void* p, const drwav_allocation_callbacks* pAllocationCallbacks) {
  if (pAllocationCallbacks != NULL) {
    drwav__free_from_callbacks(p, pAllocationCallbacks);
  } else {
    drwav__free_default(p, NULL);
  }
}

DRWAV_API uint16_t drwav_bytes_to_u16(const uint8_t* data) {
  return ((uint16_t)data[0] << 0) | ((uint16_t)data[1] << 8);
}

DRWAV_API int16_t drwav_bytes_to_s16(const uint8_t* data) {
  return (int16_t)drwav_bytes_to_u16(data);
}

DRWAV_API uint32_t drwav_bytes_to_u32(const uint8_t* data) {
  return ((uint32_t)data[0] << 0) | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) |
         ((uint32_t)data[3] << 24);
}

DRWAV_API float drwav_bytes_to_f32(const uint8_t* data) {
  union {
    uint32_t u32;
    float f32;
  } value;

  value.u32 = drwav_bytes_to_u32(data);
  return value.f32;
}

DRWAV_API int32_t drwav_bytes_to_s32(const uint8_t* data) {
  return (int32_t)drwav_bytes_to_u32(data);
}

DRWAV_API uint64_t drwav_bytes_to_u64(const uint8_t* data) {
  return ((uint64_t)data[0] << 0) | ((uint64_t)data[1] << 8) | ((uint64_t)data[2] << 16) |
         ((uint64_t)data[3] << 24) | ((uint64_t)data[4] << 32) | ((uint64_t)data[5] << 40) |
         ((uint64_t)data[6] << 48) | ((uint64_t)data[7] << 56);
}

DRWAV_API int64_t drwav_bytes_to_s64(const uint8_t* data) {
  return (int64_t)drwav_bytes_to_u64(data);
}

DRWAV_API bool drwav_guid_equal(const uint8_t a[16], const uint8_t b[16]) {
  int i;
  for (i = 0; i < 16; i += 1) {
    if (a[i] != b[i]) { return false; }
  }

  return true;
}

DRWAV_API bool drwav_fourcc_equal(const uint8_t* a, const char* b) {
  return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3];
}
