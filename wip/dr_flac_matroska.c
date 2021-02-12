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


static drflac_bool32 drflac_matroska_ebml_read_vint(drflac_bs *bs, drflac_uint64 *retval) {
    drflac_uint32 width;
    drflac_uint64 value = 0;
    drflac_uint8  thebyte;
	
	if(!drflac__read_uint8(bs, 8, &thebyte)) return DRFLAC_FALSE;
  
    /*printf("\nbyte 0x%X\n", thebyte);*/
    /* loop through the VINT_WIDTH bits */
    for(width = 1; width < 9; width++) {   
        /* check for VINT_MARKER */                            
        if(thebyte & (0x100 >> width)) {
            /*printf("embl width %u\n", width);*/               
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
					return DRFLAC_TRUE;
				}
				if(!drflac__read_uint8(bs, 8, &thebyte)) return DRFLAC_FALSE;
            } while(1);            
        }                                  
    }
    return DRFLAC_FALSE;
}

static drflac_bool32 drflac_matroska_ebml_id_and_size(drflac_bs *bs, drflac_uint64 *id, drflac_uint64 *size, drflac_bool32 *sizeunknown) {
    if(!drflac_matroska_ebml_read_vint(bs, id)) return DRFLAC_FALSE;
    if(!drflac_matroska_ebml_read_vint(bs, size)) return DRFLAC_FALSE;   
    *sizeunknown = DRFLAC_FALSE;
    return DRFLAC_TRUE;
}

static drflac_bool32 drflac_bs_read_bytearray(drflac_bs *bs, size_t bytelen, drflac_uint8* result) {
	int i;
	for(i = 0; i < bytelen; i++) {
		if( ! drflac__read_uint8(bs, 8, &result[i])) return DRFLAC_FALSE;		
	}
	return DRFLAC_TRUE;
}

static drflac_bool32 drflac_matroska_ebml_read_unsigned(drflac_bs *bs, size_t bytelen, drflac_uint64 *value) {
    drflac_uint8 element[8];
    int i;
	
	if(!drflac_bs_read_bytearray(bs, bytelen, element)) return DRFLAC_FALSE;
	
    *value = 0;
    for( i = 0; i < bytelen; i++) {
        *value |= (element[i] << ((bytelen-i-1)*8));
    }
    return DRFLAC_TRUE;
}

static drflac_bool32 drflac_matroska_ebml_read_string(drflac_bs *bs, size_t bytelen, char *string) {
	if(!drflac_bs_read_bytearray(bs, bytelen, string)) return DRFLAC_FALSE;
	string[bytelen] = '\0';
    return DRFLAC_TRUE;
}


static drflac_bool32 drflac__init_private__matroska(drflac_init_info* pInit, drflac_read_proc onRead, drflac_seek_proc onSeek, drflac_meta_proc onMeta, void* pUserData, void* pUserDataMD, drflac_bool32 relaxed)
{    
    drflac_uint64 id;
    drflac_uint64 size;
    drflac_bool32 size_unknown;
    drflac_uint64 content = 0;
    drflac_uint8  string[9];
    drflac_uint8 thebyte;    
    int i;
    (void)relaxed;
    /* master element size*/
	if(!drflac_matroska_ebml_read_vint(&pInit->bs, &content)) return DRFLAC_FALSE;
	printf("master element size %u\n", content);
    for(;;) {
        drflac_matroska_ebml_id_and_size(&pInit->bs, &id, &size, &size_unknown);
        printf("ebml size %u\n", size);
          
        switch(id) {
            case 646:
            drflac_matroska_ebml_read_unsigned(&pInit->bs, size, &content);
            printf("ebml version %u\n", content);
            break;
            case 759:
            drflac_matroska_ebml_read_unsigned(&pInit->bs, size, &content);
            printf("ebml read version %u\n", content);
            break;
            case 754:
            drflac_matroska_ebml_read_unsigned(&pInit->bs, size, &content);            
            printf("ebml max ebml id len %u\n", content);
            break;
            case 755:
            drflac_matroska_ebml_read_unsigned(&pInit->bs, size, &content);
            printf("ebml max ebml size len %u\n", content);
            break;
            case 642:
            if(size >= sizeof(string)) return DRFLAC_FALSE;
            drflac_matroska_ebml_read_string(&pInit->bs, size, string);
            printf("ebml document type %s\n", string);
            break;
            case 647:
            drflac_matroska_ebml_read_unsigned(&pInit->bs, size, &content);
            printf("ebml document version %u\n", content);
            break;
            case 645:
            drflac_matroska_ebml_read_unsigned(&pInit->bs, size, &content);
            printf("ebml document read version %u\n", content);
            break;            
            default:
            printf("ebml id %u\n", id);
            printf("ebml size %u\n", size);
            if(size > 8) {
                return DRFLAC_FALSE;
            }
			if(!drflac_bs_read_bytearray(&pInit->bs, size, &content)) return DRFLAC_FALSE;
            printf("ebml as hex 0x"); 
            for(i = 0; i < size; i++) {
                thebyte = content >> (i*8);
                printf("%X", thebyte);                
            }
            printf("\n ebml as string: ");
            for(i = 0; i < size; i++) {
                printf("%c");                
            }
            printf("\n");
            break;
        }
    }   

    return DRFLAC_FALSE;
}
