#include <math.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <net/if.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include "quickjs.h"

#define PACKET_SIZE 64
#define PING_COUNT 4
#define MAX_HOPS 30
#define PROBE_PORT 33434
#define TIMEOUT_SEC 2

// checksum
static unsigned short icmp_checksum(void *b, int len) {
    unsigned short *buf = b;
    unsigned int sum = 0;
    for (; len > 1; len -= 2) sum += *buf++;
    if (len == 1) sum += *(unsigned char*)buf;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    return ~sum;
}

// get current time in ms
static double get_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

// State table
static const char *tcp_state_name(unsigned int st) {
    switch (st) {
        case 1: return "ESTABLISHED";
        case 2: return "SYN_SENT";
        case 3: return "SYN_RECV";
        case 4: return "FIN_WAIT1";
        case 5: return "FIN_WAIT2";
        case 6: return "TIME_WAIT";
        case 7: return "CLOSE";
        case 8: return "CLOSE_WAIT";
        case 9: return "LAST_ACK";
        case 0xA: return "LISTEN";
        case 0xB: return "CLOSING";
        default: return "UNKNOWN";
    }
}

// ---------------------------------------------------------

// net.ping()
JSValue js_net_ping(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1) return JS_NewString(ctx, "host required");
    const char *host = JS_ToCString(ctx, argv[0]);
    if (!host) return JS_UNDEFINED;

    struct hostent *he = gethostbyname(host);
    if (!he) {
        JS_FreeCString(ctx, host);
        return JS_NewString(ctx, "host not found");
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr = *(struct in_addr*)he->h_addr;

    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock < 0) {
        int e = errno;
        char err[128];
        snprintf(err, sizeof(err), "socket() failed: %s (%d)", strerror(e), e);
        JS_FreeCString(ctx, host);
        return JS_NewString(ctx, err);
    }

    struct timeval tv = {TIMEOUT_SEC, 0};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    int transmitted = 0, received = 0;
    double times[PING_COUNT];
    char result[4096];
    result[0] = '\0';

    for (int seq = 0; seq < PING_COUNT; seq++) {
        struct icmp pkt;
        memset(&pkt, 0, sizeof(pkt));
        pkt.icmp_type = ICMP_ECHO;
        pkt.icmp_code = 0;
        pkt.icmp_id = getpid() & 0xFFFF;
        pkt.icmp_seq = seq;
        pkt.icmp_cksum = icmp_checksum(&pkt, sizeof(pkt));

        double start = get_time_ms();
        if (sendto(sock, &pkt, sizeof(pkt), 0, (struct sockaddr*)&addr, sizeof(addr)) <= 0) {
            strcat(result, "send failed\n");
            continue;
        }
        transmitted++;

        char buf[PACKET_SIZE + 64];
        socklen_t addrlen = sizeof(addr);
        int n = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&addr, &addrlen);
        double end = get_time_ms();

        if (n > 0) {
            received++;
            double rtt = end - start;
            times[seq] = rtt;
            char line[128];
            snprintf(line, sizeof(line), "64 bytes from %s: icmp_seq=%d ttl=? time=%.3f ms\n",
                     inet_ntoa(addr.sin_addr), seq+1, rtt);
            strcat(result, line);
        } else {
            strcat(result, "Request timeout for icmp_seq\n");
            times[seq] = 0;
        }

        usleep(1000000); // 1 sec between pings
    }

    close(sock);
    JS_FreeCString(ctx, host);

    // compute statistics
    double rtt_min = 1e9, rtt_max = 0, rtt_sum = 0, rtt_sum2 = 0;
    for (int i = 0; i < PING_COUNT; i++) {
        if (times[i] > 0) {
            if (times[i] < rtt_min) rtt_min = times[i];
            if (times[i] > rtt_max) rtt_max = times[i];
            rtt_sum += times[i];
            rtt_sum2 += times[i]*times[i];
        }
    }
    double rtt_avg = rtt_sum / received;
    double rtt_mdev = 0;
    if (received > 0) rtt_mdev = sqrt(rtt_sum2/received - rtt_avg*rtt_avg);

    char stats[256];
    snprintf(stats, sizeof(stats),
             "%d packets transmitted, %d received, %.0f%% packet loss\n"
             "rtt min=%.3fms avg=%.3fms max=%.3fms mdev=%.3fms\n",
             transmitted, received, ((transmitted-received)/(double)transmitted)*100.0,
             rtt_min, rtt_avg, rtt_max, rtt_mdev);

    strcat(result, stats);

    return JS_NewString(ctx, result);
}

