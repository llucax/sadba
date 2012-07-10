SUBDIRS=src po
BUILDDIR=build

all: subdirs

subdirs: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

.PHONY: all clean install $(SUBDIRS)

clean:
	for d in $(SUBDIRS); do (cd $$d; $(MAKE) clean); done

install:
	for d in $(SUBDIRS); do (cd $$d; $(MAKE) install); done


