// SPDX-License-Identifier: GPL-2.0+
/*
 * Ethernet driver for GENET controller found on RPI4.
 *
 */

#include <asm/io.h>
#include <clk.h>
#include <cpu_func.h>
#include <dm.h>
#include <fdt_support.h>
#include <linux/err.h>
#include <malloc.h>
#include <miiphy.h>
#include <net.h>
#include <dm/of_access.h>
#include <dm/ofnode.h>
#include <linux/libfdt.h>
#include <linux/iopoll.h>
#include <asm/dma-mapping.h>

/* Register definitions derived from Linux source */
#define SYS_REV_CTRL			 0x00

#define SYS_PORT_CTRL			 0x04
#define PORT_MODE_EXT_GPHY		 3

#define GENET_SYS_OFF			 0x0000
#define SYS_RBUF_FLUSH_CTRL		 (GENET_SYS_OFF  + 0x08)
#define SYS_TBUF_FLUSH_CTRL		 (GENET_SYS_OFF  + 0x0C)

#define GENET_EXT_OFF			 0x0080
#define EXT_RGMII_OOB_CTRL               (GENET_EXT_OFF + 0x0C)
#define RGMII_MODE_EN_V123               BIT(0)
#define RGMII_LINK                       BIT(4)
#define OOB_DISABLE                      BIT(5)
#define RGMII_MODE_EN                    BIT(6)
#define ID_MODE_DIS                      BIT(16)

#define GENET_RBUF_OFF			 0x0300
#define RBUF_FLUSH_CTRL_V1		 (GENET_RBUF_OFF + 0x04)
#define RBUF_TBUF_SIZE_CTRL              (GENET_RBUF_OFF + 0xb4)
#define RBUF_CTRL                        (GENET_RBUF_OFF + 0x00)
#define RBUF_64B_EN                      BIT(0)
#define RBUF_ALIGN_2B                    BIT(1)
#define RBUF_BAD_DIS                     BIT(2)

#define GENET_UMAC_OFF			 0x0800
#define UMAC_MIB_CTRL			 (GENET_UMAC_OFF + 0x580)
#define UMAC_MAX_FRAME_LEN		 (GENET_UMAC_OFF + 0x014)
#define UMAC_MAC0			 (GENET_UMAC_OFF + 0x00C)
#define UMAC_MAC1			 (GENET_UMAC_OFF + 0x010)
#define UMAC_CMD			 (GENET_UMAC_OFF + 0x008)
#define MDIO_CMD			 (GENET_UMAC_OFF + 0x614)
#define UMAC_TX_FLUSH			 (GENET_UMAC_OFF + 0x334)
#define MDIO_START_BUSY                  BIT(29)
#define MDIO_READ_FAIL                   BIT(28)
#define MDIO_RD                          (2 << 26)
#define MDIO_WR                          BIT(26)
#define MDIO_PMD_SHIFT                   21
#define MDIO_PMD_MASK                    0x1F
#define MDIO_REG_SHIFT                   16
#define MDIO_REG_MASK                    0x1F

#define CMD_TX_EN			 BIT(0)
#define CMD_RX_EN			 BIT(1)
#define UMAC_SPEED_10			 0
#define UMAC_SPEED_100			 1
#define UMAC_SPEED_1000			 2
#define UMAC_SPEED_2500			 3
#define CMD_SPEED_SHIFT			 2
#define CMD_SPEED_MASK			 3
#define CMD_SW_RESET			 BIT(13)
#define CMD_LCL_LOOP_EN			 BIT(15)
#define CMD_TX_EN			 BIT(0)
#define CMD_RX_EN			 BIT(1)

#define MIB_RESET_RX			 BIT(0)
#define MIB_RESET_RUNT			 BIT(1)
#define MIB_RESET_TX			 BIT(2)

/* total number of Buffer Descriptors, same for Rx/Tx */
#define TOTAL_DESC			 256

#define DEFAULT_Q                        0x10