// net.netstat()
JSValue js_net_netstat(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    const char *files[][2] = {
        {"/proc/net/tcp",  "tcp"},
        {"/proc/net/udp",  "udp"},
        {"/proc/net/tcp6", "tcp6"},
        {"/proc/net/udp6", "udp6"},
    };

    char result[32768];
    result[0] = '\0';

    for (int fidx = 0; fidx < 4; fidx++) {
        FILE *f = fopen(files[fidx][0], "r");
        if (!f) continue;

        char line[512];
        if (!fgets(line, sizeof(line), f)) {
            fclose(f);
            continue;
        }

        while (fgets(line, sizeof(line), f)) {
            unsigned long local_addr[4], rem_addr[4];
            unsigned int local_port, rem_port, state;

            if (fidx < 2) {
                // IPv4
                if (sscanf(line, "%*d: %lx:%x %lx:%x %x",
                           &local_addr[0], &local_port,
                           &rem_addr[0], &rem_port,
                           &state) < 5) continue;

                struct in_addr laddr = { .s_addr = htonl(local_addr[0]) };
                struct in_addr raddr = { .s_addr = htonl(rem_addr[0]) };

                char lbuf[64], rbuf[64], out[512];
                snprintf(lbuf, sizeof(lbuf), "%s:%u", inet_ntoa(laddr), local_port);
                snprintf(rbuf, sizeof(rbuf), "%s:%u", inet_ntoa(raddr), rem_port);

                snprintf(out, sizeof(out), "%-4s %-22s %-22s %s\n",
                         files[fidx][1], lbuf, rbuf, tcp_state_name(state));
                strncat(result, out, sizeof(result) - strlen(result) - 1);

            } else {
                // IPv6
                if (sscanf(line,
                           "%*d: %08lx%08lx%08lx%08lx:%x %08lx%08lx%08lx%08lx:%x %x",
                           &local_addr[0], &local_addr[1], &local_addr[2], &local_addr[3], &local_port,
                           &rem_addr[0], &rem_addr[1], &rem_addr[2], &rem_addr[3], &rem_port,
                           &state) < 11) continue;

                struct in6_addr l6, r6;
                for (int i = 0; i < 4; i++) {
                    ((unsigned long *)l6.s6_addr)[i] = htonl(local_addr[i]);
                    ((unsigned long *)r6.s6_addr)[i] = htonl(rem_addr[i]);
                }

                char lbuf[128], rbuf[128], out[512];
                inet_ntop(AF_INET6, &l6, lbuf, sizeof(lbuf));
                inet_ntop(AF_INET6, &r6, rbuf, sizeof(rbuf));

                char lfull[220], rfull[220];
                snprintf(lfull, sizeof(lfull), "[%s]:%u", lbuf, local_port);
                snprintf(rfull, sizeof(rfull), "[%s]:%u", rbuf, rem_port);

                snprintf(out, sizeof(out), "%-4s %-40s %-40s %s\n",
                         files[fidx][1], lfull, rfull, tcp_state_name(state));
                strncat(result, out, sizeof(result) - strlen(result) - 1);
            }
        }

        fclose(f);
    }

    return JS_NewString(ctx, result);
}

