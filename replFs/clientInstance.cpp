/*
 * ClientInstance.cpp
 *
 *  Created on: May 17, 2014
 *      Author: songhan
 */

#include "clientInstance.h"
#include <assert.h>
ClientInstance::ClientInstance() {

}

ClientInstance::ClientInstance(unsigned short port, int dropRate,
                               int numServers) {
  this->fileName = "";
  this->numServers = numServers;
  this->fd = 0;
  this->isOpened = false;
  srand(time(NULL));
  this->GUID = rand();
  this->blockID = 0;
  this->transNum = 0;
  N = new Network(GROUP, port, dropRate, CLIENT);
  for (int i = 0; i < MAX_PENDING + 1; i++) {
    pendingBlocks[i] = NULL;
  }
  INFO("init clientIntance done\n");
}

ClientInstance::~ClientInstance() {
  delete N;
  for (int i = 0; i < MAX_PENDING + 1; i++) {
    if (pendingBlocks[i] != NULL)
      delete pendingBlocks[i];
  }
}

int ClientInstance::OpenFile(char* fileName) {
  if (isOpened || fileName == NULL || strlen(fileName) > MAX_FILE_NAME)
    return -1;

  this->fd++;
  this->transNum = 1;

  DBG("\n^^^^^^^^^^^^^^^^^^^^ Client Open File Called!  ^^^^^^^^^^^^^^^^^^^^^\n");
  DBG("the newly opened fd is %d\n", fd);
  OpenPkt *p = new OpenPkt(GUID, fd, 0, transNum, fileName);
  N->send(p);
  DBG("send a file open packet!");

  struct timeval beginTime;
  struct timeval againTime;
  gettimeofday(&beginTime, NULL);
  gettimeofday(&againTime, NULL);
  std::set<uint32_t> setResponses;

  while (isTimeOut(beginTime, COLLECT_RESPONSE_TIME_OUT) == false) {
    if (isTimeOut(againTime, RESEND_INTERVAL)) {
      N->send(p);  //send again
      gettimeofday(&againTime, NULL);
      DBG("sending open again!!\n");
    }
    PacketBase *pr = N->receive();
    if (pr == NULL)
      continue;
    if (pr->type != OPEN_ACK) {
      delete pr;
      continue;
    }
    OpenAckPkt* po = (OpenAckPkt*) pr;
    if (po->status == true) {
      setResponses.insert(po->GUID);
      DBG("\n :):):):):):):)  SERVER %x ready for open\n", pr->GUID);
      if (setResponses.size() >= numServers) {
        isOpened = true;
        delete pr;
        delete p;
        return fd;
      }
    }
    delete pr;
  }
  delete p;
  return -1;  //timed out
}

int ClientInstance::WriteBlock(int fd_, char* strData, int byteOffset,
                               int blockSize) {
  if (!isOpened || (fd != fd_)
      || byteOffset
          < 0|| byteOffset + blockSize> MAX_FILE_SIZE || byteOffset<0 || strData ==NULL || blockSize<0 || blockSize > MAX_PAY_LOAD)
    return -1;

  DBG("\n^^^^^^^^^^^^^^^^^^^^ Client Write Block Called!  ^^^^^^^^^^^^^^^^^^^^^\n");

  WriteBlockPkt *p = new WriteBlockPkt(GUID, fd, 0, transNum, blockID,
                                       byteOffset, blockSize,
                                       (uint8_t*) strData);
  DBG("writing, transNum = %d, blockId = %d\n", transNum, blockID);
  N->send(p);
  if (blockID >= MAX_PENDING) {
    ERROR("overflow!!!!!!!!!!!!!!!!!!!");
    return -1;
  }
  if (pendingBlocks[blockID] != NULL) {
    delete pendingBlocks[blockID];
  }
  this->pendingBlocks[blockID] = p;
  this->blockID++;
  return 0;
}

int ClientInstance::CommitVoting(int fd_) {
  if (fd != fd_ || isOpened == false) {
    ERROR("not opened yet! \n");
    return -1;
  }
  if (blockID == 0) {
    DBG("nothing to commit~~~~~~~~~~~~~~\n");
    return 1;
  }

  CommitVotingPkt *p = new CommitVotingPkt(GUID, fd, 0, transNum, blockID);
  DBG("\n^^^^^^^^^^^^^^^^^^^^ Client Commit Voting Called!  ^^^^^^^^^^^^^^^^^^^^^\n");
  N->send(p);

  struct timeval beginTime;
  struct timeval againTime;
  gettimeofday(&beginTime, NULL);
  gettimeofday(&againTime, NULL);
  std::set<uint32_t> setResponses;

  while (isTimeOut(beginTime, COLLECT_RESPONSE_TIME_OUT) == false) {
    if (isTimeOut(againTime, RESEND_INTERVAL)) {
      N->send(p);  //send again
      gettimeofday(&againTime, NULL);
      DBG("sending Voting again!!");
    }

    PacketBase *pr = N->receive();
    if (pr == NULL)
      continue;
    if (pr->type == COMMIT_VOTING_SUCCESS) {
      setResponses.insert(pr->GUID);
      DBG("\n :):):):):):):)  SERVER %x ready \n", pr->GUID);
      if (setResponses.size() >= numServers) {
        delete p;
        delete pr;
        return 0;
      }
      delete pr;
    } else if (pr->type == COMMIT_VOTING_RESEND) {
      DBG("received a resend\n");
      pr->printPacket();
      CommitVotingResendPkt * pc = (CommitVotingResendPkt *) pr;
      for (int i = 0; i < pc->totalMissing; i++) {
        int missingID = pc->vectorMissingID[i];
        N->send(pendingBlocks[missingID]);
      }
      gettimeofday(&beginTime, NULL);
      delete pr;
    } else {
      // other useless packets.
      delete pr;
    }
  }
  DBG("\n====commit voting timed out!!\n");
  return -1;  //timed out
}