/* Body(1500) + EH_SIZE(14) + VLANTAG(4) + BRCMTAG(6) + FCS(4) = 1528.
 * 1536 is multiple of 256 bytes
 */
#define ENET_BRCM_TAG_LEN		 6
#define ENET_PAD			 8
#define ENET_MAX_MTU_SIZE		 (ETH_DATA_LEN + ETH_HLEN + VLAN_HLEN + \
					  ENET_BRCM_TAG_LEN + ETH_FCS_LEN + ENET_PAD)

/* Tx/Rx Dma Descriptor common bits*/
#define DMA_EN                           BIT(0)
#define DMA_RING_BUF_EN_SHIFT            0x01
#define DMA_RING_BUF_EN_MASK             0xFFFF
#define DMA_BUFLENGTH_MASK		 0x0fff
#define DMA_BUFLENGTH_SHIFT		 16
#define DMA_RING_SIZE_SHIFT		 16
#define DMA_OWN				 0x8000
#define DMA_EOP				 0x4000
#define DMA_SOP				 0x2000
#define DMA_WRAP			 0x1000
#define DMA_MAX_BURST_LENGTH             0x8
/* Tx specific Dma descriptor bits */
#define DMA_TX_UNDERRUN			 0x0200
#define DMA_TX_APPEND_CRC		 0x0040
#define DMA_TX_OW_CRC			 0x0020
#define DMA_TX_DO_CSUM			 0x0010
#define DMA_TX_QTAG_SHIFT		 7
#define GENET_TDMA_REG_OFF		 (0x4000 + \
					  TOTAL_DESC * DMA_DESC_SIZE)
#define GENET_RDMA_REG_OFF		 (0x2000 + \
					  TOTAL_DESC * DMA_DESC_SIZE)

/* DMA rings size */
#define DMA_RING_SIZE			 (0x40)
#define DMA_RINGS_SIZE			 (DMA_RING_SIZE * (DEFAULT_Q + 1))

/* DMA Descriptor */
#define DMA_DESC_LENGTH_STATUS		 0x00
#define DMA_DESC_ADDRESS_LO		 0x04
#define DMA_DESC_ADDRESS_HI		 0x08
#define DMA_DESC_SIZE                    0xc

#define DMA_FC_THRESH_HI		 (TOTAL_DESC >> 4)
#define DMA_FC_THRESH_LO		 5
#define DMA_XOFF_THRESHOLD_SHIFT	 16

#define TDMA_RING_REG_BASE(QUEUE_NUMBER) (GENET_TDMA_REG_OFF \
			   + (DMA_RING_SIZE * (QUEUE_NUMBER)))
#define TDMA_READ_PTR                    0x00
#define TDMA_CONS_INDEX                  0x08
#define TDMA_PROD_INDEX                  0x0C
#define DMA_RING_BUF_SIZE                0x10
#define DMA_START_ADDR                   0x14
#define DMA_END_ADDR                     0x1C
#define DMA_MBUF_DONE_THRESH             0x24
#define TDMA_FLOW_PERIOD                 0x28
#define TDMA_WRITE_PTR                   0x2C

#define RDMA_RING_REG_BASE(QUEUE_NUMBER) (GENET_RDMA_REG_OFF \
			   + (DMA_RING_SIZE * (QUEUE_NUMBER)))
#define RDMA_WRITE_PTR                   TDMA_READ_PTR
#define RDMA_READ_PTR                    TDMA_WRITE_PTR
#define RDMA_PROD_INDEX                  TDMA_CONS_INDEX
#define RDMA_CONS_INDEX                  TDMA_PROD_INDEX
#define RDMA_XON_XOFF_THRESH             TDMA_FLOW_PERIOD

#define TDMA_REG_BASE			 (GENET_TDMA_REG_OFF + DMA_RINGS_SIZE)
#define RDMA_REG_BASE			 (GENET_RDMA_REG_OFF + DMA_RINGS_SIZE)
#define DMA_RING_CFG			 0x0
#define DMA_CTRL			 0x04
#define DMA_SCB_BURST_SIZE		 0x0C

