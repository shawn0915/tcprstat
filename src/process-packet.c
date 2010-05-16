#include "process-packet.h"

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <net/ethernet.h>
#include <arpa/inet.h>

#include <string.h>

#include <pcap.h>
#include <pcap/sll.h>

#include "local-addresses.h"
#include "stats.h"

void
process_packet(unsigned char *user, const struct pcap_pkthdr *header,
        const unsigned char *packet)
{
    pcap_t *pcap;
    const struct sll_header *sll;
    const struct ether_header *ether_header;
    const struct ip *ip;
    unsigned short packet_type;
    
    pcap = (pcap_t *) user;

    // Parse packet
    switch (pcap_datalink(pcap)) {
        
    case DLT_LINUX_SLL:
        sll = (struct sll_header *) packet;
        packet_type = ntohs(sll->sll_protocol);
        ip = (const struct ip *) (packet + sizeof(struct sll_header));
        
        break;
        
    case DLT_EN10MB:
        ether_header = (struct ether_header *) packet;
        packet_type = ntohs(ether_header->ether_type);
        ip = (const struct ip *) (packet + sizeof(struct ether_header));
        
        break;
        
    case DLT_RAW:
        packet_type = ETHERTYPE_IP; //This is raw ip
        ip = (const struct ip *) packet;
        
        break;
        
    default:
        
        return;
        
    }
    
    if (packet_type != ETHERTYPE_IP)
        return;
    
    process_ip(pcap, ip);
    
}

int
process_ip(pcap_t *dev, const struct ip *ip) {
    char src[16], dst[16], *addr;
    int incoming;
    unsigned len;
    
    addr = inet_ntoa(ip->ip_src);
    strncpy(src, addr, 15);
    src[15] = '\0';
    
    addr = inet_ntoa(ip->ip_dst);
    strncpy(dst, addr, 15);
    dst[15] = '\0';
    
    if (is_local_address(ip->ip_src))
        incoming = 0;
    else if (is_local_address(ip->ip_dst))
        incoming = 1;
    else {
#if defined(DEBUG)
        char *src, *dst;
        src = strdup(inet_ntoa(ip->ip_src));
        dst = strdup(inet_ntoa(ip->ip_dst));
        printf("UNKNOWN DIRS %s:%s\n", src, dst);
        free(src);
        free(dst);
#endif
        return 1;
    }
    
    len = htons(ip->ip_len);
    
#if defined(DEBUG)
     printf("%s:%s\tsz %u", src, dst, len);
#endif
    
    switch (ip->ip_p) {
        struct tcphdr *tcp;
        uint16_t sport, dport, lport, rport;
        unsigned datalen;
    
    case IPPROTO_TCP:
        tcp = (struct tcphdr *) ((unsigned char *) ip + sizeof(struct ip));
        
#if defined(__FAVOR_BSD)
        sport = ntohs(tcp->th_sport);
        dport = ntohs(tcp->th_dport);
        datalen = len - sizeof(struct ip) - tcp->th_off * 4;    // 4 bits offset 
#else
        sport = ntohs(tcp->source);
        dport = ntohs(tcp->dest);
        datalen = len - sizeof(struct ip) - tcp->doff * 4;
#endif

        if (incoming) {
            lport = dport;
            rport = sport;
            
            inbound(ip->ip_dst, ip->ip_src, lport, rport);
            
        }
        else {
            lport = sport;
            rport = dport;
            
            outbound(ip->ip_src, ip->ip_dst, lport, rport);
            
        }

        break;
        
    default:
        break;
        
    }
    
    return 0;
    
}
