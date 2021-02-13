#include <stdio.h>

static void drflac_matroska_print_hex(drflac_uint64 value) {
    drflac_uint8 thebyte;
    int i;
    printf("ebml as hex: ");
    for(i = 0; i < 8; i++) {
        thebyte = value >> (i*8);
        printf("%02X ", thebyte);                
    }
    printf("\n");    
}

#define DR_FLAC_MAX_EBML_NEST 10
typedef struct {
    drflac_uint64 element_left[DR_FLAC_MAX_EBML_NEST];
    drflac_bool32 element_inf[DR_FLAC_MAX_EBML_NEST];
    drflac_bs *bs;
    drflac_uint32 depth;
} ebml_element_reader;

static void drflac_matroska_ebml_push_element_unchecked(ebml_element_reader *reader, drflac_uint64 size, drflac_bool32 isinf) {
    reader->element_inf[reader->depth]  = isinf;
    reader->element_left[reader->depth] = size;
    reader->depth++;
}

static drflac_bool32 drflac_matroska_ebml_size_infinite(drflac_uint64 size, drflac_uint32 size_width) {
    drflac_uint32 bits = (size_width * 7);     
    int i;
    drflac_uint64 infvalue = 0;
    for( i = 0; i < bits; i++) {
        infvalue = (infvalue << 1) | 0x1;
    }
    return  (size == infvalue);
}

static drflac_bool32 drflac_matroska_ebml_push_element(ebml_element_reader *reader, drflac_uint64 size, drflac_uint32 size_width) {
    drflac_bool32 isinf;    

    if(reader->depth == DR_FLAC_MAX_EBML_NEST) return DRFLAC_FALSE;
    if(reader->depth < 1) return DRFLAC_FALSE;
    isinf = drflac_matroska_ebml_size_infinite(size, size_width);

    /* if the parent element isn't infinite there are restrictions on our range */
    if(!reader->element_inf[reader->depth - 1]) {
        if(isinf) return DRFLAC_FALSE;
        if(reader->element_left[reader->depth - 1] < size) return DRFLAC_FALSE;
    }
    
    drflac_matroska_ebml_push_element_unchecked(reader, size, isinf);
    return DRFLAC_TRUE;
}


static drflac_bool32 drflac_matroska_ebml_ok_to_read(ebml_element_reader *reader, drflac_uint64 bytelen) {
    if(reader->element_inf[reader->depth - 1]) return DRFLAC_TRUE;
    if(reader->element_left[reader->depth - 1] >= bytelen) return DRFLAC_TRUE;
    return DRFLAC_FALSE;
}

static void drflac_matroska_ebml_sub_bytes_read(ebml_element_reader *reader, drflac_uint64 bytesread) {
   
    int i;
    for(i = 0; i < reader->depth; i++) {
        /* make no adjustment to infinite elements */
        if(reader->element_inf[i]) continue;
        reader->element_left[i] -= bytesread;
        /* no read to check child elements if this element ended */
        if(reader->element_left[i] == 0) {
            reader->depth = i;
            return;
        }               
    }
}

static drflac_uint32 drflac_matroska_ebml_vint_width(drflac_uint8 thebyte) {
    drflac_uint32 width;
    for(width = 1; width < 9; width++) {
        if(thebyte & (0x100 >> width)) return width;
    }
    return 0xFFFFFFFF;
}

static drflac_bool32 drflac_matroska_ebml_read_vint(ebml_element_reader *reader, drflac_uint64 *retval, drflac_uint32 *savewidth) {
    drflac_uint32 width;
    drflac_uint64 value = 0;
    drflac_uint8  thebyte;

    drflac_uint32 bytes_read = 0;
    drflac_bool32 ret = DRFLAC_FALSE;
	
    if(!drflac_matroska_ebml_ok_to_read(reader, 1)) return DRFLAC_FALSE;
	if(!drflac__read_uint8(reader->bs, 8, &thebyte)) return DRFLAC_FALSE;
    bytes_read++;
    width = drflac_matroska_ebml_vint_width(thebyte);
    if(width == 0xFFFFFFFF) goto drflac_matroska_ebml_read_vint_ret;
    *savewidth = width;    
    /*printf("embl width %u\n", width);*/
    if(!drflac_matroska_ebml_ok_to_read(reader, width-1))  goto drflac_matroska_ebml_read_vint_ret;

    /* clear VINT_WIDTH and VINT_MARKER */
    thebyte <<= width;
    thebyte >>= width;                                
    /* convert this byte and remaining bytes to an unsigned integer */
    /* https://commandcenter.blogspot.com/2012/04/byte-order-fallacy.html */
    do {
        width--;
        value |= (thebyte << (width*8));
        if(width == 0) {
			/*printf("embl value %u\n", value);
            drflac_matroska_print_hex(value);*/
			*retval = value;
			ret = DRFLAC_TRUE;
            goto drflac_matroska_ebml_read_vint_ret;
		}
		if(!drflac__read_uint8(reader->bs, 8, &thebyte)) goto drflac_matroska_ebml_read_vint_ret;
        bytes_read++;
    } while(1);

    drflac_matroska_ebml_read_vint_ret:
    drflac_matroska_ebml_sub_bytes_read(reader, bytes_read);
    return ret;
}

