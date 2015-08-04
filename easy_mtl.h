// Version 0.1 - Public Domain. See "unlicense" statement at the end of this file.

#ifndef __easy_mtl_h_
#define __easy_mtl_h_

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable:4201)   // Non-standard extension used: nameless struct/union.
#endif


#define EASYMTL_MAGIC_NUMBER    0x81DF7405
#define EASYMTL_CURRENT_VERSION 1


typedef int           easymtl_bool;
typedef unsigned char easymtl_uint8;


/// The various data type available to the material.
typedef enum
{
	easymtl_type_float   = 1,
	easymtl_type_float2  = 2,
	easymtl_type_float3  = 3,
	easymtl_type_float4  = 4,
    //easymtl_type_int     = 5,
    //easymtl_type_int2    = 6,
    //easymtl_type_int3    = 7,
    //easymtl_type_int4    = 8,
    easymtl_type_tex1d   = 9,
    easymtl_type_tex2d   = 10,
    easymtl_type_tex3d   = 11,
    easymtl_type_texcube = 12,
    easymtl_type_bool    = 13
	
} easymtl_type;

/// The various run-time opcodes.
typedef enum
{
    //////////////////////////
    // Assignment

    // mov
    easymtl_opcode_movf1 = 0x00000001,
    easymtl_opcode_movf2 = 0x00000002,
    easymtl_opcode_movf3 = 0x00000003,
    easymtl_opcode_movf4 = 0x00000004,
    //easymtl_opcode_movi1 = 0x00000005,
    //easymtl_opcode_movi2 = 0x00000006,
    //easymtl_opcode_movi3 = 0x00000007,
    //easymtl_opcode_movi4 = 0x00000008,


    //////////////////////////
    // Arithmetic

    // add
    easymtl_opcode_addf1 = 0x00001001,
    easymtl_opcode_addf2 = 0x00001002,
    easymtl_opcode_addf3 = 0x00001003,
    easymtl_opcode_addf4 = 0x00001004,
    //easymtl_opcode_addi1 = 0x00001005,
    //easymtl_opcode_addi2 = 0x00001006,
    //easymtl_opcode_addi3 = 0x00001007,
    //easymtl_opcode_addi4 = 0x00001008,
    
    // sub
    easymtl_opcode_subf1 = 0x00001101,
    easymtl_opcode_subf2 = 0x00001102,
    easymtl_opcode_subf3 = 0x00001103,
    easymtl_opcode_subf4 = 0x00001104,
    //easymtl_opcode_subi1 = 0x00001105,
    //easymtl_opcode_subi2 = 0x00001106,
    //easymtl_opcode_subi3 = 0x00001107,
    //easymtl_opcode_subi4 = 0x00001108,

    // mul
    easymtl_opcode_mul1 = 0x00001201,
    easymtl_opcode_mulf2 = 0x00001202,
    easymtl_opcode_mulf3 = 0x00001203,
    easymtl_opcode_mulf4 = 0x00001204,
    //easymtl_opcode_muli1 = 0x00001205,
    //easymtl_opcode_muli2 = 0x00001206,
    //easymtl_opcode_muli3 = 0x00001207,
    //easymtl_opcode_muli4 = 0x00001208,

    // div
    easymtl_opcode_divf1 = 0x00001301,
    easymtl_opcode_divf2 = 0x00001302,
    easymtl_opcode_divf3 = 0x00001303,
    easymtl_opcode_divf4 = 0x00001304,
    //easymtl_opcode_divi1 = 0x00001305,
    //easymtl_opcode_divi2 = 0x00001306,
    //easymtl_opcode_divi3 = 0x00001307,
    //easymtl_opcode_divi4 = 0x00001308,

    // pow
    easymtl_opcode_powf1 = 0x00001401,
    easymtl_opcode_powf2 = 0x00001402,
    easymtl_opcode_powf3 = 0x00001403,
    easymtl_opcode_powf4 = 0x00001404,
    //easymtl_opcode_powi1 = 0x00001405,
    //easymtl_opcode_powi2 = 0x00001406,
    //easymtl_opcode_powi3 = 0x00001407,
    //easymtl_opcode_powi4 = 0x00001408,


    //////////////////////////
    // Textures

    // tex
    easymtl_opcode_tex1 = 0x00002001,
    easymtl_opcode_tex2 = 0x00002002,
    easymtl_opcode_tex3 = 0x00002003,
    easymtl_opcode_tex4 = 0x00002004,


    //////////////////////////
    // Miscellaneous

    // ret
    easymtl_opcode_retf1 = 0x00003001,
    easymtl_opcode_retf2 = 0x00003002,
    easymtl_opcode_retf3 = 0x00003003,
    easymtl_opcode_retf4 = 0x00003004,
    //easymtl_opcode_reti1 = 0x00003005,
    //easymtl_opcode_reti2 = 0x00003006,
    //easymtl_opcode_reti3 = 0x00003007,
    //easymtl_opcode_reti4 = 0x00003008,

} easymtl_opcode;


