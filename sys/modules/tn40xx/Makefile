# $FreeBSD$

.PATH: ${SRCTOP}/sys/dev/tn40xx

KMOD=  tn40xx
SRCS=  tn40.c tn40_hw.c tn40_mbuf.c CX4_FreeBSD.c CX4.c  \
	AQR105_phy_FreeBSD.c AQR105_phy.c QT2025_phy_FreeBSD.c QT2025_phy.c \
	TLK10232_phy_FreeBSD.c TLK10232_phy.c \
	MV88X3120_phy_FreeBSD.c MV88X3120_phy.c
SRCS+= device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>

