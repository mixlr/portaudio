/*
 * $Id$
 * PortAudio Portable Real-Time Audio Library
 * Latest Version at: http://www.portaudio.com
 * ALSA implementation by Joshua Haberman and Arve Knudsen
 *
 * Copyright (c) 2002 Joshua Haberman <joshua@haberman.com>
 * Copyright (c) 2005-2009 Arve Knudsen <arve.knudsen@gmail.com>
 * Copyright (c) 2008 Kevin Kofler <kevin.kofler@chello.at>
 * Copyright (c) 2014-2015 Alan Horstmann <gineera@aspect135.co.uk>
 *
 * Based on the Open Source API proposed by Ross Bencina
 * Copyright (c) 1999-2002 Ross Bencina, Phil Burk
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * The text above constitutes the entire PortAudio license; however,
 * the PortAudio community also makes the following non-binding requests:
 *
 * Any person wishing to distribute modifications to the Software is
 * requested to send the modifications to the original developer so that
 * they can be incorporated into the canonical version. It is also
 * requested that these non-binding requests be included along with the
 * license above.
 */


#define PA_ENABLE_DEBUG_OUTPUT

#include <limits.h>

#include "pa_util.h"
#include "pa_unix_util.h"
#include "pa_debugprint.h"

#include "pa_alsa_internal.h"
#include "pa_alsa_load_dyn.h"


/* Means the number of channels is >0, but the number is not yet known */
#define CHANS_UNKNOWN 1234

/* Max numbers the code allows - smallest Linux int may be 32-bit - or use SIZEOF_INT? */
//#define MAX_CARDS       32
//#define MAX_CARD_DEVS   32


/* Helper struct for the device listing process */
typedef struct
{
    char *alsaName;
    char *name;
    int isPlug;
    int maxChansPlayback;
    int maxChansCapture;
} DevInfo;


static const CardHwConfig *FindCardPredef( int cIdx, const char *cName )
{
    /* For now, don't match by name; ignore it */
    (void) cName;
    int i;

    for( i = 0; predefinedHw[i].cardIdx > -127; i++ ) /* '-127' is the end marker */
    {
        if( predefinedHw[i].cardIdx == cIdx )
            return &predefinedHw[i];
    }

    return NULL;
}


static const PcmDevConfig *FindPcmPredef( const char *name )
{
    int i;

    for( i = 0; predefinedPcms[i].pcmName; i++ )
    {
        if( strcmp( name, predefinedPcms[i].pcmName ) == 0 )
            return &predefinedPcms[i];
    }

    return NULL;
}


/* Skip past parts at the beginning of a (pcm) info name that are already in the card name, to avoid duplication */
static char *SkipCardDetailsInName( char *infoSkipName, char *cardRefName )
{
    char *lastSpacePosn = infoSkipName;

    /* Skip matching chars; but only in chunks separated by ' ' (not part words etc), so track lastSpacePosn */
    while( *cardRefName )
    {
        while( *infoSkipName && *cardRefName && *infoSkipName == *cardRefName)
        {
            infoSkipName++;
            cardRefName++;
            if( *infoSkipName == ' ' || *infoSkipName == '\0' )
                lastSpacePosn = infoSkipName;
        }
        infoSkipName = lastSpacePosn;
        /* Look for another chunk; post-increment means ends pointing to next char */
        while( *cardRefName && ( *cardRefName++ != ' ' ));
    }
    if( *infoSkipName == '\0' )
        return "-"; /* The 2 names were identical; instead of a nul-string, return a marker string */

    /* Now want to move to the first char after any spaces */
    while( *lastSpacePosn && *lastSpacePosn == ' ' )
        lastSpacePosn++;
    /* Skip a single separator char if present in the remaining pcm name; (pa will add its own) */
    if(( *lastSpacePosn == '-' || *lastSpacePosn == ':' ) && *(lastSpacePosn + 1) == ' ' )
        lastSpacePosn += 2;

    return lastSpacePosn;
}


static PaError StrDuplA( PaAlsaHostApiRepresentation *alsaApi,
        char **dst, const char *src)
{
    PaError result = paNoError;
    int len = strlen( src ) + 1;

    /* PA_DEBUG(("PaStrDup %s %d\n", src, len)); */

    PA_UNLESS( *dst = (char *)PaUtil_GroupAllocateMemory( alsaApi->allocations, len ),
            paInsufficientMemory );
    strncpy( *dst, src, len );

error:
    return result;
}


/* Initialize device info with invalid values (maxInputChannels and maxOutputChannels are set to zero since these indicate
 * whether input/output is available) */
