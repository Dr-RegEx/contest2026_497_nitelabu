/****************************************************************************
 * arch/risc-v/src/esp32s31/esp32s31_emac.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Native NuttX Ethernet, Function CoreBoard RGMII / YT8531.
 * The MAC HAL copies packets to/from its own internal DMA allocations.
 * All MAC/PHY operations and stack buffers are serialized by net_lock().
 * The interrupt handler only acknowledges the interrupt and queues LPWORK.
 ****************************************************************************/

#include <nuttx/config.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <nuttx/clock.h>
#include <nuttx/irq.h>
#include <nuttx/wqueue.h>
#include <nuttx/net/net.h>
#include <nuttx/net/netdev.h>
#include <nuttx/net/ethernet.h>
#include <nuttx/net/ip.h>
#include <nuttx/net/ioctl.h>
#include <nuttx/net/mii.h>
#ifdef CONFIG_NET_PKT
#  include <nuttx/net/pkt.h>
#endif
#include <arch/board/board.h>
#include "esp_eth_mac_esp.h"
#include "esp_eth_phy.h"
#include "esp32s31_emac.h"

#define EMAC_BUFSIZE       1536
#define EMAC_RX_BUDGET     16
#define EMAC_SERVICE_TICKS MSEC2TICK(10)
#define EMAC_LINK_TICKS    MSEC2TICK(500)
#define EMAC_TX_TIMEOUT    SEC2TICK(3)
#define YT8531_PHY_ID      0x4f51e91bu
#define YT8531S_PHY_ID     0x4f51e91au

#if CONFIG_NET_ETH_PKTSIZE > EMAC_BUFSIZE
#  error "Ethernet packet exceeds the candidate DMA buffer"
#endif
#if BOARD_IS_KORVO
#  error "Onboard YT8531 profile is specific to Function CoreBoard"
#endif

struct s31_emac_s
{
  struct net_driver_s dev;
  esp_eth_mediator_t mediator;
  esp_eth_mac_t *mac;
  esp_eth_phy_t *phy;
  struct work_s rxwork;
  struct work_s pollwork;
  struct work_s servicework;
  bool registered;
  bool ifup;
  bool link;
  volatile bool irq_ready;
  eth_speed_t speed;
  eth_duplex_t duplex;
  uint32_t phyaddr;
  clock_t lastlink;
  clock_t txsince;
  unsigned int mdio_errors;
  uint16_t pending_len;
  uint8_t buffer[EMAC_BUFSIZE + CONFIG_NET_GUARDSIZE]
    __attribute__((aligned(4)));
  uint8_t pending[EMAC_BUFSIZE] __attribute__((aligned(4)));
};

static struct s31_emac_s g_emac;
static void emac_poll(struct s31_emac_s *priv);
static void emac_rxwork(void *arg);

static esp_err_t emac_read(esp_eth_mediator_t *eth, uint32_t addr,
                           uint32_t reg, uint32_t *value)
{
  struct s31_emac_s *priv = &g_emac;
  esp_err_t ret = priv->mac->read_phy_reg(priv->mac, addr, reg, value);
  if (ret != ESP_OK)
    {
      syslog(LOG_ERR, "eth0: MDIO read addr=%" PRIu32 " reg=%" PRIu32
             " error=0x%x\n", addr, reg, ret);
    }

  return ret;
}

static esp_err_t emac_write(esp_eth_mediator_t *eth, uint32_t addr,
                            uint32_t reg, uint32_t value)
{
  struct s31_emac_s *priv = &g_emac;
  return priv->mac->write_phy_reg(priv->mac, addr, reg, value);
}

