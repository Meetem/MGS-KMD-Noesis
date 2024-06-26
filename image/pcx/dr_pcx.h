#pragma once
#pragma once
// PCX image loader. Public domain. See "unlicense" statement at the end of this file.
// dr_pcx - v0.3.1 - 2018-09-11
//
// David Reid - mackron@gmail.com

// USAGE
//
// dr_pcx is a single-file library. To use it, do something like the following in one .c file.
//     #define DR_PCX_IMPLEMENTATION
//     #include "dr_pcx.h"
//
// You can then #include this file in other parts of the program as you would with any other header file. Do something like
// the following to load and decode an image:
//
//     int width;
//     int height;
//     int components
//     drpcx_uint8* pImageData = drpcx_load_file("my_image.pcx", DRPCX_FALSE, &width, &height, &components, 0);
//     if (pImageData == NULL) {
//         // Failed to load image.
//     }
//
//     ...
//
//     drpcx_free(pImageData);
//
// The boolean parameter (second argument in the above example) is whether or not the image should be flipped upside down.
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

#ifndef dr_pcx_h
#define dr_pcx_h

#include <stddef.h>

#if defined(_MSC_VER) && _MSC_VER < 1600
typedef   signed char    drpcx_int8;
typedef unsigned char    drpcx_uint8;
typedef   signed short   drpcx_int16;
typedef unsigned short   drpcx_uint16;
typedef   signed int     drpcx_int32;
typedef unsigned int     drpcx_uint32;
typedef   signed __int64 drpcx_int64;
typedef unsigned __int64 drpcx_uint64;
#else
#include <stdint.h>
typedef int8_t           drpcx_int8;
typedef uint8_t          drpcx_uint8;
typedef int16_t          drpcx_int16;
typedef uint16_t         drpcx_uint16;
typedef int32_t          drpcx_int32;
typedef uint32_t         drpcx_uint32;
typedef int64_t          drpcx_int64;
typedef uint64_t         drpcx_uint64;
#endif
typedef drpcx_uint8      drpcx_bool8;
typedef drpcx_uint32     drpcx_bool32;
#define DRPCX_TRUE       1
#define DRPCX_FALSE      0

#ifdef __cplusplus
extern "C" {
#endif

    // Callback for when data is read. Return value is the number of bytes actually read.
    typedef size_t(*drpcx_read_proc)(void* userData, void* bufferOut, size_t bytesToRead);

    typedef struct
    {
        drpcx_uint8 header;
        drpcx_uint8 version;
        drpcx_uint8 encoding;
        drpcx_uint8 bpp;
        drpcx_uint16 left;
        drpcx_uint16 top;
        drpcx_uint16 right;
        drpcx_uint16 bottom;
        drpcx_uint16 hres;
        drpcx_uint16 vres;
        drpcx_uint8 palette16[48];
        drpcx_uint8 reserved1;
        drpcx_uint8 bitPlanes;
        drpcx_uint16 bytesPerLine;
        drpcx_uint16 paletteType;
        drpcx_uint16 screenSizeH;
        drpcx_uint16 screenSizeV;
        drpcx_uint16 mgsMagic; //1234 always
        drpcx_uint16 flags;
        drpcx_uint16 px, py; //pixels
        drpcx_uint16 cx, cy; //clut
        drpcx_uint16 numColors;
        drpcx_uint8 reserved2[40];
    } drpcx_header;

    typedef struct
    {
        drpcx_read_proc onRead;
        void* pUserData;
        drpcx_bool32 flipped;
        drpcx_header header;

        drpcx_uint32 width;
        drpcx_uint32 height;
        drpcx_uint32 components;    // 3 = RGB; 4 = RGBA. Only 3 and 4 are supported.
        drpcx_uint8* pImageData;
        drpcx_uint8* pPaletteIndices;
        drpcx_uint8 loaded;
    } drpcx;

    // Loads a PCX file using the given callbacks.
    int drpcx_load(drpcx *result, drpcx_read_proc onRead, const char *name, void* pUserData, drpcx_bool32 flipped, int* x, int* y, int* internalComponents, int desiredComponents);

    // Frees memory returned by drpcx_load() and family.
    void drpcx_free(void* pReturnValueFromLoad);


#ifndef DR_PCX_NO_STDIO
    // Loads an PCX file from an actual file.
    int drpcx_load_file(drpcx* result, const char* filename, drpcx_bool32 flipped, int* x, int* y, int* internalComponents, int desiredComponents);
#endif

    // Helper for loading an PCX file from a block of memory.
    int drpcx_load_memory(drpcx* result, const void* data, size_t dataSize, drpcx_bool32 flipped, int* x, int* y, int* internalComponents, int desiredComponents);


#ifdef __cplusplus
}
#endif

