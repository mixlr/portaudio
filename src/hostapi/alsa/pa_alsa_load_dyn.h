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

#ifndef PA_ALSA_LOAD_DYN_H
#define PA_ALSA_LOAD_DYN_H

/* #define PA_ALSA_DYNAMIC  Adding this define here or in project build enables dynamic loading */

/* Redefine 'PA_ALSA_PATHNAME' to a different Alsa library name if desired. */
#ifndef PA_ALSA_PATHNAME
    #define PA_ALSA_PATHNAME "libasound.so"
#endif

/* Alloca helper. */
#define __alsa_snd_alloca(ptr,type) do { size_t __alsa_alloca_size = alsa_##type##_sizeof(); (*ptr) = (type##_t *) \
                                         alloca(__alsa_alloca_size); memset(*ptr, 0, __alsa_alloca_size); } while (0)

/* Declare Alsa function types and pointers to these functions. */
#define _PA_DECLARE_FUNC(x)  typedef typeof(x) x##_ft;  x##_ft *alsa_##x;

_PA_DECLARE_FUNC(snd_pcm_open);
_PA_DECLARE_FUNC(snd_pcm_close);
_PA_DECLARE_FUNC(snd_pcm_nonblock);
_PA_DECLARE_FUNC(snd_pcm_frames_to_bytes);
_PA_DECLARE_FUNC(snd_pcm_prepare);
_PA_DECLARE_FUNC(snd_pcm_start);
_PA_DECLARE_FUNC(snd_pcm_resume);
_PA_DECLARE_FUNC(snd_pcm_wait);
_PA_DECLARE_FUNC(snd_pcm_state);
_PA_DECLARE_FUNC(snd_pcm_avail_update);
_PA_DECLARE_FUNC(snd_pcm_areas_silence);
_PA_DECLARE_FUNC(snd_pcm_mmap_begin);
_PA_DECLARE_FUNC(snd_pcm_mmap_commit);
_PA_DECLARE_FUNC(snd_pcm_readi);
_PA_DECLARE_FUNC(snd_pcm_readn);
_PA_DECLARE_FUNC(snd_pcm_writei);
_PA_DECLARE_FUNC(snd_pcm_writen);
_PA_DECLARE_FUNC(snd_pcm_drain);
_PA_DECLARE_FUNC(snd_pcm_recover);
_PA_DECLARE_FUNC(snd_pcm_drop);
_PA_DECLARE_FUNC(snd_pcm_area_copy);
_PA_DECLARE_FUNC(snd_pcm_poll_descriptors);
_PA_DECLARE_FUNC(snd_pcm_poll_descriptors_count);
_PA_DECLARE_FUNC(snd_pcm_poll_descriptors_revents);
_PA_DECLARE_FUNC(snd_pcm_format_size);
_PA_DECLARE_FUNC(snd_pcm_link);
_PA_DECLARE_FUNC(snd_pcm_delay);

_PA_DECLARE_FUNC(snd_pcm_hw_params_sizeof);
_PA_DECLARE_FUNC(snd_pcm_hw_params_malloc);
_PA_DECLARE_FUNC(snd_pcm_hw_params_free);
_PA_DECLARE_FUNC(snd_pcm_hw_params_any);
_PA_DECLARE_FUNC(snd_pcm_hw_params_set_access);
_PA_DECLARE_FUNC(snd_pcm_hw_params_set_format);
_PA_DECLARE_FUNC(snd_pcm_hw_params_set_channels);
//_PA_DECLARE_FUNC(snd_pcm_hw_params_set_periods_near);
_PA_DECLARE_FUNC(snd_pcm_hw_params_set_rate_near); //!!!
_PA_DECLARE_FUNC(snd_pcm_hw_params_set_rate);
_PA_DECLARE_FUNC(snd_pcm_hw_params_set_rate_resample);
//_PA_DECLARE_FUNC(snd_pcm_hw_params_set_buffer_time_near);
_PA_DECLARE_FUNC(snd_pcm_hw_params_set_buffer_size);
_PA_DECLARE_FUNC(snd_pcm_hw_params_set_buffer_size_near); //!!!
_PA_DECLARE_FUNC(snd_pcm_hw_params_set_buffer_size_min);
//_PA_DECLARE_FUNC(snd_pcm_hw_params_set_period_time_near);
_PA_DECLARE_FUNC(snd_pcm_hw_params_set_period_size_near);
_PA_DECLARE_FUNC(snd_pcm_hw_params_set_periods_integer);
_PA_DECLARE_FUNC(snd_pcm_hw_params_set_periods_min);

_PA_DECLARE_FUNC(snd_pcm_hw_params_get_buffer_size);
//_PA_DECLARE_FUNC(snd_pcm_hw_params_get_period_size);
//_PA_DECLARE_FUNC(snd_pcm_hw_params_get_access);
//_PA_DECLARE_FUNC(snd_pcm_hw_params_get_periods);
//_PA_DECLARE_FUNC(snd_pcm_hw_params_get_rate);
_PA_DECLARE_FUNC(snd_pcm_hw_params_get_channels_min);
_PA_DECLARE_FUNC(snd_pcm_hw_params_get_channels_max);

_PA_DECLARE_FUNC(snd_pcm_hw_params_test_period_size);
_PA_DECLARE_FUNC(snd_pcm_hw_params_test_format);
_PA_DECLARE_FUNC(snd_pcm_hw_params_test_access);
_PA_DECLARE_FUNC(snd_pcm_hw_params_dump);
_PA_DECLARE_FUNC(snd_pcm_hw_params);