/// Structure containing information about an identifier. An identifier contains a type (float, float2, etc.) and a name. The
/// total size of this structure is 32 bytes. The name is null terminated.
typedef struct
{
    /// The type of the identifier.
	easymtl_type type;

    /// The name of the identifier.
	char name[28];

} easymtl_identifier;


/// Structure containing information about an input variable.
typedef struct
{
    /// The index into the identifier table that this input variable is identified by.
    unsigned int identifierIndex;
	
    /// The default value of the input variable.
	union
	{
		struct
		{
			float x;
		} f1;
		
		struct
		{
			float x;
			float y;
		} f2;
		
		struct
		{
			float x;
			float y;
			float z;
		} f3;
		
		struct
		{
			float x;
			float y;
			float z;
			float w;
		} f4;
		
		struct
		{
			char value[252];	// Enough room for a path, but less to keep the total size of the structure at 256 bytes. Null terminated.
		} path;
	};

} easymtl_input_var;


/// An instruction input value. An input value to an instruction can usually be a constant or the identifier index of the
/// applicable variable.
typedef union
{
    /// The identifier index of the applicable variable.
	unsigned int id;

    /// The constant value, as a float.
	float valuef;

    /// The constant value, as an integer.
    int valuei;

}easymtl_instruction_input;


/// Structure used for describing an instructions input data.
typedef struct
{
	unsigned char x;
	unsigned char y;
	unsigned char z;
	unsigned char w;
	
} easymtl_instruction_input_descriptor;

/// Structure containing information about an instruction.
typedef struct
{
    /// The instruction's opcode.
	easymtl_opcode opcode;
	
    /// The instruction's data.
	union
	{
        // mov data.
		struct
		{
            easymtl_instruction_input_descriptor inputDesc;
			easymtl_instruction_input input0;
			easymtl_instruction_input input1;
			easymtl_instruction_input input2;
			easymtl_instruction_input input3;
			unsigned int output;
		} mov;
		
		
        // add data.
        struct
        {
            easymtl_instruction_input_descriptor inputDesc;
            easymtl_instruction_input input0;
			easymtl_instruction_input input1;
			easymtl_instruction_input input2;
			easymtl_instruction_input input3;
			unsigned int output;
        } add;

        // sub data.
        struct
        {
            easymtl_instruction_input_descriptor inputDesc;
            easymtl_instruction_input input0;
			easymtl_instruction_input input1;
			easymtl_instruction_input input2;
			easymtl_instruction_input input3;
			unsigned int output;
        } sub;

        // mul data.
        struct
        {
            easymtl_instruction_input_descriptor inputDesc;
            easymtl_instruction_input input0;
			easymtl_instruction_input input1;
			easymtl_instruction_input input2;
			easymtl_instruction_input input3;
			unsigned int output;
        } mul;

        // div data.
        struct
        {
            easymtl_instruction_input_descriptor inputDesc;
            easymtl_instruction_input input0;
			easymtl_instruction_input input1;
			easymtl_instruction_input input2;
			easymtl_instruction_input input3;
			unsigned int output;
        } div;

        // pow data.
        struct
        {
            easymtl_instruction_input_descriptor inputDesc;
            easymtl_instruction_input input0;
			easymtl_instruction_input input1;
			easymtl_instruction_input input2;
			easymtl_instruction_input input3;
			unsigned int output;
        } pow;


        // tex data.
        struct
        {
            easymtl_instruction_input_descriptor inputDesc;
            easymtl_instruction_input input0;
			easymtl_instruction_input input1;
			easymtl_instruction_input input2;
			easymtl_instruction_input input3;
            unsigned int texture;
			unsigned int output;
        } tex;


        // Ensures the size of the instruction is always 32 bytes.
        struct
        {
            easymtl_uint8 _unused[32];
        } unused;
	};

} easymtl_instruction;


typedef struct
{
    /// The type of the property.
	easymtl_type type;

    /// The name of the property.
	char name[28];
	
    /// The default value of the input variable.
	union
	{
		struct
		{
			float x;
		} f1;
		
		struct
		{
			float x;
			float y;
		} f2;
		
		struct
		{
			float x;
			float y;
			float z;
		} f3;
		
		struct
		{
			float x;
			float y;
			float z;
			float w;
		} f4;

        struct
        {
            int x;
        } i1;

        struct
        {
            int x;
            int y;
        } i2;

        struct
        {
            int x;
            int y;
            int z;
        } i3;

        struct
        {
            int x;
            int y;
            int z;
            int w;
        } i4;
		
		struct
		{
			char value[224];	// Enough room for a path, but less to keep the total size of the structure at 256 bytes. Null terminated.
		} path;

        struct 
        {
            int x;
        } b1;
	};

} easymtl_property;


