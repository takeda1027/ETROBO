#include "Motor.h"
#include "Clock.h"

#include "util.h"

using namespace ev3api;

class Walker {
public:
  Walker();
  void run();
  void init();
  void terminate();

private:
  Motor leftWheel;
  Motor rightWheel;
  Clock clock;
  bool  isForwarding;
  const int8_t pwm = (Motor::PWM_MAX) / 6;
  const uint32_t duration = 2000;
};
