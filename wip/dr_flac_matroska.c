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

static size_t drflac__on_read_ebml(void* pUserData, void* bufferOut, size_t bytesToRead) {
    ebml_element_reader *reader = pUserData;
    drflac_uint32 bytes_read = 0;
    drflac_bool32 ret = DRFLAC_TRUE;
    int i;
    
    if(!drflac_matroska_ebml_ok_to_read(reader, bytesToRead)) return 0;

    bytes_read = reader->onRead(reader->pUserData, bufferOut, bytesToRead);
    drflac_matroska_ebml_sub_bytes_read(reader, bytes_read);
    reader->offset += bytes_read;
    return bytes_read;
}

static drflac_bool32 drflac__on_seek_ebml(void* pUserData, int offset, drflac_seek_origin origin) {
    ebml_element_reader *reader = pUserData;
    if(origin == drflac_seek_origin_current) {
        if(!drflac_matroska_ebml_ok_to_read(reader, offset)) return DRFLAC_FALSE;
        if(!reader->onSeek(reader->pUserData, offset, origin)) return DRFLAC_FALSE;
        drflac_matroska_ebml_sub_bytes_read(reader, offset);
        reader->offset += offset;
        return DRFLAC_TRUE;
    }
    return DRFLAC_FALSE;
}

/* used for seeking when we're not allowed to seek such as when reading */ 
static drflac_bool32 drflac_matroska_ebml_read_seek(ebml_element_reader *reader, size_t bytelen) {
    drflac_uint8 element[8];
    size_t toread;
    while(bytelen > 0) {
        toread = (bytelen > 8) ? 8 : bytelen;
        if(drflac__on_read_ebml(reader, element, toread) != toread) return DRFLAC_FALSE;
        bytelen -= toread;
    }
    return DRFLAC_TRUE;
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

    if(drflac__on_read_ebml(reader, &thebyte, 1) != 1) return DRFLAC_FALSE;    
    bytes_read++;
    width = drflac_matroska_ebml_vint_width(thebyte);
    if(width == 0xFFFFFFFF) goto drflac_matroska_ebml_read_vint_ret;
    *savewidth = width;

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
		if(drflac__on_read_ebml(reader, &thebyte, 1) != 1) goto drflac_matroska_ebml_read_vint_ret;
        bytes_read++;
    } while(1);

    drflac_matroska_ebml_read_vint_ret:
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

    if(drflac__on_read_ebml(reader, value, bytelen) != bytelen) return DRFLAC_FALSE;	
	
    *value = 0;
    for( i = 0; i < bytelen; i++) {
        *value |= (element[i] << ((bytelen-i-1)*8));
    }
    return DRFLAC_TRUE;
}

