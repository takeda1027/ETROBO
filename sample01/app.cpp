// tag::walker_def[]
#include "app.h"
#include "util.h"

#include "Motor.h"
#include "Clock.h"

using namespace ev3api;

class Walker {
public:
  Walker();
  void run();

private:
  Motor leftWheel;              // <1>
  Motor rightWheel;             // <1>
  Clock clock;                  // <1>

  const int8_t pwm = (Motor::PWM_MAX) / 6;
  const uint32_t duration = 2000;
};
// end::walker_def[]
// tag::walker_impl[]
Walker::Walker():
  leftWheel(PORT_C), rightWheel(PORT_B) {
}

void Walker::run() {
  init_f(__FILE__);
  while(1) {
    msg_f("Forwarding...", 1);
    leftWheel.setPWM(pwm);
    rightWheel.setPWM(pwm);
    clock.sleep(duration);

    msg_f("Backwarding...", 1);
    leftWheel.setPWM(-pwm);
    rightWheel.setPWM(-pwm);
    clock.sleep(duration);

    // 左ボタンを長押し、それを捕捉する
    if (ev3_button_is_pressed(LEFT_BUTTON)) {
      break;
    }
  }

  msg_f("Stopped.", 1);
  leftWheel.stop();
  rightWheel.stop();
  while(ev3_button_is_pressed(LEFT_BUTTON)) {
    ;
  }
}
// end::walker_impl[]
// tag::main_task[]
void main_task(intptr_t unused) {

  Walker walker;                // <1>

  walker.run();                 // <2>

  ext_tsk();
}
// end::main_task[]
