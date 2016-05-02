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
// - 2-bpp/4-plane and 4-bpp/1-plane formats have not been tested.
//
//
//
// TODO
// - Test 2-bpp/4-plane and 4-bpp/1-plane formats.

#ifndef dr_pcx_h
#define dr_pcx_h

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

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
    uint8_t pData[1];

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
    uint32_t version;
} drpcx_decoder;

static uint8_t drpcx__read_byte(drpcx_decoder* pDecoder)
{
    uint8_t byte = 0;
    pDecoder->onRead(pDecoder->pUserData, &byte, 1);

    return byte;
}

static uint8_t* drpcx__row_ptr(drpcx_decoder* pDecoder, uint32_t row)
{
    uint32_t stride = pDecoder->pPCX->width * pDecoder->pPCX->components;

    uint8_t* pRow = pDecoder->pPCX->pData;
    if (pDecoder->flipped) {
        pRow += (pDecoder->pPCX->height - row - 1) * stride;
    } else {
        pRow += row * stride;
    }

    return pRow;
}

static uint8_t drpcx__rle(drpcx_decoder* pDecoder, uint8_t* pRLEValueOut)
{
    uint8_t rleCount;
    uint8_t rleValue;

    rleValue = drpcx__read_byte(pDecoder);
    if ((rleValue & 0xC0) == 0xC0) {
        rleCount = rleValue & 0x3F;
        rleValue = drpcx__read_byte(pDecoder);
    } else {
        rleCount = 1;
    }


    *pRLEValueOut = rleValue;
    return rleCount;
}


bool drpcx__decode_1bit(drpcx_decoder* pDecoder)
{
    drpcx* pPCX = pDecoder->pPCX;
    uint8_t rleCount = 0;
    uint8_t rleValue = 0;

    switch (pDecoder->bitPlanes)
    {
        case 1:
        {
            for (uint32_t y = 0; y < pPCX->height; ++y)
            {
                uint8_t* pRow = drpcx__row_ptr(pDecoder, y);
                for (uint32_t x = 0; x < pDecoder->bytesPerLine; ++x)
                {
                    if (rleCount == 0) {
                        rleCount = drpcx__rle(pDecoder, &rleValue);
                    }
                    rleCount -= 1;

                    for (int bit = 0; (bit < 8) && ((x*8 + bit) < pPCX->width); ++bit)
                    {
                        uint8_t mask = (1 << (7 - bit));
                        uint8_t paletteIndex = (rleValue & mask) >> (7 - bit);

                        pRow[0] = paletteIndex * 255;
                        pRow[1] = paletteIndex * 255;
                        pRow[2] = paletteIndex * 255;
                        pRow += 3;
                    }
                }
            }

            return true;

        } break;

        case 2:
        case 3:
        case 4:
        {
            for (uint32_t y = 0; y < pPCX->height; ++y)
            {
                for (uint32_t component = 0; component < pDecoder->bitPlanes; ++component)
                {
                    uint8_t* pRow = drpcx__row_ptr(pDecoder, y);
                    for (uint32_t x = 0; x < pDecoder->bytesPerLine; ++x)
                    {
                        if (rleCount == 0) {
                            rleCount = drpcx__rle(pDecoder, &rleValue);
                        }
                        rleCount -= 1;

                        for (int bit = 0; (bit < 8) && ((x*8 + bit) < pPCX->width); ++bit)
                        {
                            uint8_t mask = (1 << (7 - bit));
                            uint8_t paletteIndex = (rleValue & mask) >> (7 - bit);

                            pRow[component] = paletteIndex;
                            pRow += pPCX->components;
                        }
                    }
                }


                uint8_t* pRow = drpcx__row_ptr(pDecoder, y);
                for (uint32_t x = 0; x < pPCX->width; ++x)
                {
                    uint8_t paletteIndex = 0;
                    for (uint32_t c = 0; c < pDecoder->bitPlanes; ++c) {
                        paletteIndex |= ((pRow[c] & 0x01) << c);
                    }

                    for (uint32_t c = 0; c < pDecoder->bitPlanes; ++c) {
                        pRow[c] = pDecoder->palette16[paletteIndex*3 + c];
                    }
                    
                    pRow += pDecoder->bitPlanes;
                }
            }

            return true;
        }

        default: return false;
    }
}

