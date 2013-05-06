AUTOMAKE_OPTIONS = gnu
SUBDIRS = tests
INCLUDES = -I$(top_builddir)/include -I$(top_srcdir) -I$(top_builddir)/../include 
AM_CPPFLAGS=-DCONFIGDIR="$(sysconfdir)"

bin_PROGRAMS = smbtransferd
smbtransferd_SOURCES = main.c daemon.c monitorInotify.c smbcopy.c debug.c filesystem.c runtime.c messagesQueue.c configuration.c threadsPool.c iniparser.c dictionary.c strlib.c version.c malloc.c
noinst_HEADERS = globals.h debug.h inotify-nosys.h daemon.h monitorInotify.h smbcopy.h filesystem.h runtime.h messagesQueue.h threadsPool.h event.h iniparser.h dictionary.h strlib.h strlib.h configuration.h list.h klist.h 
smbtransferd_LDFLAGS = -ldbgflags -lpthread -lsmbclient -lrt
#smbtransferd_SYSCONFIG=smbtransferd.conf
sysconf_DATA = smbtransferd.conf
EXTRA_DIST = $(sysconf_DATA)
#smbtransferd_DEPENDENCIES = ../libdbgflags/.libs/libdbgflags.so.0.0.0

#if FUNCTIONS_CALLS
#  AM_CPPFLAGS+=-ggdb3 -finstrument-functions
#  include_HEADERS += bucomm.h demangle.h traceFunctionsCalls.h
#  smbtransferd_SOURCES += traceFunctionsCalls.c
#  smbtransferd_LDFLAGS += -lbfd -liberty 
#endif

if CORBA_ENABLED
  IDL_COMPILER=tao_idl
  IDL_COMPILER_OPT=-in 
  IDL_COMPILER_GENERATED_HEADERS_FILES = remoteServersSetS.inl remoteServersSetS.h remoteServersSetC.h remoteServersSetC.inl
  IDL_COMPILER_GENERATED_SOURCES_FILES = remoteServersSetS.cpp remoteServersSetC.cpp 
  noinst_HEADERS += corbaInterfaces.h remoteServersSetManagementImpl.h remoteServersSetEventSubscriberImpl.h remoteServersSetEventSubscriberMgr.h
  smbtransferd_SOURCES += corbaInterfaces.cpp remoteServersSetManagementImpl.cpp remoteServersSetEventSubscriberImpl.cpp remoteServersSetEventSubscriberMgr.cpp $(IDL_COMPILER_GENERATED_SOURCES_FILES)
  AM_CXXFLAGS=-D_REENTRANT
  AM_CPPFLAGS+=-I/usr/include/orbsvcs $(TAO_CPPFLAGS)
  smbtransferd_LDFLAGS += -lTAO -lACE -lTAO_PortableServer -lTAO_CosNaming $(TAO_LIBS)
.idl: $(IDL_COMPILER_GENERATED_FILES)
	$(IDL_COMPILER) $(IDL_COMPILER_OPT) remoteServersSet.idl

endif

# The rpmbuild command builds software in the $RPMBUILD_ROOT/BUILD directory.
# The rpmbuild command stores binary RPMs it creates in the $RPMBUILD_ROOT/RPMS directory
# $RPMBUILD_ROOT/SOURCES directory contains the sources for the application
# $RPMBUILD_ROOT/SPECS contains the spec file for each RPM you plan to make
# The rpmbuild command stores source RPMs it creates in the $RPMBUILD_ROOT/SRPMS directory.  

RPMBUILD_ROOT = $(top_srcdir)/rpm
SPEC_FILE = ${RPMBUILD_ROOT}/SPECS/${PACKAGE_TARNAME}.spec
SPEC_BIN_FILE = ${RPMBUILD_ROOT}/SPECS/${PACKAGE_TARNAME}-bin.spec
FULL_RPMBUILD_ROOT = ${PWD}/rpm