static void InitializeDeviceInfo( PaDeviceInfo *deviceInfo )
{
    deviceInfo->structVersion = -1;
    deviceInfo->name = NULL;
    deviceInfo->hostApi = -1;
    deviceInfo->maxInputChannels = 0;
    deviceInfo->maxOutputChannels = 0;
    deviceInfo->defaultLowInputLatency = -1.;
    deviceInfo->defaultLowOutputLatency = -1.;
    deviceInfo->defaultHighInputLatency = -1.;
    deviceInfo->defaultHighOutputLatency = -1.;
    deviceInfo->defaultSampleRate = -1.;
}


/* Determine max channels and default latencies.
 *
 * This function provides functionality to grope an opened (might be opened for capture or playback) pcm device for
 * traits like max channels, suitable default latencies and default sample rate. Upon error, max channels is set to zero,
 * and a suitable result returned. The device is closed before returning. */
static PaError GropeDevice( snd_pcm_t *pcm, int isPlug, StreamDirection mode, PaAlsaDeviceInfo *devInfo )
{
    PaError result = paNoError;
    snd_pcm_hw_params_t *hwParams;
    snd_pcm_uframes_t alsaBufferFrames, alsaPeriodFrames;
    unsigned int minChans, maxChans;
    int *minChannels, *maxChannels;
    double *defaultLowLatency, *defaultHighLatency, *defaultSampleRate =
        &devInfo->baseDeviceInfo.defaultSampleRate;
    double defaultSr = *defaultSampleRate;
    int dir;

    assert( pcm );

    PA_DEBUG(( "  %s: collecting info [%s] ..\n", __FUNCTION__, ( mode == StreamDirection_In ? "Capture" : "Playback" ) ));

    if( mode == StreamDirection_In )
    {
        minChannels = &devInfo->minInputChannels;
        maxChannels = &devInfo->baseDeviceInfo.maxInputChannels;
        defaultLowLatency = &devInfo->baseDeviceInfo.defaultLowInputLatency;
        defaultHighLatency = &devInfo->baseDeviceInfo.defaultHighInputLatency;
    }
    else
    {
        minChannels = &devInfo->minOutputChannels;
        maxChannels = &devInfo->baseDeviceInfo.maxOutputChannels;
        defaultLowLatency = &devInfo->baseDeviceInfo.defaultLowOutputLatency;
        defaultHighLatency = &devInfo->baseDeviceInfo.defaultHighOutputLatency;
    }

    ENSURE_( alsa_snd_pcm_nonblock( pcm, 0 ), paUnanticipatedHostError );

    alsa_snd_pcm_hw_params_alloca( &hwParams );
    alsa_snd_pcm_hw_params_any( pcm, hwParams );

    if( defaultSr >= 0 )
    {
        /* Could be that the device opened in one mode supports samplerates that the other mode wont have,
         * so try again .. */
        if( Alsa_SetApproximateSampleRate( pcm, hwParams, defaultSr ) < 0 )
        {
            defaultSr = -1.;
            alsa_snd_pcm_hw_params_any( pcm, hwParams ); /* Clear any params (rate) that might have been set */
            PA_DEBUG(( "  %s: Original default samplerate failed, trying again ..\n", __FUNCTION__ ));
        }
    }

    if( defaultSr < 0. )           /* Default sample rate not set */
    {
        unsigned int sampleRate = 44100;        /* Will contain approximate rate returned by alsa-lib */

        /* Don't allow rate resampling when probing for the default rate (but ignore if this call fails) */
        alsa_snd_pcm_hw_params_set_rate_resample( pcm, hwParams, 0 );
        if( alsa_snd_pcm_hw_params_set_rate_near( pcm, hwParams, &sampleRate, NULL ) < 0 )
        {
            result = paUnanticipatedHostError;
            goto error;
        }
        ENSURE_( Alsa_GetExactSampleRate( hwParams, &defaultSr ), paUnanticipatedHostError );
    }

    ENSURE_( alsa_snd_pcm_hw_params_get_channels_min( hwParams, &minChans ), paUnanticipatedHostError );
    ENSURE_( alsa_snd_pcm_hw_params_get_channels_max( hwParams, &maxChans ), paUnanticipatedHostError );
    assert( maxChans <= INT_MAX );
    assert( maxChans > 0 );    /* Weird linking issue could cause wrong version of ALSA symbols to be called,
                                   resulting in zeroed values */

    /* XXX: Limit to sensible number (ALSA plugins accept a crazy amount of channels)? */
    if( isPlug && maxChans > 128 )
    {
        maxChans = 128;
        PA_DEBUG(( "  %s: Limiting number of plugin channels to %u\n", __FUNCTION__, maxChans ));
    }

    /* TWEAKME:
     * Giving values for default min and max latency is not straightforward.
     *  * for low latency, we want to give the lowest value that will work reliably.
     *      This varies based on the sound card, kernel, CPU, etc.  Better to give
     *      sub-optimal latency than to give a number too low and cause dropouts.
     *  * for high latency we want to give a large enough value that dropouts are basically impossible.
     *      This doesn't really require as much tweaking, since providing too large a number will
     *      just cause us to select the nearest setting that will work at stream config time.
     */
    /* Try low latency values, (sometimes the buffer & period that result are larger) */
    alsaBufferFrames = 512;
    alsaPeriodFrames = 128;
    ENSURE_( alsa_snd_pcm_hw_params_set_buffer_size_near( pcm, hwParams, &alsaBufferFrames ), paUnanticipatedHostError );
    ENSURE_( alsa_snd_pcm_hw_params_set_period_size_near( pcm, hwParams, &alsaPeriodFrames, &dir ), paUnanticipatedHostError );
    *defaultLowLatency = (double) (alsaBufferFrames - alsaPeriodFrames) / defaultSr;

    /* Base the high latency case on values four times larger */
    alsaBufferFrames = 2048;
    alsaPeriodFrames = 512;
    /* Have to reset hwParams, to set new buffer size; need to also set sample rate again */
    ENSURE_( alsa_snd_pcm_hw_params_any( pcm, hwParams ), paUnanticipatedHostError );
    ENSURE_( Alsa_SetApproximateSampleRate( pcm, hwParams, defaultSr ), paUnanticipatedHostError );
    ENSURE_( alsa_snd_pcm_hw_params_set_buffer_size_near( pcm, hwParams, &alsaBufferFrames ), paUnanticipatedHostError );
    ENSURE_( alsa_snd_pcm_hw_params_set_period_size_near( pcm, hwParams, &alsaPeriodFrames, &dir ), paUnanticipatedHostError );
    *defaultHighLatency = (double) (alsaBufferFrames - alsaPeriodFrames) / defaultSr;

    *minChannels = (int)minChans;
    *maxChannels = (int)maxChans;
    *defaultSampleRate = defaultSr;

end:
    alsa_snd_pcm_close( pcm );
    return result;

error:
    goto end;
}