#endif  // dr_pcx_h


///////////////////////////////////////////////////////////////////////////////
//
// IMPLEMENTATION
//
///////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifndef DR_PCX_NO_STDIO
#include <stdio.h>

static size_t drpcx__on_read_stdio(void* pUserData, void* bufferOut, size_t bytesToRead)
{
    return fread(bufferOut, 1, bytesToRead, (FILE*)pUserData);
}

int drpcx_load_file(drpcx *result, const char* filename, drpcx_bool32 flipped, int* x, int* y, int* internalComponents, int desiredComponents)
{
    FILE* pFile;
#ifdef _MSC_VER
    if (fopen_s(&pFile, filename, "rb") != 0) {
        return -16;
    }
#else
    pFile = fopen(filename, "rb");
    if (pFile == NULL) {
        return -16;
    }
#endif

    auto ret = drpcx_load(result, drpcx__on_read_stdio, filename, pFile, flipped, x, y, internalComponents, desiredComponents);

    fclose(pFile);
    return ret;
}
#endif  // DR_PCX_NO_STDIO


typedef struct
{
    // A pointer to the beginning of the data. We use a char as the type here for easy offsetting.
    const unsigned char* data;
    size_t dataSize;
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

int drpcx_load_memory(drpcx *result, const void* data, size_t dataSize, drpcx_bool32 flipped, int* x, int* y, int* internalComponents, int desiredComponents)
{
    drpcx_memory memory;
    memory.data = (const unsigned char*)data;
    memory.dataSize = dataSize;
    memory.currentReadPos = 0;
    return drpcx_load(result, drpcx__on_read_memory, "inMemory", &memory, flipped, x, y, internalComponents, desiredComponents);
}

static drpcx_uint8 drpcx__read_byte(drpcx* pPCX)
{
    drpcx_uint8 byte = 0;
    pPCX->onRead(pPCX->pUserData, &byte, 1);

    return byte;
}

static drpcx_uint8* drpcx__indices_rowptr(drpcx* pPCX, drpcx_uint32 row)
{
    drpcx_uint32 stride = pPCX->width;

    drpcx_uint8* pRow = pPCX->pPaletteIndices;
    if (pPCX->flipped) {
        pRow += (pPCX->height - row - 1) * stride;
    }
    else {
        pRow += row * stride;
    }

    return pRow;
}

static drpcx_uint8* drpcx__row_ptr(drpcx* pPCX, drpcx_uint32 row)
{
    drpcx_uint32 stride = pPCX->width * pPCX->components;

    drpcx_uint8* pRow = pPCX->pImageData;
    if (pPCX->flipped) {
        pRow += (pPCX->height - row - 1) * stride;
    }
    else {
        pRow += row * stride;
    }

    return pRow;
}

static drpcx_uint8 drpcx__rle(drpcx* pPCX, drpcx_uint8* pRLEValueOut)
{
    drpcx_uint8 rleCount;
    drpcx_uint8 rleValue;

    rleValue = drpcx__read_byte(pPCX);
    if ((rleValue & 0xC0) == 0xC0) {
        rleCount = rleValue & 0x3F;
        rleValue = drpcx__read_byte(pPCX);
    }
    else {
        rleCount = 1;
    }


    *pRLEValueOut = rleValue;
    return rleCount;
}


drpcx_bool32 drpcx__decode_1bit(drpcx* pPCX)
{
    drpcx_uint8 rleCount = 0;
    drpcx_uint8 rleValue = 0;
    int idx = 0;

    switch (pPCX->header.bitPlanes)
    {
    case 1:
    {
        for (drpcx_uint32 y = 0; y < pPCX->height; ++y) {
            drpcx_uint8* pRow = drpcx__row_ptr(pPCX, y);
            drpcx_uint8* pPalette = drpcx__indices_rowptr(pPCX, y);

            for (drpcx_uint32 x = 0; x < pPCX->header.bytesPerLine; ++x) {
                if (rleCount == 0) {
                    rleCount = drpcx__rle(pPCX, &rleValue);
                }
                rleCount -= 1;

                for (int bit = 0; (bit < 8) && ((x * 8 + bit) < pPCX->width); ++bit) {
                    drpcx_uint8 mask = (1 << (7 - bit));
                    drpcx_uint8 paletteIndex = (rleValue & mask) >> (7 - bit);

                    pRow[0] = paletteIndex * 255;
                    pRow[1] = paletteIndex * 255;
                    pRow[2] = paletteIndex * 255;

                    *pPalette++ = paletteIndex * 255;
                    pRow += 3;
                }
            }
        }

        return DRPCX_TRUE;

    } break;

    case 2:
    case 3:
    case 4:
    {
        for (drpcx_uint32 y = 0; y < pPCX->height; ++y) {
            for (drpcx_uint32 c = 0; c < pPCX->header.bitPlanes; ++c) {
                drpcx_uint8* pRow = drpcx__row_ptr(pPCX, y);
                drpcx_uint8* pPalette = drpcx__indices_rowptr(pPCX, y);
                
                for (drpcx_uint32 x = 0; x < pPCX->header.bytesPerLine; ++x) {
                    if (rleCount == 0) {
                        rleCount = drpcx__rle(pPCX, &rleValue);
                    }
                    rleCount -= 1;

                    for (int bit = 0; (bit < 8) && ((x * 8 + bit) < pPCX->width); ++bit) {
                        drpcx_uint8 mask = (1 << (7 - bit));
                        drpcx_uint8 paletteIndex = (rleValue & mask) >> (7 - bit);

                        pRow[0] |= ((paletteIndex & 0x01) << c);
                        *pPalette++ = pRow[0];

                        pRow += pPCX->components;
                    }
                }
            }

            drpcx_uint8* pRow = drpcx__row_ptr(pPCX, y);
            for (drpcx_uint32 x = 0; x < pPCX->width; ++x) {
                drpcx_uint8 paletteIndex = pRow[0];

                auto components = pPCX->components > 3 ? 3 : pPCX->components;

                for (drpcx_uint32 c = 0; c < components; ++c) {
                    pRow[c] = pPCX->header.palette16[paletteIndex * 3 + c];
                }

                pRow += pPCX->components;
            }
        }

        return DRPCX_TRUE;
    }

    default: return DRPCX_FALSE;
    }
}

drpcx_bool32 drpcx__decode_2bit(drpcx* pPCX)
{
    drpcx_uint8 rleCount = 0;
    drpcx_uint8 rleValue = 0;

    switch (pPCX->header.bitPlanes)
    {
    case 1:
    {
        drpcx_uint8 paletteCGA[48];
        paletteCGA[0] = 0x00; paletteCGA[1] = 0x00; paletteCGA[2] = 0x00;    // #000000
        paletteCGA[3] = 0x00; paletteCGA[4] = 0x00; paletteCGA[5] = 0xAA;    // #0000AA
        paletteCGA[6] = 0x00; paletteCGA[7] = 0xAA; paletteCGA[8] = 0x00;    // #00AA00
        paletteCGA[9] = 0x00; paletteCGA[10] = 0xAA; paletteCGA[11] = 0xAA;    // #00AAAA
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

        drpcx_uint8 cgaBGColor = pPCX->header.palette16[0] >> 4;
        drpcx_uint8 i = (pPCX->header.palette16[3] & 0x20) >> 5;
        drpcx_uint8 p = (pPCX->header.palette16[3] & 0x40) >> 6;
        //drpcx_uint8 c = (pPCX->header.palette16[3] & 0x80) >> 7;    // Color or monochrome. How is monochrome handled?

        for (drpcx_uint32 y = 0; y < pPCX->height; ++y) {
            drpcx_uint8* pRow = drpcx__row_ptr(pPCX, y);
            drpcx_uint8* pPalette = drpcx__indices_rowptr(pPCX, y);

            for (drpcx_uint32 x = 0; x < pPCX->header.bytesPerLine; ++x) {
                if (rleCount == 0) {
                    rleCount = drpcx__rle(pPCX, &rleValue);
                }
                rleCount -= 1;

                for (int bit = 0; bit < 4; ++bit) {
                    if (x * 4 + bit < pPCX->width) {
                        drpcx_uint8 mask = (3 << ((3 - bit) * 2));
                        drpcx_uint8 paletteIndex = (rleValue & mask) >> ((3 - bit) * 2);

                        drpcx_uint8 cgaIndex;
                        if (paletteIndex == 0) {    // Background.
                            cgaIndex = cgaBGColor;
                        }
                        else {                    // Foreground
                            cgaIndex = (((paletteIndex << 1) + p) + (i << 3));
                        }

                        *pPalette++ = paletteIndex;
                        pRow[0] = paletteCGA[cgaIndex * 3 + 0];
                        pRow[1] = paletteCGA[cgaIndex * 3 + 1];
                        pRow[2] = paletteCGA[cgaIndex * 3 + 2];
                        pRow += 3;
                    }
                }
            }
        }

        // TODO: According to http://www.fysnet.net/pcxfile.htm, we should use the palette at the end of the file
        //       instead of the standard CGA palette if the version is equal to 5. With my test files the palette
        //       at the end of the file does not exist. Research this one.
        if (pPCX->header.version == 5) {
            drpcx_uint8 paletteMarker = drpcx__read_byte(pPCX);
            if (paletteMarker == 0x0C) {
                MessageBoxA(NULL, "Found the image with 2bit palette and at the end", "Blast from the past", 0);
                // TODO: Implement Me.
            }
        }

        return DRPCX_TRUE;
    };

    case 4:
    {
        // NOTE: This is completely untested. If anybody knows where I can get a test file please let me know or send it through to me!
        // TODO: Test Me.
        MessageBoxA(NULL, "2bit + 4planes", "Blast from the past", 0);

        for (drpcx_uint32 y = 0; y < pPCX->height; ++y) {
            for (drpcx_uint32 c = 0; c < pPCX->header.bitPlanes; ++c) {
                drpcx_uint8* pRow = drpcx__row_ptr(pPCX, y);
                for (drpcx_uint32 x = 0; x < pPCX->header.bytesPerLine; ++x) {
                    if (rleCount == 0) {
                        rleCount = drpcx__rle(pPCX, &rleValue);
                    }
                    rleCount -= 1;

                    for (int bitpair = 0; (bitpair < 4) && ((x * 4 + bitpair) < pPCX->width); ++bitpair) {
                        drpcx_uint8 mask = (4 << (3 - bitpair));
                        drpcx_uint8 paletteIndex = (rleValue & mask) >> (3 - bitpair);

                        pRow[0] |= ((paletteIndex & 0x03) << (c * 2));
                        pRow += pPCX->components;
                    }
                }
            }


            drpcx_uint8* pRow = drpcx__row_ptr(pPCX, y);
            for (drpcx_uint32 x = 0; x < pPCX->width; ++x) {
                drpcx_uint8 paletteIndex = pRow[0];
                for (drpcx_uint32 c = 0; c < pPCX->header.bitPlanes; ++c) {
                    pRow[c] = pPCX->header.palette16[paletteIndex * 3 + c];
                }

                pRow += pPCX->components;
            }
        }

        return DRPCX_TRUE;
    };

    default: return DRPCX_FALSE;
    }
}

drpcx_bool32 drpcx__decode_4bit(drpcx* pPCX)
{
    // NOTE: This is completely untested. If anybody knows where I can get a test file please let me know or send it through to me!
    // TODO: Test Me.

    if (pPCX->header.bitPlanes > 1) {
        return DRPCX_FALSE;
    }

    drpcx_uint8 rleCount = 0;
    drpcx_uint8 rleValue = 0;

    for (drpcx_uint32 y = 0; y < pPCX->height; ++y) {
        for (drpcx_uint32 c = 0; c < pPCX->header.bitPlanes; ++c) {
            drpcx_uint8* pRow = drpcx__row_ptr(pPCX, y);
            drpcx_uint8* pPalette = drpcx__indices_rowptr(pPCX, y);

            for (drpcx_uint32 x = 0; x < pPCX->header.bytesPerLine; ++x) {
                if (rleCount == 0) {
                    rleCount = drpcx__rle(pPCX, &rleValue);
                }
                rleCount -= 1;

                for (int nibble = 0; (nibble < 2) && ((x * 2 + nibble) < pPCX->width); ++nibble)
                {
                    drpcx_uint8 mask = (4 << (1 - nibble));
                    drpcx_uint8 paletteIndex = (rleValue & mask) >> (1 - nibble);

                    pRow[0] |= ((paletteIndex & 0x0F) << (c * 4));
                    *pPalette++ = pRow[0];

                    pRow += pPCX->components;
                }
            }
        }

        drpcx_uint8* pRow = drpcx__row_ptr(pPCX, y);
        for (drpcx_uint32 x = 0; x < pPCX->width; ++x) {
            drpcx_uint8 paletteIndex = pRow[0];

            for (drpcx_uint32 c = 0; c < pPCX->components; ++c) {
                pRow[c] = pPCX->header.palette16[paletteIndex * 3 + c];
            }

            pRow += pPCX->components;
        }
    }

    return DRPCX_TRUE;
}

drpcx_bool32 drpcx__decode_8bit(drpcx* pPCX)
{
    drpcx_uint8 rleCount = 0;
    drpcx_uint8 rleValue = 0;
    drpcx_uint32 stride = pPCX->width * pPCX->components;

    switch (pPCX->header.bitPlanes)
    {
    case 1:
    {
        for (drpcx_uint32 y = 0; y < pPCX->height; ++y) {
            drpcx_uint8* pRow = drpcx__row_ptr(pPCX, y);
            for (drpcx_uint32 x = 0; x < pPCX->header.bytesPerLine; ++x) {
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

        // At this point we can know if we are dealing with a palette or a grayscale image by checking the next byte. If it's equal to 0x0C, we
        // need to do a simple palette lookup.
        drpcx_uint8 paletteMarker = drpcx__read_byte(pPCX);
        if (paletteMarker == 0x0C) {
            // A palette is present - we need to do a second pass.
            drpcx_uint8 palette256[768];
            if (pPCX->onRead(pPCX->pUserData, palette256, sizeof(palette256)) != sizeof(palette256)) {
                return DRPCX_FALSE;
            }

            for (drpcx_uint32 y = 0; y < pPCX->height; ++y) {
                drpcx_uint8* pRow = pPCX->pImageData + (y * stride);
                drpcx_uint8* pPalette = drpcx__indices_rowptr(pPCX, y);

                for (drpcx_uint32 x = 0; x < pPCX->width; ++x) {
                    drpcx_uint8 index = pRow[0];
                    *pPalette++ = index;

                    pRow[0] = palette256[index * 3 + 0];
                    pRow[1] = palette256[index * 3 + 1];
                    pRow[2] = palette256[index * 3 + 2];
                    pRow += 3;
                }
            }
        }

        return DRPCX_TRUE;
    }

    case 3:
    case 4:
    {
        for (drpcx_uint32 y = 0; y < pPCX->height; ++y) {
            for (drpcx_uint32 c = 0; c < pPCX->components; ++c) {
                drpcx_uint8* pRow = drpcx__row_ptr(pPCX, y);
                for (drpcx_uint32 x = 0; x < pPCX->header.bytesPerLine; ++x) {
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

        return DRPCX_TRUE;
    }
    }

    return DRPCX_TRUE;
}

int drpcx_load(drpcx *pcxResult, drpcx_read_proc onRead, const char* name, void* pUserData, drpcx_bool32 flipped, int* x, int* y, int* internalComponents, int desiredComponents)
{
    drpcx pcx{};
    pcx.loaded = 0;
    *pcxResult = pcx;

    if (onRead == NULL) return -1;
    if (desiredComponents > 4) return -2;
    
    pcx.onRead = onRead;
    pcx.pUserData = pUserData;
    pcx.flipped = flipped;
    if (onRead(pUserData, &pcx.header, sizeof(pcx.header)) != sizeof(pcx.header)) {
        return -3;    // Failed to read the header.
    }

    if (pcx.header.header != 10) {
        return -4;    // Not a PCX file.
    }

    if (pcx.header.encoding != 1) {
        return -5;    // Not supporting non-RLE encoding. Would assume a value of 0 indicates raw, unencoded, but that is apparently never used.
    }

    if (pcx.header.bpp != 1 && pcx.header.bpp != 2 && pcx.header.bpp != 4 && pcx.header.bpp != 8) {
        return -6;    // Unsupported pixel format.
    }

    if (pcx.header.left > pcx.header.right) {
        drpcx_uint16 temp = pcx.header.left;
        pcx.header.left = pcx.header.right;
        pcx.header.right = temp;
    }
    if (pcx.header.top > pcx.header.bottom) {
        drpcx_uint16 temp = pcx.header.top;
        pcx.header.top = pcx.header.bottom;
        pcx.header.bottom = temp;
    }

    pcx.width = pcx.header.right - pcx.header.left + 1;
    pcx.height = pcx.header.bottom - pcx.header.top + 1;
    pcx.components = (pcx.header.bpp == 8 && pcx.header.bitPlanes == 4) ? 4 : 3;

    size_t dataSize = pcx.width * pcx.height * pcx.components;
    pcx.pImageData = (drpcx_uint8*)calloc(1, dataSize);   // <-- Clearing to zero is important! Required for proper decoding.
    if (pcx.pImageData == NULL) {
        return -7;    // Failed to allocate memory.
    }

    pcx.pPaletteIndices = (drpcx_uint8*)calloc(sizeof(drpcx_uint8), pcx.width * pcx.height);
    if (pcx.pPaletteIndices == NULL) {
        return -7;    // Failed to allocate memory.
    }

    // There's an annoying amount of branching when loading PCX files so for simplicity I'm doing the component conversion as
    // a second pass.
    /*
    char str[2048];
    auto strl = sprintf(str, "Image %s components %d, got components %d, bpp %d, bitPlanes %d, palette type %d, res %d\n",
        name,
        (int32_t)desiredComponents, (int32_t)pcx.components, (int32_t)pcx.header.bpp, (int32_t)pcx.header.bitPlanes,
        (int32_t)pcx.header.paletteType, (int32_t)pcx.header.reserved1);
    
    auto pptr = &str[strl];

    for (int i = 0; i < 54; i++) {
        if (i % 8 == 0) {
            pptr[0] = '\n';
            pptr++;
        }

        auto nc = sprintf(pptr, "%02X ", (int32_t)pcx.header.reserved2[i]);
        pptr += nc;
    }

    MessageBoxA(nullptr, str, "Hello", 0);
    */

    drpcx_bool32 result = DRPCX_FALSE;
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
        free(pcx.pPaletteIndices);
        return -8;
    }

    if (desiredComponents == 0) 
        desiredComponents = pcx.components;
    
    if (desiredComponents != (int)pcx.components) {
        drpcx_uint8* pNewImageData = (drpcx_uint8*)malloc(pcx.width * pcx.height * desiredComponents);
        if (pNewImageData == NULL) {
            free(pcx.pImageData);
            free(pcx.pPaletteIndices);
            return -7;
        }

        drpcx_uint8* pSrcData = pcx.pImageData;
        drpcx_uint8* pDstData = pNewImageData;
        if (desiredComponents < (int)pcx.components) {
            // We're reducing the number of components. Just drop the excess.
            for (drpcx_uint32 i = 0; i < pcx.width * pcx.height; ++i) {
                for (int c = 0; c < desiredComponents; ++c) {
                    pDstData[c] = pSrcData[c];
                }

                pSrcData += pcx.components;
                pDstData += desiredComponents;
            }
        }
        else {
            // We're increasing the number of components. Always ensure the alpha channel is set to 0xFF.
            if (pcx.components == 1) {
                for (drpcx_uint32 i = 0; i < pcx.width * pcx.height; ++i) {
                    for (int c = 0; c < desiredComponents; ++c) {
                        pDstData[c] = pSrcData[0];
                    }

                    pSrcData += pcx.components;
                    pDstData += desiredComponents;
                }
            }
            else if (pcx.components == 2) {
                for (drpcx_uint32 i = 0; i < pcx.width * pcx.height; ++i) {
                    pDstData[0] = pSrcData[0];
                    pDstData[1] = pSrcData[1];
                    pDstData[2] = 0x00;
                    if (desiredComponents == 4) pDstData[3] = 0xFF;

                    pSrcData += pcx.components;
                    pDstData += desiredComponents;
                }
            }
            else {
                assert(pcx.components == 3);
                assert(desiredComponents == 4);

                for (drpcx_uint32 i = 0; i < pcx.width * pcx.height; ++i) {
                    pDstData[0] = pSrcData[2];
                    pDstData[1] = pSrcData[1];
                    pDstData[2] = pSrcData[0];
                    pDstData[3] = 0xFF;

                    pSrcData += pcx.components;
                    pDstData += desiredComponents;
                }
            }
        }

        free(pcx.pImageData);
        pcx.pImageData = pNewImageData;
    }

    if (x) *x = pcx.width;
    if (y) *y = pcx.height;
    if (internalComponents) *internalComponents = pcx.components;

    pcx.loaded = 1;
    *pcxResult = pcx;
    return 1;
}

uint8_t* makeTGAPalette(uint8_t* paletteIndex, int tgaDataSize, int16_t width, int16_t height) {
    uint32_t pad = 0;
    uint16_t twenty = 0x2020;
    uint32_t magic = 0x20000;
    uint8_t* tga = new uint8_t[0x12 + tgaDataSize];

    memcpy(tga + 0x00, &magic, 4);
    memcpy(tga + 0x04, &pad, 4);
    memcpy(tga + 0x08, &pad, 4);
    memcpy(tga + 0x0C, &width, 2);
    memcpy(tga + 0x0E, &height, 2);
    memcpy(tga + 0x10, &twenty, 2);

    uint8_t* tgaImageData = tga + 0x12;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            auto isAlpha = paletteIndex[x + y * width] == 0;
            tgaImageData[0] = isAlpha ? 0x00 : 0xFF;
            tgaImageData[1] = isAlpha ? 0x00 : 0xFF;
            tgaImageData[2] = isAlpha ? 0x00 : 0xFF;
            tgaImageData[3] = isAlpha ? 0x00 : 0xFF;
            tgaImageData += 4;
        }
    }

    return tga;
}

uint8_t* makeTGA(uint8_t *palette, uint8_t* data, int dataSize, int16_t width, int16_t height) {
    uint32_t pad = 0;
    uint16_t twenty = 0x2020;
    uint32_t magic = 0x20000;
    uint8_t* tga = new uint8_t[0x12 + dataSize];

    memcpy(tga + 0x00, &magic, 4);
    memcpy(tga + 0x04, &pad, 4);
    memcpy(tga + 0x08, &pad, 4);
    memcpy(tga + 0x0C, &width, 2);
    memcpy(tga + 0x0E, &height, 2);
    memcpy(tga + 0x10, &twenty, 2);

    extern bool g_mgs1OalphaLoad;

    uint8_t* tgaPixels = tga + 0x12;
    uint32_t* tgaPixelsABGR = (uint32_t*)(tga + 0x12);
    
    if (!g_mgs1OalphaLoad) {
        memcpy(tgaPixels, data, dataSize);
    }
    else {
        memcpy(tgaPixels, data, dataSize);

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                auto pixIdx = x + y * width;

                auto isBlack = tgaPixelsABGR[pixIdx] == 0xFF000000UL;
                auto isFirstColor = palette[pixIdx] == 0;
                auto alpha = (isBlack /* & isFirstColor*/) ? 0x00 : 0xFF;
                tgaPixels[pixIdx * 4 + 3] = (uint8_t)alpha;
            }
        }
    }

    return tga;
}


void drpcx_free(void* pReturnValueFromLoad)
{
    free(pReturnValueFromLoad);
}

// DR_PCX_IMPLEMENTATION


// REVISION HISTORY
//
// v0.3.1 - 2018-09-11
//   - Styling fixes.
//   - Fix a typo.
//
// v0.3 - 2018-02-08
//   - API CHANGE: Rename dr_* types to drpcx_*.
//
// v0.2c - 2018-02-07
//   - Fix a crash.
//
// v0.2b - 2018-02-02
//   - Fix compilation error.
//
// v0.2a - 2017-07-16
//   - Change underlying type for booleans to unsigned.
//
// v0.2 - 2016-10-28
//   - API CHANGE: Add a parameter to drpcx_load() and family to control the number of output components.
//   - Use custom sized types rather than built-in ones to improve support for older MSVC compilers.
//
// v0.1c - 2016-10-23
//   - A minor change to drpcx_bool8 and drpcx_bool32 types.
//
// v0.1b - 2016-10-11
//   - Use drpcx_bool32 instead of the built-in "bool" type. The reason for this change is that it helps maintain API/ABI consistency
//     between C and C++ builds.
//
// v0.1a - 2016-09-18
//   - Change date format to ISO 8601 (YYYY-MM-DD)
//
// v0.1 - 2016-05-04
//   - Initial versioned release.


// TODO
// - Test 2-bpp/4-plane and 4-bpp/1-plane formats.


/*
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>
*/