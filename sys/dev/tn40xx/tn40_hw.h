/*
 * Copyright (c) 2019 Tehuti Networks Ltd.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _TN40_HW_H_
#define _TN40_HW_H_

#include "tn40.h"
#include "tn40_ioctl.h"

#define TN40_DRV_NAME      			"tn40xx"
#define TN40_DRV_VERSION   			"1.2"
#define TN40_DRV_DESC      			"Tehuti FreeBSD Network Driver"

# define HTONS(x)    				(((x >> 8) & 0x00FF) | ((x << 8) & 0xFF00))

typedef	uint64_t					u64;
typedef	uint32_t					u32;
typedef	uint16_t					u16;
typedef	uint8_t						u8;
typedef	bus_addr_t					dma_addr_t;
#define	__iomem
#define __init
#define __exit
#define __initdata
#define ETHER_TYPE_IP				HTONS(0x800)
#define	VLAN_HLEN					(4)
#define VLAN_ETH_HLEN   			(ETHER_HDR_LEN + VLAN_HLEN)
#define TN40_MAX_PACKET_SIZE		(1 << 14)
#define TN40_MAX_TSO_PACKET_SIZE	(1 << 16)
#define TN40_MIN_PACKET_SIZE		(60)
#define SHORT_PKT_LEN				(64)
#define udelay(usec)				DELAY(usec)
#define mdelay(x)       			DELAY(x*1000)
#undef msleep
#define msleep(x)       			DELAY(1000*(x))
#define BYTES2PAGES(bytes) 			(((bytes) - 1) / PAGE_SIZE + 1)
#define ETHER_MIN_MTU				(68)
#define unlikely(x)					x
#define likely(x)					x
/*
 * Trace log
 */
#ifdef _TRACE_LOG_
#include "trace.h"
#else
#define traceInit()
#define traceOn()
#define traceOff()
#define traceOnce()
#define tracePrint()
#define traceAdd(loc, val)
#endif

/* Debugging Macros */

#define ERR(fmt, args...)  printf(TN40_DRV_NAME": "fmt, ## args)
#define MSG(fmt, args...)  printf(TN40_DRV_NAME": "fmt, ## args)

//#define BDX_ASSERT(x) BUG_ON(x)

//#define BDX_ASSERT(x)
#define BDX_ASSERT(x) if ((x)) printf(TN40_DRV_NAME" ASSERT at %s:%-5d\n", __func__, __LINE__)
#define TN40_ASSERT(x, fmt, args...) if (!(x)) printf(TN40_DRV_NAME" ASSERT : ""%s:%-5d: " fmt, __func__, __LINE__, ## args)

#define TN40_DEBUG
#define FTRACE
//#define REGLOG
#define WARNING

#ifdef WARNING
#define WRN(fmt, args...)   if (g_dbg)printf(TN40_DRV_NAME": " fmt,  ## args)
#else
#define WRN(fmt, args...)
#endif
//				M E M L O G
#ifdef TN40_MEMLOG
#include "memLog.h"
extern int g_memLog;
#define MEMLOG_ON							g_memLog = 1
#define MEMLOG1_ON							g_memLog = 2
#define MEMLOG_OFF							g_memLog = 0
//#define DBG(arg...)		                	if (g_memLog) memLog(TN40_DRV_NAME": " arg)

#else
#define MEMLOG_ON
#define MEMLOG1_ON
#define MEMLOG_OFF
#define memLog(args, ...)
#define memLogInit()
#define memLogDmesg()
#define memLogGetLine(x) (0)
#define memLogPrint()
#endif
//				D E B U G
#define	STRING_FMT							"%s"
#if defined(TN40_DEBUG)
extern int g_dbg;
#define DBG_ON								g_dbg = 1
#define DBG1_ON								g_dbg = 2
#define DBG2_ON								g_dbg = 3
#define DBG_OFF								g_dbg = 0
#else
#define DBG_ON
#define DBG_OFF
#endif

#if defined(TN40_DEBUG) && defined(TN40_MEMLOG)
#define DBG(fmt, args...)					if (g_memLog) 	  memLog(fmt, ##args); else if (g_dbg) 	 printf(TN40_DRV_NAME": ""%s:%-5d: " fmt, __func__, __LINE__, ## args)
#define DBG1(fmt, args...)		            if (g_memLog > 1) memLog(fmt, ##args); else if (g_dbg > 1) printf(TN40_DRV_NAME": ""%s:%-5d: " fmt, __func__, __LINE__, ## args)
#define DBG2(fmt, args...)		            if (g_memLog > 2) memLog(fmt, ##args); else if (g_dbg > 1) printf(TN40_DRV_NAME": ""%s:%-5d: " fmt, __func__, __LINE__, ## args)
#elif defined(TN40_MEMLOG)
#define DBG(fmt, args...)					if (g_memLog) 	  memLog(fmt, ##args)
#define DBG1(fmt, args...)		            if (g_memLog > 1) memLog(fmt, ##args)
#define DBG2(fmt, args...)		            if (g_memLog > 2) memLog(fmt, ##args)
#elif defined(TN40_DEBUG)
#define	DBG(fmt, args...)					if (g_dbg == 1)   printf(TN40_DRV_NAME": ""%s:%-5d: " fmt, __func__, __LINE__, ## args)
#define DBG1(fmt, args...)		            if (g_dbg == 2)   printf(TN40_DRV_NAME": ""%s:%-5d: " fmt, __func__, __LINE__, ## args)
#define DBG2(fmt, args...)		            if (g_dbg == 3)   printf(TN40_DRV_NAME": ""%s:%-5d: " fmt, __func__, __LINE__, ## args)
#define DMSG(fmt, args...)  				if (g_dbg == 1)	  printf(TN40_DRV_NAME": "fmt, ## args)
#else
#define DBG(fmt, args...)
#define DBG1(fmt, args...)
#define DBG2(fmt, args...)
#define DMSG(fmt, args...)
#endif

