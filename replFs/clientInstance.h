/*
 * clientInstance.h
 *
 *  Created on: May 17, 2014
 *      Author: songhan
 */

#ifndef CLIENTINSTANCE_H_
#define CLIENTINSTANCE_H_
#include "networkInstance.h"
#include <map>
#include <set>
#include <string>

class ClientInstance {
 private:
  int fd;
  std::string fileName;
  uint32_t numServers;
  uint32_t GUID;
  uint32_t transNum;
  uint32_t blockID;
  int numPendingBlocks;
  WriteBlockPkt* pendingBlocks[MAX_PENDING + 1];  //mapping from block id to packet that contains the payload
  Network *N;
  bool isOpened;

 public:
  ClientInstance();
  ClientInstance(unsigned short port, int dropRate, int numServers);
  virtual ~ClientInstance();
  int OpenFile(char* fileName);
  int WriteBlock(int fd, char * strData, int byteOffset, int blockSize);
  int CommitVoting(int fd);
  int CommitFinal(int fd);
  int Abort(int fd);
  int CloseFile(int fd);
 private:
  bool isTimeOut(timeval oldTime, long timeOut);
  void cleanup();
};
#endif /* CLIENTINSTANCE_H_ */

