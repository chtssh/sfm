include config.mk

SRC = sfm.c ui.c nav.c hash.c util.c
OBJ = ${SRC:.c=.o}

all: termbox options sfm

options:
	@echo sfm build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

${OBJ}: config.h config.mk

config.h:
	@echo creating $@ from config.def.h
	@cp config.def.h $@

sfm: ${OBJ}
	@echo CC -o $@
	@${CC} -o $@ ${OBJ} ${LDFLAGS}

termbox:
	$(MAKE) -C $@

clean:
	@echo cleaning
	@rm -f sfm ${OBJ}
	$(MAKE) -C termbox clean

.PHONY: all termbox options clean
