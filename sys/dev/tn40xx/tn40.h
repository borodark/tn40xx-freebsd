
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

#ifndef __TN40_H__
#define __TN40_H__
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf_ring.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/module.h>
#include <sys/sockio.h>
#include <sys/eventhandler.h>
#include <sys/priv.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_arp.h>
#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/bpf.h>
#include <net/if_types.h>
#include <net/if_vlan_var.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <netinet/tcp_lro.h>
#include <netinet/udp.h>

#include <machine/in_cksum.h>

#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/clock.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/endian.h>
#include <sys/taskqueue.h>
#include <sys/pcpu.h>
#include <sys/smp.h>
#include <machine/smp.h>
#include <sys/sbuf.h>
#include "tn40_hw.h"
// #include "tn40_ioctl.h"

#define	CONTAINING_RECORD(addr, type, field) ((type *)((vm_offset_t)(addr) - (vm_offset_t)(&((type *)0)->field)))

#define	TN40_STATE_NONE			(0x00000000)
#define	TN40_STATE_ALLOC_MSI 	(0x00000001)
#define	TN40_STATE_ATTACH 		(0x00000002)
#define TN40_BR_SIZE			(4096)
typedef struct
{
	uint16_t vid;
	uint16_t pid;
	uint16_t subvendor;
	uint16_t subdev;
	char	 *name;
} tn40_devid_t;
#ifdef TN40_COUNTERS
typedef struct
{
	u64		tx_pkts;
	u64		tx_deferred_pkts;
	u64		tx_locked_pkts;
} tn40_counters_t;

#endif
typedef struct
{
	device_t 			dev;
	struct mtx			mtx;
	struct callout		callout;
	struct resource		*pci_mem;
	struct resource		*irq;
	struct ifmedia		media;
	bus_space_tag_t		bus_tag;
	bus_space_handle_t	bus_handle;
	void				*intr_handle;
	eventhandler_tag    vlan_attach;
	eventhandler_tag    vlan_detach;
	if_t			ifp;
	struct buf_ring		*br;
	struct mtx			tx_lock;
	struct task			txq_task;
	struct taskqueue 	*tq;
	struct bdx_priv		bdx_priv;
	uint32_t			state;
	struct proc			*poll_proc;
	volatile int		poll_running;
	int					link_reported;	/* last logged link: -1 unknown, 0 down, else speed */
#ifdef TN40_COUNTERS
	tn40_counters_t		counters;
#endif
}tn40_priv_t;

u32  tn40_read_reg			(struct bdx_priv *priv, u32 reg);
void tn40_write_reg			(struct bdx_priv *priv, u32 reg, u32 value);
int  tn40_alloc_dma			(struct bdx_priv *priv, u32 size, tn40_dma_t *dma);
int  tn40_dma_tag_create	(struct bdx_priv *priv, u32 size, u32 seg_size, int nsegments, bus_dma_tag_t *dma_tag);
void tn40_free_dma			(tn40_dma_t *dma);
void tn40_checksum_results	(struct mbuf *m, uint32_t pkt_type);
//
//uint16_t pci_get_vendor   	(device_t dev);
//uint16_t pci_get_device   	(device_t dev);
//uint16_t pci_get_subvendor	(device_t dev);
//uint16_t pci_get_subdevice	(device_t dev);
#if __FreeBSD_version < 1102000
#define ifr_data_get_ptr(ifr) ifr->ifr_data
#endif

#endif

