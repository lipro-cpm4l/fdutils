# Define PIDFILE if you want to have the pid of the diskseekd stored
# somewhere on the filesystem. You'll get a (very) little shell
# script diskseek.sh that uses this to connect with the daemon
# when the disk should be cleaned by hand. In that case it's a replacement
# for diskseek. If you don't want to have this feature, don't define it.
#
DEFINES=-DPIDFILE=\"/var/run/diskseekd.pid\"

CC=gcc
OBJECTS=enh_options.o floppycontrol.o superformat.o
SOURCES=enh_options.c floppycontrol.o superformat.c
MANPAGES1=diskd.1 diskseekd.1 fdrawcmd.1 floppycontrol.1 getfdprm.1  \
makefloppies.1 superformat.1 xdfcopy.1 fdmount.1
MANPAGES4=fd.4
MANPAGES8=setfdprm.8
CFLAGS=-O4 -fno-strength-reduce -Wall $(DEFINES)
LDFLAGS=-s
#CFLAGS=-O4 -g -Wall $(DEFINES)
#LDFLAGS=-N

DESTDIR=
MANDIR=${DESTDIR}/usr/man
MANDIR1=$(MANDIR)/man1
MANDIR4=$(MANDIR)/man4
MANDIR8=$(MANDIR)/man8
SBIN=${DESTDIR}/usr/sbin
BIN=${DESTDIR}/usr/bin
PERM=755
SPERM=4750
#we only people in group floppy to run priviledged programs
MANPERM=644
UID	= root
GID	= floppy
INSTALL	= install

all: floppycontrol getfdprm setfdprm fdrawcmd \
superformat xdfcopy fdmount diskseekd diskd

floppycontrol.o getfdprm setfdprm: /usr/include/linux/fd.h
diskseekd.o superformat.o fdrawcmd.o: /usr/include/linux/fd.h \
	/usr/include/linux/fdreg.h

floppycontrol: enh_options.o floppycontrol.o
	${CC} $(LDFLAGS) -o floppycontrol enh_options.o floppycontrol.o

diskd: enh_options.o diskd.o
	${CC} $(LDFLAGS) -o diskd enh_options.o diskd.o

diskseekd: enh_options.o diskseekd.o
	${CC} $(LDFLAGS) -o diskseekd enh_options.o diskseekd.o

diskseek: diskseekd
	ln -s diskseekd diskseek

superformat: message_seen enh_options.o superformat.o
	${CC} $(LDFLAGS) -o superformat enh_options.o superformat.o

fdrawcmd: fdrawcmd.o
	${CC} $(LDFLAGS) -o fdrawcmd fdrawcmd.o

getfdprm: getfdprm.o
	${CC} $(LDFLAGS) -o getfdprm getfdprm.o

setfdprm: setfdprm.c
	${CC} $(LDFLAGS) $(CFLAGS) -o setfdprm setfdprm.c

clean:
	-rm -f *~ *.orig *.o a.out core 2>/dev/null

spotless:	clean
	-rm -f floppycontrol superformat getfdprm fdrawcmd diskseekd diskd \
	diskseek diskd_old setfdprm xdfcopy fdmount 2>/dev/null

message_seen:
	@cat message
	@read a
	@touch message_seen

install: install.bin install.man
	@grep -q '^floppy:' /etc/group \
	    || echo 'Add a group "floppy" to /etc/group.'

install.bin: all
	install -c -m $(PERM) -o $(UID) -g $(GID) MAKEFLOPPIES $(SBIN)
	install -c -s -m $(PERM) -o $(UID) -g $(GID) floppycontrol $(BIN)
	install -c -s -m $(PERM) -o $(UID) -g $(GID) getfdprm $(BIN)
	install -c -s -m $(PERM) -o $(UID) -g $(GID) setfdprm $(BIN)
	install -c -s -m $(PERM) -o $(UID) -g $(GID) fdrawcmd $(BIN)
	install -c -s -m $(PERM) -o $(UID) -g $(GID) superformat $(BIN)
	install -c -s -m $(PERM) -o $(UID) -g $(GID) xdfcopy $(BIN)
	install -c -s -m $(SPERM) -o $(UID) -g $(GID) fdmount $(BIN)
	install -c -s -m $(SPERM) -o $(UID) -g $(GID) fdmount $(BIN)/fdumount
	( cd $(BIN); \
	ln -sf xdfcopy $(BIN)/xdfformat; \
	ln -sf fdmount $(BIN)/fdumount; \
	ln -sf fdmount $(BIN)/fdlist; \
	ln -sf fdmount $(BIN)/fdmountd )

install.man:
	install -c -m $(MANPERM) -o $(UID) -g $(GID) $(MANPAGES1) $(MANDIR1)
	install -c -m $(MANPERM) -o $(UID) -g $(GID) $(MANPAGES4) $(MANDIR4)
	install -c -m $(MANPERM) -o $(UID) -g $(GID) $(MANPAGES8) $(MANDIR8)
	( cd $(MANDIR1); \
	ln -sf fdmount.1 $(MANDIR1)/fdumount.1; \
	ln -sf fdmount.1 $(MANDIR1)/fdlist.1; \
	ln -sf fdmount.1 $(MANDIR1)/fdmountd.1; \
	ln -sf xdfcopy.1 $(MANDIR1)/xdfformat.1 )

install.zman:
	for i in $(MANPAGES1); do gzip < $$i >_; install -c -m $(MANPERM) -o $(UID) -g $(GID) _ $(MANDIR1).Z/$$i; done
	for i in $(MANPAGES4); do gzip < $$i >_; install -c -m $(MANPERM) -o $(UID) -g $(GID) _ $(MANDIR4).Z/$$i; done
	for i in $(MANPAGES8); do gzip < $$i >_; install -c -m $(MANPERM) -o $(UID) -g $(GID) _ $(MANDIR8).Z/$$i; done