//				F T R A C E
#if defined(FTRACE)
extern int g_ftrace;
#define FTRACE_ON		g_ftrace = 1
#define FTRACE_OFF		g_ftrace = 0
//#define ENTER        do { if (g_ftrace) printf("%s:%-5d: - enter\n", __func__, __LINE__); } while (0)
//#define RET(args...) do { if (g_ftrace) printf("%s:%-5d: - return\n", __func__, __LINE__); return args; } while (0)
#define ENTER        do { if (g_ftrace) printf(TN40_DRV_NAME": %s: - enter\n" , __func__); } while (0)
#define RET(args...) do { if (g_ftrace) printf(TN40_DRV_NAME": %s: - return\n", __func__); return args; } while (0)
#define EXIT         do { if (g_ftrace) printf(TN40_DRV_NAME": %s(): - exit\n", __func__);return;} while (0)
#else
#define FTRACE_ON
#define FTRACE_OFF
#define ENTER
#define EXIT
#define RET(args...)   		                return args
#endif
#define BITS_PER_LONG 64
#define TN40_LITTLE_ENDIAN
#if BITS_PER_LONG == 64
#       define H32_64(x)  (u32) ((u64)(x) >> 32)
#       define L32_64(x)  (u32) ((u64)(x) & 0xffffffff)
#elif BITS_PER_LONG == 32
#       define H32_64(x)  0
#       define L32_64(x)  ((u32) (x))
#else               /* BITS_PER_LONG == ?? */
#       error BITS_PER_LONG is undefined. Must be 64 or 32
#endif              /* BITS_PER_LONG */
#define swab16 bswap16
#ifdef TN40_LITTLE_ENDIAN
#       define CPU_CHIP_SWAP32(x) (x)
#       define CPU_CHIP_SWAP16(x) (x)
#else
#       define CPU_CHIP_SWAP32(x) (SwapBytes32(x))
#       define CPU_CHIP_SWAP16(x) (SwapBytes16 (x))
#endif

#define TEHUTI_VID  			0x1FC9
#define DLINK_VID  				0x1186
#define ASUS_VID				0x1043
#define EDIMAX_VID				0x1432
#define PROMISE_VID				0x105a
#define BUFFALO_VID				0x1154

#define MDIO_SPEED_1MHZ 		(1)
#define MDIO_SPEED_6MHZ			(6)
#define TN40_MAX_PAGE_SIZE     	(0x40000)

enum LOAD_FW
{
	NO_FW_LOAD=0,
	FW_LOAD,
};

/* Supported PHYs */

/* #define PHY_MV88X3310 */
/* #define PHY_MV88E2010 */
#define PHY_MV88X3120
#define PHY_TLK10232
#define PHY_AQR105
#define PHY_QT2025
struct bdx_priv;
int MV88X3310_set_speed(struct bdx_priv *priv, signed int speed);
int MV88X3120_set_speed(struct bdx_priv *priv, signed int speed);
int TLK10232_set_speed (struct bdx_priv *priv, signed int speed);
int AQR105_set_speed   (struct bdx_priv *priv, signed int speed);
int QT2025_set_speed   (struct bdx_priv *priv, signed int speed);

#define AUTONEG_DISABLE			(0)
#define AUTONEG_ENABLE			(1)

enum PHY_TYPE
{
    PHY_TYPE_NA,      		/* Port not exists or unknown PHY	*/
    PHY_TYPE_CX4,     		/* PHY works without initialization */
    PHY_TYPE_QT2025,  		/* QT2025 10 Gbps SFP+    			*/
    PHY_TYPE_MV88X3120,  	/* MV88X3120  10baseT  				*/
    PHY_TYPE_MV88X3310,  	/* MV88X3310  10baseT  				*/
    PHY_TYPE_MV88E2010,  	/* MV88E2010  5NbaseT  				*/
    PHY_TYPE_TLK10232,		/* TI TLK10232 SFP+					*/
	PHY_TYPE_AQR105,		/* AQR105 10baseT					*/
    PHY_TYPE_CNT
};

enum PHY_LEDS_OP
{
    PHY_LEDS_SAVE,
    PHY_LEDS_RESTORE,
    PHY_LEDS_ON,
    PHY_LEDS_OFF,
    PHY_LEDS_CNT
};

