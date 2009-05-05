all: a.out

KV331_3_RondoAllaTurca.mid:
	wget http://www.mutopiaproject.org/ftp/MozartWA/KV331/KV331_3_RondoAllaTurca/KV331_3_RondoAllaTurca.mid

toccata1.mid:
	wget http://www.bachcentral.com/BachCentral/ORGAN/toccata1.html

a.out: $(wildcard *.cc) $(wildcard *.hh) KV331_3_RondoAllaTurca.mid toccata1.mid
	g++ -Wall -g -pg main.cc -lasound -lsndfile -lpthread
