

GETTEXT_PACKAGE = status-area-displayblanking-applet
PACKAGE = status-area-displayblanking-applet

SHELL = /bin/sh

prefix = /usr
DATADIRNAME = share
itlocaledir = $(prefix)/$(DATADIRNAME)/locale

INSTALL = install -c
INSTALL_DATA = ${INSTALL} -m 644

GMSGFMT = msgfmt
MSGFMT = msgfmt
XGETTEXT = xgettext
MSGMERGE = msgmerge

PO_LINGUAS= es_AR

USE_LINGUAS= es_AR

POFILES=$(shell LINGUAS="$(PO_LINGUAS)"; for lang in $$LINGUAS; do printf "$$lang.po "; done)

DISTFILES = Makefile POTFILES.in $(POFILES)
EXTRA_DISTFILES = ChangeLog POTFILES.skip Makevars LINGUAS

POTFILES = ../src/lib-display-blanking-status-menu-widget.c
# This comment gets stripped out

CATALOGS=$(shell LINGUAS="$(USE_LINGUAS)"; for lang in $$LINGUAS; do printf "$$lang.gmo "; done)

.SUFFIXES:
.SUFFIXES: .po .gmo .mo

.po.mo:
	$(MSGFMT) -o $@ $<

.po.gmo:
	file=`echo $* | sed 's,.*/,,'`.gmo \
	  && rm -f $$file && $(GMSGFMT) -o $$file $<

all: compile

compile: $(CATALOGS)

$(GETTEXT_PACKAGE).pot: $(POTFILES)
	$(XGETTEXT) --package-name=$(PACKAGE) -o $(GETTEXT_PACKAGE).pot $(POTFILES)

install: all
	@linguas="$(USE_LINGUAS)"; \
	for lang in $$linguas; do \
	  dir=$(DESTDIR)$(itlocaledir)/$$lang/LC_MESSAGES; \
	  mkdir -p $$dir; \
	  if test -r $$lang.gmo; then \
	    $(INSTALL_DATA) $$lang.gmo $$dir/$(GETTEXT_PACKAGE).mo; \
	    echo "installing $$lang.gmo as $$dir/$(GETTEXT_PACKAGE).mo"; \
	  fi; \
	done

clean:
	rm -f *.mo *.gmo

update-po:
	$(MAKE) $(GETTEXT_PACKAGE).pot
	@linguas="$(USE_LINGUAS)"; \
	for lang in $$linguas; do \
	  echo "$$lang:"; \
	  if test -r $$lang.po; then \
	    $(MSGMERGE) -U $$lang.po $(GETTEXT_PACKAGE).pot; \
	  else \
	    cp $(GETTEXT_PACKAGE).pot $$lang.po; \
	  fi; \
	done

