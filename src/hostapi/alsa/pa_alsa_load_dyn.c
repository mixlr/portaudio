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

#include "pa_alsa_load_dyn.h"
#ifdef PA_ALSA_DYNAMIC
    #include <dlfcn.h> /* For dlXXX functions */
#endif

#include "pa_debugprint.h"


#ifdef PA_ALSA_DYNAMIC
static const char *g_AlsaLibName = PA_ALSA_PATHNAME;
#endif
/* Handle to dynamically loaded library. */
static void *g_AlsaLib = NULL;



/* Defines pointers to the Alsa functions */
#define _PA_DEFINE_FUNC(x)  x##_ft *alsa_##x = 0

_PA_DEFINE_FUNC(snd_pcm_open);
_PA_DEFINE_FUNC(snd_pcm_close);
_PA_DEFINE_FUNC(snd_pcm_nonblock);
_PA_DEFINE_FUNC(snd_pcm_frames_to_bytes);
_PA_DEFINE_FUNC(snd_pcm_prepare);
_PA_DEFINE_FUNC(snd_pcm_start);
_PA_DEFINE_FUNC(snd_pcm_resume);
_PA_DEFINE_FUNC(snd_pcm_wait);
_PA_DEFINE_FUNC(snd_pcm_state);
_PA_DEFINE_FUNC(snd_pcm_avail_update);
_PA_DEFINE_FUNC(snd_pcm_areas_silence);
_PA_DEFINE_FUNC(snd_pcm_mmap_begin);
_PA_DEFINE_FUNC(snd_pcm_mmap_commit);
_PA_DEFINE_FUNC(snd_pcm_readi);
_PA_DEFINE_FUNC(snd_pcm_readn);
_PA_DEFINE_FUNC(snd_pcm_writei);
_PA_DEFINE_FUNC(snd_pcm_writen);
_PA_DEFINE_FUNC(snd_pcm_drain);
_PA_DEFINE_FUNC(snd_pcm_recover);
_PA_DEFINE_FUNC(snd_pcm_drop);
_PA_DEFINE_FUNC(snd_pcm_area_copy);
_PA_DEFINE_FUNC(snd_pcm_poll_descriptors);
_PA_DEFINE_FUNC(snd_pcm_poll_descriptors_count);
_PA_DEFINE_FUNC(snd_pcm_poll_descriptors_revents);
_PA_DEFINE_FUNC(snd_pcm_format_size);
_PA_DEFINE_FUNC(snd_pcm_link);
_PA_DEFINE_FUNC(snd_pcm_delay);

_PA_DEFINE_FUNC(snd_pcm_hw_params_sizeof);
_PA_DEFINE_FUNC(snd_pcm_hw_params_malloc);
_PA_DEFINE_FUNC(snd_pcm_hw_params_free);
_PA_DEFINE_FUNC(snd_pcm_hw_params_any);
_PA_DEFINE_FUNC(snd_pcm_hw_params_set_access);
_PA_DEFINE_FUNC(snd_pcm_hw_params_set_format);
_PA_DEFINE_FUNC(snd_pcm_hw_params_set_channels);
//_PA_DEFINE_FUNC(snd_pcm_hw_params_set_periods_near);
_PA_DEFINE_FUNC(snd_pcm_hw_params_set_rate_near); //!!!
_PA_DEFINE_FUNC(snd_pcm_hw_params_set_rate);
_PA_DEFINE_FUNC(snd_pcm_hw_params_set_rate_resample);
//_PA_DEFINE_FUNC(snd_pcm_hw_params_set_buffer_time_near);
_PA_DEFINE_FUNC(snd_pcm_hw_params_set_buffer_size);
_PA_DEFINE_FUNC(snd_pcm_hw_params_set_buffer_size_near); //!!!
_PA_DEFINE_FUNC(snd_pcm_hw_params_set_buffer_size_min);
//_PA_DEFINE_FUNC(snd_pcm_hw_params_set_period_time_near);
_PA_DEFINE_FUNC(snd_pcm_hw_params_set_period_size_near);
_PA_DEFINE_FUNC(snd_pcm_hw_params_set_periods_integer);
_PA_DEFINE_FUNC(snd_pcm_hw_params_set_periods_min);

