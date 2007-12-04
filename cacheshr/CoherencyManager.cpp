#include "CoherencyManager.h"
#include "ChannelListener.h"
#include "ChannelFactory.h"

#include <stdio.h>
#include <stdlib.h>
//#define DEBUG

#ifdef HAVE_VXWORKS_H
#include <wdLib.h>
#include <semLib.h>
#include <sysLib.h>
#endif

CoherencyManager::CoherencyManager(SharedDataManager *dataManager)
{
	this->dataManager = dataManager;
	channel = chanFactory.getChannel();
}

/* COHERENCY BEHAVIOUR
A single process on every machine hosting a cache serve cache coherency messages as follows:


1) Request for data: this is generated by a cache not owner and not warm willing to access the data. 
		The answer is sent only if the cache is owner The WHOLE data for the node is returned. 
		If not already in the associated reader node list, the requesting 
		node is added to the list. 

		Message format: int nid, int treeIdx, 
		Answer format: if owner, int nid, serialized version of associated MemNodeData instance

2) Ownership changed. Compare the current ownership timestamp with the passed timestamp: if the new timestamp
		is greater, or if it is equal and the idx of the new owner is greater that the current owner idx, 
		update ownership information, and set dirty flag for that nid. 
		If the cache is warm for that nid, send an ownership Warm message.

		Message format: int nid; int treeIdx, int timestamp, char ownerIdx 

3) Ownership Warm: returned by nodes which are not owner in response to a owneriship message
		If the receiver is owner, add nid to warm list, and send data

4) Dirty message: generated by the owner to indicate that data has been updated. It is sent to all
		those nodes which have already read data from this owner (and which are not warm). 
	 
5) Data message: this message is originated by the current owner and the node is warm in this cache. 
		It contains the serialized version of the datum
		Message Format: int nid; Serialized content of associated MemNodeData. 


In every process making data access:


When data is read, checkRead needs to be called first. checkRead does nothing if there is no owner, or the 
cache is the current owner or if the node is warm in this cache or the associated dirty flas is reset.
Otherwise, the WHOLE data set is requested to the current owner.

When data is (over)written, checkWrite needs to be called afterwards. If the cache is the current owner, 
a list of nodes is maintained (both warm and not). For all the warm nodes, a data message is sent. 
For all the not warm nodes in the list, a dirty message is generated.
If the cache is not the current owner, an ownership message is broadcasted
with an incremented timestamp and the cache becomes the current owner.

*/


void CoherencyManager::handleMessage(ChannelAddress *senderAddr, int senderIdx, char *buf, int bufLen, char type)
//Handle messages from other caches in the system. This is run by a single process on every machine.
{

	switch(type)
	{
		case REQUEST_DATA_TYPE:
		{
			int nid = channel->toNative(*(unsigned int *)buf);
			int treeIdx = channel->toNative(((unsigned int *)buf)[sizeof(int)]);
			handleRequestDataMsg(treeIdx, nid, senderAddr, senderIdx);
			break;
		}
		case OWNERSHIP_TYPE:
		{
			int nid = channel->toNative(*(unsigned int *)buf);
			int treeIdx = channel->toNative(((unsigned int *)buf)[sizeof(int)]);
			int timestamp = channel->toNative(*(unsigned int *)(&buf[2*sizeof(int)]));
			char ownerIdx = buf[3 * sizeof(int)];
			handleOwnershipMsg(treeIdx, nid, timestamp, ownerIdx, senderAddr, senderIdx);
			break;
		}
		case OWNERSHIP_WARM_ACK_TYPE:
		{
			int nid = channel->toNative(*(unsigned int *)buf);
			int treeIdx = channel->toNative(((unsigned int *)buf)[sizeof(int)]);
			handleOwnershipWarmMessage(treeIdx, nid, senderAddr, senderIdx);
			break;
		}

		case DATA_TYPE:
		{
			int nid = channel->toNative(*(unsigned int *)buf);
			int treeIdx = channel->toNative(((unsigned int *)buf)[sizeof(int)]);
			handleDataMsg(treeIdx, nid, &buf[2*sizeof(int)], bufLen - 2*sizeof(int), senderAddr, senderIdx);
			break;
		}
		case DIRTY_TYPE:
		{
			int nid = channel->toNative(*(unsigned int *)buf);
			int treeIdx = channel->toNative(((unsigned int *)buf)[sizeof(int)]);
			handleDirtyMsg(treeIdx, nid, senderAddr, senderIdx);
			break;
		}
		default: printf("Unsupported message type received: %d\n", type);
	}
}


