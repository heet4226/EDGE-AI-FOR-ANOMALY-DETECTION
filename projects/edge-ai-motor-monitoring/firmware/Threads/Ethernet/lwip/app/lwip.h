/*
 * lwip.h
 *
 *  Created on: Oct 23, 2025
 */

#ifndef APP_LWIP_H_
#define APP_LWIP_H_

#include "../../../Ethernet/src/include/lwip/netif.h"

#include "lwip/ssi_handler/ssi_handler.h"
#include "lwip/target/ethernetif.h"
#include "src/include/lwip/apps/httpd.h"
#include "src/include/lwip/dhcp.h"
#include "src/include/lwip/init.h"
#include "src/include/lwip/netif.h"
#include "src/include/netif/ethernet.h"

#include "../../Threads/Ethernet/lwip/ssi_handler/ssi_handler.h"
#include "../../Threads/Ethernet/lwip/target/ethernetif.h"
#include "../../Threads/Ethernet/src/include/lwip/apps/httpd.h"
#include "../../Threads/Ethernet/src/include/lwip/dhcp.h"
#include "../../Threads/Ethernet/src/include/lwip/ip4_addr.h"
#include "../../Threads/Ethernet/src/include/lwip/netif.h"
#include "../../Threads/Ethernet/src/include/lwip/timeouts.h"
#include "../../Threads/Ethernet/src/include/netif/ethernet.h"

/* Define led ports*/
#define LED_GREEN_GPIO_Port	GPIOI
#define LED_GREEN_Pin		GPIO_PIN_9
#define LED_RED_GPIO_Port	GPIOF
#define LED_RED_Pin			GPIO_PIN_4

/*Static IP ADDRESS: IP_ADDR0.IP_ADDR1.IP_ADDR2.IP_ADDR3 */
#define IP_ADDR0   ((uint8_t) 198U)
#define IP_ADDR1   ((uint8_t) 162U)
#define IP_ADDR2   ((uint8_t) 0U)
#define IP_ADDR3   ((uint8_t) 10U)
/*NETMASK*/
#define NETMASK_ADDR0   ((uint8_t) 255U)
#define NETMASK_ADDR1   ((uint8_t) 255U)
#define NETMASK_ADDR2   ((uint8_t) 255U)
#define NETMASK_ADDR3   ((uint8_t) 0U)
/*Gateway Address*/
#define GW_ADDR0   ((uint8_t) 198U)
#define GW_ADDR1   ((uint8_t) 162U)
#define GW_ADDR2   ((uint8_t) 0U)
#define GW_ADDR3   ((uint8_t) 1U)
void ethernet_link_status_updated(struct netif *netif);
void Ethernet_Link_Periodic_Handle(struct netif *netif);
#if LWIP_DHCP
void DHCP_Process(struct netif *netif);
void DHCP_Periodic_Handle(struct netif *netif);
#endif

#endif /* APP_LWIP_H_ */