enum TX_STATE
{
	TX_STATE_OK,
	TX_STATE_QUEUE_FULL
};

/* Driver states */

#define	BDX_STATE_NONE		(0x00000000)
#define	BDX_STATE_HW_STOPPED (0x00000001)
#define BDX_STATE_STARTED	(0x00000002)
#define BDX_STATE_OPEN		(0x00000004)

#define BDX_NDEV_TXQ_LEN    3000
#define FIFO_SIZE       	4096
#define FIFO_EXTRA_SPACE    1024
#define BDX_MAX_RX_DONE    	150
#define BDX_TXF_DESC_SZ    	16
#define BDX_MAX_TX_LEVEL   	(priv->txd_fifo0.m.memsz - 16)
#define BDX_MIN_TX_LEVEL   	256
#define BDX_NO_UPD_PACKETS 	40
#define BDX_MAX_MTU		   	(1 << 14)

#define PCK_TH_MULT   		128
#define INT_COAL_MULT 		2

#define BITS_MASK(nbits)            		((1 << nbits)-1)
#define GET_BITS_SHIFT(x, nbits, nshift)    (((x) >> nshift)&BITS_MASK(nbits))
#define BITS_SHIFT_MASK(nbits, nshift)      (BITS_MASK(nbits) << nshift)
#define BITS_SHIFT_VAL(x, nbits, nshift)    (((x) & BITS_MASK(nbits)) << nshift)
#define BITS_SHIFT_CLEAR(x, nbits, nshift)  ((x)&(~BITS_SHIFT_MASK(nbits, nshift)))

#define GET_INT_COAL(x)             GET_BITS_SHIFT(x, 15,  0)
#define GET_INT_COAL_RC(x)          GET_BITS_SHIFT(x,  1, 15)
#define GET_RXF_TH(x)               GET_BITS_SHIFT(x,  4, 16)
#define GET_PCK_TH(x)               GET_BITS_SHIFT(x,  4, 20)

#define INT_REG_VAL(coal, coal_rc, rxf_th, pck_th)  \
    ((coal) | ((coal_rc) << 15) | ((rxf_th) << 16) | ((pck_th) << 20))

typedef struct
{
	void			*va;
	bus_addr_t		pa;
	bus_dmamap_t	dma_map;
}tn40_dmamap_t;

typedef struct
{
	bus_dma_tag_t	dma_tag;
	tn40_dmamap_t	tn40_dmamap;
}tn40_dma_t;


struct fifo
{
    dma_addr_t  da;     	/* Physical address of fifo (used by HW) */
    char        *va;    	/* Virtual address of fifo (used by SW) */
    u32     	rptr, wptr; /* Cached values of RPTR and WPTR registers,
                   	   	   	   they're 32 bits on both 32 and 64 archs */
    u16     	reg_CFG0, reg_CFG1;
    u16     	reg_RPTR, reg_WPTR;
    u16     	memsz;  	/* Memory size allocated for fifo */
    u16     	size_mask;
    u16     	pktsz;  	/* Skb packet size to allocate */
    u16     	rcvno;  	/* Number of buffers that come from this RXF */
    tn40_dma_t	dma;
};

struct txf_fifo
{
    struct fifo m;   /* The minimal set of variables used by all fifos  */
};

struct txd_fifo
{
    struct fifo m;   /* The minimal set of variables used by all fifos */
};

struct rxf_fifo
{
    struct fifo m;   /* The minimal set of variables used by all fifos */
};

struct rxd_fifo
{
    struct fifo m;   /* The minimal set of variables used by all fifos */
};

struct rx_map
{
	bus_dmamap_t	dma_map;
    struct mbuf 	*mBuf;
};

struct rxdb
{
    int     		*stack;
    struct rx_map   *elems;
    int     		nelem;
    int     		top;
    tn40_dma_t		pkt;
    int				rxfSize;
    int				rxfAvail;
};

struct tx_map
{
	bus_dmamap_t	dma_map;
	int64_t			len;
    struct mbuf 	*mBuf;
};

/* tx database - implemented as circular fifo buffer*/
struct txdb
{
    struct tx_map   *start; /* Points to the first element     */
    struct tx_map   *end;   /* Points just AFTER the last element  */
    struct tx_map   *rptr;  /* Points to the next element to read  */
    struct tx_map   *wptr;  /* Points to the next element to write */
    int     size;   /* Number of elements in the db    */
};

/*Internal stats structure*/
struct bdx_stats
{
    u64 InUCast;                /* 0x7200 */
    u64 InMCast;                /* 0x7210 */
    u64 InBCast;                /* 0x7220 */
    u64 InPkts;         		/* 0x7230 */
    u64 InErrors;               /* 0x7240 */
    u64 InDropped;              /* 0x7250 */
    u64 FrameTooLong;       	/* 0x7260 */
    u64 FrameSequenceErrors;    /* 0x7270 */
    u64 InVLAN;         		/* 0x7280 */
    u64 InDroppedDFE;       	/* 0x7290 */
    u64 InDroppedIntFull;       /* 0x72A0 */
    u64 InFrameAlignErrors;     /* 0x72B0 */

