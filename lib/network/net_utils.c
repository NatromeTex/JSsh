#include <math.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <net/if.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <libssh/libssh.h>
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

// state table
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

// ip formatter
static void format_ip(char *dst, size_t size, unsigned int ip) {
    struct in_addr addr;
    addr.s_addr = ip;
    inet_ntop(AF_INET, &addr, dst, size);
}

// struct for ssh
struct ssh_param {
    char *user;
    char *host;
    int port;
    int verbosity;
};

// free function
void ssh_param_free(struct ssh_param *params) {
    if (!params) {
        return;
    }
    free(params->user);
    free(params->host);
    free(params);
}

// parse ssh params
struct ssh_param *ssh_param_parse(const char *input_str) {
    if (!input_str) {
        return NULL;
    }

    // Allocate the main struct
    struct ssh_param *deets = malloc(sizeof(struct ssh_param));
    if (!deets) {
        return NULL; // Out of memory
    }

    // Initialize with defaults
    deets->user = NULL;
    deets->host = NULL;
    deets->port = 22; // Default SSH port
    deets->verbosity = SSH_LOG_PROTOCOL;

    const char *host_part_start;
    const char *at_ptr = strchr(input_str, '@'); // Find first '@'

    if (at_ptr) { // User is present
        size_t user_len = at_ptr - input_str;
        if (user_len == 0) {
            fprintf(stderr, "Error: Username cannot be empty.\n");
            ssh_param_free(deets);
            return NULL;
        }
        deets->user = malloc(user_len + 1);
        if (!deets->user) {
            ssh_param_free(deets);
            return NULL;
        }
        memcpy(deets->user, input_str, user_len);
        deets->user[user_len] = '\0';
        host_part_start = at_ptr + 1;
    } else { // No user
        host_part_start = input_str;
    }
    
    // Check for empty host part
    if (*host_part_start == '\0') {
        fprintf(stderr, "Error: Hostname part cannot be empty.\n");
        ssh_param_free(deets);
        return NULL;
    }

    const char *colon_ptr = strrchr(host_part_start, ':');
    const char *ipv6_end_bracket = strrchr(host_part_start, ']');

    // A colon is a port separator if it's not part of an IPv6 address,
    // which means it must appear *after* any ']' character.
    if (colon_ptr && (!ipv6_end_bracket || colon_ptr > ipv6_end_bracket)) {
        size_t host_len = colon_ptr - host_part_start;
        if (host_len == 0) {
            fprintf(stderr, "Error: Hostname cannot be empty.\n");
            ssh_param_free(deets);
            return NULL;
        }
        deets->host = malloc(host_len + 1);
        if (!deets->host) {
            ssh_param_free(deets);
            return NULL;
        }
        memcpy(deets->host, host_part_start, host_len);
        deets->host[host_len] = '\0';
        
        // Parse port
        const char *port_str = colon_ptr + 1;
        char *endptr;
        errno = 0;
        long port_val = strtol(port_str, &endptr, 10);
        if (errno != 0 || *endptr != '\0' || port_val <= 0 || port_val > 65535) {
            fprintf(stderr, "Error: Invalid port number '%s'.\n", port_str);
            ssh_param_free(deets);
            return NULL;
        }
        deets->port = port_val;

    } else { // No port specified
        deets->host = strdup(host_part_start);
        if (!deets->host) {
            ssh_param_free(deets);
            return NULL;
        }
    }

    return deets;
}

static struct termios orig_termios;
static int raw_mode_active = 0;

// raw mode
static void enable_raw_mode() {
    if (raw_mode_active) return;  // Already enabled
    
    struct termios raw;
    tcgetattr(STDIN_FILENO, &orig_termios);
    raw = orig_termios;
    cfmakeraw(&raw);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    raw_mode_active = 1;
}

static void disable_raw_mode() {
    if (!raw_mode_active) return;  // Already disabled
    
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    raw_mode_active = 0;
}

