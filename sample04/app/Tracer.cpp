#include "Tracer.h"

Tracer::Tracer():
  leftWheel(PORT_C), rightWheel(PORT_B),
  colorSensor(PORT_3) {
  }

void Tracer::init() {
  init_f("Tracer");
}

void Tracer::terminate() {
  msg_f("Stopped.", 1);
  leftWheel.stop();
  rightWheel.stop();
}

void Tracer::run() {
  const float Kp = 0.83; // <1>
  const int target = 10; // <2>
  const int bias = 0;    // <3>
  
  msg_f("running...", 1);
  int diff = colorSensor.getBrightness() - target; // <4>
  float turn = Kp * diff + bias; // <5>
  leftWheel.setPWM(pwm - turn);  // <6>
  rightWheel.setPWM(pwm + turn); // <6>
}
