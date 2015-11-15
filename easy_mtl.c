// Public Domain. See "unlicense" statement at the end of this file.

#include "easy_mtl.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#if defined(__clang__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wcast-align"
#endif


// When constructing the material's raw data, memory is allocated in blocks of this amount. This must be at least 256.
#define EASYMTL_CHUNK_SIZE              4096

#define EASYMTL_STAGE_IDS               0
#define EASYMTL_STAGE_PRIVATE_INPUTS    1
#define EASYMTL_STAGE_PUBLIC_INPUTS     2
#define EASYMTL_STAGE_CHANNELS          3
#define EASYMTL_STAGE_PROPERTIES        4
#define EASYMTL_STAGE_COMPLETE          UINT_MAX


////////////////////////////////////////////////////////
// Utilities

// strcpy()
int easymtl_strcpy(char* dst, size_t dstSizeInBytes, const char* src)
{
#if defined(_MSC_VER)
    return strcpy_s(dst, dstSizeInBytes, src);
#else
    if (dst == 0) {
        return EINVAL;
    }
    if (dstSizeInBytes == 0) {
        return ERANGE;
    }
    if (src == 0) {
        dst[0] = '\0';
        return EINVAL;
    }
    
    char* iDst = dst;
    const char* iSrc = src;
    size_t remainingSizeInBytes = dstSizeInBytes;
    while (remainingSizeInBytes > 0 && iSrc[0] != '\0')
    {
        iDst[0] = iSrc[0];

        iDst += 1;
        iSrc += 1;
        remainingSizeInBytes -= 1;
    }

    if (remainingSizeInBytes > 0) {
        iDst[0] = '\0';
    } else {
        dst[0] = '\0';
        return ERANGE;
    }

    return 0;
#endif
}



/// Inflates the materials data buffer by EASYMTL_CHUNK_SIZE.
bool _easymtl_inflate(easymtl_material* pMaterial);


bool easymtl_init(easymtl_material* pMaterial)
{
    if (pMaterial != NULL)
    {
        assert(EASYMTL_CHUNK_SIZE >= 256);

        pMaterial->pRawData = malloc(EASYMTL_CHUNK_SIZE);
        if (pMaterial->pRawData != NULL)
        {
            pMaterial->sizeInBytes          = sizeof(easymtl_header);
            pMaterial->bufferSizeInBytes    = EASYMTL_CHUNK_SIZE;
            pMaterial->currentStage         = EASYMTL_STAGE_IDS;
            pMaterial->currentChannelOffset = 0;
            pMaterial->ownsRawData          = 1;

            easymtl_header* pHeader = easymtl_getheader(pMaterial);
            assert(pHeader != NULL);

            pHeader->magic                    = EASYMTL_MAGIC_NUMBER;
            pHeader->version                  = EASYMTL_CURRENT_VERSION;
            pHeader->identifierSizeInBytes    = sizeof(easymtl_identifier);
            pHeader->inputSizeInBytes         = sizeof(easymtl_input);
            pHeader->channelHeaderSizeInBytes = sizeof(easymtl_channel_header);
            pHeader->instructionSizeInBytes   = sizeof(easymtl_instruction);
            pHeader->propertySizeInBytes      = sizeof(easymtl_property);
            pHeader->identifierCount          = 0;
            pHeader->privateInputCount        = 0;
            pHeader->publicInputCount         = 0;
            pHeader->channelCount             = 0;
            pHeader->propertyCount            = 0;
            pHeader->identifiersOffset        = pMaterial->sizeInBytes;
            pHeader->inputsOffset             = pMaterial->sizeInBytes;
            pHeader->channelsOffset           = pMaterial->sizeInBytes;
            pHeader->propertiesOffset         = pMaterial->sizeInBytes;

            return 1;
        }
    }

    return 0;
}

bool easymtl_initfromexisting(easymtl_material* pMaterial, const void* pRawData, unsigned int dataSizeInBytes)
{
    if (pMaterial != NULL)
    {
        if (pRawData != NULL && dataSizeInBytes >= sizeof(easymtl_header))
        {
            if (((easymtl_header*)pMaterial->pRawData)->magic == EASYMTL_MAGIC_NUMBER)
            {
                pMaterial->pRawData = malloc(EASYMTL_CHUNK_SIZE);
                if (pMaterial->pRawData != NULL)
                {
                    memcpy(pMaterial->pRawData, pRawData, dataSizeInBytes);
                    pMaterial->sizeInBytes          = dataSizeInBytes;
                    pMaterial->bufferSizeInBytes    = dataSizeInBytes;
                    pMaterial->currentStage         = EASYMTL_STAGE_COMPLETE;
                    pMaterial->currentChannelOffset = 0;
                    pMaterial->ownsRawData          = 1;

                    return 1;
                }
            }
        }
    }

    return 0;
}

bool easymtl_initfromexisting_nocopy(easymtl_material* pMaterial, const void* pRawData, unsigned int dataSizeInBytes)
{
    if (pMaterial != NULL)
    {
        if (pRawData != NULL && dataSizeInBytes >= sizeof(easymtl_header))
        {
            if (((const easymtl_header*)pRawData)->magic == EASYMTL_MAGIC_NUMBER)
            {
                pMaterial->pRawData             = (easymtl_uint8*)pRawData;
                pMaterial->sizeInBytes          = dataSizeInBytes;
                pMaterial->bufferSizeInBytes    = dataSizeInBytes;
                pMaterial->currentStage         = EASYMTL_STAGE_COMPLETE;
                pMaterial->currentChannelOffset = 0;
                pMaterial->ownsRawData          = 0;

                return 1;
            }
        }
    }

    return 0;
}

void easymtl_uninit(easymtl_material* pMaterial)
{
    if (pMaterial != NULL)
    {
        if (pMaterial->ownsRawData)
        {
            free(pMaterial->pRawData);
        }

        pMaterial->pRawData = NULL;
    }
}


easymtl_header* easymtl_getheader(easymtl_material* pMaterial)
{
    if (pMaterial != NULL)
    {
        return (easymtl_header*)pMaterial->pRawData;
    }

    return NULL;
}


bool easymtl_appendidentifier(easymtl_material* pMaterial, easymtl_identifier identifier, unsigned int* indexOut)
{
    if (pMaterial != NULL)
    {
        if (pMaterial->currentStage <= EASYMTL_STAGE_IDS)
        {
            easymtl_header* pHeader = easymtl_getheader(pMaterial);
            if (pHeader != NULL)
            {
                if (pMaterial->sizeInBytes + pHeader->identifierSizeInBytes > pMaterial->bufferSizeInBytes)
                {
                    if (!_easymtl_inflate(pMaterial))
                    {
                        // An error occured when trying to inflate the buffer. Might be out of memory.
                        return 0;
                    }

                    pHeader = easymtl_getheader(pMaterial);
                    assert(pMaterial->sizeInBytes + pHeader->identifierSizeInBytes <= pMaterial->bufferSizeInBytes);
                }


                memcpy(pMaterial->pRawData + pHeader->inputsOffset, &identifier, pHeader->identifierSizeInBytes);
                pMaterial->sizeInBytes += pHeader->identifierSizeInBytes;

                pHeader->identifierCount  += 1;
                pHeader->inputsOffset     += pHeader->identifierSizeInBytes;
                pHeader->channelsOffset   += pHeader->identifierSizeInBytes;
                pHeader->propertiesOffset += pHeader->identifierSizeInBytes;


                if (indexOut != NULL)
                {
                    *indexOut = pHeader->identifierCount - 1;
                }

                return 1;
            }
        }
    }

    return 0;
}

bool easymtl_appendprivateinput(easymtl_material* pMaterial, easymtl_input input)
{
    if (pMaterial != NULL)
    {
        if (pMaterial->currentStage <= EASYMTL_STAGE_PRIVATE_INPUTS)
        {
            easymtl_header* pHeader = easymtl_getheader(pMaterial);
            if (pHeader != NULL)
            {
                if (pMaterial->sizeInBytes + pHeader->inputSizeInBytes > pMaterial->bufferSizeInBytes)
                {
                    if (!_easymtl_inflate(pMaterial))
                    {
                        // An error occured when trying to inflate the buffer. Might be out of memory.
                        return 0;
                    }

                    pHeader = easymtl_getheader(pMaterial);
                    assert(pMaterial->sizeInBytes + pHeader->inputSizeInBytes <= pMaterial->bufferSizeInBytes);
                }


                memcpy(pMaterial->pRawData + pHeader->channelsOffset, &input, pHeader->inputSizeInBytes);
                pMaterial->sizeInBytes += pHeader->inputSizeInBytes;

                pHeader->privateInputCount += 1;
                pHeader->channelsOffset    += pHeader->inputSizeInBytes;
                pHeader->propertiesOffset  += pHeader->inputSizeInBytes;


                pMaterial->currentStage = EASYMTL_STAGE_PRIVATE_INPUTS;
                return 1;
            }
        }
    }

    return 0;
}

bool easymtl_appendpublicinput(easymtl_material* pMaterial, easymtl_input input)
{
    if (pMaterial != NULL)
    {
        if (pMaterial->currentStage <= EASYMTL_STAGE_PUBLIC_INPUTS)
        {
            easymtl_header* pHeader = easymtl_getheader(pMaterial);
            if (pHeader != NULL)
            {
                if (pMaterial->sizeInBytes + pHeader->inputSizeInBytes > pMaterial->bufferSizeInBytes)
                {
                    if (!_easymtl_inflate(pMaterial))
                    {
                        // An error occured when trying to inflate the buffer. Might be out of memory.
                        return 0;
                    }

                    pHeader = easymtl_getheader(pMaterial);
                    assert(pMaterial->sizeInBytes + pHeader->inputSizeInBytes <= pMaterial->bufferSizeInBytes);
                }


                memcpy(pMaterial->pRawData + pHeader->channelsOffset, &input, pHeader->inputSizeInBytes);
                pMaterial->sizeInBytes += pHeader->inputSizeInBytes;

                pHeader->publicInputCount  += 1;
                pHeader->channelsOffset    += pHeader->inputSizeInBytes;
                pHeader->propertiesOffset  += pHeader->inputSizeInBytes;


                pMaterial->currentStage = EASYMTL_STAGE_PUBLIC_INPUTS;
                return 1;
            }
        }
    }

    return 0;
}

bool easymtl_appendchannel(easymtl_material* pMaterial, easymtl_channel channel)
{
    if (pMaterial != NULL)
    {
        if (pMaterial->currentStage <= EASYMTL_STAGE_CHANNELS)
        {
            easymtl_header* pHeader = easymtl_getheader(pMaterial);
            if (pHeader != NULL)
            {
                easymtl_channel_header channelHeader;
                channelHeader.channel         = channel;
                channelHeader.instructionCount = 0;

                if (pMaterial->sizeInBytes + pHeader->channelHeaderSizeInBytes > pMaterial->bufferSizeInBytes)
                {
                    if (!_easymtl_inflate(pMaterial))
                    {
                        // An error occured when trying to inflate the buffer. Might be out of memory.
                        return 0;
                    }

                    pHeader = easymtl_getheader(pMaterial);
                    assert(pMaterial->sizeInBytes + pHeader->channelHeaderSizeInBytes <= pMaterial->bufferSizeInBytes);
                }


                memcpy(pMaterial->pRawData + pHeader->propertiesOffset, &channelHeader, pHeader->channelHeaderSizeInBytes);
                pMaterial->currentChannelOffset = pMaterial->sizeInBytes;
                pMaterial->sizeInBytes += pHeader->channelHeaderSizeInBytes;

                pHeader->channelCount     += 1;
                pHeader->propertiesOffset += pHeader->channelHeaderSizeInBytes;


                pMaterial->currentStage = EASYMTL_STAGE_CHANNELS;
                return 1;
            }
        }
    }

    return 0;
}

bool easymtl_appendinstruction(easymtl_material* pMaterial, easymtl_instruction instruction)
{
    if (pMaterial != NULL)
    {
        if (pMaterial->currentStage == EASYMTL_STAGE_CHANNELS)
        {
            easymtl_header* pHeader = easymtl_getheader(pMaterial);
            if (pHeader != NULL)
            {
                if (pMaterial->sizeInBytes + pHeader->instructionSizeInBytes > pMaterial->bufferSizeInBytes)
                {
                    if (!_easymtl_inflate(pMaterial))
                    {
                        // An error occured when trying to inflate the buffer. Might be out of memory.
                        return 0;
                    }

                    pHeader = easymtl_getheader(pMaterial);
                    assert(pMaterial->sizeInBytes + pHeader->instructionSizeInBytes <= pMaterial->bufferSizeInBytes);
                }


                memcpy(pMaterial->pRawData + pHeader->propertiesOffset, &instruction, pHeader->instructionSizeInBytes);
                pMaterial->sizeInBytes += pHeader->instructionSizeInBytes;

                easymtl_channel_header* pChannelHeader = (easymtl_channel_header*)(pMaterial->pRawData + pMaterial->currentChannelOffset);
                if (pChannelHeader != NULL)
                {
                    pChannelHeader->instructionCount += 1;
                }

                pHeader->propertiesOffset += pHeader->instructionSizeInBytes;


                return 1;
            }
        }
    }

    return 0;
}

bool easymtl_appendproperty(easymtl_material* pMaterial, easymtl_property prop)
{
    if (pMaterial != NULL)
    {
        if (pMaterial->currentStage <= EASYMTL_STAGE_CHANNELS)
        {
            easymtl_header* pHeader = easymtl_getheader(pMaterial);
            if (pHeader != NULL)
            {
                if (pMaterial->sizeInBytes + pHeader->propertySizeInBytes > pMaterial->bufferSizeInBytes)
                {
                    if (!_easymtl_inflate(pMaterial))
                    {
                        // An error occured when trying to inflate the buffer. Might be out of memory.
                        return 0;
                    }

                    pHeader = easymtl_getheader(pMaterial);
                    assert(pMaterial->sizeInBytes + pHeader->propertySizeInBytes <= pMaterial->bufferSizeInBytes);
                }


                memcpy(pMaterial->pRawData + pMaterial->sizeInBytes, &prop, pHeader->propertySizeInBytes);
                pMaterial->sizeInBytes += pHeader->propertySizeInBytes;

                pHeader->propertyCount += 1;


                pMaterial->currentStage = EASYMTL_STAGE_PROPERTIES;
                return 1;
            }
        }
    }

    return 0;
}


easymtl_channel_header* easymtl_getchannelheaderbyindex(easymtl_material* pMaterial, unsigned int channelIndex)
{
    if (pMaterial != NULL)
    {
        easymtl_header* pHeader = easymtl_getheader(pMaterial);
        assert(pHeader != NULL);

        if (channelIndex < pHeader->channelCount)
        {
            easymtl_uint8* pChannelHeader = pMaterial->pRawData + pHeader->channelsOffset;
            for (unsigned int iChannel = 0; iChannel < channelIndex; ++iChannel)
            {
                pChannelHeader += sizeof(easymtl_channel_header) + (sizeof(easymtl_instruction) * ((easymtl_channel_header*)pChannelHeader)->instructionCount);
            }

            return (easymtl_channel_header*)pChannelHeader;
        }
    }

    return NULL;
}

