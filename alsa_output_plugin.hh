#ifndef ALSA_OUTPUT_PLUGIN_HH
#define ALSA_OUTPUT_PLUGIN_HH

extern "C" {
#include <alsa/asoundlib.h>
#include <pthread.h>
}

#include "plugin.hh"

class alsa_output_plugin:
	public plugin
{
public:
	alsa_output_plugin(const char* device);
	~alsa_output_plugin();

private:
	int init(const char* device);

public:
	void activate();
	void deactivate();

	void connect(unsigned int port, float* buffer);
	void disconnect(unsigned int port);

	void run(unsigned int sample_count);

private:
	static void* write_thread(void* plugin);

public:
	pthread_cond_t _write_cond;
	pthread_mutex_t _write_mutex;
	bool _write_exit;
	bool _write_ready;
	pthread_t _write_thread;

	snd_pcm_t* _playback_handle;

	short* _frames[2];
};

alsa_output_plugin::alsa_output_plugin(const char* device)
{
	int err = init(device);
	if (err < 0) {
		fprintf(stderr, "playback init failed: %s\n",
			snd_strerror(err));
		exit(EXIT_FAILURE);
	}

	_frames[0] = new short[buffer_size];
	_frames[1] = new short[buffer_size];

	_ports = new float*[2];

	pthread_cond_init(&_write_cond, NULL);
	pthread_mutex_init(&_write_mutex, NULL);
}

alsa_output_plugin::~alsa_output_plugin()
{
	pthread_cond_destroy(&_write_cond);
	pthread_mutex_destroy(&_write_mutex);

	snd_pcm_close(_playback_handle);

	delete[] _ports;

	delete[] _frames[0];
	delete[] _frames[1];
}

int
alsa_output_plugin::init(const char* device)
{
	int err;

	err = snd_pcm_open(&_playback_handle,
		device, SND_PCM_STREAM_PLAYBACK, 0);
	if (err < 0)
		return err;

	snd_pcm_hw_params_t *hw_params;
	err = snd_pcm_hw_params_malloc(&hw_params);
	if (err < 0)
		return err;

	err = snd_pcm_hw_params_any(_playback_handle, hw_params);
	if (err < 0)
		return err;

	err = snd_pcm_hw_params_set_access(_playback_handle,
		hw_params, SND_PCM_ACCESS_RW_NONINTERLEAVED);
	if (err < 0)
		return err;

	err = snd_pcm_hw_params_set_format(_playback_handle,
		hw_params, SND_PCM_FORMAT_S16);
	if (err < 0)
		return err;

	unsigned int exact_rate = sample_rate;
	err = snd_pcm_hw_params_set_rate_near(_playback_handle,
		hw_params, &exact_rate, 0);
	if (err < 0)
		return err;

	err = snd_pcm_hw_params_set_channels(_playback_handle,
		hw_params, 2);
	if (err < 0)
		return err;

	err = snd_pcm_hw_params(_playback_handle, hw_params);
	if (err < 0)
		return err;

	snd_pcm_hw_params_free(hw_params);

	err = snd_pcm_prepare(_playback_handle);
	if (err < 0)
		return err;

	return 0;
}

void
alsa_output_plugin::activate()
{
	snd_pcm_start(_playback_handle);

	/* We need the memory barriers, probably. Just in case. */
	pthread_mutex_lock(&_write_mutex);
	_write_exit = false;
	_write_ready = false;
	pthread_mutex_unlock(&_write_mutex);

	pthread_create(&_write_thread, NULL, &write_thread, (void*) this);
}

void
alsa_output_plugin::deactivate()
{
	pthread_mutex_lock(&_write_mutex);
	_write_exit = true;
	pthread_cond_broadcast(&_write_cond);
	pthread_mutex_unlock(&_write_mutex);

	pthread_join(_write_thread, NULL);
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
	assert(n == buffer_size);

	/* Wait until the previous buffer has been flushed */
	pthread_mutex_lock(&_write_mutex);
	while (_write_ready) {
		int err = pthread_cond_wait(&_write_cond, &_write_mutex);
		assert(err == 0);
	}
	pthread_mutex_unlock(&_write_mutex);

	/* Copy the new buffer */
	for (unsigned int i = 0; i < n; ++i) {
		_frames[0][i] = 32 * 1024 * _ports[0][i];
		_frames[1][i] = 32 * 1024 * _ports[1][i];
	}

	/* Wake up the writer thread */
	pthread_mutex_lock(&_write_mutex);
	assert(!_write_ready);
	_write_ready = true;
	pthread_cond_broadcast(&_write_cond);
	pthread_mutex_unlock(&_write_mutex);
}

void*
alsa_output_plugin::write_thread(void* arg)
{
	alsa_output_plugin* p = (alsa_output_plugin*) arg;

	while (true) {
		/* Wait for the new buffer to become ready */
		pthread_mutex_lock(&p->_write_mutex);
		while (!p->_write_ready && !p->_write_exit) {
			int err = pthread_cond_wait(&p->_write_cond, &p->_write_mutex);
			assert(err == 0);
		}

		bool write_exit = p->_write_exit;
		pthread_mutex_unlock(&p->_write_mutex);

		if (write_exit)
			break;

		unsigned int i = 0;
		unsigned int n = buffer_size; /* XXX */
		while (n > 0) {
			void* bufs[] = {
				(void*) (p->_frames[0] + i),
				(void*) (p->_frames[1] + i),
			};

			int err = snd_pcm_writen(p->_playback_handle, bufs, n);
			if (err < 0) {
				printf("write error: %s\n", snd_strerror(err));

				if (err == -EPIPE) {
					snd_pcm_prepare(p->_playback_handle);
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

		/* Wake up the other thread */
		pthread_mutex_lock(&p->_write_mutex);
		assert(p->_write_ready);
		p->_write_ready = false;
		pthread_cond_broadcast(&p->_write_cond);
		pthread_mutex_unlock(&p->_write_mutex);
	}

	return NULL;
}

#endif