/* Fills in info for the Pa-device using device info given, opening the device and probing hw params */
static PaError FillInDevInfo( PaAlsaHostApiRepresentation *alsaApi, DevInfo *deviceHwInfo, int blocking,
        PaAlsaDeviceInfo *devInfo, int *devIdx )
{
    PaError result = 0;
    PaDeviceInfo *baseDeviceInfo = &devInfo->baseDeviceInfo;
    snd_pcm_t *pcm = NULL;
    PaUtilHostApiRepresentation *baseApi = &alsaApi->baseHostApiRep;

    PA_DEBUG(( "%s: Filling device info for: %s\n", __FUNCTION__, deviceHwInfo->name ));

    /* Zero fields */
    InitializeDeviceInfo( baseDeviceInfo );

    /* To determine device capabilities, we must open the device and query the
     * hardware parameter configuration space */

    /* Query capture */
    if( ( deviceHwInfo->maxChansCapture > 0 ) &&
        Alsa_OpenPcm( &pcm, deviceHwInfo->alsaName, SND_PCM_STREAM_CAPTURE, blocking, 0 ) >= 0 )
    {
        if( GropeDevice( pcm, deviceHwInfo->isPlug, StreamDirection_In, devInfo ) != paNoError )
        {
            /* Error */
            PA_DEBUG(( "%s: Failed groping %s for capture\n", __FUNCTION__, deviceHwInfo->alsaName ));
            goto end;
        }
    }

    /* Query playback */
    if( ( deviceHwInfo->maxChansPlayback > 0 ) &&
        Alsa_OpenPcm( &pcm, deviceHwInfo->alsaName, SND_PCM_STREAM_PLAYBACK, blocking, 0 ) >= 0 )
    {
        if( GropeDevice( pcm, deviceHwInfo->isPlug, StreamDirection_Out, devInfo ) != paNoError )
        {
            /* Error */
            PA_DEBUG(( "%s: Failed groping %s for playback\n", __FUNCTION__, deviceHwInfo->alsaName ));
            goto end;
        }
    }

    baseDeviceInfo->structVersion = 2;
    baseDeviceInfo->hostApi = alsaApi->hostApiIndex;
    baseDeviceInfo->name = deviceHwInfo->name;
    devInfo->alsaName = deviceHwInfo->alsaName;
    devInfo->isPlug = deviceHwInfo->isPlug;

    /* A: Storing pointer to PaAlsaDeviceInfo object as pointer to PaDeviceInfo object.
     * Should now be safe to add device info, unless the device supports neither capture nor playback
     */
    if( baseDeviceInfo->maxInputChannels > 0 || baseDeviceInfo->maxOutputChannels > 0 )
    {
        /* Make device default if there isn't already one or it is the ALSA "default" device */
        if( ( baseApi->info.defaultInputDevice == paNoDevice ||
            strcmp( deviceHwInfo->alsaName, "default" ) == 0 ) && baseDeviceInfo->maxInputChannels > 0 )
        {
            baseApi->info.defaultInputDevice = *devIdx;
            PA_DEBUG(( "Setting default input device: %s\n", deviceHwInfo->name ));
        }
        if( ( baseApi->info.defaultOutputDevice == paNoDevice ||
            strcmp( deviceHwInfo->alsaName, "default" ) == 0 ) && baseDeviceInfo->maxOutputChannels > 0 )
        {
            baseApi->info.defaultOutputDevice = *devIdx;
            PA_DEBUG(( "Setting default output device: %s\n", deviceHwInfo->name ));
        }
        (*devIdx) += 1;
    }
    else
    {
        PA_DEBUG(( "%s: Skipped device: %s, all channels == 0\n", __FUNCTION__, deviceHwInfo->name ));
    }

end:
    return result;
}


