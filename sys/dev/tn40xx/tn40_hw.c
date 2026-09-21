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

#include "tn40.h"
#include "tn40_fw.h"
#include "tn40_mbuf.h"

MALLOC_DEFINE(M_TN40, "tn40", "tn40");

u32 bdx_force_no_phy_mode = 0;


/* Definitions needed by ISR  */
static int 	bdx_rx_alloc_buffers(struct bdx_priv *priv);
/* Definitions needed by FW loading */
static void bdx_tx_push_desc_safe(struct bdx_priv *priv, void *data, int size);

/* Definitions needed by bdx_probe */
#ifdef _DRIVER_RESUME_
//static int bdx_suspend(struct pci_dev *pdev, pm_message_t state);
//static int bdx_resume (struct pci_dev *pdev);
static int bdx_suspend(struct device *dev);
static int bdx_resume(struct device *dev);
#endif
//#define USE_RSS
#if defined(USE_RSS)
/* bdx_init_rss - Initialize RSS hash HW function.
 *
 * @priv       - NIC private structure
 */
#if defined(RHEL_RELEASE_CODE)
#define prandom_seed(seed)
#define prandom_u32 random32
#endif
static int bdx_init_rss(struct bdx_priv *priv)
{
    int i;
    u32 seed;

    /* Disable RSS before starting the configuration */

    WRITE_REG(priv, regRSS_CNG, 0);
    /*
     * Notes:
     *      - We do not care about the CPU, we just need the hash value, the
     *        Linux kernel is doing the rest of the work for us. We set the
     *        CPU table length to 0.
     *
     *      - We use random32() to initialize the Toeplitz secret key. This is
     *        probably not cryptographically secure way but who cares.
     */

    /* UPDATE THE HASH SECRET KEY */
    seed = (uint32_t_t)(0xFFFFFFFF & jiffies);
    prandom_seed(seed);
    for (i =0; i < 4 * RSS_HASH_LEN; i += 4)
    {
    	u32 rnd = prandom_u32();
        WRITE_REG(priv, regRSS_HASH_BASE + 4 * i, rnd);
        DBG("bdx_init_rss() rnd 0x%x\n", rnd);
    }
    WRITE_REG(priv, regRSS_CNG,
              RSS_ENABLED   | RSS_HFT_TOEPLITZ  |
              RSS_HASH_IPV4 | RSS_HASH_TCP_IPV4 |
              RSS_HASH_IPV6 | RSS_HASH_TCP_IPV6);

    DBG("regRSS_CNG =%x\n", READ_REG(priv, regRSS_CNG));
    RET(0);
}
#else
#define    bdx_init_rss(priv)
#endif

#if defined(TN40_DEBUG)
struct
{
	u32	rx;
	u32	tx;

}g_dbgCounters = {0,0};

int g_dbg=0;
#endif
#if defined(TN40_FTRACE)
int g_ftrace=0;
#endif
#if defined(TN40_REGLOG)
int g_regLog=0;
#endif
#if defined (TN40_MEMLOG)
int g_memLog=0;
#endif

//-------------------------------------------------------------------------------------------------

static struct
{
    u16 bytes;
    u16 qwords;     /* qword = 64 bit */
} txd_sizes[MAX_PBL];

static struct
{
    u16 bytes;
    u16 qwords;     /* qword = 64 bit */
} rxd_sizes[MAX_PBL];

#if defined(TN40_DEBUG)

//-------------------------------------------------------------------------------------------------

static void dbg_printFifo(struct bdx_priv *priv, struct fifo *m, const char *fName)
{
	DBG("%s fifo:\n", fName);
	DBG("WPTR (0x%x) 0x%x RPTR (0x%x) 0x%x\n",m->reg_WPTR, m->wptr, m->reg_RPTR, m->rptr);
    DBG("READ_REG 0x%04x m->reg_WPTR=0x%x\n", m->reg_WPTR, READ_REG(priv, m->reg_WPTR));
    DBG("READ_REG 0x%04x m->reg_RPTR=0x%x\n", m->reg_RPTR, READ_REG(priv, m->reg_RPTR));
    DBG("f->va %p f->da %p\n", m->va, (void*)m->da);
    DBG("READ_REG 0x%04x m->reg_CFG0=0x%x\n", m->reg_CFG0, READ_REG(priv, m->reg_CFG0));
    DBG("READ_REG 0x%04x m->reg_CFG1=0x%x\n", m->reg_CFG1, READ_REG(priv, m->reg_CFG1));

} // dbg_printFifo()

//-------------------------------------------------------------------------------------------------

static void dbg_printRegs(struct bdx_priv *priv, char *msg)
{
	ENTER;
	DBG("* %s * \n", msg);
	DBG("~~~~~~~~~~~~~\n");
	DBG("veneto:");
	DBG("pc = 0x%x li = 0x%x ic = %d\n", READ_REG(priv, 0x2300), READ_REG(priv, 0x2310), READ_REG(priv, 0x2320));
	dbg_printFifo(priv, &priv->txd_fifo0.m, (const char *)"TXD");
	dbg_printFifo(priv, &priv->rxf_fifo0.m, (const char *)"RXF");
	dbg_printFifo(priv, &priv->rxd_fifo0.m, (const char *)"RXD");
	DBG("~~~~~~~~~~~~~\n");

	EXIT;

} // dbg_printRegs()

//-------------------------------------------------------------------------------------------------

static void dbg_printPBL(struct pbl *pbl)
{
	DBG("pbl: len %u pa_lo 0x%x pa_hi 0x%x\n", pbl->len, pbl->pa_lo, pbl->pa_hi);

} // dbg_printPBL()

//-------------------------------------------------------------------------------------------------

static void dbg_print_rxdd(struct rxd_desc *rxdd, u32 rxd_val1, u16 len, u16 rxd_vlan)
{
    DBG("rxdd bc %d rxfq %d to %d type %d err %d rxp %d "
        "pkt_id %d vtag %d len %d vlan_id %d cfi %d prio %d "
        "va_lo %d va_hi %d\n",
        GET_RXD_BC(rxd_val1), GET_RXD_RXFQ(rxd_val1), GET_RXD_TO(rxd_val1),
        GET_RXD_TYPE(rxd_val1), GET_RXD_ERR(rxd_val1),
        GET_RXD_RXP(rxd_val1), GET_RXD_PKT_ID(rxd_val1),
        GET_RXD_VTAG(rxd_val1), len, GET_RXD_VLAN_ID(rxd_vlan),
        GET_RXD_CFI(rxd_vlan), GET_RXD_PRIO(rxd_vlan), rxdd->va_lo,
        rxdd->va_hi);

} // dbg_dbg_print_rxdd()

//-------------------------------------------------------------------------------------------------

static void dbg_print_rxfd(struct rxf_desc *rxfd)
{
	DBG("info 0x%x va_hi 0x%x va_lo %u\n",
			rxfd->info, rxfd->va_hi, rxfd->va_lo);

} // dbg_print_rxfd()

//-------------------------------------------------------------------------------------------------

static void dbg_printIoctl(void)
{
	MSG("DBG_ON %d, DBG_OFF %d, DBG_SUSPEND %d, DBG_RESUME %d DBG_PRINT_PAGE_TABLE %d DBG_PRINT_COUNTERS %d DBG_CLEAR_COUNTERS %d\n",
		DBG_START_DBG, DBG_STOP_DBG, DBG_SUSPEND, DBG_RESUME, DBG_PRINT_PAGE_TABLE, DBG_PRINT_COUNTERS, DBG_CLEAR_COUNTERS);

} // dbg_printIoctl

//-------------------------------------------------------------------------------------------------
static void dbg_printMbuf(struct mbuf *m, int line) __attribute__((unused));
static void dbg_printMbuf(struct mbuf *m, int line)
{
	int	j = 1;
	if (g_dbg == 1)
	{
		MSG("%d mbuf:\n", line);
		while (m)
		{
			MSG("  %d. m %p nxt %p nxtp %p dat %p len %d %d typ 0x%x flgs 0x%x\n", j,
				m, m->m_next, m->m_nextpkt, m->m_data, m->m_len,
				(m->m_flags & M_PKTHDR) ? m->m_pkthdr.len: 0, m->m_type, m->m_flags);
			if (m->m_flags & M_EXT)
			{
				MSG("  xbuf %p xsize %d xtyp 0x%x xflgs 0x%x csum 0x%x\n",
						m->m_ext.ext_buf, m->m_ext.ext_size, m->m_ext.ext_type, m->m_ext.ext_flags, (uint32_t)m->m_pkthdr.csum_flags);
			}
			m = m->m_next;
			j += 1;
		}
	}
} // dbg_printMbuf()

//-------------------------------------------------------------------------------------------------
void dbg_printSges(bus_dma_segment_t *segs, int len) __attribute__((unused));
void dbg_printSges(bus_dma_segment_t *segs, int len)
{
	int			j			= 1;
	uint64_t 	prevAddrEnd	= 0;
	if (g_dbg == 1)
	{
		MSG("Segs:\n");
		for (j = 0; j <= len; j++)
		{
			MSG("  0x%08lx  %lu %s\n", segs[j].ds_addr, segs[j].ds_len,
					(segs[j].ds_addr == prevAddrEnd)? " *" : "");
			prevAddrEnd = segs[j].ds_addr + segs[j].ds_len;
		}

	}

} // dbg_printSges()

//-------------------------------------------------------------------------------------------------
static void dbg_printRxPkt(struct mbuf *mBuf, char *pkt, u16 len) __attribute__((unused));
static void dbg_printRxPkt(struct mbuf *mBuf, char *pkt, u16 len)
{
	int i;

	if (g_dbg == 1)
	{
		if (pkt == NULL)
		{
			pkt = mBuf->m_data;
		}
		MSG("RX: len=%d\n",len);
		if (pkt)
		{
			for(i=0; i<len; i=i+16)
			{
				MSG("%.02x %.02x %.02x %.02x %.02x %.02x %.02x %.02x %.02x %.02x %.02x %.02x %.02x %.02x %.02x %.02x\n",(0xff&pkt[i]),(0xff&pkt[i+1]),(0xff&pkt[i+2]),(0xff&pkt[i+3]),(0xff&pkt[i+4]),(0xff&pkt[i+5]),(0xff&pkt[i+6]),(0xff&pkt[i+7]),(0xff&pkt[i+8]),(0xff&pkt[i+9]),(0xff&pkt[i+10]),(0xff&pkt[i+11]),(0xff&pkt[i+12]),(0xff&pkt[i+13]),(0xff&pkt[i+14]),(0xff&pkt[i+15]));
				msleep(4);
			}
		}
	}

} // dbg_printRxPkt()

//-------------------------------------------------------------------------------------------------

#else
#define dbg_printRegs(priv, msg)
#define dbg_printPBL(pbl)
#define dbg_printFifo(priv, m, fName)
#define dbg_print_rxfd(rxfd)
#define dbg_printIoctl()
#define dbg_printMbuf(mBuf, line)
#define dbg_printRxPkt(mBuf, pkt, len)
#endif

#if defined(FTRACE)
int g_ftrace = 0;
#endif

#ifdef TN40_COUNTERS

static void bdx_printCounters(struct bdx_priv *priv)
{
    tn40_priv_t		*tn40_priv = CONTAINING_RECORD(priv, tn40_priv_t, bdx_priv);

	MSG("tn40.tx_pkts          %lu\n", tn40_priv->counters.tx_pkts);
	MSG("tn40.tx_deferred_pkts %lu\n", tn40_priv->counters.tx_deferred_pkts);
	MSG("tn40.tx_locked_pkts   %lu\n", tn40_priv->counters.tx_locked_pkts);
	MSG("bdx.tx_pkts           %lu\n", priv->counters.tx_pkts);
	MSG("bdx.tx_bytes          %lu\n", priv->counters.tx_bytes);
	MSG("bdx.tx_fixed_bytes    %lu\n", priv->counters.tx_fixed_bytes);
	MSG("bdx.tx_free_pkts      %lu\n", priv->counters.tx_free_pkts);
	MSG("bdx.tx_free_bytes     %lu\n", priv->counters.tx_free_bytes);

} // bdx_printCounters()

//-------------------------------------------------------------------------------------------------

static void bdx_clearCounters(struct bdx_priv *priv)
{
    tn40_priv_t		*tn40_priv = CONTAINING_RECORD(priv, tn40_priv_t, bdx_priv);

	bzero((void *)&tn40_priv->counters, sizeof(tn40_priv->counters));
	bzero((void *)&priv->counters, 	   sizeof(priv->counters));

} // bdx_clearCounters()

#endif

//-------------------------------------------------------------------------------------------------

static void bdx_print_phys(void)
{
    #ifdef PHY_MV88X3120
        MSG("MV88X3120 phy supported\n");
    #endif
    #ifdef PHY_MV88X3310
        MSG("MV88X3310 phy supported\n");
    #endif
	#ifdef PHY_MV88E2010
		MSG("MV88E2010 phy supported\n");
	#endif
   #ifdef PHY_TLK10232
        MSG("TLK10232 phy supported\n");
    #endif
    #ifdef PHY_QT2025
        MSG("QT2025 phy supported\n");
    #endif
    #ifdef PHY_AQ2104
        MSG("AQ2104 phy supported\n");
    #endif

} // bdx_print_phys()

//-------------------------------------------------------------------------------------------------

static void print_driver_id(void)
{
    MSG("%s, %s\n", TN40_DRV_DESC, TN40_DRV_VERSION);
    bdx_print_phys();

} // print_driver_id()


/*************************************************************************
 *     MDIO Interface                            *
 *************************************************************************/
/* bdx_mdio_get - read MDIO_CMD_STAT until the device is not busy
 *
 * returns the CMD_STAT value read, or -1 (0xFFFFFFFF) for failure
 * (since the busy bit should be off, -1 can never be a valid value for
 * mdio_get).
 */

u32 bdx_mdio_get(struct bdx_priv *priv)
{
#define BDX_MAX_MDIO_BUSY_LOOPS 1024
    int tries = 0;
    while (++tries < BDX_MAX_MDIO_BUSY_LOOPS)
    {
        u32 mdio_cmd_stat = READ_REG(priv, regMDIO_CMD_STAT);

        if (GET_MDIO_BUSY(mdio_cmd_stat)== 0)
        {
            //          ERR("mdio_get success after %d tries (val=%u 0x%x)\n", tries, mdio_cmd_stat, mdio_cmd_stat);
            return mdio_cmd_stat;
        }
    }
    ERR("MDIO busy!\n");
    return 0xFFFFFFFF;

} // bdx_mdio_get()

//-------------------------------------------------------------------------------------------------

/* bdx_mdio_read - read a 16bit word through the MDIO interface
 * @priv
 * @device   - 5 bit device id
 * @port     - 5 bit port id
 * @addr     - 16 bit address
 * returns a 16bit value or -1 for failure
 */