    /* 0x72C0-0x72E0 RSRV */

    u64 OutUCast;               /* 0x72F0 */
    u64 OutMCast;               /* 0x7300 */
    u64 OutBCast;               /* 0x7310 */
    u64 OutPkts;                /* 0x7320 */

    /* 0x7330-0x7360 RSRV */

    u64 OutVLAN;                /* 0x7370 */
    u64 InUCastOctects;         /* 0x7380 */
    u64 OutUCastOctects;        /* 0x7390 */

    /* 0x73A0-0x73B0 RSRV */

    u64 InBCastOctects;         /* 0x73C0 */
    u64 OutBCastOctects;        /* 0x73D0 */
    u64 InOctects;              /* 0x73E0 */
    u64 OutOctects;             /* 0x73F0 */
};
#define PHY_LEDS				(4)
struct bdx_phy_operations
{
	unsigned short leds[PHY_LEDS];
    int  (*mdio_reset)  (struct bdx_priv *, int, unsigned short);
	u32  (*link_changed)(struct bdx_priv *);
	void (*ledset)      (struct bdx_priv *, enum PHY_LEDS_OP);
	int  (*get_settings)(struct bdx_priv *);
	int  (*set_settings)(struct bdx_priv *, int);

#ifdef _EEE_
    int  (*get_eee)		(struct net_device *, struct ethtool_eee *);
    int  (*set_eee)		(struct bdx_priv *);
    int  (*reset_eee)	(struct bdx_priv *);
#endif
    u32  mdio_speed;
};

//#define TN40_COUNTERS
#ifdef TN40_COUNTERS
typedef struct
{
	u64		tx_pkts;
	u64		tx_bytes;
	u64		tx_fixed_bytes;
	u64		tx_free_pkts;
	u64		tx_free_bytes;
} bdx_counters_t;

#define COUNT_INC(x, counter)		atomic_add_64(&(x.counter), 1)
#define COUNT_ADD(x, counter, n)	atomic_add_64(&(x.counter), n)
#else
#define COUNT_INC(x, counter)
#define COUNT_ADD(x, counter, n)
#endif
struct bdx_priv
{
    u8             				*pBdxRegs;
    struct rte_eth_dev 			*eth_dev;
//    struct napi_struct 	 		napi;
    /* RX FIFOs: 1 for data (full) descs, and 2 for free descs */
    struct rxd_fifo         	rxd_fifo0;
    struct rxf_fifo         	rxf_fifo0;
    struct rxdb     			*rxdb0;      /* Rx dbs to store skb pointers */
    int         				napi_stop;
    struct vlan_group   		*vlgrp;
    /* Tx FIFOs: 1 for data desc, 1 for empty (acks) desc */
    struct txd_fifo         	txd_fifo0;
    struct txf_fifo         	txf_fifo0;
    struct txdb     			txdb;
    int         				tx_level;
    int         				tx_update_mark;
    int         				tx_noupd;
    /* Rarely used */
    u8          				port;
    u8          				phy_mdio_port;
    enum PHY_TYPE           	phy_type;
    u32         				msg_enable;
    int         				stats_flag;
    struct bdx_stats   			hw_stats;
//    struct net_device_stats 	net_stats;
//    struct pci_dev          	*pdev;
//    struct pci_nic          	*nic;
    u8          				txd_size;
    u8          				txf_size;
    u8          				rxd_size;
    u8          				rxf_size;
    u32         				rdintcm;
    u32         				tdintcm;
    u16         				id;
    u16         				count;
//    struct timer_list   		blink_timer;
    u32  						isr_mask;
    int  						link_speed; // 0- no link or SPEED_100 SPEED_1000 SPEED_10000
    u32  						link_loop_cnt;
    u32							sfp_mod_type;
    struct bdx_phy_operations 	phy_ops;
    // SHORT_PKT_FIX
    tn40_dma_t					short_pkt;
	const char					*name;	/* device driver name */
	u32							errmsg_count;
	u32							state;
	u16							deviceId;
	u16							subsystem_vendor;
	u16							subsystem_device;
    u32							advertising, autoneg;
	u32							eee_enabled;
    u8 							mac_address[ETHER_ADDR_LEN];
	u16							mtu;
	u32							rx_buf_size;
	unsigned int 				socket_id;
	enum TX_STATE				tx_state;
	bus_dma_tag_t 				tx_dma_tag;
	bus_dma_tag_t 				rx_dma_tag;
#ifdef TN40_COUNTERS
	bdx_counters_t				counters;
#endif
};

#define SPEED_NOT_SUPPORTED		0
#define SPEED_AUTO				1
#define SPEED_10				10
#define SPEED_100		        100
#define SPEED_1000		        1000
#define SPEED_2500 				2500
#define SPEED_5000 				5000
#define SPEED_10000		        10000
#define SPEED_1000X 			(1001)
#define SPEED_100X 				(101)

#define MAX_ERRMSGS				(3)
/* RX FREE descriptor - 64bit*/

struct pbl
{
    				u32 pa_lo;
    				u32 pa_hi;
    				u32 len;
};

