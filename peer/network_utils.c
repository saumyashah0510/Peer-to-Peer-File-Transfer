#include <stdio.h>
#include <string.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// Get the real IP address (not 127.0.0.1)
int get_my_ip(char *ip_buffer) {
    struct ifaddrs *ifaddr, *ifa;
    
    if (getifaddrs(&ifaddr) == -1) {
        return -1;
    }
    
    // Loop through network interfaces
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        
        // Check if it's IPv4
        if (ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
            char *ip = inet_ntoa(addr->sin_addr);
            
            // Skip localhost (127.0.0.1)
            if (strcmp(ip, "127.0.0.1") == 0) continue;
            
            // Skip docker/virtual interfaces
            if (strncmp(ifa->ifa_name, "docker", 6) == 0) continue;
            if (strncmp(ifa->ifa_name, "veth", 4) == 0) continue;
            
            // Found a real IP!
            strcpy(ip_buffer, ip);
            freeifaddrs(ifaddr);
            return 0;
        }
    }
    
    freeifaddrs(ifaddr);
    
    // Fallback to localhost if no real IP found
    strcpy(ip_buffer, "127.0.0.1");
    return -1;
}