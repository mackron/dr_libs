
//#define DISABLE_REFERENCE_FLAC

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
//#include <conio.h>

#ifndef DISABLE_REFERENCE_FLAC
#include <FLAC/stream_decoder.h>
#endif

//#define DR_FLAC_NO_WIN32_IO
#define DR_FLAC_IMPLEMENTATION
#include "../dr_flac.h"

#define DR_AUDIO_IMPLEMENTATION
#include "../dr_audio.h"

typedef struct drge_timer drge_timer;

#ifdef _WIN32
struct drge_timer
{
    /// The high performance counter frequency.
    LARGE_INTEGER frequency;

    /// The timer counter at the point in the time the timer was last ticked.
    LARGE_INTEGER counter;
};

drge_timer* drge_create_timer()
{
    drge_timer* pTimer = malloc(sizeof(*pTimer));
    if (pTimer == NULL) {
        return NULL;
    }

    if (!QueryPerformanceFrequency(&pTimer->frequency) || !QueryPerformanceCounter(&pTimer->counter)) {
        free(pTimer);
        return NULL;
    }

    return pTimer;
}

void drge_delete_timer(drge_timer* pTimer)
{
    free(pTimer);
}

double drge_tick_timer(drge_timer* pTimer)
{
    LARGE_INTEGER oldCounter = pTimer->counter;
    if (!QueryPerformanceCounter(&pTimer->counter)) {
        return 0;
    }

    return (pTimer->counter.QuadPart - oldCounter.QuadPart) / (double)pTimer->frequency.QuadPart;
}
#else
#include <sys/time.h>

struct drge_timer
{
    double t;
};

drge_timer* drge_create_timer()
{
    drge_timer* pTimer = malloc(sizeof(*pTimer));
    if (pTimer == NULL) {
        return NULL;
    }

    memset(&pTimer->t, 0, sizeof(pTimer->t));
    return pTimer;
}

void drge_delete_timer(drge_timer* pTimer)
{
    free(pTimer);
}

double drge_tick_timer(drge_timer* pTimer)
{
    if (pTimer == NULL) {
        return 0;
    }

    double tOld = pTimer->t;

    struct timeval t;
    gettimeofday(&t, NULL);

    pTimer->t = (t.tv_sec + (t.tv_usec * 0.000001));
    return pTimer->t - tOld;
}
#endif


#ifndef DISABLE_REFERENCE_FLAC
typedef struct
{
    FLAC__StreamDecoder* pDecoder;
    int32_t* pDecodedData;
    int32_t* pDecodedDataWalker;
    uint64_t totalSampleCount;
    unsigned int channels;
    unsigned int sampleRate;
} reference_data;

static FLAC__StreamDecoderWriteStatus flac__write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data)
{
    (void)decoder;

    reference_data* pData = client_data;

    // Make sure the decoded samples are interleaved.
    for (unsigned int i = 0; i < frame->header.blocksize; i += 1) {
        for (unsigned int j = 0; j < frame->header.channels; ++j) {
            *pData->pDecodedDataWalker++ = buffer[j][i] << (32 - frame->header.bits_per_sample);
        }
    }


    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void flac__metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data)
{
    (void)decoder;

    reference_data* pData = client_data;

    // Here is where we initialize the buffer for the decoded data.
    if(metadata->type == FLAC__METADATA_TYPE_STREAMINFO)
    {
        pData->totalSampleCount = metadata->data.stream_info.total_samples * metadata->data.stream_info.channels;
        pData->pDecodedData = malloc((size_t)pData->totalSampleCount * sizeof(int32_t));
        pData->pDecodedDataWalker = pData->pDecodedData;
        pData->channels = metadata->data.stream_info.channels;
        pData->sampleRate = metadata->data.stream_info.sample_rate;
    }
}

static void flac__error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
    (void)decoder;
    (void)status;
    (void)client_data;
}
#endif


typedef struct
{
    drflac* pFlac;
    int32_t* pDecodedData;
} drflac_data;

