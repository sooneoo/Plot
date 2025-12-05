PLATFORM ?= linux_x86_64

SRC=src

CC=gcc
CFLAGS += -Wall 
CFLAGS += -Wextra 
CFLAGS += -pedantic 
CFLAGS += -O2
CFLAGS += -I$(SRC)

LIBS += -lalloc
LIBS += -ljson


TARGET=libplot.a
CACHE=.cache
OUTPUT=$(CACHE)/release


MODULES += plot.o

TEST += test.o

-include config/$(PLATFORM).mk

OBJ=$(addprefix $(CACHE)/,$(MODULES))
T_OBJ=$(addprefix $(CACHE)/,$(TEST))


$(CACHE)/%.o:
	$(CC) $(CFLAGS) -c $< -o $@


all: env $(OBJ)
	ar -crs $(OUTPUT)/$(TARGET) $(OBJ)


-include dep.list


.PHONY: env dep clean install


dep:
	$(FIND) src test -name "*.c" | xargs $(CC) -I$(SRC) -MM | sed 's|[a-zA-Z0-9_-]*\.o|$(CACHE)/&|' > dep.list


exec: env $(T_OBJ) $(OBJ) 
	$(CC) $(CFLAGS) $(T_OBJ) $(OBJ) $(LIBS) -o $(OUTPUT)/test
	$(OUTPUT)/test


install:
	mkdir -pv $(INDIR)
	cp -v $(OUTPUT)/$(TARGET) $(LIBDIR)/$(TARGET)
	cp -vr src/plot/*.[h] $(INDIR)


env:
	mkdir -pv $(CACHE)
	mkdir -pv $(OUTPUT)


clean: 
	rm -rvf $(CACHE)