/// Structure containing the header information of the material.
typedef struct
{
    /// The magic number: 0x81DF7405
    unsigned int magic;

    /// The version number. There is currently only a single version - version 1. In the future there may be other versions
    /// which will affect how the file is formatted.
    unsigned int version;


    /// The size in bytes of an identifier.
    unsigned int identifierSizeInBytes;

    /// The size in bytes of an input variable.
    unsigned int inputSizeInBytes;

    /// The size in bytes of an instruction.
    unsigned int instructionSizeInBytes;

    /// The size in bytes of a property.
    unsigned int propertySizeInBytes;


    /// The total number of identifiers.
    unsigned int identifierCount;

    /// The total number of private input variables.
    unsigned int privateInputCount;

    /// The total number of public input variables.
    unsigned int publicInputCount;

    /// The total number of channels.
    unsigned int channelCount;

    /// The total number of properties.
    unsigned int propertyCount;


    /// The offset of the identifiers.
    unsigned int identifiersOffset;

    /// The offset of the input variables.
    unsigned int inputsOffset;

    /// The offset of the channels.
    unsigned int channelsOffset;

    /// The offset of the properties.
    unsigned int propertiesOffset;


    /// Padding. Unused.
    unsigned int padding0;


} easymtl_header;

typedef struct
{
    /// The return type of the channel.
	easymtl_type type;

    /// The name of the channel. Null terminated.
	char name[28];

    /// The instruction count of the channel.
    unsigned int instructionCount;

} easymtl_channel_header;


/// Structure containing the definition of the material.
typedef struct
{
    /// A pointer to the raw data. This will at least be the size of easymtl_header (128 bytes, as of version 1).
    easymtl_uint8* pRawData;

    /// The size of the data, in bytes.
    unsigned int sizeInBytes;

    /// The size of the buffer, in bytes. This is used to determine when the buffer needs to be inflated.
    unsigned int bufferSizeInBytes;


    /// The current stage of the construction process of the material. A material must be constructed in order, so this
    /// keeps track of the current stage to ensure the proper errors are returned.
    unsigned int currentStage;

    /// The offset of the current channel. This is needed to we can determine which bytes need to be updated as
    /// instructions are added to the channel.
    unsigned int currentChannelOffset;

} easymtl_material;



////////////////////////////////////////////////////////
// Low-Level APIs
//
// Use these APIs to work on materials directly. Note that these are intentionally restrictive in that things can
// be added to the material, however they can never be removed. In addition, everything must be added in order which
// means identifiers need to be added first, followed by private inputs, followed by public inputs, followed by
// channels, followed by properties.
//
// Use these to construct a material after everything has been processed at a higher level.

/// Initializes the given material.
///
/// @param pMaterial [in] A pointer to the material to initialize.
///
/// @return True if the material is initialized successfully; false otherwise.
easymtl_bool easymtl_init(easymtl_material* pMaterial);

/// Uninitializes the given material.
///
/// @param pMaterial [in] A pointer to the material to uninitialize.
void easymtl_uninit(easymtl_material* pMaterial);


/// Retrieve a pointer to the header information.
easymtl_header* easymtl_getheader(easymtl_material* pMaterial);


/// Appends an identifier to the end of the identifier list. Use easymtl_getidentifiercount() to determine it's index.
///
/// @param pMaterial [in] A pointer to the material to append the identifier to.
easymtl_bool easymtl_appendidentifier(easymtl_material* pMaterial, const easymtl_identifier* pIdentifier);

/// Appends a private input variable.
easymtl_bool easymtl_appendprivateinput(easymtl_material* pMaterial, const easymtl_input_var* pInput);

/// Appends a public input variable.
easymtl_bool easymtl_appendpublicinput(easymtl_material* pMaterial, const easymtl_input_var* pInput);

/// Begins a new channel.
///
/// @remarks
///     Any instructions that are appended from now on will be part of this channel until another channel is begun.
///     @par
///     The end of the channel is marked when a new channel is appended or a property begins.
easymtl_bool easymtl_appendchannel(easymtl_material* pMaterial, const easymtl_channel_header* pChannelHeader);

/// Appends an instruction to the most recently appended channel.
easymtl_bool easymtl_appendinstruction(easymtl_material* pMaterial, const easymtl_instruction* pInstruction);

/// Append a property.
easymtl_bool easymtl_appendproperty(easymtl_material* pMaterial, const easymtl_property* pProperty);



#if defined(_MSC_VER)
    #pragma warning(pop)
#endif


#ifdef __cplusplus
}
#endif

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