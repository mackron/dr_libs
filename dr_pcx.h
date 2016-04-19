// Public domain. See "unlicense" statement at the end of this file.

// ABOUT
//
// dr_pcx is a simple library for loading PCX image files.
//
//
//
// USAGE
//
// dr_pcx is a single-file library. To use it, do something like the following in one .c file.
//   #define DR_PCX_IMPLEMENTATION
//   #include "dr_pcx.h"
//
// You can then #include this file in other parts of the program as you would with any other header file.
// 
//
//
// OPTIONS
// #define these options before including this file.
//
// #define DR_PCX_NO_STDIO
//   Disable drpcx_load_file().
//
//
//
// QUICK NOTES
// - 1- and 2-bit per pixel images are not properly supported.
//
//
// TODO
// - Fix decoding of 1- and 2-bit per pixel images.

#ifndef dr_pcx_h
#define dr_pcx_h

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Callback for when data is read. Return value is the number of bytes actually read.
typedef size_t (* drpcx_read_proc)(void* userData, void* bufferOut, size_t bytesToRead);

typedef struct
{
    // The width of the image.
    unsigned int width;

    // The height of the image.
    unsigned int height;

    // The number of color components. Will be 3 (RGB) or 4 (RGBA). Grayscale is converted to RGB.
    unsigned int components;

    // A pointer to the raw image data, tightly packed. Each component is always 8-bit.
    char pData[1];

} drpcx;


// Loads a PCX file using the given callbacks.
drpcx* drpcx_load(drpcx_read_proc onRead, void* pUserData, bool flipped);

// Deletes the given PCX file.
void drpcx_delete(drpcx* pPCX);


#ifndef DR_PCX_NO_STDIO
// Loads an PCX file from an actual file.
drpcx* drpcx_load_file(const char* pFile, bool flipped);
#endif

// Helper for loading an PCX file from a block of memory.
drpcx* drpcx_load_memory(const void* pData, size_t dataSize, bool flipped);


#ifdef __cplusplus
}
#endif

#endif  // dr_pcx_h


///////////////////////////////////////////////////////////////////////////////
//
// IMPLEMENTATION
//
///////////////////////////////////////////////////////////////////////////////
#ifdef DR_PCX_IMPLEMENTATION

#ifndef DR_PCX_NO_STDIO
#include <stdio.h>

static size_t drpcx__on_read_stdio(void* pUserData, void* bufferOut, size_t bytesToRead)
{
    return fread(bufferOut, 1, bytesToRead, (FILE*)pUserData);
}

drpcx* drpcx_load_file(const char* filename, bool flipped)
{
    FILE* pFile;
#ifdef _MSC_VER
    if (fopen_s(&pFile, filename, "rb") != 0) {
        return false;
    }
#else
    pFile = fopen(filename, "rb");
    if (pFile == NULL) {
        return false;
    }
#endif

    return drpcx_load(drpcx__on_read_stdio, pFile, flipped);
}
#endif  // DR_PCX_NO_STDIO


typedef struct
{
    // A pointer to the beginning of the data. We use a char as the type here for easy offsetting.
    const unsigned char* data;

    // The size of the data.
    size_t dataSize;

    // The position we're currently sitting at.
    size_t currentReadPos;

} drpcx_memory;

static size_t drpcx__on_read_memory(void* pUserData, void* bufferOut, size_t bytesToRead)
{
    drpcx_memory* memory = pUserData;
    assert(memory != NULL);
    assert(memory->dataSize >= memory->currentReadPos);

    size_t bytesRemaining = memory->dataSize - memory->currentReadPos;
    if (bytesToRead > bytesRemaining) {
        bytesToRead = bytesRemaining;
    }

    if (bytesToRead > 0) {
        memcpy(bufferOut, memory->data + memory->currentReadPos, bytesToRead);
        memory->currentReadPos += bytesToRead;
    }

    return bytesToRead;
}