/* Gather info about hardware on the system, soundcards, devices, (sub-devices).
 * 'hwDevInfosHdle' contains the address of an array of DevInfo structs; in normal use
 * this is NULL, and devCount 0, and memory is allocated as needed (later free() is required).
 * The DevInfo structs will be filled with brief info for each item found */
static PaError FindHardwareCardDevSub( PaAlsaHostApiRepresentation *alsaApi, DevInfo **hwDevInfosHdle,
                                        int *devCount, int *cardCount, unsigned int usePlughw )
{
    DevInfo *foundHwDevInfos = *hwDevInfosHdle;
    PaError result = paNoError;
    snd_ctl_card_info_t *cardInfo;
    snd_pcm_info_t *pcmInfo;
    int cardIdx = -1;
    char alsaCardStr[10]; /* The string used to open the snd_ctl interface (typically hw:x) */
    char *hwPrefix = "";
    int maxDeviceNames = *devCount > 0 ? *devCount: 1; /* At least this amount of space must aleady be allocated */

    alsa_snd_ctl_card_info_alloca( &cardInfo );
    alsa_snd_pcm_info_alloca( &pcmInfo );
    /* alsa_snd_card_next() returns 0 if it succeeded and modifies the integer passed to it to be:
     *      the index of the first card if the parameter is -1
     *      the index of the next card if the parameter is the index of a card
     *      -1 if there are no more cards */
    while( alsa_snd_card_next( &cardIdx ) == 0 && cardIdx >= 0 )
    {
        char *cardName;
        int deviceIdx = -1;
        snd_ctl_t *ctl;
        const CardHwConfig *cardPredef;
        char buf[50];

        /* Find out about the card by opening the hw:x control interface */
        snprintf( alsaCardStr, sizeof (alsaCardStr), "hw:%d", cardIdx );
        if( alsa_snd_ctl_open( &ctl, alsaCardStr, 0 ) < 0 )
        {
            /* Unable to open card :( */
            PA_DEBUG(( "%s: Unable to open device %s\n", __FUNCTION__, alsaCardStr ));
            continue;
        }
        alsa_snd_ctl_card_info( ctl, cardInfo );

        /* Acquire name of card */
        PA_ENSURE( StrDuplA( alsaApi, &cardName, alsa_snd_ctl_card_info_get_name( cardInfo )) );
        PA_DEBUG(( "%s: Found card, index %d: [%s]\n", __FUNCTION__, cardIdx, cardName ));

        /* See if there is a matching predefined config for this card */
        cardPredef = FindCardPredef( cardIdx, cardName );

        /* Find out about devices on a card */
        while( alsa_snd_ctl_pcm_next_device( ctl, &deviceIdx ) == 0 && deviceIdx >= 0 )
        {
            char *alsaDeviceName, *deviceName, *infoName;
            size_t len;
            int chansPlayback = 0, chansCapture = 0, usePlug;

            /* Obtain info about this particular device */
            alsa_snd_pcm_info_set_device( pcmInfo, deviceIdx );
            alsa_snd_pcm_info_set_subdevice( pcmInfo, 0 );
            alsa_snd_pcm_info_set_stream( pcmInfo, SND_PCM_STREAM_CAPTURE );
            if( alsa_snd_ctl_pcm_info( ctl, pcmInfo ) >= 0 )
                chansCapture = CHANS_UNKNOWN;

            alsa_snd_pcm_info_set_stream( pcmInfo, SND_PCM_STREAM_PLAYBACK );
            if( alsa_snd_ctl_pcm_info( ctl, pcmInfo ) >= 0 )
                chansPlayback = CHANS_UNKNOWN;

            if( chansPlayback == 0 && chansCapture == 0 )
                continue; /* Unexpected, but just move on to the next! */

            /* The 'plughw:' pcm will be used instead of 'hw:', set either globally or in a card config */
            usePlug = usePlughw || ( cardPredef && (cardPredef->plughwFlags & ( 1 << deviceIdx ) ));
            hwPrefix = usePlug ? "plug" : "";

            PA_DEBUG(( "%s %d", (deviceIdx == 0 ? "  ..devices (indexes):" : ","), deviceIdx ));

            /* Put together the names for the device info, allocating memory */
            infoName = SkipCardDetailsInName( (char *)alsa_snd_pcm_info_get_name( pcmInfo ), cardName );
            snprintf( buf, sizeof (buf), "%s%s,%d", hwPrefix, alsaCardStr, deviceIdx );
            /* Workout the length of the string written by snprintf plus terminating 0 */
            len = snprintf( NULL, 0, "%s: %s (%s)", cardName, infoName, buf ) + 1;
            PA_UNLESS( deviceName = (char *)PaUtil_GroupAllocateMemory( alsaApi->allocations, len ),
                    paInsufficientMemory );
            snprintf( deviceName, len, "%s: %s (%s)", cardName, infoName, buf );

            (*devCount)++;
            if( !foundHwDevInfos || *devCount > maxDeviceNames )
            {
                maxDeviceNames *= 2;
                PA_UNLESS( foundHwDevInfos = (DevInfo *) realloc( foundHwDevInfos, maxDeviceNames * sizeof (DevInfo) ),
                        paInsufficientMemory );
            }

            PA_ENSURE( StrDuplA( alsaApi, &alsaDeviceName, buf ) );

            foundHwDevInfos[*devCount - 1].alsaName = alsaDeviceName;
            foundHwDevInfos[*devCount - 1].name = deviceName;
            foundHwDevInfos[*devCount - 1].isPlug = usePlug;
            foundHwDevInfos[*devCount - 1].maxChansPlayback = chansPlayback;
            foundHwDevInfos[*devCount - 1].maxChansCapture = chansCapture;
        }
        PA_DEBUG(( "\n" ));
        alsa_snd_ctl_close( ctl );
        *cardCount = cardIdx + 1; /* The last time round the loop will leave the count correct */
    }
    *hwDevInfosHdle = foundHwDevInfos; /* Pass back the handle to the DevInfo structs */

error:
    return result;
}


