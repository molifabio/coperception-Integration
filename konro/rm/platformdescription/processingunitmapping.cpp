#include "processingunitmapping.h"

using namespace std;

ProcessingUnitMapping::ProcessingUnitMapping(int osPuIdx) :
    osPuIdx_(osPuIdx),
    osCoreIdx_(-1),
    osCpuIdx_(-1)
{
    for (int i = 0; i < NUM_CACHES; ++i) {
        osCacheIdx[i] = hwlocCacheIdx[i] = -1;
    }
}

void ProcessingUnitMapping::printOnOstream(std::ostream &os) const
{
    os << "{"
       << "\"idx\":" << osPuIdx_
       << ",\"core\":" << osCoreIdx_
       << ",\"L1cache\":" << '[' << osCacheIdx[0] << ',' << hwlocCacheIdx[0] << ']'
       << ",\"L2cache\":" << '[' << osCacheIdx[1] << ',' << hwlocCacheIdx[1] << ']'
       << ",\"L3cache\":" << '[' << osCacheIdx[2] << ',' << hwlocCacheIdx[2] << ']'
       << ",\"L4cache\":" << '[' << osCacheIdx[3] << ',' << hwlocCacheIdx[3] << ']'
       << ",\"L5cache\":" << '[' << osCacheIdx[4] << ',' << hwlocCacheIdx[4] << ']'
       << ",\"cpu\":" << osCpuIdx_
       << "}";
}
