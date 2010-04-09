SUBDIRS = mp mpsrc test

MPLEX = $(RUBY) $(PWD)/mplex -r$(PWD)/mpl.rb
export MPLEX

prep: mpl.rb
	cd mp && $(MAKE) prep

prepc:
	cd mp && $(MAKE) prepc