static esp_err_t emac_state(esp_eth_mediator_t *eth,
                            esp_eth_state_t state, void *arg)
{
  struct s31_emac_s *priv = &g_emac;
  esp_err_t ret = ESP_OK;

  switch (state)
    {
      case ETH_STATE_SPEED:
        priv->speed = (eth_speed_t)(uintptr_t)arg;
        ret = priv->mac->set_speed(priv->mac, priv->speed);
        break;
      case ETH_STATE_DUPLEX:
        priv->duplex = (eth_duplex_t)(uintptr_t)arg;
        ret = priv->mac->set_duplex(priv->mac, priv->duplex);
        break;
      case ETH_STATE_PAUSE:
        /* Keep pause flow control disabled for this bounded candidate. */
        break;
      case ETH_STATE_LINK:
        if ((eth_link_t)(uintptr_t)arg == ETH_LINK_UP && priv->ifup)
          {
            ret = priv->mac->set_link(priv->mac, ETH_LINK_UP);
            if (ret == ESP_OK)
              {
                priv->link = true;
                netdev_carrier_on(&priv->dev);
                syslog(LOG_INFO, "eth0: link up %d Mbps %s duplex\n",
                       priv->speed == ETH_SPEED_1000M ? 1000 :
                       priv->speed == ETH_SPEED_100M ? 100 : 10,
                       priv->duplex == ETH_DUPLEX_FULL ? "full" : "half");
              }
          }
        else
          {
            ret = priv->mac->set_link(priv->mac, ETH_LINK_DOWN);
            priv->link = false;
            netdev_carrier_off(&priv->dev);
            if (priv->pending_len)
              {
                NETDEV_TXERRORS(&priv->dev);
                priv->pending_len = 0;
              }

            syslog(LOG_INFO, "eth0: link down\n");
          }
        break;
      default:
        break;
    }

  if (ret != ESP_OK)
    {
      syslog(LOG_ERR, "eth0: state=%d value=%" PRIuPTR " error=0x%x\n",
             (int)state, (uintptr_t)arg, ret);
    }

  return ret;
}

/* Preserve the packet if the HAL ring is full. Polling stops until this
 * private copy has been accepted. RX uses a different buffer, so it cannot
 * corrupt an outstanding transmission. An additional RX response may be
 * dropped under pressure; TCP/ARP can retry and the drop is counted.
 */

static void emac_send(struct s31_emac_s *priv)
{
  struct net_driver_s *dev = &priv->dev;
  esp_err_t ret;

  if (dev->d_len == 0)
    {
      return;
    }

  if (!priv->link || dev->d_len > EMAC_BUFSIZE || priv->pending_len)
    {
      NETDEV_TXERRORS(dev);
      dev->d_len = 0;
      return;
    }

  NETDEV_TXPACKETS(dev);
  ret = priv->mac->transmit(priv->mac, dev->d_buf, dev->d_len);
  if (ret == ESP_ERR_NO_MEM)
    {
      memcpy(priv->pending, dev->d_buf, dev->d_len);
      priv->pending_len = dev->d_len;
      priv->txsince = clock_systime_ticks();
    }
  else if (ret != ESP_OK)
    {
      NETDEV_TXERRORS(dev);
    }

  /* ESP_OK means copied to DMA, not confirmed on the wire. Do not fabricate
   * TXDONE statistics: this HAL does not expose a completion callback.
   */

  dev->d_len = 0;
}

static int emac_txpoll(struct net_driver_s *dev)
{
  struct s31_emac_s *priv = dev->d_private;
  emac_send(priv);
  return priv->pending_len ? -EBUSY : 0;
}

static void emac_poll(struct s31_emac_s *priv)
{
  esp_err_t ret;

  if (!priv->ifup || !priv->link)
    {
      return;
    }

  if (priv->pending_len)
    {
      ret = priv->mac->transmit(priv->mac, priv->pending, priv->pending_len);
      if (ret == ESP_ERR_NO_MEM)
        {
          if (clock_systime_ticks() - priv->txsince >= EMAC_TX_TIMEOUT)
            {
              NETDEV_TXTIMEOUTS(&priv->dev);
              syslog(LOG_ERR, "eth0: TX stalled; resetting DMA ring\n");
              /* Never reset descriptors while the engine is still running. */

              if (priv->mac->stop(priv->mac) == ESP_OK)
                {
                  ret = priv->mac->start(priv->mac);
                  if (ret == ESP_OK)
                    {
                      priv->txsince = clock_systime_ticks();
                    }
                }
            }

          return;
        }

      if (ret != ESP_OK)
        {
          NETDEV_TXERRORS(&priv->dev);
        }

      priv->pending_len = 0;
    }

  priv->dev.d_buf = priv->buffer;
  priv->dev.d_len = 0;
  devif_poll(&priv->dev, emac_txpoll);
}