bool do_test(const char* filename)
{
    drge_timer* pTimer = drge_create_timer();

    double decodeTimeReference = 1;
#ifndef DISABLE_REFERENCE_FLAC
    // Reference first.
    reference_data referenceData;
    referenceData.pDecodedData = NULL;
    referenceData.pDecodedDataWalker = NULL;
    referenceData.totalSampleCount = 0;
    referenceData.pDecoder = FLAC__stream_decoder_new();
    FLAC__stream_decoder_init_file(referenceData.pDecoder, filename, flac__write_callback, flac__metadata_callback, flac__error_callback, &referenceData);

    drge_tick_timer(pTimer);
    FLAC__stream_decoder_process_until_end_of_stream(referenceData.pDecoder);
    decodeTimeReference = drge_tick_timer(pTimer);
#endif

    // dr_flac second.
    drflac_data drflacData;
    drflacData.pDecodedData = NULL;
    drflacData.pFlac = drflac_open_file(filename);

    drflacData.pDecodedData = malloc((size_t)drflacData.pFlac->totalSampleCount * sizeof(int32_t));

    drge_tick_timer(pTimer);
    drflac_read_s32(drflacData.pFlac, (size_t)drflacData.pFlac->totalSampleCount, drflacData.pDecodedData);
    double decodeTime = drge_tick_timer(pTimer);


    (void)decodeTimeReference;
    (void)decodeTime;

    // Sample-by-Sample comparison.
    bool result = true;
#ifndef DISABLE_REFERENCE_FLAC
    if (drflacData.pFlac->totalSampleCount != referenceData.totalSampleCount) {
        result = false;
        printf("TEST FAILED: %s: Total sample count differs. %lld != %lld\n", filename, drflacData.pFlac->totalSampleCount, referenceData.totalSampleCount);
        goto finish_test;
    }

    for (uint64_t i = 0; i < drflacData.pFlac->totalSampleCount; ++i) {
        if (drflacData.pDecodedData[i] != referenceData.pDecodedData[i]) {
            result = false;
            printf("TEST FAILED: %s: Sample at %lld differs. %d != %d\n", filename, i, drflacData.pDecodedData[i], referenceData.pDecodedData[i]);
            goto finish_test;
        }
    }

    printf("Reference Time: %f : dr_flac Time: %f - %d%%\n", decodeTimeReference, decodeTime, (int)(decodeTime/decodeTimeReference*100));

finish_test:
    free(referenceData.pDecodedData);
    free(drflacData.pDecodedData);
#endif
    return result;
}


int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

