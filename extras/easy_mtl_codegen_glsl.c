// Version 0.1 - Public Domain. See "unlicense" statement at the end of this file.

#include "easy_mtl_codegen_glsl.h"
#include "../easy_mtl.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

typedef struct
{
    /// A pointer to the buffer containing the string.
    char* text;

    /// The size of the buffer pointed to by text.
    unsigned int textSizeInBytes;

    /// The length of the string.
    unsigned int length;

} easymtl_output_string;

easymtl_bool easymtl_write_string(easymtl_output_string* pOutput, const char* src)
{
    assert(pOutput != NULL);

    unsigned int dstSizeInBytes = (pOutput->textSizeInBytes - pOutput->length);
    while (dstSizeInBytes > 0 && src[0] != '\0')
    {
        pOutput->text[pOutput->length + 0] = src[0];

        pOutput->length += 1;
        src += 1;
        dstSizeInBytes -= 1;
    }

    if (dstSizeInBytes > 0)
    {
        // There's enough room for the null terminator which means there was enough room in the buffer. All good.
        pOutput->text[pOutput->length] = '\0';
        return 1;
    }
    else
    {
        // There's not enough room for the null terminator which means there was NOT enough room in the buffer. Error.
        return 0;
    }
}

easymtl_bool easymtl_write_float(easymtl_output_string* pOutput, float src)
{
    assert(pOutput != NULL);

    char str[32];
    snprintf(str, 32, "%f", src);

    return easymtl_write_string(pOutput, str);
}

easymtl_bool easymtl_write_int(easymtl_output_string* pOutput, int src)
{
    assert(pOutput != NULL);

    char str[32];
    snprintf(str, 32, "%d", src);

    return easymtl_write_string(pOutput, str);
}

