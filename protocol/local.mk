# wld: protocol/local.mk

dir := protocol

PROTOCOL_EXTENSIONS = $(dir)/wayland-drm.xml

$(dir)/%-protocol.c: $(dir)/%.xml
	$(call quiet,GEN,$(WAYLAND_SCANNER)) code < $< > $@

$(dir)/%-client-protocol.h: $(dir)/%.xml
	$(call quiet,GEN,$(WAYLAND_SCANNER)) client-header < $< > $@

.deps/$(dir): | .deps
	@mkdir "$@"

$(dir)/%.o: $(dir)/%.c | .deps/$(dir)
	$(compile)

$(dir)/%.lo: $(dir)/%.c | .deps/$(dir)
	$(compile) -fPIC

CLEAN_FILES +=                                          \
    $(PROTOCOL_EXTENSIONS:%.xml=%-protocol.c)           \
    $(PROTOCOL_EXTENSIONS:%.xml=%-client-protocol.h)