int bdx_mdio_read(struct bdx_priv *priv, int device, int port, u16 addr)
{
    u32 tmp_reg, i;
    /* Wait until MDIO is not busy */
    if (bdx_mdio_get(priv)== 0xFFFFFFFF)
    {
        return -1;
    }
    i=( (device&0x1F) | ((port&0x1F)<<5) );
    WRITE_REG(priv, regMDIO_CMD, i);
    WRITE_REG(priv, regMDIO_ADDR, (u32)addr);
	if ((tmp_reg = bdx_mdio_get(priv)) == 0xFFFFFFFF)
	{
		ERR("MDIO busy after read command\n");
		return -1;
	}
		WRITE_REG(priv, regMDIO_CMD, ((1<<15) |i));
    /* Read CMD_STAT until not busy */
    if ((tmp_reg = bdx_mdio_get(priv)) == 0xFFFFFFFF)
    {
        ERR("MDIO busy after read command\n");
        return -1;
    }
    if (GET_MDIO_RD_ERR(tmp_reg))
    {
        DBG("MDIO error after read command\n");
        return -1;
    }
    tmp_reg = READ_REG(priv, regMDIO_DATA);

    //  ERR("MDIO_READ: MDIO_DATA =0x%x \n", (tmp_reg & 0xFFFF));
    return (int)(tmp_reg & 0xFFFF);

} // bdx_mdio_read()

//-------------------------------------------------------------------------------------------------

/* bdx_mdio_write - writes a 16bit word through the MDIO interface
 * @priv
 * @device    - 5 bit device id
 * @port      - 5 bit port id
 * @addr      - 16 bit address
 * @data      - 16 bit value
 * returns 0 for success or -1 for failure
 */
int bdx_mdio_write(struct bdx_priv *priv, int device, int port, u16 addr, u16 data)
{
    u32 tmp_reg;

    /* Wait until MDIO is not busy */
    if (bdx_mdio_get(priv)== 0xFFFFFFFF)
    {
        return -1;
    }
	WRITE_REG(priv, regMDIO_CMD, ((device&0x1F)|((port&0x1F)<<5)));
	WRITE_REG(priv, regMDIO_ADDR, (u32)addr);
	if (bdx_mdio_get(priv)== 0xFFFFFFFF) {		return -1;	}
	WRITE_REG(priv, regMDIO_DATA, (u32)data);
    /* Read CMD_STAT until not busy */
    if ((tmp_reg = bdx_mdio_get(priv)) == 0xFFFFFFFF)
    {
        ERR("MDIO busy after write command\n");
        return -1;
    }
    if (GET_MDIO_RD_ERR(tmp_reg))
    {
        ERR("MDIO error after write command\n");
        return -1;
    }
    return 0;

} // bdx_mdio_write()

//-------------------------------------------------------------------------------------------------

static void setMDIOSpeed(struct bdx_priv *priv, u32 speed)
{
    int 			mdio_cfg;

	mdio_cfg  = READ_REG(priv, regMDIO_CMD_STAT);
    if(1==speed)
    {
		mdio_cfg  = (0x7d<<7) | 0x08; // 1MHz
    }
    else
    {
		mdio_cfg  = 0xA08; // 6MHz
    }
	mdio_cfg |= (1<<6);
	WRITE_REG(priv, regMDIO_CMD_STAT, mdio_cfg);
	msleep(100);

} // setMDIOSpeed()

//-------------------------------------------------------------------------------------------------

static int bdx_mdio_look_for_phy(struct bdx_priv *priv, int port)
{
    int phy_id, i;
    int rVal = -1;

    i=port;
    setMDIOSpeed(priv, MDIO_SPEED_1MHZ);

    phy_id = bdx_mdio_read(priv, 1, i, 0x0002); // PHY_ID_HIGH
    phy_id &=0xFFFF;
	for (i = 0; i < 32; i++)
	{
		msleep(10);
		DBG("LOOK FOR PHY: port=0x%x\n",i);
		phy_id  = bdx_mdio_read(priv, 1, i, 0x0002); // PHY_ID_HIGH
		phy_id &=0xFFFF;
		if (phy_id!=0xFFFF && phy_id!= 0)
		{
			rVal = i;
			break;
		}
	}
    if (rVal == -1)
    {
    	ERR("PHY not found\n");
    }

    return rVal;

} // bdx_mdio_look_for_phy()

//-------------------------------------------------------------------------------------------------

static int __init bdx_mdio_phy_search(struct bdx_priv *priv, int *port_t, enum PHY_TYPE *phy_t)
{
    int i, phy_id;
    const char *s;

    if (bdx_force_no_phy_mode)
    {
    	ERR("Forced NO PHY mode\n");
    	i = 0;
    }
    else
    {
    	i = bdx_mdio_look_for_phy(priv,*port_t);
		if (i >= 0)  // PHY  found
		{
			*port_t = i;
			phy_id  = bdx_mdio_read(priv, 1, *port_t, 0x0002); // PHY_ID_HI
			i       = phy_id << 16;
			phy_id  = bdx_mdio_read(priv, 1, *port_t, 0x0003); // PHY_ID_LOW
			phy_id &=0xFFFF;
			i      |= phy_id;
		}
    }
	switch(i)
    {

#ifdef PHY_QT2025
        case 0x0043A400:
            *phy_t=PHY_TYPE_QT2025;
            s="QT2025 10Gbps SFP+";
            *phy_t = QT2025_register(priv);
            break;
#endif

#ifdef PHY_MV88X3120
        case 0x01405896:
            s="MV88X3120 10Gbps 10GBase-T";
            *phy_t = MV88X3120_register(priv);
            break;

#endif

#if (defined PHY_MV88X3310) || (defined PHY_MV88E2010)
        case 0x02b09aa:
        case 0x02b09ab:
        	if (priv->deviceId == 0x4027)
        	{
        		s="MV88X3310 (A0) 10Gbps 10GBase-T";
        		*phy_t = MV88X3310_register(priv);
        	}
        	else if (priv->deviceId == 0x4527)
        	{
        		s="MV88E2010 (A0) 5Gbps 5GBase-T";
        		*phy_t = MV88X3310_register(priv);
        	}
        	else if (priv->deviceId == 0x4010)
        	{
        		s="Dummy CX4";
        		*phy_t = CX4_register(priv);
        	}
        	else
        	{
        		s = "";
        		ERR("Unsupported device id/phy id 0x%x/0x%x !\n",priv->deviceId, i);
        	}
            break;

#endif

#ifdef PHY_TLK10232
        case 0x40005100:
            s="TLK10232 10Gbps SFP+";
            *phy_t = TLK10232_register(priv);
            break;
#endif

#ifdef PHY_AQR105
        case 0x03A1B462:   //AQR105 B0
        case 0x03A1B463:   //AQR105 B1
        case 0x03A1B4A3:   //AQR105 B1

            s="AQR105 10Gbps 10GBase-T";
            *phy_t = AQR105_register(priv);
            break;
#endif

        default:
            *phy_t=PHY_TYPE_CX4;
            s=(const char*)&"Native 10Gbps CX4";
            *phy_t = CX4_register(priv);
            break;


    } // switch(i)
	setMDIOSpeed(priv, priv->phy_ops.mdio_speed);
    MSG("PHY detected on port %u ID=%X - %s\n", *port_t,i,s);

    return (PHY_TYPE_NA == *phy_t) ? -1 : 0;

} // bdx_mdio_phy_search()

//-------------------------------------------------------------------------------------------------

static int bdx_mdio_reset(struct bdx_priv *priv, int port)
{
    int 			port_t = ++port;
    enum PHY_TYPE 	phy;

    priv->phy_mdio_port=0xFF;
    if(-1 == bdx_mdio_phy_search(priv, &port_t, &phy))
    {
    	return -1;
    }
    port                = port_t;
    priv->phy_mdio_port	= port;
    priv->phy_type		= phy;

    return priv->phy_ops.mdio_reset(priv, port, phy);

} // bdx_mdio_reset()

/*************************************************************************
 *    Print Info                             *
 *************************************************************************/

static void print_hw_id(struct bdx_priv	*priv)
{
    tn40_priv_t				*tn40_priv 		= CONTAINING_RECORD(priv, tn40_priv_t, bdx_priv);
    u16 					pci_link_status = 0;
    u16 					pci_ctrl 		= 0;

    pci_link_status = pci_read_config(tn40_priv->dev, PCI_LINK_STATUS_REG, 2);
    pci_ctrl 		= pci_read_config(tn40_priv->dev, PCI_DEV_CTRL_REG, 2);

    MSG("srom 0x%x HWver %d build %u lane# %d max_pl 0x%x mrrs 0x%x\n",
    	READ_REG(priv, SROM_VER),
		READ_REG(priv, FPGA_VER) & 0xFFFF,
		READ_REG(priv, FPGA_SEED),
        GET_LINK_STATUS_LANES(pci_link_status),
        GET_DEV_CTRL_MAXPL(pci_ctrl), GET_DEV_CTRL_MRRS(pci_ctrl));

} // print_hw_id()

//-------------------------------------------------------------------------------------------------

static void print_fw_id(struct bdx_priv	*priv)
{
    MSG("fw 0x%x\n", READ_REG(priv, FW_VER));
}

//-------------------------------------------------------------------------------------------------

static void print_eth_id(struct bdx_priv *priv)
{
    MSG("%s, Port %d\n", priv->name, priv->port);
}

//-------------------------------------------------------------------------------------------------

inline void  bdx_enable_interrupts(struct bdx_priv *priv)
{
	WRITE_REG(priv, regIMR, priv->isr_mask);
//	EXIT;
}

//-------------------------------------------------------------------------------------------------

void inline  bdx_disable_interrupts(struct bdx_priv *priv)
{
	WRITE_REG(priv, regIMR, 0);
	EXIT;
}

//-------------------------------------------------------------------------------------------------

/* bdx_fifo_init
 * Create TX/RX descriptor fifo for host-NIC communication. 1K extra space is
 * allocated at the end of the fifo to simplify processing of descriptors that
 * wraps around fifo's end.
 *
 * @priv     - NIC private structure
 * @f        - Fifo to initialize
 * @fsz_type - Fifo size type: 0-4KB, 1-8KB, 2-16KB, 3-32KB
 * @reg_XXX  - Offsets of registers relative to base address
 *
 * Returns 0 on success, negative value on failure
 *
 */

static int
bdx_fifo_init(struct bdx_priv *priv, struct fifo *f, int fsz_type,
              u16 reg_CFG0, u16 reg_CFG1, u16 reg_RPTR, u16 reg_WPTR,  const char* fifo_name)
{
	int  	Status;
	u16 	memsz 		= FIFO_SIZE * (1 << fsz_type);

    memset(f, 0, sizeof(struct fifo));
	Status = tn40_alloc_dma(priv, memsz+FIFO_EXTRA_SPACE, &f->dma);
	if (Status == 0)
	{
		f->va		 = (char *)f->dma.tn40_dmamap.va;
		f->da		 = f->dma.tn40_dmamap.pa;
		f->reg_CFG0  = reg_CFG0;
		f->reg_CFG1  = reg_CFG1;
		f->reg_RPTR  = reg_RPTR;
		f->reg_WPTR  = reg_WPTR;
		f->rptr      = 0;
		f->wptr      = 0;
		f->memsz     = memsz;
		f->size_mask = memsz - 1;
		WRITE_REG(priv, reg_CFG0, (u32) ((f->da & TX_RX_CFG0_BASE) | fsz_type));
		WRITE_REG(priv, reg_CFG1, H32_64(f->da));
		bus_dmamap_sync(f->dma.dma_tag, f->dma.tn40_dmamap.dma_map, BUS_DMASYNC_PREREAD);
		//dbg_printFifo(priv, f, fifo_name);
	}
	else
	{
		ERR("fifo_init - tn40_alloc_dma FAILED %r\n", Status);
	}
    RET(Status);

} // bdx_fifo_init()

//-------------------------------------------------------------------------------------------------

/* bdx_fifo_free - Free all resources used by fifo
 * @priv     - Nic private structure
 * @f        - Fifo to release
 */
static void bdx_fifo_free(struct bdx_priv *priv, struct fifo *f)
{
    ENTER;

    tn40_free_dma(&f->dma);

    EXIT;

} // bdx_fifo_free()
//-------------------------------------------------------------------------------------------------

