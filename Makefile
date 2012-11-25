include config.mk
PREFIX?=/usr/local
DESTDIR?=

all: logot frontend
	cd src && ${MAKE} all

frontend:
	-cd src/gui && ${MAKE} all

static: logot
	cd src && ${MAKE} static
	cd logotool && ${MAKE} static

allusb: logot
	cd src && ${MAKE} allusb

logot:
	cd logotool && ${MAKE} all

clean:
	cd src && ${MAKE} clean
	cd logotool && ${MAKE} clean

install:
	mkdir -p ${DESTDIR}${PREFIX}/bin
	mkdir -p ${DESTDIR}${PREFIX}/share/applications/
	cp src/0xFFFF ${DESTDIR}${PREFIX}/bin
	-cp src/gui/goxf ${DESTDIR}${PREFIX}/bin
	cp logotool/logotool ${DESTDIR}${PREFIX}/bin
	cp 0xFFFF.desktop ${DESTDIR}${PREFIX}/share/applications/

deinstall:
	rm -f ${DESTDIR}${PREFIX}/bin/0xFFFF
	rm -f ${DESTDIR}${PREFIX}/bin/logotool
	rm -f ${DESTDIR}${PREFIX}/bin/goxf
	rm ${DESTDIR}${PREFIX}/share/applications/0xFFFF.desktop
