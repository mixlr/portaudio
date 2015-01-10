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


#include "pa_alsa_internal.h"

/*
 * The data in these tables sets the Portaudio device enumeraton behaviour,
 * in particular how the hardware Alsa (sound-)cards and PCM definitions are combined in
 * the 'AlsaDevs_BuildList(.)' process into a single 'flat' list.
 *
 * By carefully changing the values here, the default behaviour can be modified.
 * AT PRESENT, this is an experimental facility, intended for developers and advanced users.
 *    - Alan Horstmann, Jan 2015
 */


/* Macros for explicit initialisation (robust if struct changes) */
#define SET_CARD_DEVS_SUBS_PLUG(c, d, s, p) { .cardIdx = c, .devicesFlags = d, .subdevFlags = s, .plughwFlags = p }
#define SET_NAME_PLBK_CAPT_FLGS(n, p, c, f) { .pcmName = n, .numPlaybackChans = p, .numCaptureChans = c, .cardsFlags = f }
#define ALL ((unsigned int )-1)
#define END (-127)


/*
 * Predefined options active on a 'per-card' basis; this list is matched on card index.
 * The possible choices are all device bit-flags, for up to 32 devices, ie (1 << devIdx).
 *
 * AT PRESENT, only plughw flags are implemented; later, device and sub-device, and name matching.
 * This may not be needed as a predefine, finally? - or dynamically allocated and filled?
 */
CardHwConfig predefinedHw[] = {
    SET_CARD_DEVS_SUBS_PLUG( 0, ALL, 0, 0 ), /* Just as example; this is the default behaviour anyway */
    SET_CARD_DEVS_SUBS_PLUG( 1, ALL, 0, 0 ),
    SET_CARD_DEVS_SUBS_PLUG( 2, ALL, 0, 0 ),
    SET_CARD_DEVS_SUBS_PLUG( END, 0, 0, 0 )
};


/*
 * Predefined options active 'per-pcm', matched on pcm name.
 * If channels >0 play or capture, pcm is enabled; the number will have significance LATER.
 * The card flags set which cards each pcm is listed for, ie (1 << cardIdx); 'ALL' is just that!
 * The pcm is listed just once, without a card suffix, if the card flags are all 0.
 */
PcmDevConfig predefinedPcms[] = {
    SET_NAME_PLBK_CAPT_FLGS( "center_lfe", 1, 0, 0 ),
/*  { "default", 1, 1 }, */
    SET_NAME_PLBK_CAPT_FLGS( "dmix",   1, 0, 0 ),
/*  { "dpl", 1, 0 }, */
    SET_NAME_PLBK_CAPT_FLGS( "front",  2, 0, 0 ),
    SET_NAME_PLBK_CAPT_FLGS( "iec958", 2, 1, 0 ),
/*  { "modem", 1, 0 }, */
    SET_NAME_PLBK_CAPT_FLGS( "rear",   1, 0, 0 ),
    SET_NAME_PLBK_CAPT_FLGS( "side",   1, 0, 0 ),
/*  { "spdif", 0, 0 }, */
    SET_NAME_PLBK_CAPT_FLGS( "surround40", 4, 0, 0 ),
    SET_NAME_PLBK_CAPT_FLGS( "surround41", 5, 0, 0 ),
    SET_NAME_PLBK_CAPT_FLGS( "surround50", 5, 0, 0 ),
    SET_NAME_PLBK_CAPT_FLGS( "surround51", 6, 0, 0 ),
    SET_NAME_PLBK_CAPT_FLGS( "surround71", 8, 0, 0 ),

    SET_NAME_PLBK_CAPT_FLGS( "AndroidPlayback_Earpiece_normal",         1, 0, 0 ),
    SET_NAME_PLBK_CAPT_FLGS( "AndroidPlayback_Speaker_normal",          1, 0, 0 ),
    SET_NAME_PLBK_CAPT_FLGS( "AndroidPlayback_Bluetooth_normal",        1, 0, 0 ),
    SET_NAME_PLBK_CAPT_FLGS( "AndroidPlayback_Headset_normal",          1, 0, 0 ),
    SET_NAME_PLBK_CAPT_FLGS( "AndroidPlayback_Speaker_Headset_normal",  1, 0, 0 ),
    SET_NAME_PLBK_CAPT_FLGS( "AndroidPlayback_Bluetooth-A2DP_normal",   1, 0, 0 ),
    SET_NAME_PLBK_CAPT_FLGS( "AndroidPlayback_ExtraDockSpeaker_normal", 1, 0, 0 ),
    SET_NAME_PLBK_CAPT_FLGS( "AndroidPlayback_TvOut_normal",            1, 0, 0 ),

    SET_NAME_PLBK_CAPT_FLGS( "AndroidRecord_Microphone",                0, 1, 0 ),
    SET_NAME_PLBK_CAPT_FLGS( "AndroidRecord_Earpiece_normal",           0, 1, 0 ),
    SET_NAME_PLBK_CAPT_FLGS( "AndroidRecord_Speaker_normal",            0, 1, 0 ),
    SET_NAME_PLBK_CAPT_FLGS( "AndroidRecord_Headset_normal",            0, 1, 0 ),
    SET_NAME_PLBK_CAPT_FLGS( "AndroidRecord_Bluetooth_normal",          0, 1, 0 ),
    SET_NAME_PLBK_CAPT_FLGS( "AndroidRecord_Speaker_Headset_normal",    0, 1, 0 ),

    /* Setting both playback and capture to zero makes these ignored */
    SET_NAME_PLBK_CAPT_FLGS( "hw",     0, 0, 0 ),
    SET_NAME_PLBK_CAPT_FLGS( "plughw", 0, 0, 0 ),
    SET_NAME_PLBK_CAPT_FLGS( "plug",   0, 0, 0 ),
    SET_NAME_PLBK_CAPT_FLGS( "dsnoop", 0, 0, 0 ),
    SET_NAME_PLBK_CAPT_FLGS( "tee",    0, 0, 0 ),
    SET_NAME_PLBK_CAPT_FLGS( "file",   0, 0, 0 ),
    SET_NAME_PLBK_CAPT_FLGS( "null",   0, 0, 0 ),
    SET_NAME_PLBK_CAPT_FLGS( "shm",    0, 0, 0 ),
    SET_NAME_PLBK_CAPT_FLGS( "cards",  0, 0, 0 ),
    SET_NAME_PLBK_CAPT_FLGS( "rate_convert", 0, 0, 0 ),

    SET_NAME_PLBK_CAPT_FLGS( NULL, 0, 0, 0 )
};