int ClientInstance::CommitFinal(int fd_) {
  if (fd != fd_ || isOpened == false)
    return -1;
  DBG("\n^^^^^^^^^^^^^^^^^^^^ Client Commit Final Called!  ^^^^^^^^^^^^^^^^^^^^^\n");
  CommitFinalPkt *p = new CommitFinalPkt(GUID, fd, 0, transNum);
  N->send(p);
  struct timeval beginTime;
  struct timeval againTime;
  gettimeofday(&beginTime, NULL);
  gettimeofday(&againTime, NULL);
  std::set<uint32_t> setResponses;

  while (isTimeOut(beginTime, COLLECT_RESPONSE_TIME_OUT) == false) {
    if (isTimeOut(againTime, RESEND_INTERVAL)) {
      N->send(p);  //send again
      gettimeofday(&againTime, NULL);
      DBG("sending commit final again!!");
    }

    PacketBase *pr = N->receive();
    if (pr == NULL)
      continue;
    if (pr->type != COMMIT_FINAL_REPLY) {
      delete pr;
      continue;
    }
    CommitFinalReplyPkt* pc = (CommitFinalReplyPkt*) pr;
    if (pc->status == true) {
      DBG("\n :):):):):):):)  SERVER %x commit final success \n", pr->GUID);
      setResponses.insert(pc->GUID);
      if (setResponses.size() >= numServers) {
        cleanup();
        delete pr;
        delete p;
        return 0;
      }
    }
    delete pr;
  }
  DBG("\n====commit final timed out!!\n");
  cleanup();
  delete p;
  return -1;  //timed out

}

//AbortPkt::AbortPkt(uint32_t GUID, int fd, uint32_t seqNum, uint32_t transNum)
int ClientInstance::Abort(int fd_) {
  if (fd != fd_ || isOpened == false)
    return -1;
  DBG("\n^^^^^^^^^^^^^^^^^^^^ Client Abort Called!  ^^^^^^^^^^^^^^^^^^^^^\n");
  AbortPkt *p = new AbortPkt(GUID, fd, 0, transNum);
  for (int i = 1; i < 4; i++)
    N->send(p);
  cleanup();
  delete p;
  return 0;

}

int ClientInstance::CloseFile(int fd_) {
  if (fd != fd_ || isOpened == false)
    return -1;
  DBG("^^^^^^^^^^^^^^^^^^^^^^ Client Close File Called ^^^^^^^^^^^^^^^^^^^^^^\n");

  ClosePkt *p = new ClosePkt(GUID, fd, 0, transNum);
  N->send(p);
  struct timeval beginTime;
  struct timeval againTime;
  gettimeofday(&beginTime, NULL);
  gettimeofday(&againTime, NULL);
  std::set<uint32_t> setResponses;

  while (isTimeOut(beginTime, COLLECT_RESPONSE_TIME_OUT) == false) {
    if (isTimeOut(againTime, RESEND_INTERVAL)) {
      N->send(p);  //send again
      gettimeofday(&againTime, NULL);
      DBG("sending close file packet again!!\n");
    }

    PacketBase *pr = N->receive();
    if (pr == NULL)
      continue;
    if (pr->type != CLOSE_REPLY) {
      delete pr;
      continue;
    }
    CloseReplyPkt* pc = (CloseReplyPkt*) pr;
    if (pc->status == true) {
      DBG("\n :):):):):):):)  SERVER %x file closed success \n", pr->GUID);
      setResponses.insert(pc->GUID);
      if (setResponses.size() >= numServers) {
        DBG("\n :):):):):):):)  all closed success\n");
        cleanup();
        transNum = 0;
        blockID = 0;
        numPendingBlocks = 0;
        isOpened = false;
        delete p;
        delete pr;
        return 0;
      }
    }
    delete pr;
  }
  DBG("\n====file close timed out!!\n");
  cleanup();
  transNum = 0;
  blockID = 0;
  numPendingBlocks = 0;
  isOpened = false;
  fileName = "";
  delete p;
  return -1;  //timed out
}

bool ClientInstance::isTimeOut(timeval oldTime, long timeOut) {
  struct timeval curTime;
  gettimeofday(&curTime, NULL);
  if ((curTime.tv_sec - oldTime.tv_sec) * 1000
      + (curTime.tv_usec - oldTime.tv_usec) / 1000 > timeOut)
    return true;
  else
    return false;
}

void ClientInstance::cleanup() {
  for (uint i = 0; i <= blockID; i++) {
    if (pendingBlocks[i]) {
      delete pendingBlocks[i];
      pendingBlocks[i] = NULL;
    }
  }
  blockID = 0;
  transNum++;
}
