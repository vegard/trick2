#ifndef ALSA_OUTPUT_PLUGIN_HH
#define ALSA_OUTPUT_PLUGIN_HH

extern "C" {
#include <alsa/asoundlib.h>
}

#include "plugin.hh"

class alsa_output_plugin:
	public plugin
{
public:
	alsa_output_plugin();
	~alsa_output_plugin();

private:
	int init();

public:
	void activate();
	void deactivate();

	void connect(unsigned int port, float* buffer);
	void disconnect(unsigned int port);

	void run(unsigned int sample_count);

public:
	snd_pcm_t* playback_handle;

	short* _frames[2];
};

alsa_output_plugin::alsa_output_plugin()
{
	int err = init();
	if (err < 0) {
		fprintf(stderr, "playback init failed: %s\n",
			snd_strerror(err));
		exit(EXIT_FAILURE);
	}

	_frames[0] = new short[buffer_size];
	_frames[1] = new short[buffer_size];

	_ports = new float*[2];
}

alsa_output_plugin::~alsa_output_plugin()
{
	snd_pcm_close(playback_handle);

	delete[] _ports;

	delete[] _frames[0];
	delete[] _frames[1];
}

int
alsa_output_plugin::init()
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

void
alsa_output_plugin::activate()
{
	snd_pcm_start(playback_handle);
}

void
alsa_output_plugin::deactivate()
{
}

void
alsa_output_plugin::connect(unsigned int port, float* buffer)
{
	assert(port < 2);

	plugin::connect(port, buffer);
}

void
alsa_output_plugin::disconnect(unsigned int port)
{
	assert(port < 2);

	plugin::disconnect(port);
}

void
alsa_output_plugin::run(unsigned int n)
{
	for (unsigned int i = 0; i < n; ++i) {
		_frames[0][i] = 32 * 1024 * _ports[0][i];
		_frames[1][i] = 32 * 1024 * _ports[1][i];
	}

	unsigned int i = 0;
	while (n > 0) {
		void* bufs[] = {
			(void*) (_frames[0] + i),
			(void*) (_frames[1] + i),
		};

		int err = snd_pcm_writen(playback_handle, bufs, n);
		if (err < 0) {
			printf("write error: %s\n", snd_strerror(err));

			if (err == -EPIPE) {
				snd_pcm_prepare(playback_handle);
				continue;
			} else {
				exit(EXIT_FAILURE);
			}
		}

		/* Cast is OK, because we checked for negative numbers above */
		assert((unsigned int) err <= n);

		n -= err;
		i += err;
	}
}

#endif
