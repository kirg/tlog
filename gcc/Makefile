
TARGETNAME          = tlog
TARGETTYPE          = PROGRAM
UMTYPE              = windows

SOURCES             = main.c
TARGETLIBS          = psapi.lib wtsapi32.lib

C_DEFINES           = -DUNICODE
USER_C_FLAGS        =
WIN32_WINNT_VERSION = 0x0501
# more info on WIN32_WINNT_VERSION: http://msdn.microsoft.com/en-us/library/aa383745%28VS.85%29.aspx



# standard

CC = C:\bin\MinGW\bin\gcc.exe
vpath %.c ..
vpath %.h ..

CCFLAGS     = $(C_DEFINES) $(USER_C_FLAGS) -D_WIN32_WINNT=$(WIN32_WINNT_VERSION)
LDFLAGS     = $(USER_C_FLAGS) $(foreach incdir,$(INCLUDES),-I $(incdir)) $(TARGETLIBS:%.lib=-l %)

ifeq ($(TARGETTYPE),PROGRAM)
    TARGET  = $(TARGETNAME).exe
#    LDFLAGS += -mwindows
    LDFLAGS := $(LDFLAGS) -mwindows
else
    $(error TARGETTYPE="$(TARGETTYPE)" unknown)
endif


$(TARGET): $(SOURCES:%.c=%.o)
	$(CC) -o $@ $< $(LDFLAGS)

%.o: %.c
	$(CC) $(CCFLAGS) -c $<

clean:
	del $(TARGET) *.o

# todo:
# - other TARGETTYPEs
# - UMTYPE, UMENTRY
# - UNICODE