bool drpcx__decode_2bit(drpcx_decoder* pDecoder)
{
    drpcx* pPCX = pDecoder->pPCX;
    uint8_t rleCount = 0;
    uint8_t rleValue = 0;

    switch (pDecoder->bitPlanes)
    {
        case 1:
        {
            uint8_t paletteCGA[48];
            paletteCGA[ 0] = 0x00; paletteCGA[ 1] = 0x00; paletteCGA[ 2] = 0x00;    // #000000
            paletteCGA[ 3] = 0x00; paletteCGA[ 4] = 0x00; paletteCGA[ 5] = 0xAA;    // #0000AA
            paletteCGA[ 6] = 0x00; paletteCGA[ 7] = 0xAA; paletteCGA[ 8] = 0x00;    // #00AA00
            paletteCGA[ 9] = 0x00; paletteCGA[10] = 0xAA; paletteCGA[11] = 0xAA;    // #00AAAA
            paletteCGA[12] = 0xAA; paletteCGA[13] = 0x00; paletteCGA[14] = 0x00;    // #AA0000
            paletteCGA[15] = 0xAA; paletteCGA[16] = 0x00; paletteCGA[17] = 0xAA;    // #AA00AA
            paletteCGA[18] = 0xAA; paletteCGA[19] = 0x55; paletteCGA[20] = 0x00;    // #AA5500
            paletteCGA[21] = 0xAA; paletteCGA[22] = 0xAA; paletteCGA[23] = 0xAA;    // #AAAAAA
            paletteCGA[24] = 0x55; paletteCGA[25] = 0x55; paletteCGA[26] = 0x55;    // #555555
            paletteCGA[27] = 0x55; paletteCGA[28] = 0x55; paletteCGA[29] = 0xFF;    // #5555FF
            paletteCGA[30] = 0x55; paletteCGA[31] = 0xFF; paletteCGA[32] = 0x55;    // #55FF55
            paletteCGA[33] = 0x55; paletteCGA[34] = 0xFF; paletteCGA[35] = 0xFF;    // #55FFFF
            paletteCGA[36] = 0xFF; paletteCGA[37] = 0x55; paletteCGA[38] = 0x55;    // #FF5555
            paletteCGA[39] = 0xFF; paletteCGA[40] = 0x55; paletteCGA[41] = 0xFF;    // #FF55FF
            paletteCGA[42] = 0xFF; paletteCGA[43] = 0xFF; paletteCGA[44] = 0x55;    // #FFFF55
            paletteCGA[45] = 0xFF; paletteCGA[46] = 0xFF; paletteCGA[47] = 0xFF;    // #FFFFFF

            uint8_t cgaBGColor   = pDecoder->palette16[0] >> 4;
            uint8_t i = (pDecoder->palette16[3] & 0x20) >> 5;
            uint8_t p = (pDecoder->palette16[3] & 0x40) >> 6;
            //uint8_t c = (pDecoder->palette16[3] & 0x80) >> 7;    // Color or monochrome. How is monochrome handled?

            for (uint32_t y = 0; y < pPCX->height; ++y)
            {
                uint8_t* pRow = drpcx__row_ptr(pDecoder, y);
                for (uint32_t x = 0; x < pDecoder->bytesPerLine; ++x)
                {
                    if (rleCount == 0) {
                        rleCount = drpcx__rle(pDecoder, &rleValue);
                    }
                    rleCount -= 1;

                    for (int bit = 0; bit < 4; ++bit)
                    {
                        if (x*4 + bit < pPCX->width)
                        {
                            uint8_t mask = (3 << ((3 - bit) * 2));
                            uint8_t paletteIndex = (rleValue & mask) >> ((3 - bit) * 2);

                            uint8_t cgaIndex;
                            if (paletteIndex == 0) {    // Background.
                                cgaIndex = cgaBGColor;
                            } else {                    // Foreground
                                cgaIndex = (((paletteIndex << 1) + p) + (i << 3));
                            }

                            pRow[0] = paletteCGA[cgaIndex*3 + 0];
                            pRow[1] = paletteCGA[cgaIndex*3 + 1];
                            pRow[2] = paletteCGA[cgaIndex*3 + 2];
                            pRow += 3;
                        }
                    }
                }
            }

            // TODO: According to http://www.fysnet.net/pcxfile.htm, we should use the palette at the end of the file
            //       instead of the standard CGA palette if the version is equal to 5. With my test files the palette
            //       at the end of the file does not exist. Research this one.
            if (pDecoder->version == 5) {
                uint8_t paletteMarker = drpcx__read_byte(pDecoder);
                if (paletteMarker == 0x0C) {
                    // TODO: Implement Me.
                }
            }
            
            return true;
        };

        case 4:
        {
            // NOTE: This is completely untested. If anybody knows where I can get a test file please let me know or send it through to me!
            // TODO: Test Me.

            for (uint32_t y = 0; y < pPCX->height; ++y)
            {
                for (uint32_t c = 0; c < pDecoder->bitPlanes; ++c)
                {
                    uint8_t* pRow = drpcx__row_ptr(pDecoder, y);
                    for (uint32_t x = 0; x < pDecoder->bytesPerLine; ++x)
                    {
                        if (rleCount == 0) {
                            rleCount = drpcx__rle(pDecoder, &rleValue);
                        }
                        rleCount -= 1;

                        for (int bitpair = 0; (bitpair < 4) && ((x*4 + bitpair) < pPCX->width); ++bitpair)
                        {
                            uint8_t mask = (4 << (3 - bitpair));
                            uint8_t paletteIndex = (rleValue & mask) >> (3 - bitpair);

                            pRow[c] = paletteIndex;
                            pRow += pDecoder->bitPlanes;
                        }
                    }
                }


                uint8_t* pRow = drpcx__row_ptr(pDecoder, y);
                for (uint32_t x = 0; x < pPCX->width; ++x)
                {
                    uint8_t paletteIndex = 0;
                    for (uint32_t c = 0; c < pDecoder->bitPlanes; ++c) {
                        paletteIndex |= ((pRow[c] & 0x03) << c);
                    }

                    for (uint32_t c = 0; c < pDecoder->bitPlanes; ++c) {
                        pRow[c] = pDecoder->palette16[paletteIndex*3 + c];
                    }
                    
                    pRow += pDecoder->bitPlanes;
                }
            }

            return true;
        };

        default: return false;
    }
}

