#
# Makefile for VDR plugin ohpvstatus
#

PLUGIN = ohpvstatus

VERSION = $(shell grep 'static const char \*VERSION' $(PLUGIN).cpp | awk '{ print $$6 }' | sed -e 's/[";]//g')

PKGCFG = $(if $(VDRDIR),$(shell pkg-config --variable=$(1) $(VDRDIR)/vdr.pc),$(shell PKG_CONFIG_PATH="$$PKG_CONFIG_PATH:../../.." pkg-config --variable=$(1) vdr))
LIBDIR  = $(call PKGCFG,libdir)
LOCDIR  = $(call PKGCFG,locdir)
PLGCFG  = $(call PKGCFG,plgcfg)

TMPDIR ?= /tmp

export CFLAGS   = $(call PKGCFG,cflags)
export CXXFLAGS = $(call PKGCFG,cxxflags)

APIVERSION = $(call PKGCFG,apiversion)

-include $(PLGCFG)

ARCHIVE = $(PLUGIN)-$(VERSION)
PACKAGE = vdr-$(ARCHIVE)
SOFILE  = libvdr-$(PLUGIN).so

INCLUDES += -I/usr/include/vdr
DEFINES  += -DPLUGIN_NAME_I18N='"$(PLUGIN)"'
LIBS     += -lcurl

OBJS = $(PLUGIN).o menu.o openhab.o

all: $(SOFILE) i18n

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $(DEFINES) $(INCLUDES) -o $@ $<

$(OBJS): Makefile

### I18N
PODIR    = po
I18Npo   = $(wildcard $(PODIR)/*.po)
I18Nmo   = $(addsuffix .mo, $(foreach file, $(I18Npo), $(basename $(file))))
I18Nmsgs = $(addprefix $(DESTDIR)$(LOCDIR)/, $(addsuffix /LC_MESSAGES/vdr-$(PLUGIN).mo, $(notdir $(foreach file, $(I18Npo), $(basename $(file))))))
I18Npot  = $(PODIR)/$(PLUGIN).pot

%.mo: %.po
	msgfmt -c -o $@ $<

$(I18Npot): $(wildcard *.cpp *.h)
	xgettext -C -cTRANSLATORS --no-wrap --no-location -k -ktr -ktrNOOP \
	-k --keyword=trNOOP --flag=tr:1:pass-c-format --from-code=UTF-8 \
	-o $@ $(wildcard *.cpp *.h)

%.po: $(I18Npot)
	msgmerge -U --no-wrap --no-location --backup=none -q -N $@ $<
	@touch $@

$(I18Nmsgs): $(DESTDIR)$(LOCDIR)/%/LC_MESSAGES/vdr-$(PLUGIN).mo: $(PODIR)/%.mo
	install -D -m644 $< $@

.PHONY: i18n
i18n: $(I18Nmo) $(I18Npot)

install-i18n: $(I18Nmsgs)

### Targets
$(SOFILE): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -shared $(OBJS) $(LIBS) -o $@

install-lib: $(SOFILE)
	install -D -m644 $^ $(DESTDIR)$(LIBDIR)/$^.$(APIVERSION)

install-conf:
	install -D -m644 $(PLUGIN).conf $(DESTDIR)/etc/vdr/conf.avail/$(PLUGIN).conf

install: install-lib install-i18n

uninstall:
	rm -f $(DESTDIR)$(LIBDIR)/$(SOFILE).$(APIVERSION)
	rm -f $(DESTDIR)/etc/vdr/conf.avail/$(PLUGIN).conf
	@for f in $(I18Nmo); do \
		lang=$$(basename $$f .mo); \
		rm -f $(DESTDIR)$(LOCDIR)/$$lang/LC_MESSAGES/vdr-$(PLUGIN).mo; \
	done

dist: $(I18Npo) clean
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@mkdir $(TMPDIR)/$(ARCHIVE)
	@cp -a * $(TMPDIR)/$(ARCHIVE)
	@tar czf $(PACKAGE).tgz -C $(TMPDIR) $(ARCHIVE)
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@echo Distribution package created as $(PACKAGE).tgz

clean:
	@-rm -f $(OBJS) $(SOFILE) $(I18Nmo) $(I18Npot) *.tgz
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
