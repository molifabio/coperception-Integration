#ifndef MEMORYINFO_H
#define MEMORYINFO_H

#include <sys/sysinfo.h>

namespace rmcommon {

/*!
 * Returns the total amount of RAM on the machine in KB
 *
 * \param totalRamKb [out] total RAM in KB
 */
void getMemoryInfo(unsigned long &totalRamKb) noexcept
{
    struct sysinfo info;
    if (sysinfo(&info) < 0) {
        totalRamKb = 0;
    } else {
        totalRamKb = info.totalram * info.mem_unit / 1024;
    }
}

/*!
 * Returns the total amount of SWAP on the machine in KB
 *
 * \param totalSwapKb [out] total SWAP in KB
 */
void getSwapInfo(unsigned long &totalSwapKb) noexcept
{
    struct sysinfo info;
    if (sysinfo(&info) < 0) {
        totalSwapKb = 0;
    } else {
        totalSwapKb = info.totalswap * info.mem_unit / 1024;
    }
}

}   // namespace rmcommon

#endif // MEMORYINFO_H