//net.ifconfig()
JSValue js_ifconfig(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    int fd;
    struct ifconf ifc;
    struct ifreq *ifr;
    char buf[8192];
    char *out;
    size_t out_size = 32768;  // generous buffer
    int i, n;
    int minimal = 0;

    if (argc > 0 && JS_IsString(argv[0])) {
        const char *mode = JS_ToCString(ctx, argv[0]);
        if (mode && strcmp(mode, "min") == 0)
            minimal = 1;
        JS_FreeCString(ctx, mode);
    }

    out = malloc(out_size);
    if (!out) return JS_ThrowOutOfMemory(ctx);
    out[0] = '\0';

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        free(out);
        return JS_ThrowTypeError(ctx, "socket() failed");
    }

    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if (ioctl(fd, SIOCGIFCONF, &ifc) < 0) {
        close(fd);
        free(out);
        return JS_ThrowTypeError(ctx, "ioctl(SIOCGIFCONF) failed");
    }

    ifr = ifc.ifc_req;
    n = ifc.ifc_len / sizeof(struct ifreq);

    // Track seen interfaces to avoid duplicates
    char seen_interfaces[128][IFNAMSIZ];
    int seen_count = 0;

    for (i = 0; i < n; i++) {
        // Check if we've already processed this interface
        int is_duplicate = 0;
        for (int j = 0; j < seen_count; j++) {
            if (strcmp(ifr[i].ifr_name, seen_interfaces[j]) == 0) {
                is_duplicate = 1;
                break;
            }
        }
        
        if (is_duplicate) {
            continue; // Skip duplicate interface
        }
        
        // Add to seen interfaces
        strncpy(seen_interfaces[seen_count], ifr[i].ifr_name, IFNAMSIZ);
        seen_count++;

        char line[1024];
        struct ifreq ifdata;
        struct sockaddr_in *sa;
        char ip[64] = "N/A";
        char netmask[64] = "N/A";
        char brd[64] = "N/A";
        char mac[32] = "N/A";
        int mtu = 0;
        short flags = 0;

        strncpy(ifdata.ifr_name, ifr[i].ifr_name, IFNAMSIZ);

        // IP address
        if (ioctl(fd, SIOCGIFADDR, &ifdata) == 0) {
            sa = (struct sockaddr_in*)&ifdata.ifr_addr;
            snprintf(ip, sizeof(ip), "%s", inet_ntoa(sa->sin_addr));
        }

        // Netmask
        if (ioctl(fd, SIOCGIFNETMASK, &ifdata) == 0) {
            sa = (struct sockaddr_in*)&ifdata.ifr_netmask;
            snprintf(netmask, sizeof(netmask), "%s", inet_ntoa(sa->sin_addr));
        }

        // Broadcast
        if (ioctl(fd, SIOCGIFBRDADDR, &ifdata) == 0) {
            sa = (struct sockaddr_in*)&ifdata.ifr_broadaddr;
            snprintf(brd, sizeof(brd), "%s", inet_ntoa(sa->sin_addr));
        }

        // MAC address
        if (ioctl(fd, SIOCGIFHWADDR, &ifdata) == 0) {
            unsigned char *hw = (unsigned char*)ifdata.ifr_hwaddr.sa_data;
            snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x", hw[0], hw[1], hw[2], hw[3], hw[4], hw[5]);
        }

        // Flags
        if (ioctl(fd, SIOCGIFFLAGS, &ifdata) == 0) {
            flags = ifdata.ifr_flags;
        }

        // MTU
        if (ioctl(fd, SIOCGIFMTU, &ifdata) == 0) {
            mtu = ifdata.ifr_mtu;
        }

        if (minimal) {
            snprintf(line, sizeof(line),
                     "%s: %s\n",
                     ifr[i].ifr_name, ip);
        } else {
            snprintf(line, sizeof(line),
                     "%s:  Link encap:Ethernet  HWaddr %s\n"
                     "      inet addr:%s  Bcast:%s  Mask:%s\n"
                     "      Flags:0x%x  MTU:%d\n\n",
                     ifr[i].ifr_name, mac,
                     ip, brd, netmask,
                     flags, mtu);
        }

        strncat(out, line, out_size - strlen(out) - 1);
    }

    close(fd);

    JSValue ret = JS_NewString(ctx, out);
    free(out);
    return ret;
}

