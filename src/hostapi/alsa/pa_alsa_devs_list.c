/*
 * $Id$
 * PortAudio Portable Real-Time Audio Library
 * Latest Version at: http://www.portaudio.com
 * ALSA implementation by Joshua Haberman and Arve Knudsen
 *
 * Copyright (c) 2002 Joshua Haberman <joshua@haberman.com>
 * Copyright (c) 2005-2009 Arve Knudsen <arve.knudsen@gmail.com>
 * Copyright (c) 2008 Kevin Kofler <kevin.kofler@chello.at>
 * Copyright (c) 2014 Alan Horstmann <gineera@aspect135.co.uk>
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


#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API
#include <alsa/asoundlib.h>
#undef ALSA_PCM_NEW_HW_PARAMS_API
#undef ALSA_PCM_NEW_SW_PARAMS_API

#include <limits.h>

#include "portaudio.h"
#include "pa_util.h"
#include "pa_unix_util.h"
#include "pa_allocation.h"
#include "pa_hostapi.h"
#include "pa_stream.h"
#include "pa_process.h"
#include "pa_debugprint.h"

#include "pa_linux_alsa.h"
#include "pa_alsa_internal.h"
#include "pa_alsa_load_dyn.h"


/* Helper struct */
typedef struct
{
    char *alsaName;
    char *name;
    int isPlug;
    int hasPlayback;
    int hasCapture;
} HwDevInfo;


HwDevInfo predefinedNames[] = {
    { "center_lfe", NULL, 0, 1, 0 },
/* { "default", NULL, 0, 1, 1 }, */
    { "dmix", NULL, 0, 1, 0 },
/* { "dpl", NULL, 0, 1, 0 }, */
/* { "dsnoop", NULL, 0, 0, 1 }, */
    { "front", NULL, 0, 1, 0 },
    { "iec958", NULL, 0, 1, 0 },
/* { "modem", NULL, 0, 1, 0 }, */
    { "rear", NULL, 0, 1, 0 },
    { "side", NULL, 0, 1, 0 },
/*     { "spdif", NULL, 0, 0, 0 }, */
    { "surround40", NULL, 0, 1, 0 },
    { "surround41", NULL, 0, 1, 0 },
    { "surround50", NULL, 0, 1, 0 },
    { "surround51", NULL, 0, 1, 0 },
    { "surround71", NULL, 0, 1, 0 },

    { "AndroidPlayback_Earpiece_normal",         NULL, 0, 1, 0 },
    { "AndroidPlayback_Speaker_normal",          NULL, 0, 1, 0 },
    { "AndroidPlayback_Bluetooth_normal",        NULL, 0, 1, 0 },
    { "AndroidPlayback_Headset_normal",          NULL, 0, 1, 0 },
    { "AndroidPlayback_Speaker_Headset_normal",  NULL, 0, 1, 0 },
    { "AndroidPlayback_Bluetooth-A2DP_normal",   NULL, 0, 1, 0 },
    { "AndroidPlayback_ExtraDockSpeaker_normal", NULL, 0, 1, 0 },
    { "AndroidPlayback_TvOut_normal",            NULL, 0, 1, 0 },

    { "AndroidRecord_Microphone",                NULL, 0, 0, 1 },
    { "AndroidRecord_Earpiece_normal",           NULL, 0, 0, 1 },
    { "AndroidRecord_Speaker_normal",            NULL, 0, 0, 1 },
    { "AndroidRecord_Headset_normal",            NULL, 0, 0, 1 },
    { "AndroidRecord_Bluetooth_normal",          NULL, 0, 0, 1 },
    { "AndroidRecord_Speaker_Headset_normal",    NULL, 0, 0, 1 },

    { NULL, NULL, 0, 1, 0 }
};