static void emac_receive(struct s31_emac_s *priv)
{
  struct net_driver_s *dev = &priv->dev;
  unsigned int count;

  for (count = 0; priv->ifup && priv->link && count < EMAC_RX_BUDGET; count++)
    {
      struct eth_hdr_s *hdr = (struct eth_hdr_s *)priv->buffer;
      uint32_t length = EMAC_BUFSIZE;
      esp_err_t ret = priv->mac->receive(priv->mac, priv->buffer, &length);

      if (ret != ESP_OK)
        {
          NETDEV_RXERRORS(dev);
          continue;
        }

      if (length == 0)
        {
          break;
        }

      if (length < ETH_HDRLEN || length > CONFIG_NET_ETH_PKTSIZE)
        {
          NETDEV_RXDROPPED(dev);
          continue;
        }

      dev->d_buf = priv->buffer;
      dev->d_len = length;
      NETDEV_RXPACKETS(dev);
#ifdef CONFIG_NET_PKT
      pkt_input(dev);
#endif
#ifdef CONFIG_NET_IPv4
      if (hdr->type == HTONS(ETHTYPE_IP))
        {
          NETDEV_RXIPV4(dev);
          if (ipv4_input(dev) < 0)
            {
              NETDEV_RXDROPPED(dev);
              dev->d_len = 0;
            }
        }
      else
#endif
#ifdef CONFIG_NET_IPv6
      if (hdr->type == HTONS(ETHTYPE_IP6))
        {
          NETDEV_RXIPV6(dev);
          if (ipv6_input(dev) < 0)
            {
              NETDEV_RXDROPPED(dev);
              dev->d_len = 0;
            }
        }
      else
#endif
      if (hdr->type == HTONS(ETHTYPE_ARP))
        {
          NETDEV_RXARP(dev);
          arp_input(dev);
          if (dev->d_len > 0 &&
              memcmp(hdr->src, dev->d_mac.ether.ether_addr_octet, 6) != 0)
            {
              NETDEV_RXDROPPED(dev);
              dev->d_len = 0;
            }
        }
      else
        {
          NETDEV_RXDROPPED(dev);
          dev->d_len = 0;
        }

      emac_send(priv);
    }

  if (count == EMAC_RX_BUDGET)
    {
      esp32s31_emac_rxnotify();
    }
}

static void emac_rxwork(void *arg)
{
  struct s31_emac_s *priv = arg;
  net_lock();
  emac_receive(priv);
  emac_poll(priv);
  net_unlock();
}

void esp32s31_emac_rxnotify(void)
{
  struct s31_emac_s *priv = &g_emac;

  if (priv->irq_ready && work_available(&priv->rxwork))
    {
      work_queue(LPWORK, &priv->rxwork, emac_rxwork, priv, 0);
    }
}

static void emac_pollwork(void *arg)
{
  net_lock();
  emac_poll(arg);
  net_unlock();
}

static int emac_txavail(struct net_driver_s *dev)
{
  struct s31_emac_s *priv = dev->d_private;
  if (work_available(&priv->pollwork))
    {
      work_queue(LPWORK, &priv->pollwork, emac_pollwork, priv, 0);
    }

  return OK;
}

/* This service also prevents missed/coalesced RX work from stranding packets
 * and polls TX ownership because this HAL enables RX interrupts only.
 */

static void emac_service(void *arg)
{
  struct s31_emac_s *priv = arg;
  clock_t now;

  net_lock();
  now = clock_systime_ticks();
  if (priv->ifup && now - priv->lastlink >= EMAC_LINK_TICKS)
    {
      uint32_t bmsr;
      priv->lastlink = now;
      /* First read clears the BMSR latch-low bit. get_link reads it again. */

      esp_err_t mdio_ret = emac_read(&priv->mediator, priv->phyaddr, 1, &bmsr);
      esp_err_t link_ret = mdio_ret == ESP_OK ?
                          priv->phy->get_link(priv->phy) : mdio_ret;
      if (link_ret != ESP_OK)
        {
          if (++priv->mdio_errors <= 3)
            {
              syslog(LOG_ERR, "eth0: link service read=0x%x update=0x%x BMSR=%" PRIx32 "\n",
                     mdio_ret, link_ret, mdio_ret == ESP_OK ? bmsr : 0);
            }

          if (priv->mdio_errors == 3)
            {
              syslog(LOG_ERR, "eth0: repeated link-service failure\n");
              emac_state(&priv->mediator, ETH_STATE_LINK,
                         (void *)(uintptr_t)ETH_LINK_DOWN);
            }
        }
      else
        {
          if (priv->mdio_errors >= 3)
            {
              bool enabled;
              /* This also clears the PHY driver's cached link state.
               * autonego_timeout_ms is zero: do not sleep waiting for link.
               */

              priv->phy->autonego_ctrl(priv->phy, ESP_ETH_PHY_AUTONEGO_RESTART,
                                       &enabled);
            }

          priv->mdio_errors = 0;
        }
    }

  emac_receive(priv);
  emac_poll(priv);
  work_queue(LPWORK, &priv->servicework, emac_service, priv,
             EMAC_SERVICE_TICKS);
  net_unlock();
}

