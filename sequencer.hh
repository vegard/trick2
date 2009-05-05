#ifndef SEQUENCER_HH
#define SEQUENCER_HH

class sequencer {
public:
	sequencer();
	virtual ~sequencer();

public:
	virtual unsigned int duration_remaining(unsigned int voice) = 0;
	virtual void advance(unsigned int voice, unsigned int duration) = 0;
};

sequencer::sequencer()
{
}

sequencer::~sequencer()
{
}

#endif
