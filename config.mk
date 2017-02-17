# sfm version
VERSION = 0.1

# includes and libs
LIBS = -ltermbox

# flags
CPPFLAGS = -D_POSIX_C_SOURCE=200809L
CFLAGS += -std=c99 -pedantic -Wall -Wextra -Wvariadic-macros -Os ${INCS} ${CPPFLAGS}
LDFLAGS += ${LIBS}