#define RX_BUF_LENGTH			 2048
#define RX_TOTAL_BUFSIZE		 (RX_BUF_LENGTH * TOTAL_DESC)
#define RX_BUF_OFFSET			 2

DECLARE_GLOBAL_DATA_PTR;

struct bcmgenet_eth_priv {
	void *mac_reg;
	char rxbuffer[RX_TOTAL_BUFSIZE] __aligned(ARCH_DMA_MINALIGN);
	void *tx_desc_base;
	void *rx_desc_base;
	int tx_index;
	int rx_index;
	int c_index;
	int phyaddr;
	u32 interface;
	u32 speed;
	struct phy_device *phydev;
	struct mii_dev *bus;
};

static void bcmgenet_umac_reset(struct bcmgenet_eth_priv *priv)
{
	u32 reg;

	reg = readl(priv->mac_reg + SYS_RBUF_FLUSH_CTRL);
	reg |= BIT(1);
	writel(reg, (priv->mac_reg + SYS_RBUF_FLUSH_CTRL));
	udelay(10);

	reg &= ~BIT(1);
	writel(reg, (priv->mac_reg + SYS_RBUF_FLUSH_CTRL));
	udelay(10);

	writel(0, (priv->mac_reg + SYS_RBUF_FLUSH_CTRL));
	udelay(10);

	writel(0, priv->mac_reg + UMAC_CMD);

	writel(CMD_SW_RESET | CMD_LCL_LOOP_EN, priv->mac_reg + UMAC_CMD);
	udelay(2);
	writel(0, priv->mac_reg + UMAC_CMD);

	/* clear tx/rx counter */
	writel(MIB_RESET_RX | MIB_RESET_TX | MIB_RESET_RUNT,
	       priv->mac_reg + UMAC_MIB_CTRL);
	writel(0, priv->mac_reg + UMAC_MIB_CTRL);

	writel(ENET_MAX_MTU_SIZE, priv->mac_reg + UMAC_MAX_FRAME_LEN);

	/* init rx registers, enable ip header optimization */
	reg = readl(priv->mac_reg + RBUF_CTRL);
	reg |= RBUF_ALIGN_2B;
	writel(reg, (priv->mac_reg + RBUF_CTRL));

	writel(1, (priv->mac_reg + RBUF_TBUF_SIZE_CTRL));
}

static int bcmgenet_gmac_write_hwaddr(struct udevice *dev)
{
	struct bcmgenet_eth_priv *priv = dev_get_priv(dev);
	struct eth_pdata *pdata = dev_get_platdata(dev);
	uchar *addr = pdata->enetaddr;
	u32 reg;

	reg = addr[0] << 24 | addr[1] << 16 | addr[2] << 8 | addr[3];
	writel_relaxed(reg, priv->mac_reg + UMAC_MAC0);

	reg = addr[4] << 8 | addr[5];
	writel_relaxed(reg, priv->mac_reg + UMAC_MAC1);

	return 0;
}

static u32 bcmgenet_dma_disable(struct bcmgenet_eth_priv *priv)
{
	u32 reg;
	u32 dma_ctrl;

	dma_ctrl = 1 << (DEFAULT_Q + DMA_RING_BUF_EN_SHIFT) | DMA_EN;
	reg = readl(priv->mac_reg + TDMA_REG_BASE + DMA_CTRL);
	reg &= ~dma_ctrl;
	writel(reg, (priv->mac_reg + TDMA_REG_BASE + DMA_CTRL));

	reg = readl(priv->mac_reg + RDMA_REG_BASE + DMA_CTRL);
	reg &= ~dma_ctrl;
	writel(reg, (priv->mac_reg + RDMA_REG_BASE + DMA_CTRL));

	writel(1, priv->mac_reg + UMAC_TX_FLUSH);
	udelay(10);
	writel(0, priv->mac_reg + UMAC_TX_FLUSH);

	return dma_ctrl;
}

