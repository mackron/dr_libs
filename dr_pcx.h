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


// Loads a PCX file using the given callbacks.
uint8_t* drpcx_load(drpcx_read_proc onRead, void* pUserData, bool flipped, int* x, int* y, int* comp);

// Frees memory returned by drpcx_load() and family.
void drpcx_free(void* pReturnValueFromLoad);


#ifndef DR_PCX_NO_STDIO
// Loads an PCX file from an actual file.
uint8_t* drpcx_load_file(const char* filename, bool flipped, int* x, int* y, int* comp);
#endif

// Helper for loading an PCX file from a block of memory.
uint8_t* drpcx_load_memory(const void* data, size_t dataSize, bool flipped, int* x, int* y, int* comp);


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

uint8_t* drpcx_load_file(const char* filename, bool flipped, int* x, int* y, int* comp)
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

    return drpcx_load(drpcx__on_read_stdio, pFile, flipped, x, y, comp);
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
    drpcx_memory* memory = (drpcx_memory*)pUserData;
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

uint8_t* drpcx_load_memory(const void* data, size_t dataSize, bool flipped, int* x, int* y, int* comp)
{
    drpcx_memory memory;
    memory.data = (const unsigned char*)data;
    memory.dataSize = dataSize;
    memory.currentReadPos = 0;
    return drpcx_load(drpcx__on_read_memory, &memory, flipped, x, y, comp);
}


typedef struct
{
    uint8_t header;
    uint8_t version;
    uint8_t encoding;
    uint8_t bpp;
    uint16_t left;
    uint16_t top;
    uint16_t right;
    uint16_t bottom;
    uint16_t hres;
    uint16_t vres;
    uint8_t palette16[48];
    uint8_t reserved1;
    uint8_t bitPlanes;
    uint16_t bytesPerLine;
    uint16_t paletteType;
    uint16_t screenSizeH;
    uint16_t screenSizeV;
    uint8_t reserved2[54];
} drpcx_header;

typedef struct
{
    drpcx_read_proc onRead;
    void* pUserData;
    bool flipped;
    drpcx_header header;

    uint32_t width;
    uint32_t height;
    uint32_t components;    // 3 = RGB; 4 = RGBA. Only 3 and 4 are supported.
    uint8_t* pImageData;
} drpcx;


static uint8_t drpcx__read_byte(drpcx* pPCX)
{
    uint8_t byte = 0;
    pPCX->onRead(pPCX->pUserData, &byte, 1);

    return byte;
}

static uint8_t* drpcx__row_ptr(drpcx* pPCX, uint32_t row)
{
    uint32_t stride = pPCX->width * pPCX->components;

    uint8_t* pRow = pPCX->pImageData;
    if (pPCX->flipped) {
        pRow += (pPCX->height - row - 1) * stride;
    } else {
        pRow += row * stride;
    }

    return pRow;
}

static uint8_t drpcx__rle(drpcx* pPCX, uint8_t* pRLEValueOut)
{
    uint8_t rleCount;
    uint8_t rleValue;

    rleValue = drpcx__read_byte(pPCX);
    if ((rleValue & 0xC0) == 0xC0) {
        rleCount = rleValue & 0x3F;
        rleValue = drpcx__read_byte(pPCX);
    } else {
        rleCount = 1;
    }


    *pRLEValueOut = rleValue;
    return rleCount;
}