struct rxf_desc
{
    u32         	info;       /* Buffer Count + Info - described below */
    u32         	va_lo;      /* VAdr[31:0]    */
    u32         	va_hi;      /* VAdr[63:32]   */
    struct pbl      pbl[1]; 	/* DMA scatter gather 					 */
}__attribute__ ((packed));

#define GET_RXD_BC(x)       GET_BITS_SHIFT((x), 5, 0)
#define GET_RXD_RXFQ(x)     GET_BITS_SHIFT((x), 2, 8)
#define GET_RXD_TO(x)       GET_BITS_SHIFT((x), 1, 15)
#define GET_RXD_TYPE(x)     GET_BITS_SHIFT((x), 4, 16)
#define GET_RXD_ERR(x)      GET_BITS_SHIFT((x), 6, 21)
#define GET_RXD_RXP(x)      GET_BITS_SHIFT((x), 1, 27)
#define GET_RXD_PKT_ID(x)   GET_BITS_SHIFT((x), 3, 28)
#define GET_RXD_VTAG(x)     GET_BITS_SHIFT((x), 1, 31)
#define GET_RXD_VLAN_ID(x)  GET_BITS_SHIFT((x), 12, 0)
#define GET_RXD_VLAN_TCI(x) GET_BITS_SHIFT((x), 16, 0)
#define GET_RXD_CFI(x)      GET_BITS_SHIFT((x), 1, 12)
#define GET_RXD_PRIO(x)     GET_BITS_SHIFT((x), 3, 13)

#define GET_RSS_FUNC(x)     GET_BITS_SHIFT((x), 2, 0)
#define GET_RSS_TYPE(x)     GET_BITS_SHIFT((x), 8, 8)
#define GET_RSS_TCPU(x)     GET_BITS_SHIFT((x), 8, 16)

enum
{
	PKT_TYPE_NON_IP 	= 0,
	PKT_TYPE_TCP  		= 1,
	PKT_TYPE_UDP  		= 2,
	PKT_TYPE_IP   		= 3,		// Not TCP or UDP
	PKT_TYPE_RESERVED	= 4,
	PKT_TYPE_TCPV6  	= 5,
	PKT_TYPE_UDPV6  	= 6,
	PKT_TYPE_IPV6  		= 7,		// Not TCP or UDP
};

typedef struct eth_header
{
	u8  		dstAddr[ETHER_ADDR_LEN];
	u8  		srcAddr[ETHER_ADDR_LEN];
	u16 		type;
}  __attribute__ ((packed)) ETHER_HEADER;

struct ip_header
{
	u8			ihl:4, version:4;
	u8			ToS;
	u16			tLen;
	u16			id;
	u16			fragOff;
	u8			TTL;
	u8			protocol;
	u16			checkSum;
	u32			srcAddr;
	u32			dstAddr;
}__attribute__ ((packed));

struct rxd_desc
{
    u32         rxd_val1;
    u16         len;
    u16         rxd_vlan;
    u32         va_lo;
    u32         va_hi;
    u32         rss_lo;
    u32         rss_hash;
};

#define MAX_SEGMENTS			(64)
#define MAX_PBL					(19)
/* PBL describes each virtual buffer to be transmitted from the host. */


/*
 * First word for TXD descriptor. It means: type = 3 for regular Tx packet,
 * hw_csum = 7 for IP+UDP+TCP HW checksums.
 */

#define TXD_W1_VAL(bc, checksum, vtag, lgsnd, vlan_id)  \
    ((bc) | ((checksum) << 5)  | ((vtag) << 8) |    \
    ((lgsnd) << 9) | (0x30000) | ((vlan_id & 0x0fff) << 20) | (((vlan_id >> 13) & 7) << 13))

struct txd_desc
{
    u32             txd_val1;
    u16             mss;
    u16             length;
    u32             va_lo;
    u32             va_hi;
    struct pbl      pbl[0]; /* Fragments */
} __attribute__ ((packed));

struct txf_desc
{
    u32             status;
    u32             va_lo;      /* VAdr[31:0]    						 */
    u32             va_hi;      /* VAdr[63:32]   						 */
    u32				pad;
}__attribute__ ((packed));

#define READ_REG(pp, reg)     	tn40_read_reg (pp, reg)
#define WRITE_REG(pp, reg, val) tn40_write_reg(pp, reg, val)

/* Register region size */
#define BDX_REGS_SIZE           0x10000

/* Registers from 0x0000-0x00fc were remapped to 0x4000-0x40fc */
#define regTXD_CFG1_0           0x4000
#define regTXD_CFG1_1           0x4004
#define regTXD_CFG1_2           0x4008
#define regTXD_CFG1_3           0x400C

#define regRXF_CFG1_0           0x4010
#define regRXF_CFG1_1           0x4014
#define regRXF_CFG1_2           0x4018
#define regRXF_CFG1_3           0x401C

#define regRXD_CFG1_0           0x4020
#define regRXD_CFG1_1           0x4024
#define regRXD_CFG1_2           0x4028
#define regRXD_CFG1_3           0x402C

#define regTXF_CFG1_0           0x4030
#define regTXF_CFG1_1           0x4034
#define regTXF_CFG1_2           0x4038
#define regTXF_CFG1_3           0x403C