// shell function
int interactive_shell(ssh_session session) {
    ssh_channel channel = NULL;
    int rc = -1;
    char buffer[2048]; // allocate 2MB for I/O buffer

    channel = ssh_channel_new(session);
    if (!channel) return -1;
    if (ssh_channel_open_session(channel) != SSH_OK) goto cleanup;
    if (ssh_channel_request_pty(channel) != SSH_OK) goto cleanup;
    if (ssh_channel_request_shell(channel) != SSH_OK) goto cleanup;

    enable_raw_mode();

    while (ssh_channel_is_open(channel) && !ssh_channel_is_eof(channel)) {
        fd_set fds;
        int maxfd;
        int nbytes;

        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        FD_SET(ssh_get_fd(session), &fds);
        maxfd = ssh_get_fd(session) + 1;

        int sel = select(maxfd, &fds, NULL, NULL, NULL);
        if (sel < 0) break;

        // User input → remote
        if (FD_ISSET(STDIN_FILENO, &fds)) {
            nbytes = read(STDIN_FILENO, buffer, sizeof(buffer));
            if (nbytes <= 0) {
                // User input closed (Ctrl-D)
                ssh_channel_send_eof(channel);
                break;
            }
            ssh_channel_write(channel, buffer, nbytes);
        }

        // Remote output → user
        if (FD_ISSET(ssh_get_fd(session), &fds)) {
            if (ssh_channel_is_eof(channel) || ssh_channel_is_closed(channel)) {
                // Drain remaining output before exit
                while ((nbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 0)) > 0)
                    write(STDOUT_FILENO, buffer, nbytes);
                break;
            }

            nbytes = ssh_channel_read_nonblocking(channel, buffer, sizeof(buffer), 0);
            if (nbytes > 0)
                write(STDOUT_FILENO, buffer, nbytes);
        }
    }

    rc = 0;

cleanup:
    disable_raw_mode();

    if (channel) {
        ssh_channel_send_eof(channel);
        ssh_channel_close(channel);
        ssh_channel_free(channel);
    }

    return rc;
}


// host verifier for ssh
int verify_knownhost(ssh_session session)
{
    enum ssh_known_hosts_e state;
    unsigned char *hash = NULL;
    ssh_key srv_pubkey = NULL;
    size_t hlen;
    char buf[10];
    char *hexa;
    char *p;
    int cmp;
    int rc;
 
    rc = ssh_get_server_publickey(session, &srv_pubkey);
    if (rc < 0) {
        printf("0\n");
        return -1;
    }
 
    rc = ssh_get_publickey_hash(srv_pubkey,
                                SSH_PUBLICKEY_HASH_SHA1,
                                &hash,
                                &hlen);
    ssh_key_free(srv_pubkey);
    if (rc < 0) {
        printf("0\n");
        return -1;
    }
 
    state = ssh_session_is_known_server(session);
    switch (state) {
        case SSH_KNOWN_HOSTS_OK:
            // OK
 
            break;
        case SSH_KNOWN_HOSTS_CHANGED:
            fprintf(stderr, "Host key for server changed: it is now:\n");
            ssh_print_hexa("Public key hash", hash, hlen);
            fprintf(stderr, "For security reasons, connection will be stopped\n");
            ssh_clean_pubkey_hash(&hash);
 
            return -1;
        case SSH_KNOWN_HOSTS_OTHER:
            fprintf(stderr, "The host key for this server was not found but an other"
                    "type of key exists.\n");
            fprintf(stderr, "An attacker might change the default server key to"
                    "confuse your client into thinking the key does not exist\n");
            ssh_clean_pubkey_hash(&hash);
 
            return -1;
        case SSH_KNOWN_HOSTS_NOT_FOUND:
            fprintf(stderr, "Could not find known host file.\n");
            fprintf(stderr, "If you accept the host key here, the file will be"
                    "automatically created.\n");
 
            // FALL THROUGH to SSH_SERVER_NOT_KNOWN behavior
 
        case SSH_KNOWN_HOSTS_UNKNOWN:
            hexa = ssh_get_hexa(hash, hlen);
            fprintf(stderr,"The server is unknown. Do you trust the host key?\n");
            fprintf(stderr, "Public key hash: %s\n", hexa);
            ssh_string_free_char(hexa);
            ssh_clean_pubkey_hash(&hash);
            p = fgets(buf, sizeof(buf), stdin);
            if (p == NULL) {
                return -1;
            }
 
            cmp = strncasecmp(buf, "yes", 3);
            if (cmp != 0) {
                return -1;
            }
 
            rc = ssh_session_update_known_hosts(session);
            if (rc < 0) {
                fprintf(stderr, "Error %s\n", strerror(errno));
                return -1;
            }
 
            break;
        case SSH_KNOWN_HOSTS_ERROR:
            fprintf(stderr, "Error %s", ssh_get_error(session));
            ssh_clean_pubkey_hash(&hash);
            return -1;
    }
 
    ssh_clean_pubkey_hash(&hash);
    return 0;
}

