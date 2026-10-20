# Host unit tests for hook/src/shopredirect_logic.h, against the HOST Qt
# (Qt 6 preferred, Qt 5.14+ as fallback). No cross toolchain needed.
#   make check

CXX      ?= c++
# -fPIC: Debian/Ubuntu build Qt with -reduce-relocations, which refuses to
# link into a non-PIC (-fPIE) binary. Harmless elsewhere.
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Werror -fPIC

QT_CORE  := $(shell pkg-config --exists Qt6Core && echo Qt6Core || echo Qt5Core)
CPPFLAGS += $(shell pkg-config --cflags $(QT_CORE))
LDLIBS   += $(shell pkg-config --libs $(QT_CORE))

test_logic: test_logic.cpp ../hook/src/shopredirect_logic.h
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) test_logic.cpp -o $@ $(LDLIBS)

check: test_logic
	./test_logic ../examples/config.ini

clean:
	rm -f test_logic

.PHONY: check clean
