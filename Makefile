SOURCE = main.cpp			\
		 console.cpp

CC=gcc
CXX=g++
CFLAG= -g -O0 -m64 -fPIC -rdynamic
LDFLAG= -g -O0 -m64 -rdynamic -ldl
# DEFS=

TARGET = console

all:$(TARGET)

# INCLUDE=$(OB_REL)/include

DEFS+=LINUX

# for normal

LD_LIBARY=$(addprefix -l, $(PREDPEND))
INCLUDE_PATHS=$(addprefix -I, $(INCLUDE))
LIBARY_PATHS=$(addprefix -L, $(LIBARY))
MACRO_DEFS=$(addprefix -D, $(DEFS))

OBJECT=$(SOURCE:.cpp=.o)
DEPEND=$(SOURCE:.cpp=.d)

console:$(OBJECT)
	$(CXX) $(LDFLAG) -o $@ $(OBJECT)

$(DEPEND):%.d:%.cpp
	$(CXX) $(INCLUDE_PATHS) -MM $< > $@;	\
	echo "\t$(CXX) $(CFLAG) $(INCLUDE_PATHS) $(MACRO_DEFS) -c $< -o $(@:.d=.o)" >> $@


-include $(DEPEND)

.PHONY: clean
clean:
	-rm -f $(TARGET) $(OBJECT) $(DEPEND) core*

