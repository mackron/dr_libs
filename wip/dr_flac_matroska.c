#include <stdio.h>
/*
static drflac_uint32 drflac_matroska_ebml_read_vint_width(drflac_uint64 vint) {
    drflac_uint32 width;
    unsigned i;

    for(i = 0; i < (sizeof(vint)*8); i++) {
        ++width; 
        if(vint & 0x1) return width;
        vint >>= 1;
    }    
    return 0;
}
*/

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

static drflac_uint64 drflac_matroska_ebml_read_vint(drflac_read_proc onRead, void* pUserData) {
    drflac_uint32 width = 0;
    drflac_uint64 value;
    drflac_uint64 temp = 0;
    drflac_uint8  thebyte;
    int i;    

    while(onRead(pUserData, &thebyte, 1) == 1) {
    /*unsigned char data[] = {0x1a, 0x45, 0xdf, 0xa3};
    int j;
    for(j = 0; j < 4; j++) {
        thebyte = data[j];*/
        printf("byte 0x%X\n", thebyte);
        for(i = 0; i < 8; i++) {
            width++;            
            if(thebyte & 0x80) {
                thebyte <<= 1;
                value = thebyte >> width;                
                printf("embl width %u\n", width);                                
                if(width > 1) {
                    /* convert to big endian */                    
                    value <<= ((sizeof(drflac_uint64) - width)*8);
                    width--;                    
                    onRead(pUserData, &temp, width);
                    value |= (temp << ((sizeof(drflac_uint64) - width)*8));                    
                    value = drflac__swap_endian_uint64(value);
                }
                printf("embl value %u\n", value);
                drflac_matroska_print_hex(value);
                return value; 
            }
            thebyte <<= 1;
        }
    }
    return 0;    
}

static drflac_bool32 drflac_matroska_ebml_id_and_size(drflac_read_proc onRead, void* pUserData, drflac_uint64 *id, drflac_uint64 *size, drflac_bool32 *sizeunknown) {
    *id = drflac_matroska_ebml_read_vint(onRead, pUserData);
    printf("ebml id %u\n", *id);
    *size = drflac_matroska_ebml_read_vint(onRead, pUserData);
    printf("ebml size %u\n", *size);
    *sizeunknown = DRFLAC_FALSE;
    return DRFLAC_TRUE;
}

static drflac_bool32 drflac_matroska_read_unsigned(drflac_read_proc onRead, void* pUserData, size_t bytelen, drflac_uint64 *value) {
    *value = 0;
    return onRead(pUserData, value, bytelen) == bytelen;    
}

static drflac_bool32 drflac__init_private__matroska(drflac_init_info* pInit, drflac_read_proc onRead, drflac_seek_proc onSeek, drflac_meta_proc onMeta, void* pUserData, void* pUserDataMD, drflac_bool32 relaxed)
{    
    drflac_uint64 id;
    drflac_uint64 size;
    drflac_bool32 size_unknown;
    drflac_uint64 content = 0;
    drflac_uint8 thebyte;
    int i;
    (void)relaxed;
    /* master element size*/
    drflac_matroska_ebml_read_vint(onRead, pUserData);
    for(;;) {
        drflac_matroska_ebml_id_and_size(onRead, pUserData, &id, &size, &size_unknown);

        
        if(size <= 8) {
            drflac_matroska_read_unsigned(onRead, pUserData, size, &content);
        }
        else {
            return DRFLAC_FALSE;
        }            
        switch(id) {
            /* ebml version */ 
            case 646:
            printf("ebml version %u\n", content);
            break;
            case 759:
            printf("ebml read version %u\n", content);
            break;
            case 754:            
            printf("ebml max ebml id len %u\n", content);
            break;
            case 755:
            printf("ebml max ebml size len %u\n", content);
            break;
            case 642:
            printf("ebml document type %s\n", &content);
            break;
            case 647:
            printf("ebml document version %u\n", content);
            break;
            case 645:
            printf("ebml document read version %u\n", content);
            break;            
            default:
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