static const HwDevInfo *FindDeviceName( const char *name )
{
    int i;

    for( i = 0; predefinedNames[i].alsaName; i++ )
    {
        if( strcmp( name, predefinedNames[i].alsaName ) == 0 )
        {
            return &predefinedNames[i];
        }
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


/* Disregard some standard plugins
 */
static int IgnorePlugin( const char *pluginId )
{
    static const char *ignoredPlugins[] = {"hw", "plughw", "plug", "dsnoop", "tee",
        "file", "null", "shm", "cards", "rate_convert", NULL};
    int i = 0;
    while( ignoredPlugins[i] )
    {
        if( !strcmp( pluginId, ignoredPlugins[i] ) )
        {
            return 1;
        }
        ++i;
    }

    return 0;
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


/** Determine max channels and default latencies.
 *
 * This function provides functionality to grope an opened (might be opened for capture or playback) pcm device for
 * traits like max channels, suitable default latencies and default sample rate. Upon error, max channels is set to zero,
 * and a suitable result returned. The device is closed before returning.
 */
static PaError GropeDevice( snd_pcm_t* pcm, int isPlug, StreamDirection mode, int openBlocking,
        PaAlsaDeviceInfo* devInfo )
{
    PaError result = paNoError;
    snd_pcm_hw_params_t *hwParams;
    snd_pcm_uframes_t alsaBufferFrames, alsaPeriodFrames;
    unsigned int minChans, maxChans;
    int* minChannels, * maxChannels;
    double * defaultLowLatency, * defaultHighLatency, * defaultSampleRate =
        &devInfo->baseDeviceInfo.defaultSampleRate;
    double defaultSr = *defaultSampleRate;
    int dir;

    assert( pcm );

    PA_DEBUG(( "%s: collecting info ..\n", __FUNCTION__ ));

    if( StreamDirection_In == mode )
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
            PA_DEBUG(( "%s: Original default samplerate failed, trying again ..\n", __FUNCTION__ ));
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
        PA_DEBUG(( "%s: Limiting number of plugin channels to %u\n", __FUNCTION__, maxChans ));
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


static PaError FillInDevInfo( PaAlsaHostApiRepresentation *alsaApi, HwDevInfo* deviceHwInfo, int blocking,
        PaAlsaDeviceInfo* devInfo, int* devIdx )
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
    if( deviceHwInfo->hasCapture &&
        Alsa_OpenPcm( &pcm, deviceHwInfo->alsaName, SND_PCM_STREAM_CAPTURE, blocking, 0 ) >= 0 )
    {
        if( GropeDevice( pcm, deviceHwInfo->isPlug, StreamDirection_In, blocking, devInfo ) != paNoError )
        {
            /* Error */
            PA_DEBUG(( "%s: Failed groping %s for capture\n", __FUNCTION__, deviceHwInfo->alsaName ));
            goto end;
        }
    }

    /* Query playback */
    if( deviceHwInfo->hasPlayback &&
        Alsa_OpenPcm( &pcm, deviceHwInfo->alsaName, SND_PCM_STREAM_PLAYBACK, blocking, 0 ) >= 0 )
    {
        if( GropeDevice( pcm, deviceHwInfo->isPlug, StreamDirection_Out, blocking, devInfo ) != paNoError )
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
            !strcmp( deviceHwInfo->alsaName, "default" ) ) && baseDeviceInfo->maxInputChannels > 0 )
        {
            baseApi->info.defaultInputDevice = *devIdx;
            PA_DEBUG(( "Default input device: %s\n", deviceHwInfo->name ));
        }
        if( ( baseApi->info.defaultOutputDevice == paNoDevice ||
            !strcmp( deviceHwInfo->alsaName, "default" ) ) && baseDeviceInfo->maxOutputChannels > 0 )
        {
            baseApi->info.defaultOutputDevice = *devIdx;
            PA_DEBUG(( "Default output device: %s\n", deviceHwInfo->name ));
        }
        PA_DEBUG(( "%s: Adding device %s: %d\n", __FUNCTION__, deviceHwInfo->name, *devIdx ));
        baseApi->deviceInfos[*devIdx] = (PaDeviceInfo *) devInfo;
        (*devIdx) += 1;
    }
    else
    {
        PA_DEBUG(( "%s: Skipped device: %s, all channels == 0\n", __FUNCTION__, deviceHwInfo->name ));
    }

end:
    return result;
}



/* Build PaDeviceInfo list, ignore devices for which we cannot determine capabilities (possibly busy, sigh) */
PaError AlsaDevs_BuildList( PaAlsaHostApiRepresentation *alsaApi )
{
    PaUtilHostApiRepresentation *baseApi = &alsaApi->baseHostApiRep;
    PaAlsaDeviceInfo *deviceInfoArray;
    int cardIdx = -1, devIdx = 0;
    snd_ctl_card_info_t *cardInfo;
    PaError result = paNoError;
    size_t numDeviceNames = 0, maxDeviceNames = 1, i;
    HwDevInfo *hwDevInfos = NULL;
    snd_config_t *topNode = NULL;
    snd_pcm_info_t *pcmInfo;
    int res;
    int blocking = SND_PCM_NONBLOCK;
    int usePlughw = 0;
    char *hwPrefix = "";
    char alsaCardName[50];
#ifdef PA_ENABLE_DEBUG_OUTPUT
    PaTime startTime = PaUtil_GetTime();
#endif

    if( getenv( "PA_ALSA_INITIALIZE_BLOCK" ) && atoi( getenv( "PA_ALSA_INITIALIZE_BLOCK" ) ) )
        blocking = 0;

    /* If PA_ALSA_PLUGHW is 1 (non-zero), use the plughw: pcm throughout instead of hw: */
    if( getenv( "PA_ALSA_PLUGHW" ) && atoi( getenv( "PA_ALSA_PLUGHW" ) ) )
    {
        usePlughw = 1;
        hwPrefix = "plug";
        PA_DEBUG(( "%s: Using Plughw\n", __FUNCTION__ ));
    }

    /* These two will be set to the first working input and output device, respectively */
    baseApi->info.defaultInputDevice = paNoDevice;
    baseApi->info.defaultOutputDevice = paNoDevice;

    /* Gather info about hw devices

     * alsa_snd_card_next() modifies the integer passed to it to be:
     *      the index of the first card if the parameter is -1
     *      the index of the next card if the parameter is the index of a card
     *      -1 if there are no more cards
     *
     * The function itself returns 0 if it succeeded. */
    cardIdx = -1;
    alsa_snd_ctl_card_info_alloca( &cardInfo );
    alsa_snd_pcm_info_alloca( &pcmInfo );
    while( alsa_snd_card_next( &cardIdx ) == 0 && cardIdx >= 0 )
    {
        char *cardName;
        int devIdx = -1;
        snd_ctl_t *ctl;
        char buf[50];

        snprintf( alsaCardName, sizeof (alsaCardName), "hw:%d", cardIdx );

        /* Acquire name of card */
        if( alsa_snd_ctl_open( &ctl, alsaCardName, 0 ) < 0 )
        {
            /* Unable to open card :( */
            PA_DEBUG(( "%s: Unable to open device %s\n", __FUNCTION__, alsaCardName ));
            continue;
        }
        alsa_snd_ctl_card_info( ctl, cardInfo );

        PA_ENSURE( StrDuplA( alsaApi, &cardName, alsa_snd_ctl_card_info_get_name( cardInfo )) );

        while( alsa_snd_ctl_pcm_next_device( ctl, &devIdx ) == 0 && devIdx >= 0 )
        {
            char *alsaDeviceName, *deviceName, *infoName;
            size_t len;
            int hasPlayback = 0, hasCapture = 0;

            snprintf( buf, sizeof (buf), "%s%s,%d", hwPrefix, alsaCardName, devIdx );

            /* Obtain info about this particular device */
            alsa_snd_pcm_info_set_device( pcmInfo, devIdx );
            alsa_snd_pcm_info_set_subdevice( pcmInfo, 0 );
            alsa_snd_pcm_info_set_stream( pcmInfo, SND_PCM_STREAM_CAPTURE );
            if( alsa_snd_ctl_pcm_info( ctl, pcmInfo ) >= 0 )
            {
                hasCapture = 1;
            }

            alsa_snd_pcm_info_set_stream( pcmInfo, SND_PCM_STREAM_PLAYBACK );
            if( alsa_snd_ctl_pcm_info( ctl, pcmInfo ) >= 0 )
            {
                hasPlayback = 1;
            }

            if( !hasPlayback && !hasCapture )
            {
                /* Error */
                continue;
            }

            infoName = SkipCardDetailsInName( (char *)alsa_snd_pcm_info_get_name( pcmInfo ), cardName );

            /* The length of the string written by snprintf plus terminating 0 */
            len = snprintf( NULL, 0, "%s: %s (%s)", cardName, infoName, buf ) + 1;
            PA_UNLESS( deviceName = (char *)PaUtil_GroupAllocateMemory( alsaApi->allocations, len ),
                    paInsufficientMemory );
            snprintf( deviceName, len, "%s: %s (%s)", cardName, infoName, buf );

            ++numDeviceNames;
            if( !hwDevInfos || numDeviceNames > maxDeviceNames )
            {
                maxDeviceNames *= 2;
                PA_UNLESS( hwDevInfos = (HwDevInfo *) realloc( hwDevInfos, maxDeviceNames * sizeof (HwDevInfo) ),
                        paInsufficientMemory );
            }

            PA_ENSURE( StrDuplA( alsaApi, &alsaDeviceName, buf ) );

            hwDevInfos[ numDeviceNames - 1 ].alsaName = alsaDeviceName;
            hwDevInfos[ numDeviceNames - 1 ].name = deviceName;
            hwDevInfos[ numDeviceNames - 1 ].isPlug = usePlughw;
            hwDevInfos[ numDeviceNames - 1 ].hasPlayback = hasPlayback;
            hwDevInfos[ numDeviceNames - 1 ].hasCapture = hasCapture;
        }
        alsa_snd_ctl_close( ctl );
    }

    /* Iterate over plugin devices */
    if( NULL == (*alsa_snd_config) )
    {
        /* alsa_snd_config_update is called implicitly by some functions, if this hasn't happened snd_config will be NULL (bleh) */
        ENSURE_( alsa_snd_config_update(), paUnanticipatedHostError );
        PA_DEBUG(( "Updating snd_config\n" ));
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
            const HwDevInfo *predefined = NULL;
            snd_config_t *n = alsa_snd_config_iterator_entry( i ), * tp = NULL;;

            if( (err = alsa_snd_config_search( n, "type", &tp )) < 0 )
            {
                if( -ENOENT != err )
                {
                    ENSURE_(err, paUnanticipatedHostError);
                }
            }
            else
            {
                ENSURE_( alsa_snd_config_get_string( tp, &tpStr ), paUnanticipatedHostError );
            }
            ENSURE_( alsa_snd_config_get_id( n, &idStr ), paUnanticipatedHostError );
            if( IgnorePlugin( idStr ) )
            {
                PA_DEBUG(( "%s: Ignoring ALSA plugin device [%s] of type [%s]\n", __FUNCTION__, idStr, tpStr ));
                continue;
            }
            PA_DEBUG(( "%s: Found plugin [%s] of type [%s]\n", __FUNCTION__, idStr, tpStr ));

            PA_UNLESS( alsaDeviceName = (char*)PaUtil_GroupAllocateMemory( alsaApi->allocations,
                                                            strlen(idStr) + 6 ), paInsufficientMemory );
            strcpy( alsaDeviceName, idStr );
            PA_UNLESS( deviceName = (char*)PaUtil_GroupAllocateMemory( alsaApi->allocations,
                                                            strlen(idStr) + 1 ), paInsufficientMemory );
            strcpy( deviceName, idStr );

            ++numDeviceNames;
            if( !hwDevInfos || numDeviceNames > maxDeviceNames )
            {
                maxDeviceNames *= 2;
                PA_UNLESS( hwDevInfos = (HwDevInfo *) realloc( hwDevInfos, maxDeviceNames * sizeof (HwDevInfo) ),
                        paInsufficientMemory );
            }

            predefined = FindDeviceName( alsaDeviceName );

            hwDevInfos[numDeviceNames - 1].alsaName = alsaDeviceName;
            hwDevInfos[numDeviceNames - 1].name     = deviceName;
            hwDevInfos[numDeviceNames - 1].isPlug   = 1;

            if( predefined )
            {
                hwDevInfos[numDeviceNames - 1].hasPlayback = predefined->hasPlayback;
                hwDevInfos[numDeviceNames - 1].hasCapture  = predefined->hasCapture;
            }
            else
            {
                hwDevInfos[numDeviceNames - 1].hasPlayback = 1;
                hwDevInfos[numDeviceNames - 1].hasCapture  = 1;
            }
        }
    }
    else
        PA_DEBUG(( "%s: Iterating over ALSA plugins failed: %s\n", __FUNCTION__, alsa_snd_strerror( res ) ));

    /* allocate deviceInfo memory based on the number of devices */
    PA_UNLESS( baseApi->deviceInfos = (PaDeviceInfo**)PaUtil_GroupAllocateMemory(
            alsaApi->allocations, sizeof(PaDeviceInfo*) * (numDeviceNames) ), paInsufficientMemory );

    /* allocate all device info structs in a contiguous block */
    PA_UNLESS( deviceInfoArray = (PaAlsaDeviceInfo*)PaUtil_GroupAllocateMemory(
            alsaApi->allocations, sizeof(PaAlsaDeviceInfo) * numDeviceNames ), paInsufficientMemory );

    /* Loop over list of cards, filling in info. If a device is deemed unavailable (can't get name),
     * it's ignored.
     *
     * Note that we do this in two stages. This is a workaround owing to the fact that the 'dmix'
     * plugin may cause the underlying hardware device to be busy for a short while even after it
     * (dmix) is closed. The 'default' plugin may also point to the dmix plugin, so the same goes
     * for this.
     */
    PA_DEBUG(( "%s: Filling device info for %d devices\n", __FUNCTION__, numDeviceNames ));
    for( i = 0, devIdx = 0; i < numDeviceNames; ++i )
    {
        PaAlsaDeviceInfo* devInfo = &deviceInfoArray[i];
        HwDevInfo* hwInfo = &hwDevInfos[i];
        if( !strcmp( hwInfo->name, "dmix" ) || !strcmp( hwInfo->name, "default" ) )
        {
            continue;
        }

        PA_ENSURE( FillInDevInfo( alsaApi, hwInfo, blocking, devInfo, &devIdx ) );
    }
    assert( devIdx < numDeviceNames );
    /* Now inspect 'dmix' and 'default' plugins */
    for( i = 0; i < numDeviceNames; ++i )
    {
        PaAlsaDeviceInfo* devInfo = &deviceInfoArray[i];
        HwDevInfo* hwInfo = &hwDevInfos[i];
        if( strcmp( hwInfo->name, "dmix" ) && strcmp( hwInfo->name, "default" ) )
        {
            continue;
        }

        PA_ENSURE( FillInDevInfo( alsaApi, hwInfo, blocking, devInfo, &devIdx ) );
    }
    free( hwDevInfos );

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