static esp_err_t emac_yt8531(struct s31_emac_s *priv)
{
  esp_eth_mediator_t *eth = &priv->mediator;
  uint32_t id1;
  uint32_t id2;
  uint32_t value;
  bool enabled;

#define PHY_TRY(call) do { esp_err_t e = (call); \
                          if (e != ESP_OK) return e; } while (0)

  PHY_TRY(priv->phy->get_addr(priv->phy, &priv->phyaddr));
  PHY_TRY(emac_read(eth, priv->phyaddr, 2, &id1));
  PHY_TRY(emac_read(eth, priv->phyaddr, 3, &id2));
  syslog(LOG_INFO, "eth0: PHY addr=%" PRIu32 " id=%04" PRIx32
         "%04" PRIx32 "\n", priv->phyaddr, id1, id2);
  if ((id1 << 16 | id2) != YT8531_PHY_ID &&
      (id1 << 16 | id2) != YT8531S_PHY_ID)
    {
      syslog(LOG_ERR, "eth0: expected YT8531; refusing extended writes\n");
      return ESP_ERR_NOT_SUPPORTED;
    }

  PHY_TRY(priv->phy->autonego_ctrl(priv->phy, ESP_ETH_PHY_AUTONEGO_EN,
                                  &enabled));
  PHY_TRY(emac_write(eth, priv->phyaddr, 0x1e, 0xa001));
  PHY_TRY(emac_read(eth, priv->phyaddr, 0x1f, &value));
  PHY_TRY(emac_write(eth, priv->phyaddr, 0x1f, value | (1u << 8)));
  PHY_TRY(emac_write(eth, priv->phyaddr, 0x1e, 0xa003));
  PHY_TRY(emac_read(eth, priv->phyaddr, 0x1f, &value));
  PHY_TRY(emac_write(eth, priv->phyaddr, 0x1f, (value & ~0xffu) | 0xddu));
  PHY_TRY(priv->phy->autonego_ctrl(priv->phy, ESP_ETH_PHY_AUTONEGO_RESTART,
                                  &enabled));
#undef PHY_TRY
  return ESP_OK;
}

static int emac_ifup(struct net_driver_s *dev)
{
  struct s31_emac_s *priv = dev->d_private;
  esp_err_t ret;

  if (priv->ifup)
    {
      return OK;
    }

  ret = priv->phy->init(priv->phy);
  if (ret == ESP_OK)
    {
      ret = emac_yt8531(priv);
    }

  if (ret != ESP_OK)
    {
      syslog(LOG_ERR, "eth0: PHY initialization error 0x%x\n", ret);
      return -EIO;
    }

  ret = priv->mac->set_addr(priv->mac, dev->d_mac.ether.ether_addr_octet);
  if (ret != ESP_OK)
    {
      return -EIO;
    }

  priv->ifup = true;
  priv->link = false;
  priv->pending_len = 0;
  priv->mdio_errors = 0;
  priv->lastlink = clock_systime_ticks() - EMAC_LINK_TICKS;
  netdev_carrier_off(dev);
  return OK;
}

static int emac_ifdown(struct net_driver_s *dev)
{
  struct s31_emac_s *priv = dev->d_private;
  esp_err_t ret;

  priv->ifup = false;
  priv->link = false;
  if (priv->pending_len)
    {
      NETDEV_TXERRORS(dev);
      priv->pending_len = 0;
    }

  dev->d_len = 0;
  netdev_carrier_off(dev);
  ret = priv->mac->stop(priv->mac);
  /* Keep PHY powered: RGMII RX clock is required for MAC reset on restart. */

  return ret == ESP_OK ? OK : -EIO;
}

#ifdef CONFIG_NETDEV_IOCTL
static int emac_ioctl(struct net_driver_s *dev, int cmd, unsigned long arg)
{
  struct s31_emac_s *priv = dev->d_private;
  struct mii_ioctl_data_s *req = (struct mii_ioctl_data_s *)(uintptr_t)arg;
  uint32_t value;

  if (req == NULL)
    {
      return -EINVAL;
    }

  switch (cmd)
    {
      case SIOCGMIIPHY:
        req->phy_id = priv->phyaddr;
        return OK;
      case SIOCGMIIREG:
        if (req->phy_id > 31 || req->reg_num > 31)
          {
            return -EINVAL;
          }

        if (emac_read(&priv->mediator, req->phy_id, req->reg_num, &value)
            != ESP_OK)
          {
            return -EIO;
          }

        req->val_out = value;
        return OK;
      default:
        return -ENOTTY;
    }
}
#endif

