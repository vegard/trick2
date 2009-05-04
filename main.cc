#include <map>
#include <set>

extern "C" {
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ladspa.h>
#include <math.h>
}

static const unsigned long sample_rate = 44100;
static const unsigned long buffer_size = 16384;

static LADSPA_Data silence_buffer[buffer_size];

#include "alsa_output_plugin.hh"
#include "edge.hh"
#include "graph.hh"
#include "ladspa_plugin.hh"
#include "midi_sequencer.hh"
#include "plugin.hh"
#include "sequencer.hh"
#include "simple_sequencer.hh"
#include "wav_output_plugin.hh"

#if 0
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
#endif

static bool running;

static void handle_sigint(int signo)
{
	running = false;
}

int
main(int argc, char* argv[])
{
	signal(SIGINT, &handle_sigint);

	graph* g = new graph();

#ifndef FILE_OUTPUT
	plugin* output = new alsa_output_plugin("plughw:0,0");
#else
	plugin* output = new wav_output_plugin("output.wav");
#endif

	//plugin* organ = new plugin("/usr/lib64/ladspa/cmt.so", "organ");
	plugin* organ = new ladspa_plugin("/home/vegard/programming/cmt/plugins/cmt.so", "organ");

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

	plugin* reverb = new ladspa_plugin("/usr/lib64/ladspa/plate_1423.so", "plate");

	reverb->_ports[0][0] = 6.00;	/* Reverb time */
	reverb->_ports[1][0] = 0.07;	/* Damping */
	reverb->_ports[2][0] = 0.50;	/* Dry/wet */

#if 0
	sequencer* seq = new simple_sequencer(60,
		song, sizeof(song) / sizeof(*song),
		organ->_ports[3]);
#else
	midi_sequencer* seq = new midi_sequencer("KV331_3_RondoAllaTurca.mid");
	seq->connect_frequency(0, organ->_ports[3]);
#endif
	organ->_seqs.insert(seq);

	g->add(organ);
	g->add(reverb);
	g->add(output);
	g->connect(organ, 0, reverb, 3);
	g->connect(reverb, 4, output, 0);
	g->connect(reverb, 5, output, 1);

	printf("running...\n");

	g->activate();

	running = true;
#ifdef FILE_OUTPUT
	for (unsigned int i = 0; running && i < 1000; ++i)
#else
	while (running)
#endif
		g->run(buffer_size);

	g->deactivate();

	g->disconnect(reverb, 5, output, 1);
	g->disconnect(reverb, 4, output, 0);
	g->disconnect(organ, 0, reverb, 3);
	g->remove(output);
	g->remove(reverb);
	g->remove(organ);

	delete seq;
	delete reverb;
	delete organ;
	delete output;
	delete g;

	return EXIT_SUCCESS;
}