easymtl_bool easymtl_write_type(easymtl_output_string* pOutput, easymtl_type type)
{
    assert(pOutput != NULL);

    switch (type)
    {
    case easymtl_type_float:
        {
            if (!easymtl_write_string(pOutput, "float"))
            {
                return 0;
            }

            break;
        }

    case easymtl_type_float2:
        {
            if (!easymtl_write_string(pOutput, "vec2"))
            {
                return 0;
            }

            break;
        }

    case easymtl_type_float3:
        {
            if (!easymtl_write_string(pOutput, "vec3"))
            {
                return 0;
            }

            break;
        }

    case easymtl_type_float4:
        {
            if (!easymtl_write_string(pOutput, "vec4"))
            {
                return 0;
            }

            break;
        }

    case easymtl_type_tex1d:
        {
            if (!easymtl_write_string(pOutput, "sampler1D"))
            {
                return 0;
            }

            break;
        }

    case easymtl_type_tex2d:
        {
            if (!easymtl_write_string(pOutput, "sampler2D"))
            {
                return 0;
            }

            break;
        }

    case easymtl_type_tex3d:
        {
            if (!easymtl_write_string(pOutput, "sampler3D"))
            {
                return 0;
            }

            break;
        }

    case easymtl_type_texcube:
        {
            if (!easymtl_write_string(pOutput, "samplerCube"))
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

easymtl_bool easymtl_write_channel_function_begin(easymtl_output_string* pOutput, easymtl_channel_header* pChannelHeader)
{
    assert(pOutput        != NULL);
    assert(pChannelHeader != NULL);

    // <type> <name> {\n
    return easymtl_write_type(pOutput, pChannelHeader->channel.type) && easymtl_write_string(pOutput, " ") && easymtl_write_string(pOutput, pChannelHeader->channel.name) && easymtl_write_string(pOutput, "() {\n");
}

easymtl_bool easymtl_write_channel_function_close(easymtl_output_string* pOutput)
{
    assert(pOutput != NULL);

    return easymtl_write_string(pOutput, "}\n");
}

easymtl_bool easymtl_glsl_write_instruction_input_scalar(easymtl_output_string* pOutput, easymtl_identifier* pIdentifiers, unsigned char descriptor, easymtl_instruction_input* pInput)
{
    assert(pOutput      != NULL);
    assert(pIdentifiers != NULL);
    assert(pInput       != NULL);

    if (descriptor == EASYMTL_INPUT_DESC_CONSTF)
    {
        // It's a constant float.
        return easymtl_write_float(pOutput, pInput->valuef);
    }
    else if (descriptor == EASYMTL_INPUT_DESC_CONSTI)
    {
        // It's a constant int.
        return easymtl_write_int(pOutput, pInput->valuei);
    }
    else
    {
        // It's a variable.
        easymtl_identifier* pIdentifier = pIdentifiers + pInput->id;
        assert(pIdentifier != NULL);

        if (pIdentifier->type == easymtl_type_float)
        {
            // The input variable is a float, so we don't want to use any selectors.
            return easymtl_write_string(pOutput, pIdentifier->name);
        }
        else
        {
            if (easymtl_write_string(pOutput, pIdentifier->name) && easymtl_write_string(pOutput, "."))
            {
                switch (descriptor)
                {
                case 0: return easymtl_write_string(pOutput, "x");
                case 1: return easymtl_write_string(pOutput, "y");
                case 2: return easymtl_write_string(pOutput, "z");
                case 3: return easymtl_write_string(pOutput, "w");
                default: return 0;
                }
            }
        }
    }

    return 0;
}

easymtl_bool easymtl_glsl_write_instruction_input_initializer(easymtl_output_string* pOutput, easymtl_type type, easymtl_identifier* pIdentifiers, easymtl_instruction_input_descriptor inputDesc, easymtl_instruction_input* pInputs)
{
    assert(pOutput      != NULL);
    assert(pIdentifiers != NULL);
    assert(pInputs      != NULL);

    switch (type)
    {
    case easymtl_type_float:
        {
            return easymtl_glsl_write_instruction_input_scalar(pOutput, pIdentifiers, inputDesc.x, pInputs + 0);
        }

    case easymtl_type_float2:
        {
            if (easymtl_write_string(pOutput, "vec2("))
            {
                if (easymtl_glsl_write_instruction_input_scalar(pOutput, pIdentifiers, inputDesc.x, pInputs + 0) && easymtl_write_string(pOutput, ", ") &&
                    easymtl_glsl_write_instruction_input_scalar(pOutput, pIdentifiers, inputDesc.y, pInputs + 1))
                {
                    return easymtl_write_string(pOutput, ")");
                }
            }

            break;
        }

    case easymtl_type_float3:
        {
            if (easymtl_write_string(pOutput, "vec3("))
            {
                if (easymtl_glsl_write_instruction_input_scalar(pOutput, pIdentifiers, inputDesc.x, pInputs + 0) && easymtl_write_string(pOutput, ", ") &&
                    easymtl_glsl_write_instruction_input_scalar(pOutput, pIdentifiers, inputDesc.y, pInputs + 1) && easymtl_write_string(pOutput, ", ") &&
                    easymtl_glsl_write_instruction_input_scalar(pOutput, pIdentifiers, inputDesc.z, pInputs + 2))
                {
                    return easymtl_write_string(pOutput, ")");
                }
            }

            break;
        }

    case easymtl_type_float4:
        {
            if (easymtl_write_string(pOutput, "vec4("))
            {
                if (easymtl_glsl_write_instruction_input_scalar(pOutput, pIdentifiers, inputDesc.x, pInputs + 0) && easymtl_write_string(pOutput, ", ") &&
                    easymtl_glsl_write_instruction_input_scalar(pOutput, pIdentifiers, inputDesc.y, pInputs + 1) && easymtl_write_string(pOutput, ", ") &&
                    easymtl_glsl_write_instruction_input_scalar(pOutput, pIdentifiers, inputDesc.z, pInputs + 2) && easymtl_write_string(pOutput, ", ") &&
                    easymtl_glsl_write_instruction_input_scalar(pOutput, pIdentifiers, inputDesc.w, pInputs + 3))
                {
                    return easymtl_write_string(pOutput, ")");
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

easymtl_bool easymtl_write_instruction_mov(easymtl_output_string* pOutput, easymtl_instruction* pInstruction, easymtl_identifier* pIdentifiers)
{
    assert(pOutput      != NULL);
    assert(pInstruction != NULL);
    
    easymtl_identifier* pOutputIdentifier = pIdentifiers + pInstruction->mov.output;
    assert(pOutputIdentifier != NULL);

    if (easymtl_write_string(pOutput, pOutputIdentifier->name) && easymtl_write_string(pOutput, " = "))
    {
        easymtl_type type;
        switch (pInstruction->opcode)
        {
        case easymtl_opcode_movf1: type = easymtl_type_float;  break;
        case easymtl_opcode_movf2: type = easymtl_type_float2; break;
        case easymtl_opcode_movf3: type = easymtl_type_float3; break;
        case easymtl_opcode_movf4: type = easymtl_type_float4; break;
        default: return 0;
        }

        return easymtl_glsl_write_instruction_input_initializer(pOutput, type, pIdentifiers, pInstruction->mov.inputDesc, &pInstruction->mov.inputX) && easymtl_write_string(pOutput, ";\n");
    }
    
    return 0;
}

easymtl_bool easymtl_write_instruction_add(easymtl_output_string* pOutput, easymtl_instruction* pInstruction, easymtl_identifier* pIdentifiers)
{
    assert(pOutput      != NULL);
    assert(pInstruction != NULL);

    easymtl_identifier* pOutputIdentifier = pIdentifiers + pInstruction->add.output;
    assert(pOutputIdentifier != NULL);

    if (easymtl_write_string(pOutput, pOutputIdentifier->name) && easymtl_write_string(pOutput, " += "))
    {
        easymtl_type type;
        switch (pInstruction->opcode)
        {
        case easymtl_opcode_addf1: type = easymtl_type_float;  break;
        case easymtl_opcode_addf2: type = easymtl_type_float2; break;
        case easymtl_opcode_addf3: type = easymtl_type_float3; break;
        case easymtl_opcode_addf4: type = easymtl_type_float4; break;
        default: return 0;
        }

        return easymtl_glsl_write_instruction_input_initializer(pOutput, type, pIdentifiers, pInstruction->add.inputDesc, &pInstruction->add.inputX) && easymtl_write_string(pOutput, ";\n");
    }
    
    return 0;
}

easymtl_bool easymtl_write_instruction_sub(easymtl_output_string* pOutput, easymtl_instruction* pInstruction, easymtl_identifier* pIdentifiers)
{
    assert(pOutput      != NULL);
    assert(pInstruction != NULL);
    
    easymtl_identifier* pOutputIdentifier = pIdentifiers + pInstruction->sub.output;
    assert(pOutputIdentifier != NULL);

    if (easymtl_write_string(pOutput, pOutputIdentifier->name) && easymtl_write_string(pOutput, " -= "))
    {
        easymtl_type type;
        switch (pInstruction->opcode)
        {
        case easymtl_opcode_subf1: type = easymtl_type_float;  break;
        case easymtl_opcode_subf2: type = easymtl_type_float2; break;
        case easymtl_opcode_subf3: type = easymtl_type_float3; break;
        case easymtl_opcode_subf4: type = easymtl_type_float4; break;
        default: return 0;
        }

        return easymtl_glsl_write_instruction_input_initializer(pOutput, type, pIdentifiers, pInstruction->sub.inputDesc, &pInstruction->sub.inputX) && easymtl_write_string(pOutput, ";\n");
    }

    return 0;
}

easymtl_bool easymtl_write_instruction_mul(easymtl_output_string* pOutput, easymtl_instruction* pInstruction, easymtl_identifier* pIdentifiers)
{
    assert(pOutput      != NULL);
    assert(pInstruction != NULL);

    easymtl_identifier* pOutputIdentifier = pIdentifiers + pInstruction->mul.output;
    assert(pOutputIdentifier != NULL);

    if (easymtl_write_string(pOutput, pOutputIdentifier->name) && easymtl_write_string(pOutput, " *= "))
    {
        easymtl_type type;
        switch (pInstruction->opcode)
        {
        case easymtl_opcode_mulf1: type = easymtl_type_float;  break;
        case easymtl_opcode_mulf2: type = easymtl_type_float2; break;
        case easymtl_opcode_mulf3: type = easymtl_type_float3; break;
        case easymtl_opcode_mulf4: type = easymtl_type_float4; break;
        default: return 0;
        }

        return easymtl_glsl_write_instruction_input_initializer(pOutput, type, pIdentifiers, pInstruction->mul.inputDesc, &pInstruction->mul.inputX) && easymtl_write_string(pOutput, ";\n");
    }

    return 0;
}

easymtl_bool easymtl_write_instruction_div(easymtl_output_string* pOutput, easymtl_instruction* pInstruction, easymtl_identifier* pIdentifiers)
{
    assert(pOutput      != NULL);
    assert(pInstruction != NULL);

    easymtl_identifier* pOutputIdentifier = pIdentifiers + pInstruction->div.output;
    assert(pOutputIdentifier != NULL);

    if (easymtl_write_string(pOutput, pOutputIdentifier->name) && easymtl_write_string(pOutput, " = "))
    {
        easymtl_type type;
        switch (pInstruction->opcode)
        {
        case easymtl_opcode_divf1: type = easymtl_type_float;  break;
        case easymtl_opcode_divf2: type = easymtl_type_float2; break;
        case easymtl_opcode_divf3: type = easymtl_type_float3; break;
        case easymtl_opcode_divf4: type = easymtl_type_float4; break;
        default: return 0;
        }

        return easymtl_glsl_write_instruction_input_initializer(pOutput, type, pIdentifiers, pInstruction->div.inputDesc, &pInstruction->div.inputX) && easymtl_write_string(pOutput, ";\n");
    }

    return 0;
}

easymtl_bool easymtl_write_instruction_pow(easymtl_output_string* pOutput, easymtl_instruction* pInstruction, easymtl_identifier* pIdentifiers)
{
    assert(pOutput      != NULL);
    assert(pInstruction != NULL);

    easymtl_identifier* pOutputIdentifier = pIdentifiers + pInstruction->pow.output;
    assert(pOutputIdentifier != NULL);

    if (easymtl_write_string(pOutput, pOutputIdentifier->name) && easymtl_write_string(pOutput, " = pow(") && easymtl_write_string(pOutput, pOutputIdentifier->name) && easymtl_write_string(pOutput, ", ")) 
    {
        easymtl_type type;
        switch (pInstruction->opcode)
        {
        case easymtl_opcode_powf1: type = easymtl_type_float;  break;
        case easymtl_opcode_powf2: type = easymtl_type_float2; break;
        case easymtl_opcode_powf3: type = easymtl_type_float3; break;
        case easymtl_opcode_powf4: type = easymtl_type_float4; break;
        default: return 0;
        }

        return easymtl_glsl_write_instruction_input_initializer(pOutput, type, pIdentifiers, pInstruction->pow.inputDesc, &pInstruction->pow.inputX) && easymtl_write_string(pOutput, ");\n");
    }

    return 0;
}

easymtl_bool easymtl_write_instruction_tex(easymtl_output_string* pOutput, easymtl_instruction* pInstruction, easymtl_identifier* pIdentifiers)
{
    assert(pOutput      != NULL);
    assert(pInstruction != NULL);
    
    easymtl_identifier* pOutputIdentifier = pIdentifiers + pInstruction->tex.output;
    assert(pOutputIdentifier != NULL);

    easymtl_identifier* pTextureIdentifier = pIdentifiers + pInstruction->tex.texture;
    assert(pTextureIdentifier != NULL);

    if (easymtl_write_string(pOutput, pOutputIdentifier->name) && easymtl_write_string(pOutput, " = "))
    {
        easymtl_type type;
        switch (pInstruction->opcode)
        {
        case easymtl_opcode_tex1:
        {
            type = easymtl_type_float;
            if (!easymtl_write_string(pOutput, "texture1D("))
            {
                return 0;
            }

            break;
        }

        case easymtl_opcode_tex2:
        {
            type = easymtl_type_float2;
            if (!easymtl_write_string(pOutput, "texture2D("))
            {
                return 0;
            }

            break;
        }

        case easymtl_opcode_tex3:
        {
            type = easymtl_type_float3;
            if (!easymtl_write_string(pOutput, "texture3D("))
            {
                return 0;
            }

            break;
        }

        case easymtl_opcode_texcube:
        {
            type = easymtl_type_float3;
            if (!easymtl_write_string(pOutput, "textureCube("))
            {
                return 0;
            }

            break;
        }

        default: return 0;
        }

        return
            easymtl_write_string(pOutput, pTextureIdentifier->name) && easymtl_write_string(pOutput, ", ") &&
            easymtl_glsl_write_instruction_input_initializer(pOutput, type, pIdentifiers, pInstruction->tex.inputDesc, &pInstruction->tex.inputX) && easymtl_write_string(pOutput, ");\n");
    }

    return 0;
}

easymtl_bool easymtl_write_instruction_var(easymtl_output_string* pOutput, easymtl_instruction* pInstruction, easymtl_identifier* pIdentifiers)
{
    assert(pOutput      != NULL);
    assert(pInstruction != NULL);

    easymtl_identifier* pIdentifier = pIdentifiers + pInstruction->var.identifierIndex;
    assert(pIdentifier != NULL);
    
    return easymtl_write_type(pOutput, pIdentifier->type) && easymtl_write_string(pOutput, " ") && easymtl_write_string(pOutput, pIdentifier->name) && easymtl_write_string(pOutput, ";\n");
}

easymtl_bool easymtl_write_instruction_ret(easymtl_output_string* pOutput, easymtl_instruction* pInstruction, easymtl_identifier* pIdentifiers)
{
    assert(pOutput      != NULL);
    assert(pInstruction != NULL);
    
    if (easymtl_write_string(pOutput, "return "))
    {
        easymtl_type type;
        switch (pInstruction->opcode)
        {
        case easymtl_opcode_retf1: type = easymtl_type_float;  break;
        case easymtl_opcode_retf2: type = easymtl_type_float2; break;
        case easymtl_opcode_retf3: type = easymtl_type_float3; break;
        case easymtl_opcode_retf4: type = easymtl_type_float4; break;
        default: return 0;
        }

        return easymtl_glsl_write_instruction_input_initializer(pOutput, type, pIdentifiers, pInstruction->ret.inputDesc, &pInstruction->ret.inputX) && easymtl_write_string(pOutput, ";\n");
    }

    return 0;
}


easymtl_bool easymtl_write_instruction(easymtl_output_string* pOutput, easymtl_instruction* pInstruction, easymtl_identifier* pIdentifiers)
{
    assert(pOutput      != NULL);
    assert(pInstruction != NULL);

    switch (pInstruction->opcode)
    {
    case easymtl_opcode_movf1:
    case easymtl_opcode_movf2:
    case easymtl_opcode_movf3:
    case easymtl_opcode_movf4:
        {
            return easymtl_write_instruction_mov(pOutput, pInstruction, pIdentifiers);
        }


    case easymtl_opcode_addf1:
    case easymtl_opcode_addf2:
    case easymtl_opcode_addf3:
    case easymtl_opcode_addf4:
        {
            return easymtl_write_instruction_add(pOutput, pInstruction, pIdentifiers);
        }

    case easymtl_opcode_subf1:
    case easymtl_opcode_subf2:
    case easymtl_opcode_subf3:
    case easymtl_opcode_subf4:
        {
            return easymtl_write_instruction_sub(pOutput, pInstruction, pIdentifiers);
        }

    case easymtl_opcode_mulf1:
    case easymtl_opcode_mulf2:
    case easymtl_opcode_mulf3:
    case easymtl_opcode_mulf4:
        {
            return easymtl_write_instruction_mul(pOutput, pInstruction, pIdentifiers);
        }

    case easymtl_opcode_divf1:
    case easymtl_opcode_divf2:
    case easymtl_opcode_divf3:
    case easymtl_opcode_divf4:
        {
            return easymtl_write_instruction_div(pOutput, pInstruction, pIdentifiers);
        }

    case easymtl_opcode_powf1:
    case easymtl_opcode_powf2:
    case easymtl_opcode_powf3:
    case easymtl_opcode_powf4:
        {
            return easymtl_write_instruction_pow(pOutput, pInstruction, pIdentifiers);
        }

    case easymtl_opcode_tex1:
    case easymtl_opcode_tex2:
    case easymtl_opcode_tex3:
    case easymtl_opcode_texcube:
        {
            return easymtl_write_instruction_tex(pOutput, pInstruction, pIdentifiers);
        }


    case easymtl_opcode_var:
        {
            return easymtl_write_instruction_var(pOutput, pInstruction, pIdentifiers);
        }

    case easymtl_opcode_retf1:
    case easymtl_opcode_retf2:
    case easymtl_opcode_retf3:
    case easymtl_opcode_retf4:
        {
            return easymtl_write_instruction_ret(pOutput, pInstruction, pIdentifiers);
        }


    default:
        {
            // Unknown or unsupported opcode.
            return 0;
        }
    }
}

easymtl_bool easymtl_write_channel_instructions(easymtl_output_string* pOutput, easymtl_instruction* pInstructions, unsigned int instructionCount, easymtl_identifier* pIdentifiers)
{
    assert(pOutput       != NULL);
    assert(pInstructions != NULL);

    for (unsigned int iInstruction = 0; iInstruction < instructionCount; ++iInstruction)
    {
        easymtl_instruction* pInstruction = pInstructions + iInstruction;
        assert(pInstruction != NULL);

        if (!easymtl_write_instruction(pOutput, pInstruction, pIdentifiers))
        {
            return 0;
        }
    }

    return 1;
}


easymtl_bool easymtl_codegen_glsl_channel(easymtl_material* pMaterial, const char* channelName, char* codeOut, unsigned int codeOutSizeInBytes)
{
    if (pMaterial != NULL && codeOut != NULL && codeOutSizeInBytes > 0)
    {
        easymtl_header* pHeader = easymtl_getheader(pMaterial);
        if (pHeader != NULL)
        {
            easymtl_channel_header* pChannelHeader = easymtl_getchannelheaderbyname(pMaterial, channelName);
            if (pChannelHeader != NULL)
            {
                easymtl_output_string output;
                output.text            = codeOut;
                output.textSizeInBytes = codeOutSizeInBytes;
                output.length          = 0;

                if (easymtl_write_channel_function_begin(&output, pChannelHeader))
                {
                    easymtl_instruction* pInstructions = (easymtl_instruction*)(pChannelHeader + 1);
                    assert(pInstructions != NULL);

                    easymtl_identifier* pIdentifiers = easymtl_getidentifiers(pMaterial);
                    assert(pIdentifiers != NULL);

                    if (easymtl_write_channel_instructions(&output, pInstructions, pChannelHeader->instructionCount, pIdentifiers))
                    {
                        return easymtl_write_channel_function_close(&output);
                    }
                }
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