//Manages request for data: read the whole data slot set  and send it. 
//The returned message contains the data slots.
void CoherencyManager::handleRequestDataMsg(int treeIdx, int nid, ChannelAddress *senderAddr, int senderIdx)
{

	int serializedSize;
	
#ifdef DEBUG
	printf("DATA REQUEST message. nid: %d\n", nid);
#endif
	serializedSize = dataManager->getSerializedSize(treeIdx, nid);
	char *serialized = new char[2*sizeof(int)+serializedSize];
	*(int *)serialized = channel->fromNative(nid);
	((int *)serialized)[1] = channel->fromNative(treeIdx);
	dataManager->getSerialized(treeIdx, nid, &serialized[2*sizeof(int)]);
	dataManager->addReader(treeIdx, nid, senderIdx);
	ChannelAddress *retAddr = chanFactory.getAddress(senderIdx);

	channel->sendMessage(retAddr, serialized, serializedSize + 2 * sizeof(int), DATA_TYPE);
	delete [] serialized;
}


void CoherencyManager::handleOwnershipMsg(int treeIdx, int nid, int timestamp, char ownerIdx, 
										  ChannelAddress *addr, int senderIdx)
{

	int prevOwnerIdx;
	bool isOwner;
	bool isWarm;
	bool isDirty;
	int prevTimestamp;
	dataManager->getCoherencyInfo(treeIdx, nid, isOwner, prevOwnerIdx, isWarm, isDirty, prevTimestamp);

#ifdef DEBUG
	printf("OWNERSHIP message. nid: %d, timestamp: %d ownerIdx: %d\n", nid, timestamp, ownerIdx);
#endif

/*  TEMPORANEO
	if((timestamp < prevTimestamp )|| //It is an outdated message
		((timestamp == prevTimestamp) && ownerIdx < prevOwnerIdx)) //It is a concurrent write, but the sender has lowe priority

	{
#ifdef DEBUG
	printf("Outdated ownership message\n");
#endif
		return;
	}

*/		
	dataManager->setOwner(treeIdx, nid, ownerIdx, timestamp);


	if(isWarm)
	{
		int msgNid = channel->fromNative(nid);
		int msgTreeIdx = channel->fromNative(treeIdx);
		int msgInfo[2];
		msgInfo[0] = nid;
		msgInfo[1] = treeIdx;
		ChannelAddress *retAddr = chanFactory.getAddress(senderIdx);
		channel->sendMessage(retAddr, (char *)msgInfo, 2*sizeof(int), OWNERSHIP_WARM_ACK_TYPE);
	}
	else
		dataManager->setDirty(treeIdx, nid, true);
}


void CoherencyManager::handleDataMsg(int treeIdx, int nid, char *serializedData, int dataLen, 
									 ChannelAddress *senderAddr, int senderIdx)
{
#ifdef DEBUG
	printf("DATA message. nid: %d dataLen: %d\n", nid, dataLen);
#endif

	dataManager->setSerializedData(treeIdx, nid, serializedData, dataLen);
	Event *dataEvent = dataManager->getDataEvent(treeIdx, nid);
	if(dataEvent)
	{
		dataEvent->signal();
	}
	dataManager->setDirty(treeIdx, nid, false);
}


void CoherencyManager::handleDirtyMsg(int treeIdx, int nid, ChannelAddress *senderAddr, int senderIdx)
{
#ifdef DEBUG
	printf("DIRTY message. nid: %d \n", nid);
#endif
	dataManager->setDirty(treeIdx, nid, true);
}

void CoherencyManager::handleOwnershipWarmMessage(int treeIdx, int nid, ChannelAddress *senderAddr, int senderIdx)
{
	int ownerIdx;
	bool isOwner;
	bool isWarm;
	bool isDirty;
	int timestamp;

#ifdef DEBUG
	printf("WARM OWNERSHIP message. nid: %d\n", nid);
#endif

	dataManager->getCoherencyInfo(treeIdx, nid, isOwner, ownerIdx, isWarm, isDirty, timestamp);
	if(!isOwner) return;
	dataManager->addWarm(treeIdx, nid, senderIdx);
	int serializedSize = dataManager->getSerializedSize(treeIdx, nid);
	char *serialized = new char[2*sizeof(int)+serializedSize];
	*(int *)serialized = channel->fromNative(nid);
	((int *)serialized)[1] = channel->fromNative(treeIdx);
	dataManager->getSerialized(treeIdx, nid, &serialized[2*sizeof(int)]);
	ChannelAddress *retAddr = chanFactory.getAddress(senderIdx);
	channel->sendMessage(retAddr, serialized, serializedSize+2*sizeof(int), DATA_TYPE);
	delete[] serialized;
}