/* Gather a list of the Alsa pcms on the system
 * 'pcmDevInfosHdle' contains the address of an array of DevInfo structs; in normal use
 * this is NULL, and devCount 0, and memory is allocated as needed (later free() is required).
 * The DevInfo structs will be filled with brief info for each item found
 *  */
static PaError ListValidPcms( PaAlsaHostApiRepresentation *alsaApi, DevInfo **pcmDevInfosHdle, int *devCount )
{
    DevInfo *foundPcmDevInfos = *pcmDevInfosHdle;
    PaError result = paNoError;
    snd_config_t *topNode = NULL;
    int res;
    int maxPcmNames = *devCount > 0 ? *devCount: 1;

    /* Iterate over pcms */
    if( (*alsa_snd_config) == NULL )
    {
        /* alsa_snd_config_update is called implicitly by some functions, if this hasn't happened snd_config will be NULL (bleh) */
        PA_DEBUG(( "Updating snd_config\n" ));
        ENSURE_( alsa_snd_config_update(), paUnanticipatedHostError );
    }
    assert( *alsa_snd_config );
    if( ( res = alsa_snd_config_search( snd_config, "pcm", &topNode ) ) >= 0 )
    {
        snd_config_iterator_t i, next;

        alsa_snd_config_for_each( i, next, topNode )
        {
            const char *tpStr = "unknown", *idStr = NULL;
            int err = 0;

            char *alsaDeviceName, *deviceName;
            const PcmDevConfig *predefinedPcm = NULL;
            snd_config_t *n = alsa_snd_config_iterator_entry( i ), * tp = NULL;;

            if( (err = alsa_snd_config_search( n, "type", &tp )) < 0 )
            {
                if( err != -ENOENT )
                    ENSURE_(err, paUnanticipatedHostError);
            }
            else
            {
                ENSURE_( alsa_snd_config_get_string( tp, &tpStr ), paUnanticipatedHostError );
            }
            ENSURE_( alsa_snd_config_get_id( n, &idStr ), paUnanticipatedHostError );

            predefinedPcm = FindPcmPredef( idStr );
            if( predefinedPcm && ( predefinedPcm->numPlaybackChans == 0 && predefinedPcm->numCaptureChans == 0 ) )
            {
                PA_DEBUG(( "%s: Ignoring ALSA pcm: [%s] of type [%s]\n", __FUNCTION__, idStr, tpStr ));
                continue;
            }
            PA_DEBUG(( "%s: Found pcm: [%s] of type [%s]\n", __FUNCTION__, idStr, tpStr ));

            PA_UNLESS( alsaDeviceName = (char*)PaUtil_GroupAllocateMemory( alsaApi->allocations,
                                                            strlen(idStr) + 6 ), paInsufficientMemory );
            strcpy( alsaDeviceName, idStr );
            PA_UNLESS( deviceName = (char*)PaUtil_GroupAllocateMemory( alsaApi->allocations,
                                                            strlen(idStr) + 1 ), paInsufficientMemory );
            strcpy( deviceName, idStr );

            (*devCount)++;
            if( !foundPcmDevInfos || *devCount > maxPcmNames )
            {
                maxPcmNames *= 2;
                PA_UNLESS( foundPcmDevInfos = (DevInfo *) realloc( foundPcmDevInfos, maxPcmNames * sizeof (DevInfo) ),
                        paInsufficientMemory );
            }

            foundPcmDevInfos[*devCount - 1].alsaName = alsaDeviceName;
            foundPcmDevInfos[*devCount - 1].name     = deviceName;
            foundPcmDevInfos[*devCount - 1].isPlug   = 1;

            if( predefinedPcm )
            {
                foundPcmDevInfos[*devCount - 1].maxChansPlayback = predefinedPcm->numPlaybackChans;
                foundPcmDevInfos[*devCount - 1].maxChansCapture  = predefinedPcm->numCaptureChans;
            }
            else
            {
                foundPcmDevInfos[*devCount - 1].maxChansPlayback = CHANS_UNKNOWN;
                foundPcmDevInfos[*devCount - 1].maxChansCapture  = CHANS_UNKNOWN;
            }
        }
    }
    else
    {
        PA_DEBUG(( "%s: Iterating over ALSA plugins failed: %s\n", __FUNCTION__, alsa_snd_strerror( res ) ));
    }
    *pcmDevInfosHdle = foundPcmDevInfos; /* Pass back the handle to the DevInfo structs */

error:
    return result;
}