easymtl_channel_header* easymtl_getchannelheaderbyname(easymtl_material* pMaterial, const char* channelName)
{
    if (pMaterial != NULL)
    {
        easymtl_header* pHeader = easymtl_getheader(pMaterial);
        assert(pHeader != NULL);

        easymtl_uint8* pChannelHeader = pMaterial->pRawData + pHeader->channelsOffset;
        for (unsigned int iChannel = 0; iChannel < pHeader->channelCount; ++iChannel)
        {
            if (strcmp(((easymtl_channel_header*)pChannelHeader)->channel.name, channelName) == 0)
            {
                return (easymtl_channel_header*)pChannelHeader;
            }

            pChannelHeader += sizeof(easymtl_channel_header) + (sizeof(easymtl_instruction) * ((easymtl_channel_header*)pChannelHeader)->instructionCount);
        }
    }

    return NULL;
}

easymtl_identifier* easymtl_getidentifiers(easymtl_material* pMaterial)
{
    if (pMaterial != NULL)
    {
        easymtl_header* pHeader = easymtl_getheader(pMaterial);
        assert(pHeader != NULL);

        return (easymtl_identifier*)(pMaterial->pRawData + pHeader->identifiersOffset);
    }

    return NULL;
}

easymtl_identifier* easymtl_getidentifier(easymtl_material* pMaterial, unsigned int index)
{
    if (pMaterial != NULL)
    {
        easymtl_header* pHeader = easymtl_getheader(pMaterial);
        assert(pHeader != NULL);

        if (index < pHeader->identifierCount)
        {
            easymtl_identifier* firstIdentifier = (easymtl_identifier*)(pMaterial->pRawData + pHeader->identifiersOffset);
            return firstIdentifier + index;
        }
    }

    return NULL;
}

unsigned int easymtl_getidentifiercount(easymtl_material* pMaterial)
{
    if (pMaterial != NULL)
    {
        easymtl_header* pHeader = easymtl_getheader(pMaterial);
        assert(pHeader != NULL);

        return pHeader->identifierCount;
    }

    return 0;
}


unsigned int easymtl_getinputcount(easymtl_material* pMaterial)
{
    if (pMaterial != NULL)
    {
        easymtl_header* pHeader = easymtl_getheader(pMaterial);
        assert(pHeader != NULL);

        return pHeader->privateInputCount + pHeader->publicInputCount;
    }

    return 0;
}

easymtl_input* easymtl_getinputbyindex(easymtl_material* pMaterial, unsigned int index)
{
    if (pMaterial != NULL)
    {
        easymtl_header* pHeader = easymtl_getheader(pMaterial);
        assert(pHeader != NULL);

        if (index < (pHeader->privateInputCount + pHeader->publicInputCount))
        {
            easymtl_input* firstInput = (easymtl_input*)(pMaterial->pRawData + pHeader->inputsOffset);
            return firstInput + index;
        }
    }

    return NULL;
}

unsigned int easymtl_getprivateinputcount(easymtl_material* pMaterial)
{
    if (pMaterial != NULL)
    {
        easymtl_header* pHeader = easymtl_getheader(pMaterial);
        assert(pHeader != NULL);

        return pHeader->privateInputCount;
    }

    return 0;
}

easymtl_input* easymtl_getprivateinputbyindex(easymtl_material* pMaterial, unsigned int index)
{
    if (pMaterial != NULL)
    {
        easymtl_header* pHeader = easymtl_getheader(pMaterial);
        assert(pHeader != NULL);

        if (index < pHeader->privateInputCount)
        {
            easymtl_input* firstInput = (easymtl_input*)(pMaterial->pRawData + pHeader->inputsOffset);
            return firstInput + index;
        }
    }

    return NULL;
}

unsigned int easymtl_getpublicinputcount(easymtl_material* pMaterial)
{
    if (pMaterial != NULL)
    {
        easymtl_header* pHeader = easymtl_getheader(pMaterial);
        assert(pHeader != NULL);

        return pHeader->publicInputCount;
    }

    return 0;
}

easymtl_input* easymtl_getpublicinputbyindex(easymtl_material* pMaterial, unsigned int index)
{
    if (pMaterial != NULL)
    {
        easymtl_header* pHeader = easymtl_getheader(pMaterial);
        assert(pHeader != NULL);

        if (index < pHeader->publicInputCount)
        {
            easymtl_input* firstInput = (easymtl_input*)(pMaterial->pRawData + pHeader->inputsOffset);
            return firstInput + pHeader->privateInputCount + index;
        }
    }

    return NULL;
}


unsigned int easymtl_getpropertycount(easymtl_material* pMaterial)
{
    if (pMaterial != NULL)
    {
        easymtl_header* pHeader = easymtl_getheader(pMaterial);
        assert(pHeader != NULL);

        return pHeader->propertyCount;
    }

    return 0;
}

easymtl_property* easymtl_getpropertybyindex(easymtl_material* pMaterial, unsigned int index)
{
    if (pMaterial != NULL)
    {
        easymtl_header* pHeader = easymtl_getheader(pMaterial);
        assert(pHeader != NULL);

        if (index < pHeader->propertyCount)
        {
            easymtl_property* firstProperty = (easymtl_property*)(pMaterial->pRawData + pHeader->propertiesOffset);
            return firstProperty + index;
        }
    }

    return NULL;
}

easymtl_property* easymtl_getpropertybyname(easymtl_material* pMaterial, const char* name)
{
    if (pMaterial != NULL)
    {
        easymtl_header* pHeader = easymtl_getheader(pMaterial);
        assert(pHeader != NULL);

        for (unsigned int i = 0; i < pHeader->propertyCount; ++i)
        {
            easymtl_property* pProperty = ((easymtl_property*)(pMaterial->pRawData + pHeader->propertiesOffset)) + i;
            assert(pProperty != NULL);

            if (strcmp(pProperty->name, name) == 0)
            {
                return pProperty;
            }
        }
    }

    return NULL;
}


//////////////////////////////////
// Private low-level API.

bool _easymtl_inflate(easymtl_material* pMaterial)
{
    assert(pMaterial != NULL);

    easymtl_uint8* pOldBuffer = pMaterial->pRawData;
    easymtl_uint8* pNewBuffer = malloc(pMaterial->bufferSizeInBytes + EASYMTL_CHUNK_SIZE);
    if (pNewBuffer != NULL)
    {
        memcpy(pNewBuffer, pOldBuffer, pMaterial->sizeInBytes);
        pMaterial->pRawData = pNewBuffer;
        pMaterial->bufferSizeInBytes += EASYMTL_CHUNK_SIZE;

        free(pOldBuffer);
        return 1;
    }

    return 0;
}



////////////////////////////////////////////////////////
// Mid-Level APIs

easymtl_identifier easymtl_identifier_float(const char* name)
{
    easymtl_identifier identifier;
    identifier.type = easymtl_type_float;
    easymtl_strcpy(identifier.name, EASYMTL_MAX_IDENTIFIER_NAME, name);

    return identifier;
}

easymtl_identifier easymtl_identifier_float2(const char* name)
{
    easymtl_identifier identifier;
    identifier.type = easymtl_type_float2;
    easymtl_strcpy(identifier.name, EASYMTL_MAX_IDENTIFIER_NAME, name);

    return identifier;
}

easymtl_identifier easymtl_identifier_float3(const char* name)
{
    easymtl_identifier identifier;
    identifier.type = easymtl_type_float3;
    easymtl_strcpy(identifier.name, EASYMTL_MAX_IDENTIFIER_NAME, name);

    return identifier;
}

easymtl_identifier easymtl_identifier_float4(const char* name)
{
    easymtl_identifier identifier;
    identifier.type = easymtl_type_float4;
    easymtl_strcpy(identifier.name, EASYMTL_MAX_IDENTIFIER_NAME, name);

    return identifier;
}

easymtl_identifier easymtl_identifier_int(const char* name)
{
    easymtl_identifier identifier;
    identifier.type = easymtl_type_int;
    easymtl_strcpy(identifier.name, EASYMTL_MAX_IDENTIFIER_NAME, name);

    return identifier;
}

easymtl_identifier easymtl_identifier_int2(const char* name)
{
    easymtl_identifier identifier;
    identifier.type = easymtl_type_int2;
    easymtl_strcpy(identifier.name, EASYMTL_MAX_IDENTIFIER_NAME, name);

    return identifier;
}

easymtl_identifier easymtl_identifier_int3(const char* name)
{
    easymtl_identifier identifier;
    identifier.type = easymtl_type_int3;
    easymtl_strcpy(identifier.name, EASYMTL_MAX_IDENTIFIER_NAME, name);

    return identifier;
}

easymtl_identifier easymtl_identifier_int4(const char* name)
{
    easymtl_identifier identifier;
    identifier.type = easymtl_type_int4;
    easymtl_strcpy(identifier.name, EASYMTL_MAX_IDENTIFIER_NAME, name);

    return identifier;
}

easymtl_identifier easymtl_identifier_tex2d(const char* name)
{
    easymtl_identifier identifier;
    identifier.type = easymtl_type_tex2d;
    easymtl_strcpy(identifier.name, EASYMTL_MAX_IDENTIFIER_NAME, name);

    return identifier;
}


easymtl_input easymtl_input_float(unsigned int identifierIndex, float x)
{
    easymtl_input input;
    input.identifierIndex = identifierIndex;
    input.f1.x = x;

    return input;
}

easymtl_input easymtl_input_float2(unsigned int identifierIndex, float x, float y)
{
    easymtl_input input;
    input.identifierIndex = identifierIndex;
    input.f2.x = x;
    input.f2.y = y;

    return input;
}

easymtl_input easymtl_input_float3(unsigned int identifierIndex, float x, float y, float z)
{
    easymtl_input input;
    input.identifierIndex = identifierIndex;
    input.f3.x = x;
    input.f3.y = y;
    input.f3.z = z;

    return input;
}

easymtl_input easymtl_input_float4(unsigned int identifierIndex, float x, float y, float z, float w)
{
    easymtl_input input;
    input.identifierIndex = identifierIndex;
    input.f4.x = x;
    input.f4.y = y;
    input.f4.z = z;
    input.f4.w = w;

    return input;
}

easymtl_input easymtl_input_int(unsigned int identifierIndex, int x)
{
    easymtl_input input;
    input.identifierIndex = identifierIndex;
    input.i1.x = x;

    return input;
}

easymtl_input easymtl_input_int2(unsigned int identifierIndex, int x, int y)
{
    easymtl_input input;
    input.identifierIndex = identifierIndex;
    input.i2.x = x;
    input.i2.y = y;

    return input;
}

easymtl_input easymtl_input_int3(unsigned int identifierIndex, int x, int y, int z)
{
    easymtl_input input;
    input.identifierIndex = identifierIndex;
    input.i3.x = x;
    input.i3.y = y;
    input.i3.z = z;

    return input;
}

easymtl_input easymtl_input_int4(unsigned int identifierIndex, int x, int y, int z, int w)
{
    easymtl_input input;
    input.identifierIndex = identifierIndex;
    input.i4.x = x;
    input.i4.y = y;
    input.i4.z = z;
    input.i4.w = w;

    return input;
}

easymtl_input easymtl_input_tex(unsigned int identifierIndex, const char* path)
{
    easymtl_input input;
    input.identifierIndex = identifierIndex;
    easymtl_strcpy(input.path.value, EASYMTL_MAX_INPUT_PATH, path);

    return input;
}


easymtl_channel easymtl_channel_float(const char* name)
{
    easymtl_channel channel;
    channel.type = easymtl_type_float;
    easymtl_strcpy(channel.name, EASYMTL_MAX_CHANNEL_NAME, name);

    return channel;
}

easymtl_channel easymtl_channel_float2(const char* name)
{
    easymtl_channel channel;
    channel.type = easymtl_type_float2;
    easymtl_strcpy(channel.name, EASYMTL_MAX_CHANNEL_NAME, name);

    return channel;
}

easymtl_channel easymtl_channel_float3(const char* name)
{
    easymtl_channel channel;
    channel.type = easymtl_type_float3;
    easymtl_strcpy(channel.name, EASYMTL_MAX_CHANNEL_NAME, name);

    return channel;
}

easymtl_channel easymtl_channel_float4(const char* name)
{
    easymtl_channel channel;
    channel.type = easymtl_type_float4;
    easymtl_strcpy(channel.name, EASYMTL_MAX_CHANNEL_NAME, name);

    return channel;
}

easymtl_channel easymtl_channel_int(const char* name)
{
    easymtl_channel channel;
    channel.type = easymtl_type_int;
    easymtl_strcpy(channel.name, EASYMTL_MAX_CHANNEL_NAME, name);

    return channel;
}

easymtl_channel easymtl_channel_int2(const char* name)
{
    easymtl_channel channel;
    channel.type = easymtl_type_int2;
    easymtl_strcpy(channel.name, EASYMTL_MAX_CHANNEL_NAME, name);

    return channel;
}

easymtl_channel easymtl_channel_int3(const char* name)
{
    easymtl_channel channel;
    channel.type = easymtl_type_int3;
    easymtl_strcpy(channel.name, EASYMTL_MAX_CHANNEL_NAME, name);

    return channel;
}

easymtl_channel easymtl_channel_int4(const char* name)
{
    easymtl_channel channel;
    channel.type = easymtl_type_int4;
    easymtl_strcpy(channel.name, EASYMTL_MAX_CHANNEL_NAME, name);

    return channel;
}