//Make sure data is up-to-date with latest cache version before reading
void CoherencyManager::checkRead(int treeIdx, int nid)
{
	int ownerIdx;
	bool isOwner;
	bool isWarm;
	bool isDirty;
	int timestamp;
	int sendNid, sendTreeIdx;

	if(!chanFactory.isCommunicationEnabled()) return;
	dataManager->getCoherencyInfo(treeIdx, nid, isOwner, ownerIdx, isWarm, isDirty, timestamp);
	if(isOwner || ownerIdx == -1 || isWarm || !isDirty) 
		return;

	ChannelAddress *addr = chanFactory.getAddress(ownerIdx);
	//Request whole data set
	sendNid = channel->fromNative(nid);
	sendTreeIdx = channel->fromNative(treeIdx);
	int sendInfo[2];
	sendInfo[0] = sendNid;
	sendInfo[1] = sendTreeIdx;
	Event *dataEvent = dataManager->getDataEvent(treeIdx, nid);
	channel->sendMessage(addr, (char *)sendInfo, 2*sizeof(int), REQUEST_DATA_TYPE);
	dataEvent->wait();

}

void CoherencyManager::checkWrite(int treeIdx, int nid)
{
	int ownerIdx;
	bool isOwner;
	bool isWarm;
	int timestamp;

	char *warmList;
	int numWarm;
	char *readerList;
	int numReader;

	if(!chanFactory.isCommunicationEnabled()) return;

	dataManager->getCoherencyInfo(treeIdx, nid, isOwner, ownerIdx, isWarm, timestamp, 
		warmList, numWarm, readerList, numReader);
	if(!isOwner) //It was not owner, update all nodes, possibly sending the whole data item (for warm nodes)nodes
	{
		int numAddresses;
		timestamp++;
		ChannelAddress **addresses = chanFactory.getOtherAddresses(numAddresses);
		char outBuf[3*sizeof(int)+1];
		*(unsigned int *)outBuf = channel->fromNative(nid);
		((unsigned int *)outBuf)[1] = channel->fromNative(treeIdx);
		((unsigned int *)outBuf)[2] = channel->fromNative(timestamp);
		outBuf[3 * sizeof(int)] = chanFactory.getThisAddressIdx();

		dataManager->setCoherencyInfo(treeIdx, nid, true, -1, isWarm, timestamp, NULL, 0, NULL, 0);
		for(int i = 0; i < numAddresses; i++)
		{
			channel->sendMessage(addresses[i], outBuf, 3 * sizeof(int)+1, OWNERSHIP_TYPE);
		}
	}
		
	else if(numWarm > 0 || numReader > 0) //It is owner, send last data slot to all warm nodes and dirty message to all current readers
	{

		if(numWarm > 0)
		{
			int serializedSize = dataManager->getSerializedSize(treeIdx, nid);
			char *serialized = new char[serializedSize+2*sizeof(int)];
			*(int *)serialized = channel->fromNative(nid);
			((int *)serialized)[1] = channel->fromNative(treeIdx);
			dataManager->getSerialized(treeIdx, nid, &serialized[2*sizeof(int)]);
			
			for(int i = 0; i < numWarm; i++)
			{
				ChannelAddress *currAddr = chanFactory.getAddress(warmList[i]);
				channel->sendMessage(currAddr, serialized, serializedSize+2*sizeof(int), DATA_TYPE);
			}
		}
		for(int i = 0; i < numReader; i++)
		{
			ChannelAddress *currAddr = chanFactory.getAddress(readerList[i]);
			int msgNid = channel->fromNative(nid);
			int msgTreeIdx = channel->fromNative(treeIdx);
			int msgInfo[2];
			msgInfo[0] = msgNid;
			msgInfo[1] = msgTreeIdx;
			channel->sendMessage(currAddr, (char *)msgInfo, 2*sizeof(int), DIRTY_TYPE);
		}
	}
}


void CoherencyManager::startServer()
{
	if(!channel)return;
	channel->attachListener(this, REQUEST_DATA_TYPE);
	channel->attachListener(this, OWNERSHIP_TYPE);
	channel->attachListener(this, DATA_TYPE);
	channel->attachListener(this, DIRTY_TYPE);
	channel->attachListener(this, OWNERSHIP_WARM_ACK_TYPE);
	channel->connectReceiver(chanFactory.getThisAddress());
}


			