bool drpcx__decode_4bit(drpcx_decoder* pDecoder)
{
    // NOTE: This is completely untested. If anybody knows where I can get a test file please let me know or send it through to me!
    // TODO: Test Me.

    if (pDecoder->bitPlanes > 1) {
        return false;
    }

    drpcx* pPCX = pDecoder->pPCX;
    uint8_t rleCount = 0;
    uint8_t rleValue = 0;

    for (uint32_t y = 0; y < pPCX->height; ++y)
    {
        for (uint32_t c = 0; c < pDecoder->bitPlanes; ++c)
        {
            uint8_t* pRow = drpcx__row_ptr(pDecoder, y);
            for (uint32_t x = 0; x < pDecoder->bytesPerLine; ++x)
            {
                if (rleCount == 0) {
                    rleCount = drpcx__rle(pDecoder, &rleValue);
                }
                rleCount -= 1;

                for (int nibble = 0; (nibble < 2) && ((x*2 + nibble) < pPCX->width); ++nibble)
                {
                    uint8_t mask = (4 << (1 - nibble));
                    uint8_t paletteIndex = (rleValue & mask) >> (1 - nibble);

                    pRow[c] = paletteIndex;
                    pRow += pDecoder->bitPlanes;
                }
            }
        }


        uint8_t* pRow = drpcx__row_ptr(pDecoder, y);
        for (uint32_t x = 0; x < pPCX->width; ++x)
        {
            uint8_t paletteIndex = 0;
            for (uint32_t c = 0; c < pDecoder->bitPlanes; ++c) {
                paletteIndex |= ((pRow[c] & 0x0F) << c);
            }

            for (uint32_t c = 0; c < pDecoder->bitPlanes; ++c) {
                pRow[c] = pDecoder->palette16[paletteIndex*3 + c];
            }
                    
            pRow += pDecoder->bitPlanes;
        }
    }

    return true;
}

bool drpcx__decode_8bit(drpcx_decoder* pDecoder)
{
    drpcx* pPCX = pDecoder->pPCX;
    uint8_t rleCount = 0;
    uint8_t rleValue = 0;
    uint32_t stride = pPCX->width * pPCX->components;

    switch (pDecoder->bitPlanes)
    {
        case 1:
        {
            for (uint32_t y = 0; y < pPCX->height; ++y)
            {
                uint8_t* pRow = drpcx__row_ptr(pDecoder, y);
                for (uint32_t x = 0; x < pDecoder->bytesPerLine; ++x)
                {
                    if (rleCount == 0) {
                        rleCount = drpcx__rle(pDecoder, &rleValue);
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
            uint8_t paletteMarker = drpcx__read_byte(pDecoder);
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

            return true;
        }

        case 3:
        case 4:
        {
            for (uint32_t y = 0; y < pPCX->height; ++y)
            {
                for (uint32_t c = 0; c < pDecoder->bitPlanes; ++c)
                {
                    uint8_t* pRow = drpcx__row_ptr(pDecoder, y);
                    for (uint32_t x = 0; x < pDecoder->bytesPerLine; ++x)
                    {
                        if (rleCount == 0) {
                            rleCount = drpcx__rle(pDecoder, &rleValue);
                        }
                        rleCount -= 1;

                        if (x < pPCX->width) {
                            pRow[c] = rleValue;
                            pRow += pPCX->components;
                        }
                    }
                }
            }

            return true;
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

    uint8_t version = header[1];

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

    uint32_t components = (bpp == 8 && bitPlanes == 4) ? 4 : 3;

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
    decoder.version = version;

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