int esp32s31_emac_initialize(void)
{
  struct s31_emac_s *priv = &g_emac;
  eth_esp32_emac_config_t hw = ETH_ESP32_EMAC_DEFAULT_CONFIG();
  eth_mac_config_t mc = ETH_MAC_DEFAULT_CONFIG();
  eth_phy_config_t pc = ETH_PHY_DEFAULT_CONFIG();
  bool mac_initialized = false;
  int ret = -EIO;

  if (priv->registered)
    {
      return OK;
    }

  hw.smi_gpio.mdc_num = BOARD_ETH_MDC;
  hw.smi_gpio.mdio_num = BOARD_ETH_MDIO;
  hw.clock_config.rgmii.clock_tx_gpio = BOARD_ETH_TX_CLK;
  hw.clock_config.rgmii.clock_rx_gpio = BOARD_ETH_RX_CLK;
  hw.emac_dataif_gpio.rgmii.tx_ctl_num = BOARD_ETH_TX_CTL;
  hw.emac_dataif_gpio.rgmii.txd0_num = BOARD_ETH_TXD0;
  hw.emac_dataif_gpio.rgmii.txd1_num = BOARD_ETH_TXD1;
  hw.emac_dataif_gpio.rgmii.txd2_num = BOARD_ETH_TXD2;
  hw.emac_dataif_gpio.rgmii.txd3_num = BOARD_ETH_TXD3;
  hw.emac_dataif_gpio.rgmii.rx_ctl_num = BOARD_ETH_RX_CTL;
  hw.emac_dataif_gpio.rgmii.rxd0_num = BOARD_ETH_RXD0;
  hw.emac_dataif_gpio.rgmii.rxd1_num = BOARD_ETH_RXD1;
  hw.emac_dataif_gpio.rgmii.rxd2_num = BOARD_ETH_RXD2;
  hw.emac_dataif_gpio.rgmii.rxd3_num = BOARD_ETH_RXD3;
  pc.phy_addr = -1;
  pc.reset_gpio_num = BOARD_ETH_RESET;
  pc.autonego_timeout_ms = 0;
  priv->mediator.phy_reg_read = emac_read;
  priv->mediator.phy_reg_write = emac_write;
  priv->mediator.on_state_changed = emac_state;
  priv->dev.d_private = priv;
  priv->dev.d_buf = priv->buffer;
  priv->dev.d_ifup = emac_ifup;
  priv->dev.d_ifdown = emac_ifdown;
  priv->dev.d_txavail = emac_txavail;
#ifdef CONFIG_NETDEV_IOCTL
  priv->dev.d_ioctl = emac_ioctl;
#endif

  priv->mac = esp_eth_mac_new_esp32(&hw, &mc);
  if (priv->mac == NULL)
    {
      goto failed;
    }

  priv->phy = esp_eth_phy_new_generic(&pc);
  if (priv->phy == NULL)
    {
      ret = -ENOMEM;
      goto failed;
    }

  if (priv->mac->set_mediator(priv->mac, &priv->mediator) != ESP_OK ||
      priv->phy->set_mediator(priv->phy, &priv->mediator) != ESP_OK ||
      priv->phy->reset_hw(priv->phy) != ESP_OK ||
      priv->mac->init(priv->mac) != ESP_OK)
    {
      goto failed;
    }

  mac_initialized = true;
  if (priv->mac->get_addr(priv->mac, priv->dev.d_mac.ether.ether_addr_octet)
      != ESP_OK)
    {
      goto failed;
    }

  ret = netdev_register(&priv->dev, NET_LL_ETHERNET);
  if (ret < 0)
    {
      goto failed;
    }

  priv->registered = true;
  priv->irq_ready = true;
  work_queue(LPWORK, &priv->servicework, emac_service, priv,
             EMAC_SERVICE_TICKS);
  syslog(LOG_INFO, "eth0: native S31 RGMII registered, YT8531 GPIO5-19\n");
  return OK;

failed:
  if (mac_initialized)
    {
      priv->mac->deinit(priv->mac);
    }

  if (priv->phy)
    {
      priv->phy->del(priv->phy);
      priv->phy = NULL;
    }

  if (priv->mac)
    {
      priv->mac->del(priv->mac);
      priv->mac = NULL;
    }

  return ret;
}
