#include "Walker.h"

Walker::Walker():
  leftWheel(PORT_C), rightWheel(PORT_B), isForwarding(true) {
}

void Walker::init() {
  init_f("Walker");
}

void Walker::terminate() {
  msg_f("Stopped.", 1);
  leftWheel.stop();
  rightWheel.stop();
}

void Walker::run() {
  if(isForwarding) {
    msg_f("Forwarding...", 1);
    leftWheel.setPWM(pwm);
    rightWheel.setPWM(pwm);
    isForwarding = false;
  } else {
    msg_f("Backwarding...", 1);
    leftWheel.setPWM(-pwm);
    rightWheel.setPWM(-pwm);
    isForwarding = true;
  }
}