static void bcmgenet_enable_dma(struct bcmgenet_eth_priv *priv, u32 dma_ctrl)
{
	u32 reg;

	dma_ctrl |= (1 << (DEFAULT_Q + DMA_RING_BUF_EN_SHIFT));
	dma_ctrl |= DMA_EN;

	writel(dma_ctrl, priv->mac_reg + TDMA_REG_BASE + DMA_CTRL);

	reg = readl(priv->mac_reg + RDMA_REG_BASE + DMA_CTRL);
	reg |= dma_ctrl;
	writel(reg, priv->mac_reg + RDMA_REG_BASE + DMA_CTRL);
}

static int bcmgenet_gmac_eth_send(struct udevice *dev, void *packet, int length)
{
	struct bcmgenet_eth_priv *priv = dev_get_priv(dev);
	void *desc_base = priv->tx_desc_base + priv->tx_index * DMA_DESC_SIZE;
	u32 len_stat = length << DMA_BUFLENGTH_SHIFT;
	u32 prod_index, cons;
	u32 tries = 100;

	prod_index = readl(priv->mac_reg + TDMA_RING_REG_BASE(DEFAULT_Q) + TDMA_PROD_INDEX);

	flush_dcache_range((ulong)packet, (ulong)packet + length);

	len_stat |= 0x3F << DMA_TX_QTAG_SHIFT;
	len_stat |= DMA_TX_APPEND_CRC | DMA_SOP | DMA_EOP;

	/* Set-up packet for transmission */
	writel(lower_32_bits((ulong)packet), (desc_base + DMA_DESC_ADDRESS_LO));
	writel(upper_32_bits((ulong)packet), (desc_base + DMA_DESC_ADDRESS_HI));
	writel(len_stat, (desc_base + DMA_DESC_LENGTH_STATUS));

	/* Increment index and wrap-up */
	priv->tx_index++;
	if (!(priv->tx_index % TOTAL_DESC)) {
		priv->tx_index = 0;
	}

	prod_index++;

	/* Start Transmisson */
	writel(prod_index, (priv->mac_reg + TDMA_RING_REG_BASE(DEFAULT_Q) + TDMA_PROD_INDEX));

	do {
		cons = readl(priv->mac_reg + TDMA_RING_REG_BASE(DEFAULT_Q) + TDMA_CONS_INDEX);
	} while ((cons & 0xffff) < prod_index && --tries);
	if (!tries)
		return -ETIMEDOUT;

	return 0;
}

static int bcmgenet_gmac_eth_recv(struct udevice *dev,
				  int flags, uchar **packetp)
{
	struct bcmgenet_eth_priv *priv = dev_get_priv(dev);
	u32 len;
	u32 addr;
	u32 length;
	void *desc_base = priv->rx_desc_base + priv->rx_index * DMA_DESC_SIZE;
	u32 prod_index = readl(priv->mac_reg + RDMA_PROD_INDEX);

	if (prod_index > priv->c_index) {
		len  = readl(desc_base + DMA_DESC_LENGTH_STATUS);
		addr = readl(desc_base + DMA_DESC_ADDRESS_LO);

		length = (len >> DMA_BUFLENGTH_SHIFT) & DMA_BUFLENGTH_MASK;

		invalidate_dcache_range((uintptr_t)addr,
					(addr + RX_BUF_LENGTH));

		/*
		 * two dummy bytes are added for IP alignment, this can be
		 * avoided by not programming RBUF_ALIGN_2B bit in RBUF_CTRL
		 */
		*packetp = (uchar *)(ulong)addr + RX_BUF_OFFSET;

		return (length - RX_BUF_OFFSET);
	}

	return -EAGAIN;
}

static int bcmgenet_gmac_free_pkt(struct udevice *dev, uchar *packet,
				  int length)
{
	struct bcmgenet_eth_priv *priv = dev_get_priv(dev);

	priv->c_index = (priv->c_index + 1) & 0xFFFF;
	writel(priv->c_index,
	       priv->mac_reg + RDMA_RING_REG_BASE(DEFAULT_Q) + RDMA_CONS_INDEX);

	priv->rx_index++;
	if (!(priv->rx_index % TOTAL_DESC)) {
		priv->rx_index = 0;
	}

	return 0;
}