#if 1
    const char* testFiles[] = {
        "ocremix_tests/7th Saga - Seven Songs for Seventh Saga/FLAC/01 Seven Songs for Seventh Saga - I. Wind.flac",
        "ocremix_tests/7th Saga - Seven Songs for Seventh Saga/FLAC/02 Seven Songs for Seventh Saga - II. Water.flac",
        "ocremix_tests/7th Saga - Seven Songs for Seventh Saga/FLAC/03 Seven Songs for Seventh Saga - III. Star.flac",
        "ocremix_tests/7th Saga - Seven Songs for Seventh Saga/FLAC/04 Seven Songs for Seventh Saga - IV. Sky.flac",
        "ocremix_tests/7th Saga - Seven Songs for Seventh Saga/FLAC/05 Seven Songs for Seventh Saga - V. Moon.flac",
        "ocremix_tests/7th Saga - Seven Songs for Seventh Saga/FLAC/06 Seven Songs for Seventh Saga - VI. Light.flac",
        "ocremix_tests/7th Saga - Seven Songs for Seventh Saga/FLAC/07 Seven Songs for Seventh Saga - VII. Wizard.flac",

        "ocremix_tests/Apex 2015 - This Is the Moment/FLAC/01 John Ryan - This Is the Moment [Main Theme of Apex 2015].flac",
        "ocremix_tests/Apex 2015 - This Is the Moment/FLAC/02 DusK - A Day Like No Other [Ultimate Marvel vs. Capcom 3].flac",
        "ocremix_tests/Apex 2015 - This Is the Moment/FLAC/03 DarkeSword - Got My Mind on My Money Match [Super Smash Bros. for Wii U].flac",
        "ocremix_tests/Apex 2015 - This Is the Moment/FLAC/04 CJthemusicdude - Smashed Fridge Bits [Super Smash Bros. Melee].flac",
        "ocremix_tests/Apex 2015 - This Is the Moment/FLAC/05 Amphibious - Forest Fire [Pokemon X and Y].flac",
        "ocremix_tests/Apex 2015 - This Is the Moment/FLAC/06 DjjD - Bull in a China Shop [Super Smash Bros. Melee].flac",
        "ocremix_tests/Apex 2015 - This Is the Moment/FLAC/07 Sixto Sounds - Falcon DREAM!! [Super Smash Bros. Brawl].flac",
        "ocremix_tests/Apex 2015 - This Is the Moment/FLAC/08 WillRock - Filler Instinct [Killer Instinct].flac",
        "ocremix_tests/Apex 2015 - This Is the Moment/FLAC/09 Neblix - Girl from Another World [Ultra Street Fighter IV].flac",
        "ocremix_tests/Apex 2015 - This Is the Moment/FLAC/10 Ivan Hakstok - May the Stars Light Your Way [Guilty Gear Xrd].flac",

        "ocremix_tests/CEO 2015 - Champion/FLAC/01 O_Super x Mag.Lo - CEO Champion [Main Theme of CEO 2015].flac",
        "ocremix_tests/CEO 2015 - Champion/FLAC/02 Jeff Matthews - The Last Kill [Killer Instinct].flac",
        "ocremix_tests/CEO 2015 - Champion/FLAC/03 DarkeSword - Kuro Yuki [Persona 4 Arena].flac",
        "ocremix_tests/CEO 2015 - Champion/FLAC/04 DjjD - Prodigious Blitz [Tekken Tag Tournament 2].flac",
        "ocremix_tests/CEO 2015 - Champion/FLAC/05 Richie Branson - Iron Fist [Tekken Theme of CEO 2015].flac",
        "ocremix_tests/CEO 2015 - Champion/FLAC/06 Flexstyle - It's Okay, I Still Made Money [Divekick].flac",
        "ocremix_tests/CEO 2015 - Champion/FLAC/07 zykO - #unanimous #undisputed [Super Smash Bros. for Wii U].flac",
        "ocremix_tests/CEO 2015 - Champion/FLAC/08 Benjamin Briggs - FALCON PUNCH [Super Smash Bros. Theme of CEO 2015].flac",
        "ocremix_tests/CEO 2015 - Champion/FLAC/09 Neblix - Together, We Fly [Super Smash Bros. Melee].flac",
        "ocremix_tests/CEO 2015 - Champion/FLAC/10 DarkeSword - Fatalistic [Mortal Kombat].flac",
        "ocremix_tests/CEO 2015 - Champion/FLAC/11 Richie Branson - Finish Him [Mortal Kombat Theme of CEO 2015].flac",
        "ocremix_tests/CEO 2015 - Champion/FLAC/12 DarkeSword - U JELLY! [Guilty Gear X].flac",
        "ocremix_tests/CEO 2015 - Champion/FLAC/13 Nutritious - Dash Cancel [Ultra Street Fighter IV].flac",
        "ocremix_tests/CEO 2015 - Champion/FLAC/14 Ivan Hakstok, Sixto Sounds - What's Your Poison [Ultimate Marvel vs. Capcom 3].flac",

        "ocremix_tests/Final Fantasy II - Rebellion/FLAC/1-01 Preluematsude (Prelude) [Jeff Ball].flac",
        "ocremix_tests/Final Fantasy II - Rebellion/FLAC/1-02 The Last March (The Imperial Army) [Dr. Manhattan].flac",
        "ocremix_tests/Final Fantasy II - Rebellion/FLAC/1-03 Rebirth (Revival) [Brandon Strader].flac",
        "ocremix_tests/Final Fantasy II - Rebellion/FLAC/1-04 Rebel Dream [Main Theme (FF1), The Rebel Army, Find Your Way (FF8), Main Theme] (BONKERS).flac",
        "ocremix_tests/Final Fantasy II - Rebellion/FLAC/1-05 Leon Is a Fucking Dick (Battle Theme 1) [Kidd Cabbage].flac",
        "ocremix_tests/Final Fantasy II - Rebellion/FLAC/1-06 garLACTUS Win [Victory, Fanfare (FF7)] (Darkmoocher).flac",
        "ocremix_tests/Final Fantasy II - Rebellion/FLAC/1-07 Deuces (Ancient Castle) [mellogear].flac",
        "ocremix_tests/Final Fantasy II - Rebellion/FLAC/1-08 Firion N Maria (Will Take You to the Rebels) [The Rebel Army] (PrototypeRaptor).flac",
        "ocremix_tests/Final Fantasy II - Rebellion/FLAC/1-09 Analog Freedom (Town, The Rebel Army) [BONKERS].flac",
        "ocremix_tests/Final Fantasy II - Rebellion/FLAC/1-10 the final WON (Battle Theme A, Victory) [W!SE the all.E].flac",
        "ocremix_tests/Final Fantasy II - Rebellion/FLAC/1-11 Rebellion (Dead Music, The Rebel Army) [Brandon Strader, Chernabogue, Detective Tuesday].flac",
        "ocremix_tests/Final Fantasy II - Rebellion/FLAC/2-01 Heroes of Dawn [Chaos Temple (FF1), Reunion, The Rebel Army, Deep Under the Water (FF3), Dead Music (FF1)] (PacificPoem).flac",
        "ocremix_tests/Final Fantasy II - Rebellion/FLAC/2-02 Snakeyes (Battle Theme B) [zykO].flac",
        "ocremix_tests/Final Fantasy II - Rebellion/FLAC/2-03 Grind My Crank (Tower of the Magi) [XPRTNovice].flac",
        "ocremix_tests/Final Fantasy II - Rebellion/FLAC/2-04 Personification of Evil (The Emperor's Rebirth, Escape!) [Tuberz McGee].flac",
        "ocremix_tests/Final Fantasy II - Rebellion/FLAC/2-05 A Kingdom Fallen (Main Theme) [Sixto Sounds].flac",
        "ocremix_tests/Final Fantasy II - Rebellion/FLAC/2-06 Torchlit (Dungeon) [Viking Guitar].flac",
        "ocremix_tests/Final Fantasy II - Rebellion/FLAC/2-07 GG but ___ Solos Win [Victory, The Winner (FF8)] (Sir Jordanius feat. Brandon Strader).flac",
        "ocremix_tests/Final Fantasy II - Rebellion/FLAC/2-08 Castellum Infernum (Castle Pandemonium) [Brandon Strader].flac",
        "ocremix_tests/Final Fantasy II - Rebellion/FLAC/2-09 Imperial Rapture (Battle Theme 2) [IanFitC].flac",
        "ocremix_tests/Final Fantasy II - Rebellion/FLAC/2-10 Finally (Finale) [Hat].flac",

        "ocremix_tests/01 The Orichalcon - megAsfear (Title).flac",
        "ocremix_tests/02 Evil Horde - Running from Evil Horde (MAP01 - Running from Evil).flac",
        "ocremix_tests/03 analoq - Adrian's Sleep (MAP25 - Adrian's Asleep).flac",
        "ocremix_tests/04 Mazedude - Westside Archvile (MAP20 - Message for the Archvile).flac",
        "ocremix_tests/05 Jovette Rivera - The Countdown (MAP03 - Countdown to Death).flac",
        "ocremix_tests/06 The Orichalcon - Crushing Headache (MAP06 - In the Dark).flac",
        "ocremix_tests/07 Mazedude - Silent Healer (MAP02 - The Healer Stalks).flac",
        "ocremix_tests/08 Big Giant Circles, Flik - Icon of Sinwave (MAP30 - Opening to Hell).flac",
        "ocremix_tests/09 John Revoredo - 31 Seconds (MAP09 - Into Sandy's City).flac",
        "ocremix_tests/10 Mazedude, Ailsean - The End of Hell (Endgame).flac",
        "ocremix_tests/11 phoenixdk - No Smoking Area (MAP23 - Bye Bye American Pie).flac",
        "ocremix_tests/12 Evil Horde - The Duel (MAP08 - The Dave D. Taylor Blues).flac",
        "ocremix_tests/13 djpretzel - Red Waltz (Intermission).flac",
        "ocremix_tests/Bonus phoenixdk - Ablaze (MAP10 - The Demon's Dead).flac",

        "Hallelujah.flac",
        "1 Sullivan The Lost Chord, Seated one day at the organ.FLAC",
        "1 Vaet Videns Dominus.FLAC",
        "3 Schubert String Quartet No 14 in D minor Death and the Maiden, D810 - Movement 3 Scherzo Allegro molto.FLAC",
        "14 Clementi Piano Sonata in D major, Op 25 No 6 - Movement 2 Un poco andante.FLAC",
        "E+questa+vita+un+lampo+Studio+Master.flac",
        "recit24bit.flac",
        "recit16bit.flac",
        "recit8bit.flac",
        "song1.flac",
        "BIS1536-001-flac_24.flac",
        "BIS1447-002-flac_24.flac",
    };

    unsigned int testCount = sizeof(testFiles) / sizeof(testFiles[0]);
    for (unsigned int i = 0; i < testCount; ++i)
    {
        const char* filename = testFiles[i];

        if (do_test(filename)) {
            printf("TEST PASSED: %s\n", filename);
        }
    }
