#ifdef DEBUG
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
#endif

static void drflac_matroska_ebml_push_element_unchecked(drflac_matroska_ebml_tree *reader, drflac_uint32 id, drflac_uint64 size, drflac_bool32 isinf) {
    reader->element_inf[reader->depth]  = isinf;
    reader->element_left[reader->depth] = size;
    reader->depth++;
    reader->id[reader->depth-1] = id;
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

static drflac_bool32 drflac_matroska_ebml_push_element(drflac_matroska_ebml_tree *reader, drflac_uint32 id, drflac_uint64 size, drflac_uint32 size_width) {
    drflac_bool32 isinf;    

    if(reader->depth == DR_FLAC_MAX_EBML_NEST) return DRFLAC_FALSE;
    if(reader->depth < 1) return DRFLAC_FALSE;
    isinf = drflac_matroska_ebml_size_infinite(size, size_width);

    /* if the parent element isn't infinite there are restrictions on our range */
    if(!reader->element_inf[reader->depth - 1]) {
        if(isinf) return DRFLAC_FALSE;
        if(reader->element_left[reader->depth - 1] < size) return DRFLAC_FALSE;
    }
    
    drflac_matroska_ebml_push_element_unchecked(reader, id, size, isinf);
    return DRFLAC_TRUE;
}


static drflac_bool32 drflac_matroska_ebml_ok_to_read(drflac_matroska_ebml_tree *reader, drflac_uint64 bytelen) {
    if(reader->element_inf[reader->depth - 1]) return DRFLAC_TRUE;
    if(reader->element_left[reader->depth - 1] >= bytelen) return DRFLAC_TRUE;
    return DRFLAC_FALSE;
}

static void drflac_matroska_ebml_sub_bytes_read(drflac_matroska_ebml_tree *reader, drflac_uint64 bytesread) {
   
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

typedef struct {
    drflac_matroska_ebml_tree reader;
    drflac_read_proc onRead;
    drflac_seek_proc onSeek;
    void* pUserData;    
} drflac_matroska_ebml_io;

static size_t drflac__on_read_ebml(void* pUserData, void* bufferOut, size_t bytesToRead) {
    drflac_matroska_ebml_io *io = (drflac_matroska_ebml_io*)pUserData;
    drflac_uint32 bytes_read = 0;
    
    if(!drflac_matroska_ebml_ok_to_read(&io->reader, bytesToRead)) return 0;

    bytes_read = io->onRead(io->pUserData, bufferOut, bytesToRead);
    drflac_matroska_ebml_sub_bytes_read(&io->reader, bytes_read);
    io->reader.offset += bytes_read;
    return bytes_read;
}

static drflac_bool32 drflac__on_seek_ebml(void* pUserData, int offset, drflac_seek_origin origin) {
    drflac_matroska_ebml_io *io = (drflac_matroska_ebml_io*)pUserData;
    if(origin == drflac_seek_origin_current) {
        if(!drflac_matroska_ebml_ok_to_read(&io->reader, offset)) return DRFLAC_FALSE;
        if(!io->onSeek(io->pUserData, offset, origin)) return DRFLAC_FALSE;
        drflac_matroska_ebml_sub_bytes_read(&io->reader, offset);
        io->reader.offset += offset;
        return DRFLAC_TRUE;
    }
    return DRFLAC_FALSE;
}

static drflac_bool32 drflac_matroska_ebml_close_current_element(drflac_matroska_ebml_io *io) {
    return drflac__on_seek_ebml(io, io->reader.element_left[io->reader.depth-1], drflac_seek_origin_current);
}

static drflac_bool32 drflac_matroska_ebml_read_vint(drflac_matroska_ebml_io *io, drflac_uint64 *retval, drflac_uint32 *savewidth) {
    drflac_uint32 width = 1;
    drflac_uint64 value = 0;
    drflac_uint8  thebyte;
    drflac_bool32 done;	

    /* read the VINT width byte*/
    if(drflac__on_read_ebml(io, &thebyte, 1) != 1) return DRFLAC_FALSE; 

    /* read the width, clearing VINT_WIDTH and VINT_MARKER along the way */
    for(;;width++) {        
        done = (thebyte & 0x80);
        thebyte <<= 1;         
        if(done) break;        
        if(width == 8) return DRFLAC_FALSE;
    }
    thebyte >>= width;
    *savewidth = width;
                           
    /* convert this byte and remaining bytes to an unsigned integer */
    /* https://commandcenter.blogspot.com/2012/04/byte-order-fallacy.html */
    for(;;) {             
        value |= thebyte;
        width--;
        if(width == 0) break;
        if(drflac__on_read_ebml(io, &thebyte, 1) != 1) return DRFLAC_FALSE;
        value <<= 8;
    }
    *retval = value;
    return DRFLAC_TRUE;
}

static drflac_bool32 drflac_matroska_ebml_load_element(drflac_matroska_ebml_io *io, drflac_uint64 *id) {
    drflac_uint64 size;
    drflac_uint32 width;

    if(!drflac_matroska_ebml_read_vint(io, id, &width)) return DRFLAC_FALSE;
    if(!drflac_matroska_ebml_read_vint(io, &size, &width)) return DRFLAC_FALSE;   
    return drflac_matroska_ebml_push_element(&io->reader, *id, size, width);    
}

static drflac_bool32 drflac_matroska_ebml_read_unsigned(drflac_matroska_ebml_io *io, size_t bytelen, drflac_uint64 *value) {
    drflac_uint8 element[8];
    int i;
    if(bytelen > sizeof(element)) return DRFLAC_FALSE;

    if(drflac__on_read_ebml(io, element, bytelen) != bytelen) return DRFLAC_FALSE;	
	
    *value = 0;
    for( i = 0; i < bytelen; i++) {
        *value |= (element[i] << ((bytelen-i-1)*8));
    }
    return DRFLAC_TRUE;
}

static drflac_bool32 drflac_matroska_ebml_read_uint8(drflac_matroska_ebml_io *io, drflac_uint8 *value) {
    return drflac__on_read_ebml(io, value, 1) == 1;
}

static drflac_bool32 drflac_matroska_ebml_read_string(drflac_matroska_ebml_io *io, size_t bytelen, drflac_uint8 *string) {
	if(drflac__on_read_ebml(io, string, bytelen) != bytelen) return DRFLAC_FALSE;
	string[bytelen] = '\0';
    return DRFLAC_TRUE;
}


static drflac_bool32 drflac_matroska_ebml_read_int16(drflac_matroska_ebml_io *io, drflac_int16 *value) {
	drflac_uint8 element[2];
    if(drflac__on_read_ebml(io, element, 2) != 2) return DRFLAC_FALSE;
	*value = (element[0] << 8) | element[1];    
    return DRFLAC_TRUE;
}

/* jump to a saved point in the ebml_stack*/
static drflac_bool32 drflac_matroska_seek_into_ebml(drflac_matroska_ebml_io *io, drflac_matroska_ebml_tree *src) {
    if(!io->onSeek(io->pUserData, src->offset, drflac_seek_origin_start)) return DRFLAC_FALSE;
    io->reader = *src;
    return DRFLAC_TRUE;
}

static drflac_bool32 drflac__init_private__matroska(drflac_init_info* pInit, drflac_read_proc onRead, drflac_seek_proc onSeek, drflac_meta_proc onMeta, void* pUserData, void* pUserDataMD, drflac_bool32 relaxed)
{   
    drflac_matroska_ebml_io io;

    drflac_matroska_ebml_tree privdata;
    drflac_uint64 id;
    drflac_uint64 content = 0;
    drflac_uint32 width = 0;
    drflac_uint32 size = 0;
    drflac_uint8  string[9];
    
    drflac_streaminfo streaminfo;
    drflac_uint8 isLastBlock;
    drflac_uint8 blockType;
    drflac_uint32 blockSize;

    /* setup ebml io*/
    io.reader.offset = 0x4;
    io.reader.depth = 0;
    io.onRead = onRead;
    io.onSeek = onSeek;
    io.pUserData = pUserData;    

    (void)relaxed;
    
    pInit->container = drflac_container_matroska;
    pInit->matroskaSegment.offset = 0;
    pInit->matroskaTsscale = 1;
    pInit->matroskaCodecPrivate.depth = 0;

    /* add an infinite element as the head to simplify tree operations */
    drflac_matroska_ebml_push_element_unchecked(&io.reader, 0, 0, DRFLAC_TRUE);

    
    /*skip past the master element*/
	if(!drflac_matroska_ebml_read_vint(&io, &content, &width)) return DRFLAC_FALSE;  
    if(!drflac_matroska_ebml_push_element(&io.reader, 0, content, width)) return DRFLAC_FALSE;
    if(!drflac_matroska_ebml_close_current_element(&io)) return DRFLAC_FALSE;  
    if(io.reader.depth != 1) return DRFLAC_FALSE; 
    
    for(;;) {
        if(!drflac_matroska_ebml_load_element(&io, &id)) return DRFLAC_FALSE;
        size = io.reader.element_left[io.reader.depth-1];
        /* Segment */
        if((io.reader.depth == 2) && (id == 139690087)) {
            pInit->matroskaSegment = io.reader;
            continue;            
        }
        /* Segment Information*/
        else if((io.reader.depth == 3) && (id == 88713574)) {
            continue;
        }
        /* timestamp scale */
        else if((io.reader.depth == 4) && (id == 710577)) {
            if(drflac_matroska_ebml_read_unsigned(&io, size, &content)) {
                pInit->matroskaTsscale = content;              
            }                       
        }
        /* Tracks */
        else if((io.reader.depth == 3) && (id == 106212971)) {
            continue;
        }
        /* Track */
        else if((io.reader.depth == 4) && (id == 46)) {
            continue;            
        }
        /* Codec priv data */
        else if((io.reader.depth == 5) && (id == 9122)) {
            privdata = io.reader;
            if(!drflac_matroska_ebml_read_string(&io, 4, string)) return DRFLAC_FALSE;
            if((string[0] == 'f') && (string[1] == 'L') && (string[2] == 'a') && (string[3] == 'C')) {
                pInit->matroskaCodecPrivate = privdata;            
                break;
            }            
        }        
        
        /* skip over element otherwise */
        if(!drflac_matroska_ebml_close_current_element(&io)) return DRFLAC_FALSE;   
    }

    /* The remaining data in the page should be the STREAMINFO block. */    
    if (!drflac__read_and_decode_block_header(drflac__on_read_ebml, &io, &isLastBlock, &blockType, &blockSize)) {
        return DRFLAC_FALSE;
    }

    if (blockType != DRFLAC_METADATA_BLOCK_TYPE_STREAMINFO || blockSize != 34) {
        return DRFLAC_FALSE;    /* Invalid block type. First block must be the STREAMINFO block. */
    }
    
    if (!drflac__read_streaminfo(drflac__on_read_ebml, &io, &streaminfo)) {
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

    pInit->matroskaReader = io.reader;
    return DRFLAC_TRUE;

    /* Tags  39109479 */
    /* Cluster 256095861 */
      /* CRC 63*/
      /* cluster timestamp 103  */
      /* Simple block 35*/
}

#define DRFLAC_MATROSKA_BS_CACHE_SIZE 1048576
typedef struct {
    drflac_matroska_ebml_io io;    
  
    drflac_matroska_ebml_tree segment;
    drflac_matroska_ebml_tree codecPrivate;
    drflac_uint64 tsscale;

    drflac_uint8 cache[DRFLAC_MATROSKA_BS_CACHE_SIZE];
    size_t bytes_in_cache;
    /* real io*/
    drflac_read_proc _onRead;
    drflac_seek_proc _onSeek;        
} drflac_matroskabs;


/* used for seeking when we're not allowed to seek such as when reading */ 
static drflac_bool32 drflac_matroska_close_current_element(drflac_matroskabs *bs) {
    return drflac_matroska_ebml_close_current_element(&bs->io);
}

static drflac_bool32 drflac_matroska_read_block(drflac_matroska_ebml_io *io) {
    drflac_uint64 trackno;
    drflac_uint32 width;
    drflac_int16 timestamp;
    drflac_uint8 flag;
    drflac_uint8 frames;
    drflac_uint64 framesize;
    drflac_uint64 frameoffset;
    int i;

    if(!drflac_matroska_ebml_read_vint(io, &trackno, &width)) return DRFLAC_FALSE;

    /* timestamp s16*/
    if(!drflac_matroska_ebml_read_int16(io, &timestamp)) return DRFLAC_FALSE;
    /* flag */
    if(!drflac_matroska_ebml_read_uint8(io, &flag)) return DRFLAC_FALSE;                
    /* no lacing we should be right at a frame*/
    if((flag & 0x6) == 0){
        return DRFLAC_TRUE;
    }
    
    /* number of frames - 1*/
    if(!drflac_matroska_ebml_read_uint8(io, &frames)) return DRFLAC_FALSE;

    /* fixed size lacing */
    if((flag & 0x6) == 0x4) return DRFLAC_TRUE;
     
    /* ebml lacing */
    if((flag & 0x6) == 0x6) {
        if(!drflac_matroska_ebml_read_vint(io, &framesize, &width)) return DRFLAC_FALSE;
        for(i = 0; i < (frames-1); i++) {
            if(!drflac_matroska_ebml_read_vint(io, &frameoffset, &width)) return DRFLAC_FALSE;
            /* subtract by 2^(bit width-1) - 1 to get the actual integer */
        }
        return DRFLAC_TRUE;                   
    }         

    /* xiph lacing - not implemented*/ 
    return DRFLAC_FALSE;
}

static drflac_bool32 drflac_matroska_find_flac_data(drflac_matroskabs *bs) {
    drflac_uint64 id;
    drflac_uint64 afterpriv = (bs->codecPrivate.offset + bs->codecPrivate.element_left[bs->codecPrivate.depth-1]);
    drflac_matroska_ebml_io *io = &bs->io;

    /* if in a simpleblock, block, or codec private data, we are done*/
    while((EBML_ID(&io->reader) != 35) && (EBML_ID(&io->reader) != 33) && (EBML_ID(&io->reader) != 9122)) {
        
        /* if we left segment we are done*/
        if(io->reader.depth < 2) {
            return DRFLAC_FALSE;
        }
        
        if(io->reader.offset < afterpriv) {
            /* if it's not Segment, Tracks, or Track we don't care, skip past it */
            if((EBML_ID(&io->reader) != 139690087) && (EBML_ID(&io->reader) != 106212971) && (EBML_ID(&io->reader) != 46)) {
                if(!drflac_matroska_ebml_close_current_element(io)) return DRFLAC_FALSE;   
                continue; 
            }            
        }
        else if(io->reader.offset == afterpriv) {
            /* seek out to segment level*/
            while(EBML_ID(&io->reader) != 139690087) {
                if(!drflac_matroska_ebml_close_current_element(io)) return DRFLAC_FALSE;  
            }
        }
        else {
            /* if it's not Segment, Cluster, BlockGroup we don't care, skip past it */
            if((EBML_ID(&io->reader) != 139690087) && (EBML_ID(&io->reader) != 256095861) && (EBML_ID(&io->reader) != 32)){
                if(!drflac_matroska_ebml_close_current_element(io)) return DRFLAC_FALSE; 
                continue; 
            }            
        }

        /* while we can't read this element seek past it*/
        while(!drflac_matroska_ebml_load_element(io, &id)) {
            if(!drflac_matroska_ebml_close_current_element(io)) return DRFLAC_FALSE;
        }
        /* if we found a block we need to read its header*/
        if((id == 35) || (id == 33)) {
            return drflac_matroska_read_block(io);
        }        
    }
    return DRFLAC_TRUE; 
}

static size_t drflac__on_read_matroska(void* pUserData, void* bufferOut, size_t bytesToRead) {
    drflac_uint8 *bufferbytes = bufferOut;
    drflac_matroskabs *bs = (drflac_matroskabs *)pUserData;
    drflac_matroska_ebml_io *io = &bs->io;
    size_t tocopy;
    size_t bytesread = 0;
    size_t currentread;    

    while(bytesToRead > 0) {
        if(!drflac_matroska_find_flac_data(bs)) {
            return  bytesread;
        }        
          
        /* copy data from the frame */
        tocopy = io->reader.element_left[io->reader.depth-1] < bytesToRead ? io->reader.element_left[io->reader.depth-1] : bytesToRead;
        currentread = drflac__on_read_ebml(io, bufferbytes+bytesread, tocopy);
        bytesread += currentread;
        
        if(currentread != tocopy) {
            return bytesread;
        } 
        bytesToRead -= tocopy;
    }    
    return bytesread;   
}

static drflac_bool32 drflac__on_seek_matroska(void* pUserData, int offset, drflac_seek_origin origin)
{
    drflac_matroskabs *bs = (drflac_matroskabs*)pUserData;      
    int toseek;
    drflac_matroska_ebml_io *io;
    DRFLAC_ASSERT(bs != NULL);
    DRFLAC_ASSERT(offset >= 0);  /* <-- Never seek backwards. */  
    io = &bs->io;   
    
    if(origin == drflac_seek_origin_start) {

        /* seek to start*/
        if(!drflac_matroska_seek_into_ebml(io, &bs->codecPrivate)) return DRFLAC_FALSE;
              
        return drflac__on_seek_matroska(bs, offset, drflac_seek_origin_current);
    }
    DRFLAC_ASSERT(origin == drflac_seek_origin_current);    

    while(offset > 0) {
        if(!drflac_matroska_find_flac_data(bs)) return DRFLAC_FALSE;
        toseek = offset > io->reader.element_left[io->reader.depth-1] ? io->reader.element_left[io->reader.depth-1] : offset;
        if(!drflac__on_seek_ebml(io, toseek, drflac_seek_origin_current)) return DRFLAC_FALSE;
        offset -= toseek;
    }   

    return DRFLAC_TRUE;
}

static drflac_uint64 drflac_matroska_round(double d) {
    return (drflac_uint64)(d + 0.5);
}

static drflac_bool32 drflac_matroska__seek_to_pcm_frame(drflac* pFlac, drflac_uint64 pcmFrameIndex) {
    
    drflac_matroskabs *bs = (drflac_matroskabs *)pFlac->bs.pUserData;
    drflac_matroska_ebml_io *io = &bs->io;
    drflac_matroska_ebml_tree *reader = &io->reader;
    
    drflac_uint64 id;
    drflac_uint64 ts;
    #ifdef DEBUG    
    drflac_uint64 beforeframe;
    #endif
    drflac_uint64 curframe;

    drflac_uint64 runningPCMFrameCount;
    
    drflac_matroska_ebml_tree desired_cluster;
    desired_cluster.offset = 0;

    /* first seek into Segment */
    if(!drflac_matroska_seek_into_ebml(io, &bs->segment)) return DRFLAC_FALSE;    

    /* seek into the cluster that might have our frame */
    DRFLAC_ASSERT(io->reader.depth == 2);
    for(;;) {        
        if(!drflac_matroska_ebml_load_element(io, &id)) break;

        /* Entered Cluster */
        if((reader->depth == 3) && (id == 256095861)) {       
            continue;
        }           
        /* Found a Cluster timestamp */
        if((reader->depth == 4) && (id == 103)) {          
            if(!drflac_matroska_ebml_read_unsigned(io, io->reader.element_left[io->reader.depth-1], &ts)) return DRFLAC_FALSE;
            curframe = drflac_matroska_round((ts * bs->tsscale)  * (double)pFlac->sampleRate / 1000000000);

            /* Hack, adjust curframe when the tsscale isn't good enough*/
            if(bs->tsscale > (1000000000 / pFlac->sampleRate)) {
                #ifdef DEBUG
                beforeframe = curframe;
                #endif
                curframe = drflac_matroska_round((double)curframe/4096)*4096;          
            }
            
            /* this cluster could contain our frame, save it's info*/
            if(curframe <= pcmFrameIndex) {
                desired_cluster = *reader;
                runningPCMFrameCount = curframe;                
            }
            /* this cluster is at or past the frame, breakout */                
            if(curframe >= pcmFrameIndex){
                break;
            }            
        }

        if(!drflac_matroska_close_current_element(bs)) return DRFLAC_FALSE; 
    }
    if(desired_cluster.offset == 0) return DRFLAC_FALSE;
    if(!drflac_matroska_seek_into_ebml(io, &desired_cluster)) return DRFLAC_FALSE;   
    
    for (;;) {
        /*
        Use the native FLAC decoder for the rest of seeking for simplicity.
        This is the same code from the bottom of drflac_ogg__seek_to_pcm_frame.
        */

        drflac_uint64 firstPCMFrameInFLACFrame = 0;
        drflac_uint64 lastPCMFrameInFLACFrame = 0;
        drflac_uint64 pcmFrameCountInThisFrame;

        if (!drflac__read_next_flac_frame_header(&pFlac->bs, pFlac->bitsPerSample, &pFlac->currentFLACFrame.header)) {
            return DRFLAC_FALSE;
        }

        drflac__get_pcm_frame_range_of_current_flac_frame(pFlac, &firstPCMFrameInFLACFrame, &lastPCMFrameInFLACFrame);

        pcmFrameCountInThisFrame = (lastPCMFrameInFLACFrame - firstPCMFrameInFLACFrame) + 1;

        /* If we are seeking to the end of the file and we've just hit it, we're done. */
        if (pcmFrameIndex == pFlac->totalPCMFrameCount && (runningPCMFrameCount + pcmFrameCountInThisFrame) == pFlac->totalPCMFrameCount) {
            drflac_result result = drflac__decode_flac_frame(pFlac);
            if (result == DRFLAC_SUCCESS) {
                pFlac->currentPCMFrame = pcmFrameIndex;
                pFlac->currentFLACFrame.pcmFramesRemaining = 0;
                return DRFLAC_TRUE;
            } else {
                return DRFLAC_FALSE;
            }
        }

        if (pcmFrameIndex < (runningPCMFrameCount + pcmFrameCountInThisFrame)) {
            /*
            The sample should be in this FLAC frame. We need to fully decode it, however if it's an invalid frame (a CRC mismatch), we need to pretend
            it never existed and keep iterating.
            */
            drflac_result result = drflac__decode_flac_frame(pFlac);
            if (result == DRFLAC_SUCCESS) {
                /* The frame is valid. We just need to skip over some samples to ensure it's sample-exact. */
                drflac_uint64 pcmFramesToDecode = (size_t)(pcmFrameIndex - runningPCMFrameCount);    /* <-- Safe cast because the maximum number of samples in a frame is 65535. */
                if (pcmFramesToDecode == 0) {
                    return DRFLAC_TRUE;
                }

                pFlac->currentPCMFrame = runningPCMFrameCount;

                return drflac__seek_forward_by_pcm_frames(pFlac, pcmFramesToDecode) == pcmFramesToDecode;  /* <-- If this fails, something bad has happened (it should never fail). */
            } else {
                if (result == DRFLAC_CRC_MISMATCH) {
                    continue;   /* CRC mismatch. Pretend this frame never existed. */
                } else {
                    return DRFLAC_FALSE;
                }
            }
        } else {
            /*
            It's not in this frame. We need to seek past the frame, but check if there was a CRC mismatch. If so, we pretend this
            frame never existed and leave the running sample count untouched.
            */
            drflac_result result = drflac__seek_to_next_flac_frame(pFlac);
            if (result == DRFLAC_SUCCESS) {
                runningPCMFrameCount += pcmFrameCountInThisFrame;
            } else {
                if (result == DRFLAC_CRC_MISMATCH) {
                    continue;   /* CRC mismatch. Pretend this frame never existed. */
                } else {
                    return DRFLAC_FALSE;
                }
            }
        }
    }    
}