easymtl_instruction easymtl_movf1_v1(unsigned int outputIdentifierIndex, unsigned int inputIdentifierIndex)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_movf1;
    inst.mov.inputDesc.x = EASYMTL_INPUT_DESC_VARX;
    inst.mov.inputX.id   = inputIdentifierIndex;
    inst.mov.output      = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_movf1_c1(unsigned int outputIdentifierIndex, float x)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_movf1;
    inst.mov.inputDesc.x   = EASYMTL_INPUT_DESC_CONSTF;
    inst.mov.inputX.valuef = x;
    inst.mov.output        = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_movf2_v2(unsigned int outputIdentifierIndex, unsigned int inputIdentifierIndex)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_movf2;
    inst.mov.inputDesc.x = EASYMTL_INPUT_DESC_VARX;
    inst.mov.inputDesc.y = EASYMTL_INPUT_DESC_VARY;
    inst.mov.inputX.id   = inputIdentifierIndex;
    inst.mov.inputY.id   = inputIdentifierIndex;
    inst.mov.output      = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_movf2_c2(unsigned int outputIdentifierIndex, float x, float y)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_movf2;
    inst.mov.inputDesc.x   = EASYMTL_INPUT_DESC_CONSTF;
    inst.mov.inputDesc.y   = EASYMTL_INPUT_DESC_CONSTF;
    inst.mov.inputX.valuef = x;
    inst.mov.inputY.valuef = y;
    inst.mov.output        = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_movf3_v3(unsigned int outputIdentifierIndex, unsigned int inputIdentifierIndex)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_movf3;
    inst.mov.inputDesc.x = EASYMTL_INPUT_DESC_VARX;
    inst.mov.inputDesc.y = EASYMTL_INPUT_DESC_VARY;
    inst.mov.inputDesc.z = EASYMTL_INPUT_DESC_VARZ;
    inst.mov.inputX.id   = inputIdentifierIndex;
    inst.mov.inputY.id   = inputIdentifierIndex;
    inst.mov.inputZ.id   = inputIdentifierIndex;
    inst.mov.output      = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_movf3_c3(unsigned int outputIdentifierIndex, float x, float y, float z)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_movf3;
    inst.mov.inputDesc.x   = EASYMTL_INPUT_DESC_CONSTF;
    inst.mov.inputDesc.y   = EASYMTL_INPUT_DESC_CONSTF;
    inst.mov.inputDesc.z   = EASYMTL_INPUT_DESC_CONSTF;
    inst.mov.inputX.valuef = x;
    inst.mov.inputY.valuef = y;
    inst.mov.inputZ.valuef = z;
    inst.mov.output        = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_movf4_v4(unsigned int outputIdentifierIndex, unsigned int inputIdentifierIndex)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_movf4;
    inst.mov.inputDesc.x = EASYMTL_INPUT_DESC_VARX;
    inst.mov.inputDesc.y = EASYMTL_INPUT_DESC_VARY;
    inst.mov.inputDesc.z = EASYMTL_INPUT_DESC_VARZ;
    inst.mov.inputDesc.w = EASYMTL_INPUT_DESC_VARW;
    inst.mov.inputX.id   = inputIdentifierIndex;
    inst.mov.inputY.id   = inputIdentifierIndex;
    inst.mov.inputZ.id   = inputIdentifierIndex;
    inst.mov.inputW.id   = inputIdentifierIndex;
    inst.mov.output      = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_movf4_c4(unsigned int outputIdentifierIndex, float x, float y, float z, float w)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_movf4;
    inst.mov.inputDesc.x   = EASYMTL_INPUT_DESC_CONSTF;
    inst.mov.inputDesc.y   = EASYMTL_INPUT_DESC_CONSTF;
    inst.mov.inputDesc.z   = EASYMTL_INPUT_DESC_CONSTF;
    inst.mov.inputDesc.w   = EASYMTL_INPUT_DESC_CONSTF;
    inst.mov.inputX.valuef = x;
    inst.mov.inputY.valuef = y;
    inst.mov.inputZ.valuef = z;
    inst.mov.inputW.valuef = w;
    inst.mov.output        = outputIdentifierIndex;

    return inst;
}


easymtl_instruction easymtl_addf1_v1(unsigned int outputIdentifierIndex, unsigned int inputIdentifierIndex)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_addf1;
    inst.add.inputDesc.x   = EASYMTL_INPUT_DESC_VARX;
    inst.add.inputX.id     = inputIdentifierIndex;
    inst.add.output        = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_addf1_c1(unsigned int outputIdentifierIndex, float x)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_addf1;
    inst.add.inputDesc.x   = EASYMTL_INPUT_DESC_CONSTF;
    inst.add.inputX.valuef = x;
    inst.add.output        = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_addf2_v2(unsigned int outputIdentifierIndex, unsigned int inputIdentifierIndex)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_addf2;
    inst.add.inputDesc.x   = EASYMTL_INPUT_DESC_VARX;
    inst.add.inputDesc.y   = EASYMTL_INPUT_DESC_VARY;
    inst.add.inputX.id     = inputIdentifierIndex;
    inst.add.inputY.id     = inputIdentifierIndex;
    inst.add.output        = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_addf2_c2(unsigned int outputIdentifierIndex, float x, float y)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_addf2;
    inst.add.inputDesc.x   = EASYMTL_INPUT_DESC_CONSTF;
    inst.add.inputDesc.y   = EASYMTL_INPUT_DESC_CONSTF;
    inst.add.inputX.valuef = x;
    inst.add.inputY.valuef = y;
    inst.add.output        = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_addf3_v3(unsigned int outputIdentifierIndex, unsigned int inputIdentifierIndex)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_addf3;
    inst.add.inputDesc.x   = EASYMTL_INPUT_DESC_VARX;
    inst.add.inputDesc.y   = EASYMTL_INPUT_DESC_VARY;
    inst.add.inputDesc.z   = EASYMTL_INPUT_DESC_VARZ;
    inst.add.inputX.id     = inputIdentifierIndex;
    inst.add.inputY.id     = inputIdentifierIndex;
    inst.add.inputZ.id     = inputIdentifierIndex;
    inst.add.output        = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_addf3_c3(unsigned int outputIdentifierIndex, float x, float y, float z)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_addf3;
    inst.add.inputDesc.x   = EASYMTL_INPUT_DESC_CONSTF;
    inst.add.inputDesc.y   = EASYMTL_INPUT_DESC_CONSTF;
    inst.add.inputDesc.z   = EASYMTL_INPUT_DESC_CONSTF;
    inst.add.inputX.valuef = x;
    inst.add.inputY.valuef = y;
    inst.add.inputZ.valuef = z;
    inst.add.output        = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_addf4_v4(unsigned int outputIdentifierIndex, unsigned int inputIdentifierIndex)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_addf4;
    inst.add.inputDesc.x   = EASYMTL_INPUT_DESC_VARX;
    inst.add.inputDesc.y   = EASYMTL_INPUT_DESC_VARY;
    inst.add.inputDesc.z   = EASYMTL_INPUT_DESC_VARZ;
    inst.add.inputDesc.w   = EASYMTL_INPUT_DESC_VARW;
    inst.add.inputX.id     = inputIdentifierIndex;
    inst.add.inputY.id     = inputIdentifierIndex;
    inst.add.inputZ.id     = inputIdentifierIndex;
    inst.add.inputW.id     = inputIdentifierIndex;
    inst.add.output        = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_addf4_c4(unsigned int outputIdentifierIndex, float x, float y, float z, float w)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_addf4;
    inst.add.inputDesc.x   = EASYMTL_INPUT_DESC_CONSTF;
    inst.add.inputDesc.y   = EASYMTL_INPUT_DESC_CONSTF;
    inst.add.inputDesc.z   = EASYMTL_INPUT_DESC_CONSTF;
    inst.add.inputDesc.w   = EASYMTL_INPUT_DESC_CONSTF;
    inst.add.inputX.valuef = x;
    inst.add.inputY.valuef = y;
    inst.add.inputZ.valuef = z;
    inst.add.inputW.valuef = w;
    inst.add.output        = outputIdentifierIndex;

    return inst;
}


easymtl_instruction easymtl_subf1_v1(unsigned int outputIdentifierIndex, unsigned int inputIdentifierIndex)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_subf1;
    inst.sub.inputDesc.x   = EASYMTL_INPUT_DESC_VARX;
    inst.sub.inputX.id     = inputIdentifierIndex;
    inst.sub.output        = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_subf1_c1(unsigned int outputIdentifierIndex, float x)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_subf1;
    inst.sub.inputDesc.x   = EASYMTL_INPUT_DESC_CONSTF;
    inst.sub.inputX.valuef = x;
    inst.sub.output        = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_subf2_v2(unsigned int outputIdentifierIndex, unsigned int inputIdentifierIndex)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_subf2;
    inst.sub.inputDesc.x   = EASYMTL_INPUT_DESC_VARX;
    inst.sub.inputDesc.y   = EASYMTL_INPUT_DESC_VARY;
    inst.sub.inputX.id     = inputIdentifierIndex;
    inst.sub.inputY.id     = inputIdentifierIndex;
    inst.sub.output        = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_subf2_c2(unsigned int outputIdentifierIndex, float x, float y)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_subf2;
    inst.sub.inputDesc.x   = EASYMTL_INPUT_DESC_CONSTF;
    inst.sub.inputDesc.y   = EASYMTL_INPUT_DESC_CONSTF;
    inst.sub.inputX.valuef = x;
    inst.sub.inputY.valuef = y;
    inst.sub.output        = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_subf3_v3(unsigned int outputIdentifierIndex, unsigned int inputIdentifierIndex)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_subf3;
    inst.sub.inputDesc.x   = EASYMTL_INPUT_DESC_VARX;
    inst.sub.inputDesc.y   = EASYMTL_INPUT_DESC_VARY;
    inst.sub.inputDesc.z   = EASYMTL_INPUT_DESC_VARZ;
    inst.sub.inputX.id     = inputIdentifierIndex;
    inst.sub.inputY.id     = inputIdentifierIndex;
    inst.sub.inputZ.id     = inputIdentifierIndex;
    inst.sub.output        = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_subf3_c3(unsigned int outputIdentifierIndex, float x, float y, float z)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_subf3;
    inst.sub.inputDesc.x   = EASYMTL_INPUT_DESC_CONSTF;
    inst.sub.inputDesc.y   = EASYMTL_INPUT_DESC_CONSTF;
    inst.sub.inputDesc.z   = EASYMTL_INPUT_DESC_CONSTF;
    inst.sub.inputX.valuef = x;
    inst.sub.inputY.valuef = y;
    inst.sub.inputZ.valuef = z;
    inst.sub.output        = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_subf4_v4(unsigned int outputIdentifierIndex, unsigned int inputIdentifierIndex)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_subf4;
    inst.sub.inputDesc.x   = EASYMTL_INPUT_DESC_VARX;
    inst.sub.inputDesc.y   = EASYMTL_INPUT_DESC_VARY;
    inst.sub.inputDesc.z   = EASYMTL_INPUT_DESC_VARZ;
    inst.sub.inputDesc.w   = EASYMTL_INPUT_DESC_VARW;
    inst.sub.inputX.id     = inputIdentifierIndex;
    inst.sub.inputY.id     = inputIdentifierIndex;
    inst.sub.inputZ.id     = inputIdentifierIndex;
    inst.sub.inputW.id     = inputIdentifierIndex;
    inst.sub.output        = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_subf4_c4(unsigned int outputIdentifierIndex, float x, float y, float z, float w)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_subf4;
    inst.sub.inputDesc.x   = EASYMTL_INPUT_DESC_CONSTF;
    inst.sub.inputDesc.y   = EASYMTL_INPUT_DESC_CONSTF;
    inst.sub.inputDesc.z   = EASYMTL_INPUT_DESC_CONSTF;
    inst.sub.inputDesc.w   = EASYMTL_INPUT_DESC_CONSTF;
    inst.sub.inputX.valuef = x;
    inst.sub.inputY.valuef = y;
    inst.sub.inputZ.valuef = z;
    inst.sub.inputW.valuef = w;
    inst.sub.output        = outputIdentifierIndex;

    return inst;
}


easymtl_instruction easymtl_mulf1_v1(unsigned int outputIdentifierIndex, unsigned int inputIdentifierIndex)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_mulf1;
    inst.mul.inputDesc.x   = EASYMTL_INPUT_DESC_VARX;
    inst.mul.inputX.id     = inputIdentifierIndex;
    inst.mul.output        = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_mulf1_c1(unsigned int outputIdentifierIndex, float x)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_mulf1;
    inst.mul.inputDesc.x   = EASYMTL_INPUT_DESC_CONSTF;
    inst.mul.inputX.valuef = x;
    inst.mul.output        = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_mulf2_v2(unsigned int outputIdentifierIndex, unsigned int inputIdentifierIndex)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_mulf2;
    inst.mul.inputDesc.x   = EASYMTL_INPUT_DESC_VARX;
    inst.mul.inputDesc.y   = EASYMTL_INPUT_DESC_VARY;
    inst.mul.inputX.id     = inputIdentifierIndex;
    inst.mul.inputY.id     = inputIdentifierIndex;
    inst.mul.output        = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_mulf2_c2(unsigned int outputIdentifierIndex, float x, float y)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_mulf2;
    inst.mul.inputDesc.x   = EASYMTL_INPUT_DESC_CONSTF;
    inst.mul.inputDesc.y   = EASYMTL_INPUT_DESC_CONSTF;
    inst.mul.inputX.valuef = x;
    inst.mul.inputY.valuef = y;
    inst.mul.output        = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_mulf3_v3(unsigned int outputIdentifierIndex, unsigned int inputIdentifierIndex)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_mulf3;
    inst.mul.inputDesc.x   = EASYMTL_INPUT_DESC_VARX;
    inst.mul.inputDesc.y   = EASYMTL_INPUT_DESC_VARY;
    inst.mul.inputDesc.z   = EASYMTL_INPUT_DESC_VARZ;
    inst.mul.inputX.id     = inputIdentifierIndex;
    inst.mul.inputY.id     = inputIdentifierIndex;
    inst.mul.inputZ.id     = inputIdentifierIndex;
    inst.mul.output        = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_mulf3_c3(unsigned int outputIdentifierIndex, float x, float y, float z)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_mulf3;
    inst.mul.inputDesc.x   = EASYMTL_INPUT_DESC_CONSTF;
    inst.mul.inputDesc.y   = EASYMTL_INPUT_DESC_CONSTF;
    inst.mul.inputDesc.z   = EASYMTL_INPUT_DESC_CONSTF;
    inst.mul.inputX.valuef = x;
    inst.mul.inputY.valuef = y;
    inst.mul.inputZ.valuef = z;
    inst.mul.output        = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_mulf4_v4(unsigned int outputIdentifierIndex, unsigned int inputIdentifierIndex)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_mulf4;
    inst.mul.inputDesc.x   = EASYMTL_INPUT_DESC_VARX;
    inst.mul.inputDesc.y   = EASYMTL_INPUT_DESC_VARY;
    inst.mul.inputDesc.z   = EASYMTL_INPUT_DESC_VARZ;
    inst.mul.inputDesc.w   = EASYMTL_INPUT_DESC_VARW;
    inst.mul.inputX.id     = inputIdentifierIndex;
    inst.mul.inputY.id     = inputIdentifierIndex;
    inst.mul.inputZ.id     = inputIdentifierIndex;
    inst.mul.inputW.id     = inputIdentifierIndex;
    inst.mul.output        = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_mulf4_c4(unsigned int outputIdentifierIndex, float x, float y, float z, float w)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_mulf4;
    inst.mul.inputDesc.x   = EASYMTL_INPUT_DESC_CONSTF;
    inst.mul.inputDesc.y   = EASYMTL_INPUT_DESC_CONSTF;
    inst.mul.inputDesc.z   = EASYMTL_INPUT_DESC_CONSTF;
    inst.mul.inputDesc.w   = EASYMTL_INPUT_DESC_CONSTF;
    inst.mul.inputX.valuef = x;
    inst.mul.inputY.valuef = y;
    inst.mul.inputZ.valuef = z;
    inst.mul.inputW.valuef = w;
    inst.mul.output        = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_mulf4_v3v1(unsigned int outputIdentifierIndex, unsigned int inputIdentifierIndexXYZ, unsigned int inputIdentifierIndexW)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_mulf4;
    inst.mul.inputDesc.x   = EASYMTL_INPUT_DESC_VARX;
    inst.mul.inputDesc.y   = EASYMTL_INPUT_DESC_VARY;
    inst.mul.inputDesc.z   = EASYMTL_INPUT_DESC_VARZ;
    inst.mul.inputDesc.w   = EASYMTL_INPUT_DESC_VARX;
    inst.mul.inputX.id     = inputIdentifierIndexXYZ;
    inst.mul.inputY.id     = inputIdentifierIndexXYZ;
    inst.mul.inputZ.id     = inputIdentifierIndexXYZ;
    inst.mul.inputW.id     = inputIdentifierIndexW;
    inst.mul.output        = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_mulf4_v3c1(unsigned int outputIdentifierIndex, unsigned int inputIdentifierIndex, float w)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_mulf4;
    inst.mul.inputDesc.x   = EASYMTL_INPUT_DESC_VARX;
    inst.mul.inputDesc.y   = EASYMTL_INPUT_DESC_VARY;
    inst.mul.inputDesc.z   = EASYMTL_INPUT_DESC_VARZ;
    inst.mul.inputDesc.w   = EASYMTL_INPUT_DESC_CONSTF;
    inst.mul.inputX.id     = inputIdentifierIndex;
    inst.mul.inputY.id     = inputIdentifierIndex;
    inst.mul.inputZ.id     = inputIdentifierIndex;
    inst.mul.inputW.valuef = w;
    inst.mul.output        = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_mulf4_v2c2(unsigned int outputIdentifierIndex, unsigned int inputIdentifierIndex, float z, float w)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_mulf4;
    inst.mul.inputDesc.x   = EASYMTL_INPUT_DESC_VARX;
    inst.mul.inputDesc.y   = EASYMTL_INPUT_DESC_VARY;
    inst.mul.inputDesc.z   = EASYMTL_INPUT_DESC_CONSTF;
    inst.mul.inputDesc.w   = EASYMTL_INPUT_DESC_CONSTF;
    inst.mul.inputX.id     = inputIdentifierIndex;
    inst.mul.inputY.id     = inputIdentifierIndex;
    inst.mul.inputZ.valuef = z;
    inst.mul.inputW.valuef = w;
    inst.mul.output        = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_mulf4_v1c3(unsigned int outputIdentifierIndex, unsigned int inputIdentifierIndex, float y, float z, float w)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_mulf4;
    inst.mul.inputDesc.x   = EASYMTL_INPUT_DESC_VARX;
    inst.mul.inputDesc.y   = EASYMTL_INPUT_DESC_CONSTF;
    inst.mul.inputDesc.z   = EASYMTL_INPUT_DESC_CONSTF;
    inst.mul.inputDesc.w   = EASYMTL_INPUT_DESC_CONSTF;
    inst.mul.inputX.id     = inputIdentifierIndex;
    inst.mul.inputY.valuef = y;
    inst.mul.inputZ.valuef = z;
    inst.mul.inputW.valuef = w;
    inst.mul.output        = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_mulf4_v1v1v1v1(unsigned int outputIdentifierIndex, unsigned int inputIdentifierIndexX, unsigned int inputIdentifierIndexY, unsigned int inputIdentifierIndexZ, unsigned int inputIdentifierIndexW)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_mulf4;
    inst.mul.inputDesc.x   = EASYMTL_INPUT_DESC_VARX;
    inst.mul.inputDesc.y   = EASYMTL_INPUT_DESC_VARX;
    inst.mul.inputDesc.z   = EASYMTL_INPUT_DESC_VARX;
    inst.mul.inputDesc.w   = EASYMTL_INPUT_DESC_VARX;
    inst.mul.inputX.id     = inputIdentifierIndexX;
    inst.mul.inputY.id     = inputIdentifierIndexY;
    inst.mul.inputZ.id     = inputIdentifierIndexZ;
    inst.mul.inputW.id     = inputIdentifierIndexW;
    inst.mul.output        = outputIdentifierIndex;

    return inst;
}