int bdx_speed_set(struct bdx_priv *priv, u32 speed)
{
    int i;
    u32 val;
    //void __iomem * regs=priv->pBdxRegs;
    //int port=priv->phy_mdio_port;

	DBG("speed %d\n", speed);

	switch(speed)
    {
        case SPEED_10000:
        case SPEED_5000:
        case SPEED_2500:
        case SPEED_1000X:
        case SPEED_100X:
        	DBG("link_speed %d\n", speed);
			// BDX_MDIO_WRITE(priv, 1,0,0x2040);
            WRITE_REG(priv, 0x1010, 0x217); /*ETHSD.REFCLK_CONF  */
            WRITE_REG(priv, 0x104c, 0x4c);  /*ETHSD.L0_RX_PCNT  */
            WRITE_REG(priv, 0x1050, 0x4c);  /*ETHSD.L1_RX_PCNT  */
            WRITE_REG(priv, 0x1054, 0x4c);  /*ETHSD.L2_RX_PCNT  */
            WRITE_REG(priv, 0x1058, 0x4c);  /*ETHSD.L3_RX_PCNT  */
            WRITE_REG(priv, 0x102c, 0x434); /*ETHSD.L0_TX_PCNT  */
            WRITE_REG(priv, 0x1030, 0x434); /*ETHSD.L1_TX_PCNT  */
            WRITE_REG(priv, 0x1034, 0x434); /*ETHSD.L2_TX_PCNT  */
            WRITE_REG(priv, 0x1038, 0x434); /*ETHSD.L3_TX_PCNT  */
            WRITE_REG(priv, 0x6300, 0x0400); /*MAC.PCS_CTRL*/
            //  udelay(50); val = READ_REG(priv,0x6300);ERR("MAC init:0x6300= 0x%x \n",val);
            WRITE_REG(priv, 0x1018, 0x00); /*Mike2*/
            udelay(5);
            WRITE_REG(priv, 0x1018, 0x04); /*Mike2*/
            udelay(5);
            WRITE_REG(priv, 0x1018, 0x06); /*Mike2*/
            udelay(5);
            //MikeFix1
            //L0: 0x103c , L1: 0x1040 , L2: 0x1044 , L3: 0x1048 =0x81644
            WRITE_REG(priv, 0x103c, 0x81644); /*ETHSD.L0_TX_DCNT  */
            WRITE_REG(priv, 0x1040, 0x81644); /*ETHSD.L1_TX_DCNT  */
            WRITE_REG(priv, 0x1044, 0x81644); /*ETHSD.L2_TX_DCNT  */
            WRITE_REG(priv, 0x1048, 0x81644); /*ETHSD.L3_TX_DCNT  */
            WRITE_REG(priv, 0x1014, 0x043); /*ETHSD.INIT_STAT*/
            for(i=1000; i; i--)
            {
                udelay(50);
                val = READ_REG(priv,0x1014); /*ETHSD.INIT_STAT*/
                if(val & (1<<9))
                {
                    WRITE_REG(priv, 0x1014, 0x3); /*ETHSD.INIT_STAT*/
                    val = READ_REG(priv,0x1014); /*ETHSD.INIT_STAT*/
                    //                 ERR("MAC init:0x1014=0x%x i=%d\n",val,i);
                    break;
                }
            }
            if(0==i)
            {
                ERR("MAC init timeout!\n");
            }

            WRITE_REG(priv, 0x6350, 0x0); /*MAC.PCS_IF_MODE*/
            WRITE_REG(priv, regCTRLST, 0xC13);//0x93//0x13
            WRITE_REG(priv, 0x111c, 0x7ff); /*MAC.MAC_RST_CNT*/
            for(i=40; i--;)
            {
                udelay(50);
            }
            WRITE_REG(priv, 0x111c, 0x0); /*MAC.MAC_RST_CNT*/
            //WRITE_REG(priv, 0x1104,0x24);  // EEE_CTRL.EXT_PHY_LINK=1 (bit 2)
            break;

        case SPEED_1000:
        case SPEED_100:
            //BDX_MDIO_WRITE(priv, 1,0,0x2000);       // write  1.0 0x2000  # Force 1G
            WRITE_REG(priv, 0x1010, 0x613); /*ETHSD.REFCLK_CONF  */
            WRITE_REG(priv, 0x104c, 0x4d);  /*ETHSD.L0_RX_PCNT  */
            WRITE_REG(priv, 0x1050, 0x0);  /*ETHSD.L1_RX_PCNT  */
            WRITE_REG(priv, 0x1054, 0x0);  /*ETHSD.L2_RX_PCNT  */
            WRITE_REG(priv, 0x1058, 0x0);  /*ETHSD.L3_RX_PCNT  */
            WRITE_REG(priv, 0x102c, 0x35); /*ETHSD.L0_TX_PCNT  */
            WRITE_REG(priv, 0x1030, 0x0); /*ETHSD.L1_TX_PCNT  */
            WRITE_REG(priv, 0x1034, 0x0); /*ETHSD.L2_TX_PCNT  */
            WRITE_REG(priv, 0x1038, 0x0); /*ETHSD.L3_TX_PCNT  */
            WRITE_REG(priv, 0x6300, 0x01140); /*MAC.PCS_CTRL*/
            //  udelay(50); val = READ_REG(priv,0x6300);ERR("MAC init:0x6300= 0x%x \n",val);
            WRITE_REG(priv, 0x1014, 0x043); /*ETHSD.INIT_STAT*/
            for(i=1000; i; i--)
            {
                udelay(50);
                val = READ_REG(priv,0x1014); /*ETHSD.INIT_STAT*/
                if(val & (1<<9))
                {
                    WRITE_REG(priv, 0x1014, 0x3); /*ETHSD.INIT_STAT*/
                    val = READ_REG(priv,0x1014); /*ETHSD.INIT_STAT*/
                    //                 ERR("MAC init:0x1014= 0x%x i=%d\n",val,i);
                    break;
                }
            }
            if(0==i)
            {
                ERR("MAC init timeout!\n");
            }
            WRITE_REG(priv, 0x6350, 0x2b); /*MAC.PCS_IF_MODE 1g*/
            WRITE_REG(priv, 0x6310, 0x9801); /*MAC.PCS_DEV_AB */
            //100 WRITE_REG(priv, 0x6350, 0x27); /*MAC.PCS_IF_MODE 100m*/
            //100 WRITE_REG(priv, 0x6310, 0x9501); /*MAC.PCS_DEV_AB */

            WRITE_REG(priv, 0x6314, 0x1); /*MAC.PCS_PART_AB */
            WRITE_REG(priv, 0x6348, 0xc8); /*MAC.PCS_LINK_LO */
            WRITE_REG(priv, 0x634c, 0xc8); /*MAC.PCS_LINK_HI */
            udelay(50);
            WRITE_REG(priv, regCTRLST, 0xC13);//0x93//0x13
            WRITE_REG(priv, 0x111c, 0x7ff); /*MAC.MAC_RST_CNT*/
            for(i=40; i--;)
            {
                udelay(50);
            }
            WRITE_REG(priv, 0x111c, 0x0); /*MAC.MAC_RST_CNT*/
            WRITE_REG(priv, 0x6300, 0x1140); /*MAC.PCS_CTRL*/
            //        WRITE_REG(priv, 0x1104,0x24);  // EEE_CTRL.EXT_PHY_LINK=1 (bit 2)
            break;

      case 0: // Link down
            //  WRITE_REG(priv, 0x1010, 0x613); /*ETHSD.REFCLK_CONF  */
            WRITE_REG(priv, 0x104c, 0x0);  /*ETHSD.L0_RX_PCNT  */
            WRITE_REG(priv, 0x1050, 0x0);  /*ETHSD.L1_RX_PCNT  */
            WRITE_REG(priv, 0x1054, 0x0);  /*ETHSD.L2_RX_PCNT  */
            WRITE_REG(priv, 0x1058, 0x0);  /*ETHSD.L3_RX_PCNT  */
            WRITE_REG(priv, 0x102c, 0x0); /*ETHSD.L0_TX_PCNT  */
            WRITE_REG(priv, 0x1030, 0x0); /*ETHSD.L1_TX_PCNT  */
            WRITE_REG(priv, 0x1034, 0x0); /*ETHSD.L2_TX_PCNT  */
            WRITE_REG(priv, 0x1038, 0x0); /*ETHSD.L3_TX_PCNT  */

            //                 WRITE_REG(priv, 0x1104,0x20); // EEE_CTRL.EXT_PHY_LINK=0 (bit 2)

            WRITE_REG(priv, regCTRLST, 0x800);
            WRITE_REG(priv, 0x111c, 0x7ff); /*MAC.MAC_RST_CNT*/
            for(i=40; i--;)
            {
                udelay(50);
            }
            WRITE_REG(priv, 0x111c, 0x0); /*MAC.MAC_RST_CNT*/
            break;
        default:
            ERR("%s Link speed was not identified yet (%d)\n", priv->name, speed);
            speed=0;
            break;
    }

	return speed;

} // bdx_speed_set()

//-------------------------------------------------------------------------------------------------

void bdx_speed_changed(struct bdx_priv *priv, u32 speed)
{
    tn40_priv_t		*tn40_priv = CONTAINING_RECORD(priv, tn40_priv_t, bdx_priv);

	DBG("speed %d\n", speed);
	speed = bdx_speed_set(priv, speed);
	DBG("link_speed %d speed %d\n", priv->link_speed, speed);
    if (priv->link_speed != speed)
    {
    	priv->link_speed = speed;
    	DBG("Speed changed %d\n", priv->link_speed);
    	mtx_lock(&tn40_priv->mtx);
    	mtx_unlock(&tn40_priv->mtx);
    }

} // bdx_speed_changed()

//-------------------------------------------------------------------------------------------------
/*
 * bdx_link_changed - Notify the OS about hw link state.
 *
 * @bdx_priv - HW adapter structure
 */

void bdx_link_changed(struct bdx_priv *priv)
{
	tn40_priv_t			*tn40_priv 	= CONTAINING_RECORD(priv, tn40_priv_t, bdx_priv);
	if_t	 		ifp 		= tn40_priv->ifp;
	int					reported;

	ENTER;
    u32 link = priv->phy_ops.link_changed(priv);;

    /*
     * This is called from the poll thread/callout on every pass, so only
     * touch the stack and log when the link state actually changed.
     */
    reported = link ? priv->link_speed : 0;
    if (reported == tn40_priv->link_reported)
    {
    	return;
    }
    tn40_priv->link_reported = reported;

    if (!link)
    {
    	if_link_state_change(ifp, LINK_STATE_DOWN);
        MSG("%s Link Down\n", if_name(ifp));

#ifdef _EEE_
    	    if(NULL!=priv->phy_ops.reset_eee)
    	    {
    	    	priv->phy_ops.reset_eee(priv);
    	    }
#endif
    }
    else
    {
    	if_link_state_change(ifp, LINK_STATE_UP);
        MSG("%s Link Up %s\n", if_name(ifp), 	(priv->link_speed == SPEED_10000) ? "10G"   :
        										(priv->link_speed == SPEED_5000)  ? "5G"    :
        										(priv->link_speed == SPEED_2500)  ? "2.5G"  :
        										(priv->link_speed == SPEED_1000) || (priv->link_speed == SPEED_1000X) ? "1G"   :
        										(priv->link_speed == SPEED_100)  || (priv->link_speed == SPEED_100X)  ? "100M" :
        										 " ");
    }
    EXIT;
} // bdx_link_changed()

//-------------------------------------------------------------------------------------------------

/* bdx_isr - Interrupt Service Routine for Bordeaux NIC
 */
int bdx_isr(struct bdx_priv *priv)
{
    u32 isr = READ_REG(priv, regISR_MSK0);

    DBG("isr = 0x%x\n", isr);

    RET(isr);

} // bdx_isr()

//-------------------------------------------------------------------------------------------------

static inline int bdx_tx_space(struct bdx_priv *priv)
{
    struct txd_fifo *f = &priv->txd_fifo0;
    int fsize;

    f->m.rptr = READ_REG(priv, f->m.reg_RPTR) & TXF_WPTR_WR_PTR;
    fsize     = f->m.rptr - f->m.wptr;
    if (fsize <= 0)
        fsize = f->m.memsz + fsize;
    return (fsize);

} // bdx_tx_space()

//-------------------------------------------------------------------------------------------------

static void bdx_tx_push_desc(struct bdx_priv *priv, void *data, int size)
{
    struct txd_fifo *f = &priv->txd_fifo0;
    int i = f->m.memsz - f->m.wptr;

    if (size == 0)
        return;

    if (i > size)
    {
    	memcpy(f->m.va + f->m.wptr, data, size);
        f->m.wptr += size;
    }
    else
    {
    	memcpy(f->m.va + f->m.wptr, data, i);
        f->m.wptr = size - i;
        memcpy(f->m.va, (char *)data + i, f->m.wptr);
    }
    WRITE_REG(priv, f->m.reg_WPTR, f->m.wptr & TXF_WPTR_WR_PTR);

} // bdx_tx_push_desc()

//-------------------------------------------------------------------------------------------------

static void bdx_tx_push_desc_safe(struct bdx_priv *priv, void *data, int size)
{
    int timer = 0;
    ENTER;

    while (size > 0)
    {
        /*
        * We subtract 8 because when the fifo is full rptr == wptr, which
        * also means that fifo is empty, we can understand the difference,
        * but could the HW do the same ??? :)
        */
        int avail = bdx_tx_space(priv) - 8;
        if (avail <= 0)
        {
            if (timer++ > 300)      /* Prevent endless loop */
            {
                DBG("timeout while writing desc to TxD fifo\n");
                break;
            }
            udelay(50); /* Give the HW a chance to clean the fifo */
            continue;
        }
        avail = MIN(avail, size);
        DBG("about to push  %d bytes starting %p size %d\n", avail, data, size);
        bdx_tx_push_desc(priv, data, avail);
        size -= avail;
        data = (char *)data + avail;
    }

    EXIT;

} // bdx_tx_push_desc_safe()

//-------------------------------------------------------------------------------------------------

/* bdx_fw_load - Load the firmware to the NIC
 * @priv       - NIC private structure
 *
 * The firmware is loaded via TXD fifo, which needs be initialized first.
 * The firmware needs to be loaded once per NIC and not per PCI device
 * provided by NIC (a NIC can have multiple devices). So all the drivers use
 * semaphore register to load the FW only once.
 */

static int __init bdx_fw_load(struct bdx_priv *priv)
{
    int master, i;
    int rVal = 0;

    ENTER;
    master = READ_REG(priv, regINIT_SEMAPHORE);
    if (!READ_REG(priv, regINIT_STATUS) && master)
    {
        DBG("Loading FW...\n");
        bdx_tx_push_desc_safe(priv, s_firmLoad, sizeof(s_firmLoad));
        mdelay(100);
    }
    for (i = 0; i < 200; i++)
    {
        if (READ_REG(priv, regINIT_STATUS))
            break;
        mdelay(2);
    }
    if (master)
        WRITE_REG(priv, regINIT_SEMAPHORE, 1);

    if (i == 200)
    {
        ERR("%s firmware loading failed\n", priv->name);
        DBG("VPC = 0x%x VIC = 0x%x INIT_STATUS = 0x%x i =%d\n",
            READ_REG(priv, regVPC),
            READ_REG(priv, regVIC), READ_REG(priv, regINIT_STATUS), i);
        rVal = -1;
    }
    else
    {
        DBG("%s firmware loading success\n", priv->name);
    }
    print_fw_id(priv);
    RET(rVal);

} // bdx_fw_load()

//-------------------------------------------------------------------------------------------------

static void bdx_restore_mac(struct bdx_priv *priv)
{
    u32 val;

    ENTER;
    DBG("mac0 =%x mac1 =%x mac2 =%x\n",
    READ_REG(priv, regUNC_MAC0_A),
    READ_REG(priv, regUNC_MAC1_A), READ_REG(priv, regUNC_MAC2_A));

    val = (priv->mac_address[0] << 8) | (priv->mac_address[1]);
    WRITE_REG(priv, regUNC_MAC2_A, val);
    val = (priv->mac_address[2] << 8) | (priv->mac_address[3]);
    WRITE_REG(priv, regUNC_MAC1_A, val);
    val = (priv->mac_address[4] << 8) | (priv->mac_address[5]);
    WRITE_REG(priv, regUNC_MAC0_A, val);
    /* More then IP MAC address */
    WRITE_REG(priv, regMAC_ADDR_0, (priv->mac_address[3] << 24) | (priv->mac_address[2] << 16) | (priv->mac_address[1] << 8) | (priv->mac_address[0]));
    WRITE_REG(priv, regMAC_ADDR_1, (priv->mac_address[5] << 8)  | (priv->mac_address[4]));
    DBG("mac0 =%x mac1 =%x mac2 =%x\n",
    READ_REG(priv, regUNC_MAC0_A),
    READ_REG(priv, regUNC_MAC1_A), READ_REG(priv, regUNC_MAC2_A));
    EXIT;

} // bdx_restore_mac()

//-------------------------------------------------------------------------------------------------

