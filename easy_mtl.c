// Version 0.1 - Public Domain. See "unlicense" statement at the end of this file.

#include "easy_mtl.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// When constructing the material's raw data, memory allocated in blocks of this amount. This must be at least 256.
#define EASYMTL_CHUNK_SIZE              512     // NOTE: Intentionally small for now to ensure we hit the inflation branch during testing. Increase later.

#define EASYMTL_STAGE_IDS               0
#define EASYMTL_STAGE_PRIVATE_INPUTS    1
#define EASYMTL_STAGE_PUBLIC_INPUTS     2
#define EASYMTL_STAGE_CHANNELS          3
#define EASYMTL_STAGE_PROPERTIES        4


/// Inflates the materials data buffer by EASYMTL_CHUNK_SIZE.
easymtl_bool _easymtl_inflate(easymtl_material* pMaterial);


easymtl_bool easymtl_init(easymtl_material* pMaterial)
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

            easymtl_header* pHeader = easymtl_getheader(pMaterial);
            assert(pHeader != NULL);

            pHeader->magic                  = EASYMTL_MAGIC_NUMBER;
            pHeader->version                = EASYMTL_CURRENT_VERSION;
            pHeader->identifierSizeInBytes  = sizeof(easymtl_identifier);
            pHeader->inputSizeInBytes       = sizeof(easymtl_input_var);
            pHeader->instructionSizeInBytes = sizeof(easymtl_instruction);
            pHeader->propertySizeInBytes    = sizeof(easymtl_property);
            pHeader->identifierCount        = 0;
            pHeader->privateInputCount      = 0;
            pHeader->publicInputCount       = 0;
            pHeader->channelCount           = 0;
            pHeader->propertyCount          = 0;
            pHeader->identifiersOffset      = pMaterial->sizeInBytes;
            pHeader->inputsOffset           = pMaterial->sizeInBytes;
            pHeader->channelsOffset         = pMaterial->sizeInBytes;
            pHeader->propertiesOffset       = pMaterial->sizeInBytes;
            pHeader->padding0               = 0;

            return 1;
        }
    }

    return 0;
}

void easymtl_uninit(easymtl_material* pMaterial)
{
    if (pMaterial != NULL)
    {
        free(pMaterial->pRawData);
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


easymtl_bool easymtl_appendidentifier(easymtl_material* pMaterial, const easymtl_identifier* pIdentifier)
{
    if (pMaterial != NULL && pIdentifier != NULL)
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
                    
                    assert(pMaterial->sizeInBytes + pHeader->identifierSizeInBytes <= pMaterial->bufferSizeInBytes);
                }
                

                memcpy(pMaterial->pRawData + pHeader->inputsOffset, pIdentifier, pHeader->identifierSizeInBytes);
                pMaterial->sizeInBytes += pHeader->identifierSizeInBytes;
                
                pHeader->identifierCount  += 1;
                pHeader->inputsOffset     += pHeader->identifierSizeInBytes;
                pHeader->channelsOffset   += pHeader->identifierSizeInBytes;
                pHeader->propertiesOffset += pHeader->identifierSizeInBytes;

                return 1;
            }
        }
    }

    return 0;
}

easymtl_bool easymtl_appendprivateinput(easymtl_material* pMaterial, const easymtl_input_var* pInput)
{
    if (pMaterial != NULL && pInput != NULL)
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
                    
                    assert(pMaterial->sizeInBytes + pHeader->inputSizeInBytes <= pMaterial->bufferSizeInBytes);
                }


                memcpy(pMaterial->pRawData + pHeader->channelsOffset, pInput, pHeader->inputSizeInBytes);
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

easymtl_bool easymtl_appendpublicinput(easymtl_material* pMaterial, const easymtl_input_var* pInput)
{
    if (pMaterial != NULL && pInput != NULL)
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
                    
                    assert(pMaterial->sizeInBytes + pHeader->inputSizeInBytes <= pMaterial->bufferSizeInBytes);
                }


                memcpy(pMaterial->pRawData + pHeader->channelsOffset, pInput, pHeader->inputSizeInBytes);
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

easymtl_bool easymtl_appendchannel(easymtl_material* pMaterial, const easymtl_channel_header* pChannelHeader)
{
    if (pMaterial != NULL && pChannelHeader != NULL)
    {
        if (pMaterial->currentStage <= EASYMTL_STAGE_CHANNELS)
        {
            easymtl_header* pHeader = easymtl_getheader(pMaterial);
            if (pHeader != NULL)
            {
                if (pMaterial->sizeInBytes + sizeof(easymtl_channel_header) > pMaterial->bufferSizeInBytes)
                {
                    if (!_easymtl_inflate(pMaterial))
                    {
                        // An error occured when trying to inflate the buffer. Might be out of memory.
                        return 0;
                    }
                    
                    assert(pMaterial->sizeInBytes + sizeof(easymtl_channel_header) <= pMaterial->bufferSizeInBytes);
                }


                memcpy(pMaterial->pRawData + pHeader->propertiesOffset, pChannelHeader, sizeof(easymtl_channel_header));
                pMaterial->currentChannelOffset = pMaterial->sizeInBytes;
                pMaterial->sizeInBytes += sizeof(easymtl_channel_header);

                pHeader->channelCount     += 1;
                pHeader->propertiesOffset += sizeof(easymtl_channel_header);


                pMaterial->currentStage = EASYMTL_STAGE_CHANNELS;
                return 1;
            }
        }
    }

    return 0;
}

easymtl_bool easymtl_appendinstruction(easymtl_material* pMaterial, const easymtl_instruction* pInstruction)
{
    if (pMaterial != NULL && pInstruction != NULL)
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
                    
                    assert(pMaterial->sizeInBytes + pHeader->instructionSizeInBytes <= pMaterial->bufferSizeInBytes);
                }


                memcpy(pMaterial->pRawData + pHeader->propertiesOffset, pInstruction, pHeader->instructionSizeInBytes);
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

easymtl_bool easymtl_appendproperty(easymtl_material* pMaterial, const easymtl_property* pProperty)
{
    if (pMaterial != NULL && pProperty != NULL)
    {
        if (pMaterial->currentStage <= EASYMTL_STAGE_PRIVATE_INPUTS)
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
                    
                    assert(pMaterial->sizeInBytes + pHeader->propertySizeInBytes <= pMaterial->bufferSizeInBytes);
                }


                memcpy(pMaterial->pRawData + pMaterial->sizeInBytes, pProperty, pHeader->propertySizeInBytes);
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
            if (strcmp(((easymtl_channel_header*)pChannelHeader)->name, channelName) == 0)
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


//////////////////////////////////
// Private low-level API.

easymtl_bool _easymtl_inflate(easymtl_material* pMaterial)
{
    assert(pMaterial != NULL);

    easymtl_uint8* pOldBuffer = pMaterial->pRawData;
    easymtl_uint8* pNewBuffer = malloc(pMaterial->bufferSizeInBytes + EASYMTL_CHUNK_SIZE);
    if (pNewBuffer != NULL)
    {
        memcpy(pNewBuffer, pOldBuffer, pMaterial->sizeInBytes);
        pMaterial->bufferSizeInBytes += EASYMTL_CHUNK_SIZE;

        free(pOldBuffer);
        return 1;
    }

    return 0;
}



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