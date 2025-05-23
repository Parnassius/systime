/*
 * Copyright (c) 2024, Aditya
 *
 * https://codeberg.org/0ref/ntpc
 * See: LICENCE-ntpc
 *
 */

#include <netdb.h>
#include <strings.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <switch.h>

static const uint64_t FirstDayOfNTP = 2208988800ULL; // First day NTP (Era 0)

typedef struct {
    uint32_t sec, frac;
} Timestamp;

struct NTP_Packet {
    /*
     *		First Byte Is Called Header And Contains:
     *		LI   = Leap Indicator, 2 Bits
     *		VN   = Version Number, 3 Bits
     *		Mode =           Mode, 3 Bits
     */
    uint8_t  Header;
    uint8_t  Stratum;   // Precision (https://en.wikipedia.org/wiki/Network_Time_Protocol#Clock_strata)
    uint8_t  Poll;      // Max interval between successive messages
    uint8_t  Precision; // Precision of local clock

    uint32_t RootDelay;
    uint32_t RootDispersion;
    uint32_t ReferenceID;

    Timestamp t_origin;   // Time When Client Sent the Packet
    Timestamp t_recv;     // Time When Server Received the Packet
    Timestamp t_transmit; // Time When Server Replied
    Timestamp t_dest;     // Time When Client Received the reply
};

Result FetchNTP_Packet(struct NTP_Packet* pkt, const char* const server, const char* const port) {
    struct addrinfo hints;
    bzero(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;     // Allow IPV4 & IPV6
    hints.ai_socktype = SOCK_DGRAM;  // UDP Socket
    hints.ai_flags = 0;              // No Extra Flags
    hints.ai_protocol = IPPROTO_UDP; // Only Return UDP Sockets

    struct addrinfo* result = NULL;
    int AddrInfoResult = getaddrinfo(server, port, &hints, &result);
    if (AddrInfoResult != 0) {
        return 1;
    }

    /* getaddrinfo() returns a list of address structures.
     T ry each *address until we successfully connect(2).
     If socket(2) (or connect(2)) fails, we (close the socket)
     and try the next address. */

    struct addrinfo* rp = NULL;
    int SocketFD = -1;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        SocketFD = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (SocketFD == -1)
            continue;

        if (connect(SocketFD, rp->ai_addr, rp->ai_addrlen) != -1) break; // Success

        close(SocketFD);
    }

    if (rp == NULL) {
        freeaddrinfo(result);
        return 1;
    }
    freeaddrinfo(result);

    int NumBytesWritten = write(SocketFD, pkt, sizeof(*pkt));
    if (NumBytesWritten < 0) {
        close(SocketFD);
        return 1;
    } else if (NumBytesWritten != sizeof(*pkt)) {
        close(SocketFD);
        return 1;
    }

    int NumBytesRead = read(SocketFD, pkt, sizeof(*pkt));
    if (NumBytesRead < 0) {
        close(SocketFD);
        return 1;
    } else if (NumBytesRead != sizeof(*pkt)) {
        close(SocketFD);
        return 1;
    }

    pkt->t_origin.sec = htonl(pkt->t_origin.sec);
    pkt->t_origin.frac = htonl(pkt->t_origin.frac);

    pkt->t_recv.sec = htonl(pkt->t_recv.sec);
    pkt->t_recv.frac = htonl(pkt->t_recv.frac);

    pkt->t_transmit.sec = htonl(pkt->t_transmit.sec);
    pkt->t_transmit.frac = htonl(pkt->t_transmit.frac);

    pkt->t_dest.sec = htonl(pkt->t_dest.sec);
    pkt->t_dest.frac = htonl(pkt->t_dest.frac);

    close(SocketFD);
    return 0;
}

Result getNtpTime(uint64_t *timestamp) {
    Result rc;

    const char* server = "pool.ntp.org";
    const char* port = "123";

    struct NTP_Packet pkt;
    bzero(&pkt, sizeof(pkt));

    /*
     *		LI     = 0 (00)  Corresponds To No Warning Of An Impending Leap Second To Be Inserted/Deleted
     *		VN     = 4 (010) Corresponds To NTP v4
     *		Mode   = 3 (011) Corresponds To Client Mode
     *		Header = 0x13 (00 010 011)
     */
    pkt.Header = 0x13;

    rc = FetchNTP_Packet(&pkt, server, port);
    if (R_SUCCEEDED(rc)) {
        *timestamp = pkt.t_transmit.sec - FirstDayOfNTP;
    }

    return rc;
}
