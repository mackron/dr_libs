// Version 0.1 - Public Domain. See "unlicense" statement at the end of this file.

#include "easy_mtl_compiler_mtl.h"
#include "../easy_mtl.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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

easymtl_bool easymtl_wavefront_is_whitespace(char c)
{
    return c == ' ' || c == '\t';
}

easymtl_bool easymtl_wavefront_is_valid_digit(char c)
{
    return c >= '0' && c <= '9';
}

easymtl_bool easymtl_wavefront_atof(const char* str, const char* strEnd, const char** strEndOut, float* valueOut)
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

easymtl_bool easymtl_wavefront_atof_3(const char* str, const char* strEnd, const char** strEndOut, float valueOut[3])
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


easymtl_bool easymtl_wavefront_parse_K(const char* pDataCur, const char* pDataEnd, float valueOut[3])
{
    assert(pDataCur != NULL);
    assert(pDataEnd != NULL);

    return easymtl_wavefront_atof_3(pDataCur, pDataEnd, &pDataEnd, valueOut);
}

easymtl_bool easymtl_wavefront_parse_N(const char* pDataCur, const char* pDataEnd, float* valueOut)
{
    assert(pDataCur != NULL);
    assert(pDataEnd != NULL);
    
    return easymtl_wavefront_atof(pDataCur, pDataEnd, &pDataEnd, valueOut);
}

easymtl_bool easymtl_wavefront_parse_map(const char* pDataCur, const char* pDataEnd, char* pathOut, unsigned int pathSizeInBytes)
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

                size_t pathLength = pPathEnd - pPathStart;
                if (pathLength + 1 < pathSizeInBytes)
                {
                    memcpy(pathOut, pPathStart, pathLength);
                    pathOut[pathLength] = '\0';

                    return 1;
                }
            }
        }
    }

    return 0;
}


easymtl_bool easymtl_wavefront_seek_to_next_line(easymtl_wavefront* pWavefront)
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

easymtl_bool easymtl_wavefront_seek_to_newmtl(easymtl_wavefront* pWavefront)
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

easymtl_bool easymtl_wavefront_parse(easymtl_wavefront* pWavefront)
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

easymtl_bool easymtl_wavefront_compile(easymtl_material* pMaterial, easymtl_wavefront* pWavefront, const char* texcoordInputName)
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


easymtl_bool easymtl_compile_wavefront_mtl(easymtl_material* pMaterial, const char* mtlData, unsigned int mtlDataSizeInBytes, const char* texcoordInputName)
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