#define regTXD_CFG0_0           0x4040
#define regTXD_CFG0_1           0x4044
#define regTXD_CFG0_2           0x4048
#define regTXD_CFG0_3           0x404C

#define regRXF_CFG0_0           0x4050
#define regRXF_CFG0_1           0x4054
#define regRXF_CFG0_2           0x4058
#define regRXF_CFG0_3           0x405C

#define regRXD_CFG0_0           0x4060
#define regRXD_CFG0_1           0x4064
#define regRXD_CFG0_2           0x4068
#define regRXD_CFG0_3           0x406C

#define regTXF_CFG0_0           0x4070
#define regTXF_CFG0_1           0x4074
#define regTXF_CFG0_2           0x4078
#define regTXF_CFG0_3           0x407C

#define regTXD_WPTR_0           0x4080
#define regTXD_WPTR_1           0x4084
#define regTXD_WPTR_2           0x4088
#define regTXD_WPTR_3           0x408C

#define regRXF_WPTR_0           0x4090
#define regRXF_WPTR_1           0x4094
#define regRXF_WPTR_2           0x4098
#define regRXF_WPTR_3           0x409C

#define regRXD_WPTR_0           0x40A0
#define regRXD_WPTR_1           0x40A4
#define regRXD_WPTR_2           0x40A8
#define regRXD_WPTR_3           0x40AC

#define regTXF_WPTR_0           0x40B0
#define regTXF_WPTR_1           0x40B4
#define regTXF_WPTR_2           0x40B8
#define regTXF_WPTR_3           0x40BC

#define regTXD_RPTR_0           0x40C0
#define regTXD_RPTR_1           0x40C4
#define regTXD_RPTR_2           0x40C8
#define regTXD_RPTR_3           0x40CC

#define regRXF_RPTR_0           0x40D0
#define regRXF_RPTR_1           0x40D4
#define regRXF_RPTR_2           0x40D8
#define regRXF_RPTR_3           0x40DC

#define regRXD_RPTR_0           0x40E0
#define regRXD_RPTR_1           0x40E4
#define regRXD_RPTR_2           0x40E8
#define regRXD_RPTR_3           0x40EC

#define regTXF_RPTR_0           0x40F0
#define regTXF_RPTR_1           0x40F4
#define regTXF_RPTR_2           0x40F8
#define regTXF_RPTR_3           0x40FC

/* Hardware versioning */
#define  FW_VER             	0x5010
#define  SROM_VER           	0x5020
#define  FPGA_VER           	0x5030
#define  FPGA_SEED          	0x5040

/* Registers from 0x0100-0x0150 were remapped to 0x5100-0x5150 */
#define regISR regISR0
#define regISR0             	0x5100

#define regIMR regIMR0
#define regIMR0             	0x5110

#define regRDINTCM0         	0x5120
#define regRDINTCM2         	0x5128

#define regTDINTCM0         	0x5130

#define regISR_MSK0         	0x5140

#define regINIT_SEMAPHORE       0x5170
#define regINIT_STATUS          0x5180

#define regMAC_LNK_STAT         0x0200
#define MAC_LINK_STAT           0x0004      /* Link state */

#define regBLNK_LED         	0x0210

#define regGMAC_RXF_A           0x1240

#define regUNC_MAC0_A           0x1250
#define regUNC_MAC1_A           0x1260
#define regUNC_MAC2_A           0x1270

#define regVLAN_0           	0x1800

#define regMAX_FRAME_A          0x12C0

#define regRX_MAC_MCST0         0x1A80
#define regRX_MAC_MCST1         0x1A84
#define MAC_MCST_NUM            15
#define regRX_MCST_HASH0        0x1A00
#define MAC_MCST_HASH_NUM       8

#define regVPC              	0x2300
#define regVIC              	0x2320
#define regVGLB             	0x2340

#define regCLKPLL           	0x5000

/* MDIO interface */

#define regMDIO_CMD_STAT		0x6030
#define regMDIO_CMD				0x6034
#define regMDIO_DATA			0x6038
#define regMDIO_ADDR			0x603C
#define GET_MDIO_BUSY(x)		GET_BITS_SHIFT(x, 1, 0)
#define GET_MDIO_RD_ERR(x)		GET_BITS_SHIFT(x, 1, 1)

/*for 10G only*/
#define regRSS_CNG      		0x000000b0

#define RSS_ENABLED     		0x00000001
#define RSS_HFT_TOEPLITZ    	0x00000002
#define RSS_HASH_IPV4       	0x00000100
#define RSS_HASH_TCP_IPV4   	0x00000200
#define RSS_HASH_IPV6       	0x00000400
#define RSS_HASH_IPV6_EX    	0x00000800
#define RSS_HASH_TCP_IPV6   	0x00001000
#define RSS_HASH_TCP_IPV6_EX    0x00002000

#define regRSS_HASH_BASE        0x0400
#define RSS_HASH_LEN            40
#define regRSS_INDT_BASE        0x0600
#define RSS_INDT_LEN            256

