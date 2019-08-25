# wld: protocol/local.mk

dir := protocol

PROTOCOL_EXTENSIONS = $(dir)/wayland-drm.xml

# GNUMake correctly marks %-protocol.c as an intermediate file, and deletes it
# after the build. The file can be preserved by marking it as precious.
.PRECIOUS: $(dir)/%-protocol.c

$(dir)/%-protocol.c: $(dir)/%.xml
	$(call quiet,GEN,$(WAYLAND_SCANNER)) private-code < $< > $@

$(dir)/%-client-protocol.h: $(dir)/%.xml
	$(call quiet,GEN,$(WAYLAND_SCANNER)) client-header < $< > $@

CLEAN_FILES +=                                          \
    $(PROTOCOL_EXTENSIONS:%.xml=%-protocol.c)           \
    $(PROTOCOL_EXTENSIONS:%.xml=%-client-protocol.h)

include common.mk