_PA_DEFINE_FUNC(snd_pcm_hw_params_get_buffer_size);
//_PA_DEFINE_FUNC(snd_pcm_hw_params_get_period_size);
//_PA_DEFINE_FUNC(snd_pcm_hw_params_get_access);
//_PA_DEFINE_FUNC(snd_pcm_hw_params_get_periods);
//_PA_DEFINE_FUNC(snd_pcm_hw_params_get_rate);
_PA_DEFINE_FUNC(snd_pcm_hw_params_get_channels_min);
_PA_DEFINE_FUNC(snd_pcm_hw_params_get_channels_max);

_PA_DEFINE_FUNC(snd_pcm_hw_params_test_period_size);
_PA_DEFINE_FUNC(snd_pcm_hw_params_test_format);
_PA_DEFINE_FUNC(snd_pcm_hw_params_test_access);
_PA_DEFINE_FUNC(snd_pcm_hw_params_dump);
_PA_DEFINE_FUNC(snd_pcm_hw_params);

_PA_DEFINE_FUNC(snd_pcm_hw_params_get_periods_min);
_PA_DEFINE_FUNC(snd_pcm_hw_params_get_periods_max);
_PA_DEFINE_FUNC(snd_pcm_hw_params_set_period_size);
_PA_DEFINE_FUNC(snd_pcm_hw_params_get_period_size_min);
_PA_DEFINE_FUNC(snd_pcm_hw_params_get_period_size_max);
_PA_DEFINE_FUNC(snd_pcm_hw_params_get_buffer_size_max);
_PA_DEFINE_FUNC(snd_pcm_hw_params_get_rate_min);
_PA_DEFINE_FUNC(snd_pcm_hw_params_get_rate_max);
_PA_DEFINE_FUNC(snd_pcm_hw_params_get_rate_numden);

_PA_DEFINE_FUNC(snd_pcm_sw_params_sizeof);
_PA_DEFINE_FUNC(snd_pcm_sw_params_malloc);
_PA_DEFINE_FUNC(snd_pcm_sw_params_current);
_PA_DEFINE_FUNC(snd_pcm_sw_params_set_avail_min);
_PA_DEFINE_FUNC(snd_pcm_sw_params);
_PA_DEFINE_FUNC(snd_pcm_sw_params_free);
_PA_DEFINE_FUNC(snd_pcm_sw_params_set_start_threshold);
_PA_DEFINE_FUNC(snd_pcm_sw_params_set_stop_threshold);
_PA_DEFINE_FUNC(snd_pcm_sw_params_get_boundary);
_PA_DEFINE_FUNC(snd_pcm_sw_params_set_silence_threshold);
_PA_DEFINE_FUNC(snd_pcm_sw_params_set_silence_size);
_PA_DEFINE_FUNC(snd_pcm_sw_params_set_xfer_align);
_PA_DEFINE_FUNC(snd_pcm_sw_params_set_tstamp_mode);

_PA_DEFINE_FUNC(snd_pcm_info);
_PA_DEFINE_FUNC(snd_pcm_info_sizeof);
_PA_DEFINE_FUNC(snd_pcm_info_malloc);
_PA_DEFINE_FUNC(snd_pcm_info_free);
_PA_DEFINE_FUNC(snd_pcm_info_set_device);
_PA_DEFINE_FUNC(snd_pcm_info_set_subdevice);
_PA_DEFINE_FUNC(snd_pcm_info_set_stream);
_PA_DEFINE_FUNC(snd_pcm_info_get_name);
_PA_DEFINE_FUNC(snd_pcm_info_get_card);

_PA_DEFINE_FUNC(snd_ctl_pcm_next_device);
_PA_DEFINE_FUNC(snd_ctl_pcm_info);
_PA_DEFINE_FUNC(snd_ctl_open);
_PA_DEFINE_FUNC(snd_ctl_close);
_PA_DEFINE_FUNC(snd_ctl_card_info_malloc);
_PA_DEFINE_FUNC(snd_ctl_card_info_free);
_PA_DEFINE_FUNC(snd_ctl_card_info);
_PA_DEFINE_FUNC(snd_ctl_card_info_sizeof);
_PA_DEFINE_FUNC(snd_ctl_card_info_get_name);

