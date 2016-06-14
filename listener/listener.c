#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <pthread.h>

#include "copilot.h"
#include "dai_mon_data.h"

#define PORT    10873   // The port on which to listen for incoming data.

void fail(char *error) {
      perror(error);
      exit (1);
}

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
struct dai_mon_vehicle_data data;
bool fresh = false;

void* udp_listener(void* args) {
      struct dai_mon_msg rcvd;

      int msg_length = sizeof(rcvd);
      int data_length = sizeof(data);

      // Create a UDP socket.
      int self_socket;
      if ((self_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
            fail("socket");

      struct sockaddr_in self_address = {0};
      self_address.sin_family = AF_INET;
      self_address.sin_port = htons(PORT);
      self_address.sin_addr.s_addr = htonl(INADDR_ANY);

      // Bind socket to port.
      if (bind(self_socket, (struct sockaddr*) &self_address, sizeof(self_address)) == -1)
            fail("bind");

      // Keep listening for data.
      for (;;) {
            printf("[UDP THREAD] waiting for data...\n");

            // Try to receive some data, this is a blocking call.
            struct sockaddr_in other_address;
            socklen_t address_length = sizeof(other_address);
            ssize_t actual_length;
            if ((actual_length = recvfrom(self_socket, &rcvd, sizeof(rcvd), 0, (struct sockaddr *) &other_address, &address_length)) == -1)
                  fail("recvfrom()");

            // TODO(chathhorn)
            if (actual_length < sizeof(rcvd))
                  fail("received incomplete message, so we're failing instead of waiting for the rest.");

            // Print details of the client/peer and the data received.
            printf("[UDP THREAD] received packet from: %s:%uh\n", inet_ntoa(other_address.sin_addr), ntohs(other_address.sin_port));

            pthread_mutex_lock(&mutex);
            memcpy(&data, &rcvd.data, data_length);
            fresh = true;
            pthread_mutex_unlock(&mutex);
      }

      close(self_socket);
      return ((void*) 0);
}

double altI; double altO;
double gsI;  double gsO;
double latI; double latO;
double lonI; double lonO;
double trkI; double trkO;
double vsI;  double vsO;

double dthr;
double tthr;
double tcoathr;
double zthr;

int main(void) {
      struct dai_mon_vehicle_data local;
      int data_length = sizeof(data);

      pthread_t listener_tid;

      if (pthread_create(&listener_tid, NULL, udp_listener, NULL) != 0)
            fail("pthread_create()");

      for (;;) {
            if (fresh) {
                  pthread_mutex_lock(&mutex);
                  memcpy(&local, &data, data_length);
                  fresh = false;
                  pthread_mutex_unlock(&mutex);

                  printf("--------------------\n");
                  printf("vehicle_number: %s\n"
                         "latitude: %f\n"
                         "longitude: %f\n"
                         "altitude: %f\n"
                         "ground_speed: %f\n"
                         "ground_track: %f\n"
                         "vertical_speed: %f\n"
                         "time_ms: %ud\n",
                         local.vehicle_number,
                         local.latitude,
                         local.longitude,
                         local.altitude,
                         local.ground_speed,
                         local.ground_track,
                         local.vertical_speed,
                         local.time_ms);

                  latO = local.latitude;
                  lonO = local.longitude;
                  altO = local.altitude;
                  gsO  = local.ground_speed;
                  trkO = local.ground_track;
                  vsO  = local.vertical_speed;
                  step();
            }
      }

      pthread_join(listener_tid, NULL);
      return 0;
}

void alert_wcv(void) { puts("ALERT: WCV VIOLATION!"); }
