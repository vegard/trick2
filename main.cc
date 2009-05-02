#include <map>
#include <set>

extern "C" {
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <alsa/asoundlib.h>
#include <ladspa.h>
#include <math.h>
}

static const unsigned long sample_rate = 44100;
static const unsigned long buffer_size = 16384;

static LADSPA_Data silence_buffer[buffer_size];

#include "edge.hh"
#include "graph.hh"
#include "plugin.hh"
#include "sequencer.hh"
#include "simple_sequencer.hh"

static snd_pcm_t* playback_handle;

static int
playback_init()
{
	int err;

	err = snd_pcm_open(&playback_handle,
		"plughw:0,0", SND_PCM_STREAM_PLAYBACK, 0);
	if (err < 0)
		return err;

	snd_pcm_hw_params_t *hw_params;
	err = snd_pcm_hw_params_malloc(&hw_params);
	if (err < 0)
		return err;

	err = snd_pcm_hw_params_any(playback_handle, hw_params);
	if (err < 0)
		return err;

	err = snd_pcm_hw_params_set_access(playback_handle,
		hw_params, SND_PCM_ACCESS_RW_NONINTERLEAVED);
	if (err < 0)
		return err;

	err = snd_pcm_hw_params_set_format(playback_handle,
		hw_params, SND_PCM_FORMAT_S16);
	if (err < 0)
		return err;

	unsigned int exact_rate = sample_rate;
	err = snd_pcm_hw_params_set_rate_near(playback_handle,
		hw_params, &exact_rate, 0);
	if (err < 0)
		return err;

	err = snd_pcm_hw_params_set_channels(playback_handle,
		hw_params, 2);
	if (err < 0)
		return err;

	err = snd_pcm_hw_params(playback_handle, hw_params);
	if (err < 0)
		return err;

	snd_pcm_hw_params_free(hw_params);

	err = snd_pcm_prepare(playback_handle);
	if (err < 0)
		return err;

	return 0;
}

static void
playback_destroy()
{
	snd_pcm_close(playback_handle);
}

static struct note song[] = {
	{60, 2},
	{58, 2},
	{60, 8},
	{58, 2},
	{56, 2},
	{55, 2},
	{53, 2},
	{52, 8},
	{53, 8},

	{0, 0},
};

int
main(int argc, char* argv[])
{
	graph* g = new graph();

	//plugin* organ = new plugin("/usr/lib64/ladspa/cmt.so", "organ");
	plugin* organ = new plugin("/home/vegard/programming/cmt/plugins/cmt.so", "organ");
	organ->activate();

	organ->_ports[1][0] = 1;	/* Gate */
	organ->_ports[2][0] = 1.0;	/* Velocity */
	organ->_ports[3][0] = 0;	/* Frequency */
	organ->_ports[4][0] = 0.9;	/* Brass */
	organ->_ports[5][0] = 0.5;	/* Reed */
	organ->_ports[6][0] = 0.4;	/* Flute */
	organ->_ports[7][0] = 0.3;	/* 16th Harmonic */
	organ->_ports[8][0] = 0.3;	/* 8th Harmonic */
	organ->_ports[9][0] = 0.3;	/* 5 1/3rd Harmonic */
	organ->_ports[10][0] = 0.3;	/* 4th Harmonic */
	organ->_ports[11][0] = 0.3;	/* 2 2/3rd Harmonic */
	organ->_ports[12][0] = 0.3;	/* 2nd Harmonic */
	organ->_ports[13][0] = 0.01;	/* Attack Lo*/
	organ->_ports[14][0] = 0.8;	/* Decay Lo */
	organ->_ports[15][0] = 1;	/* Sustain Lo */
	organ->_ports[16][0] = 1;	/* Release Lo */
	organ->_ports[17][0] = 0;	/* Attack Hi */
	organ->_ports[18][0] = 1;	/* Decay Hi */
	organ->_ports[19][0] = 1;	/* Sustain Hi */
	organ->_ports[20][0] = 1;	/* Release Hi */

	g->add(organ);

	plugin* reverb = new plugin("/usr/lib64/ladspa/plate_1423.so", "plate");
	reverb->activate();

	reverb->_ports[0][0] = 6.00;	/* Reverb time */
	reverb->_ports[1][0] = 0.07;	/* Damping */
	reverb->_ports[2][0] = 0.50;	/* Dry/wet */

	g->add(reverb);

	g->connect(organ, 0, reverb, 3);

	sequencer* seq = new simple_sequencer(60,
		song, sizeof(song) / sizeof(*song),
		organ->_ports[3]);
	organ->_seqs.insert(seq);

	int err;
	err = playback_init();
	if (err < 0) {
		fprintf(stderr, "playback init failed (%s)\n",
			snd_strerror(err));
		exit(EXIT_FAILURE);
	}

	short* frames[2];
	frames[0] = new short[buffer_size];
	frames[1] = new short[buffer_size];

	snd_pcm_start(playback_handle);

	printf("running...\n");
	while (1) {
		unsigned long n = buffer_size;
		g->run(n);
		for (unsigned int i = 0; i < n; ++i) {
			frames[0][i] = 32 * 1024 * reverb->_ports[4][i];
			frames[1][i] = 32 * 1024 * reverb->_ports[5][i];
		}

		unsigned int i = 0;
		while (n > 0) {
			void* bufs[] = {
				(void*) (frames[0] + i),
				(void*) (frames[1] + i),
			};

			err = snd_pcm_writen(playback_handle, bufs, n);
			if (err < 0) {
				printf("write error: %s\n", snd_strerror(err));

				if (err == -EPIPE) {
					snd_pcm_prepare(playback_handle);
					continue;
				} else {
					exit(EXIT_FAILURE);
				}
			}

			n -= err;
			i += err;
		}
	}

	playback_destroy();

	delete seq;

	reverb->deactivate();
	delete reverb;

	organ->deactivate();
	delete organ;

	delete g;

	return EXIT_SUCCESS;
}