dist-rpm: ${PACKAGE_TARNAME}-${PACKAGE_VERSION}.tar.gz
	/bin/rm -Rf ${RPMBUILD_ROOT} && \
	/bin/mkdir -p ${RPMBUILD_ROOT} ${RPMBUILD_ROOT}/BUILD ${RPMBUILD_ROOT}/RPMS ${RPMBUILD_ROOT}/SOURCES ${RPMBUILD_ROOT}/SPECS ${RPMBUILD_ROOT}/SRPMS 
	/bin/cp $(top_srcdir)/${PACKAGE_TARNAME}-${PACKAGE_VERSION}.tar.gz ${RPMBUILD_ROOT}/SOURCES
	echo /bin/rm -F ${RPMBUILD_ROOT}/SPECS/*
	/bin/echo "Summary: SMB transfer daemon" >${SPEC_FILE}
	/bin/echo "%define version ${PACKAGE_VERSION}" >>${SPEC_FILE}
	/bin/echo "License: GPL" >>${SPEC_FILE}
	/bin/echo "Group: Applications/Communications" >>${SPEC_FILE}
	/bin/echo "Name: ${PACKAGE_NAME}" >>${SPEC_FILE}
	/bin/echo "Prefix: ${prefix}" >>${SPEC_FILE}
	/bin/echo "Release: 1" >>${SPEC_FILE}
	/bin/echo "Source: ${PACKAGE_TARNAME}-${PACKAGE_VERSION}.tar.gz" >>${SPEC_FILE}
	/bin/echo "Version: %{version}" >>${SPEC_FILE}
	/bin/echo "Requires: dbgflags, libsmb, TAO" >>${SPEC_FILE}
	/bin/echo "Buildroot: /tmp/${PACKAGE_NAME}" >>${SPEC_FILE}
	/bin/echo "%description" >>${SPEC_FILE}
	/bin/echo "transfert files from a monitored local directory to a remote server using the SMB protocol" >>${SPEC_FILE}
	/bin/echo "%prep" >>${SPEC_FILE}
	/bin/echo "%setup -q -n ${PACKAGE_NAME}-%{PACKAGE_VERSION}" >>${SPEC_FILE}
	/bin/echo "%build" >>${SPEC_FILE}
	/bin/echo "./configure --prefix=${prefix} && make" >>${SPEC_FILE}
	/bin/echo "%install" >>${SPEC_FILE}
	/bin/echo "/bin/rm -Rf ${RPM_BUILD_ROOT}" >>${SPEC_FILE}
	/bin/echo "make DESTDIR=%{?buildroot:%{buildroot}} install" >>${SPEC_FILE}
	/bin/echo "%clean" >>${SPEC_FILE}
	/bin/echo "/bin/rm -Rf ${RPM_BUILD_ROOT}" >>${SPEC_FILE}
	/bin/echo "%files" >>${SPEC_FILE}
	/bin/echo "%defattr(-,root,root)" >>${SPEC_FILE}
	/bin/echo "${prefix}/bin/${bin_PROGRAMS}" >>${SPEC_FILE}
	/bin/echo "${prefix}/etc/${sysconf_DATA}" >>${SPEC_FILE}	
	/bin/echo "%_topdir ${FULL_RPMBUILD_ROOT}" >${HOME}/.rpmmacros
	/bin/echo "%_topdir ${FULL_RPMBUILD_ROOT}" >${FULL_RPMBUILD_ROOT}/rpmmacros
	/bin/cp $(top_srcdir)/${PACKAGE_TARNAME}-${PACKAGE_VERSION}.tar.gz ${RPMBUILD_ROOT}/SOURCES
	rpmbuild -v -ba --macros=${FULL_RPMBUILD_ROOT}/rpmmacros ${SPEC_FILE} 

dist-bin-rpm: ${bin_PROGRAMS} ${sysconf_DATA}
	/bin/rm -Rf ${RPMBUILD_ROOT} && \
	/bin/mkdir -p ${RPMBUILD_ROOT} ${RPMBUILD_ROOT}/BUILD ${RPMBUILD_ROOT}/RPMS ${RPMBUILD_ROOT}/SOURCES ${RPMBUILD_ROOT}/SPECS ${RPMBUILD_ROOT}/SRPMS 
	/bin/cp $(top_srcdir)/${PACKAGE_TARNAME}-${PACKAGE_VERSION}.tar.gz ${RPMBUILD_ROOT}/SOURCES
	echo /bin/rm -F ${RPMBUILD_ROOT}/SPECS/*
	/bin/echo "Summary: SMB transfer daemon" >${SPEC_BIN_FILE}
	/bin/echo "%define version ${PACKAGE_VERSION}" >>${SPEC_BIN_FILE}
	/bin/echo "License: GPL" >>${SPEC_BIN_FILE}
	/bin/echo "Group: Applications/Communications" >>${SPEC_BIN_FILE}
	/bin/echo "Name: ${PACKAGE_NAME}" >>${SPEC_BIN_FILE}
	/bin/echo "Prefix: ${prefix}" >>${SPEC_BIN_FILE}
	/bin/echo "Release: bin" >>${SPEC_BIN_FILE}
	/bin/echo "Version: %{version}" >>${SPEC_BIN_FILE}
	/bin/echo "Requires: dbgflags, TAO" >>${SPEC_BIN_FILE}
	/bin/echo "Buildroot: ${FULL_RPMBUILD_ROOT}/BUILD" >>${SPEC_BIN_FILE}
	/bin/echo "%description" >>${SPEC_BIN_FILE}
	/bin/echo "transfert files from a monitored local directory to a remote server using the SMB protocol" >>${SPEC_BIN_FILE}
	/bin/echo "%prep" >>${SPEC_FILE}
	/bin/echo "%setup -q -n rdpmgr-%{PACKAGE_VERSION}" >>${SPEC_BIN_FILE}
	/bin/echo "%build" >>${SPEC_BIN_FILE}
	/bin/echo "/bin/mkdir -p ${FULL_RPMBUILD_ROOT}/BUILD${prefix}/bin " >>${SPEC_BIN_FILE}
	/bin/echo "/bin/cp ${PWD}/${bin_PROGRAMS} ${FULL_RPMBUILD_ROOT}/BUILD${prefix}/bin" >>${SPEC_BIN_FILE}
	/bin/echo "/bin/mkdir -p ${FULL_RPMBUILD_ROOT}/BUILD${prefix}/etc " >>${SPEC_BIN_FILE}
	/bin/echo "/bin/cp ${PWD}/${sysconf_DATA} ${FULL_RPMBUILD_ROOT}/BUILD${prefix}/etc" >>${SPEC_BIN_FILE}
	/bin/echo "%clean" >>${SPEC_BIN_FILE}
	/bin/echo "/bin/rm -Rf ${RPM_BUILD_ROOT}" >>${SPEC_BIN_FILE}
	/bin/echo "%files" >>${SPEC_BIN_FILE}
	/bin/echo "%defattr(-,root,root)" >>${SPEC_BIN_FILE}
	/bin/echo "${prefix}/bin/${bin_PROGRAMS}" >>${SPEC_BIN_FILE}
	/bin/echo "${prefix}/etc/${sysconf_DATA}" >>${SPEC_BIN_FILE}	
	/bin/echo "%_topdir ${FULL_RPMBUILD_ROOT}" >${HOME}/.rpmmacros
	/bin/echo "%_topdir ${FULL_RPMBUILD_ROOT}" >${FULL_RPMBUILD_ROOT}/rpmmacros
	rpmbuild -v -bb --macros=${FULL_RPMBUILD_ROOT}/rpmmacros ${SPEC_BIN_FILE} 
