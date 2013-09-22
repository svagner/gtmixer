.include <bsd.own.mk>

PROG=	gtmixer
MAN=	gtmixer.1
SRCS=	device_func.c config.c gtmixer.c# ini.c ini.h

GTK_CFLAGS !=   pkg-config --cflags gtk+-2.0
GTK_LDFLAGS !=  pkg-config --libs gtk+-2.0

#INI_CFLAGS +=	-DINI_ALLOW_MULTILINE=0 -DINI_ALLOW_BOM=0 -DINI_USE_STACK=0 -DINI_MAX_LINE=1000

.if defined(WITHOUT_NLS)
CFLAGS +=       -DWITHOUT_NLS
.else
LDFLAGS +=      -L${PREFIX}/lib -lintl
.endif

.if defined(DEBUG)
#INI_CFLAGS +=	-DDEBUG=1 
CFLAGS +=	-dgdb
.endif

.if defined(GIT_VERSION)
CFLAGS +=	-DGIT_VERSION="\"1aahjghgjhvjh\""
.endif

CFLAGS +=       ${GTK_CFLAGS} ${INI_CFLAGS}
LDFLAGS +=      ${GTK_LDFLAGS}

.include <bsd.prog.mk>