static drflac_bool32 drflac_bs_read_bytearray(ebml_element_reader *reader, size_t bytelen, drflac_uint8* result) {
	drflac_uint32 bytes_read = 0;
    drflac_bool32 ret = DRFLAC_TRUE;
    int i;

    if(!drflac_matroska_ebml_ok_to_read(reader, bytelen)) return DRFLAC_FALSE;

	for(i = 0; i < bytelen; i++) {
		if( ! drflac__read_uint8(reader->bs, 8, &result[i])) {
            ret = DRFLAC_FALSE;
            break;
        }
        bytes_read++;	
	}
    drflac_matroska_ebml_sub_bytes_read(reader, bytes_read);
	return ret;
}

static drflac_bool32 drflac_matroska_ebml_load_element(ebml_element_reader *reader, drflac_uint64 *id) {
    drflac_uint64 size;
    drflac_uint32 width;

    if(!drflac_matroska_ebml_read_vint(reader, id, &width)) return DRFLAC_FALSE;
    if(!drflac_matroska_ebml_read_vint(reader, &size, &width)) return DRFLAC_FALSE;   
    return drflac_matroska_ebml_push_element(reader, size, width);    
}



static drflac_bool32 drflac_matroska_ebml_read_unsigned(ebml_element_reader *reader, size_t bytelen, drflac_uint64 *value) {
    drflac_uint8 element[8];
    int i;
	
	if(!drflac_bs_read_bytearray(reader, bytelen, element)) return DRFLAC_FALSE;
	
    *value = 0;
    for( i = 0; i < bytelen; i++) {
        *value |= (element[i] << ((bytelen-i-1)*8));
    }
    return DRFLAC_TRUE;
}

static drflac_bool32 drflac_matroska_ebml_read_string(ebml_element_reader *reader, size_t bytelen, drflac_uint8 *string) {
	if(!drflac_bs_read_bytearray(reader, bytelen, string)) return DRFLAC_FALSE;
	string[bytelen] = '\0';
    return DRFLAC_TRUE;
}

static drflac_bool32 drflac_matroska_ebml_seek(ebml_element_reader *reader, size_t bytelen) {
    drflac_uint8 element[8];
    size_t toread;
    while(bytelen > 0) {
        toread = (bytelen > 8) ? 8 : bytelen;
        if(!drflac_bs_read_bytearray(reader, toread, element)) return DRFLAC_FALSE;
        bytelen -= toread;
    }
    return DRFLAC_TRUE;
}

static drflac_bool32 drflac_matroska_ebml_get_element_with_depth(ebml_element_reader *reader, drflac_uint64 *id, drflac_uint32 depth) {
    if(reader->depth != depth) return DRFLAC_FALSE;
    if(!drflac_matroska_ebml_load_element(reader, id)) {
        printf("unable to find element at depth\n");
        return DRFLAC_FALSE;
    }   
    if(reader->depth <= depth) return DRFLAC_FALSE;
    return DRFLAC_TRUE;
}

static size_t drflac__on_read_ebml(void* pUserData, void* bufferOut, size_t bytesToRead)
{
    ebml_element_reader *reader = pUserData;
    if(!drflac_bs_read_bytearray(reader, bytesToRead, bufferOut)) return 0;
    return bytesToRead;
}