static void bdx_CX4_hw_start(struct bdx_priv *priv)
{
	int i;
	u32 val;

	WRITE_REG(priv, 0x1010, 0x217); /*ETHSD.REFCLK_CONF  */
	WRITE_REG(priv, 0x104c, 0x4c);  /*ETHSD.L0_RX_PCNT  */
	WRITE_REG(priv, 0x1050, 0x4c);  /*ETHSD.L1_RX_PCNT  */
	WRITE_REG(priv, 0x1054, 0x4c);  /*ETHSD.L2_RX_PCNT  */
	WRITE_REG(priv, 0x1058, 0x4c);  /*ETHSD.L3_RX_PCNT  */
	WRITE_REG(priv, 0x102c, 0x434); /*ETHSD.L0_TX_PCNT  */
	WRITE_REG(priv, 0x1030, 0x434); /*ETHSD.L1_TX_PCNT  */
	WRITE_REG(priv, 0x1034, 0x434); /*ETHSD.L2_TX_PCNT  */
	WRITE_REG(priv, 0x1038, 0x434); /*ETHSD.L3_TX_PCNT  */
	WRITE_REG(priv, 0x6300, 0x0400); /*MAC.PCS_CTRL*/
	//  udelay(50); val = READ_REG(priv,0x6300);ERR("MAC init:0x6300= 0x%x \n",val);
	WRITE_REG(priv, 0x1018, 0x00); /*Mike2*/
	udelay(5);
	WRITE_REG(priv, 0x1018, 0x04); /*Mike2*/
	udelay(5);
	WRITE_REG(priv, 0x1018, 0x06); /*Mike2*/
	udelay(5);
	//MikeFix1
	//L0: 0x103c , L1: 0x1040 , L2: 0x1044 , L3: 0x1048 =0x81644
	WRITE_REG(priv, 0x103c, 0x81644); /*ETHSD.L0_TX_DCNT  */
	WRITE_REG(priv, 0x1040, 0x81644); /*ETHSD.L1_TX_DCNT  */
	WRITE_REG(priv, 0x1044, 0x81644); /*ETHSD.L2_TX_DCNT  */
	WRITE_REG(priv, 0x1048, 0x81644); /*ETHSD.L3_TX_DCNT  */
	WRITE_REG(priv, 0x1014, 0x043); /*ETHSD.INIT_STAT*/
	for(i=1000; i; i--)
	{
		udelay(50);
		val = READ_REG(priv,0x1014); /*ETHSD.INIT_STAT*/
		if(val & (1<<9))
		{
			WRITE_REG(priv, 0x1014, 0x3); /*ETHSD.INIT_STAT*/
			val = READ_REG(priv,0x1014); /*ETHSD.INIT_STAT*/
			//                 ERR("MAC init:0x1014=0x%x i=%d\n",val,i);
			break;
		}
	}
	if(0==i)
	{
		ERR("MAC init timeout!\n");
	}
	WRITE_REG(priv, 0x6350, 0x0); /*MAC.PCS_IF_MODE*/
	WRITE_REG(priv, regCTRLST, 0xC13);//0x93//0x13
	WRITE_REG(priv, 0x111c, 0x7ff); /*MAC.MAC_RST_CNT*/
	for(i=40; i--;)
	{
		udelay(50);
	}
	WRITE_REG(priv, 0x111c, 0x0); /*MAC.MAC_RST_CNT*/
	//WRITE_REG(priv, 0x1104,0x24);  // EEE_CTRL.EXT_PHY_LINK=1 (bit 2)

} // bdx_CX4_hw_start

//-------------------------------------------------------------------------------------------------

/* bdx_hw_start - Initialize registers and starts HW's Rx and Tx engines
 * @priv    - NIC private structure
 */
static int bdx_hw_start(struct bdx_priv *priv)
{

    ENTER;

    DBG("********** bdx_hw_start() ************\n");
    priv->link_speed=0; /* -1 */
    if(priv->phy_type==PHY_TYPE_CX4)
    {
    	bdx_CX4_hw_start(priv);
    }
    else
    {
    	DBG("********** bdx_hw_start() NOT CX4 ************\n");
		WRITE_REG(priv, regFRM_LENGTH, 0X3FE0);
		WRITE_REG(priv, regGMAC_RXF_A, 0X10fd);
		//MikeFix1
		//L0: 0x103c , L1: 0x1040 , L2: 0x1044 , L3: 0x1048 =0x81644
		WRITE_REG(priv, 0x103c, 0x81644); /*ETHSD.L0_TX_DCNT  */
		WRITE_REG(priv, 0x1040, 0x81644); /*ETHSD.L1_TX_DCNT  */
		WRITE_REG(priv, 0x1044, 0x81644); /*ETHSD.L2_TX_DCNT  */
		WRITE_REG(priv, 0x1048, 0x81644); /*ETHSD.L3_TX_DCNT  */
		WRITE_REG(priv, regRX_FIFO_SECTION, 0x10);
		WRITE_REG(priv, regTX_FIFO_SECTION, 0xE00010);
		WRITE_REG(priv, regRX_FULLNESS, 0);
		WRITE_REG(priv, regTX_FULLNESS, 0);
	}
    WRITE_REG(priv, regVGLB, 0);
    WRITE_REG(priv, regMAX_FRAME_A, priv->rxf_fifo0.m.pktsz & MAX_FRAME_AB_VAL);

    DBG("RDINTCM =%08x\n", priv->rdintcm);  /*NOTE: test script uses this */
    WRITE_REG(priv, regRDINTCM0, priv->rdintcm);
    WRITE_REG(priv, regRDINTCM2, 0);    /*cpu_to_le32(rcm.val)); */

    DBG("TDINTCM =%08x\n", priv->tdintcm);  /*NOTE: test script uses this */
    WRITE_REG(priv, regTDINTCM0, priv->tdintcm);    /* old val = 0x300064 */

    /* Enable timer interrupt once in 2 secs. */
    /*WRITE_REG(priv, regGTMR0, ((GTMR_SEC * 2) & GTMR_DATA)); */
    bdx_restore_mac(priv);
    /* Pause frame */
    WRITE_REG(priv, 0x12E0, 0x28);
    WRITE_REG(priv, regPAUSE_QUANT, 0xFFFF);
    WRITE_REG(priv, 0x6064, 0xF);

    WRITE_REG(priv, regGMAC_RXF_A, GMAC_RX_FILTER_OSEN | GMAC_RX_FILTER_TXFC | GMAC_RX_FILTER_AM | GMAC_RX_FILTER_AB);

  //  bdx_setAffinity(priv->pdev->irq);
    bdx_link_changed(priv);
    bdx_enable_interrupts(priv);

    RET(0);

} // bdx_hw_start()

//-------------------------------------------------------------------------------------------------

static void bdx_hw_stop(struct bdx_priv *priv)
{
    ENTER;

    if ((priv->state & BDX_STATE_HW_STOPPED) == 0)
    {
		priv->state |= BDX_STATE_HW_STOPPED;
		bdx_disable_interrupts(priv);
    }

    EXIT;

} // bdx_hw_stop()

//-------------------------------------------------------------------------------------------------

static int bdx_hw_reset(struct bdx_priv *priv)
{
    u32 val, i;
    ENTER;

    if (priv->port == 0)
    {
        /* Reset sequences: read, write 1, read, write 0 */
        val = READ_REG(priv, regCLKPLL);
        WRITE_REG(priv, regCLKPLL, (val | CLKPLL_SFTRST) + 0x8);
        udelay(50);
        val = READ_REG(priv, regCLKPLL);
        WRITE_REG(priv, regCLKPLL, val & ~CLKPLL_SFTRST);
    }
    /* Check that the PLLs are locked and reset ended */
    for (i = 0; i < 70; i++, mdelay(10))
    {
        if ((READ_REG(priv, regCLKPLL) & CLKPLL_LKD) == CLKPLL_LKD)
        {
            udelay(50);
            /* Do any PCI-E read transaction */
            READ_REG(priv, regRXD_CFG0_0);
            return 0;

        }
    }
    ERR("HW reset failed\n");

    RET(1);       /* Failure */

} // bdx_hw_reset()

//-------------------------------------------------------------------------------------------------

static int bdx_sw_reset(struct bdx_priv *priv)
{
    int i;

    ENTER;
    /* 1. load MAC (obsolete) */
    /* 2. disable Rx (and Tx) */
    WRITE_REG(priv, regGMAC_RXF_A, 0);
    mdelay(100);
    /* 3. Disable port */
    WRITE_REG(priv, regDIS_PORT, 1);
    /* 4. Disable queue */
    WRITE_REG(priv, regDIS_QU, 1);
    /* 5. Wait until hw is disabled */
    for (i = 0; i < 50; i++)
    {
        if (READ_REG(priv, regRST_PORT) & 1)
            break;
        mdelay(10);
    }
    if (i == 50)
    {
        ERR("%s SW reset timeout. continuing anyway\n", priv->name);
    }
    /* 6. Disable interrupts */
    WRITE_REG(priv, regRDINTCM0, 0);
    WRITE_REG(priv, regTDINTCM0, 0);
    WRITE_REG(priv, regIMR, 0);
    READ_REG(priv, regISR);

    /* 7. Reset queue */
    WRITE_REG(priv, regRST_QU, 1);
    /* 8. Reset port */
    WRITE_REG(priv, regRST_PORT, 1);
    /* 9. Zero all read and write pointers */
    //for (i = regTXD_WPTR_0; i <= regTXF_RPTR_3; i += 0x10)
    //    DBG("%x = %x\n", i, READ_REG(priv, i) & TXF_WPTR_WR_PTR);
    for (i = regTXD_WPTR_0; i <= regTXF_RPTR_3; i += 0x10)
        WRITE_REG(priv, i, 0);
    /* 10. Unset port disable */
    WRITE_REG(priv, regDIS_PORT, 0);
    /* 11. Unset queue disable */
    WRITE_REG(priv, regDIS_QU, 0);
    /* 12. Unset queue reset */
    WRITE_REG(priv, regRST_QU, 0);
    /* 13. Unset port reset */
    WRITE_REG(priv, regRST_PORT, 0);
    /* 14. Enable Rx */
    /* Skipped. will be done later */
    /* 15. Save MAC (obsolete) */
    //for (i = regTXD_WPTR_0; i <= regTXF_RPTR_3; i += 0x10)
    //{
    //    DBG("%x = %x\n", i, READ_REG(priv, i) & TXF_WPTR_WR_PTR);
    //}

    RET(0);

} // bdx_sw_reset()

//-------------------------------------------------------------------------------------------------

/* bdx_reset - Perform the right type of reset depending on hw type */
static int bdx_reset(struct bdx_priv *priv)
{
    ENTER;
    //  RET((priv->pdev->device == 0x4010) ? bdx_hw_reset(priv) : bdx_sw_reset(priv));
    RET(bdx_hw_reset(priv));

} // bdx_reset()

//-------------------------------------------------------------------------------------------------

static int bdx_start(struct bdx_priv *priv, int bLoadFw)
{
	int					rc = 0;

    if ((priv->state & BDX_STATE_STARTED) == 0)
    {
    	priv->state |= BDX_STATE_STARTED;
		do
		{
			rc = -1;
			if (bdx_tx_init(priv))
			{
				break;
			}
			if (bdx_rx_init(priv))
			{
				break;
			}
			bdx_rx_alloc_buffers(priv);
			if (bLoadFw && bdx_fw_load(priv))
			{
				break;
			}
			bdx_init_rss(priv);
			rc = 0;
		} while(0);
    }
    if (rc == 0)
    {
		if (priv->state & BDX_STATE_OPEN)
		{
			rc = bdx_hw_start(priv);
		}
    }

    return rc;

} // bdx_start()

//-------------------------------------------------------------------------------------------------

static void bdx_stop(struct bdx_priv *priv)
{
	if (priv->state & BDX_STATE_STARTED)
	{
		priv->state &= ~BDX_STATE_STARTED;
		bdx_hw_stop(priv);
		bdx_sw_reset(priv);
		bdx_rx_free(priv);
		bdx_tx_free(priv);
	}
} // bdx_stop()

//-------------------------------------------------------------------------------------------------

int bdx_close(struct bdx_priv *priv)
{
    ENTER;
    bdx_stop(priv);
    priv->state &= ~BDX_STATE_OPEN;
    /* Re-announce the link state when the interface comes back up */
    CONTAINING_RECORD(priv, tn40_priv_t, bdx_priv)->link_reported = -1;

    RET(0);

} // bdx_close()

//-------------------------------------------------------------------------------------------------

int bdx_open(struct bdx_priv *priv)
{
    int rc = 0;

    ENTER;
    if ((priv->state & BDX_STATE_OPEN) == 0)
    {
		priv->state |= BDX_STATE_OPEN;
		bdx_sw_reset(priv);

		if ((rc = bdx_start(priv, NO_FW_LOAD)) != 0)
		{
			bdx_close(priv);
		}
    }
    RET(rc);

} // bdx_open()

//-------------------------------------------------------------------------------------------------

#ifdef TN40_BIG_ENDIAN
static void __init bdx_firmware_endianess(void)
{
    int i;
    for (i = 0; i < ARRAY_SIZE(s_firmLoad); i++)
    {
        s_firmLoad[i] = CPU_CHIP_SWAP32(s_firmLoad[i]);
    }
}
#endif

//-------------------------------------------------------------------------------------------------

void bdx_vlan_rx_vid(struct bdx_priv *priv, uint16_t vid, int enable)
{
    u32 reg, bit, val;

    ENTER;
    DBG("vid =%d value =%d\n", (int)vid, enable);
    if (unlikely(vid >= 4096))
    {
        ERR("invalid VID: %u (> 4096)\n", vid);
        RET();
    }
    reg = regVLAN_0 + (vid / 32) * 4;
    bit = 1 << vid % 32;
    val = READ_REG(priv, reg);
    DBG("reg =%x, val =%x, bit =%d\n", reg, val, bit);
    if (enable)
        val |= bit;
    else
        val &= ~bit;
    DBG("new val %x\n", val);
    WRITE_REG(priv, reg, val);
    RET();

} // bdx_vlan_rx_vid()

//-------------------------------------------------------------------------------------------------
/**
 * bdx_change_mtu - Change the Maximum Transfer Unit
 *
 * @netdev  - Network interface device structure
 * @new_mtu - New value for maximum frame size
 *
 * Returns 0 on success, negative error code on failure
 */
int bdx_change_mtu(struct bdx_priv *priv, int new_mtu)
{
    tn40_priv_t	*tn40_priv = CONTAINING_RECORD(priv, tn40_priv_t, bdx_priv);

    ENTER;

    if (new_mtu == priv->mtu)
    {
        RET(0);
    }
    /* enforce minimum frame size */
    if (new_mtu < ETHER_MIN_MTU)
    {
        ERR("%s mtu %d is less then minimal %d\n", priv->name, new_mtu, ETHER_MIN_MTU);
        RET(-1);
    }
    else if (new_mtu > BDX_MAX_MTU)
    {
        ERR("%s mtu %d is greater then max mtu %d\n", priv->name, new_mtu, BDX_MAX_MTU);
        RET(-1);
    }
    priv->mtu = new_mtu;
    mtx_lock(&tn40_priv->tx_lock);
    bdx_close(priv);
    bdx_open (priv);
    mtx_unlock(&tn40_priv->tx_lock);

    RET(0);

} // bdx_change_mtu()

//-------------------------------------------------------------------------------------------------

int bdx_set_mac(struct bdx_priv *priv, u8 *addr)
{

    ENTER;

    memcpy(priv->mac_address, addr, ETHER_ADDR_LEN);
    bdx_restore_mac(priv);

    RET(0);

} // bdx_set_mac()

//-------------------------------------------------------------------------------------------------

static int bdx_read_mac(struct bdx_priv *priv)
{
    u16 mac_address[3], i;
    ENTER;

    mac_address[2] = READ_REG(priv, regUNC_MAC0_A);
    mac_address[2] = READ_REG(priv, regUNC_MAC0_A);
    mac_address[1] = READ_REG(priv, regUNC_MAC1_A);
    mac_address[1] = READ_REG(priv, regUNC_MAC1_A);
    mac_address[0] = READ_REG(priv, regUNC_MAC2_A);
    mac_address[0] = READ_REG(priv, regUNC_MAC2_A);
    for (i = 0; i < 3; i++)
    {
        priv->mac_address[i * 2 + 1] = (u8)(mac_address[i]);
        priv->mac_address[i * 2]     = (u8)(mac_address[i] >> 8);
    }
    DBG("Mac address : %02x:%02x:%02x:%02x:%02x:%02x\n", priv->mac_address[0],priv->mac_address[1],priv->mac_address[2],
    		priv->mac_address[3], priv->mac_address[4], priv->mac_address[5]);
    RET(0);

} // bdx_read_mac()

//-------------------------------------------------------------------------------------------------

static int bdx_range_check(struct bdx_priv *priv, u32 offset)
{
    return ((offset > (u32) (BDX_REGS_SIZE)) ? -EINVAL : 0);
}