drpcx* drpcx_load_memory(const void* data, size_t dataSize, bool flipped)
{
    drpcx_memory* pUserData = malloc(sizeof(*pUserData));
    if (pUserData == NULL) {
        return false;
    }

    pUserData->data = data;
    pUserData->dataSize = dataSize;
    pUserData->currentReadPos = 0;
    return drpcx_load(drpcx__on_read_memory, pUserData, flipped);
}


typedef struct
{
    drpcx* pPCX;
    drpcx_read_proc onRead;
    void* pUserData;
    bool flipped;

    uint8_t* palette16;
    uint32_t bitPlanes;
    uint32_t bytesPerLine;
    uint32_t stride;
} drpcx_decoder;

static uint8_t drpcx_read_byte(drpcx_decoder* pReader)
{
    uint8_t byte = 0;
    pReader->onRead(pReader->pUserData, &byte, 1);

    return byte;
}

bool drpcx__decode_1bit(drpcx_decoder* pDecoder)
{
    drpcx* pPCX = pDecoder->pPCX;
    uint32_t rleCount = 0;
    uint32_t rleValue = 0;
    uint32_t stride = pPCX->width * pPCX->components;

    if (pDecoder->bitPlanes == 1)
    {
        for (uint32_t y = 0; y < pPCX->height; ++y)
        {
            uint8_t* pRow = pPCX->pData;
            if (pDecoder->flipped) {
                pRow += (pPCX->height - y - 1) * stride;
            } else {
                pRow += y * stride;
            }

            for (uint32_t x = 0; x < pDecoder->bytesPerLine; ++x)
            {
                if (rleCount == 0) {
                    rleValue = drpcx_read_byte(pDecoder);
                    if ((rleValue & 0xC0) == 0xC0) {
                        rleCount = rleValue & 0x3F;
                        rleValue = drpcx_read_byte(pDecoder);
                    } else {
                        rleCount = 1;
                    }
                }

                rleCount -= 1;

                for (int bit = 0; bit < 8; ++bit)
                {
                    if (x*8 + bit < pPCX->width)
                    {
                        uint8_t mask = (1 << (7 - bit));
                        uint8_t paletteIndex = (rleValue & mask) >> (7 - bit);

                        pRow[0] = pDecoder->palette16[paletteIndex*3 + 0];
                        pRow[1] = pDecoder->palette16[paletteIndex*3 + 1];
                        pRow[2] = pDecoder->palette16[paletteIndex*3 + 2];
                        pRow += pPCX->components;
                    }
                }
            }
        }

        return true;
    }
    else
    {
        if (pDecoder->bitPlanes == 3 || pDecoder->bitPlanes == 4)
        {
            // NOTE: THIS IS WRONG.

            for (uint32_t y = 0; y < pPCX->height; ++y)
            {
                for (uint32_t component = 0; component < pPCX->components; ++component)
                {
                    uint8_t* pRow = pPCX->pData;
                    if (pDecoder->flipped) {
                        pRow += (pPCX->height - y - 1) * stride;
                    } else {
                        pRow += y * stride;
                    }

                    for (uint32_t x = 0; x < pDecoder->bytesPerLine; ++x)
                    {
                        if (rleCount == 0) {
                            rleValue = drpcx_read_byte(pDecoder);
                            if ((rleValue & 0xC0) == 0xC0) {
                                rleCount = rleValue & 0x3F;
                                rleValue = drpcx_read_byte(pDecoder);
                            } else {
                                rleCount = 1;
                            }
                        }

                        rleCount -= 1;

                        for (int bit = 0; bit < 8; ++bit)
                        {
                            if (x*8 + bit < pPCX->width)
                            {
                                uint8_t mask = (1 << (7 - bit));
                                uint8_t paletteIndex = (rleValue & mask) >> (7 - bit);

                                pRow[component] = pDecoder->palette16[paletteIndex*3 + component];
                                pRow += pPCX->components;
                            }
                        }
                    }
                }
            }

            return true;
        }
    }

    return false;
}

