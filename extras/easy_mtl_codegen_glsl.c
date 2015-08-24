// Version 0.1 - Public Domain. See "unlicense" statement at the end of this file.

#include "easy_mtl_codegen_glsl.h"
#include "../easy_mtl.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>


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

easymtl_bool easymtl_codegen_glsl_write(easymtl_codegen_glsl* pCodegen, const char* src)
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

easymtl_bool easymtl_codegen_glsl_write_float(easymtl_codegen_glsl* pCodegen, float src)
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

easymtl_bool easymtl_codegen_glsl_write_int(easymtl_codegen_glsl* pCodegen, int src)
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

easymtl_bool easymtl_codegen_glsl_write_indentation(easymtl_codegen_glsl* pCodegen)
{
    assert(pCodegen != NULL);

    for (unsigned int i = 0; i < pCodegen->indentationLevel; ++i)
    {
        easymtl_codegen_glsl_write(pCodegen, " ");
    }

    return 1;
}

easymtl_bool easymtl_codegen_glsl_write_type(easymtl_codegen_glsl* pCodegen, easymtl_type type)
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

easymtl_bool easymtl_codegen_glsl_write_instruction_input_scalar(easymtl_codegen_glsl* pCodegen, unsigned char descriptor, easymtl_instruction_input* pInput)
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

easymtl_bool easymtl_codegen_glsl_write_instruction_input_initializer(easymtl_codegen_glsl* pCodegen, easymtl_type type, easymtl_instruction_input_descriptor inputDesc, easymtl_instruction_input* pInputs)
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


easymtl_bool easymtl_codegen_glsl_write_instruction_mov(easymtl_codegen_glsl* pCodegen, easymtl_instruction* pInstruction)
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

easymtl_bool easymtl_codegen_glsl_write_instruction_add(easymtl_codegen_glsl* pCodegen, easymtl_instruction* pInstruction)
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

easymtl_bool easymtl_codegen_glsl_write_instruction_sub(easymtl_codegen_glsl* pCodegen, easymtl_instruction* pInstruction)
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

easymtl_bool easymtl_codegen_glsl_write_instruction_mul(easymtl_codegen_glsl* pCodegen, easymtl_instruction* pInstruction)
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

easymtl_bool easymtl_codegen_glsl_write_instruction_div(easymtl_codegen_glsl* pCodegen, easymtl_instruction* pInstruction)
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

easymtl_bool easymtl_codegen_glsl_write_instruction_pow(easymtl_codegen_glsl* pCodegen, easymtl_instruction* pInstruction)
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

easymtl_bool easymtl_codegen_glsl_write_instruction_tex(easymtl_codegen_glsl* pCodegen, easymtl_instruction* pInstruction)
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

easymtl_bool easymtl_codegen_glsl_write_instruction_var(easymtl_codegen_glsl* pCodegen, easymtl_instruction* pInstruction)
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

easymtl_bool easymtl_codegen_glsl_write_instruction_ret(easymtl_codegen_glsl* pCodegen, easymtl_instruction* pInstruction)
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

easymtl_bool easymtl_codegen_glsl_write_instruction(easymtl_codegen_glsl* pCodegen, easymtl_instruction* pInstruction)
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

easymtl_bool easymtl_codegen_glsl_write_instructions(easymtl_codegen_glsl* pCodegen, easymtl_instruction* pInstructions, unsigned int instructionCount)
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

easymtl_bool easymtl_codegen_glsl_channel_function_begin(easymtl_codegen_glsl* pCodegen, easymtl_channel_header* pChannelHeader)
{
    assert(pCodegen       != NULL);
    assert(pChannelHeader != NULL);

    // <type> <name> {\n
    easymtl_bool result =
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

easymtl_bool easymtl_codegen_glsl_channel_function_close(easymtl_codegen_glsl* pCodegen)
{
    assert(pCodegen != NULL);

    if (pCodegen->indentationLevel > 4) {
        pCodegen->indentationLevel -= 4;
    } else {
        pCodegen->indentationLevel = 0;
    }

    return easymtl_codegen_glsl_write(pCodegen, "}\n");
}

easymtl_bool easymtl_codegen_glsl_channel(easymtl_material* pMaterial, const char* channelName, char* codeOut, unsigned int codeOutSizeInBytes, unsigned int* pBytesWrittenOut)
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
                        easymtl_bool result = easymtl_codegen_glsl_channel_function_close(&codegen);
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



easymtl_bool easymtl_codegen_glsl_uniform(easymtl_codegen_glsl* pCodegen, easymtl_input* pInput)
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

easymtl_bool easymtl_codegen_glsl_uniforms(easymtl_material* pMaterial, char* codeOut, unsigned int codeOutSizeInBytes, unsigned int* pBytesWritteOut)
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
