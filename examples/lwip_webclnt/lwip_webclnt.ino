// lwip web client
#include "lwip_t41.h"
#include "lwip/inet.h"
#include "lwip/dhcp.h"
#include "lwip/udp.h"
#include "lwip/tcp.h"
#include "lwip/stats.h"

#define WEBSERVER "192.168.1.4"

#define swap2 __builtin_bswap16
#define swap4 __builtin_bswap32

uint32_t rtt;

// debug stats stuff
extern "C" {
#if LWIP_STATS
  struct stats_ lwip_stats;
#endif
}

void print_stats() {
  // lwip stats_display() needed printf
#if LWIP_STATS
  char str[128];

  // my  LINK stats
  sprintf(str, "LINK in %d out %d drop %d memerr %d",
          lwip_stats.link.recv, lwip_stats.link.xmit, lwip_stats.link.drop, lwip_stats.link.memerr);
  Serial.println(str);
  sprintf(str, "TCP in %d out %d drop %d memerr %d",
          lwip_stats.tcp.recv, lwip_stats.tcp.xmit, lwip_stats.tcp.drop, lwip_stats.tcp.memerr);
  Serial.println(str);
  sprintf(str, "UDP in %d out %d drop %d memerr %d",
          lwip_stats.udp.recv, lwip_stats.udp.xmit, lwip_stats.udp.drop, lwip_stats.udp.memerr);
  Serial.println(str);
  sprintf(str, "ICMP in %d out %d",
          lwip_stats.icmp.recv, lwip_stats.icmp.xmit);
  Serial.println(str);
  sprintf(str, "ARP in %d out %d",
          lwip_stats.etharp.recv, lwip_stats.etharp.xmit);
  Serial.println(str);
#if MEM_STATS
  sprintf(str, "HEAP avail %d used %d max %d err %d",
          lwip_stats.mem.avail, lwip_stats.mem.used, lwip_stats.mem.max, lwip_stats.mem.err);
  Serial.println(str);
#endif
#endif
}

static void netif_status_callback(struct netif *netif)
{
  static char str1[IP4ADDR_STRLEN_MAX], str2[IP4ADDR_STRLEN_MAX], str3[IP4ADDR_STRLEN_MAX];
  Serial.printf("netif status changed: ip %s, mask %s, gw %s\n", ip4addr_ntoa_r(netif_ip_addr4(netif), str1, IP4ADDR_STRLEN_MAX), ip4addr_ntoa_r(netif_ip_netmask4(netif), str2, IP4ADDR_STRLEN_MAX), ip4addr_ntoa_r(netif_ip_gw4(netif), str3, IP4ADDR_STRLEN_MAX));
}

static void link_status_callback(struct netif *netif)
{
  Serial.printf("enet link status: %s\n", netif_is_link_up(netif) ? "up" : "down");
}


void tcperr_callback(void * arg, err_t err)
{
  // set with tcp_err()
  Serial.print("TCP err "); Serial.println(err);
  *(int *)arg = err;
}

err_t connect_callback(void *arg, struct tcp_pcb *tpcb, err_t err) {
  Serial.print("connected "); Serial.println(tcp_sndbuf(tpcb));
  *(int *)arg = 1;
  return 0;
}

void web_client(const char *query) {
  ip_addr_t server;
  struct tcp_pcb * pcb;
  int connected = 0;
  err_t err;
  uint32_t sendqlth;

  Serial.println("web client");
  inet_aton(WEBSERVER, &server);
  pcb = tcp_new();
  tcp_err(pcb, tcperr_callback);
  tcp_arg(pcb, &connected);
  tcp_bind(pcb, IP_ADDR_ANY, 3333);   // local port

  tcp_recv(pcb, recv_callback);  // all the action is now in callback
  sendqlth = tcp_sndbuf(pcb);

  do {
    err = tcp_connect(pcb, &server, 80, connect_callback);
    //Serial.print("err ");Serial.println(err);
    loop();
  } while (err < 0);
  while (!connected) loop();
  if (connected < 0) {
    Serial.println("connect error");
    return;  // err
  }

  do {
    err = tcp_write(pcb, query, strlen(query), TCP_WRITE_FLAG_COPY);
    loop();   // keep checkin while we blast
  } while ( err < 0);  // -1 is ERR_MEM
  tcp_output(pcb);

  while (tcp_sndbuf(pcb) != sendqlth) loop(); // wait til sent

}


err_t recv_callback(void * arg, struct tcp_pcb * tpcb, struct pbuf * p, err_t err)
{
  static uint32_t bytes = 0;

  if (p == NULL) {
    // other end closed
    Serial.println("remote closed");
    tcp_close(tpcb);
    return 0;
  }
  tcp_recved(tpcb, p->tot_len);  // data processed
  bytes += p->tot_len;
  Serial.write((char *)p->payload, p->tot_len);
  pbuf_free(p);
  return 0;
}

void setup()
{
  Serial.begin(9600);
  while (!Serial) delay(100);

  Serial.println(); Serial.print(F_CPU); Serial.print(" ");

  Serial.print(__TIME__); Serial.print(" "); Serial.println(__DATE__);
  enet_init(NULL, NULL, NULL);
  netif_set_status_callback(netif_default, netif_status_callback);
  netif_set_link_callback(netif_default, link_status_callback);
  netif_set_up(netif_default);

  dhcp_start(netif_default);

  while (!netif_is_link_up(netif_default)) loop(); // await on link up

  while (!dhcp_supplied_address(netif_default)) loop(); // wait for dhcp to finish

  web_client("GET /index.html\r\n");


#if 0
  // optional stats every 5 s, need LWIP_STATS 1 lwipopts.h
  while (1) {
    static uint32_t ms = millis();
    if (millis() - ms > 5000) {
      ms = millis();
      print_stats();
    }
    loop();  // poll
  }
#endif
}

void loop()
{
  static uint32_t last_ms;
  uint32_t ms;

  enet_proc_input();

  ms = millis();
  if (ms - last_ms > 100)
  {
    last_ms = ms;
    enet_poll();
  }
}