easymtl_instruction easymtl_divf1_v1(unsigned int outputIdentifierIndex, unsigned int inputIdentifierIndex)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_divf1;
    inst.div.inputDesc.x   = EASYMTL_INPUT_DESC_VARX;
    inst.div.inputX.id     = inputIdentifierIndex;
    inst.div.output        = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_divf1_c1(unsigned int outputIdentifierIndex, float x)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_divf1;
    inst.div.inputDesc.x   = EASYMTL_INPUT_DESC_CONSTF;
    inst.div.inputX.valuef = x;
    inst.div.output        = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_divf2_v2(unsigned int outputIdentifierIndex, unsigned int inputIdentifierIndex)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_divf2;
    inst.div.inputDesc.x   = EASYMTL_INPUT_DESC_VARX;
    inst.div.inputDesc.y   = EASYMTL_INPUT_DESC_VARY;
    inst.div.inputX.id     = inputIdentifierIndex;
    inst.div.inputY.id     = inputIdentifierIndex;
    inst.div.output        = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_divf2_c2(unsigned int outputIdentifierIndex, float x, float y)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_divf2;
    inst.div.inputDesc.x   = EASYMTL_INPUT_DESC_CONSTF;
    inst.div.inputDesc.y   = EASYMTL_INPUT_DESC_CONSTF;
    inst.div.inputX.valuef = x;
    inst.div.inputY.valuef = y;
    inst.div.output        = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_divf3_v3(unsigned int outputIdentifierIndex, unsigned int inputIdentifierIndex)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_divf3;
    inst.div.inputDesc.x   = EASYMTL_INPUT_DESC_VARX;
    inst.div.inputDesc.y   = EASYMTL_INPUT_DESC_VARY;
    inst.div.inputDesc.z   = EASYMTL_INPUT_DESC_VARZ;
    inst.div.inputX.id     = inputIdentifierIndex;
    inst.div.inputY.id     = inputIdentifierIndex;
    inst.div.inputZ.id     = inputIdentifierIndex;
    inst.div.output        = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_divf3_c3(unsigned int outputIdentifierIndex, float x, float y, float z)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_divf3;
    inst.div.inputDesc.x   = EASYMTL_INPUT_DESC_CONSTF;
    inst.div.inputDesc.y   = EASYMTL_INPUT_DESC_CONSTF;
    inst.div.inputDesc.z   = EASYMTL_INPUT_DESC_CONSTF;
    inst.div.inputX.valuef = x;
    inst.div.inputY.valuef = y;
    inst.div.inputZ.valuef = z;
    inst.div.output        = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_divf4_v4(unsigned int outputIdentifierIndex, unsigned int inputIdentifierIndex)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_divf4;
    inst.div.inputDesc.x   = EASYMTL_INPUT_DESC_VARX;
    inst.div.inputDesc.y   = EASYMTL_INPUT_DESC_VARY;
    inst.div.inputDesc.z   = EASYMTL_INPUT_DESC_VARZ;
    inst.div.inputDesc.w   = EASYMTL_INPUT_DESC_VARW;
    inst.div.inputX.id     = inputIdentifierIndex;
    inst.div.inputY.id     = inputIdentifierIndex;
    inst.div.inputZ.id     = inputIdentifierIndex;
    inst.div.inputW.id     = inputIdentifierIndex;
    inst.div.output        = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_divf4_c4(unsigned int outputIdentifierIndex, float x, float y, float z, float w)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_divf4;
    inst.div.inputDesc.x   = EASYMTL_INPUT_DESC_CONSTF;
    inst.div.inputDesc.y   = EASYMTL_INPUT_DESC_CONSTF;
    inst.div.inputDesc.z   = EASYMTL_INPUT_DESC_CONSTF;
    inst.div.inputDesc.w   = EASYMTL_INPUT_DESC_CONSTF;
    inst.div.inputX.valuef = x;
    inst.div.inputY.valuef = y;
    inst.div.inputZ.valuef = z;
    inst.div.inputW.valuef = w;
    inst.div.output        = outputIdentifierIndex;

    return inst;
}


easymtl_instruction easymtl_tex2(unsigned int outputIdentifierIndex, unsigned int textureIdentifierIndex, unsigned int texcoordIdentifierIndex)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_tex2;
    inst.tex.inputDesc.x = EASYMTL_INPUT_DESC_VARX;
    inst.tex.inputDesc.y = EASYMTL_INPUT_DESC_VARY;
    inst.tex.inputX.id   = texcoordIdentifierIndex;
    inst.tex.inputY.id   = texcoordIdentifierIndex;
    inst.tex.texture     = textureIdentifierIndex;
    inst.tex.output      = outputIdentifierIndex;

    return inst;
}

easymtl_instruction easymtl_var(unsigned int identifierIndex)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_var;
    inst.var.identifierIndex = identifierIndex;

    return inst;
}

easymtl_instruction easymtl_retf1(unsigned int identifierIndex)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_retf1;
    inst.ret.inputDesc.x = EASYMTL_INPUT_DESC_VARX;
    inst.ret.inputX.id = identifierIndex;

    return inst;
}

easymtl_instruction easymtl_retf2(unsigned int identifierIndex)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_retf2;
    inst.ret.inputDesc.x = EASYMTL_INPUT_DESC_VARX;
    inst.ret.inputDesc.y = EASYMTL_INPUT_DESC_VARY;
    inst.ret.inputX.id = identifierIndex;
    inst.ret.inputY.id = identifierIndex;

    return inst;
}

easymtl_instruction easymtl_retf3(unsigned int identifierIndex)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_retf3;
    inst.ret.inputDesc.x = EASYMTL_INPUT_DESC_VARX;
    inst.ret.inputDesc.y = EASYMTL_INPUT_DESC_VARY;
    inst.ret.inputDesc.z = EASYMTL_INPUT_DESC_VARZ;
    inst.ret.inputX.id = identifierIndex;
    inst.ret.inputY.id = identifierIndex;
    inst.ret.inputZ.id = identifierIndex;

    return inst;
}

easymtl_instruction easymtl_retf4(unsigned int identifierIndex)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_retf4;
    inst.ret.inputDesc.x = EASYMTL_INPUT_DESC_VARX;
    inst.ret.inputDesc.y = EASYMTL_INPUT_DESC_VARY;
    inst.ret.inputDesc.z = EASYMTL_INPUT_DESC_VARZ;
    inst.ret.inputDesc.w = EASYMTL_INPUT_DESC_VARW;
    inst.ret.inputX.id = identifierIndex;
    inst.ret.inputY.id = identifierIndex;
    inst.ret.inputZ.id = identifierIndex;
    inst.ret.inputW.id = identifierIndex;

    return inst;
}

easymtl_instruction easymtl_retf1_c1(float x)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_retf1;
    inst.ret.inputDesc.x = EASYMTL_INPUT_DESC_CONSTF;
    inst.ret.inputX.valuef = x;

    return inst;
}

easymtl_instruction easymtl_retf2_c2(float x, float y)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_retf2;
    inst.ret.inputDesc.x = EASYMTL_INPUT_DESC_CONSTF;
    inst.ret.inputDesc.y = EASYMTL_INPUT_DESC_CONSTF;
    inst.ret.inputX.valuef = x;
    inst.ret.inputY.valuef = y;

    return inst;
}

easymtl_instruction easymtl_retf3_c3(float x, float y, float z)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_retf3;
    inst.ret.inputDesc.x = EASYMTL_INPUT_DESC_CONSTF;
    inst.ret.inputDesc.y = EASYMTL_INPUT_DESC_CONSTF;
    inst.ret.inputDesc.z = EASYMTL_INPUT_DESC_CONSTF;
    inst.ret.inputX.valuef = x;
    inst.ret.inputY.valuef = y;
    inst.ret.inputZ.valuef = z;

    return inst;
}

easymtl_instruction easymtl_retf4_c4(float x, float y, float z, float w)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_retf4;
    inst.ret.inputDesc.x = EASYMTL_INPUT_DESC_CONSTF;
    inst.ret.inputDesc.y = EASYMTL_INPUT_DESC_CONSTF;
    inst.ret.inputDesc.z = EASYMTL_INPUT_DESC_CONSTF;
    inst.ret.inputDesc.w = EASYMTL_INPUT_DESC_CONSTF;
    inst.ret.inputX.valuef = x;
    inst.ret.inputY.valuef = y;
    inst.ret.inputZ.valuef = z;
    inst.ret.inputW.valuef = w;

    return inst;
}

easymtl_instruction easymtl_reti1(unsigned int identifierIndex)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_reti1;
    inst.ret.inputDesc.x = EASYMTL_INPUT_DESC_VARX;
    inst.ret.inputX.id = identifierIndex;

    return inst;
}

easymtl_instruction easymtl_reti2(unsigned int identifierIndex)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_reti2;
    inst.ret.inputDesc.x = EASYMTL_INPUT_DESC_VARX;
    inst.ret.inputDesc.y = EASYMTL_INPUT_DESC_VARY;
    inst.ret.inputX.id = identifierIndex;
    inst.ret.inputY.id = identifierIndex;

    return inst;
}

easymtl_instruction easymtl_reti3(unsigned int identifierIndex)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_reti3;
    inst.ret.inputDesc.x = EASYMTL_INPUT_DESC_VARX;
    inst.ret.inputDesc.y = EASYMTL_INPUT_DESC_VARY;
    inst.ret.inputDesc.z = EASYMTL_INPUT_DESC_VARZ;
    inst.ret.inputX.id = identifierIndex;
    inst.ret.inputY.id = identifierIndex;
    inst.ret.inputZ.id = identifierIndex;

    return inst;
}

easymtl_instruction easymtl_reti4(unsigned int identifierIndex)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_reti4;
    inst.ret.inputDesc.x = EASYMTL_INPUT_DESC_VARX;
    inst.ret.inputDesc.y = EASYMTL_INPUT_DESC_VARY;
    inst.ret.inputDesc.z = EASYMTL_INPUT_DESC_VARZ;
    inst.ret.inputDesc.w = EASYMTL_INPUT_DESC_VARW;
    inst.ret.inputX.id = identifierIndex;
    inst.ret.inputY.id = identifierIndex;
    inst.ret.inputZ.id = identifierIndex;
    inst.ret.inputW.id = identifierIndex;

    return inst;
}

easymtl_instruction easymtl_reti1_c1(int x)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_reti1;
    inst.ret.inputDesc.x = EASYMTL_INPUT_DESC_CONSTI;
    inst.ret.inputX.valuei = x;

    return inst;
}

easymtl_instruction easymtl_reti2_c2(int x, int y)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_reti2;
    inst.ret.inputDesc.x = EASYMTL_INPUT_DESC_CONSTI;
    inst.ret.inputDesc.y = EASYMTL_INPUT_DESC_CONSTI;
    inst.ret.inputX.valuei = x;
    inst.ret.inputY.valuei = y;

    return inst;
}

easymtl_instruction easymtl_reti3_c3(int x, int y, int z)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_reti3;
    inst.ret.inputDesc.x = EASYMTL_INPUT_DESC_CONSTI;
    inst.ret.inputDesc.y = EASYMTL_INPUT_DESC_CONSTI;
    inst.ret.inputDesc.z = EASYMTL_INPUT_DESC_CONSTI;
    inst.ret.inputX.valuei = x;
    inst.ret.inputY.valuei = y;
    inst.ret.inputZ.valuei = z;

    return inst;
}

easymtl_instruction easymtl_reti4_c4(int x, int y, int z, int w)
{
    easymtl_instruction inst;
    inst.opcode = easymtl_opcode_reti4;
    inst.ret.inputDesc.x = EASYMTL_INPUT_DESC_CONSTI;
    inst.ret.inputDesc.y = EASYMTL_INPUT_DESC_CONSTI;
    inst.ret.inputDesc.z = EASYMTL_INPUT_DESC_CONSTI;
    inst.ret.inputDesc.w = EASYMTL_INPUT_DESC_CONSTI;
    inst.ret.inputX.valuei = x;
    inst.ret.inputY.valuei = y;
    inst.ret.inputZ.valuei = z;
    inst.ret.inputW.valuei = w;

    return inst;
}