static drflac_bool32 drflac_matroska_ebml_read_string(ebml_element_reader *reader, size_t bytelen, drflac_uint8 *string) {
	if(drflac__on_read_ebml(reader, string, bytelen) != bytelen) return DRFLAC_FALSE;
	string[bytelen] = '\0';
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

/*
static drflac_bool32 drflac_matroska_get_flac_codec_privdata(ebml_element_reader *reader) {
    if(reader->depth == 2) {
                    
    }    
}
*/

static drflac_bool32 drflac_matroska_get_simpleblock(ebml_element_reader *reader) {
    drflac_uint64 id, content, content1, content2;
    size_t size;
    drflac_uint32 width;
     
    if(reader->depth == 2) {
        /* seek to a cluster*/
        while(drflac_matroska_ebml_get_element_with_depth(reader, &id, 2)) {
            size = reader->element_left[reader->depth-1];
            /*printf("element id %u, size %u\n", id, size);*/
            if(id == 256095861) break;         
            if(!drflac_matroska_ebml_read_seek(reader, size)) return DRFLAC_FALSE;
        }            
    }

    if(reader->depth == 3) {
        /* seek to a simple block */
        while(drflac_matroska_ebml_get_element_with_depth(reader, &id, 3)) {
            size = reader->element_left[reader->depth-1];
            /*printf("element id %u, size %u\n", id, size);*/
            if(id == 35) {
                /* read Simple block */
                if(!drflac_matroska_ebml_read_vint(reader, &content, &width)) return DRFLAC_FALSE;
                /* timestamp s16*/
                if(!drflac_matroska_ebml_read_unsigned(reader, 2, &content1)) return DRFLAC_FALSE;
                /* flag */
                if(!drflac_matroska_ebml_read_unsigned(reader, 1, &content2)) return DRFLAC_FALSE;
                /*printf("track: %u timestamp %u flag %u\n", content, content1, content2);*/
                break;
            }      
            if(!drflac_matroska_ebml_read_seek(reader, size)) return DRFLAC_FALSE;            
        }            
    }
    return reader->depth == 4;    
}

static drflac_bool32 drflac__init_private__matroska(drflac_init_info* pInit, drflac_read_proc onRead, drflac_seek_proc onSeek, drflac_meta_proc onMeta, void* pUserData, void* pUserDataMD, drflac_bool32 relaxed)
{    
    ebml_element_reader reader;
    drflac_uint64 id;
    drflac_uint64 content = 0;
    drflac_uint64 content1 = 0;
    drflac_uint64 content2 = 0;
    drflac_uint32 width = 0;
    drflac_uint32 size = 0;
    drflac_uint8  string[9];
    
    drflac_uint32 depth;
    
    drflac_streaminfo streaminfo;
    drflac_uint8 isLastBlock;
    drflac_uint8 blockType;
    drflac_uint32 blockSize;

    (void)relaxed;
    
    pInit->container = drflac_container_matroska;

    /* setup our ebml stack */
    reader.depth = 0;
    reader.onRead = onRead;
    reader.onSeek = onSeek;
    reader.pUserData = pUserData;
    reader.offset = 0x4;
    reader.flac_start_offset = 0;
    reader.flac_priv_size = 0;
    reader.audio_start_offset = 0;

    drflac_matroska_ebml_push_element_unchecked(&reader, 0, DRFLAC_TRUE);

    
    /*Don't care about master element, just skip past it*/
	if(!drflac_matroska_ebml_read_vint(&reader, &content, &width)) return DRFLAC_FALSE;  
    if(!drflac_matroska_ebml_push_element(&reader, content, width)) return DRFLAC_FALSE;
    if(!drflac_matroska_ebml_read_seek(&reader, content)) return DRFLAC_FALSE;
    if(reader.depth != 1) return DRFLAC_FALSE;
    
    
    
    for(;;) {
        depth = reader.depth;
        if(!drflac_matroska_ebml_load_element(&reader, &id)) break;
        size = reader.element_left[reader.depth-1];
        /* Segment */
        if((depth == 1) && (id == 139690087)) {
            continue;            
        }
        /* Tracks */
        else if((depth == 2) && (id == 106212971)) {
            continue;
        }
        /* Track */
        else if((depth == 3) && (id == 46)) {
            continue;            
        }
        /* Codec priv data */
        else if((depth == 4) && (id == 9122)) {
            if(!drflac_matroska_ebml_read_string(&reader, 4, string)) break;
            if((string[0] == 'f') && (string[1] == 'L') && (string[2] == 'a') && (string[3] == 'C')) {
                reader.flac_start_offset = reader.offset - 4;
                reader.flac_priv_size = size;
                goto ON_FLAC;
            }            
        }
        
        /*printf("skipping depth %u id %u size %u\n", depth, id, size);*/ 
        /* skip over segment otherwise */
        if(!drflac_matroska_ebml_read_seek(&reader, size)) return DRFLAC_FALSE;  
    }
    
   
    return DRFLAC_FALSE;
    
ON_FLAC:
    /*printf("ON_FLAC\n");*/
    
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
    /*printf("streaminfo loaded\n");*/
    
    /* disabling metadata for now */
    pInit->hasMetadataBlocks = DRFLAC_FALSE;

    /* for now seek to the first FLAC frame*/
    /* seek out to just Segment level */
    while(reader.depth > 2) {
        size = reader.element_left[reader.depth-1];
        if(!drflac_matroska_ebml_read_seek(&reader, size)) return DRFLAC_FALSE;
    }

    /* Tags  39109479 */
    /* Cluster 256095861 */
      /* CRC 63*/
      /* cluster timestamp 103  */
      /* Simple block 35*/
    if(!drflac_matroska_get_simpleblock(&reader)) return DRFLAC_FALSE;
    reader.audio_start_offset = reader.offset;
    printf("flac start at %u audio start at %u\n", reader.flac_start_offset, reader.audio_start_offset);     
    
    /* we need the reader still */
    pInit->matroskaReader = reader;    
    
    return DRFLAC_TRUE;    
}

typedef struct {
    drflac_uint8 cache[1048576];
    ebml_element_reader reader;
    size_t bytes_in_cache;    
} drflac_matroskabs;

static size_t drflac__on_read_matroska(void* pUserData, void* bufferOut, size_t bytesToRead) {
    drflac_uint8 *bufferbytes = bufferOut;
    drflac_matroskabs *bs = (drflac_matroskabs *)pUserData;
    size_t tocopy;
    size_t bytesread = 0;
    size_t currentread;

    size_t size;
    drflac_uint64 id, content, content1, content2;
    drflac_uint32 width;

    while(bytesToRead > 0) {
        if(!drflac_matroska_get_simpleblock(&bs->reader)) return  bytesread;        

        /* copy data from the frame */
        tocopy = bs->reader.element_left[bs->reader.depth-1] < bytesToRead ? bs->reader.element_left[bs->reader.depth-1] : bytesToRead;
        currentread = drflac__on_read_ebml(&bs->reader, bufferbytes+bytesread, tocopy);
        bytesread += currentread;
        if(currentread != tocopy) return bytesread;
        bytesToRead -= tocopy;
    }    
    return bytesread;   
}

static drflac_bool32 drflac__on_seek_matroska(void* pUserData, int offset, drflac_seek_origin origin)
{
    drflac_matroskabs * matroskabs = (drflac_matroskabs*)pUserData;
    int bytesSeeked = 0;

    DRFLAC_ASSERT(matroskabs != NULL);
    DRFLAC_ASSERT(offset >= 0);  /* <-- Never seek backwards. */
    if(origin == drflac_seek_origin_start) {
        if(!matroskabs->reader.onSeek(matroskabs->reader.pUserData, 0, drflac_seek_origin_current)) return DRFLAC_FALSE;
        matroskabs->reader.depth = 1;
        /* set offset to segment */
        matroskabs->reader.offset = 0;         
        return drflac__on_seek_matroska(matroskabs, offset, drflac_seek_origin_current);
    }
    DRFLAC_ASSERT(origin == drflac_seek_origin_current);
    
   

    return DRFLAC_FALSE;
}


static drflac_bool32 drflac_matroska__seek_to_pcm_frame(drflac* pFlac, drflac_uint64 pcmFrameIndex) {
    return DRFLAC_FALSE;
}