/* Add a pcm-type device to a given device list, possibly as an extended device with card suffix if an idex is given */
PaError AddDeviceToList( PaAlsaHostApiRepresentation *alsaApi, DevInfo **dstInfosHdle, DevInfo srcInfos, int *devCount, int cdIdx )
{
    DevInfo *addedDevInfos = *dstInfosHdle;
    PaError result = paNoError;
    char *deviceName;
    int maxAddedDevs = *devCount > 0 ? *devCount: 1;

    PA_UNLESS( deviceName = (char*)PaUtil_GroupAllocateMemory( alsaApi->allocations,
                                                            strlen(srcInfos.name) + 4 ), paInsufficientMemory );
    if( cdIdx < 0 )
        strcpy( deviceName, srcInfos.name ); /* Don't add  card suffix */
    else
        sprintf( deviceName, "%s:%d", srcInfos.name, cdIdx );

    (*devCount)++;
    if( !addedDevInfos || *devCount > maxAddedDevs )
    {
        maxAddedDevs *= 2;
        PA_UNLESS( addedDevInfos = (DevInfo *) realloc( addedDevInfos, maxAddedDevs * sizeof (DevInfo) ),
                paInsufficientMemory );
    }

    PA_DEBUG(( "%s: Forming extended device %d: %s\n", __FUNCTION__, *devCount, deviceName ));
    /* Copy the info src->dst, and overwrite the names */
    memcpy( &addedDevInfos[*devCount - 1], &srcInfos, sizeof(DevInfo) );
    addedDevInfos[*devCount - 1].alsaName = deviceName;
    addedDevInfos[*devCount - 1].name     = deviceName;

    *dstInfosHdle = addedDevInfos; /* Pass back the handle to the DevInfo structs */

error:
    return result;
}


#if 0
/* Check through a list of hw infos for a given card and test if it has at least the num channels requested
 * Those channels could be on any of the hw devices on that card */
int CheckCardMaxChans( DevInfo *hwInfoList, int numHwDevs, int cardIdx, int reqPlayChans, int reqCaptChans )
{
}
#endif



/** Build a list of Portaudio devices.
 *
 *  The Pa internal PaUtilHostApiRepresentation has an array of pointers to PaDeviceInfo structs for
 *  each device in the list.  Both the pointer array itself and the structures are allocated in
 *  this function as hostApi allocations.
 *  The process involves finding out about the specific Alsa system, formulating a list of
 *  Pa devices that will be available to the 'user' and filling in the PaDeviceInfo.
 *  Ignore devices for which we cannot determine capabilities (possibly busy, sigh) */