easymtl_property easymtl_property_float(const char* name, float x)
{
    easymtl_property prop;
    prop.type = easymtl_type_float;
    easymtl_strcpy(prop.name, EASYMTL_MAX_PROPERTY_NAME, name);
    prop.f1.x = x;

    return prop;
}

easymtl_property easymtl_property_float2(const char* name, float x, float y)
{
    easymtl_property prop;
    prop.type = easymtl_type_float2;
    easymtl_strcpy(prop.name, EASYMTL_MAX_PROPERTY_NAME, name);
    prop.f2.x = x;
    prop.f2.y = y;

    return prop;
}

easymtl_property easymtl_property_float3(const char* name, float x, float y, float z)
{
    easymtl_property prop;
    prop.type = easymtl_type_float3;
    easymtl_strcpy(prop.name, EASYMTL_MAX_PROPERTY_NAME, name);
    prop.f3.x = x;
    prop.f3.y = y;
    prop.f3.z = z;

    return prop;
}

easymtl_property easymtl_property_float4(const char* name, float x, float y, float z, float w)
{
    easymtl_property prop;
    prop.type = easymtl_type_float4;
    easymtl_strcpy(prop.name, EASYMTL_MAX_PROPERTY_NAME, name);
    prop.f4.x = x;
    prop.f4.y = y;
    prop.f4.z = z;
    prop.f4.w = w;

    return prop;
}

easymtl_property easymtl_property_int(const char* name, int x)
{
    easymtl_property prop;
    prop.type = easymtl_type_int;
    easymtl_strcpy(prop.name, EASYMTL_MAX_PROPERTY_NAME, name);
    prop.i1.x = x;

    return prop;
}

easymtl_property easymtl_property_int2(const char* name, int x, int y)
{
    easymtl_property prop;
    prop.type = easymtl_type_int2;
    easymtl_strcpy(prop.name, EASYMTL_MAX_PROPERTY_NAME, name);
    prop.i2.x = x;
    prop.i2.y = y;

    return prop;
}

easymtl_property easymtl_property_int3(const char* name, int x, int y, int z)
{
    easymtl_property prop;
    prop.type = easymtl_type_int3;
    easymtl_strcpy(prop.name, EASYMTL_MAX_PROPERTY_NAME, name);
    prop.i3.x = x;
    prop.i3.y = y;
    prop.i3.z = z;

    return prop;
}

easymtl_property easymtl_property_int4(const char* name, int x, int y, int z, int w)
{
    easymtl_property prop;
    prop.type = easymtl_type_int4;
    easymtl_strcpy(prop.name, EASYMTL_MAX_PROPERTY_NAME, name);
    prop.i4.x = x;
    prop.i4.y = y;
    prop.i4.z = z;
    prop.i4.w = w;

    return prop;
}

easymtl_property easymtl_property_bool(const char* name, bool value)
{
    easymtl_property prop;
    prop.type = easymtl_type_bool;
    easymtl_strcpy(prop.name, EASYMTL_MAX_PROPERTY_NAME, name);
    prop.b1.x = value;

    return prop;
}





///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
// Compilers
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#ifndef EASYMTL_NO_MTL_COMPILER
typedef struct
{
    /// A pointer to the buffer containing the Wavefront MTL data.
    const char* pData;

    /// The size of the data buffer size.
    unsigned int dataSizeInBytes;

    /// A pointer to the next bytes to read.
    const char* pDataCur;

    /// A pointer to the end of the buffer.
    const char* pDataEnd;


    /// The diffuse colour.
    float diffuse[3];

    /// The diffuse map.
    char diffuseMap[EASYMTL_MAX_INPUT_PATH];


    /// The specular colour.
    float specular[3];

    /// The specular map.
    char specularMap[EASYMTL_MAX_INPUT_PATH];


    /// The specular exponent.
    float specularExponent;

    /// The specular exponent map.
    char specularExponentMap[EASYMTL_MAX_INPUT_PATH];


    /// The alpha transparency value.
    float alpha;

    /// The alpha transparency map.
    char alphaMap[EASYMTL_MAX_INPUT_PATH];


} easymtl_wavefront;

bool easymtl_wavefront_is_whitespace(char c)
{
    return c == ' ' || c == '\t';
}

bool easymtl_wavefront_is_valid_digit(char c)
{
    return c >= '0' && c <= '9';
}

bool easymtl_wavefront_atof(const char* str, const char* strEnd, const char** strEndOut, float* valueOut)
{
    // Skip leading whitespace.
    while (str < strEnd && easymtl_wavefront_is_whitespace(*str))
    {
        str += 1;
    }


    // Check that we have a string after moving the whitespace.
    if (str < strEnd)
    {
        float sign  = 1.0f;
        float value = 0.0f;

        // Sign.
        if (*str == '-')
        {
            sign = -1.0f;
            str += 1;
        }
        else if (*str == '+')
        {
            sign = 1.0f;
            str += 1;
        }


        // Digits before the decimal point.
        while (str < strEnd && easymtl_wavefront_is_valid_digit(*str))
        {
            value = value * 10.0f + (*str - '0');

            str += 1;
        }

        // Digits after the decimal point.
        if (*str == '.')
        {
            float pow10 = 10.0f;

            str += 1;
            while (str < strEnd && easymtl_wavefront_is_valid_digit(*str))
            {
                value += (*str - '0') / pow10;
                pow10 *= 10.0f;

                str += 1;
            }
        }

            
        if (strEndOut != NULL)
        {
            *strEndOut = str;
        }

        if (valueOut != NULL)
        {
            *valueOut = sign * value;
        }

        return 1;
    }
    else
    {
        // Empty string. Leave output untouched and return 0.
        return 0;
    }
}

bool easymtl_wavefront_atof_3(const char* str, const char* strEnd, const char** strEndOut, float valueOut[3])
{
    float value[3];
    if (easymtl_wavefront_atof(str, strEnd, &str, &value[0]))
    {
        value[1] = value[0];
        value[2] = value[0];

        if (easymtl_wavefront_atof(str, strEnd, &str, &value[1]))
        {
            // We got two numbers which means we must have the third for this to be successful.
            if (!easymtl_wavefront_atof(str, strEnd, strEndOut, &value[2]))
            {
                // Failed to get the third number. We only found 2 which is not valid. Error.
                return 0;
            }
        }
        

        valueOut[0] = value[0];
        valueOut[1] = value[1];
        valueOut[2] = value[2];

        return 1;
    }

    return 0;
}

const char* easymtl_wavefront_find_end_of_line(const char* pDataCur, const char* pDataEnd)
{
    assert(pDataCur != NULL);
    assert(pDataEnd != NULL);

    while (pDataCur < pDataEnd)
    {
        if (pDataCur[0] == '\n')
        {
            return pDataCur;
        }
        else
        {
            if (pDataCur + 1 < pDataEnd)
            {
                if (pDataCur[0] == '\r' && pDataCur[1] == '\n')
                {
                    return pDataCur;
                }
            }
        }

        pDataCur += 1;
    }

    // If we get here it means we hit the end of the file before find a new-line character.
    return pDataEnd;
}

const char* easymtl_wavefront_find_next_line(const char* pDataCur, const char* pDataEnd)
{
    assert(pDataCur != NULL);
    assert(pDataEnd != NULL);

    pDataCur = easymtl_wavefront_find_end_of_line(pDataCur, pDataEnd);
    if (pDataCur != NULL)
    {
        if (pDataCur < pDataEnd)
        {
            if (pDataCur[0] == '\n')
            {
                return pDataCur + 1;
            }
            else
            {
                if (pDataCur + 1 < pDataEnd)
                {
                    if (pDataCur[0] == '\r' && pDataCur[1] == '\n')
                    {
                        return pDataCur + 2;
                    }
                }
            }
        }
    }

    return NULL;
}

const char* easymtl_wavefront_find_next_newmtl(const char* pDataCur, const char* pDataEnd)
{
    assert(pDataCur != NULL);
    assert(pDataEnd != NULL);

    while (pDataCur + 7 < pDataEnd)   // +7 for "newmtl" + a whitespace character.
    {
        if (pDataCur[0] == 'n' && pDataCur[1] == 'e' && pDataCur[2] == 'w' && pDataCur[3] == 'm' && pDataCur[4] == 't' && pDataCur[5] == 'l')
        {
            // We found "newmtl", however the next line must be whitespace.
            if (easymtl_wavefront_is_whitespace(pDataCur[6]))
            {
                // We found it.
                return pDataCur;
            }
        }


        const char* nextLineStart = easymtl_wavefront_find_next_line(pDataCur, pDataEnd);
        if (nextLineStart != NULL)
        {
            pDataCur = nextLineStart;
        }
        else
        {
            // Reached the end before finding "newmtl". Return null.
            return NULL;
        }
    }

    return NULL;
}

const char* easymtl_wavefront_find_next_nonwhitespace(const char* pDataCur, const char* pDataEnd)
{
    assert(pDataCur != NULL);
    assert(pDataEnd != NULL);

    while (pDataCur < pDataEnd)
    {
        if (!easymtl_wavefront_is_whitespace(pDataCur[0]))
        {
            return pDataCur;
        }

        pDataCur += 1;
    }

    return NULL;
}


bool easymtl_wavefront_parse_K(const char* pDataCur, const char* pDataEnd, float valueOut[3])
{
    assert(pDataCur != NULL);
    assert(pDataEnd != NULL);

    return easymtl_wavefront_atof_3(pDataCur, pDataEnd, &pDataEnd, valueOut);
}

bool easymtl_wavefront_parse_N(const char* pDataCur, const char* pDataEnd, float* valueOut)
{
    assert(pDataCur != NULL);
    assert(pDataEnd != NULL);
    
    return easymtl_wavefront_atof(pDataCur, pDataEnd, &pDataEnd, valueOut);
}

bool easymtl_wavefront_parse_map(const char* pDataCur, const char* pDataEnd, char* pathOut, unsigned int pathSizeInBytes)
{
    assert(pDataCur != NULL);
    assert(pDataEnd != NULL);

    // For now we're not supporting options, however support for that will be added later.

    const char* pPathStart = easymtl_wavefront_find_next_nonwhitespace(pDataCur, pDataEnd);
    if (pPathStart != NULL)
    {
        if (pPathStart < pDataEnd)
        {
            if (pPathStart[0] != '#')
            {
                // Find the last non-whitespace, making sure we don't include comments.
                pDataCur = pPathStart;
                const char* pPathEnd = pDataCur;
                while (pDataCur < pDataEnd && pDataCur[0] != '#')
                {
                    if (!easymtl_wavefront_is_whitespace(pDataCur[0]))
                    {
                        pPathEnd = pDataCur + 1;
                    }

                    pDataCur += 1;
                }

                assert(pPathStart < pPathEnd);

                ptrdiff_t pathLength = pPathEnd - pPathStart;
                if ((size_t)pathLength + 1 < pathSizeInBytes)
                {
                    memcpy(pathOut, pPathStart, (size_t)pathLength);
                    pathOut[pathLength] = '\0';

                    return 1;
                }
            }
        }
    }

    return 0;
}


bool easymtl_wavefront_seek_to_next_line(easymtl_wavefront* pWavefront)
{
    assert(pWavefront != NULL);

    const char* lineStart = easymtl_wavefront_find_next_line(pWavefront->pDataCur, pWavefront->pDataEnd);
    if (lineStart != NULL)
    {
        pWavefront->pDataCur = lineStart;
        return 1;
    }

    return 0;
}

bool easymtl_wavefront_seek_to_newmtl(easymtl_wavefront* pWavefront)
{
    assert(pWavefront != NULL);

    const char* usemtl = easymtl_wavefront_find_next_newmtl(pWavefront->pDataCur, pWavefront->pDataEnd);
    if (usemtl != NULL)
    {
        pWavefront->pDataCur = usemtl;
        return 1;
    }

    return 0;
}

bool easymtl_wavefront_parse(easymtl_wavefront* pWavefront)
{
    assert(pWavefront != NULL);

    if (easymtl_wavefront_seek_to_newmtl(pWavefront) && easymtl_wavefront_seek_to_next_line(pWavefront))
    {
        // Set the end of the material to the start of the second usemtl statement, if it exists.
        const char* usemtl2 = easymtl_wavefront_find_next_newmtl(pWavefront->pDataCur, pWavefront->pDataEnd);
        if (usemtl2 != NULL)
        {
            pWavefront->pDataEnd = usemtl2;
        }


        while (pWavefront->pDataCur < pWavefront->pDataEnd)
        {
            const char* lineCur = pWavefront->pDataCur;
            const char* lineEnd = easymtl_wavefront_find_end_of_line(lineCur, pWavefront->pDataEnd);

            lineCur = easymtl_wavefront_find_next_nonwhitespace(lineCur, lineEnd);
            if (lineCur != NULL && (lineCur + 2 < lineEnd))
            {
                if (lineCur[0] == 'K' && lineCur[1] == 'd' && easymtl_wavefront_is_whitespace(lineCur[2]))          // Diffuse colour
                {
                    lineCur += 3;
                    easymtl_wavefront_parse_K(lineCur, lineEnd, pWavefront->diffuse);
                }
                else if (lineCur[0] == 'K' && lineCur[1] == 's' && easymtl_wavefront_is_whitespace(lineCur[2]))     // Specular colour
                {
                    lineCur += 3;
                    easymtl_wavefront_parse_K(lineCur, lineEnd, pWavefront->specular);
                }
                else if (lineCur[0] == 'N' && lineCur[1] == 's' && easymtl_wavefront_is_whitespace(lineCur[2]))     // Specular exponent
                {
                    lineCur += 3;
                    easymtl_wavefront_parse_N(lineCur, lineEnd, &pWavefront->specularExponent);
                }
                else if (lineCur[0] == 'd' && easymtl_wavefront_is_whitespace(lineCur[1]))                          // Opacity/Alpha
                {
                    lineCur += 2;
                    easymtl_wavefront_parse_N(lineCur, lineEnd, &pWavefront->alpha);
                }
                else
                {
                    // Check for maps.
                    if (lineCur + 6 < lineEnd)
                    {
                        if (lineCur[0] == 'm' && lineCur[1] == 'a' && lineCur[2] == 'p' && lineCur[3] == '_')
                        {
                            if (lineCur[4] == 'K' && lineCur[5] == 'd' && easymtl_wavefront_is_whitespace(lineCur[6]))          // Diffuse map
                            {
                                lineCur += 7;
                                easymtl_wavefront_parse_map(lineCur, lineEnd, pWavefront->diffuseMap, EASYMTL_MAX_INPUT_PATH);
                            }
                            else if (lineCur[4] == 'K' && lineCur[5] == 's' && easymtl_wavefront_is_whitespace(lineCur[6]))     // Specular map
                            {
                                lineCur += 7;
                                easymtl_wavefront_parse_map(lineCur, lineEnd, pWavefront->specularMap, EASYMTL_MAX_INPUT_PATH);
                            }
                            else if (lineCur[4] == 'N' && lineCur[5] == 's' && easymtl_wavefront_is_whitespace(lineCur[6]))     // Specular exponent map
                            {
                                lineCur += 7;
                                easymtl_wavefront_parse_map(lineCur, lineEnd, pWavefront->specularExponentMap, EASYMTL_MAX_INPUT_PATH);
                            }
                            else if (lineCur[4] == 'd' && easymtl_wavefront_is_whitespace(lineCur[5]))                          // Opacity/Alpha map
                            {
                                lineCur += 6;
                                easymtl_wavefront_parse_map(lineCur, lineEnd, pWavefront->alphaMap, EASYMTL_MAX_INPUT_PATH);
                            }
                        }
                    }
                }
            }


            // Move to the end of the line.
            pWavefront->pDataCur = lineEnd;

            // Move to the start of the next line. If this fails it probably means we've reached the end of the data so we just break from the loop
            if (!easymtl_wavefront_seek_to_next_line(pWavefront))
            {
                break;
            }
        }


        return 1;
    }

    return 0;
}

