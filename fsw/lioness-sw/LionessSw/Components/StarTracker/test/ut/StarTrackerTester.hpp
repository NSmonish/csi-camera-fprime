// StarTrackerTester.hpp
// Unit test harness for the StarTracker component.
// Style rule for this project: plain punctuation only, no em dashes.

#ifndef LionessSw_Components_StarTracker_test_ut_StarTrackerTester_HPP
#define LionessSw_Components_StarTracker_test_ut_StarTrackerTester_HPP

#include "StarTrackerGTestBase.hpp"
#include "LionessSw/Components/StarTracker/StarTracker.hpp"

namespace Components {

class StarTrackerTester : public StarTrackerGTestBase {
public:
    explicit StarTrackerTester(StarTracker& inst);
    ~StarTrackerTester();

    void testValidFrame();
    void testInvalidFrame();

private:
    StarTracker& m_impl;
};

}  // namespace Components

#endif