bool drpcx__decode_1bit(drpcx* pPCX)
{
    uint8_t rleCount = 0;
    uint8_t rleValue = 0;

    switch (pPCX->header.bitPlanes)
    {
        case 1:
        {
            for (uint32_t y = 0; y < pPCX->height; ++y)
            {
                uint8_t* pRow = drpcx__row_ptr(pPCX, y);
                for (uint32_t x = 0; x < pPCX->header.bytesPerLine; ++x)
                {
                    if (rleCount == 0) {
                        rleCount = drpcx__rle(pPCX, &rleValue);
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
                for (uint32_t c = 0; c < pPCX->header.bitPlanes; ++c)
                {
                    uint8_t* pRow = drpcx__row_ptr(pPCX, y);
                    for (uint32_t x = 0; x < pPCX->header.bytesPerLine; ++x)
                    {
                        if (rleCount == 0) {
                            rleCount = drpcx__rle(pPCX, &rleValue);
                        }
                        rleCount -= 1;

                        for (int bit = 0; (bit < 8) && ((x*8 + bit) < pPCX->width); ++bit)
                        {
                            uint8_t mask = (1 << (7 - bit));
                            uint8_t paletteIndex = (rleValue & mask) >> (7 - bit);

                            pRow[0] |= ((paletteIndex & 0x01) << c);
                            pRow += pPCX->components;
                        }
                    }
                }


                uint8_t* pRow = drpcx__row_ptr(pPCX, y);
                for (uint32_t x = 0; x < pPCX->width; ++x)
                {
                    uint8_t paletteIndex = pRow[0];
                    for (uint32_t c = 0; c < pPCX->components; ++c) {
                        pRow[c] = pPCX->header.palette16[paletteIndex*3 + c];
                    }
                    
                    pRow += pPCX->components;
                }
            }

            return true;
        }

        default: return false;
    }
}

bool drpcx__decode_2bit(drpcx* pPCX)
{
    uint8_t rleCount = 0;
    uint8_t rleValue = 0;

    switch (pPCX->header.bitPlanes)
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

            uint8_t cgaBGColor   = pPCX->header.palette16[0] >> 4;
            uint8_t i = (pPCX->header.palette16[3] & 0x20) >> 5;
            uint8_t p = (pPCX->header.palette16[3] & 0x40) >> 6;
            //uint8_t c = (pPCX->header.palette16[3] & 0x80) >> 7;    // Color or monochrome. How is monochrome handled?

            for (uint32_t y = 0; y < pPCX->height; ++y)
            {
                uint8_t* pRow = drpcx__row_ptr(pPCX, y);
                for (uint32_t x = 0; x < pPCX->header.bytesPerLine; ++x)
                {
                    if (rleCount == 0) {
                        rleCount = drpcx__rle(pPCX, &rleValue);
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
            if (pPCX->header.version == 5) {
                uint8_t paletteMarker = drpcx__read_byte(pPCX);
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
                for (uint32_t c = 0; c < pPCX->header.bitPlanes; ++c)
                {
                    uint8_t* pRow = drpcx__row_ptr(pPCX, y);
                    for (uint32_t x = 0; x < pPCX->header.bytesPerLine; ++x)
                    {
                        if (rleCount == 0) {
                            rleCount = drpcx__rle(pPCX, &rleValue);
                        }
                        rleCount -= 1;

                        for (int bitpair = 0; (bitpair < 4) && ((x*4 + bitpair) < pPCX->width); ++bitpair)
                        {
                            uint8_t mask = (4 << (3 - bitpair));
                            uint8_t paletteIndex = (rleValue & mask) >> (3 - bitpair);

                            pRow[0] |= ((paletteIndex & 0x03) << (c*2));
                            pRow += pPCX->components;
                        }
                    }
                }


                uint8_t* pRow = drpcx__row_ptr(pPCX, y);
                for (uint32_t x = 0; x < pPCX->width; ++x)
                {
                    uint8_t paletteIndex = pRow[0];
                    for (uint32_t c = 0; c < pPCX->header.bitPlanes; ++c) {
                        pRow[c] = pPCX->header.palette16[paletteIndex*3 + c];
                    }
                    
                    pRow += pPCX->components;
                }
            }

            return true;
        };

        default: return false;
    }
}

bool drpcx__decode_4bit(drpcx* pPCX)
{
    // NOTE: This is completely untested. If anybody knows where I can get a test file please let me know or send it through to me!
    // TODO: Test Me.

    if (pPCX->header.bitPlanes > 1) {
        return false;
    }

    uint8_t rleCount = 0;
    uint8_t rleValue = 0;

    for (uint32_t y = 0; y < pPCX->height; ++y)
    {
        for (uint32_t c = 0; c < pPCX->header.bitPlanes; ++c)
        {
            uint8_t* pRow = drpcx__row_ptr(pPCX, y);
            for (uint32_t x = 0; x < pPCX->header.bytesPerLine; ++x)
            {
                if (rleCount == 0) {
                    rleCount = drpcx__rle(pPCX, &rleValue);
                }
                rleCount -= 1;

                for (int nibble = 0; (nibble < 2) && ((x*2 + nibble) < pPCX->width); ++nibble)
                {
                    uint8_t mask = (4 << (1 - nibble));
                    uint8_t paletteIndex = (rleValue & mask) >> (1 - nibble);

                    pRow[0] |= ((paletteIndex & 0x0F) << (c*4)); 
                    pRow += pPCX->components;
                }
            }
        }


        uint8_t* pRow = drpcx__row_ptr(pPCX, y);
        for (uint32_t x = 0; x < pPCX->width; ++x)
        {
            uint8_t paletteIndex = pRow[0];
            for (uint32_t c = 0; c < pPCX->components; ++c) {
                pRow[c] = pPCX->header.palette16[paletteIndex*3 + c];
            }
                    
            pRow += pPCX->components;
        }
    }

    return true;
}

bool drpcx__decode_8bit(drpcx* pPCX)
{
    uint8_t rleCount = 0;
    uint8_t rleValue = 0;
    uint32_t stride = pPCX->width * pPCX->components;

    switch (pPCX->header.bitPlanes)
    {
        case 1:
        {
            for (uint32_t y = 0; y < pPCX->height; ++y)
            {
                uint8_t* pRow = drpcx__row_ptr(pPCX, y);
                for (uint32_t x = 0; x < pPCX->header.bytesPerLine; ++x)
                {
                    if (rleCount == 0) {
                        rleCount = drpcx__rle(pPCX, &rleValue);
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
            uint8_t paletteMarker = drpcx__read_byte(pPCX);
            if (paletteMarker == 0x0C)
            {
                // A palette is present - we need to do a second pass.
                uint8_t palette256[768];
                if (pPCX->onRead(pPCX->pUserData, palette256, sizeof(palette256)) != sizeof(palette256)) {
                    return false;
                }

                for (uint32_t y = 0; y < pPCX->height; ++y)
                {
                    uint8_t* pRow = pPCX->pImageData + (y * stride);
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
                for (uint32_t c = 0; c < pPCX->components; ++c)
                {
                    uint8_t* pRow = drpcx__row_ptr(pPCX, y);
                    for (uint32_t x = 0; x < pPCX->header.bytesPerLine; ++x)
                    {
                        if (rleCount == 0) {
                            rleCount = drpcx__rle(pPCX, &rleValue);
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

uint8_t* drpcx_load(drpcx_read_proc onRead, void* pUserData, bool flipped, int* x, int* y, int* comp)
{
    if (onRead == NULL) {
        return NULL;
    }

    drpcx pcx;
    pcx.onRead    = onRead;
    pcx.pUserData = pUserData;
    pcx.flipped   = flipped;
    if (onRead(pUserData, &pcx.header, sizeof(pcx.header)) != sizeof(pcx.header)) {
        return NULL;    // Failed to read the header.
    }

    if (pcx.header.header != 10) {
        return NULL;    // Not a PCX file.
    }

    if (pcx.header.encoding != 1) {
        return NULL;    // Not supporting non-RLE encoding. Would assume a value of 0 indicates raw, unencoded, but that is apparently never used.
    }

    if (pcx.header.bpp != 1 && pcx.header.bpp != 2 && pcx.header.bpp != 4 && pcx.header.bpp != 8) {
        return NULL;    // Unsupported pixel format.
    }


    pcx.width = pcx.header.right - pcx.header.left + 1;
    pcx.height = pcx.header.bottom - pcx.header.top + 1;
    pcx.components = (pcx.header.bpp == 8 && pcx.header.bitPlanes == 4) ? 4 : 3;
    size_t dataSize = pcx.width * pcx.height * pcx.components;
    pcx.pImageData = (uint8_t*)calloc(1, dataSize);   // <-- Clearing to zero is important! Required for proper decoding.
    if (pcx.pImageData == NULL) {
        return NULL;    // Failed to allocate memory.
    }

    bool result = false;
    switch (pcx.header.bpp)
    {
        case 1:
        {
            result = drpcx__decode_1bit(&pcx);
        } break;

        case 2:
        {
            result = drpcx__decode_2bit(&pcx);
        } break;

        case 4:
        {
            result = drpcx__decode_4bit(&pcx);
        } break;

        case 8:
        {
            result = drpcx__decode_8bit(&pcx);
        } break;
    }

    if (!result) {
        free(pcx.pImageData);
        return NULL;
    }

    if (x) *x = pcx.width;
    if (y) *y = pcx.height;
    if (comp) *comp = pcx.components;
    return pcx.pImageData;
}

void drpcx_free(void* pReturnValueFromLoad)
{
    free(pReturnValueFromLoad);
}

#endif // DR_PCX_IMPLEMENTATION