_PA_DEFINE_FUNC(snd_config);
_PA_DEFINE_FUNC(snd_config_update);
_PA_DEFINE_FUNC(snd_config_search);
_PA_DEFINE_FUNC(snd_config_iterator_entry);
_PA_DEFINE_FUNC(snd_config_iterator_first);
_PA_DEFINE_FUNC(snd_config_iterator_end);
_PA_DEFINE_FUNC(snd_config_iterator_next);
_PA_DEFINE_FUNC(snd_config_get_string);
_PA_DEFINE_FUNC(snd_config_get_id);
_PA_DEFINE_FUNC(snd_config_update_free_global);

_PA_DEFINE_FUNC(snd_pcm_status);
_PA_DEFINE_FUNC(snd_pcm_status_sizeof);
_PA_DEFINE_FUNC(snd_pcm_status_get_tstamp);
_PA_DEFINE_FUNC(snd_pcm_status_get_state);
_PA_DEFINE_FUNC(snd_pcm_status_get_trigger_tstamp);
_PA_DEFINE_FUNC(snd_pcm_status_get_delay);

_PA_DEFINE_FUNC(snd_card_next);
_PA_DEFINE_FUNC(snd_asoundlib_version);
_PA_DEFINE_FUNC(snd_strerror);
_PA_DEFINE_FUNC(snd_output_stdio_attach);

#undef _PA_DEFINE_FUNC



