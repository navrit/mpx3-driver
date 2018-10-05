#ifndef PACKETCONTAINER_H
#define PACKETCONTAINER_H

typedef struct {
  int chipIndex;
  long size;
  char data[9000];
} PacketContainer;

#endif // PACKETCONTAINER_H
