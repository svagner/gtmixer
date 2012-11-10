# $FreeBSD: gtmixer/Makefile 


.include <bsd.own.mk>

PROG=	gtmixer
MAN=	gtmixer.1

GTK_CFLAGS !=   pkg-config --cflags gtk+-2.0
GTK_LDFLAGS !=  pkg-config --libs gtk+-2.0

.if defined(WITHOUT_NLS)
CFLAGS +=       -DWITHOUT_NLS 
.else
LDFLAGS +=      -L${PREFIX}/lib -lintl
.endif

CFLAGS +=       ${GTK_CFLAGS}
LDFLAGS +=      ${GTK_LDFLAGS}

.include <bsd.prog.mk>
