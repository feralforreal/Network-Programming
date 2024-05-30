CLANG := $(shell command -v clang++ 2> /dev/null)
GCC := $(shell command -v g++ 2> /dev/null)

CXX := $(if $(CLANG),$(CLANG),$(if $(GCC),$(GCC),$(error "No C++ compiler found")))
BINDIR := bin
OBJDIR := build
SRCDIR := src
INCDIR := include
LIBDIR := lib
DIRNAME := $(shell basename $(CURDIR))
PEDANTIC ?=
CFLAGS := $(if $(PEDANTIC),-Wall -Wextra -Werror -Wpedantic,) -Wno-format-security -std=c++11 -g -O0
CPPFLAGS := -I./$(INCDIR)
LDLIBS := -lpthread -lzproxy
LDFLAGS := -L./$(LIBDIR)

libzproxy := $(LIBDIR)/libzproxy.a
BINARIES := $(BINDIR)/webproxy

.PHONY : all clean tar pdf

.SUFFIXES:
.SECONDEXPANSION:
%/.DIR :
	@mkdir -p $(@D)
	@touch $@
.PRECIOUS: %/.DIR

SRC := $(wildcard $(SRCDIR)/*.cpp) $(wildcard $(SRCDIR)/**/*.cpp)
OBJ := $(SRC:%.cpp=$(OBJDIR)/%.o)
INC := $(wildcard $(INCDIR)/*.h) $(wildcard $(INCDIR)/**/*.h)

all : $(BINARIES) $(libzproxy) $(OBJ)

$(libzproxy) : $(OBJ) $(INC) | $$(@D)/.DIR
	ar crD $@ $^

$(OBJDIR)/%.o : $(CURDIR)/%.cpp | $$(@D)/.DIR
	$(CXX) $(CPPFLAGS) $(CFLAGS) -c -o $@ $(abspath $<)

$(OBJDIR)/% : %.cpp $(libzproxy) | $$(@D)/.DIR
	$(CXX) $(CPPFLAGS) $(CFLAGS) -o $@ $(abspath $<) $(LDFLAGS) $(LDLIBS)

$(BINDIR)/webproxy : $(OBJDIR)/webproxy | $(BINDIR)/.DIR
	ln -sf $(abspath $<) $@

clean : 
	rm -rf $(BINDIR) $(OBJDIR) $(LIBDIR)

pdf : 
	@echo "Generating README.pdf"
	@grip README.md --export README.html 2>/dev/null
	@-wkhtmltopdf README.html README.pdf 2>/dev/null 
	@rm README.html

tar : clean
	cd ..; tar --exclude-from=$(DIRNAME)/.tarignore -vczf zaat6507_PA3.tar.gz ./$(DIRNAME)
