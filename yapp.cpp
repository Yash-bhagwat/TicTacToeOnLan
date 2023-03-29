// All the includes statements
#include <iostream>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
using namespace std;

struct ping_packet{
	// Adding a payload to the header to make a ping packet with a conventional 64 bit size
	struct icmphdr hdr;
	char data[64 - sizeof(struct icmphdr)];
};

void pinger(int sockfd, struct sockaddr_in *addr, string ip){
	// Creating the ping packet, filling up the ping packet with the ICMP
	struct ping_packet packet;
	bzero(&packet, sizeof(packet));

	// Constructing the ping packet header
    packet.hdr.type = ICMP_ECHO;
    packet.hdr.un.echo.id = getpid();

	// Setting up timeout at 5 seconds and timespecs to get RTT
	struct timeval timeouts;
    timeouts.tv_sec = 5;
    timeouts.tv_usec = 0;

	struct timespec start_time, end_time;

    // Using setsockopt to control socket behaviour (setting timeouts etc.)
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeouts, sizeof(timeouts));

    // Sending Packet and getting Start time for RTT
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    if (sendto(sockfd, &packet, sizeof(packet), 0, (struct sockaddr*) addr, sizeof(*addr)) <= 0){
        cout << "Packet Sending failed\n"; return;
    }

    // Receive Packet and getting End time for RTT
    struct sockaddr_in receipt;
    unsigned int receipt_length = sizeof(receipt);
    if (recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr*)&receipt, &receipt_length) <= 0){
        cout << "Request timed out or host unreacheable\n"; return;
    }
    clock_gettime(CLOCK_MONOTONIC, &end_time);

    // Calculating the RTT time by calculating difference between end time and start time
    double rtt = ((double)(end_time.tv_nsec - start_time.tv_nsec))/1000000.0;

    cout << "Reply from " << ip << ". RTT = " << rtt << " milliseconds\n";
}


// Main function
int main(int argc, char** argv){
	// The IP we want to ping
	char* ip_as_char = argv[1]; 
	string ip = (string) argv[1];

	// Validate if we have a good hostname using inet_aton
	struct in_addr validator; 
	if(inet_aton(ip_as_char, &validator) == 0){
		cout << "Bad hostname\n";
		return 0;
	}

	// Creating a socket 
	int sockfd;
	struct sockaddr_in addr;   
	int addr_len = sizeof(addr);
	addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip_as_char);
    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
	if(sockfd < 0){
		cout << "Socket creation error\n";
		return 0;
	}

	// Calling the pinging function which fills the ping packet and then sends and receives it 
	pinger(sockfd, &addr, ip);
	return 0;
}