//-------------------------------------------------------------------------------------------------

int bdx_ioctl_priv(struct bdx_priv *priv, struct ifreq *ifr)
{
    tn40_ioctl_t tn40_ioctl;
    int error;
    u16 dev,addr;

    ENTER;
	error = copyin(ifr_data_get_ptr(ifr), &tn40_ioctl, sizeof(tn40_ioctl));
	if (error)
	{
		ERR("cant copy from user\n");
		RET(error);
	}
	DBG("%d 0x%x 0x%x 0x%p\n", tn40_ioctl.data[0], tn40_ioctl.data[1], tn40_ioctl.data[2], tn40_ioctl.buf);

    switch (tn40_ioctl.data[0])
    {
        case OP_INFO:
            switch(tn40_ioctl.data[1])
            {
                case 1:
                    tn40_ioctl.data[2] = 1;
                    break;
                case 2:
                    tn40_ioctl.data[2] = priv->phy_mdio_port;
                    break;
                default:
                    tn40_ioctl.data[2] = 0xFFFFFFFF;
                    break;
            }
            error = copyout(&tn40_ioctl, ifr_data_get_ptr(ifr), sizeof(tn40_ioctl));
            if (error)
                RET(error);
            break;

        case OP_READ_REG:
            error = bdx_range_check(priv, tn40_ioctl.data[1]);
            if (error < 0)
                return error;
            tn40_ioctl.data[2] = READ_REG(priv, tn40_ioctl.data[1]);
            DBG("read_reg(0x%x)=0x%x (dec %d)\n", tn40_ioctl.data[1], tn40_ioctl.data[2],
                tn40_ioctl.data[2]);
            error = copyout(&tn40_ioctl, ifr_data_get_ptr(ifr), sizeof(tn40_ioctl));
            if (error)
                RET(error);
            break;

        case OP_WRITE_REG:
            error = bdx_range_check(priv, tn40_ioctl.data[1]);
            if (error < 0)
                return error;
            WRITE_REG(priv, tn40_ioctl.data[1], tn40_ioctl.data[2]);
            break;

        case OP_MDIO_READ:
            if(priv->phy_mdio_port==0xFF) return -EINVAL;
            dev=(u16)(0xFFFF&(tn40_ioctl.data[1]>>16));
            addr=(u16)(0xFFFF&tn40_ioctl.data[1]);
            tn40_ioctl.data[2] =0xFFFF&PHY_MDIO_READ(priv,dev,addr);
            error = copyout(&tn40_ioctl, ifr_data_get_ptr(ifr), sizeof(tn40_ioctl));
            if (error)
                RET(error);
            break;

        case OP_MDIO_WRITE:
            if(priv->phy_mdio_port==0xFF) return -EINVAL;
            dev=(u16)(0xFFFF&(tn40_ioctl.data[1]>>16));
            addr=(u16)(0xFFFF&tn40_ioctl.data[1]);
            PHY_MDIO_WRITE(priv,dev,addr,(u16)(tn40_ioctl.data[2]));
            break;

#ifdef _TRACE_LOG_
        case op_TRACE_ON:
        	traceOn();
        	break;

        case op_TRACE_OFF:
        	traceOff();
        	break;

        case op_TRACE_ONCE:
        	traceOnce();
        	break;

        case op_TRACE_PRINT:
        	tracePrint();
        	break;
#endif
#ifdef TN40_MEMLOG
        case OP_MEMLOG_DMESG:
        	memLogDmesg();
        	break;

        case OP_MEMLOG_PRINT:
        {
            char 			*buf;
            uint 			buf_size;
            unsigned long 	bytes;

            error = 0;
 	 	 	buf = memLogGetLine(&buf_size);
          	if (buf != NULL)
         	{

          		tn40_ioctl.data[2] = min(tn40_ioctl.data[1], buf_size);
          		bytes = copyout(&tn40_ioctl, ifr_data_get_ptr(ifr), sizeof(tn40_ioctl));
 				bytes = copyout(&tn40_ioctl.buf, buf, tn40_ioctl.data[2]);
          		DBG("copyout %p %u return %lu\n", tn40_ioctl.buf, tn40_ioctl.data[2], bytes);
         	}
         	else
         	{
         		DBG("=================== EOF =================\n");
         		error = -EIO;
         	}
          	RET(error);
         	break;
        }
#endif
#ifdef TN40_DEBUG
        case OP_DBG:
        	switch (tn40_ioctl.data[1])
        	{
#ifdef _DRIVER_RESUME_DBG

        		pm_message_t pmMsg = {0};

        		case DBG_SUSPEND:
        			bdx_suspend(priv->pdev, pmMsg);
        			bdx_resume(priv->pdev);
        			break;

        		case DBG_RESUME:
        			bdx_resume(priv->pdev);
        			break;
#endif
        		case DBG_START_DBG:
        			DBG_ON;
        			break;

        		case DBG_STOP_DBG:
        			DBG_OFF;
        			break;
#ifdef TN40_COUNTERS
        		case DBG_PRINT_COUNTERS:
        			bdx_printCounters(priv);
        			break;

        		case DBG_CLEAR_COUNTERS:
        			bdx_clearCounters(priv);
        			break;

#endif

#ifdef RX_REUSE_PAGES
        		case DBG_PRINT_PAGE_TABLE:
        			dbg_printRxPageTable(priv);
        			break;
#endif
        		default:
        			dbg_printIoctl();
        			break;

        	}
        	break;
#endif // TN40_DEBUG

        default:
            RET(-EOPNOTSUPP);
    }

    return 0;

} // bdx_ioctl_priv()


/*************************************************************************
 *     Rx DB                                 *
 *************************************************************************/

static void bdx_rxdb_destroy(struct rxdb *db)
{
	ENTER;
	if (db->pkt.tn40_dmamap.va != NULL)
	{
		tn40_free_dma(&db->pkt);
	}
    if (db)
    {
    	free((void *)db, M_TN40);
    }
    EXIT;

} // bdx_rxdb_destroy()

//-------------------------------------------------------------------------------------------------

static struct rxdb *bdx_rxdb_create(struct bdx_priv *priv, bool bAlloc)
{
    struct rxdb	*db;
    tn40_dma_t 	pkt;
    int 		i;

	int rxfSize = priv->rxf_fifo0.m.memsz;
	u16 pktSize = priv->rxf_fifo0.m.pktsz;
	int nelem   = rxfSize / sizeof(struct rxf_desc);
    size_t size = sizeof(struct rxdb) + (nelem * sizeof(int)) + (nelem * sizeof(struct rx_map));
    DBG("bdx_rxdb_create() nelem %d\n", nelem);
    if (bAlloc)
    {
        db = malloc(size, M_TN40, M_NOWAIT);
        if (db)
        {
        	bzero((void *)db, size);
        	if (tn40_alloc_dma(priv, priv->rxf_fifo0.m.pktsz, &pkt) != 0)
        	{
				ERR("bdx_rxdb_create() failed to alloc %d\n", (int)pktSize);
				bdx_rxdb_destroy(db);
				db = NULL;
			}
        }
    }
    else
    {
    	db  = priv->rxdb0;
    	pkt = db->pkt;
    }
    if (likely(db != NULL))
    {
        db->stack 	 = (int *)(db + 1);
        db->elems 	 = (struct rx_map *)(db->stack + nelem);
        db->nelem 	 = nelem;
        db->top   	 = nelem;
        db->rxfSize  = rxfSize;
        db->rxfAvail = rxfSize;
        db->pkt		 = pkt;
        for (i = 0; i < nelem; i++)
        {
            db->stack[i] = nelem - i - 1; /* To make the first alloc close to db struct */
        }
    }

    return db;

} // bdx_rxdb_create()

//-------------------------------------------------------------------------------------------------

static inline int bdx_rxdb_alloc_elem(struct rxdb *db, int rxfdSize)
{
    BDX_ASSERT(db->top <= 0);

    db->rxfAvail -= rxfdSize;

    BDX_ASSERT(db->rxfAvail < 0);

    return db->stack[--(db->top)];

} // bdx_rxdb_alloc_elem()

//-------------------------------------------------------------------------------------------------

static inline void bdx_rxdb_free_elem(struct rxdb *db, unsigned n, int rxfdSize)
{
    BDX_ASSERT((n >= (unsigned)db->nelem));

    db->rxfAvail += rxfdSize;

    BDX_ASSERT((db->rxfAvail > db->rxfSize));

    db->stack[(db->top)++] = n;

} // bdx_rxdb_free_elem()

//-------------------------------------------------------------------------------------------------

static inline void *bdx_rxdb_addr_elem(struct rxdb *db, unsigned n)
{
    BDX_ASSERT((n >= (unsigned)db->nelem));

    return db->elems + n;

} // bdx_rxdb_addr_elem()

//-------------------------------------------------------------------------------------------------
/*
static inline int bdx_rxdb_nelem(struct rxdb *db)
{
    return db->nelem;

} // bdx_rxdb_nelem()
*/
//-------------------------------------------------------------------------------------------------

static inline int bdx_rxdb_available(struct rxdb *db)
{
    return db->top;

} // bdx_rxdb_available()

/*************************************************************************
 *     Rx Init                               *
 *************************************************************************/

/* bdx_rx_init - Initialize RX all related HW and SW resources
 * @priv       - NIC private structure
 *
 * Returns 0 on success, a negative value on failure
 *
 * bdx_rx_init creates rxf and rxd fifos, updates the relevant HW registers,
 * preallocates skbs for rx. It assumes that Rx is disabled in HW funcs are
 * grouped for better cache usage
 *
 * RxD fifo is smaller then RxF fifo by design. Upon high load, RxD will be
 * filled and packets will be dropped by the NIC without getting into the host
 * or generating interrupts. In this situation the host has no chance of
 * processing all the packets. Dropping packets by the NIC is cheaper, since it
 * takes 0 CPU cycles.
 */

/* TBD: Ensure proper packet size */

static u16 bdx_rx_set_mbuf_size(u16 pktsz)
{
	u16 mBufSize = 0;

	if (pktsz <= MCLBYTES)
	{
		mBufSize = MCLBYTES;
	}
	else if (pktsz <= MJUMPAGESIZE)
	{
		mBufSize = MJUMPAGESIZE;
	}
	else if (pktsz <= MJUM9BYTES)
	{
		mBufSize = MJUM9BYTES;
	}
	else if (pktsz <= MJUM16BYTES)
	{
		mBufSize = MJUM16BYTES;
	}
	else
	{
		ERR("invalid MTU %d !\n", (int)pktsz);
	}

	return mBufSize;

} // bdx_rx_set_mbuf_size()

//-------------------------------------------------------------------------------------------------

int bdx_rx_init(struct bdx_priv *priv)
{
	int rVal = -1;

    ENTER;
    do
    {
		if (bdx_fifo_init(priv, &priv->rxd_fifo0.m, priv->rxd_size,
						  regRXD_CFG0_0, regRXD_CFG1_0,
						  regRXD_RPTR_0, regRXD_WPTR_0, (const char*)"RXD"))
		{
			break;
		}
		if (bdx_fifo_init(priv, &priv->rxf_fifo0.m, priv->rxf_size,
						  regRXF_CFG0_0, regRXF_CFG1_0,
						  regRXF_RPTR_0, regRXF_WPTR_0, (const char*)"RXF"))
		{
			break;
		}
		priv->rxf_fifo0.m.pktsz = priv->mtu + VLAN_ETH_HLEN;
		if ((priv->rx_buf_size = bdx_rx_set_mbuf_size(priv->rxf_fifo0.m.pktsz)) == 0)
		{
			break;
		}
		if (!(priv->rxdb0 = bdx_rxdb_create(priv, true)))
		{
			break;
		}
		rVal = tn40_dma_tag_create(priv, priv->rx_buf_size, priv->rx_buf_size, 1, &priv->rx_dma_tag);
		if (rVal != 0)
		{
			ERR("tn40_dma_tag_create rx FAILED !\n");
			break;
		}

		rVal = 0;

    } while(0);

    RET(rVal);

} // bdx_rx_init()

//-------------------------------------------------------------------------------------------------

static void bdx_rx_free_buffers(struct bdx_priv *priv)
{
	struct rxdb 	*db   = priv->rxdb0;
    struct rx_map 	*dm;
    u16 i;

    ENTER;
    DBG("total = %d free = %d busy = %d\n", db->nelem, bdx_rxdb_available(db), db->nelem - bdx_rxdb_available(db));
    while (bdx_rxdb_available(db) > 0)
    {
        i        = bdx_rxdb_alloc_elem(db, 0);
        dm       = (struct rx_map *)bdx_rxdb_addr_elem(db, i);
        dm->mBuf = NULL;
    }
    for (i = 0; i < db->nelem; i++)
    {
        dm = (struct rx_map *)bdx_rxdb_addr_elem(db, i);
        if (dm->mBuf)
        {
        	m_free(dm->mBuf);
        	dm->mBuf = NULL;
        }
        bus_dmamap_destroy(priv->rx_dma_tag, dm->dma_map);
    }
    EXIT;

} // bdx_rx_free_buffers()

//-------------------------------------------------------------------------------------------------

void bdx_rx_free(struct bdx_priv *priv)
{
    ENTER;

    if (priv->rxdb0)
    {
        bdx_rx_free_buffers(priv);
        bdx_rxdb_destroy(priv->rxdb0);
        priv->rxdb0 = NULL;
    }
    bdx_fifo_free(priv, &priv->rxf_fifo0.m);
    bdx_fifo_free(priv, &priv->rxd_fifo0.m);
    bus_dma_tag_destroy(priv->rx_dma_tag);
    EXIT;

} // bdx_rx_free()

/*************************************************************************
 *     Rx Engine                             *
 *************************************************************************/

//-------------------------------------------------------------------------------------------------

static inline u16 tcpCheckSum(u16 *buf, u16 len, u16 *saddr, u16 *daddr, u16 proto)
{
	u32 		sum;
	u16			j = len;

	sum = 0;
	while (j > 1)
	{
		sum += *buf++;
		if (sum & 0x80000000)
		{
			sum = (sum & 0xFFFF) + (sum >> 16);
		}
		j -= 2;
	}
	if ( j & 1 )
	{
		sum += *((u8 *)buf);
	}
	// Add the tcp pseudo-header
	sum += *(saddr++);
	sum += *saddr;
	sum += *(daddr++);
	sum += *daddr;
	sum += htons(proto);
	sum += htons(len);
	// Fold 32-bit sum to 16 bits
	while (sum >> 16)
	{
			sum = (sum & 0xFFFF) + (sum >> 16);
	}
	// One's complement of sum

	//return ( (u16)(~sum)  );

	return ( (u16)(sum)  );

} // tcpCheckSum()

//-------------------------------------------------------------------------------------------------

#define PKT_ERR_LEN		(70)

