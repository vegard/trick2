#ifndef GRAPH_HH
#define GRAPH_HH

#include <set>

#include "edge.hh"
#include "plugin.hh"

class graph {
public:
	typedef std::set<plugin*> plugin_set;

public:
	graph();
	~graph();

public:
	void add(plugin* p);
	void remove(plugin* p);

	void connect(plugin* a, unsigned int a_port,
		plugin* b, unsigned int b_port);
	void disconnect(plugin* a, unsigned int a_port,
		plugin* b, unsigned int b_port);

	void run(unsigned int sample_count);

public:
	plugin_set _plugins;
};

graph::graph()
{
}

graph::~graph()
{
}

void
graph::add(plugin* p)
{
	_plugins.insert(p);
}

void
graph::remove(plugin* p)
{
	_plugins.erase(p);
}

/* "a" is the output plugin, "b" is the input plugin.
 * "b" consumes "a"'s input.
 * "b" depends on "a". */
void
graph::connect(plugin* a, unsigned int a_port,
	plugin* b, unsigned int b_port)
{
	/* add a to b's set of incoming edges */
	{
		plugin::plugin_map::iterator i = b->_deps.find(a);
		if (i == b->_deps.end()) {
			edge* e = new edge();
			e->inc();
			b->_deps.insert(std::make_pair(a, e));
		} else
			++i->second->_count;
	}

	/* add b to a's set of outgoing edges */
	{
		plugin::plugin_map::iterator i = a->_rev_deps.find(b);
		if (i == a->_rev_deps.end()) {
			edge* e = new edge();
			e->inc();
			a->_rev_deps.insert(std::make_pair(b, e));
		} else
			++i->second->_count;
	}

	b->connect(b_port, a->_ports[a_port]);
}

void
graph::disconnect(plugin* a, unsigned int a_port,
	plugin* b, unsigned int b_port)
{
	/* remove a from b's set of incoming edges */
	{
		plugin::plugin_map::iterator i = b->_deps.find(a);
		assert(i != b->_deps.end());
		edge* e = i->second;
		if (e->dec() == 0) {
			b->_deps.erase(i);
			delete e;
		}
	}

	/* remove b from a's set of outgoing edges */
	{
		plugin::plugin_map::iterator i = a->_rev_deps.find(b);
		assert(i != a->_rev_deps.end());
		edge* e = i->second;
		if (e->dec() == 0) {
			a->_rev_deps.erase(i);
			delete e;
		}
	}

	b->disconnect(b_port);
}

void
graph::run(unsigned int sample_count)
{
	unsigned int n = 0;
	for (plugin_set::iterator i = _plugins.begin(), end = _plugins.end();
		i != end; ++i)
	{
		plugin* p = *i;

		if (p->_rev_deps.empty()) {
			p->run(sample_count);
			++n;
		}
	}

	assert(n > 0);
}

#endif