static void rx_descs_init(struct bcmgenet_eth_priv *priv)
{
	char *rxbuffs = &priv->rxbuffer[0];
	u32 len_stat, i;
	void *desc_base = priv->rx_desc_base;

	priv->c_index = 0;

	len_stat = ((RX_BUF_LENGTH << DMA_BUFLENGTH_SHIFT) | DMA_OWN);

	for (i = 0; i < TOTAL_DESC; i++) {
		writel(lower_32_bits((uintptr_t)&rxbuffs[i * RX_BUF_LENGTH]),
		       (((desc_base + (i * DMA_DESC_SIZE)) + DMA_DESC_ADDRESS_LO)));
		writel(upper_32_bits((uintptr_t)&rxbuffs[i * RX_BUF_LENGTH]),
		       (((desc_base + (i * DMA_DESC_SIZE)) + DMA_DESC_ADDRESS_HI)));
		writel(len_stat,
		       ((desc_base + (i * DMA_DESC_SIZE) + DMA_DESC_LENGTH_STATUS)));
	}
}

static void rx_ring_init(struct bcmgenet_eth_priv *priv)
{
	writel(DMA_MAX_BURST_LENGTH,
	       (priv->mac_reg + RDMA_REG_BASE + DMA_SCB_BURST_SIZE));
	writel(0x0,
	       (priv->mac_reg + RDMA_RING_REG_BASE(DEFAULT_Q) + DMA_START_ADDR));
	writel(0x0,
	       (priv->mac_reg + RDMA_RING_REG_BASE(DEFAULT_Q) + RDMA_READ_PTR));
	writel(0x0,
	       (priv->mac_reg + RDMA_RING_REG_BASE(DEFAULT_Q) + RDMA_WRITE_PTR));
	writel(TOTAL_DESC * DMA_DESC_SIZE / 4 - 1,
	       (priv->mac_reg + RDMA_RING_REG_BASE(DEFAULT_Q) + DMA_END_ADDR));
	writel(0x0,
	       (priv->mac_reg + RDMA_RING_REG_BASE(DEFAULT_Q) + RDMA_PROD_INDEX));
	writel(0x0,
	       (priv->mac_reg + RDMA_RING_REG_BASE(DEFAULT_Q) + RDMA_CONS_INDEX));
	writel(((TOTAL_DESC << DMA_RING_SIZE_SHIFT) | RX_BUF_LENGTH),
	       (priv->mac_reg + RDMA_RING_REG_BASE(DEFAULT_Q) + DMA_RING_BUF_SIZE));
	writel(((DMA_FC_THRESH_LO << DMA_XOFF_THRESHOLD_SHIFT) | DMA_FC_THRESH_HI),
	       (priv->mac_reg + RDMA_RING_REG_BASE(DEFAULT_Q) + RDMA_XON_XOFF_THRESH));
	writel((1 << DEFAULT_Q),
	       (priv->mac_reg + RDMA_REG_BASE + DMA_RING_CFG));
}