#define regREVISION         	0x6000
#define regSCRATCH          	0x6004
#define regCTRLST           	0x6008
#define regMAC_ADDR_0           0x600C
#define regMAC_ADDR_1           0x6010
#define regFRM_LENGTH           0x6014
#define regPAUSE_QUANT          0x6054
#define regRX_FIFO_SECTION      0x601C
#define regTX_FIFO_SECTION      0x6020
#define regRX_FULLNESS          0x6024
#define regTX_FULLNESS          0x6028
#define regHASHTABLE            0x602C

#define regRST_PORT         	0x7000
#define regDIS_PORT         	0x7010
#define regRST_QU           	0x7020
#define regDIS_QU           	0x7030

#define regCTRLST_TX_ENA        0x0001
#define regCTRLST_RX_ENA        0x0002
#define regCTRLST_PRM_ENA       0x0010
#define regCTRLST_PAD_ENA       0x0020

#define regCTRLST_BASE          (regCTRLST_PAD_ENA|regCTRLST_PRM_ENA)

#define regRX_FLT               0x1400

/* TXD TXF RXF RXD  CONFIG 0x0000 --- 0x007c*/
#define  TX_RX_CFG1_BASE    	0xffffffff      /*0-31 */
#define  TX_RX_CFG0_BASE    	0xfffff000      /*31:12 */
#define  TX_RX_CFG0_RSVD    	0x00000ffc      /*11:2 */
#define  TX_RX_CFG0_SIZE    	0x00000003      /*1:0 */

/*  TXD TXF RXF RXD  WRITE 0x0080 --- 0x00BC */
#define  TXF_WPTR_WR_PTR    	0x00007ff8      /*14:3 */

/*  TXD TXF RXF RXD  READ  0x00CO --- 0x00FC */
#define  TXF_RPTR_RD_PTR    	0x00007ff8     /*14:3 */

#define TXF_WPTR_MASK 0x7ff0    /* The last 4 bits are dropped
* size is rounded to 16 */

/*  regISR 0x0100 */
/*  regIMR 0x0110 */
#define  IMR_INPROG     		0x80000000    /*31 */
#define  IR_LNKCHG1     		0x10000000    /*28 */
#define  IR_LNKCHG0     		0x08000000    /*27 */
#define  IR_GPIO        		0x04000000    /*26 */
#define  IR_RFRSH       		0x02000000    /*25 */
#define  IR_RSVD        		0x01000000    /*24 */
#define  IR_SWI         		0x00800000    /*23 */
#define  IR_RX_FREE_3   		0x00400000    /*22 */
#define  IR_RX_FREE_2   		0x00200000    /*21 */
#define  IR_RX_FREE_1   		0x00100000    /*20 */
#define  IR_RX_FREE_0   		0x00080000    /*19 */
#define  IR_TX_FREE_3   		0x00040000    /*18 */
#define  IR_TX_FREE_2   		0x00020000    /*17 */
#define  IR_TX_FREE_1   		0x00010000    /*16 */
#define  IR_TX_FREE_0   		0x00008000    /*15 */
#define  IR_RX_DESC_3   		0x00004000    /*14 */
#define  IR_RX_DESC_2   		0x00002000    /*13 */
#define  IR_RX_DESC_1   		0x00001000    /*12 */
#define  IR_RX_DESC_0   		0x00000800    /*11 */
#define  IR_PSE         		0x00000400    /*10 */
#define  IR_TMR3        		0x00000200    /* 9 */
#define  IR_TMR2        		0x00000100    /* 8 */
#define  IR_TMR1        		0x00000080    /* 7 */
#define  IR_TMR0        		0x00000040    /* 6 */
#define  IR_VNT         		0x00000020    /* 5 */
#define  IR_RxFL        		0x00000010    /* 4 */
#define  IR_SDPERR      		0x00000008    /* 3 */
#define  IR_TR          		0x00000004    /* 2 */
#define  IR_PCIE_LINK   		0x00000002    /* 1 */
#define  IR_PCIE_TOUT   		0x00000001    /* 0 */

#define  IR_EXTRA (IR_RX_FREE_0 | IR_LNKCHG0 | IR_LNKCHG1 | IR_PSE | \
    IR_TMR0 | IR_PCIE_LINK | IR_PCIE_TOUT)
#define  IR_RUN (IR_EXTRA | IR_RX_DESC_0 | IR_TX_FREE_0)
#define  IR_ALL 0xfdfffff7

#define  IR_LNKCHG0_ofst    27

#define  GMAC_RX_FILTER_OSEN    0x1000  /* shared OS enable */
#define  GMAC_RX_FILTER_TXFC    0x0400  /* Tx flow control */
#define  GMAC_RX_FILTER_RSV0    0x0200  /* reserved */
#define  GMAC_RX_FILTER_FDA     0x0100  /* filter out direct address */
#define  GMAC_RX_FILTER_AOF     0x0080  /* accept over run */
#define  GMAC_RX_FILTER_ACF     0x0040  /* accept control frames */
#define  GMAC_RX_FILTER_ARUNT   0x0020  /* accept under run */
#define  GMAC_RX_FILTER_ACRC    0x0010  /* accept crc error */
#define  GMAC_RX_FILTER_AM      0x0008  /* accept multicast */
#define  GMAC_RX_FILTER_AB      0x0004  /* accept broadcast */
#define  GMAC_RX_FILTER_PRM     0x0001  /* [0:1] promiscuous mode */