bool easymtl_wavefront_compile(easymtl_material* pMaterial, easymtl_wavefront* pWavefront, const char* texcoordInputName)
{
    assert(pMaterial  != NULL);
    assert(pWavefront != NULL);

    unsigned int texCoordID;    // Private input for texture coordinates.
    unsigned int diffuseID;
    unsigned int specularID;
    unsigned int specularExponentID;
    unsigned int alphaID;
    unsigned int diffuseMapID = (unsigned int)-1;
    unsigned int specularMapID = (unsigned int)-1;
    unsigned int specularExponentMapID = (unsigned int)-1;
    unsigned int alphaMapID = (unsigned int)-1;
    unsigned int diffuseResultID = (unsigned int)-1;
    unsigned int specularResultID = (unsigned int)-1;
    unsigned int specularExponentResultID = (unsigned int)-1;
    unsigned int alphaResultID = (unsigned int)-1;

    
    // Identifiers.
    easymtl_appendidentifier(pMaterial, easymtl_identifier_float2(texcoordInputName), &texCoordID);
    easymtl_appendidentifier(pMaterial, easymtl_identifier_float4("DiffuseColor"), &diffuseID);
    easymtl_appendidentifier(pMaterial, easymtl_identifier_float3("SpecularColor"), &specularID);
    easymtl_appendidentifier(pMaterial, easymtl_identifier_float("SpecularExponent"), &specularExponentID);
    easymtl_appendidentifier(pMaterial, easymtl_identifier_float("Alpha"), &alphaID);
    
    if (pWavefront->diffuseMap[0] != '\0') {
        easymtl_appendidentifier(pMaterial, easymtl_identifier_tex2d("DiffuseMap"), &diffuseMapID);
        easymtl_appendidentifier(pMaterial, easymtl_identifier_float4("DiffuseResult"), &diffuseResultID);
    }
    if (pWavefront->specularMap[0] != '\0') {
        easymtl_appendidentifier(pMaterial, easymtl_identifier_tex2d("SpecularMap"), &specularMapID);
        easymtl_appendidentifier(pMaterial, easymtl_identifier_float4("SpecularResult"), &specularResultID);
    }
    if (pWavefront->specularExponentMap[0] != '\0') {
        easymtl_appendidentifier(pMaterial, easymtl_identifier_tex2d("SpecularExponentMap"), &specularExponentMapID);
        easymtl_appendidentifier(pMaterial, easymtl_identifier_float4("SpecularExponentResult"), &specularExponentResultID);
    }
    if (pWavefront->alphaMap[0] != '\0') {
        easymtl_appendidentifier(pMaterial, easymtl_identifier_tex2d("AlphaMap"), &alphaMapID);
        easymtl_appendidentifier(pMaterial, easymtl_identifier_float4("AlphaResult"), &alphaResultID);
    }


    // Inputs.
    easymtl_appendprivateinput(pMaterial, easymtl_input_float2(texCoordID, 0, 0));
    easymtl_appendpublicinput(pMaterial, easymtl_input_float4(diffuseID, pWavefront->diffuse[0], pWavefront->diffuse[1], pWavefront->diffuse[2], 1.0f));
    easymtl_appendpublicinput(pMaterial, easymtl_input_float3(specularID, pWavefront->specular[0], pWavefront->specular[1], pWavefront->specular[2]));
    easymtl_appendpublicinput(pMaterial, easymtl_input_float(specularExponentID, pWavefront->specularExponent));
    easymtl_appendpublicinput(pMaterial, easymtl_input_float(alphaID, pWavefront->alpha));
    
    if (pWavefront->diffuseMap[0] != '\0') {
        easymtl_appendpublicinput(pMaterial, easymtl_input_tex(diffuseMapID, pWavefront->diffuseMap));
    }
    if (pWavefront->specularMap[0] != '\0') {
        easymtl_appendpublicinput(pMaterial, easymtl_input_tex(specularMapID, pWavefront->specularMap));
    }
    if (pWavefront->specularExponentMap[0] != '\0') {
        easymtl_appendpublicinput(pMaterial, easymtl_input_tex(specularExponentMapID, pWavefront->specularExponentMap));
    }
    if (pWavefront->alphaMap[0] != '\0') {
        easymtl_appendpublicinput(pMaterial, easymtl_input_tex(alphaMapID, pWavefront->alphaMap));
    }


    // Channels.
    easymtl_appendchannel(pMaterial, easymtl_channel_float4("DiffuseChannel"));
    if (pWavefront->diffuseMap[0] != '\0') {
        easymtl_appendinstruction(pMaterial, easymtl_var(diffuseResultID));
        easymtl_appendinstruction(pMaterial, easymtl_tex2(diffuseResultID, diffuseMapID, texCoordID));
        easymtl_appendinstruction(pMaterial, easymtl_mulf4_v3c1(diffuseResultID, diffuseID, 1.0f));
        easymtl_appendinstruction(pMaterial, easymtl_retf4(diffuseResultID));
    } else {
        easymtl_appendinstruction(pMaterial, easymtl_retf4(diffuseID));
    }

    easymtl_appendchannel(pMaterial, easymtl_channel_float3("SpecularChannel"));
    if (pWavefront->specularMap[0] != '\0') {
        easymtl_appendinstruction(pMaterial, easymtl_var(specularResultID));
        easymtl_appendinstruction(pMaterial, easymtl_tex2(specularResultID, specularMapID, texCoordID));
        easymtl_appendinstruction(pMaterial, easymtl_mulf4_v3c1(specularResultID, specularID, 1.0f));
        easymtl_appendinstruction(pMaterial, easymtl_retf3(specularResultID));
    } else {
        easymtl_appendinstruction(pMaterial, easymtl_retf3(specularID));
    }

    easymtl_appendchannel(pMaterial, easymtl_channel_float("SpecularExponentChannel"));
    if (pWavefront->specularExponentMap[0] != '\0') {
        easymtl_appendinstruction(pMaterial, easymtl_var(specularExponentResultID));
        easymtl_appendinstruction(pMaterial, easymtl_tex2(specularResultID, specularMapID, texCoordID));
        easymtl_appendinstruction(pMaterial, easymtl_mulf4_v1c3(specularResultID, specularID, 1.0f, 1.0f, 1.0f));
        easymtl_appendinstruction(pMaterial, easymtl_retf1(specularResultID));
    } else {
        easymtl_appendinstruction(pMaterial, easymtl_retf1(specularExponentID));
    }
    
    easymtl_appendchannel(pMaterial, easymtl_channel_float("AlphaChannel"));
    if (pWavefront->alphaMap[0] != '\0') {
        easymtl_appendinstruction(pMaterial, easymtl_var(alphaResultID));
        easymtl_appendinstruction(pMaterial, easymtl_tex2(alphaResultID, alphaMapID, texCoordID));
        easymtl_appendinstruction(pMaterial, easymtl_mulf4_v1c3(alphaResultID, alphaID, 1.0f, 1.0f, 1.0f));
        easymtl_appendinstruction(pMaterial, easymtl_retf1(alphaResultID));
    } else {
        easymtl_appendinstruction(pMaterial, easymtl_retf1(alphaID));
    }



    // Properties.
    if (pWavefront->alphaMap[0] != '\0' || pWavefront->alpha < 1)
    {
        easymtl_appendproperty(pMaterial, easymtl_property_bool("IsTransparent", 1));
    }

    return 1;
}


bool easymtl_compile_wavefront_mtl(easymtl_material* pMaterial, const char* mtlData, unsigned int mtlDataSizeInBytes, const char* texcoordInputName)
{
    if (pMaterial != NULL && mtlData != NULL && mtlDataSizeInBytes > 0)
    {
        if (easymtl_init(pMaterial))
        {
            easymtl_wavefront wavefront;
            wavefront.pData                  = mtlData;
            wavefront.dataSizeInBytes        = mtlDataSizeInBytes;
            wavefront.pDataCur               = wavefront.pData;
            wavefront.pDataEnd               = wavefront.pData + wavefront.dataSizeInBytes;
            wavefront.diffuse[0]             = 1; wavefront.diffuse[1] = 1;  wavefront.diffuse[2] = 1;
            wavefront.diffuseMap[0]          = '\0';
            wavefront.specular[0]            = 1; wavefront.specular[1] = 1; wavefront.specular[2] = 1;
            wavefront.specularMap[0]         = '\0';
            wavefront.specularExponent       = 10;
            wavefront.specularExponentMap[0] = '\0';
            wavefront.alpha                  = 1;
            wavefront.alphaMap[0]            = '\0';

            if (easymtl_wavefront_parse(&wavefront))
            {
                if (easymtl_wavefront_compile(pMaterial, &wavefront, texcoordInputName))
                {
                    return 1;
                }
                else
                {
                    // Failed to compile.
                    easymtl_uninit(pMaterial);
                }
            }
            else
            {
                // Failed to parse the file.
                easymtl_uninit(pMaterial);
            }
        }
    }

    return 0;
}
#endif




///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
// Code Generators
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#ifndef EASYMTL_NO_GLSL_CODEGEN
#include <stdio.h>

#if defined(__clang__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wswitch-enum"
    #pragma GCC diagnostic ignored "-Wcovered-switch-default"
    #pragma GCC diagnostic ignored "-Wused-but-marked-unused"   // This ie emitted for snprintf() for some reason. Need to investigate...
#endif

typedef struct
{
    /// The output buffer. Can be null, in which case nothing is written.
    char* pBufferOut;

    /// The size of the output buffer.
    unsigned int bufferOutSizeInBytes;

    /// The current length of the string copied to the output buffer.
    unsigned int runningLength;


    /// A pointer to the material that is being used as the source of the code generation.
    easymtl_material* pMaterial;

    /// A pointer to the buffer containing the material's identifiers.
    easymtl_identifier* pIdentifiers;

    /// The number of identifiers.
    unsigned int identifierCount;


    /// The current indentation level, in spaces.
    unsigned int indentationLevel;

} easymtl_codegen_glsl;

bool easymtl_codegen_glsl_write(easymtl_codegen_glsl* pCodegen, const char* src)
{
    assert(pCodegen != NULL);
    assert(src      != NULL);

    if (pCodegen->pBufferOut != NULL)
    {
        unsigned int dstSizeInBytes = (pCodegen->bufferOutSizeInBytes - pCodegen->runningLength);
        while (dstSizeInBytes > 0 && src[0] != '\0')
        {
            pCodegen->pBufferOut[pCodegen->runningLength + 0] = src[0];

            pCodegen->runningLength += 1;
            src += 1;
            dstSizeInBytes -= 1;
        }

        if (dstSizeInBytes > 0)
        {
            // There's enough room for the null terminator which means there was enough room in the buffer. All good.
            pCodegen->pBufferOut[pCodegen->runningLength] = '\0';
            return 1;
        }
        else
        {
            // There's not enough room for the null terminator which means there was NOT enough room in the buffer. Error.
            return 0;
        }
    }
    else
    {
        // We're just measuring.
        pCodegen->runningLength += (unsigned int)strlen(src);
        return 1;
    }
}

bool easymtl_codegen_glsl_write_float(easymtl_codegen_glsl* pCodegen, float src)
{
    assert(pCodegen != NULL);

    char str[32];
    if (snprintf(str, 32, "%f", src) > 0)
    {
        return easymtl_codegen_glsl_write(pCodegen, str);
    }
    else
    {
        return 0;
    }
}

bool easymtl_codegen_glsl_write_int(easymtl_codegen_glsl* pCodegen, int src)
{
    assert(pCodegen != NULL);

    char str[32];
    if (snprintf(str, 32, "%d", src) > 0)
    {
        return easymtl_codegen_glsl_write(pCodegen, str);
    }
    else
    {
        return 0;
    }
}

bool easymtl_codegen_glsl_write_indentation(easymtl_codegen_glsl* pCodegen)
{
    assert(pCodegen != NULL);

    for (unsigned int i = 0; i < pCodegen->indentationLevel; ++i)
    {
        easymtl_codegen_glsl_write(pCodegen, " ");
    }

    return 1;
}

bool easymtl_codegen_glsl_write_type(easymtl_codegen_glsl* pCodegen, easymtl_type type)
{
    assert(pCodegen != NULL);

    switch (type)
    {
    case easymtl_type_float:
        {
            if (!easymtl_codegen_glsl_write(pCodegen, "float"))
            {
                return 0;
            }

            break;
        }
    case easymtl_type_float2:
        {
            if (!easymtl_codegen_glsl_write(pCodegen, "vec2"))
            {
                return 0;
            }

            break;
        }
    case easymtl_type_float3:
        {
            if (!easymtl_codegen_glsl_write(pCodegen, "vec3"))
            {
                return 0;
            }

            break;
        }
    case easymtl_type_float4:
        {
            if (!easymtl_codegen_glsl_write(pCodegen, "vec4"))
            {
                return 0;
            }

            break;
        }

    case easymtl_type_int:
        {
            if (!easymtl_codegen_glsl_write(pCodegen, "int"))
            {
                return 0;
            }

            break;
        }
    case easymtl_type_int2:
        {
            if (!easymtl_codegen_glsl_write(pCodegen, "ivec2"))
            {
                return 0;
            }

            break;
        }
    case easymtl_type_int3:
        {
            if (!easymtl_codegen_glsl_write(pCodegen, "ivec3"))
            {
                return 0;
            }

            break;
        }
    case easymtl_type_int4:
        {
            if (!easymtl_codegen_glsl_write(pCodegen, "ivec4"))
            {
                return 0;
            }

            break;
        }

    case easymtl_type_tex1d:
        {
            if (!easymtl_codegen_glsl_write(pCodegen, "sampler1D"))
            {
                return 0;
            }

            break;
        }
    case easymtl_type_tex2d:
        {
            if (!easymtl_codegen_glsl_write(pCodegen, "sampler2D"))
            {
                return 0;
            }

            break;
        }
    case easymtl_type_tex3d:
        {
            if (!easymtl_codegen_glsl_write(pCodegen, "sampler3D"))
            {
                return 0;
            }

            break;
        }
    case easymtl_type_texcube:
        {
            if (!easymtl_codegen_glsl_write(pCodegen, "samplerCube"))
            {
                return 0;
            }

            break;
        }

    default:
        {
            // Unsupported return type.
            return 0;
        }
    }

    return 1;
}