static void tx_ring_init(struct bcmgenet_eth_priv *priv)
{
	writel(DMA_MAX_BURST_LENGTH,
	       (priv->mac_reg + TDMA_REG_BASE + DMA_SCB_BURST_SIZE));
	writel(0x0,
	       (priv->mac_reg + TDMA_RING_REG_BASE(DEFAULT_Q) + DMA_START_ADDR));
	writel(0x0,
	       (priv->mac_reg + TDMA_RING_REG_BASE(DEFAULT_Q) + TDMA_READ_PTR));
	writel(0x0,
	       (priv->mac_reg + TDMA_RING_REG_BASE(DEFAULT_Q) + TDMA_WRITE_PTR));
	writel(TOTAL_DESC * DMA_DESC_SIZE / 4 - 1,
	       (priv->mac_reg + TDMA_RING_REG_BASE(DEFAULT_Q) + DMA_END_ADDR));
	writel(0x0,
	       (priv->mac_reg + TDMA_RING_REG_BASE(DEFAULT_Q) + TDMA_PROD_INDEX));
	writel(0x0,
	       (priv->mac_reg + TDMA_RING_REG_BASE(DEFAULT_Q) + TDMA_CONS_INDEX));
	writel(0x1,
	       (priv->mac_reg + TDMA_RING_REG_BASE(DEFAULT_Q) + DMA_MBUF_DONE_THRESH));
	writel(0x0,
	       (priv->mac_reg + TDMA_RING_REG_BASE(DEFAULT_Q) + TDMA_FLOW_PERIOD));
	writel(((TOTAL_DESC << DMA_RING_SIZE_SHIFT) | RX_BUF_LENGTH),
	       (priv->mac_reg + TDMA_RING_REG_BASE(DEFAULT_Q) + DMA_RING_BUF_SIZE));
	writel((1 << DEFAULT_Q),
	       (priv->mac_reg + TDMA_REG_BASE + DMA_RING_CFG));
}

static void bcmgenet_adjust_link(struct bcmgenet_eth_priv *priv)
{
	struct phy_device *phy_dev = priv->phydev;
	u32 reg = 0, reg_rgmii;

	switch (phy_dev->speed) {
	case SPEED_1000:
		reg = UMAC_SPEED_1000;
		break;
	case SPEED_100:
		reg = UMAC_SPEED_100;
		break;
	case SPEED_10:
		reg = UMAC_SPEED_10;
		break;
	}

	reg <<= CMD_SPEED_SHIFT;

	reg_rgmii = readl(priv->mac_reg + EXT_RGMII_OOB_CTRL);
	reg_rgmii &= ~OOB_DISABLE;
	reg_rgmii |= (RGMII_LINK | RGMII_MODE_EN | ID_MODE_DIS);
	writel(reg_rgmii, priv->mac_reg + EXT_RGMII_OOB_CTRL);

	writel(reg, (priv->mac_reg + UMAC_CMD));
}

static int bcmgenet_gmac_eth_start(struct udevice *dev)
{
	struct bcmgenet_eth_priv *priv = dev_get_priv(dev);
	u32 dma_ctrl, reg;

	priv->tx_desc_base = priv->mac_reg + 0x4000;
	priv->rx_desc_base = priv->mac_reg + 0x2000;
	priv->tx_index = 0x0;
	priv->rx_index = 0x0;

	bcmgenet_umac_reset(priv);

	bcmgenet_gmac_write_hwaddr(dev);

	/* Disable RX/TX DMA and flush TX queues */
	dma_ctrl = bcmgenet_dma_disable(priv);

	rx_ring_init(priv);
	rx_descs_init(priv);

	tx_ring_init(priv);

	/* Enable RX/TX DMA */
	bcmgenet_enable_dma(priv, dma_ctrl);

	/* PHY Start Up, read PHY properties over the wire
	 * from generic PHY set-up
	 */
	phy_startup(priv->phydev);

	/* Update MAC registers based on PHY property */
	bcmgenet_adjust_link(priv);

	/* Enable Rx/Tx */
	reg = readl(priv->mac_reg + UMAC_CMD);
	reg |= (CMD_TX_EN | CMD_RX_EN);
	writel(reg, (priv->mac_reg + UMAC_CMD));

	return 0;
}

static int bcmgenet_phy_init(struct bcmgenet_eth_priv *priv, void *dev)
{
	struct phy_device *phydev;
	int ret;

	phydev = phy_connect(priv->bus, priv->phyaddr, dev, priv->interface);
	if (!phydev)
		return -ENODEV;

	phydev->supported &= PHY_GBIT_FEATURES;
	if (priv->speed) {
		ret = phy_set_supported(priv->phydev, priv->speed);
		if (ret)
			return ret;
	}
	phydev->advertising = phydev->supported;

	phy_connect_dev(phydev, dev);

	priv->phydev = phydev;
	phy_config(priv->phydev);

	return 0;
}

