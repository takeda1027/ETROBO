#include "Motor.h"
#include "ColorSensor.h"
#include "util.h"
#include "GyroSensor.h" //朋子追加
#include "SonarSensor.h" //朋子追加
#include "Clock.h"  // 朋子追加
// #include "PrmReq.h" //朋子追加

using namespace ev3api;

class Tracer {
public:
  Tracer();
  void run();
  void init();
  void terminate();
  //朋子変数
  int hoge=0;
  int pwm_slow = 10;
  int slalom_flg = 0;
 
private:
  Motor leftWheel;
  Motor rightWheel;
  ColorSensor colorSensor;
  GyroSensor gyro_a; //朋子追加
  SonarSensor sonar_a; //朋子追加
  Motor sippoWheel; //朋子追加 尻尾用
  Motor armWheel; //朋子追加　アーム用

//  PrmReq prmadd; //朋子追加
  const int8_t mThreshold = 20;
  const int8_t pwm = (Motor::PWM_MAX) / 3;
  int judge_c = 0;

  float calc_prop_value();      // <1>
  void turn_r(Motor r_leftWheel,Motor l_leftWheel,Clock c_clock);
};