//net.tracert()
JSValue js_tracert(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1 || !JS_IsString(argv[0])) {
        return JS_ThrowTypeError(ctx, "Expected hostname");
    }
    const char *host = JS_ToCString(ctx, argv[0]);
    if (!host) return JS_EXCEPTION;
    
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    
    if (getaddrinfo(host, NULL, &hints, &res) != 0) {
        JS_FreeCString(ctx, host);
        return JS_ThrowTypeError(ctx, "getaddrinfo failed");
    }
    
    int sendfd = socket(AF_INET, SOCK_DGRAM, 0);
    int recvfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sendfd < 0 || recvfd < 0) {
        freeaddrinfo(res);
        JS_FreeCString(ctx, host);
        return JS_ThrowTypeError(ctx, "socket() failed");
    }
    
    struct timeval tv = {TIMEOUT_SEC, 0};
    setsockopt(recvfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    char *out = malloc(16384);
    out[0] = '\0';
    
    struct sockaddr_in *dest = (struct sockaddr_in*)res->ai_addr;
    char dest_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &dest->sin_addr, dest_ip, sizeof(dest_ip));
    
    // Header
    char header[256];
    snprintf(header, sizeof(header), "Tracing route to %s [%s]\nover a maximum of %d hops:\n\n", 
             host, dest_ip, MAX_HOPS);
    strncat(out, header, 16384 - strlen(out) - 1);
    
    dest->sin_port = htons(PROBE_PORT);
    char sendbuf[32] = {0};
    
    for (int ttl = 1; ttl <= MAX_HOPS; ttl++) {
        setsockopt(sendfd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));
        
        struct timeval start, end;
        gettimeofday(&start, NULL);
        
        sendto(sendfd, sendbuf, sizeof(sendbuf), 0,
               (struct sockaddr*)dest, sizeof(*dest));
        
        struct sockaddr_in reply_addr;
        socklen_t addrlen = sizeof(reply_addr);
        char recvbuf[512];
        int n = recvfrom(recvfd, recvbuf, sizeof(recvbuf), 0, 
                        (struct sockaddr*)&reply_addr, &addrlen);
        
        gettimeofday(&end, NULL);
        long ms = (end.tv_sec - start.tv_sec) * 1000 + 
                  (end.tv_usec - start.tv_usec) / 1000;
        
        char line[256];
        if (n < 0) {
            snprintf(line, sizeof(line), " %2d     *        *        *     Request timed out.\n", ttl);
        } else {
            char ipstr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &reply_addr.sin_addr, ipstr, sizeof(ipstr));
            
            // Reverse DNS lookup
            char hostname[256] = {0};
            struct sockaddr_in sa;
            sa.sin_family = AF_INET;
            sa.sin_addr = reply_addr.sin_addr;
            if (getnameinfo((struct sockaddr*)&sa, sizeof(sa), hostname, sizeof(hostname),
                          NULL, 0, 0) == 0) {
                snprintf(line, sizeof(line), " %2d   %3ld ms   %3ld ms   %3ld ms  %s [%s]\n", 
                        ttl, ms, ms, ms, hostname, ipstr);
            } else {
                snprintf(line, sizeof(line), " %2d   %3ld ms   %3ld ms   %3ld ms  %s\n", 
                        ttl, ms, ms, ms, ipstr);
            }
            
            // Check if we reached destination
            if (reply_addr.sin_addr.s_addr == dest->sin_addr.s_addr) {
                strncat(out, line, 16384 - strlen(out) - 1);
                break;
            }
        }
        strncat(out, line, 16384 - strlen(out) - 1);
    }
    
    strncat(out, "\nTrace complete.\n", 16384 - strlen(out) - 1);
    
    close(sendfd);
    close(recvfd);
    freeaddrinfo(res);
    JS_FreeCString(ctx, host);
    
    JSValue ret = JS_NewString(ctx, out);
    free(out);
    return ret;
}