static inline void bcmgenet_mdio_start(struct bcmgenet_eth_priv *priv)
{
	u32 reg;

	reg = readl_relaxed(priv->mac_reg + MDIO_CMD);
	reg |= MDIO_START_BUSY;
	writel_relaxed(reg, priv->mac_reg + MDIO_CMD);
}

static int bcmgenet_mdio_write(struct mii_dev *bus, int addr, int devad,
			       int reg, u16 value)
{
	struct udevice *dev = bus->priv;
	struct bcmgenet_eth_priv *priv = dev_get_priv(dev);
	u32 status, val;
	ulong start_time;
	ulong timeout_us = 20000;

	start_time = timer_get_us();

	/* Prepare the read operation */
	val = MDIO_WR | (addr << MDIO_PMD_SHIFT) |
		(reg << MDIO_REG_SHIFT) | (0xffff & value);
	writel_relaxed(val,  priv->mac_reg + MDIO_CMD);

	/* Start MDIO transaction */
	bcmgenet_mdio_start(priv);

	for (;;) {
		status = readl_relaxed(priv->mac_reg + MDIO_CMD);
		if (!(status & MDIO_START_BUSY))
			break;
		if (timeout_us > 0 && (timer_get_us() - start_time)
				>= timeout_us)
			return -ETIMEDOUT;
	}

	return 0;
}

static int bcmgenet_mdio_read(struct mii_dev *bus, int addr, int devad, int reg)
{
	struct udevice *dev = bus->priv;
	struct bcmgenet_eth_priv *priv = dev_get_priv(dev);
	u32 status, val;
	ulong start_time;
	ulong timeout_us =  20000;

	start_time = timer_get_us();

	/* Prepare the read operation */
	val = MDIO_RD | (addr << MDIO_PMD_SHIFT) | (reg << MDIO_REG_SHIFT);
	writel_relaxed(val, priv->mac_reg + MDIO_CMD);

	/* Start MDIO transaction */
	bcmgenet_mdio_start(priv);

	for (;;) {
		status = readl_relaxed(priv->mac_reg + MDIO_CMD);
		if (!(status & MDIO_START_BUSY))
			break;
		if (timeout_us > 0 && (timer_get_us() - start_time) >= timeout_us)
			return -ETIMEDOUT;
	}

	val = readl_relaxed(priv->mac_reg + MDIO_CMD);

	return val & 0xffff;
}

static int bcmgenet_mdio_init(const char *name, struct udevice *priv)
{
	struct mii_dev *bus = mdio_alloc();

	if (!bus) {
		debug("Failed to allocate MDIO bus\n");
		return -ENOMEM;
	}

	bus->read = bcmgenet_mdio_read;
	bus->write = bcmgenet_mdio_write;
	snprintf(bus->name, sizeof(bus->name), name);
	bus->priv = (void *)priv;

	return  mdio_register(bus);
}

static int bcmgenet_interface_set(struct bcmgenet_eth_priv *priv)
{
	phy_interface_t phy_mode = priv->interface;

	switch (phy_mode) {
	case PHY_INTERFACE_MODE_RGMII:
		writel(PORT_MODE_EXT_GPHY, priv->mac_reg + SYS_PORT_CTRL);
		break;
	default:
		printf("unknown phy mode: %d\n", priv->interface);
		return -EINVAL;
	}

	return 0;
}