bool easymtl_codegen_glsl_write_instruction_input_scalar(easymtl_codegen_glsl* pCodegen, unsigned char descriptor, easymtl_instruction_input* pInput)
{
    assert(pCodegen != NULL);
    assert(pInput   != NULL);

    if (descriptor == EASYMTL_INPUT_DESC_CONSTF)
    {
        // It's a constant float.
        return easymtl_codegen_glsl_write_float(pCodegen, pInput->valuef);
    }
    else if (descriptor == EASYMTL_INPUT_DESC_CONSTI)
    {
        // It's a constant int.
        return easymtl_codegen_glsl_write_int(pCodegen, pInput->valuei);
    }
    else
    {
        // It's a variable.
        if (pInput->id < pCodegen->identifierCount)
        {
            easymtl_identifier* pIdentifier = pCodegen->pIdentifiers + pInput->id;
            assert(pIdentifier != NULL);

            if (pIdentifier->type == easymtl_type_float)
            {
                // The input variable is a float, so we don't want to use any selectors.
                return easymtl_codegen_glsl_write(pCodegen, pIdentifier->name);
            }
            else
            {
                if (easymtl_codegen_glsl_write(pCodegen, pIdentifier->name) && easymtl_codegen_glsl_write(pCodegen, "."))
                {
                    switch (descriptor)
                    {
                    case 0: return easymtl_codegen_glsl_write(pCodegen, "x");
                    case 1: return easymtl_codegen_glsl_write(pCodegen, "y");
                    case 2: return easymtl_codegen_glsl_write(pCodegen, "z");
                    case 3: return easymtl_codegen_glsl_write(pCodegen, "w");
                    default: return 0;
                    }
                }
            }
        }
    }

    return 0;
}

bool easymtl_codegen_glsl_write_instruction_input_initializer(easymtl_codegen_glsl* pCodegen, easymtl_type type, easymtl_instruction_input_descriptor inputDesc, easymtl_instruction_input* pInputs)
{
    assert(pCodegen != NULL);
    assert(pInputs  != NULL);

    switch (type)
    {
    case easymtl_type_float:
        {
            return easymtl_codegen_glsl_write_instruction_input_scalar(pCodegen, inputDesc.x, pInputs + 0);
        }

    case easymtl_type_float2:
        {
            if (easymtl_codegen_glsl_write(pCodegen, "vec2("))
            {
                if (easymtl_codegen_glsl_write_instruction_input_scalar(pCodegen, inputDesc.x, pInputs + 0) && easymtl_codegen_glsl_write(pCodegen, ", ") &&
                    easymtl_codegen_glsl_write_instruction_input_scalar(pCodegen, inputDesc.y, pInputs + 1))
                {
                    return easymtl_codegen_glsl_write(pCodegen, ")");
                }
            }

            break;
        }

    case easymtl_type_float3:
        {
            if (easymtl_codegen_glsl_write(pCodegen, "vec3("))
            {
                if (easymtl_codegen_glsl_write_instruction_input_scalar(pCodegen, inputDesc.x, pInputs + 0) && easymtl_codegen_glsl_write(pCodegen, ", ") &&
                    easymtl_codegen_glsl_write_instruction_input_scalar(pCodegen, inputDesc.y, pInputs + 1) && easymtl_codegen_glsl_write(pCodegen, ", ") &&
                    easymtl_codegen_glsl_write_instruction_input_scalar(pCodegen, inputDesc.z, pInputs + 2))
                {
                    return easymtl_codegen_glsl_write(pCodegen, ")");
                }
            }

            break;
        }

    case easymtl_type_float4:
        {
            if (easymtl_codegen_glsl_write(pCodegen, "vec4("))
            {
                if (easymtl_codegen_glsl_write_instruction_input_scalar(pCodegen, inputDesc.x, pInputs + 0) && easymtl_codegen_glsl_write(pCodegen, ", ") &&
                    easymtl_codegen_glsl_write_instruction_input_scalar(pCodegen, inputDesc.y, pInputs + 1) && easymtl_codegen_glsl_write(pCodegen, ", ") &&
                    easymtl_codegen_glsl_write_instruction_input_scalar(pCodegen, inputDesc.z, pInputs + 2) && easymtl_codegen_glsl_write(pCodegen, ", ") &&
                    easymtl_codegen_glsl_write_instruction_input_scalar(pCodegen, inputDesc.w, pInputs + 3))
                {
                    return easymtl_codegen_glsl_write(pCodegen, ")");
                }
            }

            break;
        }


    case easymtl_type_int:
        {
            return easymtl_codegen_glsl_write_instruction_input_scalar(pCodegen, inputDesc.x, pInputs + 0);
        }

    case easymtl_type_int2:
        {
            if (easymtl_codegen_glsl_write(pCodegen, "ivec2("))
            {
                if (easymtl_codegen_glsl_write_instruction_input_scalar(pCodegen, inputDesc.x, pInputs + 0) && easymtl_codegen_glsl_write(pCodegen, ", ") &&
                    easymtl_codegen_glsl_write_instruction_input_scalar(pCodegen, inputDesc.y, pInputs + 1))
                {
                    return easymtl_codegen_glsl_write(pCodegen, ")");
                }
            }

            break;
        }

    case easymtl_type_int3:
        {
            if (easymtl_codegen_glsl_write(pCodegen, "ivec3("))
            {
                if (easymtl_codegen_glsl_write_instruction_input_scalar(pCodegen, inputDesc.x, pInputs + 0) && easymtl_codegen_glsl_write(pCodegen, ", ") &&
                    easymtl_codegen_glsl_write_instruction_input_scalar(pCodegen, inputDesc.y, pInputs + 1) && easymtl_codegen_glsl_write(pCodegen, ", ") &&
                    easymtl_codegen_glsl_write_instruction_input_scalar(pCodegen, inputDesc.z, pInputs + 2))
                {
                    return easymtl_codegen_glsl_write(pCodegen, ")");
                }
            }

            break;
        }

    case easymtl_type_int4:
        {
            if (easymtl_codegen_glsl_write(pCodegen, "ivec4("))
            {
                if (easymtl_codegen_glsl_write_instruction_input_scalar(pCodegen, inputDesc.x, pInputs + 0) && easymtl_codegen_glsl_write(pCodegen, ", ") &&
                    easymtl_codegen_glsl_write_instruction_input_scalar(pCodegen, inputDesc.y, pInputs + 1) && easymtl_codegen_glsl_write(pCodegen, ", ") &&
                    easymtl_codegen_glsl_write_instruction_input_scalar(pCodegen, inputDesc.z, pInputs + 2) && easymtl_codegen_glsl_write(pCodegen, ", ") &&
                    easymtl_codegen_glsl_write_instruction_input_scalar(pCodegen, inputDesc.w, pInputs + 3))
                {
                    return easymtl_codegen_glsl_write(pCodegen, ")");
                }
            }

            break;
        }


    default:
        {
            // Unsupported return type.
            return 0;
        }
    }

    return 0;
}


bool easymtl_codegen_glsl_write_instruction_mov(easymtl_codegen_glsl* pCodegen, easymtl_instruction* pInstruction)
{
    assert(pCodegen     != NULL);
    assert(pInstruction != NULL);

    if (pInstruction->mov.output < pCodegen->identifierCount)
    {
        easymtl_identifier* pOutputIdentifier = pCodegen->pIdentifiers + pInstruction->mov.output;
        assert(pOutputIdentifier != NULL);

        if (easymtl_codegen_glsl_write(pCodegen, pOutputIdentifier->name) && easymtl_codegen_glsl_write(pCodegen, " = "))
        {
            easymtl_type type;
            switch (pInstruction->opcode)
            {
            case easymtl_opcode_movf1: type = easymtl_type_float;  break;
            case easymtl_opcode_movf2: type = easymtl_type_float2; break;
            case easymtl_opcode_movf3: type = easymtl_type_float3; break;
            case easymtl_opcode_movf4: type = easymtl_type_float4; break;
            case easymtl_opcode_movi1: type = easymtl_type_int;    break;
            case easymtl_opcode_movi2: type = easymtl_type_int2;   break;
            case easymtl_opcode_movi3: type = easymtl_type_int3;   break;
            case easymtl_opcode_movi4: type = easymtl_type_int4;   break;
            default: return 0;
            }

            return easymtl_codegen_glsl_write_instruction_input_initializer(pCodegen, type, pInstruction->mov.inputDesc, &pInstruction->mov.inputX) && easymtl_codegen_glsl_write(pCodegen, ";\n");
        }
    }

    return 0;
}

bool easymtl_codegen_glsl_write_instruction_add(easymtl_codegen_glsl* pCodegen, easymtl_instruction* pInstruction)
{
    assert(pCodegen     != NULL);
    assert(pInstruction != NULL);

    if (pInstruction->add.output < pCodegen->identifierCount)
    {
        easymtl_identifier* pOutputIdentifier = pCodegen->pIdentifiers + pInstruction->add.output;
        assert(pOutputIdentifier != NULL);

        if (easymtl_codegen_glsl_write(pCodegen, pOutputIdentifier->name) && easymtl_codegen_glsl_write(pCodegen, " += "))
        {
            easymtl_type type;
            switch (pInstruction->opcode)
            {
            case easymtl_opcode_addf1: type = easymtl_type_float;  break;
            case easymtl_opcode_addf2: type = easymtl_type_float2; break;
            case easymtl_opcode_addf3: type = easymtl_type_float3; break;
            case easymtl_opcode_addf4: type = easymtl_type_float4; break;
            case easymtl_opcode_addi1: type = easymtl_type_int;    break;
            case easymtl_opcode_addi2: type = easymtl_type_int2;   break;
            case easymtl_opcode_addi3: type = easymtl_type_int3;   break;
            case easymtl_opcode_addi4: type = easymtl_type_int4;   break;
            default: return 0;
            }

            return easymtl_codegen_glsl_write_instruction_input_initializer(pCodegen, type, pInstruction->add.inputDesc, &pInstruction->add.inputX) && easymtl_codegen_glsl_write(pCodegen, ";\n");
        }
    }

    return 0;
}

bool easymtl_codegen_glsl_write_instruction_sub(easymtl_codegen_glsl* pCodegen, easymtl_instruction* pInstruction)
{
    assert(pCodegen     != NULL);
    assert(pInstruction != NULL);

    if (pInstruction->add.output < pCodegen->identifierCount)
    {
        easymtl_identifier* pOutputIdentifier = pCodegen->pIdentifiers + pInstruction->sub.output;
        assert(pOutputIdentifier != NULL);

        if (easymtl_codegen_glsl_write(pCodegen, pOutputIdentifier->name) && easymtl_codegen_glsl_write(pCodegen, " -= "))
        {
            easymtl_type type;
            switch (pInstruction->opcode)
            {
            case easymtl_opcode_subf1: type = easymtl_type_float;  break;
            case easymtl_opcode_subf2: type = easymtl_type_float2; break;
            case easymtl_opcode_subf3: type = easymtl_type_float3; break;
            case easymtl_opcode_subf4: type = easymtl_type_float4; break;
            case easymtl_opcode_subi1: type = easymtl_type_int;    break;
            case easymtl_opcode_subi2: type = easymtl_type_int2;   break;
            case easymtl_opcode_subi3: type = easymtl_type_int3;   break;
            case easymtl_opcode_subi4: type = easymtl_type_int4;   break;
            default: return 0;
            }

            return easymtl_codegen_glsl_write_instruction_input_initializer(pCodegen, type, pInstruction->sub.inputDesc, &pInstruction->sub.inputX) && easymtl_codegen_glsl_write(pCodegen, ";\n");
        }
    }

    return 0;
}

bool easymtl_codegen_glsl_write_instruction_mul(easymtl_codegen_glsl* pCodegen, easymtl_instruction* pInstruction)
{
    assert(pCodegen     != NULL);
    assert(pInstruction != NULL);

    if (pInstruction->mul.output < pCodegen->identifierCount)
    {
        easymtl_identifier* pOutputIdentifier = pCodegen->pIdentifiers + pInstruction->mul.output;
        assert(pOutputIdentifier != NULL);

        if (easymtl_codegen_glsl_write(pCodegen, pOutputIdentifier->name) && easymtl_codegen_glsl_write(pCodegen, " *= "))
        {
            easymtl_type type;
            switch (pInstruction->opcode)
            {
            case easymtl_opcode_mulf1: type = easymtl_type_float;  break;
            case easymtl_opcode_mulf2: type = easymtl_type_float2; break;
            case easymtl_opcode_mulf3: type = easymtl_type_float3; break;
            case easymtl_opcode_mulf4: type = easymtl_type_float4; break;
            case easymtl_opcode_muli1: type = easymtl_type_int;    break;
            case easymtl_opcode_muli2: type = easymtl_type_int2;   break;
            case easymtl_opcode_muli3: type = easymtl_type_int3;   break;
            case easymtl_opcode_muli4: type = easymtl_type_int4;   break;
            default: return 0;
            }

            return easymtl_codegen_glsl_write_instruction_input_initializer(pCodegen, type, pInstruction->mul.inputDesc, &pInstruction->mul.inputX) && easymtl_codegen_glsl_write(pCodegen, ";\n");
        }
    }

    return 0;
}

bool easymtl_codegen_glsl_write_instruction_div(easymtl_codegen_glsl* pCodegen, easymtl_instruction* pInstruction)
{
    assert(pCodegen     != NULL);
    assert(pInstruction != NULL);

    if (pInstruction->div.output < pCodegen->identifierCount)
    {
        easymtl_identifier* pOutputIdentifier = pCodegen->pIdentifiers + pInstruction->div.output;
        assert(pOutputIdentifier != NULL);

        if (easymtl_codegen_glsl_write(pCodegen, pOutputIdentifier->name) && easymtl_codegen_glsl_write(pCodegen, " = "))
        {
            easymtl_type type;
            switch (pInstruction->opcode)
            {
            case easymtl_opcode_divf1: type = easymtl_type_float;  break;
            case easymtl_opcode_divf2: type = easymtl_type_float2; break;
            case easymtl_opcode_divf3: type = easymtl_type_float3; break;
            case easymtl_opcode_divf4: type = easymtl_type_float4; break;
            case easymtl_opcode_divi1: type = easymtl_type_int;    break;
            case easymtl_opcode_divi2: type = easymtl_type_int2;   break;
            case easymtl_opcode_divi3: type = easymtl_type_int3;   break;
            case easymtl_opcode_divi4: type = easymtl_type_int4;   break;
            default: return 0;
            }

            return easymtl_codegen_glsl_write_instruction_input_initializer(pCodegen, type, pInstruction->div.inputDesc, &pInstruction->div.inputX) && easymtl_codegen_glsl_write(pCodegen, ";\n");
        }
    }

    return 0;
}