// ---------------------------------------------------------

// net.ping()
JSValue js_net_ping(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1 || !JS_IsString(argv[0])) {
        return JS_ThrowTypeError(ctx, "Expected hostname");
    }
    
    const char *host = JS_ToCString(ctx, argv[0]);
    if (!host) return JS_EXCEPTION;
    
    // Use getaddrinfo instead of deprecated gethostbyname
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_RAW;
    hints.ai_protocol = IPPROTO_ICMP;
    
    if (getaddrinfo(host, NULL, &hints, &res) != 0) {
        JS_FreeCString(ctx, host);
        return JS_ThrowTypeError(ctx, "Cannot resolve hostname");
    }
    
    struct sockaddr_in *addr = (struct sockaddr_in*)res->ai_addr;
    char dest_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr->sin_addr, dest_ip, sizeof(dest_ip));
    
    // Reverse DNS lookup
    char resolved_host[256] = {0};
    getnameinfo((struct sockaddr*)addr, sizeof(*addr), resolved_host, sizeof(resolved_host),
                NULL, 0, 0);
    
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock < 0) {
        freeaddrinfo(res);
        JS_FreeCString(ctx, host);
        return JS_ThrowTypeError(ctx, "socket() failed");
    }
    
    struct timeval tv = {TIMEOUT_SEC, 0};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    char *result = malloc(8192);
    result[0] = '\0';
    
    // Header
    char header[256];
    if (resolved_host[0] && strcmp(resolved_host, dest_ip) != 0) {
        snprintf(header, sizeof(header), "Pinging %s [%s] with 32 bytes of data:\n\n",
                 resolved_host, dest_ip);
    } else {
        snprintf(header, sizeof(header), "Pinging %s with 32 bytes of data:\n\n", dest_ip);
    }
    strncat(result, header, 8192 - strlen(result) - 1);
    
    int transmitted = 0, received = 0;
    double times[PING_COUNT];
    memset(times, 0, sizeof(times));
    
    for (int seq = 0; seq < PING_COUNT; seq++) {
        struct icmp pkt;
        memset(&pkt, 0, sizeof(pkt));
        pkt.icmp_type = ICMP_ECHO;
        pkt.icmp_code = 0;
        pkt.icmp_id = getpid() & 0xFFFF;
        pkt.icmp_seq = seq;
        pkt.icmp_cksum = 0;
        pkt.icmp_cksum = icmp_checksum(&pkt, sizeof(pkt));
        
        double start = get_time_ms();
        if (sendto(sock, &pkt, sizeof(pkt), 0, (struct sockaddr*)addr, sizeof(*addr)) <= 0) {
            char line[128];
            snprintf(line, sizeof(line), "Send failed for icmp_seq=%d\n", seq + 1);
            strncat(result, line, 8192 - strlen(result) - 1);
            continue;
        }
        transmitted++;
        
        char buf[PACKET_SIZE + 64];
        struct sockaddr_in reply_addr;
        socklen_t addrlen = sizeof(reply_addr);
        int n = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&reply_addr, &addrlen);
        double end = get_time_ms();
        
        if (n > 0) {
            // Parse ICMP header (skip IP header)
            struct ip *ip_hdr = (struct ip*)buf;
            int ip_hdr_len = ip_hdr->ip_hl * 4;
            struct icmp *icmp_reply = (struct icmp*)(buf + ip_hdr_len);
            
            // Check if it's an echo reply
            if (icmp_reply->icmp_type == ICMP_ECHOREPLY && 
                icmp_reply->icmp_id == (getpid() & 0xFFFF)) {
                received++;
                double rtt = end - start;
                times[seq] = rtt;
                
                char reply_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &reply_addr.sin_addr, reply_ip, sizeof(reply_ip));
                
                char line[256];
                snprintf(line, sizeof(line), 
                        "Reply from %s: bytes=32 time=%.0fms TTL=%d\n",
                        reply_ip, rtt, ip_hdr->ip_ttl);
                strncat(result, line, 8192 - strlen(result) - 1);
            }
        } else {
            char line[128];
            snprintf(line, sizeof(line), "Request timed out.\n");
            strncat(result, line, 8192 - strlen(result) - 1);
        }
        
        if (seq < PING_COUNT - 1) {
            usleep(1000000); // 1 sec between pings
        }
    }
    
    close(sock);
    freeaddrinfo(res);
    JS_FreeCString(ctx, host);
    
    // Compute statistics
    double rtt_min = 1e9, rtt_max = 0, rtt_sum = 0;
    int count = 0;
    for (int i = 0; i < PING_COUNT; i++) {
        if (times[i] > 0) {
            if (times[i] < rtt_min) rtt_min = times[i];
            if (times[i] > rtt_max) rtt_max = times[i];
            rtt_sum += times[i];
            count++;
        }
    }
    
    char stats[512];
    if (received > 0) {
        double rtt_avg = rtt_sum / count;
        int loss_pct = ((transmitted - received) * 100) / transmitted;
        snprintf(stats, sizeof(stats),
                "\nPing statistics for %s:\n"
                "    Packets: Sent = %d, Received = %d, Lost = %d (%d%% loss),\n"
                "Approximate round trip times in milli-seconds:\n"
                "    Minimum = %.0fms, Maximum = %.0fms, Average = %.0fms\n",
                dest_ip, transmitted, received, transmitted - received, loss_pct,
                rtt_min, rtt_max, rtt_avg);
    } else {
        snprintf(stats, sizeof(stats),
                "\nPing statistics for %s:\n"
                "    Packets: Sent = %d, Received = %d, Lost = %d (100%% loss)\n",
                dest_ip, transmitted, received, transmitted);
    }
    strncat(result, stats, 8192 - strlen(result) - 1);
    
    JSValue ret = JS_NewString(ctx, result);
    free(result);
    return ret;
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