_PA_DECLARE_FUNC(snd_pcm_hw_params_get_periods_min);
_PA_DECLARE_FUNC(snd_pcm_hw_params_get_periods_max);
_PA_DECLARE_FUNC(snd_pcm_hw_params_set_period_size);
_PA_DECLARE_FUNC(snd_pcm_hw_params_get_period_size_min);
_PA_DECLARE_FUNC(snd_pcm_hw_params_get_period_size_max);
_PA_DECLARE_FUNC(snd_pcm_hw_params_get_buffer_size_max);
_PA_DECLARE_FUNC(snd_pcm_hw_params_get_rate_min);
_PA_DECLARE_FUNC(snd_pcm_hw_params_get_rate_max);
_PA_DECLARE_FUNC(snd_pcm_hw_params_get_rate_numden);
#define alsa_snd_pcm_hw_params_alloca(ptr) __alsa_snd_alloca(ptr, snd_pcm_hw_params)

_PA_DECLARE_FUNC(snd_pcm_sw_params_sizeof);
_PA_DECLARE_FUNC(snd_pcm_sw_params_malloc);
_PA_DECLARE_FUNC(snd_pcm_sw_params_current);
_PA_DECLARE_FUNC(snd_pcm_sw_params_set_avail_min);
_PA_DECLARE_FUNC(snd_pcm_sw_params);
_PA_DECLARE_FUNC(snd_pcm_sw_params_free);
_PA_DECLARE_FUNC(snd_pcm_sw_params_set_start_threshold);
_PA_DECLARE_FUNC(snd_pcm_sw_params_set_stop_threshold);
_PA_DECLARE_FUNC(snd_pcm_sw_params_get_boundary);
_PA_DECLARE_FUNC(snd_pcm_sw_params_set_silence_threshold);
_PA_DECLARE_FUNC(snd_pcm_sw_params_set_silence_size);
_PA_DECLARE_FUNC(snd_pcm_sw_params_set_xfer_align);
_PA_DECLARE_FUNC(snd_pcm_sw_params_set_tstamp_mode);
#define alsa_snd_pcm_sw_params_alloca(ptr) __alsa_snd_alloca(ptr, snd_pcm_sw_params)

_PA_DECLARE_FUNC(snd_pcm_info);
_PA_DECLARE_FUNC(snd_pcm_info_sizeof);
_PA_DECLARE_FUNC(snd_pcm_info_malloc);
_PA_DECLARE_FUNC(snd_pcm_info_free);
_PA_DECLARE_FUNC(snd_pcm_info_set_device);
_PA_DECLARE_FUNC(snd_pcm_info_set_subdevice);
_PA_DECLARE_FUNC(snd_pcm_info_set_stream);
_PA_DECLARE_FUNC(snd_pcm_info_get_name);
_PA_DECLARE_FUNC(snd_pcm_info_get_card);
_PA_DECLARE_FUNC(snd_pcm_info_get_subdevices_count);
_PA_DECLARE_FUNC(snd_pcm_info_get_subdevice_name);
#define alsa_snd_pcm_info_alloca(ptr) __alsa_snd_alloca(ptr, snd_pcm_info)

_PA_DECLARE_FUNC(snd_ctl_pcm_next_device);
_PA_DECLARE_FUNC(snd_ctl_pcm_info);
_PA_DECLARE_FUNC(snd_ctl_open);
_PA_DECLARE_FUNC(snd_ctl_close);
_PA_DECLARE_FUNC(snd_ctl_card_info_malloc);
_PA_DECLARE_FUNC(snd_ctl_card_info_free);
_PA_DECLARE_FUNC(snd_ctl_card_info);
_PA_DECLARE_FUNC(snd_ctl_card_info_sizeof);
_PA_DECLARE_FUNC(snd_ctl_card_info_get_name);
#define alsa_snd_ctl_card_info_alloca(ptr) __alsa_snd_alloca(ptr, snd_ctl_card_info)

_PA_DECLARE_FUNC(snd_config);
_PA_DECLARE_FUNC(snd_config_update);
_PA_DECLARE_FUNC(snd_config_search);
_PA_DECLARE_FUNC(snd_config_iterator_entry);
_PA_DECLARE_FUNC(snd_config_iterator_first);
_PA_DECLARE_FUNC(snd_config_iterator_end);
_PA_DECLARE_FUNC(snd_config_iterator_next);
_PA_DECLARE_FUNC(snd_config_get_string);
_PA_DECLARE_FUNC(snd_config_get_id);
_PA_DECLARE_FUNC(snd_config_update_free_global);

_PA_DECLARE_FUNC(snd_pcm_status);
_PA_DECLARE_FUNC(snd_pcm_status_sizeof);
_PA_DECLARE_FUNC(snd_pcm_status_get_tstamp);
_PA_DECLARE_FUNC(snd_pcm_status_get_state);
_PA_DECLARE_FUNC(snd_pcm_status_get_trigger_tstamp);
_PA_DECLARE_FUNC(snd_pcm_status_get_delay);
#define alsa_snd_pcm_status_alloca(ptr) __alsa_snd_alloca(ptr, snd_pcm_status)

_PA_DECLARE_FUNC(snd_card_next);
_PA_DECLARE_FUNC(snd_asoundlib_version);
_PA_DECLARE_FUNC(snd_strerror);
_PA_DECLARE_FUNC(snd_output_stdio_attach);

#define alsa_snd_config_for_each(pos, next, node)\
    for (pos = alsa_snd_config_iterator_first(node),\
         next = alsa_snd_config_iterator_next(pos);\
         pos != alsa_snd_config_iterator_end(node); pos = next, next = alsa_snd_config_iterator_next(pos))

#undef _PA_DECLARE_FUNC


void AlsaLoad_SetLibraryPathName( const char *pathName );
int AlsaLoad_OpenLibrary();
void AlsaLoad_CloseLibrary();

#endif /* PA_ALSA_LOAD_DYN_H */
