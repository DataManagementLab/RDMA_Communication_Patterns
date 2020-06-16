#pragma once

enum class RdmaTransfer { WRITE, SEND };
// RQ and SRQ are only usable when the sender selected SEND as transfer mode
enum class RdmaPoll { RQ, SRQ, BUF};

// Buffer in bytes 4GB
constexpr size_t RESP_BUFFER_BYTES = 4294967296;
