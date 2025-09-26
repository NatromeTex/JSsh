#include "quickjs.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <math.h>
#include <sys/time.h>

#define PACKET_SIZE 64
#define PING_COUNT 4
#define TIMEOUT_SEC 1

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
