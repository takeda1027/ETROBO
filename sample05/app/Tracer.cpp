#include "Tracer.h"
#include "Clock.h"  // 朋子追加
//#include "PrmReq.h" //朋子追加

Tracer::Tracer():
  leftWheel(PORT_C), rightWheel(PORT_B),
  colorSensor(PORT_3), gyro_a(PORT_2),sonar_a(PORT_4),sippoWheel(PORT_D),armWheel(PORT_A){
}

void Tracer::init() {
  init_f("Tracer");
  judge_c = 0;
  slalom_flg = 0;
}

void Tracer::terminate() {
  msg_f("Stopped.", 1);
  leftWheel.stop();
  rightWheel.stop();
}

float Tracer::calc_prop_value() {
//  const float Kp = 0.83;        // <1>
//  const int target = 10;        // <2>
//  const float Kp = 0.83;        // <1> const int8_t pwm = (Motor::PWM_MAX) / 3;
//  const int target = 18;        // <2> const int8_t pwm = (Motor::PWM_MAX) / 3;
  const float Kp = 0.83;        // <1> const int8_t pwm = 2*(Motor::PWM_MAX) / 3;
  const int target = 18;        // <2> const int8_t pwm = 2*(Motor::PWM_MAX) / 3;
  const int bias = 0;

  int diff = colorSensor.getBrightness() - target; // <3>
  //朋子追加 ログ
  //printf("getBrightness=%d, diff=%d, target=%d\n", colorSensor.getBrightness(),diff,target);
  //printf("getAngle=%d, getAnglerVelocity=%d\n",gyro_a.getAngle(),gyro_a.getAnglerVelocity());//ジャイロセンサー値確認　朋子
  return (Kp * diff + bias);                       // <4>
}

void Tracer::turn_r(Motor r_leftWheel,Motor l_leftWheel,Clock c_clock) {
    r_leftWheel.setPWM(0);
    l_leftWheel.setPWM(50);
    c_clock.sleep(910); 
}

void Tracer::run() {
  msg_f("running...", 1);
  float turn = calc_prop_value(); // <1>
  Clock clock;    // 朋子追加
  //int pwm_l = pwm - turn;      // <2>
  //int pwm_r = pwm + turn;      // <2>
  //朋子変更　ストレートで加速 20200809
  int pwm_l;
  int pwm_r;
  int pwm_s;

  pwm_l = pwm - turn;      // <2>
  pwm_r = pwm + turn;      // <2>
  pwm_s = -100;
  
  printf("pwm_l=%d,pwm_r=%d,ソナー１=%d, ソナー2=%d, hoge=%d,gyro_a=%d\n",pwm_l,pwm_r,sonar_a.listen(),sonar_a.getDistance(),hoge,gyro_a.getAnglerVelocity());//ジャイロセンサー値確認　朋子
  judge_c = judge_c + gyro_a.getAnglerVelocity();

  //段差をソナーセンサーで検知、駆動力上げ、ジャンプorアーム上げで突破
  if(sonar_a.getDistance() > 0 && sonar_a.getDistance() < 12){
    leftWheel.setPWM(100);
    rightWheel.setPWM(100);
    //尻尾で床を叩いてジャンプ！
    sippoWheel.setPWM(100);
    //腕上げる
    //armWheel.setPWM(100);
    printf("駆動力ＵＰ開始、ジャイロは%d\n",judge_c);
    clock.sleep(1000);
    printf("駆動力ＵＰ終了\n");
    slalom_flg = 1;
    //角まで前進
    rightWheel.setPWM(55);
    leftWheel.setPWM(50);
    clock.sleep(800);
    leftWheel.setPWM(57);
    clock.sleep(3400);

    leftWheel.setPWM(0);
    rightWheel.setPWM(0);
    //腕下げる
    //armWheel.setPWM(-100);
    //尻尾しまう
    sippoWheel.setPWM(-100);
    printf("駆動力０、しっぽしまう開始\n");
    clock.sleep(2000);
    printf("駆動力０、しっぽしまう終了\n");

    printf("回転開始\n");
    turn_r(rightWheel,leftWheel,clock);

    //回転後、とりあえず直進
    rightWheel.setPWM(55);
    leftWheel.setPWM(55);
    clock.sleep(7500);

  }
  
  //printf("pwm_l=%d, pwm_r=%d, turn=%d, hoge=%d\n", pwm_l,pwm_r,turn,hoge);
  leftWheel.setPWM(pwm_l);
  rightWheel.setPWM(pwm_r);
  sippoWheel.setPWM(pwm_s);
}

