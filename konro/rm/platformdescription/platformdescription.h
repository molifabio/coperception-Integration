#ifndef PLATFORMDESCRIPTION_H
#define PLATFORMDESCRIPTION_H

#include "processingunitmapping.h"
#include <log4cpp/Category.hh>
#include <memory>
/*!
 * \brief stores information about the machine on which Konro is running
 */
class PlatformDescription {

    /* Pointer to implementation pattern */
    struct PlatformDescriptionImpl;
    std::shared_ptr<PlatformDescriptionImpl> pimpl_;

    log4cpp::Category &cat_;

    /* Total amount of RAM in kilobytes */
    unsigned long totalRamKB_;
    /* Total amount of SWAP in kilobytes */
    unsigned long totalSwapKB_;

    /*! Finds memory information about the machine */
    void initMemoryInfo();

public:
    explicit PlatformDescription();

    /*!
     * Gets the number physical processors on the machine.
     * A CPU is physical package that gets inserted into a
     * socket on the motherboard. A processor package usually
     * contains multiple cores.
     * \return the number of CPUs
     */
    int getNumCpus() const;

    /*!
     * \brief Gets the number of cores on the machine
     * \return the number of cores
     */
    int getNumCores() const;

    /*!
     * Gets the number of processing units (PUs) on the machine.
     * A PU is the smallest processing element under which a process can be
     * scheduled. The total number of PUs of the machine is
     * always equal to its total amount of hardware threads.
     * Clarly, in non-SMT processors the number of hardware threads is
     * equal to the number of cores.
     * \return the number of processing units
     */
    int getNumProcessingUnits() const;

    /*!
     * \brief Returns the machine topology as a vector of ProcessingUnitMapping
     */
    std::vector<ProcessingUnitMapping> getTopology() const;

    std::set<short> getPUSet();

    /*!
     * \li distance 0: pu1 and pu2 on the same L1 cache
     * \li distance 1: pu1 and pu2 on the same L2 cache
     * \li distance 2: pu1 and pu2 on the same L3 cache
     * \li distance 4: otherwise
     *
     * \param pu1 Processing Unit number
     * \param pu2 Processing Unit number
     * \return the "distance" between the two PUs
     */
    int getPUDistance(short pu1, short pu2);

    /*!
     * \brief Gets the total amount of RAM on the machine in Kb
     * \return the amount of RAM
     */
    unsigned long getTotalRam() const {
        return totalRamKB_;
    }

    /*!
     * \brief Gets the total amount of swap on the machine in Kb
     * \return the amount of swap
     */
    unsigned long getTotalSwap() const {
        return totalSwapKB_;
    }

    /*!
     * Prints a textual representation of the machine topology.
     */
    void logTopology();
};

#endif // PLATFORMDESCRIPTION_H
