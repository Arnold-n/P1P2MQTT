#include "P1P2Processor.h"
#include "P1P2_CompatAPI.h"

namespace P1P2Processor
{

void process(const P1P2Parser::Packet& packet)
{
    P1P2Compat_process(packet);
}

} // namespace P1P2Processor
