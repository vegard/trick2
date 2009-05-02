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

#include "alsa_plugin.hh"
#include "edge.hh"
#include "graph.hh"
#include "ladspa_plugin.hh"
#include "plugin.hh"
#include "sequencer.hh"
#include "simple_sequencer.hh"

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
	plugin* organ = new ladspa_plugin("/home/vegard/programming/cmt/plugins/cmt.so", "organ");
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

	plugin* reverb = new ladspa_plugin("/usr/lib64/ladspa/plate_1423.so", "plate");
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

	alsa_plugin* output = new alsa_plugin();
	output->activate();

	output->connect(0, reverb->_ports[4]);
	output->connect(1, reverb->_ports[5]);

	printf("running...\n");
	while (1) {
		g->run(buffer_size);
		output->run(buffer_size);
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