static int bdx_rx_error(char *pkt, u32 rxd_err, u16 len)
{
	struct ether_header *eth = (struct ether_header *)pkt;
	struct ip           *iph = (struct ip *)(pkt +  sizeof(struct ether_header) + ((eth->ether_type == htons(ETHERTYPE_VLAN)) ? VLAN_HLEN : 0));
	int 				rVal = 1;

	if (rxd_err == 0x8)     // UDP checksum error
	{
		struct udphdr *udp = (struct udphdr *)((u8 *)iph + sizeof(struct ip));
		if (udp->uh_sum == 0)
		{
			DBG("false rxd_err = 0x%x\n", rxd_err);
			rVal = 0;           					// Work around H/W false error indication
		}
		else if (len < PKT_ERR_LEN)
		{
			u16 udpSum;
			udpSum = tcpCheckSum((u16 *)udp, htons(iph->ip_len) - (iph->ip_hl * sizeof(u32)), (u16 *)&iph->ip_src, (u16 *)&iph->ip_dst, IPPROTO_UDP);
			if (udpSum == 0xFFFF)
			{
				DBG("false rxd_err = 0x%x\n", rxd_err);
				rVal = 0;           				// Work around H/W false error indication
			}
		}
	}
	else if ((rxd_err == 0x10) && (len < PKT_ERR_LEN))     	// TCP checksum error
	{
		u16 tcpSum;
        struct tcphdr *tcp = (struct tcphdr *)((u8 *)iph + sizeof(struct ip));
        tcpSum = tcpCheckSum((u16 *)tcp, htons(iph->ip_len) - (iph->ip_hl * sizeof(u32)), (u16 *)&iph->ip_src, (u16 *)&iph->ip_dst, IPPROTO_TCP);
		if (tcpSum == 0xFFFF)
		{
			DBG("false rxd_err = 0x%x\n", rxd_err);
			rVal = 0;           					// Work around H/W false error indication
		}
	}

	return rVal;

} // bdx_rx_error()

//-------------------------------------------------------------------------------------------------

static inline int bdx_rx_map_mbuf(struct bdx_priv *priv, struct mbuf *mBuf, struct rx_map *dm, struct rxf_desc *rxfd)
{
	int 						nsegs;
	bus_dma_segment_t			seg[1];
	int							err  	= 0;
    struct pbl 					*pbl	= &rxfd->pbl[0];

    ENTER;

    do
    {
		if (dm->dma_map == NULL)
		{
			DBG("Creating DMA map\n");
			err = bus_dmamap_create(priv->rx_dma_tag, 0, &dm->dma_map);
			if (err != 0)
			{
				ERR("bdx_rx_map_mbuf() create FAILED !\n");
			}
		}
		else
		{
			DBG("Unloading DMA map\n");
			bus_dmamap_unload(priv->rx_dma_tag, dm->dma_map);
		}
		err = bus_dmamap_load_mbuf_sg(priv->rx_dma_tag, dm->dma_map, mBuf, seg, &nsegs, BUS_DMA_NOWAIT);
		if (err != 0)
		{
			ERR("bdx_rx_map_mbuf() load FAILED !\n");
			break;
		}
		bus_dmamap_sync(priv->rx_dma_tag, dm->dma_map, BUS_DMASYNC_PREREAD);
		pbl->len   			= (u32)CPU_CHIP_SWAP32(seg[0].ds_len);
		pbl->pa_lo 			= CPU_CHIP_SWAP32(L32_64(seg[0].ds_addr));
		pbl->pa_hi 			= CPU_CHIP_SWAP32(H32_64(seg[0].ds_addr));
		DBG("m %p dmap %p nsegs %d\n", mBuf, dm->dma_map, nsegs);
		dbg_printPBL(pbl);

    } while(0);
    RET(err);

} // bdx_rx_map_mbuf()

//-------------------------------------------------------------------------------------------------

int bdx_rx_alloc_buffers(struct bdx_priv *priv)
{
//	struct mbuf					*mBuf, *mPrev, **m;
//    int							j;
	struct mbuf					*mBuf;
    struct rxf_desc				*rxfd;
    struct rx_map 				*dm;
    int 						delta;
    int							rxfdSize;
	struct rxdb 				*db   	 = priv->rxdb0;
	struct rxf_fifo 			*f	  	 = &priv->rxf_fifo0;
    int							nBufs 	 = (f->m.pktsz + f->m.pktsz -1) / priv->rx_buf_size;;
    int							nFrags	 = nBufs -1;
    int							idx		 = 0;
    int 						nAllocs  = 0;

    ENTER;
 	DBG("bdx_rx_alloc_buffers() nelem %d pktsz %d avail %d first idx %d\n", db->nelem, f->m.pktsz, db->rxfAvail, db->stack[db->top-1]);
    while (db->rxfAvail > sizeof(struct rxf_desc))
    {
/*
    	mBuf = NULL;
    	m 	 = &mBuf;
     	for (j = 0; j < nBufs; j++)
    	{
    		*m = m_getcl(M_NOWAIT, MT_DATA, M_EXT);
    		//DBG("m %p\n", *m);
			if (*m == NULL)
			{
				if (mBuf != NULL)
				{
					m_freem(mBuf);
					mBuf = NULL;
				}
				break;
			}
			(*m)->m_len = priv->rx_buf_size;
			mPrev = *m;
			m 	  = &((*m)->m_next);
    	}
*/
     	mBuf = m_getjcl(M_NOWAIT, MT_DATA, M_PKTHDR,priv->rx_buf_size);
     	if (mBuf == NULL)
     	{
     		break;
     	}
//    	mBuf->m_pkthdr.len = priv->rx_buf_size * nBufs;
//    	mBuf->m_flags  |= M_PKTHDR;
//    	mPrev->m_flags |= M_EOR;
     	mBuf->m_len = mBuf->m_pkthdr.len = priv->rx_buf_size;
        rxfd     = (struct rxf_desc *)(f->m.va + f->m.wptr);
        rxfdSize = rxd_sizes[nFrags].bytes;
        if ((rxfdSize > db->rxfAvail) || (nFrags  < 0))
        {
            ERR("bdx_rx_alloc_buffers() no room: rxfdSize %d > rxfAvail %d , nFrags %d\n", rxfdSize, db->rxfAvail, nFrags);
            m_freem(mBuf);
            break;
        }
        idx      	= bdx_rxdb_alloc_elem(db, rxfdSize);
        dm       	= (struct rx_map *)bdx_rxdb_addr_elem(db, idx);
        if (bdx_rx_map_mbuf(priv, mBuf, dm, rxfd) != 0)
        {
        	bdx_rxdb_free_elem(db, idx, rxfdSize);
         	m_freem(mBuf);
        	break;
        }
        dbg_printMbuf(mBuf,__LINE__);
        dm->mBuf  	= mBuf;
        rxfd->info  = CPU_CHIP_SWAP32(0x10000 | rxd_sizes[nFrags].qwords); /* INFO =1 */
        rxfd->va_lo = idx;
        rxfd->va_hi = rxfdSize;
       	DBG("idx %d nFrags %d info 0x%x\n", idx, nFrags, rxfd->info);
       	if ((idx % 200) == 0)
        {
        	DBG("idx %d nFrags %d info 0x%x\n", idx, nFrags, rxfd->info);
        	dbg_print_rxfd(rxfd);
        }
        f->m.wptr  += rxfdSize;
        delta       = f->m.wptr - f->m.memsz;
        if (unlikely(delta >= 0))
        {
            f->m.wptr = delta;
            if (delta > 0)
            {
                memcpy(f->m.va, f->m.va + f->m.memsz, delta);
                DBG("Wrapped descriptor %d\n", idx);
            }
        }
        nAllocs += 1;
    }
    WRITE_REG(priv, f->m.reg_WPTR, f->m.wptr & TXF_WPTR_WR_PTR);
	DBG("bdx_rx_alloc_buffers() allocated %u last idx %d avail %d rxfd size %d wptr %d\n", nAllocs, idx, db->rxfAvail, rxd_sizes[nFrags].bytes, (int)f->m.wptr);
    //DBG("WRITE_REG 0x%04x f->m.reg_WPTR 0x%x\n", f->m.reg_WPTR, f->m.wptr & TXF_WPTR_WR_PTR);
    //DBG("READ_REG  0x%04x f->m.reg_RPTR=0x%x\n", f->m.reg_RPTR, READ_REG(priv, f->m.reg_RPTR));
    //DBG("READ_REG  0x%04x f->m.reg_WPTR=0x%x\n", f->m.reg_WPTR, READ_REG(priv, f->m.reg_WPTR));
    //dbg_printFifo(priv, &priv->rxf_fifo0.m, (char *)"RXF");

	RET(nAllocs);

} // bdx_rx_alloc_buffers()

//-------------------------------------------------------------------------------------------------

void bdx_rx_receive(struct bdx_priv *priv)
{
    struct rxd_desc *rxdd;
    struct rx_map 	*dm;
    struct mbuf 	*mBuf;
    int 			tmp_len, size;
    u32 			rxd_val1, rxd_err, pktType;
    u16 			len;
    u16 			rxd_vlan;
    tn40_priv_t		*tn40_priv = CONTAINING_RECORD(priv, tn40_priv_t, bdx_priv);
	struct rxd_fifo *f    = &priv->rxd_fifo0;
    struct rxdb 	*db   = priv->rxdb0;
    u32				nRcv  = 0;

    ENTER;

	bus_dmamap_sync(f->m.dma.dma_tag, f->m.dma.tn40_dmamap.dma_map, BUS_DMASYNC_POSTREAD);
	f->m.wptr = READ_REG(priv, f->m.reg_WPTR) & TXF_WPTR_WR_PTR;
    size = f->m.wptr - f->m.rptr;
    if (size < 0)
    {
        size += f->m.memsz;     /* Size is negative :-) */
    }
   	DBG("<-- bdx_rx_receive() size %d\n", size);
    while (size > 0)
    {
        rxdd = (struct rxd_desc *)(f->m.va + f->m.rptr);
        /*
         * Note: We have a chicken and egg problem here. If the
         *       descriptor is wrapped we first need to copy the tail
         *       of the descriptor to the end of the buffer before
         *       extracting values from the descriptor. However in
         *       order to know if the descriptor is wrapped we need to
         *       obtain the length of the descriptor from (the
         *       wrapped) descriptor. Luckily the length is the first
         *       word of the descriptor. Descriptor lengths are
         *       multiples of 8 bytes so in case of a wrapped
         *       descriptor the first 8 bytes guaranteed to appear
         *       before the end of the buffer. We first obtain the
         *       length, we then copy the rest of the descriptor if
         *       needed and then extract the rest of the values from
         *       the descriptor.
         *
         *   Do not change the order of operations as it will
         *       break the code!!!
         */
        rxd_val1 = CPU_CHIP_SWAP32(rxdd->rxd_val1);
        tmp_len  = GET_RXD_BC(rxd_val1) << 3;
        pktType  = GET_RXD_PKT_ID(rxd_val1);
        BDX_ASSERT(tmp_len <= 0);
        size    -= tmp_len;
        /* CHECK FOR A PARTIALLY ARRIVED DESCRIPTOR */
        if (size < 0)
        {
        	DBG("<-- bdx_rx_receive() partial descriptor\n");
            break;
        }
        /* HAVE WE REACHED THE END OF THE QUEUE? */
        f->m.rptr += tmp_len;
        tmp_len    = f->m.rptr - f->m.memsz;
        if (unlikely(tmp_len >= 0))
        {
            f->m.rptr = tmp_len;
            if (tmp_len > 0)
            {
                /* COPY PARTIAL DESCRIPTOR TO THE END OF THE QUEUE */
                DBG("wrapped desc rptr=%d tmp_len=%d\n", f->m.rptr, tmp_len);
                memcpy(f->m.va + f->m.memsz, f->m.va, tmp_len);
            }
        }
        dm       = (struct rx_map *)bdx_rxdb_addr_elem(db, rxdd->va_lo);
        len      = CPU_CHIP_SWAP16(rxdd->len);
        rxd_vlan = CPU_CHIP_SWAP16(rxdd->rxd_vlan);
        mBuf 	 = dm->mBuf;
        //DBG("mBuf %p dmap %p\n", mBuf, dm->dma_map);
        bus_dmamap_sync(priv->rx_dma_tag, dm->dma_map, BUS_DMASYNC_POSTREAD);
        dbg_print_rxdd(rxdd, rxd_val1, len, rxd_vlan);
        nRcv += 1;
        /* CHECK FOR ERRORS */
        if (unlikely((rxd_err = GET_RXD_ERR(rxd_val1))))
        {
        	int  bErr = 1;
        	char *pkt = NULL;
			if ( ( !( rxd_err &  0x4))		&& 										// NOT CRC error
				 (
					((rxd_err == 0x8)  && (pktType == 2))		||    				// UDP checksum error
					((rxd_err == 0x10) && (len < PKT_ERR_LEN) && (pktType == 1))	// TCP checksum error
				 )
			   )
			{
				pkt = malloc(len, M_TN40, M_NOWAIT);
				DBG("rxd_err 0x%x dm->mBu %p len %d pkt %p\n", rxd_err, mBuf, len, pkt);
				if (pkt)
				{
					m_copydata (mBuf, 0, len, pkt);
					bErr = bdx_rx_error(pkt, rxd_err, len);
				}
			}
			if (bErr)
			{
				ERR("rxd_err 0x%x len %d\n", rxd_err, (int)len);
//#define	DUMP_RX_ERRORS
#ifdef DUMP_RX_ERRORS
				dbg_printRxPkt(mBuf, pkt, len);
				// Debug code : dump packet and do not drop
#else
				//priv->net_stats.rx_errors++;
				// Drop bad packets
				bdx_rxdb_free_elem(db, rxdd->va_lo, rxdd->va_hi);
				m_freem(mBuf);
				if (pkt != NULL)
				{
					free((void *)pkt, M_TN40);
				}
				continue;
#endif
			}
        }
        /* PROCESS PACKET */
/*
		if (len <= 128)
		{
			struct mbuf *m = m_gethdr(M_NOWAIT, MT_DATA);
			if (m != NULL)
			{
				memcpy(m->m_data, mBuf->m_data, len);
				mBuf = m;
				m_freem(mBuf);
			}
		}
*/
        mBuf->m_len = mBuf->m_pkthdr.len = len;
        mBuf->m_pkthdr.rcvif  = tn40_priv->ifp;
 //       mBuf->m_pkthdr.flowid = 0;
 //       M_HASHTYPE_SET(mBuf, M_HASHTYPE_OPAQUE);
        if (GET_RXD_VTAG(rxd_val1))/* Vlan case */
        {
        	mBuf->m_pkthdr.ether_vtag = GET_RXD_VLAN_TCI(rxd_vlan);
        	mBuf->m_flags |= M_VLANTAG;
        }
		DBG("<-- bdx_rx_receive() mbuf %p len %u rxfdSize %d type %d\n", mBuf, (u32)len, rxdd->va_hi, pktType);
        tn40_checksum_results(mBuf, pktType);
		dbg_printMbuf(mBuf, __LINE__);
        dbg_printRxPkt(mBuf, NULL, len);
		if_input(tn40_priv->ifp, mBuf);
		DBG("<-- bdx_rx_receive rxAvail %d\n", db->rxfAvail);
	    bdx_rxdb_free_elem(db, rxdd->va_lo, rxdd->va_hi);
    }
   /* CLEANUP */
    WRITE_REG(priv, f->m.reg_RPTR, f->m.rptr & TXF_WPTR_WR_PTR);
    bus_dmamap_sync(f->m.dma.dma_tag, f->m.dma.tn40_dmamap.dma_map, BUS_DMASYNC_PREWRITE);
    bdx_rx_alloc_buffers(priv);
   	DBG("bdx_rx_receive() received %u\n", nRcv);
    EXIT;

} // bdx_rx_receive()

