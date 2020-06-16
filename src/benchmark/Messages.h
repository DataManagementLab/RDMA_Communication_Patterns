#pragma once

enum class MessageType: uint8_t {EMPTY = 0, ExpMsg = 1, InitExp = 2, FinishedExp = 3};

struct MessageBase{
   volatile bool access;
   volatile size_t msgId = 0; // in init phase gets set to the beginning range
   volatile size_t id = 0; //    in init phase gets set to the end of the request range
   volatile size_t destinationNodeID = 0;
   volatile size_t RDMAptr = 0;
   volatile size_t padding_destNodeID = 0;
   volatile size_t padding_destRDMA = 0;
   volatile MessageType type = MessageType::EMPTY; 
   // important to poll on last otherwise the cacheline could be loaded with partial msg

   MessageBase() = default;
  MessageBase(MessageType type) : type(type){};
  
};


static_assert(sizeof(MessageBase) == 64, "Cache Line Sized");

struct ExperimentMessage : public MessageBase {
ExperimentMessage() : MessageBase(MessageType::ExpMsg) {}
};


struct InitExperimentMsg : public MessageBase {
  
InitExperimentMsg(): MessageBase(MessageType::InitExp) {}   
};

   
struct FinishedExperimentMsg : public MessageBase {
  
FinishedExperimentMsg() : MessageBase(MessageType::FinishedExp) {} 
};


template<size_t PL_SIZE>
struct Response {
   size_t senderNodeId = 0;
   size_t responseId = 0;
   char payload [PL_SIZE - (sizeof(size_t) * 3)];
   volatile size_t transferFlag = 0;
};