bool easymtl_codegen_glsl_write_instruction_pow(easymtl_codegen_glsl* pCodegen, easymtl_instruction* pInstruction)
{
    assert(pCodegen     != NULL);
    assert(pInstruction != NULL);

    if (pInstruction->pow.output < pCodegen->identifierCount)
    {
        easymtl_identifier* pOutputIdentifier = pCodegen->pIdentifiers + pInstruction->pow.output;
        assert(pOutputIdentifier != NULL);

        if (easymtl_codegen_glsl_write(pCodegen, pOutputIdentifier->name) && easymtl_codegen_glsl_write(pCodegen, " = pow(") && easymtl_codegen_glsl_write(pCodegen, pOutputIdentifier->name) && easymtl_codegen_glsl_write(pCodegen, ", "))
        {
            easymtl_type type;
            switch (pInstruction->opcode)
            {
            case easymtl_opcode_powf1: type = easymtl_type_float;  break;
            case easymtl_opcode_powf2: type = easymtl_type_float2; break;
            case easymtl_opcode_powf3: type = easymtl_type_float3; break;
            case easymtl_opcode_powf4: type = easymtl_type_float4; break;
            case easymtl_opcode_powi1: type = easymtl_type_int;    break;
            case easymtl_opcode_powi2: type = easymtl_type_int2;   break;
            case easymtl_opcode_powi3: type = easymtl_type_int3;   break;
            case easymtl_opcode_powi4: type = easymtl_type_int4;   break;
            default: return 0;
            }

            return easymtl_codegen_glsl_write_instruction_input_initializer(pCodegen, type, pInstruction->pow.inputDesc, &pInstruction->pow.inputX) && easymtl_codegen_glsl_write(pCodegen, ");\n");
        }
    }

    return 0;
}

bool easymtl_codegen_glsl_write_instruction_tex(easymtl_codegen_glsl* pCodegen, easymtl_instruction* pInstruction)
{
    assert(pCodegen     != NULL);
    assert(pInstruction != NULL);

    if (pInstruction->tex.output < pCodegen->identifierCount && pInstruction->tex.texture < pCodegen->identifierCount)
    {
        easymtl_identifier* pOutputIdentifier = pCodegen->pIdentifiers + pInstruction->tex.output;
        assert(pOutputIdentifier != NULL);

        easymtl_identifier* pTextureIdentifier = pCodegen->pIdentifiers + pInstruction->tex.texture;
        assert(pTextureIdentifier != NULL);

        if (easymtl_codegen_glsl_write(pCodegen, pOutputIdentifier->name) && easymtl_codegen_glsl_write(pCodegen, " = "))
        {
            easymtl_type type;
            switch (pInstruction->opcode)
            {
            case easymtl_opcode_tex1:
            {
                type = easymtl_type_float;
                if (!easymtl_codegen_glsl_write(pCodegen, "texture1D("))
                {
                    return 0;
                }

                break;
            }

            case easymtl_opcode_tex2:
            {
                type = easymtl_type_float2;
                if (!easymtl_codegen_glsl_write(pCodegen, "texture2D("))
                {
                    return 0;
                }

                break;
            }

            case easymtl_opcode_tex3:
            {
                type = easymtl_type_float3;
                if (!easymtl_codegen_glsl_write(pCodegen, "texture3D("))
                {
                    return 0;
                }

                break;
            }

            case easymtl_opcode_texcube:
            {
                type = easymtl_type_float3;
                if (!easymtl_codegen_glsl_write(pCodegen, "textureCube("))
                {
                    return 0;
                }

                break;
            }

            default: return 0;
            }

            return
                easymtl_codegen_glsl_write(pCodegen, pTextureIdentifier->name) &&
                easymtl_codegen_glsl_write(pCodegen, ", ") &&
                easymtl_codegen_glsl_write_instruction_input_initializer(pCodegen, type, pInstruction->tex.inputDesc, &pInstruction->tex.inputX) &&
                easymtl_codegen_glsl_write(pCodegen, ");\n");
        }
    }

    return 0;
}

bool easymtl_codegen_glsl_write_instruction_var(easymtl_codegen_glsl* pCodegen, easymtl_instruction* pInstruction)
{
    assert(pCodegen     != NULL);
    assert(pInstruction != NULL);

    if (pInstruction->var.identifierIndex < pCodegen->identifierCount)
    {
        easymtl_identifier* pIdentifier = pCodegen->pIdentifiers + pInstruction->var.identifierIndex;
        assert(pIdentifier != NULL);

        return easymtl_codegen_glsl_write_type(pCodegen, pIdentifier->type) && easymtl_codegen_glsl_write(pCodegen, " ") && easymtl_codegen_glsl_write(pCodegen, pIdentifier->name) && easymtl_codegen_glsl_write(pCodegen, ";\n");
    }

    return 0;
}

bool easymtl_codegen_glsl_write_instruction_ret(easymtl_codegen_glsl* pCodegen, easymtl_instruction* pInstruction)
{
    assert(pCodegen     != NULL);
    assert(pInstruction != NULL);

    if (easymtl_codegen_glsl_write(pCodegen, "return "))
    {
        easymtl_type type;
        switch (pInstruction->opcode)
        {
        case easymtl_opcode_retf1: type = easymtl_type_float;  break;
        case easymtl_opcode_retf2: type = easymtl_type_float2; break;
        case easymtl_opcode_retf3: type = easymtl_type_float3; break;
        case easymtl_opcode_retf4: type = easymtl_type_float4; break;
        case easymtl_opcode_reti1: type = easymtl_type_int;    break;
        case easymtl_opcode_reti2: type = easymtl_type_int2;   break;
        case easymtl_opcode_reti3: type = easymtl_type_int3;   break;
        case easymtl_opcode_reti4: type = easymtl_type_int4;   break;
        default: return 0;
        }

        return easymtl_codegen_glsl_write_instruction_input_initializer(pCodegen, type, pInstruction->ret.inputDesc, &pInstruction->ret.inputX) && easymtl_codegen_glsl_write(pCodegen, ";\n");
    }

    return 0;
}

bool easymtl_codegen_glsl_write_instruction(easymtl_codegen_glsl* pCodegen, easymtl_instruction* pInstruction)
{
    assert(pCodegen     != NULL);
    assert(pInstruction != NULL);

    if (easymtl_codegen_glsl_write_indentation(pCodegen))
    {
        switch (pInstruction->opcode)
        {
        case easymtl_opcode_movf1:
        case easymtl_opcode_movf2:
        case easymtl_opcode_movf3:
        case easymtl_opcode_movf4:
        case easymtl_opcode_movi1:
        case easymtl_opcode_movi2:
        case easymtl_opcode_movi3:
        case easymtl_opcode_movi4:
            {
                return easymtl_codegen_glsl_write_instruction_mov(pCodegen, pInstruction);
            }


        case easymtl_opcode_addf1:
        case easymtl_opcode_addf2:
        case easymtl_opcode_addf3:
        case easymtl_opcode_addf4:
        case easymtl_opcode_addi1:
        case easymtl_opcode_addi2:
        case easymtl_opcode_addi3:
        case easymtl_opcode_addi4:
            {
                return easymtl_codegen_glsl_write_instruction_add(pCodegen, pInstruction);
            }

        case easymtl_opcode_subf1:
        case easymtl_opcode_subf2:
        case easymtl_opcode_subf3:
        case easymtl_opcode_subf4:
        case easymtl_opcode_subi1:
        case easymtl_opcode_subi2:
        case easymtl_opcode_subi3:
        case easymtl_opcode_subi4:
            {
                return easymtl_codegen_glsl_write_instruction_sub(pCodegen, pInstruction);
            }

        case easymtl_opcode_mulf1:
        case easymtl_opcode_mulf2:
        case easymtl_opcode_mulf3:
        case easymtl_opcode_mulf4:
        case easymtl_opcode_muli1:
        case easymtl_opcode_muli2:
        case easymtl_opcode_muli3:
        case easymtl_opcode_muli4:
            {
                return easymtl_codegen_glsl_write_instruction_mul(pCodegen, pInstruction);
            }

        case easymtl_opcode_divf1:
        case easymtl_opcode_divf2:
        case easymtl_opcode_divf3:
        case easymtl_opcode_divf4:
        case easymtl_opcode_divi1:
        case easymtl_opcode_divi2:
        case easymtl_opcode_divi3:
        case easymtl_opcode_divi4:
            {
                return easymtl_codegen_glsl_write_instruction_div(pCodegen, pInstruction);
            }

        case easymtl_opcode_powf1:
        case easymtl_opcode_powf2:
        case easymtl_opcode_powf3:
        case easymtl_opcode_powf4:
        case easymtl_opcode_powi1:
        case easymtl_opcode_powi2:
        case easymtl_opcode_powi3:
        case easymtl_opcode_powi4:
            {
                return easymtl_codegen_glsl_write_instruction_pow(pCodegen, pInstruction);
            }

        case easymtl_opcode_tex1:
        case easymtl_opcode_tex2:
        case easymtl_opcode_tex3:
        case easymtl_opcode_texcube:
            {
                return easymtl_codegen_glsl_write_instruction_tex(pCodegen, pInstruction);
            }


        case easymtl_opcode_var:
            {
                return easymtl_codegen_glsl_write_instruction_var(pCodegen, pInstruction);
            }

        case easymtl_opcode_retf1:
        case easymtl_opcode_retf2:
        case easymtl_opcode_retf3:
        case easymtl_opcode_retf4:
        case easymtl_opcode_reti1:
        case easymtl_opcode_reti2:
        case easymtl_opcode_reti3:
        case easymtl_opcode_reti4:
            {
                return easymtl_codegen_glsl_write_instruction_ret(pCodegen, pInstruction);
            }


        default:
            {
                // Unknown or unsupported opcode.
                break;
            }
        }
    }

    return 0;
}

bool easymtl_codegen_glsl_write_instructions(easymtl_codegen_glsl* pCodegen, easymtl_instruction* pInstructions, unsigned int instructionCount)
{
    assert(pCodegen      != NULL);
    assert(pInstructions != NULL);

    for (unsigned int iInstruction = 0; iInstruction < instructionCount; ++iInstruction)
    {
        easymtl_instruction* pInstruction = pInstructions + iInstruction;
        assert(pInstruction != NULL);

        if (!easymtl_codegen_glsl_write_instruction(pCodegen, pInstruction))
        {
            return 0;
        }
    }

    return 1;
}

bool easymtl_codegen_glsl_channel_function_begin(easymtl_codegen_glsl* pCodegen, easymtl_channel_header* pChannelHeader)
{
    assert(pCodegen       != NULL);
    assert(pChannelHeader != NULL);

    // <type> <name> {\n
    bool result =
        easymtl_codegen_glsl_write_type(pCodegen, pChannelHeader->channel.type) &&
        easymtl_codegen_glsl_write(pCodegen, " ") &&
        easymtl_codegen_glsl_write(pCodegen, pChannelHeader->channel.name) &&
        easymtl_codegen_glsl_write(pCodegen, "() {\n");
    if (result)
    {
        pCodegen->indentationLevel += 4;
    }

    return result;
}

bool easymtl_codegen_glsl_channel_function_close(easymtl_codegen_glsl* pCodegen)
{
    assert(pCodegen != NULL);

    if (pCodegen->indentationLevel > 4) {
        pCodegen->indentationLevel -= 4;
    } else {
        pCodegen->indentationLevel = 0;
    }

    return easymtl_codegen_glsl_write(pCodegen, "}\n");
}

bool easymtl_codegen_glsl_channel(easymtl_material* pMaterial, const char* channelName, char* codeOut, unsigned int codeOutSizeInBytes, unsigned int* pBytesWrittenOut)
{
    if (pMaterial != NULL)
    {
        easymtl_header* pHeader = easymtl_getheader(pMaterial);
        if (pHeader != NULL)
        {
            easymtl_channel_header* pChannelHeader = easymtl_getchannelheaderbyname(pMaterial, channelName);
            if (pChannelHeader != NULL)
            {
                easymtl_codegen_glsl codegen;
                codegen.pBufferOut           = codeOut;
                codegen.bufferOutSizeInBytes = codeOutSizeInBytes;
                codegen.runningLength        = 0;
                codegen.pMaterial            = pMaterial;
                codegen.pIdentifiers         = easymtl_getidentifiers(pMaterial);
                codegen.identifierCount      = easymtl_getidentifiercount(pMaterial);
                codegen.indentationLevel     = 0;

                if (easymtl_codegen_glsl_channel_function_begin(&codegen, pChannelHeader))
                {
                    easymtl_instruction* pInstructions = (easymtl_instruction*)(pChannelHeader + 1);
                    assert(pInstructions != NULL);

                    if (easymtl_codegen_glsl_write_instructions(&codegen, pInstructions, pChannelHeader->instructionCount))
                    {
                        bool result = easymtl_codegen_glsl_channel_function_close(&codegen);
                        if (result)
                        {
                            if (pBytesWrittenOut != NULL)
                            {
                                *pBytesWrittenOut = codegen.runningLength + 1;
                            }
                        }

                        return result;
                    }
                }
            }
        }
    }

    return 0;
}



bool easymtl_codegen_glsl_uniform(easymtl_codegen_glsl* pCodegen, easymtl_input* pInput)
{
    assert(pCodegen != NULL);
    assert(pInput   != NULL);

    if (pInput->identifierIndex < pCodegen->identifierCount)
    {
        easymtl_identifier* pIdentifier = pCodegen->pIdentifiers + pInput->identifierIndex;
        assert(pIdentifier != NULL);

        // uniform <type> <name>;
        return
            easymtl_codegen_glsl_write(pCodegen, "uniform ") &&
            easymtl_codegen_glsl_write_type(pCodegen, pIdentifier->type) &&
            easymtl_codegen_glsl_write(pCodegen, " ") &&
            easymtl_codegen_glsl_write(pCodegen, pIdentifier->name) &&
            easymtl_codegen_glsl_write(pCodegen, ";\n");
    }

    return 0;
}

bool easymtl_codegen_glsl_uniforms(easymtl_material* pMaterial, char* codeOut, unsigned int codeOutSizeInBytes, unsigned int* pBytesWritteOut)
{
    if (pMaterial != NULL)
    {
        easymtl_codegen_glsl codegen;
        codegen.pBufferOut           = codeOut;
        codegen.bufferOutSizeInBytes = codeOutSizeInBytes;
        codegen.runningLength        = 0;
        codegen.pMaterial            = pMaterial;
        codegen.pIdentifiers         = easymtl_getidentifiers(pMaterial);
        codegen.identifierCount      = easymtl_getidentifiercount(pMaterial);
        codegen.indentationLevel     = 0;


        unsigned int inputCount = easymtl_getpublicinputcount(pMaterial);
        if (inputCount > 0)
        {
            for (unsigned int iInput = 0; iInput < inputCount; ++iInput)
            {
                easymtl_input* pInput = easymtl_getpublicinputbyindex(pMaterial, iInput);
                assert(pInput != NULL);

                if (!easymtl_codegen_glsl_uniform(&codegen, pInput))
                {
                    // There was an error writing one of the uniforms. Return false.
                    return 0;
                }
            }
        }
        else
        {
            // No inputs. Just write an empty string.
            easymtl_codegen_glsl_write(&codegen, "");
        }

        if (pBytesWritteOut != NULL)
        {
            *pBytesWritteOut = codegen.runningLength + 1;
        }

        return 1;
    }

    return 0;
}

#if defined(__clang__)
    #pragma GCC diagnostic pop
#endif
#endif



#if defined(__clang__)
    #pragma GCC diagnostic pop
#endif

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