#define  MAX_FRAME_AB_VAL   	0x3fff  /* 13:0 */

#define  CLKPLL_PLLLKD      	0x0200  /* 9 */
#define  CLKPLL_RSTEND      	0x0100  /* 8 */
#define  CLKPLL_SFTRST      	0x0001  /* 0 */

#define  CLKPLL_LKD     		(CLKPLL_PLLLKD|CLKPLL_RSTEND)

/*
 * PCI-E Device Control Register (Offset 0x88)
 * Source: Luxor Data Sheet, 7.1.3.3.3
 */
#define PCI_DEV_CTRL_REG 0x88
#define GET_DEV_CTRL_MAXPL(x)   GET_BITS_SHIFT(x, 3, 5)
#define GET_DEV_CTRL_MRRS(x)    GET_BITS_SHIFT(x, 3, 12)

/*
 * PCI-E Link Status Register (Offset 0x92)
 * Source: Luxor Data Sheet, 7.1.3.3.7
 */
#define PCI_LINK_STATUS_REG 	0x92
#define GET_LINK_STATUS_LANES(x) GET_BITS_SHIFT(x, 6, 4)
#if !defined(ROUND_UP)
#define ROUND_UP(n, m)  (((n) + (m - 1)) & ~(m - 1))
#endif
int bdx_mdio_write(struct bdx_priv *priv, int device, int port, u16 addr, u16 data);
int bdx_mdio_read(struct bdx_priv *priv, int device, int port, u16 addr);

#define PHY_MDIO_READ(priv,device,addr) bdx_mdio_read(priv,(device),(priv)->phy_mdio_port,(addr))
#define PHY_MDIO_WRITE(priv,device,addr,data) bdx_mdio_write((priv),(device),(priv)->phy_mdio_port,    (addr), (data))

#define  BDX_MDIO_WRITE(priv, space, addr, val) 					\
    if (bdx_mdio_write((priv), (space), port, (addr), (val))) {     \
        ERR("bdx: failed to write  to phy at %x.%x val %x\n", 		\
                    (space),(addr),(val));                          \
        return 1;                                             		\
    }
enum PHY_TYPE MV88X3310_register(struct bdx_priv *priv);
enum PHY_TYPE MV88X3120_register(struct bdx_priv *priv);
enum PHY_TYPE QT2025_register	(struct bdx_priv *priv);
enum PHY_TYPE CX4_register		(struct bdx_priv *priv);
enum PHY_TYPE TLK10232_register	(struct bdx_priv *priv);
enum PHY_TYPE AQR105_register	(struct bdx_priv *priv);

void bdx_link_changed			(struct bdx_priv *priv);
int  bdx_probe					(struct bdx_priv *priv);
void bdx_speed_changed			(struct bdx_priv *priv, u32 speed);
int  bdx_mdio_read				(struct bdx_priv *priv, int device, int port, u16 addr);
int  bdx_mdio_write				(struct bdx_priv *priv, int device, int port, u16 addr, u16 data);
int  bdx_tx_init				(struct bdx_priv *priv);
int  bdx_rx_init				(struct bdx_priv *priv);
void bdx_rx_free				(struct bdx_priv *priv);
void bdx_tx_free				(struct bdx_priv *priv);
int  bdx_open					(struct bdx_priv *priv);
int  bdx_close					(struct bdx_priv *priv);
int  bdx_set_mac				(struct bdx_priv *priv, u8 *addr);
int  bdx_change_mtu				(struct bdx_priv *priv, int new_mtu);
int  bdx_tx_transmit			(struct bdx_priv *priv, struct mbuf *mBuf);
void bdx_enable_interrupts		(struct bdx_priv *priv);
void bdx_disable_interrupts		(struct bdx_priv *priv);
void bdx_update_stats			(struct bdx_priv *priv);
u16  bdx_get_rxbuf_size			(struct bdx_priv *priv);
u16  bdx_rx_pkt_size			(struct bdx_priv *priv);
int  bdx_get_status				(struct bdx_priv *priv);
int  bdx_speed_set				(struct bdx_priv *priv, u32 speed);
void bdx_remove					(struct bdx_priv *priv);
void bdx_setbroadcast			(struct bdx_priv *priv, int flags);
void bdx_setmulti				(struct bdx_priv *priv);
int  bdx_ioctl_priv				(struct bdx_priv *priv, struct ifreq *ifr);
void bdx_rx_receive				(struct bdx_priv *priv);
void bdx_tx_cleanup				(struct bdx_priv *priv);
void bdx_link_changed			(struct bdx_priv *priv);
int  bdx_isr					(struct bdx_priv *priv);
u32  bdx_mdio_get 				(struct bdx_priv *priv);
void bdx_vlan_rx_vid			(struct bdx_priv *priv, uint16_t vid, int enable);

#endif /* _TN40_HW_H_ */

