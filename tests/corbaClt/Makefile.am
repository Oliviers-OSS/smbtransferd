AUTOMAKE_OPTIONS = gnu
INCLUDES = -I$(top_builddir)/include -I$(top_srcdir) -I$(top_builddir)/../include 
AM_CPPFLAGS=-DCONFIGDIR="$(sysconfdir)"

bin_PROGRAMS = smbtransferdCorbaClt
smbtransferdCorbaClt_SOURCES = main.cpp EventListenerImpl.cpp
include_HEADERS = EventListenerImpl.h
smbtransferdCorbaClt_LDFLAGS = -ldbgflags -lpthread
#smbtransferd_DEPENDENCIES = ../libdbgflags/.libs/libdbgflags.so.0.0.0

IDL_COMPILER=tao_idl
IDL_COMPILER_OPT=-in
IDL_COMPILER_GENERATED_HEADERS_FILES = ../../remoteServersSetS.inl ../../remoteServersSetS.h ../../remoteServersSetC.h ../../remoteServersSetC.inl
IDL_COMPILER_GENERATED_SOURCES_FILES = ../../remoteServersSetS.cpp ../../remoteServersSetC.cpp 
include_HEADERS += 
smbtransferdCorbaClt_SOURCES += $(IDL_COMPILER_GENERATED_SOURCES_FILES)
AM_CXXFLAGS=-D_REENTRANT
AM_CPPFLAGS+=-I/usr/include/orbsvcs
smbtransferdCorbaClt_LDFLAGS += -lTAO -lACE -lTAO_PortableServer -lTAO_CosNaming

.idl: $(IDL_COMPILER_GENERATED_FILES)
	$(IDL_COMPILER) $(IDL_COMPILER_OPT) ../../remoteServersSet.idl