PaError AlsaDevs_BuildList( PaAlsaHostApiRepresentation *alsaApi )
{
    PaUtilHostApiRepresentation *baseApi = &alsaApi->baseHostApiRep;
    PaAlsaDeviceInfo *hwDeviceInfos;
    PaAlsaDeviceInfo *deviceInfoArray;
    int numCards, devIdx;
    PaError result = paNoError;
    int numHwDeviceNames = 0, numPcmNames = 0, numExtDevs = 0, i, j;
    DevInfo *hwDevInfos = NULL; /* Buildup an array of structs with hw info */
    DevInfo *pcmDevInfos = NULL; /* Buildup an array of structs with pcm info */
    int blocking = SND_PCM_NONBLOCK;
    int usePlughw = 0;
#ifdef PA_ENABLE_DEBUG_OUTPUT
    PaTime startTime = PaUtil_GetTime();
#endif

/*---- Set any config options in place ----*/

    if( getenv( "PA_ALSA_INITIALIZE_BLOCK" ) && atoi( getenv( "PA_ALSA_INITIALIZE_BLOCK" ) ) )
        blocking = 0;
    /* If PA_ALSA_PLUGHW is 1 (non-zero), use the plughw: pcm throughout instead of hw: */
    if( getenv( "PA_ALSA_PLUGHW" ) && atoi( getenv( "PA_ALSA_PLUGHW" ) ) )
    {
        usePlughw = 1;
        PA_DEBUG(( "%s: env 'Alsa Plughw' set, for all hw devices\n", __FUNCTION__ ));
    }

    /* These two will be set to the first working input and output device, respectively */
    baseApi->info.defaultInputDevice = paNoDevice;
    baseApi->info.defaultOutputDevice = paNoDevice;


/*---- Find out details about the audio system ----*/

    /* Get info about hardware available; memory for hwDevInfos is allocated, info put in, numHwDeviceNames incremented */
    PA_ENSURE( FindHardwareCardDevSub( alsaApi, &hwDevInfos, &numHwDeviceNames, &numCards, usePlughw) );

    PA_DEBUG(( "%s: Number of cards: %d\n", __FUNCTION__, numCards ));

    /* Allocate memory, just for the hw device info structs; since this is contiguous, it can be indexed into */
    PA_UNLESS( hwDeviceInfos = (PaAlsaDeviceInfo*)PaUtil_GroupAllocateMemory( alsaApi->allocations,
                sizeof(PaAlsaDeviceInfo) * (numHwDeviceNames) ), paInsufficientMemory );

    /* Loop over list of cards/devices/[sub-devs] now, filling in info, as it is useful in making pcm choices.
     * If a device is deemed unavailable, it is ignored. */
    devIdx = 0; /* This will be incremented by FillInDevInfo() if the device is not skipped */
    PA_DEBUG(( "%s: Filling hw device info for %d devices\n", __FUNCTION__, numHwDeviceNames ));
    for( i = 0; i < numHwDeviceNames; ++i )
    {
        PaAlsaDeviceInfo *devInfo = &hwDeviceInfos[i];
        DevInfo *hwInfo = &hwDevInfos[i];

        PA_ENSURE( FillInDevInfo( alsaApi, hwInfo, blocking, devInfo, &devIdx ) );
    }
    assert( devIdx <= numHwDeviceNames );
    numHwDeviceNames = devIdx; /* This will reduce it to the actual number in the list */


    /* Get info about the Alsa pcms; memory for pcmDevInfos is allocated, info put in, numPcmNames incremented */
    PA_ENSURE( ListValidPcms( alsaApi, &pcmDevInfos, &numPcmNames ) );


/*---- Determine what to present to the 'user' as Portaudio 'devices' ----*/

    /* We now know the hardware (card, device [sub-dev]) and available pcms.
     * Each pcm can open some parts of the hardware.
     * To help clarify the options, distinguish the Pa devices as either 'basic' or 'extended':
     *     Basic ones - closely mirror the underlying hardware; these use just the 'hw' and 'plughw' pcms
     *     Extended ones - these pull in complex Alsa functionality such as routing & conversions and normally
     *            operate at the 'card' level only.  All other pcms create devices in this category. */

    /* The 'basic' pa_devices correspond to the existing hwDevice list.  For the 'extended' devices,
     * form yet another (tmp) list of DevInfo structs, as most pcms should be listed for each card. */
    DevInfo *extDevInfos = NULL; /* Buildup an array of structs with extended devices info */
    const PcmDevConfig *predefinedPcm = NULL;

    for( i = 0; i < numPcmNames; i++ )
    {
        //PA_DEBUG(( "%s: Check name %d: %s\n", __FUNCTION__, i, pcmDevInfos[i].name ));
        predefinedPcm = FindPcmPredef( pcmDevInfos[i].name );
        if( predefinedPcm && predefinedPcm->cardsFlags != 0 )
        {
            for( j = 0; j < numCards; j++ )
            {
                //PA_DEBUG(( "%s: Ext device, each card %d: %s\n", __FUNCTION__, j, pcmDevInfos[i].name ));
                if( predefinedPcm->cardsFlags & ( 1 << j ) ) /* The corresponding bit is set */
                    PA_ENSURE( AddDeviceToList( alsaApi, &extDevInfos, pcmDevInfos[i], &numExtDevs, j ) );
            }
        }
        else
        {
            PA_ENSURE( AddDeviceToList( alsaApi, &extDevInfos, pcmDevInfos[i], &numExtDevs, -1 ) ); /* Indicate no card ext with '-1' */
        }
    }


    /* The last stage is to build the final list of Alsa 'Portaudio-devices',
     * For now, list the hw devices, then the extended pcms in the order they come */


    /* Allocate deviceInfos array based on the total number of devices (basic/hw and extended) */
    PA_UNLESS( baseApi->deviceInfos = (PaDeviceInfo**)PaUtil_GroupAllocateMemory( alsaApi->allocations,
                sizeof(PaDeviceInfo*) * (numHwDeviceNames+numExtDevs) ), paInsufficientMemory );

    /* Fill the first deviceInfos pointers from the hwDeviceInfos array of structs that are already filled in */
    for( i = 0; i < numHwDeviceNames; ++i )
    {
        PaAlsaDeviceInfo *devInfo = &hwDeviceInfos[i];
        PA_DEBUG(( "%s: Adding basic device %d: %s\n", __FUNCTION__, i, devInfo->baseDeviceInfo.name ));
        baseApi->deviceInfos[i] = (PaDeviceInfo *)devInfo;
    }


    /* Allocate info structs for the extended/pcms in a contiguous block */
    PA_UNLESS( deviceInfoArray = (PaAlsaDeviceInfo*)PaUtil_GroupAllocateMemory( alsaApi->allocations,
                sizeof(PaAlsaDeviceInfo) * (numExtDevs) ), paInsufficientMemory );

    /* Now loop over the extended device list, filling in the final info.
     * Note that we do this in two stages. This is a workaround owing to the fact that the 'dmix'
     * plugin may cause the underlying hardware device to be busy for a short while even after it
     * (dmix) is closed. The 'default' plugin may also point to the dmix plugin, so the same goes
     * for this. */
    PA_DEBUG(( "%s: Filling pcm device info for %d devices\n", __FUNCTION__, numExtDevs ));
    for( i = 0; i < numExtDevs; ++i )
    {
        PaAlsaDeviceInfo *devInfo = &deviceInfoArray[i];
        DevInfo *pcmInfo = &extDevInfos[i];
        int priorIdx = devIdx;
        if( !strncmp( pcmInfo->name, "dmix", 4 ) || !strncmp( pcmInfo->name, "default", 7 ) )
            continue;

        PA_ENSURE( FillInDevInfo( alsaApi, pcmInfo, blocking, devInfo, &devIdx ) );
        if( devIdx > priorIdx )
        {   /* A device has been added (not skipped) */
            PA_DEBUG(( "%s: Adding extended device %d: %s\n", __FUNCTION__, devIdx - 1, devInfo->baseDeviceInfo.name ));
            baseApi->deviceInfos[devIdx-1] = (PaDeviceInfo *)devInfo;
        }
    }

    /* Now inspect 'dmix' and 'default' plugins */
    for( i = 0; i < numExtDevs; ++i )
    {
        PaAlsaDeviceInfo *devInfo = &deviceInfoArray[i];
        DevInfo *pcmInfo = &extDevInfos[i];
        int priorIdx = devIdx;
        if( strncmp( pcmInfo->name, "dmix", 4 ) && strncmp( pcmInfo->name, "default", 7 ) )
            continue;

        PA_ENSURE( FillInDevInfo( alsaApi, pcmInfo, blocking, devInfo, &devIdx ) );
        if( devIdx > priorIdx )
        {   /* A device has been added (not skipped) */
            PA_DEBUG(( "%s: Adding extended device %d: %s\n", __FUNCTION__, devIdx - 1, devInfo->baseDeviceInfo.name ));
            baseApi->deviceInfos[devIdx-1] = (PaDeviceInfo *)devInfo;
        }
    }
    assert( devIdx <= numHwDeviceNames+numExtDevs );

    free( hwDevInfos );
    free( pcmDevInfos ); // TODO may wish to free name allocations not in the final list?
    free( extDevInfos );

    baseApi->info.deviceCount = devIdx;   /* Number of successfully queried devices */

#ifdef PA_ENABLE_DEBUG_OUTPUT
    PA_DEBUG(( "%s: Building device list took %f seconds\n", __FUNCTION__, PaUtil_GetTime() - startTime ));
#endif

end:
    return result;

error:
    /* No particular action */
    goto end;
}

