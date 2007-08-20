# Makefile for toplevel elsa distribution

# just do the obvious recursive thing
all:
	$(MAKE) -C smbase
	$(MAKE) -C ast
	$(MAKE) -C elkhound
	$(MAKE) -C elsa
	$(MAKE) -C asttools all
	$(MAKE) -C cfg all

check:
	$(MAKE) -C smbase check
	$(MAKE) -C ast check
	$(MAKE) -C elkhound check
	$(MAKE) -C elsa check
	$(MAKE) -C asttools check
	$(MAKE) -C cfg check

clean:
	$(MAKE) -C smbase clean
	$(MAKE) -C ast clean
	$(MAKE) -C elkhound clean
	$(MAKE) -C elsa clean
	$(MAKE) -C asttools clean
	$(MAKE) -C cfg clean

# I generate distributions out of cvs, where there are no Makefiles
distclean:
	$(MAKE) -C smbase -f Makefile.in distclean
	$(MAKE) -C ast -f Makefile.in distclean
	$(MAKE) -C elkhound -f Makefile.in distclean
	$(MAKE) -C elsa -f Makefile.in distclean
	$(MAKE) -C asttools -f Makefile.in distclean
	$(MAKE) -C cfg -f Makefile.in distclean
	rm -rf test semantic TODO cvsanon-filter

doc:
	$(MAKE) -C smbase doc
	$(MAKE) -C ast doc
	$(MAKE) -C elkhound doc
	$(MAKE) -C elsa doc
	$(MAKE) -C asttools doc
	$(MAKE) -C cfg doc

docclean:
	$(MAKE) -C smbase docclean
	$(MAKE) -C ast docclean
	$(MAKE) -C elkhound docclean
	$(MAKE) -C elsa docclean
	$(MAKE) -C asttools docclean
	$(MAKE) -C cfg docclean

stable:
	cvs tag -d latest-stable
	cvs tag latest-stable