/*************************************************************************
 *     Tx DB                                 *
 *************************************************************************/
/* __bdx_tx_ptr_next - A helper function, increment read/write pointer + wrap.
 *
 * @d   - Tx data base
 * @ptr - Read or write pointer
 */
static inline void __bdx_tx_db_ptr_next(struct txdb *db, struct tx_map **pptr)
{
    BDX_ASSERT(db == NULL || pptr == NULL); /* sanity */
    BDX_ASSERT(*pptr != db->rptr && /* expect either read */
               *pptr != db->wptr);  /* or write pointer */
    BDX_ASSERT(*pptr < db->start || /* pointer has to be */
               *pptr >= db->end);   /* in range */

    ++*pptr;
    if (unlikely(*pptr == db->end))
        *pptr = db->start;
}

//-------------------------------------------------------------------------------------------------
/* bdx_tx_db_inc_rptr - Increment the read pointer.
 *
 * @d - tx data base
 */
static inline void bdx_tx_db_inc_rptr(struct txdb *db)
{
    BDX_ASSERT(db->rptr == db->wptr);   /* can't read from empty db */
    __bdx_tx_db_ptr_next(db, &db->rptr);
}

//-------------------------------------------------------------------------------------------------
/* bdx_tx_db_inc_rptr - Increment the   write pointer.
 *
 * @d - tx data base
 */
static inline void bdx_tx_db_inc_wptr(struct txdb *db)
{
    __bdx_tx_db_ptr_next(db, &db->wptr);
    BDX_ASSERT(db->rptr == db->wptr);   /* we can not get empty db as
                           a result of write */
}

//-------------------------------------------------------------------------------------------------
/* bdx_tx_db_init - Create and initialize txdb.
 *
 * @db      - tx data base
 * @sz_type - size of tx fifo
 * Returns 0 on success, error code otherwise
 */
static int bdx_tx_db_init(struct bdx_priv *priv, int sz_type)
{
	int j, err = 0;
    int memsz = FIFO_SIZE * (1 << (sz_type + 1));
    struct txdb *db = &priv->txdb;

    db->start = malloc(memsz, M_TN40, M_NOWAIT);
    if (!db->start)
    {
    	ERR("bdx_tx_db_init() malloc FAILED ! (%d)\n", memsz);
        return -1;
    }
    bzero((void *)db->start, memsz);
    /*
     * In order to differentiate between an empty db state and a full db
     * state at least one element should always be empty in order to
     * avoid rptr == wptr, which means that the db is empty.
     */
    db->size = memsz / sizeof(struct tx_map) - 1;
    db->end  = db->start + db->size + 1;   /* just after last element */

    /* All dbs are created empty */
    db->rptr = db->start;
    db->wptr = db->start;
    for (j = 0; j < db->size; j++)
    {
    	err = bus_dmamap_create(priv->tx_dma_tag, 0, &db->wptr->dma_map);
    	if (err != 0)
    	{
    		free(db->start, M_TN40);
    		ERR("bus_dmamap_create() FAILD at %d\n", j);
    		break;
    	}
    	bdx_tx_db_inc_wptr(db);
    }
    db->wptr = db->start;

    return err;

} // bdx_tx_db_init()

//-------------------------------------------------------------------------------------------------
/* bdx_tx_db_close - Close tx db and free all memory.
 *
 * @d - tx data base
 */
static void bdx_tx_db_close(struct bdx_priv *priv)
{
	int j;
	struct txdb *db = &priv->txdb;

    BDX_ASSERT(db == NULL);

    if (db->start)
    {
    	db->wptr = db->start;
        for (j = 0; j < db->size; j++)
        {
        	bus_dmamap_destroy(priv->tx_dma_tag, db->wptr->dma_map);
        }
    	free(db->start, M_TN40);
        db->start = NULL;
    }
} // bdx_tx_db_close()

//-------------------------------------------------------------------------------------------------

static int desc_lwords(int minSize, int nFrags)
{
	int lwords;

    // 3 - is number of lwords used for every additional phys buffer

    lwords = minSize + (nFrags * 3);
    if (lwords & 1)
    {
    	lwords++;   /* pad it with 1 lword */
    }

    return lwords;

} // desc_lwords

//-------------------------------------------------------------------------------------------------

/*
 * init_txd_sizes - Pre-calculate the sizes of descriptors for skbs up to 16
 * frags The number of frags is used as an index to fetch the correct
 * descriptors size, instead of calculating it each time
 */
static void __init init_txd_sizes(void)
{
    int i, lwords;

    /* 7 - is number of lwords in txd with one phys buffer */
    DBG("txd_size:\n");
    for (i = 0; i < MAX_PBL; i++)
    {
        lwords = desc_lwords(7, i);
        txd_sizes[i].qwords = lwords >> 1;
        txd_sizes[i].bytes  = lwords << 2;
        DBG("%2d. %d\n", i, txd_sizes[i].bytes);
    }

} // init_txd_sizes()

//-------------------------------------------------------------------------------------------------

static void __init init_rxd_sizes(void)
{
    int i, lwords;

    /* 6 - is number of lwords in rxfd with one phys buffer */
    DBG("rxd_size:\n");
    for (i = 0; i < MAX_PBL; i++)
    {
        lwords = desc_lwords(6, i);
        rxd_sizes[i].qwords = lwords >> 1;
        rxd_sizes[i].bytes  = lwords << 2;
        DBG("%2d. %d\n", i, rxd_sizes[i].qwords);
    }

} // init_rxd_sizes()

//-------------------------------------------------------------------------------------------------
/*
 * bdx_tx_init - Initialize all Tx related stuff.  Namely, TXD and TXF fifos,
 *       database etc
 */
int bdx_tx_init(struct bdx_priv *priv)
{

	int 						rVal 		 = -1;

	do
	{
		if (bdx_fifo_init(priv, &priv->txd_fifo0.m, priv->txd_size,
						  regTXD_CFG0_0, regTXD_CFG1_0,
						  regTXD_RPTR_0, regTXD_WPTR_0, (const char*)"TXD"))
		{
			break;
		}
		if (bdx_fifo_init(priv, &priv->txf_fifo0.m, priv->txf_size,
						  regTXF_CFG0_0, regTXF_CFG1_0,
						  regTXF_RPTR_0, regTXF_WPTR_0, (const char*)"TXF"))
		{
			break;
		}
		/*
		 * The TX db has to keep mappings for all packets sent (on TxD)
		 * and not yet reclaimed (on TxF)
		 */
		if (bdx_tx_db_init(priv, MAX(priv->txd_size, priv->txf_size)))
		{
			break;
		}
		// SHORT_PKT_FIX
		if (tn40_alloc_dma(priv, SHORT_PKT_LEN, &priv->short_pkt) != 0)
		{
			ERR("short packet alloc FAILED !\n");
			break;
		}
		bzero((void *)priv->short_pkt.tn40_dmamap.va, SHORT_PKT_LEN);
		// SHORT_PKT_FIX end

		priv->tx_level = BDX_MAX_TX_LEVEL;
		priv->tx_update_mark = priv->tx_level - 1024;

		rVal = 0;
	} while(0);
	if (rVal != 0)
	{
		ERR("bdx_tx_init FAILED ! (%d)\n", rVal);
	}

	RET(rVal);
} // bdx_tx_init()

//-------------------------------------------------------------------------------------------------

void bdx_tx_free(struct bdx_priv *priv)
{
    ENTER;
    bdx_fifo_free(priv, &priv->txd_fifo0.m);
    bdx_fifo_free(priv, &priv->txf_fifo0.m);
    bdx_tx_db_close(priv);
    // SHORT_PKT_FIX
    if(priv->short_pkt.tn40_dmamap.va != NULL)
    {
    	tn40_free_dma(&priv->short_pkt);
    }
    // SHORT_PKT_FIX end

} // bdx_tx_free()

//-------------------------------------------------------------------------------------------------

int bdx_probe(struct bdx_priv *priv)
{
	int err = -1;
	do
	{
		bdx_reset(priv);
		print_hw_id(priv);
		if ((READ_REG(priv,FPGA_VER) & 0xFFF) == 308)
		{
			DBG("HW statistics not supported\n");
			priv->stats_flag = 0;
		}
		bdx_mdio_reset(priv, 0);
		/* Initialize fifo sizes. */
		priv->name     = (const char*)TN40_DRV_NAME;
		priv->txd_size = 3;
		priv->txf_size = 3;
		priv->rxd_size = 3;
		priv->rxf_size = 3;
		priv->mtu	   = 1500;
		/* Initialize the initial coalescing registers. */
		priv->rdintcm = INT_REG_VAL(0x20, 1, 4, 12);
		priv->tdintcm = INT_REG_VAL(0x20, 1, 0, 12);
		traceInit();
		init_txd_sizes();
		init_rxd_sizes();
		print_driver_id();
		if (bdx_read_mac(priv))
		{
			ERR("load MAC address failed\n");
			break;
		}
		bdx_reset(priv);
		WRITE_REG(priv, 0x51E0,0x30010006); 			// GPIO_OE_ WR CMD
		WRITE_REG(priv, 0x51F0,0x0); 					// GPIO_OE_ DATA
		WRITE_REG(priv, regMDIO_CMD_STAT, 0x3ec8);
		if ((err = bdx_tx_init(priv)))
		{
			break;
		}
		if ((err = bdx_fw_load(priv)))
		{
			break;
		}
		bdx_tx_free(priv);
		print_eth_id(priv);
#ifdef TN40_MEMLOG
		memLogInit();
#endif

		err = 0;
	} while(0);

    RET(err);

} // bdx_probe()

//-------------------------------------------------------------------------------------------------

/**
 * bdx_remove - Device removal routine.
 *
 * @pdev - PCI device information struct
 *
 * bdx_remove is called by the PCI subsystem to notify the driver
 * that it should release a PCI device.  This could be caused by a
 * Hot-Plug event, or because the driver is going to be removed from
 * memory.
 */
void bdx_remove(struct bdx_priv *priv)
{

	bdx_hw_stop(priv);
	bdx_sw_reset(priv);
	bdx_rx_free(priv);
	bdx_tx_free(priv);
#if  (defined VM_KLNX)
	bdx_freeAllCmem(priv);
#endif

    MSG("Device removed\n");

    EXIT;

} // bdx_remove()


 //-------------------------------------------------------------------------------------------------
 // Use PMF to accept first MAC_MCST_NUM (15) addresses and accept the rest of addresses through IMF.
 //
 // TODO: Sort the addresses and write them in ascending order into RX_MAC_MCST regs.
 //
 // Currently we skip this phase now and accept ALL the multicast frames through IMF.

 //-------------------------------------------------------------------------------------------------

void bdx_setbroadcast(struct bdx_priv *priv, int flags)
{
	int j;
	u32 rxf_val = GMAC_RX_FILTER_AM | GMAC_RX_FILTER_AB | GMAC_RX_FILTER_OSEN | GMAC_RX_FILTER_TXFC;

	ENTER;
	if (flags & IFF_PROMISC)
	{
		rxf_val |= GMAC_RX_FILTER_PRM;
	}
	if (flags & IFF_ALLMULTI)
	{
		/* set IMF to accept all multicast frames */
		for (j = 0; j < MAC_MCST_HASH_NUM; j++)
		{
			WRITE_REG(priv, regRX_MCST_HASH0 + j * 4, ~0);
		}
	}
	if (flags & IFF_BROADCAST)
	{
		rxf_val |= GMAC_RX_FILTER_AB;
	}

	WRITE_REG(priv, regGMAC_RXF_A, rxf_val);
	EXIT;

} // bdx_set_broadcast()

//-------------------------------------------------------------------------------------------------

static u_int bdx_setmulti_cb(void *arg, struct sockaddr_dl *sdl, u_int cnt)
{
	struct bdx_priv	*priv = arg;
	u8				hash = 0;
	u32				reg, val;
	int				j;

	for (j = 0; j < ETHER_ADDR_LEN; j++)
	{
		hash ^= LLADDR(sdl)[j];
	}
	reg = regRX_MCST_HASH0 + ((hash >> 5) << 2);
	val = READ_REG(priv, reg);
	val |= (1 << (hash % 32));
	WRITE_REG(priv, reg, val);
	return (1);
}

void bdx_setmulti(struct bdx_priv *priv)

{
	int 				j;
	tn40_priv_t			*tn40_priv 	= CONTAINING_RECORD(priv, tn40_priv_t, bdx_priv);
	if_t	 		ifp 		= tn40_priv->ifp;

	ENTER;
	// Set IMF to deny all multicast frames
	for (j = 0; j < MAC_MCST_HASH_NUM; j++)
	{
		WRITE_REG(priv, regRX_MCST_HASH0 + j * 4, 0);
	}
	// Set PMF to deny all multicast frames
	for (j = 0; j < MAC_MCST_NUM; j++)
	{
		WRITE_REG(priv, regRX_MAC_MCST0 + j * 8, 0);
		WRITE_REG(priv, regRX_MAC_MCST1 + j * 8, 0);
	}
	if_foreach_lladdr(ifp, bdx_setmulti_cb, priv);
	EXIT;

 } // bdx_setmulti()

//-------------------------------------------------------------------------------------------------

static u64 bdx_read_l2stat(struct bdx_priv *priv, int reg)
{
	u64 val;

	val = READ_REG(priv, reg);
	val |= ((u64) READ_REG(priv, reg + 8)) << 32;
	return val;
} // bdx_read_l2stat()

//-------------------------------------------------------------------------------------------------

void bdx_update_stats(struct bdx_priv *priv)
{
	struct bdx_stats 	*stats = &priv->hw_stats;
	u64 				*stats_vector = (u64 *) stats;
	int 				i;
	int 				addr;

	/*Fill HW structure */
	addr = 0x7200;

	/*First 12 statistics - 0x7200 - 0x72B0 */
	for (i = 0; i < 12; i++)
	{
		stats_vector[i] = bdx_read_l2stat(priv, addr);
		addr += 0x10;
	} BDX_ASSERT(addr != 0x72C0);

	/* 0x72C0-0x72E0 RSRV */
	addr = 0x72F0;
	for (; i < 16; i++)
	{
		stats_vector[i] = bdx_read_l2stat(priv, addr);
		addr += 0x10;
	} BDX_ASSERT(addr != 0x7330);

	/* 0x7330-0x7360 RSRV */
	addr = 0x7370;
	for (; i < 19; i++)
	{
		stats_vector[i] = bdx_read_l2stat(priv, addr);
		addr += 0x10;
	} BDX_ASSERT(addr != 0x73A0);

	/* 0x73A0-0x73B0 RSRV */
	addr = 0x73C0;
	for (; i < 23; i++)
	{
		stats_vector[i] = bdx_read_l2stat(priv, addr);
		addr += 0x10;
	}

	BDX_ASSERT(addr != 0x7400); BDX_ASSERT((sizeof(struct bdx_stats) / sizeof(u64)) != i);

} // bdx_update_stats()

//-------------------------------------------------------------------------------------------------

