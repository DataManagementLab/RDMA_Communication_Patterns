

#ifndef MESSAGEERRORS_H_
#define MESSAGEERRORS_H_

#include "../utils/Config.h"

namespace rdma {

enum MessageErrors
  : unsigned int {

  NO_ERROR = 0,
  INVALID_MESSAGE,  // make use of auto-increment

  MEMORY_NOT_AVAILABLE = 100,
  MEMORY_RELEASE_FAILED,

  NODEID_NOT_FOUND = 200,
};

}

#endif /* MESSAGEERRORS_H_ */