//net.route()
JSValue js_route(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    FILE *fp = fopen("/proc/net/route", "r");
    if (!fp) {
        return JS_ThrowInternalError(ctx, "cannot open /proc/net/route: %s",  strerror(errno));
    }
    
    char line[256];
    // Skip header line
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return JS_ThrowInternalError(ctx, "empty route table");
    }
    
    // Use dynamic buffer to avoid truncation
    size_t outsize = 8192;
    char *outbuf = malloc(outsize);
    if (!outbuf) {
        fclose(fp);
        return JS_ThrowOutOfMemory(ctx);
    }
    
    size_t outlen = 0;
    int n = snprintf(outbuf + outlen, outsize - outlen,
                     "Kernel IP routing table\n%-16s %-16s %-16s %-8s %-8s\n",
                     "Destination", "Gateway", "Mask", "Flags", "Iface");
    if (n < 0 || (size_t)n >= outsize - outlen) {
        free(outbuf);
        fclose(fp);
        return JS_ThrowInternalError(ctx, "buffer formatting error");
    }
    outlen += n;
    
    char iface[32];
    unsigned int dest, gateway, flags, mask;
    unsigned int refcnt, use, metric;  // Named instead of junk
    
    while (fgets(line, sizeof(line), fp)) {
        int parsed = sscanf(line, "%31s %X %X %X %u %u %u %X",
                           iface, &dest, &gateway, &flags,
                           &refcnt, &use, &metric, &mask);
        if (parsed < 8)
            continue;
        
        char deststr[64], gwstr[64], maskstr[64];
        format_ip(deststr, sizeof(deststr), dest);
        format_ip(gwstr, sizeof(gwstr), gateway);
        format_ip(maskstr, sizeof(maskstr), mask);
        
        // Check if we need more space (with safety margin)
        size_t needed = outlen + 256;
        if (needed > outsize) {
            size_t newsize = outsize * 2;
            char *newbuf = realloc(outbuf, newsize);
            if (!newbuf) {
                free(outbuf);
                fclose(fp);
                return JS_ThrowOutOfMemory(ctx);
            }
            outbuf = newbuf;
            outsize = newsize;
        }
        
        n = snprintf(outbuf + outlen, outsize - outlen,
                    "%-16s %-16s %-16s %-8X %-8s\n",
                    deststr, gwstr, maskstr, flags, iface);
        if (n < 0 || (size_t)n >= outsize - outlen) {
            break;  // Shouldn't happen with resize logic, but be safe
        }
        outlen += n;
    }
    
    fclose(fp);
    
    JSValue result = JS_NewStringLen(ctx, outbuf, outlen);
    free(outbuf);
    
    return result;
}