static inline void
tn40_map_mbuf(struct bdx_priv *priv, struct mbuf *mBuf, struct txd_desc *txdd, bus_dma_segment_t *segs, int nr_frags, size_t pktLen)
{
	int i;
    struct txdb 	*db 	= &priv->txdb;
    struct pbl 		*pbl 	= &txdd->pbl[0];
    u32				tLen    = 0;

    DBG1("=== nr_frags : %d pktLen %lu================\n", nr_frags, pktLen);
    DBG1("=== dma_addr : 0x%lx   ================\n", segs->ds_addr);
    for (i = 0; i <= nr_frags; i++)
    {
        db->wptr->len  		= segs->ds_len;
        pbl->len   			= CPU_CHIP_SWAP32(db->wptr->len);
        pbl->pa_lo 			= CPU_CHIP_SWAP32(L32_64(segs->ds_addr));
        pbl->pa_hi 			= CPU_CHIP_SWAP32(H32_64(segs->ds_addr));
        dbg_printPBL(pbl);
        bdx_tx_db_inc_wptr(db);
        tLen += segs->ds_len;
        DBG1("pbl[%d]: ph 0x%08x pl 0x%08x l %u\n", i, pbl->pa_hi, pbl->pa_lo, pbl->len);
        pbl++;
        segs++;
    }
    if (pktLen != tLen)
    {
    	ERR("tn40MapMbuf() BAD packet length %u != %lu\n", tLen, pktLen);
    }
    if(pktLen < 60)
    {
    	++nr_frags;	// SHORT_PKT_FIX
    }
    DBG1("tn40MapMbuf() packet len %u segments %d\n", tLen, nr_frags);
    /* Add mbuf clean up info. */
    db->wptr->len       = -txd_sizes[nr_frags].bytes;
    db->wptr->mBuf = mBuf;
    bdx_tx_db_inc_wptr(db);

} // tn40_map_mbuf()

//-------------------------------------------------------------------------------------------------

/* bdx_tx_transmit - Send a packet to the NIC.
 *
 * @skb - Packet to send
 */
#define		MAX_SHRIK_TRIES		(5)		// safety counter
int bdx_tx_transmit(struct bdx_priv *priv, struct mbuf *mBuf)
{
    struct txd_desc 			*txdd;
    int 						len;
    int							nr_frags;
    size_t						pktLen;
    int							txTrigger;
    int							err;
    tn40_priv_t					*tn40_priv 	 = CONTAINING_RECORD(priv, tn40_priv_t, bdx_priv);
    struct txdb 				*db 		 = &priv->txdb;
    u32							coLen		 = 0;
    int 						txd_checksum = 0;   /* no checksum */
    int 						txd_vlan_id  = 0;
    int 						txd_vtag     = 0;
    int 						shrik_tries  = 0;
    u32 						txd_mss      = 0;
    u32							txd_lgsnd    = 0;
    struct txd_fifo 			*f 			 = &priv->txd_fifo0;
    bus_dma_segment_t 			segs[MAX_SEGMENTS];

    ENTER;

    pktLen 		   = mBuf->m_pkthdr.len;

	DBG("--> m %p len %lu\n", mBuf, pktLen);
	//    mtx_lock(&priv->tx_lock);
    // traceAdd(0x11, pktLen);
    do
    {
    	do
    	{
    		err = bus_dmamap_load_mbuf_sg(priv->tx_dma_tag, db->wptr->dma_map, mBuf, segs, &nr_frags, BUS_DMA_NOWAIT);
    		if (err != 0)
    		{
    			if (err == EFBIG)
    			{
    				nr_frags = MAX_SEGMENTS;
    			}
    			else
    			{
					ERR("bdx_tx_transmit() bus_dmamap_load_mbuf_sg 1 FAILED %d\n", err);
					dbg_printMbuf(mBuf, __LINE__);
					break;
    			}
    		}
    		if (nr_frags > MAX_PBL)
    		{
    			shrik_tries += 1;
    			dbg_printMbuf(mBuf,__LINE__);
    			dbg_printSges(segs, nr_frags);
    			if ((err = tn40_mbuf_shrink(mBuf, nr_frags)) != 0)
    			{
					ERR("tn40_mbuf_shrink() FAILED %d\n", err);
    				break;
    			}
    		}
    	} while ((nr_frags > MAX_PBL) && (shrik_tries < MAX_SHRIK_TRIES));

		if (nr_frags > MAX_PBL)
		{
			err = ENOBUFS;
		}
		if (err != 0)
		{
			ERR("bdx_tx_transmit() max PBL exceeded %d !\n", nr_frags);
			break;
		}
		bus_dmamap_sync(priv->tx_dma_tag, db->wptr->dma_map, BUS_DMASYNC_PREWRITE);
 		nr_frags -= 1;
		DBG1("bdx_tx_transmit() coLen %u\n", coLen);
		if (mBuf->m_pkthdr.csum_flags & CSUM_IP ) txd_checksum |= 0x4;   	// Turn on H/W IP  checksum
		if (mBuf->m_pkthdr.csum_flags & CSUM_TCP) txd_checksum |= 0x2;		// Turn on H/W TCP checksum
		else
		if (mBuf->m_pkthdr.csum_flags & CSUM_UDP) txd_checksum |= 0x1;		// Turn on H/W UDP checksum
		/* Build tx descriptor */
		BDX_ASSERT(f->m.wptr >= f->m.memsz);    /* started with valid wptr */
		txdd = (struct txd_desc *)(f->m.va + f->m.wptr);
		if (mBuf->m_flags & M_VLANTAG)
		{
			txd_vlan_id = htole16(mBuf->m_pkthdr.ether_vtag);
			DBG("txd_vlan_id 0x%x\n", txd_vlan_id);
			txd_vtag = 1;
		}
		if (mBuf->m_pkthdr.csum_flags & CSUM_IP_TSO)
		{
			txd_mss   = mBuf->m_pkthdr.tso_segsz;
			txd_lgsnd = 1;
		}
	    ////////////////////////
	    if (txd_mss >= pktLen)
	    {
	    	txd_mss   = 0;
	    	txd_lgsnd = 0;
	    }
	    ////////////////////////
		txdd->length   = CPU_CHIP_SWAP16(pktLen);
		txdd->mss      = CPU_CHIP_SWAP16(txd_mss);
		txdd->txd_val1 = CPU_CHIP_SWAP32(TXD_W1_VAL(txd_sizes[nr_frags].qwords,
										 txd_checksum, txd_vtag,
										 txd_lgsnd, txd_vlan_id));
		txdd->va_hi    = coLen;
		//txdd->va_lo	   = (u32)((u64)mBuf);
		tn40_map_mbuf(priv, mBuf, txdd , segs, nr_frags, pktLen);
		// SHORT_PKT_FIX
		if(pktLen < 60)
		{
			struct pbl *pbl = &txdd->pbl[++nr_frags];
			txdd->length    = CPU_CHIP_SWAP16(60);
			txdd->txd_val1  = CPU_CHIP_SWAP32(TXD_W1_VAL(txd_sizes[nr_frags].qwords, txd_checksum, txd_vtag,txd_lgsnd, txd_vlan_id));
			pbl->len        = (u32)CPU_CHIP_SWAP32(60 - pktLen);
			pbl->pa_lo 		= CPU_CHIP_SWAP32(L32_64(priv->short_pkt.tn40_dmamap.pa));
			pbl->pa_hi 		= CPU_CHIP_SWAP32(H32_64(priv->short_pkt.tn40_dmamap.pa));
			DBG1("=== SHORT_PKT_FIX   ================\n");
			DBG1("=== nr_frags : %d   ================\n", nr_frags);
			dbg_printPBL(pbl);
			COUNT_ADD(priv->counters, tx_fixed_bytes, 60 - pktLen);
		}
		// SHORT_PKT_FIX end
		DBG1("=== txdd %p =====\n" , txdd);
		DBG1("=== w1 qwords[%d] %d =====\n" , nr_frags, txd_sizes[nr_frags].qwords);
		DBG1("=== TxD desc =====================\n");
		DBG1("=== w1: 0x%x ================\n", txdd->txd_val1);
		DBG1("=== w2: mss 0x%x len 0x%x\n", txdd->mss, txdd->length);
		/*
		 * Increment TXD write pointer. In case of fifo wrapping copy reminder of
		 *  the descriptor to the beginning
		*/
		f->m.wptr += txd_sizes[nr_frags].bytes;
		len        = f->m.wptr - f->m.memsz;
		if (unlikely(len >= 0))
		{
			f->m.wptr = len;
			if (len > 0)
			{
				BDX_ASSERT(len > f->m.memsz);
				memcpy(f->m.va, f->m.va + f->m.memsz, len);
			}
		}
		DBG("--> bdx_tx_transmit() mbuf %p wptr %u\n", mBuf, f->m.wptr);
		BDX_ASSERT(f->m.wptr >= f->m.memsz);    /* finished with valid wptr */
		priv->tx_level -= txd_sizes[nr_frags].bytes;
		BDX_ASSERT(priv->tx_level <= 0 || priv->tx_level > BDX_MAX_TX_LEVEL);
		DBG1("tx_level = %d tx_update_mark = %d tx_noupd =%d\n",priv->tx_level, priv->tx_update_mark, priv->tx_noupd);
		txTrigger = 0;
		if (priv->tx_level > priv->tx_update_mark)
		{
			txTrigger = 1;
		}
		else
		{
			if (priv->tx_noupd++ > BDX_NO_UPD_PACKETS)
			{
				priv->tx_noupd = 0;
				txTrigger      = 1;
			}
		}
		COUNT_INC(priv->counters, tx_pkts);
	    COUNT_ADD(priv->counters, tx_bytes, pktLen);
		if (txTrigger)
		{
			WRITE_REG(priv, f->m.reg_WPTR, f->m.wptr & TXF_WPTR_WR_PTR);
			DBG1("WRITE_REG 0x%04x f->m.reg_WPTR 0x%x\n", f->m.reg_WPTR, f->m.wptr & TXF_WPTR_WR_PTR);
			bus_dmamap_sync(f->m.dma.dma_tag, f->m.dma.tn40_dmamap.dma_map, BUS_DMASYNC_PREWRITE);
		}
		DBG("--> bdx_tx_transmit() TX %s triggered rptr %u wptr %u tx_level %d tx_update_mark %d tx_noupd %d \n",
			txTrigger ? "" : "*NOT*", READ_REG(priv, f->m.reg_RPTR) & TXF_WPTR_WR_PTR, f->m.wptr, priv->tx_level, priv->tx_update_mark, priv->tx_noupd);
		dbg_printRegs(priv,(char *)"TX");
		if (priv->tx_level < BDX_MIN_TX_LEVEL)
		{
			DBG2("TX Q STOP level %d\n", priv->tx_level);
			if_setdrvflagbits(tn40_priv->ifp, IFF_DRV_OACTIVE, 0);
			priv->tx_state = TX_STATE_QUEUE_FULL;
			break;
		}
		err = 0;

    } while(0);

//    mtx_unlock(&priv->tx_lock);

    RET(err);

} // bdx_tx_transmit()

//-------------------------------------------------------------------------------------------------

/* bdx_tx_cleanup - Clean the TXF fifo, run in the context of IRQ.
 *
 * @priv - bdx adapter
 *
 * This function scans the TXF fifo for descriptors, frees DMA mappings and
 * reports to the OS that those packets were sent.
 */
void bdx_tx_cleanup(struct bdx_priv *priv)
{
    struct txf_desc *txfd;
    tn40_priv_t		*tn40_priv 	= CONTAINING_RECORD(priv, tn40_priv_t, bdx_priv);
    struct txf_fifo *f 			= &priv->txf_fifo0;
    struct txdb 	*db 		= &priv->txdb;
    int 			tx_level	= 0;

    ENTER;
	bus_dmamap_sync(f->m.dma.dma_tag, f->m.dma.tn40_dmamap.dma_map, BUS_DMASYNC_POSTREAD);
    f->m.wptr = READ_REG(priv, f->m.reg_WPTR) & TXF_WPTR_MASK;
    BDX_ASSERT(f->m.rptr >= f->m.memsz);    /* Started with valid rptr */
//    mtx_lock(&priv->tx_lock);
    while (f->m.wptr != f->m.rptr)
    {
        txfd   = (struct txf_desc *)(f->m.va + f->m.rptr);
        if (txfd->va_hi != 0)
        {
        	//freeCmem(size_t(txfd->va_hi));
        }
        f->m.rptr += BDX_TXF_DESC_SZ;
        f->m.rptr &= f->m.size_mask;
        BDX_ASSERT(db->rptr->len == 0);
        do
        {
            BDX_ASSERT(db->rptr->len == 0);
            bdx_tx_db_inc_rptr(db);
        } while (db->rptr->len > 0);
        tx_level -= db->rptr->len; // '-' Because the len is negative
        DBG("m %p %d\n", db->rptr->mBuf, db->rptr->mBuf ? db->rptr->mBuf->m_pkthdr.len : 0);
        COUNT_INC(priv->counters, tx_free_pkts);
        COUNT_ADD(priv->counters, tx_free_bytes, db->rptr->mBuf ? db->rptr->mBuf->m_pkthdr.len : 0);
        m_freem(db->rptr->mBuf);
        bdx_tx_db_inc_rptr(db);
        DBG("<-- bdx_tx_cleanup va_hi %u va_lo 0x%x tx_level %d + %d %s\n",
        	txfd->va_hi, txfd->va_lo, priv->tx_level, tx_level,txfd->status & 0x80000000 ? "TXE!" :"");
    }
    /* Let the HW know which TXF descriptors were cleaned */
    BDX_ASSERT((f->m.wptr & TXF_WPTR_WR_PTR) >= f->m.memsz);
    WRITE_REG(priv, f->m.reg_RPTR, f->m.rptr & TXF_WPTR_WR_PTR);
    /*
     * We reclaimed resources, so in case the Q is stopped by xmit callback,
     */
    priv->tx_level += tx_level;
    BDX_ASSERT(priv->tx_level <= 0 || priv->tx_level > BDX_MAX_TX_LEVEL);
    DBG("<-- bdx_tx_cleanup() TX %s triggered rptr %u  wptr %u tx_level %d tx_noupd %d \n",
    	priv->tx_noupd ? "" : "*NOT*", READ_REG(priv, priv->txd_fifo0.m.reg_RPTR) & TXF_WPTR_WR_PTR, priv->txd_fifo0.m.wptr, priv->tx_level, priv->tx_noupd);
    if (priv->tx_noupd)
    {
        priv->tx_noupd = 0;
        WRITE_REG(priv, priv->txd_fifo0.m.reg_WPTR, priv->txd_fifo0.m.wptr & TXF_WPTR_WR_PTR);
   		//dbg_printTXQ(priv);
    }
	bus_dmamap_sync(f->m.dma.dma_tag, f->m.dma.tn40_dmamap.dma_map, BUS_DMASYNC_PREWRITE);
    if (priv->tx_state == TX_STATE_QUEUE_FULL)
    {
    	if (priv->tx_level >= BDX_MIN_TX_LEVEL)
    	{
    		priv->tx_state = TX_STATE_OK;
    		DBG("TX Q wake level %d\n", priv->tx_level);
    		if_setdrvflagbits(tn40_priv->ifp, 0, IFF_DRV_OACTIVE);
    	}
    	else
    	{
    		DBG("bdx_tx_cleanup() TX Q stall at level %d\n", priv->tx_level);
    	}
    }
//    mtx_unlock(&priv->tx_lock);

    EXIT;

} // bdx_tx_cleanup()

//-------------------------------------------------------------------------------------------------