bool drpcx__decode_2bit(drpcx_decoder* pDecoder)
{
    drpcx* pPCX = pDecoder->pPCX;
    uint32_t rleCount = 0;
    uint32_t rleValue = 0;
    uint32_t stride = pPCX->width * pPCX->components;

    if (pDecoder->bitPlanes == 1)
    {
        // NOTE: THIS IS WRONG. I think the palette needs to be converted to CGA, but I'm not fully sure.

        for (uint32_t y = 0; y < pPCX->height; ++y)
        {
            uint8_t* pRow = pPCX->pData;
            if (pDecoder->flipped) {
                pRow += (pPCX->height - y - 1) * stride;
            } else {
                pRow += y * stride;
            }

            for (uint32_t x = 0; x < pDecoder->bytesPerLine; ++x)
            {
                if (rleCount == 0) {
                    rleValue = drpcx_read_byte(pDecoder);
                    if ((rleValue & 0xC0) == 0xC0) {
                        rleCount = rleValue & 0x3F;
                        rleValue = drpcx_read_byte(pDecoder);
                    } else {
                        rleCount = 1;
                    }
                }

                rleCount -= 1;

                for (int bit = 0; bit < 4; ++bit)
                {
                    if (x*4 + bit < pPCX->width)
                    {
                        uint8_t mask = (3 << ((3 - bit) * 2));
                        uint8_t paletteIndex = (rleValue & mask) >> ((3 - bit) * 2);

                        pRow[0] = pDecoder->palette16[paletteIndex*3 + 0];
                        pRow[1] = pDecoder->palette16[paletteIndex*3 + 1];
                        pRow[2] = pDecoder->palette16[paletteIndex*3 + 2];
                        pRow += 3;
                    }
                }
            }
        }

        return true;
    }

    return false;
}

bool drpcx__decode_4bit(drpcx_decoder* pDecoder)
{
    drpcx* pPCX = pDecoder->pPCX;
    uint32_t rleCount = 0;
    uint32_t rleValue = 0;
    uint32_t stride = pPCX->width * pPCX->components;

    // TODO: Implement me.
    return false;
}

bool drpcx__decode_8bit(drpcx_decoder* pDecoder)
{
    drpcx* pPCX = pDecoder->pPCX;
    uint32_t rleCount = 0;
    uint32_t rleValue = 0;
    uint32_t stride = pPCX->width * pPCX->components;

    if (pDecoder->bitPlanes == 3 || pDecoder->bitPlanes == 4)
    {
        for (uint32_t y = 0; y < pPCX->height; ++y)
        {
            for (uint32_t component = 0; component < pPCX->components; ++component)
            {
                uint8_t* pRow = pPCX->pData;
                if (pDecoder->flipped) {
                    pRow += (pPCX->height - y - 1) * stride;
                } else {
                    pRow += y * stride;
                }

                for (uint32_t x = 0; x < pDecoder->bytesPerLine; ++x)
                {
                    if (rleCount == 0) {
                        rleValue = drpcx_read_byte(pDecoder);
                        if ((rleValue & 0xC0) == 0xC0) {
                            rleCount = rleValue & 0x3F;
                            rleValue = drpcx_read_byte(pDecoder);
                        } else {
                            rleCount = 1;
                        }
                    }

                    rleCount -= 1;

                    if (x < pPCX->width) {
                        pRow[component] = rleValue;
                        pRow += pPCX->components;
                    }
                }
            }
        }
    }
    else
    {
        for (uint32_t y = 0; y < pPCX->height; ++y)
        {
            uint8_t* pRow = pPCX->pData;
            if (pDecoder->flipped) {
                pRow += (pPCX->height - y - 1) * stride;
            } else {
                pRow += y * stride;
            }

            for (uint32_t x = 0; x < pDecoder->bytesPerLine; ++x)
            {
                if (rleCount == 0) {
                    rleValue = drpcx_read_byte(pDecoder);
                    if ((rleValue & 0xC0) == 0xC0) {
                        rleCount = rleValue & 0x3F;
                        rleValue = drpcx_read_byte(pDecoder);
                    } else {
                        rleCount = 1;
                    }
                }

                rleCount -= 1;

                if (x < pPCX->width) {
                    pRow[0] = rleValue;
                    pRow[1] = rleValue;
                    pRow[2] = rleValue;
                    pRow += 3;
                }
            }
        }

        // At this point we can know if we are dealing with a palette or a grayscale image by checking the next byte. If ti's equal to 0x0C, we
        // need to do a simple palette lookup.
        uint8_t paletteMarker = drpcx_read_byte(pDecoder);
        if (paletteMarker == 0x0C)
        {
            // A palette is present - we need to do a second pass.
            uint8_t palette256[768];
            if (pDecoder->onRead(pDecoder->pUserData, palette256, sizeof(palette256)) != sizeof(palette256)) {
                free(pPCX);
                return false;
            }

            for (uint32_t y = 0; y < pPCX->height; ++y)
            {
                uint8_t* pRow = pPCX->pData + (y * stride);

                for (uint32_t x = 0; x < pPCX->width; ++x)
                {
                    uint8_t index = pRow[0];
                    pRow[0] = palette256[index*3 + 0];
                    pRow[1] = palette256[index*3 + 1];
                    pRow[2] = palette256[index*3 + 2];
                    pRow += 3;
                }
            }
        }
    }

    return true;
}