static int bcmgenet_eth_probe(struct udevice *dev)
{
	struct eth_pdata *pdata = dev_get_platdata(dev);
	struct bcmgenet_eth_priv *priv = dev_get_priv(dev);
	int offset = dev_of_offset(dev);
	const char *name;
	int reg;
	u8 major;

	priv->mac_reg = (void *)pdata->iobase;
	priv->interface = pdata->phy_interface;
	priv->speed = pdata->max_speed;

	/* Read GENET HW version */
	reg = readl_relaxed(priv->mac_reg + SYS_REV_CTRL);
	major = (reg >> 24 & 0x0f);
	if (major == 6)
		major = 5;
	else if (major == 5)
		major = 4;
	else if (major == 0)
		major = 1;

	debug("GENET version is %1d.%1d EPHY: 0x%04x",
	      major, (reg >> 16) & 0x0f, reg & 0xffff);

	bcmgenet_interface_set(priv);

	offset = fdt_first_subnode(gd->fdt_blob, offset);
	name = fdt_get_name(gd->fdt_blob, offset, NULL);

	bcmgenet_mdio_init(name, dev);
	priv->bus = miiphy_get_dev_by_name(name);

	return bcmgenet_phy_init(priv, dev);
}

static void bcmgenet_gmac_eth_stop(struct udevice *dev)
{
	struct bcmgenet_eth_priv *priv = dev_get_priv(dev);
	u32 reg, dma_ctrl;

	reg = readl(priv->mac_reg + UMAC_CMD);
	reg &= ~(CMD_TX_EN | CMD_RX_EN);
	writel(reg, (priv->mac_reg + UMAC_CMD));

	dma_ctrl = 1 << (DEFAULT_Q + DMA_RING_BUF_EN_SHIFT) | DMA_EN;
	reg = readl(priv->mac_reg + TDMA_REG_BASE + DMA_CTRL);
	reg &= ~dma_ctrl;
	writel(reg, (priv->mac_reg + TDMA_REG_BASE + DMA_CTRL));
}

static const struct eth_ops bcmgenet_gmac_eth_ops = {
	.start                  = bcmgenet_gmac_eth_start,
	.write_hwaddr           = bcmgenet_gmac_write_hwaddr,
	.send                   = bcmgenet_gmac_eth_send,
	.recv                   = bcmgenet_gmac_eth_recv,
	.free_pkt               = bcmgenet_gmac_free_pkt,
	.stop                   = bcmgenet_gmac_eth_stop,
};

static int bcmgenet_eth_ofdata_to_platdata(struct udevice *dev)
{
	struct eth_pdata *pdata = dev_get_platdata(dev);
	struct bcmgenet_eth_priv *priv = dev_get_priv(dev);
	const char *phy_mode;
	int node = dev_of_offset(dev);
	int offset = 0;

	pdata->iobase = (phys_addr_t)devfdt_get_addr(dev);

	/* Get phy mode from DT */
	pdata->phy_interface = -1;
	phy_mode = fdt_getprop(gd->fdt_blob, dev_of_offset(dev), "phy-mode",
			       NULL);

	if (phy_mode)
		pdata->phy_interface = phy_get_interface_by_name(phy_mode);
	if (pdata->phy_interface == -1) {
		debug("%s: Invalid PHY interface '%s'\n", __func__, phy_mode);
		return -EINVAL;
	}

	offset = fdtdec_lookup_phandle(gd->fdt_blob, node, "phy-handle");
	if (offset > 0) {
		priv->phyaddr = fdtdec_get_int(gd->fdt_blob, offset, "reg", 0);
		pdata->max_speed = fdtdec_get_int(gd->fdt_blob, offset, "max-speed", 0);
	}

	return 0;
}

static const struct udevice_id bcmgenet_eth_ids[] = {
	{.compatible = "brcm,genet-v5"},
	{.compatible = "brcm,bcm2711-genet-v5"},
	{}
};

U_BOOT_DRIVER(eth_bcmgenet) = {
	.name   = "eth_bcmgenet",
	.id     = UCLASS_ETH,
	.of_match = bcmgenet_eth_ids,
	.ofdata_to_platdata = bcmgenet_eth_ofdata_to_platdata,
	.probe  = bcmgenet_eth_probe,
	.ops    = &bcmgenet_gmac_eth_ops,
	.priv_auto_alloc_size = sizeof(struct bcmgenet_eth_priv),
	.platdata_auto_alloc_size = sizeof(struct eth_pdata),
	.flags = DM_FLAG_ALLOC_PRIV_DMA,
};
