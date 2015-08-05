// Version 0.1 - Public Domain. See "unlicense" statement at the end of this file.

#include "easy_mtl.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>


// Platform detection
#if   !defined(EASYMTL_PLATFORM_WINDOWS) && (defined(__WIN32__) || defined(_WIN32) || defined(_WIN64))
#define EASYMTL_PLATFORM_WINDOWS
#elif !defined(EASFMTL_PLATFORM_LINUX)   &&  defined(__linux__)
#define EASYMTL_PLATFORM_LINUX
#elif !defined(EASYMTL_PLATFORM_MAC)     && (defined(__APPLE__) && defined(__MACH__))
#define	EASYMTL_PLATFORM_MAC
#endif


// When constructing the material's raw data, memory allocated in blocks of this amount. This must be at least 256.
#define EASYMTL_CHUNK_SIZE              4096

#define EASYMTL_STAGE_IDS               0
#define EASYMTL_STAGE_PRIVATE_INPUTS    1
#define EASYMTL_STAGE_PUBLIC_INPUTS     2
#define EASYMTL_STAGE_CHANNELS          3
#define EASYMTL_STAGE_PROPERTIES        4
#define EASYMTL_STAGE_COMPLETE          UINT_MAX


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

easymtl_bool easymtl_initfromexisting(easymtl_material* pMaterial, const void* pRawData, unsigned int dataSizeInBytes)
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

easymtl_bool easymtl_initfromexisting_nocopy(easymtl_material* pMaterial, const void* pRawData, unsigned int dataSizeInBytes)
{
    if (pMaterial != NULL)
    {
        if (pRawData != NULL && dataSizeInBytes >= sizeof(easymtl_header))
        {
            if (((easymtl_header*)pRawData)->magic == EASYMTL_MAGIC_NUMBER)
            {
                pMaterial->pRawData             = (void*)pRawData;
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


easymtl_bool easymtl_appendidentifier(easymtl_material* pMaterial, easymtl_identifier identifier, unsigned int* indexOut)
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

easymtl_bool easymtl_appendprivateinput(easymtl_material* pMaterial, easymtl_input input)
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

easymtl_bool easymtl_appendpublicinput(easymtl_material* pMaterial, easymtl_input input)
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

easymtl_bool easymtl_appendchannel(easymtl_material* pMaterial, easymtl_channel channel)
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

easymtl_bool easymtl_appendinstruction(easymtl_material* pMaterial, easymtl_instruction instruction)
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

easymtl_bool easymtl_appendproperty(easymtl_material* pMaterial, easymtl_property prop)
{
    if (pMaterial != NULL)
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

unsigned int easymtl_getpublicinputvariablecount(easymtl_material* pMaterial)
{
    if (pMaterial != NULL)
    {
        easymtl_header* pHeader = easymtl_getheader(pMaterial);
        assert(pHeader != NULL);

        return pHeader->publicInputCount;
    }

    return 0;
}

easymtl_input* easymtl_getpublicinputvariable(easymtl_material* pMaterial, unsigned int index)
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
    input.f3.x = x;
    input.f3.y = y;
    input.f3.z = z;
    input.f4.w = w;

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


easymtl_property easymtl_property_bool(const char* name, easymtl_bool value)
{
    easymtl_property prop;
    prop.type = easymtl_type_bool;
    easymtl_strcpy(prop.name, EASYMTL_MAX_PROPERTY_NAME, name);
    prop.b1.x = value;

    return prop;
}

easymtl_property easymtl_property_float(const char* name, float value)
{
    easymtl_property prop;
    prop.type = easymtl_type_float;
    easymtl_strcpy(prop.name, EASYMTL_MAX_PROPERTY_NAME, name);
    prop.f1.x = value;

    return prop;
}




////////////////////////////////////////////////////////
// Utilities

// strcpy()
void easymtl_strcpy(char* dst, size_t dstSizeInBytes, const char* src)
{
#if defined(EASYMTL_PLATFORM_WINDOWS)
    strcpy_s(dst, dstSizeInBytes, src);
#else
    while (dstSizeInBytes > 0 && src[0] != '\0')
    {
        dst[0] = src[0];

        dst += 1;
        src += 1;
        dstSizeInBytes -= 1;
    }

    if (dstSizeInBytes > 0)
    {
        dst[0] = '\0';
    }
#endif
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