drpcx* drpcx_load(drpcx_read_proc onRead, void* pUserData, bool flipped)
{
    if (onRead == NULL) {
        return NULL;
    }

    // The first thing to do when loading is to find the dimensions and component count. Once we've done that we can figure
    // out how much memory to allocate.
    unsigned char header[128];
    if (onRead(pUserData, header, sizeof(header)) != sizeof(header)) {
        return NULL;    // Failed to read the header.
    }

    if (header[0] != 10) {
        return NULL;    // Not a PCX file.
    }

    //uint8_t version = header[1];    // Unused.

    uint8_t encoding = header[2];
    if (encoding != 1) {
        return NULL;    // Not supporting non-RLE encoding. Would assume a value of 0 indicates raw, unencoded, but that is apparently never used.
    }

    uint8_t bpp = header[3];
    if (bpp != 1 && bpp != 2 && bpp != 4 && bpp != 8) {
        return NULL;    // Unsupported bits per pixel.
    }

    uint16_t left   = (header[ 5] << 8) | (header[ 4] << 0);
    uint16_t top    = (header[ 7] << 8) | (header[ 6] << 0);
    uint16_t right  = (header[ 9] << 8) | (header[ 8] << 0);
    uint16_t bottom = (header[11] << 8) | (header[10] << 0);
    uint8_t* palette16 = header + 16;
    uint8_t bitPlanes = header[65];
    uint16_t bytesPerLine = (header[67] << 8) | (header[66] << 0);

    uint32_t components = (bitPlanes == 1/* || bpp == 1*/) ? 3 : bitPlanes;

    uint32_t width    = right - left + 1;
    uint32_t height   = bottom - top + 1;
    uint32_t dataSize = width * height * components;

    drpcx* pPCX = malloc(sizeof(*pPCX) - sizeof(pPCX->pData) + dataSize);
    if (pPCX == NULL) {
        return NULL;
    }

    pPCX->width      = width;
    pPCX->height     = height;
    pPCX->components = components;

    drpcx_decoder decoder;
    decoder.pPCX = pPCX;
    decoder.onRead = onRead;
    decoder.pUserData = pUserData;
    decoder.flipped = flipped;
    decoder.palette16 = palette16;
    decoder.bitPlanes = bitPlanes;
    decoder.bytesPerLine = bytesPerLine;

    switch (bpp)
    {
        case 1:
        {
            drpcx__decode_1bit(&decoder);
        } break;

        case 2:
        {
            drpcx__decode_2bit(&decoder);
        } break;

        case 4:
        {
            drpcx__decode_4bit(&decoder);
        } break;

        case 8:
        {
            drpcx__decode_8bit(&decoder);
        } break;
    }

    return pPCX;
}

void drpcx_delete(drpcx* pPCX)
{
    free(pPCX);
}

#endif // DR_PCX_IMPLEMENTATION