static drflac_bool32 drflac__init_private__matroska(drflac_init_info* pInit, drflac_read_proc onRead, drflac_seek_proc onSeek, drflac_meta_proc onMeta, void* pUserData, void* pUserDataMD, drflac_bool32 relaxed)
{    
    ebml_element_reader reader;
    drflac_uint64 id;
    drflac_uint64 content = 0;
    drflac_uint32 width = 0;
    drflac_uint32 size = 0;
    drflac_uint8  string[9];
    
    drflac_streaminfo streaminfo;
    drflac_uint8 isLastBlock;
    drflac_uint8 blockType;
    drflac_uint32 blockSize;

    (void)relaxed;
    
    pInit->container = drflac_container_matroska;

    /* setup our ebml stack */
     reader.depth = 0;
    reader.bs = &pInit->bs;
    drflac_matroska_ebml_push_element_unchecked(&reader, 0, DRFLAC_TRUE);

    /* setup the master element */   
	if(!drflac_matroska_ebml_read_vint(&reader, &content, &width)) return DRFLAC_FALSE;  /* determine the size of the master element. */
    printf("\nebml Master Element, size %u\n", (drflac_uint32)content);   
    if(!drflac_matroska_ebml_push_element(&reader, content, width)) return DRFLAC_FALSE;

    /* seek past the master element */
    if(!drflac_matroska_ebml_seek(&reader, content)) return DRFLAC_FALSE;
    if(reader.depth != 1) return DRFLAC_FALSE;
    
    /* Search for Segment */
    while(drflac_matroska_ebml_get_element_with_depth(&reader, &id, 1)) {
        /* skip past anything that's not segment*/
        if(id != 139690087) {
            size = reader.element_left[reader.depth-1];
            if(!drflac_matroska_ebml_seek(&reader, size)) return DRFLAC_FALSE;
            continue;
        }
        
        /* search for Tracks element */
        while(drflac_matroska_ebml_get_element_with_depth(&reader, &id, 2)) {
            if(id != 106212971) {
                size = reader.element_left[reader.depth-1];
                if(!drflac_matroska_ebml_seek(&reader, size)) return DRFLAC_FALSE;
                continue;                
            }            
            /* Search for Track element */
            while(drflac_matroska_ebml_get_element_with_depth(&reader, &id, 3)) {
                if(id != 46) {
                    size = reader.element_left[reader.depth-1];
                    if(!drflac_matroska_ebml_seek(&reader, size)) return DRFLAC_FALSE;
                    continue;
                }
                /* Search for codec private data */
                while(drflac_matroska_ebml_get_element_with_depth(&reader, &id, 4)) {
                    if(id == 9122) {
                         /* check for fLaC */
                         if(!drflac_matroska_ebml_read_string(&reader, 4, string)) return DRFLAC_FALSE;
                         printf("string %s\n", string);
                         if((string[0] == 'f') && (string[1] == 'L') && (string[2] == 'a') && (string[3] == 'C')) {
                             goto ON_FLAC;
                         }                                                 
                    }                    
                    size = reader.element_left[reader.depth-1];
                    if(!drflac_matroska_ebml_seek(&reader, size)) return DRFLAC_FALSE;                   
                }                
            }
        }        
    }
    return DRFLAC_FALSE;
    
ON_FLAC:
    printf("ON_FLAC\n");
    
    /* The remaining data in the page should be the STREAMINFO block. */    
    if (!drflac__read_and_decode_block_header(drflac__on_read_ebml, &reader, &isLastBlock, &blockType, &blockSize)) {
        return DRFLAC_FALSE;
    }

    if (blockType != DRFLAC_METADATA_BLOCK_TYPE_STREAMINFO || blockSize != 34) {
        return DRFLAC_FALSE;    /* Invalid block type. First block must be the STREAMINFO block. */
    }
    
    if (!drflac__read_streaminfo(drflac__on_read_ebml, &reader, &streaminfo)) {
        return DRFLAC_FALSE;
    }
    
    /* Success! */
    pInit->hasStreamInfoBlock      = DRFLAC_TRUE;
    pInit->sampleRate              = streaminfo.sampleRate;
    pInit->channels                = streaminfo.channels;
    pInit->bitsPerSample           = streaminfo.bitsPerSample;
    pInit->totalPCMFrameCount      = streaminfo.totalPCMFrameCount;
    pInit->maxBlockSizeInPCMFrames = streaminfo.maxBlockSizeInPCMFrames;
    pInit->hasMetadataBlocks       = !isLastBlock;

    if (onMeta) {
        drflac_metadata metadata;
        metadata.type = DRFLAC_METADATA_BLOCK_TYPE_STREAMINFO;
        metadata.pRawData = NULL;
        metadata.rawDataSize = 0;
        metadata.data.streaminfo = streaminfo;
        onMeta(pUserDataMD, &metadata);
    }
    printf("streaminfo loaded\n");
    
    /* disabling metadata for now */
    pInit->hasMetadataBlocks = DRFLAC_FALSE;
    
    return DRFLAC_TRUE;    
}

static size_t drflac__on_read_matroska(void* pUserData, void* bufferOut, size_t bytesToRead) {
    return 0;    
}
