#ifndef PA_ALSA_INTERNAL_H
#define PA_ALSA_INTERNAL_H
/*
 * $Id: pa_alsa_internal.h 1911 2014-12-17 12:44:09Z gineera $
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


/* Check return value of ALSA function, and map it to PaError */
#define ENSURE_(expr, code) \
    do { \
        int __pa_unsure_error_id;\
        if( UNLIKELY( (__pa_unsure_error_id = (expr)) < 0 ) ) \
        { \
            /* PaUtil_SetLastHostErrorInfo should only be used in the main thread */ \
            if( (code) == paUnanticipatedHostError && pthread_equal( pthread_self(), paUnixMainThread) ) \
            { \
                PaUtil_SetLastHostErrorInfo( paALSA, __pa_unsure_error_id, alsa_snd_strerror( __pa_unsure_error_id ) ); \
            } \
            PaUtil_DebugPrint( "Expression '" #expr "' failed in '" __FILE__ "', line: " STRINGIZE( __LINE__ ) "\n" ); \
            if( (code) == paUnanticipatedHostError ) \
                PA_DEBUG(( "Host error description: %s\n", alsa_snd_strerror( __pa_unsure_error_id ) )); \
            result = (code); \
            goto error; \
        } \
    } while (0)

#define ASSERT_CALL_(expr, success) \
    do {\
        int __pa_assert_error_id;\
        __pa_assert_error_id = (expr);\
        assert( success == __pa_assert_error_id );\
    } while (0)


/* PaAlsaHostApiRepresentation - host api datastructure specific to this implementation */
typedef struct PaAlsaHostApiRepresentation
{
    PaUtilHostApiRepresentation baseHostApiRep;
    PaUtilStreamInterface callbackStreamInterface;
    PaUtilStreamInterface blockingStreamInterface;

    PaUtilAllocationGroup *allocations;

    PaHostApiIndex hostApiIndex;
    PaUint32 alsaLibVersion; /* Retrieved from the library at run-time */
}
PaAlsaHostApiRepresentation;


typedef struct PaAlsaDeviceInfo
{
    PaDeviceInfo baseDeviceInfo;
    char *alsaName;
    int isPlug;
    int minInputChannels;
    int minOutputChannels;
}
PaAlsaDeviceInfo;


typedef enum
{
    StreamDirection_In,
    StreamDirection_Out
} StreamDirection;


/* Internal functions */
int Alsa_GetExactSampleRate( snd_pcm_hw_params_t *hwParams, double *sampleRate );
int Alsa_SetApproximateSampleRate( snd_pcm_t *pcm, snd_pcm_hw_params_t *hwParams, double sampleRate );
int Alsa_OpenPcm( snd_pcm_t **pcmp, const char *name, snd_pcm_stream_t stream, int mode, int waitOnBusy );
PaError AlsaDevs_BuildList( PaAlsaHostApiRepresentation *alsaApi );

#endif
