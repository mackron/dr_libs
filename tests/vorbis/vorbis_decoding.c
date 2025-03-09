#include "vorbis_common.c"
#include <stdio.h>

int on_meta(void* pUserData, const dr_vorbis_metadata* pMetadata)
{
    (void)pUserData;

    if (pMetadata->type == dr_vorbis_metadata_type_vendor) {
        printf("VENDOR=%s\n", pMetadata->data.vendor.pData);
    }

    if (pMetadata->type == dr_vorbis_metadata_type_comment) {
        printf("%s\n", pMetadata->data.comment.pData);
    }

    return 0;
}

int main(int argc, char** argv)
{
    int result;
    dr_vorbis vorbis;

    if (argc < 2) {
        printf("No input file.\n");
        return -1;
    }

    result = dr_vorbis_init_file_ex(argv[1], NULL, on_meta, NULL, &vorbis);
    if (result != 0) {
        return result;
    }



    dr_vorbis_uninit(&vorbis);

    return 0;
}