static void load_alsa_functions(void *lib_handle)
{
#ifdef PA_ALSA_DYNAMIC
#define _PA_LOAD_FUNC(x) do {                \
        alsa_##x = dlsym( lib_handle, #x );   \
        if( alsa_##x == NULL ) {             \
            PA_DEBUG(( "%s: symbol [%s] not found in - %s, error: %s\n", __FUNCTION__, #x, g_AlsaLibName, dlerror() )); } \
        } while(0)
#else
#define _PA_LOAD_FUNC(x) do {  (void)lib_handle; alsa_##x = &x; } while (0)
#endif

    _PA_LOAD_FUNC(snd_pcm_open);
    _PA_LOAD_FUNC(snd_pcm_close);
    _PA_LOAD_FUNC(snd_pcm_nonblock);
    _PA_LOAD_FUNC(snd_pcm_frames_to_bytes);
    _PA_LOAD_FUNC(snd_pcm_prepare);
    _PA_LOAD_FUNC(snd_pcm_start);
    _PA_LOAD_FUNC(snd_pcm_resume);
    _PA_LOAD_FUNC(snd_pcm_wait);
    _PA_LOAD_FUNC(snd_pcm_state);
    _PA_LOAD_FUNC(snd_pcm_avail_update);
    _PA_LOAD_FUNC(snd_pcm_areas_silence);
    _PA_LOAD_FUNC(snd_pcm_mmap_begin);
    _PA_LOAD_FUNC(snd_pcm_mmap_commit);
    _PA_LOAD_FUNC(snd_pcm_readi);
    _PA_LOAD_FUNC(snd_pcm_readn);
    _PA_LOAD_FUNC(snd_pcm_writei);
    _PA_LOAD_FUNC(snd_pcm_writen);
    _PA_LOAD_FUNC(snd_pcm_drain);
    _PA_LOAD_FUNC(snd_pcm_recover);
    _PA_LOAD_FUNC(snd_pcm_drop);
    _PA_LOAD_FUNC(snd_pcm_area_copy);
    _PA_LOAD_FUNC(snd_pcm_poll_descriptors);
    _PA_LOAD_FUNC(snd_pcm_poll_descriptors_count);
    _PA_LOAD_FUNC(snd_pcm_poll_descriptors_revents);
    _PA_LOAD_FUNC(snd_pcm_format_size);
    _PA_LOAD_FUNC(snd_pcm_link);
    _PA_LOAD_FUNC(snd_pcm_delay);

    _PA_LOAD_FUNC(snd_pcm_hw_params_sizeof);
    _PA_LOAD_FUNC(snd_pcm_hw_params_malloc);
    _PA_LOAD_FUNC(snd_pcm_hw_params_free);
    _PA_LOAD_FUNC(snd_pcm_hw_params_any);
    _PA_LOAD_FUNC(snd_pcm_hw_params_set_access);
    _PA_LOAD_FUNC(snd_pcm_hw_params_set_format);
    _PA_LOAD_FUNC(snd_pcm_hw_params_set_channels);
//    _PA_LOAD_FUNC(snd_pcm_hw_params_set_periods_near);
    _PA_LOAD_FUNC(snd_pcm_hw_params_set_rate_near);
    _PA_LOAD_FUNC(snd_pcm_hw_params_set_rate);
    _PA_LOAD_FUNC(snd_pcm_hw_params_set_rate_resample);
//    _PA_LOAD_FUNC(snd_pcm_hw_params_set_buffer_time_near);
    _PA_LOAD_FUNC(snd_pcm_hw_params_set_buffer_size);
    _PA_LOAD_FUNC(snd_pcm_hw_params_set_buffer_size_near);
    _PA_LOAD_FUNC(snd_pcm_hw_params_set_buffer_size_min);
//    _PA_LOAD_FUNC(snd_pcm_hw_params_set_period_time_near);
    _PA_LOAD_FUNC(snd_pcm_hw_params_set_period_size_near);
    _PA_LOAD_FUNC(snd_pcm_hw_params_set_periods_integer);
    _PA_LOAD_FUNC(snd_pcm_hw_params_set_periods_min);

    _PA_LOAD_FUNC(snd_pcm_hw_params_get_buffer_size);
//    _PA_LOAD_FUNC(snd_pcm_hw_params_get_period_size);
//    _PA_LOAD_FUNC(snd_pcm_hw_params_get_access);
//    _PA_LOAD_FUNC(snd_pcm_hw_params_get_periods);
//    _PA_LOAD_FUNC(snd_pcm_hw_params_get_rate);
    _PA_LOAD_FUNC(snd_pcm_hw_params_get_channels_min);
    _PA_LOAD_FUNC(snd_pcm_hw_params_get_channels_max);

    _PA_LOAD_FUNC(snd_pcm_hw_params_test_period_size);
    _PA_LOAD_FUNC(snd_pcm_hw_params_test_format);
    _PA_LOAD_FUNC(snd_pcm_hw_params_test_access);
    _PA_LOAD_FUNC(snd_pcm_hw_params_dump);
    _PA_LOAD_FUNC(snd_pcm_hw_params);

    _PA_LOAD_FUNC(snd_pcm_hw_params_get_periods_min);
    _PA_LOAD_FUNC(snd_pcm_hw_params_get_periods_max);
    _PA_LOAD_FUNC(snd_pcm_hw_params_set_period_size);
    _PA_LOAD_FUNC(snd_pcm_hw_params_get_period_size_min);
    _PA_LOAD_FUNC(snd_pcm_hw_params_get_period_size_max);
    _PA_LOAD_FUNC(snd_pcm_hw_params_get_buffer_size_max);
    _PA_LOAD_FUNC(snd_pcm_hw_params_get_rate_min);
    _PA_LOAD_FUNC(snd_pcm_hw_params_get_rate_max);
    _PA_LOAD_FUNC(snd_pcm_hw_params_get_rate_numden);

    _PA_LOAD_FUNC(snd_pcm_sw_params_sizeof);
    _PA_LOAD_FUNC(snd_pcm_sw_params_malloc);
    _PA_LOAD_FUNC(snd_pcm_sw_params_current);
    _PA_LOAD_FUNC(snd_pcm_sw_params_set_avail_min);
    _PA_LOAD_FUNC(snd_pcm_sw_params);
    _PA_LOAD_FUNC(snd_pcm_sw_params_free);
    _PA_LOAD_FUNC(snd_pcm_sw_params_set_start_threshold);
    _PA_LOAD_FUNC(snd_pcm_sw_params_set_stop_threshold);
    _PA_LOAD_FUNC(snd_pcm_sw_params_get_boundary);
    _PA_LOAD_FUNC(snd_pcm_sw_params_set_silence_threshold);
    _PA_LOAD_FUNC(snd_pcm_sw_params_set_silence_size);
    _PA_LOAD_FUNC(snd_pcm_sw_params_set_xfer_align);
    _PA_LOAD_FUNC(snd_pcm_sw_params_set_tstamp_mode);

    _PA_LOAD_FUNC(snd_pcm_info);
    _PA_LOAD_FUNC(snd_pcm_info_sizeof);
    _PA_LOAD_FUNC(snd_pcm_info_malloc);
    _PA_LOAD_FUNC(snd_pcm_info_free);
    _PA_LOAD_FUNC(snd_pcm_info_set_device);
    _PA_LOAD_FUNC(snd_pcm_info_set_subdevice);
    _PA_LOAD_FUNC(snd_pcm_info_set_stream);
    _PA_LOAD_FUNC(snd_pcm_info_get_name);
    _PA_LOAD_FUNC(snd_pcm_info_get_card);

    _PA_LOAD_FUNC(snd_ctl_pcm_next_device);
    _PA_LOAD_FUNC(snd_ctl_pcm_info);
    _PA_LOAD_FUNC(snd_ctl_open);
    _PA_LOAD_FUNC(snd_ctl_close);
    _PA_LOAD_FUNC(snd_ctl_card_info_malloc);
    _PA_LOAD_FUNC(snd_ctl_card_info_free);
    _PA_LOAD_FUNC(snd_ctl_card_info);
    _PA_LOAD_FUNC(snd_ctl_card_info_sizeof);
    _PA_LOAD_FUNC(snd_ctl_card_info_get_name);

    _PA_LOAD_FUNC(snd_config);
    _PA_LOAD_FUNC(snd_config_update);
    _PA_LOAD_FUNC(snd_config_search);
    _PA_LOAD_FUNC(snd_config_iterator_entry);
    _PA_LOAD_FUNC(snd_config_iterator_first);
    _PA_LOAD_FUNC(snd_config_iterator_end);
    _PA_LOAD_FUNC(snd_config_iterator_next);
    _PA_LOAD_FUNC(snd_config_get_string);
    _PA_LOAD_FUNC(snd_config_get_id);
    _PA_LOAD_FUNC(snd_config_update_free_global);

    _PA_LOAD_FUNC(snd_pcm_status);
    _PA_LOAD_FUNC(snd_pcm_status_sizeof);
    _PA_LOAD_FUNC(snd_pcm_status_get_tstamp);
    _PA_LOAD_FUNC(snd_pcm_status_get_state);
    _PA_LOAD_FUNC(snd_pcm_status_get_trigger_tstamp);
    _PA_LOAD_FUNC(snd_pcm_status_get_delay);

    _PA_LOAD_FUNC(snd_card_next);
    _PA_LOAD_FUNC(snd_asoundlib_version);
    _PA_LOAD_FUNC(snd_strerror);
    _PA_LOAD_FUNC(snd_output_stdio_attach);
#undef _PA_LOAD_FUNC
}


#ifdef PA_ALSA_DYNAMIC

#define _PA_LOCAL_IMPL(x) __pa_local_##x

int _PA_LOCAL_IMPL(snd_pcm_hw_params_set_rate_near) (snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
{
    int ret;

    if(( ret = alsa_snd_pcm_hw_params_set_rate(pcm, params, (*val), (*dir)) ) < 0 )
        return ret;

    return 0;
}

int _PA_LOCAL_IMPL(snd_pcm_hw_params_set_buffer_size_near) (snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val)
{
    int ret;

    if(( ret = alsa_snd_pcm_hw_params_set_buffer_size(pcm, params, (*val)) ) < 0 )
        return ret;

    return 0;
}

int _PA_LOCAL_IMPL(snd_pcm_hw_params_set_period_size_near) (snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val, int *dir)
{
    int ret;

    if(( ret = alsa_snd_pcm_hw_params_set_period_size(pcm, params, (*val), (*dir)) ) < 0 )
        return ret;

    return 0;
}

int _PA_LOCAL_IMPL(snd_pcm_hw_params_get_channels_min) (const snd_pcm_hw_params_t *params, unsigned int *val)
{
    (*val) = 1;
    return 0;
}

int _PA_LOCAL_IMPL(snd_pcm_hw_params_get_channels_max) (const snd_pcm_hw_params_t *params, unsigned int *val)
{
    (*val) = 2;
    return 0;
}

int _PA_LOCAL_IMPL(snd_pcm_hw_params_get_periods_min) (const snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
{
    (*val) = 2;
    return 0;
}

int _PA_LOCAL_IMPL(snd_pcm_hw_params_get_periods_max) (const snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
{
    (*val) = 8;
    return 0;
}

int _PA_LOCAL_IMPL(snd_pcm_hw_params_get_period_size_min) (const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *frames, int *dir)
{
    (*frames) = 64;
    return 0;
}

int _PA_LOCAL_IMPL(snd_pcm_hw_params_get_period_size_max) (const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *frames, int *dir)
{
    (*frames) = 512;
    return 0;
}

int _PA_LOCAL_IMPL(snd_pcm_hw_params_get_buffer_size_max) (const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val)
{
    int ret;
    int dir                = 0;
    snd_pcm_uframes_t pmax = 0;
    unsigned int      pcnt = 0;

    if(( ret = _PA_LOCAL_IMPL(snd_pcm_hw_params_get_period_size_max)(params, &pmax, &dir) ) < 0 )
        return ret;
    if(( ret = _PA_LOCAL_IMPL(snd_pcm_hw_params_get_periods_max)(params, &pcnt, &dir) ) < 0 )
        return ret;

    (*val) = pmax * pcnt;
    return 0;
}

int _PA_LOCAL_IMPL(snd_pcm_hw_params_get_rate_min) (const snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
{
    (*val) = 44100;
    return 0;
}

int _PA_LOCAL_IMPL(snd_pcm_hw_params_get_rate_max) (const snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
{
    (*val) = 44100;
    return 0;
}


static void validate_load_replacement(void)
{
#define _PA_VALIDATE_LOAD_REPLACEMENT(x)\
    do {\
        if( alsa_##x == NULL )\
        {\
            alsa_##x = &(_PA_LOCAL_IMPL(x));\
            PA_DEBUG(( "%s: replacing [%s] with local implementation\n", __FUNCTION__, #x ));\
        }\
    } while (0)

    _PA_VALIDATE_LOAD_REPLACEMENT(snd_pcm_hw_params_set_rate_near);
    _PA_VALIDATE_LOAD_REPLACEMENT(snd_pcm_hw_params_set_buffer_size_near);
    _PA_VALIDATE_LOAD_REPLACEMENT(snd_pcm_hw_params_set_period_size_near);
    _PA_VALIDATE_LOAD_REPLACEMENT(snd_pcm_hw_params_get_channels_min);
    _PA_VALIDATE_LOAD_REPLACEMENT(snd_pcm_hw_params_get_channels_max);
    _PA_VALIDATE_LOAD_REPLACEMENT(snd_pcm_hw_params_get_periods_min);
    _PA_VALIDATE_LOAD_REPLACEMENT(snd_pcm_hw_params_get_periods_max);
    _PA_VALIDATE_LOAD_REPLACEMENT(snd_pcm_hw_params_get_period_size_min);
    _PA_VALIDATE_LOAD_REPLACEMENT(snd_pcm_hw_params_get_period_size_max);
    _PA_VALIDATE_LOAD_REPLACEMENT(snd_pcm_hw_params_get_buffer_size_max);
    _PA_VALIDATE_LOAD_REPLACEMENT(snd_pcm_hw_params_get_rate_min);
    _PA_VALIDATE_LOAD_REPLACEMENT(snd_pcm_hw_params_get_rate_max);
#undef _PA_VALIDATE_LOAD_REPLACEMENT
}

#endif // PA_ALSA_DYNAMIC


void AlsaLoad_SetLibraryPathName( const char *pathName )
{
#ifdef PA_ALSA_DYNAMIC
    g_AlsaLibName = pathName;
#else
    (void)pathName;
#endif
}


/* Trying to load Alsa library dynamically if 'PA_ALSA_DYNAMIC' is defined, otherwise
   will link during compilation.
*/
int AlsaLoad_OpenLibrary( void )
{
#ifdef PA_ALSA_DYNAMIC
    PA_DEBUG(( "%s: loading ALSA library file - %s\n", __FUNCTION__, g_AlsaLibName ));
    dlerror();
    g_AlsaLib = dlopen( g_AlsaLibName, ( RTLD_NOW | RTLD_GLOBAL ) );
    if( g_AlsaLib == NULL )
    {
        PA_DEBUG(( "%s: failed dlopen() ALSA library file - %s, error: %s\n", __FUNCTION__, g_AlsaLibName, dlerror() ));
        return 0;
    }
    PA_DEBUG(( "%s: loading ALSA API\n", __FUNCTION__ ));
#endif

    load_alsa_functions( g_AlsaLib );

#ifdef PA_ALSA_DYNAMIC
    PA_DEBUG(( "%s: loaded ALSA API - ok\n", __FUNCTION__ ));
    validate_load_replacement();
#endif

    return 1;
}


/* Close handle to Alsa library. */
void AlsaLoad_CloseLibrary( void )
{
#ifdef PA_ALSA_DYNAMIC
    dlclose(g_AlsaLib);
    g_AlsaLib = NULL;
#endif
}

