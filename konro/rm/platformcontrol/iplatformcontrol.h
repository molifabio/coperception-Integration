#ifndef IPLATFORMCONTROL_H
#define IPLATFORMCONTROL_H

#include "app.h"

namespace pc {

/*!
 * \interface an interface describing the behavior of the platform control component
 */
class IPlatformControl {
public:

    /*!
     * \brief Adds an application under the management of Konro.
     * \param app the application to manage
     */
    virtual bool addApplication(std::shared_ptr<rmcommon::App> app) = 0;

    /*!
     * \brief Removes an application from the management of Konro.
     * \param app the application to remove from Konro's management
     */
    virtual bool removeApplication(std::shared_ptr<rmcommon::App> app) = 0;
};

}

#endif // IPLATFORMCONTROL_H