#endif


#if 0
    FILE* pFile;
    //if (fopen_s(&pFile, "BIS1536-001-flac_24.flac", "rb") != 0) {
    //if (fopen_s(&pFile, "BIS1447-002-flac_24.flac", "rb") != 0) {
    //if (fopen_s(&pFile, "song1.flac", "rb") != 0) {
    //if (fopen_s(&pFile, "recit24bit.flac", "rb") != 0) {
    if (fopen_s(&pFile, "recit8bit.flac", "rb") != 0) {
        return -1;
    }

    fseek(pFile, 0, SEEK_END);
    size_t fileDataSize = (size_t)ftell(pFile);
    fseek(pFile, 0, SEEK_SET);

    void* pFileData = malloc(fileDataSize);
    fread(pFileData, 1, fileDataSize, pFile);
    fclose(pFile);

    drflac* pFlac = drflac_open_memory(pFileData, fileDataSize);
    if (pFlac == NULL) {
        return -1;
    }

    uint64_t firstSample = pFlac->totalSampleCount / 2;
    drflac_seek_to_sample(pFlac, firstSample);

    int* pSampleData = malloc((size_t)pFlac->totalSampleCount * sizeof(int));
    drflac_read_s32(pFlac, (size_t)(pFlac->totalSampleCount - firstSample), pSampleData);


    //drflac_seek_to_sample(&flac, 0);
    //drflac_read_s32(&flac, (size_t)(flac.totalSampleCount - firstSample), pSampleData);


    draudio_context* pContext = draudio_create_context();
    if (pContext == NULL) {
        return -2;
    }

    draudio_device* pDevice = draudio_create_output_device(pContext, 0);
    if (pDevice == NULL) {
        return -3;
    }

    draudio_buffer_desc bufferDesc;
    memset(&bufferDesc, 0, sizeof(&bufferDesc));
    bufferDesc.format        = draudio_format_pcm;
    bufferDesc.channels      = pFlac->channels;
    bufferDesc.sampleRate    = pFlac->sampleRate;
    bufferDesc.bitsPerSample = sizeof(int)*8;
    bufferDesc.sizeInBytes   = (size_t)pFlac->totalSampleCount * sizeof(int);
    bufferDesc.pData         = pSampleData;

    draudio_buffer* pBuffer = draudio_create_buffer(pDevice, &bufferDesc, 0);
    if (pBuffer == NULL) {
        return -4;
    }


    draudio_play(pBuffer, false);
#endif


#if 0
    drflac flac;
    drflac_open_file(&flac, "MyFile.flac");

    int* pSampleData = malloc((size_t)flac.totalSampleCount * sizeof(int));
    drflac_read_s32(&flac, flac.totalSampleCount, pSampleData);
#endif

    //_getch();
    return 0;
}