//net.ssh()
JSValue js_ssh(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1 || !JS_IsString(argv[0])) {
        return JS_ThrowTypeError(ctx, "Expected connection string");
    }

    const char *input_str = JS_ToCString(ctx, argv[0]);
    if (!input_str) {
        return JS_EXCEPTION;
    }

    struct ssh_param *params = ssh_param_parse(input_str);
    JS_FreeCString(ctx, input_str);

    if (!params) {
        ssh_param_free(params);
        return JS_ThrowTypeError(ctx, "Invalid SSH connection string format");
    }
    
    printf("Connecting to: %s@%s:%d\n", 
         params->user ? params->user : "(none)",
         params->host ? params->host : "(none)",
         params->port
        );
    
    int rc;
    char *password;

    ssh_session ssh_sesh = ssh_new();
    if (!ssh_sesh){
        ssh_param_free(params);
        return JS_ThrowInternalError(ctx, "SSH session could not be created");
    }
    
    if (params->host) ssh_options_set(ssh_sesh, SSH_OPTIONS_HOST, params->host);
    if (params->user) ssh_options_set(ssh_sesh, SSH_OPTIONS_USER, params->user);
    if (params->port) ssh_options_set(ssh_sesh, SSH_OPTIONS_PORT, &params->port);
    // ssh_options_set(ssh_sesh, SSH_OPTIONS_LOG_VERBOSITY, &params->verbosity);

    rc = ssh_connect(ssh_sesh);
    if (rc != SSH_OK){
        ssh_free(ssh_sesh);
        ssh_param_free(params);
        return JS_ThrowInternalError(ctx, "Error connecting to %s: %s\n",
            input_str,
            ssh_get_error(ssh_sesh)
        );
    }

    if (verify_knownhost(ssh_sesh) < 0){
        ssh_disconnect(ssh_sesh);
        ssh_free(ssh_sesh);    
        ssh_param_free(params);
        return JS_UNDEFINED;
    }

    printf("%s@%s's password: ", params->user, params->host);
    password = getpass("");
    rc = ssh_userauth_password(ssh_sesh, NULL, password);
    if (rc != SSH_AUTH_SUCCESS){
        ssh_disconnect(ssh_sesh);
        ssh_free(ssh_sesh);    
        ssh_param_free(params);
        return JS_ThrowInternalError(ctx, "Error authenticating with password: %s\n", ssh_get_error(ssh_sesh));
    }

    interactive_shell(ssh_sesh);

    ssh_disconnect(ssh_sesh);
    ssh_free(ssh_sesh);    
    ssh_finalize();
    ssh_param_free(params);
    return JS_UNDEFINED;
}