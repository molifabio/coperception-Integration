#ifndef __PYTHONMOBILITY_H
#define __PYTHONMOBILITY_H

#include "inet/mobility/base/MovingMobilityBase.h"

using namespace inet;

class PythonMobility : public MovingMobilityBase
{
  protected:
    virtual void initialize(int stage) override;
    virtual void move() override;

  public:
    void setPosition(double x, double y, double z);
};

#endif
