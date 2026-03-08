#ifndef KONROFEEDBACK_H
#define KONROFEEDBACK_H

#include <string>

/*!
 * \brief client library for implementing integrated applications.
 *
 * An integrated application must first send an add request
 * to Konro. Then, it must periodically send a feedback
 * message to Konro using the functions of this library.
 */
namespace konro {

    /*!
     * Computes the feedback value to send to Konro.
     * The value is generated as a simple percentage, based on
     * an arbitrary metric and target value chosen by the application.
     * The result is always included in the [0, 200] range.
     * Any value greater than 200 is approximated with 200.
     * \p
     * If an application feedback is over 100 (the target metric is
     * satisfied and beyond), then that app is a good candidate to
     * further constrain resource consumption by the policy.
     *
     * \example a video processing application chooses the frame rate
     * as its metric and the value 30 as target frame rate.
     *
     * \code
     * Feedback = (current frame rate / target) * 100
     * \endcode
     *
     * \param curValue the current value for the metric
     * \param target the target value for the metric
     * \return the feedback value
     */
    extern int computeFeedback(int curValue, int target);

    /*!
     * Sends a feedback message to Konro in JSON format.
     */
    extern std::string sendFeedbackMessage(int feedback);


    /*!
     * Sends an add message to Konro in JSON format.
     */
    extern std::string sendAddMessage();

}   // namespace feedback

#endif // KONROFEEDBACK_H
