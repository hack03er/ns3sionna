/*
 * Author: Johannes Hack
 */

#ifndef SIONNA_RIS_MODEL_H
#define SIONNA_RIS_MODEL_H

#include "ns3/vector.h"

namespace ns3 {

/**
* @brief Abstract base class for RIS controllers
 *
 * This class provides the basic framework for RIS controller implementations
 * with position and lookAt vector properties.
 */
class AbstractRisController {
protected:
    Vector position;    // Current position
    Vector lookAt;      // Direction vector the controller is looking at

public:

    AbstractRisController();

    AbstractRisController(const Vector& pos, const Vector& look);

    /**
     * @brief Virtual destructor
     */
    virtual ~AbstractRisController();

    Vector getPosition() const;

    void setPosition(const Vector& pos);

    Vector getLookAt() const;

    void setLookAt(const Vector& look);

    /**
     * @brief Update method to be implemented by derived classes
     */
    virtual void update() = 0;
};

/**
 * @brief A controller that updates at periodic intervals
 *
 * This class extends AbstractRisController to add periodic update functionality
 * based on a specified update frequency.
 */
class PeriodicRisController : public AbstractRisController {
private:
    double updateFrequency;  // How often the controller updates (in Hz)
    double lastUpdateTime;   // Time of last update

public:
    PeriodicRisController();

    PeriodicRisController(const Vector& pos, const Vector& look, double freq);

    double getUpdateFrequency() const;

    void setUpdateFrequency(double freq);

    double getLastUpdateTime() const;

    /**
     * @brief Update the controller if enough time has passed
     * @param currentTime The current system time
     */
    void updateIfNeeded(double currentTime);

    void update() override;
};

}

#endif // SIONNA_RIS_MODEL_H