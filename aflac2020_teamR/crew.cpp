//
//  crew.cpp
//  aflac2019
//
//  Created by Wataru Taniguchi on 2019/04/28.
//  Copyright © 2019 Ahiruchan Koubou. All rights reserved.
//

#include "app.h"
#include "crew.hpp"

// global variables to pass FIR-filtered color from Observer to Navigator and its sub-classes
rgb_raw_t g_rgb;
hsv_raw_t g_hsv;
int16_t g_grayScale, g_grayScaleBlueless, test_mode, obstcl_mode;
bool b1, b2, b3, slalom_flg, angl_calc_flg, line_trace_flg;
// global variables to gyro sensor output from Observer to  Navigator and its sub-classes
int16_t g_angle, g_anglerVelocity;

Radioman::Radioman() {
    _debug(syslog(LOG_NOTICE, "%08u, Radioman constructor", clock->now()));
    /* Open Bluetooth file */
    bt = ev3_serial_open_file(EV3_SERIAL_BT);
    assert(bt != NULL);
}

void Radioman::operate() {
    uint8_t c = fgetc(bt); /* 受信 */
    fputc(c, bt); /* エコーバック */
    switch(c)
    {
        case CMD_START_R:
        case CMD_START_r:
            syslog(LOG_NOTICE, "%08u, StartCMD R-mode received", clock->now());
            captain->decide(EVT_cmdStart_R);
            break;
        case CMD_START_L:
        case CMD_START_l:
            syslog(LOG_NOTICE, "%08u, StartCMD L-mode received", clock->now());
            captain->decide(EVT_cmdStart_L);
            break;
        default:
            break;
    }
}

Radioman::~Radioman() {
    _debug(syslog(LOG_NOTICE, "%08u, Radioman destructor", clock->now()));
    fclose(bt);
}

Observer::Observer(Motor* lm, Motor* rm, Motor* am, Motor* tm, TouchSensor* ts, SonarSensor* ss, GyroSensor* gs, ColorSensor* cs) {
    _debug(syslog(LOG_NOTICE, "%08u, Observer constructor", clock->now()));
    leftMotor   = lm;
    rightMotor  = rm;
    armMotor = am;
    tailMotor = tm;
    touchSensor = ts;
    sonarSensor = ss;
    gyroSensor  = gs;
    colorSensor = cs;
    distance = 0.0;
    azimuth = 0.0;
    locX = 0.0;
    locY = 0.0;
    prevAngL = 0;
    prevAngR = 0;
    notifyDistance = 0;
    traceCnt = 0;
    prevGS = INT16_MAX;
    touch_flag = false;
    sonar_flag = false;
    lost_flag = false;
    blue_flag = false;

    curAngle = 0; //sano
    prevAngle = 0; //sano
    test_mode = 1; // sanoテスト用
    obstcl_mode = 0; // sano
    b1 = false; // sano
    b2 = false; // sano テスト用
    b3 = false; // sano
    slalom_flg = false; // sanoテスト用
    angl_calc_flg = false; // sano
    line_trace_flg = true;// sano
    obj_flg=false; //sano
    right_angle =false;//sano
    gyroSensor->setOffset(0);//sano

    fir_r = new FIR_Transposed<FIR_ORDER>(hn);
    fir_g = new FIR_Transposed<FIR_ORDER>(hn);
    fir_b = new FIR_Transposed<FIR_ORDER>(hn);
    ma = new MovingAverage<int32_t, MA_CAP>();
}

void Observer::goOnDuty() {
    // register cyclic handler to EV3RT
    sta_cyc(CYC_OBS_TSK);
    //clock->sleep() seems to be still taking milisec parm
    clock->sleep(PERIOD_OBS_TSK/2/1000); // wait a while
    _debug(syslog(LOG_NOTICE, "%08u, Observer handler set", clock->now()));
}

void Observer::reset() {
    distance = 0.0;
    azimuth = 0.0;
    locX = 0.0;
    locY = 0.0;
    prevAngL = leftMotor->getCount();
    prevAngR = rightMotor->getCount();
}

void Observer::notifyOfDistance(int32_t delta) {
    notifyDistance = delta + distance;
}

int32_t Observer::getDistance() {
    return (int32_t)distance;
}

int16_t Observer::getAzimuth() {
    // degree = 360.0 * radian / M_2PI;
    int16_t degree = (360.0 * azimuth / M_2PI);
    return degree;
}

int16_t Observer::getDegree() {
    // degree = 360.0 * radian / M_2PI;
    int16_t degree = (360.0 * azimuth / M_2PI);
    if (degree > 180){
        degree -= 360;
    }
    return degree;
}

int32_t Observer::getLocX() {
    return (int32_t)locX;
}

int32_t Observer::getLocY() {
    return (int32_t)locY;
}

double Observer::getAccumAngleVl() {
    //printf("b3_aabeforecaptain=%lf,curTime=%d\n,",b3_aa,curTime);
    return 0;
}

void Observer::operate() {
    colorSensor->getRawColor(cur_rgb);
    // process RGB by the Low Pass Filter
    cur_rgb.r = fir_r->Execute(cur_rgb.r);
    cur_rgb.g = fir_g->Execute(cur_rgb.g);
    cur_rgb.b = fir_b->Execute(cur_rgb.b);
    rgb_to_hsv(cur_rgb, cur_hsv);
    // save filtered color variables to the global area
    g_rgb = cur_rgb;
    g_hsv = cur_hsv;
    // calculate gray scale and save them to the global area
 
    //スラローム上では、茶色地面なので青、赤を逆転っぽく sano
    if(!slalom_flg){
        g_grayScale = (cur_rgb.r * 77 + cur_rgb.g * 120 + cur_rgb.b * 29) / 226;
        g_grayScaleBlueless = (cur_rgb.r * 77 + cur_rgb.g * 120 + (cur_rgb.b - cur_rgb.g) * 29) / 226; // B - G cuts off blue
    }else{
        g_grayScale = ((cur_rgb.r-4) * 77 + (cur_rgb.g-1) * 120 + (cur_rgb.b + 17) * 29) / 226;
        g_grayScaleBlueless = ((cur_rgb.r-4) * 77 + (cur_rgb.g-1) * 120 +  (cur_rgb.b - cur_rgb.g + 16)  * 29) / 226; // B - G cuts off blue
    }
    
    // save gyro sensor output to the global area
    g_angle = gyroSensor->getAngle();
    g_anglerVelocity = gyroSensor->getAnglerVelocity();

    //printf("g_angle=%d, g_anglerVelocity=%d\n", g_angle, g_anglerVelocity);

    // accumulate distance
    int32_t curAngL = leftMotor->getCount();
    int32_t curAngR = rightMotor->getCount();
    double deltaDistL = M_PI * TIRE_DIAMETER * (curAngL - prevAngL) / 360.0;
    double deltaDistR = M_PI * TIRE_DIAMETER * (curAngR - prevAngR) / 360.0;
    double deltaDist = (deltaDistL + deltaDistR) / 2.0;
    distance += deltaDist;
    prevAngL = curAngL;
    prevAngR = curAngR;
    // calculate azimuth
    double deltaAzi = atan2((deltaDistL - deltaDistR), WHEEL_TREAD);
    azimuth += deltaAzi;
    if (azimuth > M_2PI) {
        azimuth -= M_2PI;
    } else if (azimuth < 0.0) {
        azimuth += M_2PI;
    }

    // estimate location
    locX += (deltaDist * sin(azimuth));
    locY += (deltaDist * cos(azimuth));

    printf("getAngle=%d\n",gyroSensor->getAngle());

    //sano テスト
    if(b2 && gyroSensor->getAngle()<-10){
        //azimuth =0;
        float hoge = 360.0 * azimuth/ M_2PI;
        if(hoge < 180){
            hoge =  hoge * 22 /30;
        }else{
            hoge = (hoge - 360) * 22 /30 + 360;
        }
        azimuth = hoge * M_2PI / 360.0;
        printf("通っている\n");
    }
    float r = 0.0;
    r = 360.0 * azimuth/ M_2PI - 13 ;
    if(r < 0){
        r+=360;
    }
    //printf("deg=%lf,touchSensor=%d\n",r,touchSensor->isPressed());
    //

    // monitor distance
    if ((notifyDistance != 0.0) && (distance > notifyDistance) && !slalom_flg) {
        syslog(LOG_NOTICE, "%08u, distance reached", clock->now());
        notifyDistance = 0.0; // event to be sent only once
        captain->decide(EVT_dist_reached);
    }
    
    // monitor touch sensor
    bool result = check_touch();
    if (result && !touch_flag) {
        syslog(LOG_NOTICE, "%08u, TouchSensor flipped on", clock->now());
        touch_flag = true;
        captain->decide(EVT_touch_On);
    } else if (!result && touch_flag) { // sanoテスト用
        syslog(LOG_NOTICE, "%08u, TouchSensor flipped off", clock->now());
        touch_flag = false;
        captain->decide(EVT_touch_Off);
     }
    
    // monitor sonar sensor
    // sanoコメントアウト
    result = check_sonar();
    if (result && !sonar_flag) {
        syslog(LOG_NOTICE, "%08u, SonarSensor flipped on", clock->now());
        sonar_flag = true;
        captain->decide(EVT_sonar_On);
    } else if (!result && sonar_flag) {
        syslog(LOG_NOTICE, "%08u, SonarSensor flipped off", clock->now());
        sonar_flag = false;
        captain->decide(EVT_sonar_Off);
    }

    // if (!frozen) { // these checks are meaningless thus bypassed when frozen
    //     // determine if still tracing the line
    //     result = check_lost();
    //     if (result && !lost_flag) {
    //         syslog(LOG_NOTICE, "%08u, line lost", clock->now());
    //         lost_flag = true;
    //         captain->decide(EVT_line_lost);
    //     } else if (!result && lost_flag) {
    //         syslog(LOG_NOTICE, "%08u, line found", clock->now());
    //         lost_flag = false;
    //         captain->decide(EVT_line_found);
    //     }

    // }
    
    // display trace message in every PERIOD_TRACE_MSG ms */
    int32_t d = getDistance();

    //sano：開始
    //printf(",d=%d",d);
    //障害物との距離=>アーム上げのタイミング判定に利用
     int32_t dis = sonarSensor->getDistance();
 
    //ブルー１個目の判断 前方何もなし、ラインブルーの場合(スタート地点でなぜか処理に入ってしまうためcur_rgb.b <=255を追加)
    if( cur_rgb.b - cur_rgb.r > 60 && !b1 && dis > 250 && cur_rgb.b <=255 && cur_rgb.r<=255){
        b1 =true;
    }else if(b1 && cur_rgb.b - cur_rgb.r < 40){
        b1 = false; //１つめのブルー検知フラグを落とす
    }

    //b-r青判定、スラロームが近い（前方障害あり）、ブルー２個目b2フラグ立てる
    if(cur_rgb.b - cur_rgb.r > 60 && dis < 50  && cur_rgb.b <=255 && cur_rgb.r<=255){
        b2 =true;
        armMotor->setPWM(20);
    }

    //スラローム判定
    if(b2 && !slalom_flg){
        // if(dis < 10){
        //     //armMotor->setPWM(20);
        //     tailMotor->setPWM(50);
        // }
        //printf(",g_angle=%d\n", g_angle);
        // if(check_touch()){
        //     tailMotor->setPWM(100);
        // }
        curAngle = g_angle;
        if(curAngle < -10){
            tailMotor->setPWM(100);
            prevAngle = curAngle;
        }
        if (prevAngle < -10 && curAngle >= 0){
            printf("スラロームフラグ\n");
            slalom_flg=true;
            curAngle = 0;//初期化
            prevAngle = 0;//初期化
            captain->decide(EVT_slalom_on);
        }
    }

    if (b2){
        printf("deg=%lf,touchSensor=%d\n",r,touchSensor->isPressed());
    }
    
    //スラローム専用処理
    //printf("g_angle=%d\n", g_angle);
    if(slalom_flg){

        //captain->decide(EVT_slalom_avoid);
        printf("Degree=%d\n", observer->getDegree());
        // printf("distance=%d", dis);
        // printf(",r+g+b=%d,r=%d,g=%d,b=%d,right_angle=%d\n",cur_rgb.r + cur_rgb.g + cur_rgb.b,cur_rgb.r,cur_rgb.g,cur_rgb.b,right_angle);
    
        // if ((getDegree() > 10 || getDegree() < -10) && obstcl_mode < 1){
        //     //printf("Degree=%d\n", observer->getDegree());
        //     captain->decide(EVT_goto_obstcl); // 
        // }

        // 左下の直角カーブ対応
        if(cur_rgb.r + cur_rgb.g + cur_rgb.b <= 80 && !right_angle && line_trace_flg){
            right_angle=true;
            line_trace_flg = false;
            printf("直角ターン\n");
            captain->decide(EVT_turnCnr); // ここで直角ターン
        }
        //車体の傾きでスラローム終了を検知
        //printf("g_angle=%d\n");
         if(g_angle > 10){
            slalom_flg = false;
        }
    }


    //sano　青判定３回目
    if( cur_rgb.b - cur_rgb.r > 60 && right_angle){
        //printf(",b-r=%d,r=%d,g=%d,b=%d,right_angle=%d\n", cur_rgb.b - cur_rgb.r,cur_rgb.r,cur_rgb.g,cur_rgb.b,right_angle);
        b3 = true;
        captain->decide(EVT_turnb3); // ソナー稼働回転、物体を見つけに行く
    }

    //printf(",b-r=%d,r=%03u, g=%03u, b=%03u, b1=%d,b2=%d,slalom_flg=%d\n", g_rgb.b-g_rgb.r,g_rgb.r, g_rgb.g, g_rgb.b,b1,b2,slalom_flg);
    //sano：終了

    // display trace message in every PERIOD_TRACE_MSG ms */
    if (++traceCnt * PERIOD_OBS_TSK >= PERIOD_TRACE_MSG) {
    //if ((++traceCnt * PERIOD_OBS_TSK >= PERIOD_TRACE_MSG) && (d < 11000)) {
        traceCnt = 0;
        //_debug(syslog(LOG_NOTICE, "%08u, Observer::operate(): distance = %d, azimuth = %d, x = %d, y = %d", clock->now(), getDistance(), getAzimuth(), getLocX(), getLocY()));
    //    _debug(syslog(LOG_NOTICE, "%08u, Observer::operate(): distance = %d, azimuth = %d, x = %d, y = %d", clock->now(), d, getAzimuth(), getLocX(), getLocY()));
    //  _debug(syslog(LOG_NOTICE, "%08u, Observer::operate(): hsv = (%03u, %03u, %03u)", clock->now(), g_hsv.h, g_hsv.s, g_hsv.v));
    //  _debug(syslog(LOG_NOTICE, "%08u, Observer::operate(): rgb = (%03u, %03u, %03u)", clock->now(), g_rgb.r, g_rgb.g, g_rgb.b));
    //  _debug(syslog(LOG_NOTICE, "%08u, Observer::operate(): angle = %d, anglerVelocity = %d", clock->now(), g_angle, g_anglerVelocity));
    //} else if (d >= 11000) {
    //    _debug(syslog(LOG_NOTICE, "%08u, Observer::operate(): distance = %d, azimuth = %d, x = %d, y = %d", clock->now(), d, getAzimuth(), getLocX(), getLocY()));
    //    _debug(syslog(LOG_NOTICE, "%08u, Observer::operate(): hsv = (%03u, %03u, %03u)", clock->now(), g_hsv.h, g_hsv.s, g_hsv.v));
    //    _debug(syslog(LOG_NOTICE, "%08u, Observer::operate(): rgb = (%03u, %03u, %03u)", clock->now(), g_rgb.r, g_rgb.g, g_rgb.b));
    }
}

void Observer::goOffDuty() {
    // deregister cyclic handler from EV3RT
    stp_cyc(CYC_OBS_TSK);
    //clock->sleep() seems to be still taking milisec parm
    clock->sleep(PERIOD_OBS_TSK/2/1000); // wait a while
    //_debug(syslog(LOG_NOTICE, "%08u, Observer handler unset", clock->now()));
}

// bool Observer::check_touch(void) {
//     if (touchSensor->isPressed() && !b2 ){  //sano_t
//         return true;
//     } else {
//         return false;
//     }
// }

bool Observer::check_touch(void) {
    if (touchSensor->isPressed()){  //sano_t
    printf("タッチ\n");
        return true;
    } else {
        return false;
    }
}

bool Observer::check_sonar(void) {
    int32_t distance = sonarSensor->getDistance();
    if ((distance <= SONAR_ALERT_DISTANCE) && (distance >= 0)) {
        return true; // obstacle detected - alert
    } else {
        return false; // no problem
    }
}

bool Observer::check_lost(void) {
    if (g_grayScale > GS_LOST) {
        return true;
    } else {
        return false;
    }
}

bool Observer::check_tilt(void) {
    int16_t anglerVelocity = gyroSensor->getAnglerVelocity();
    if (anglerVelocity < ANG_V_TILT && anglerVelocity > (-1) * ANG_V_TILT) {
        return false;
    } else {
       // _debug(syslog(LOG_NOTICE, "%08u, Observer::operate(): TILT anglerVelocity = %d", clock->now(), anglerVelocity));
        return true;
    }
}

void Observer::freeze() {
    frozen = true;
}

void Observer::unfreeze() {
    frozen = false;
}

Observer::~Observer() {
    _debug(syslog(LOG_NOTICE, "%08u, Observer destructor", clock->now()));
}

Navigator::Navigator() {
    _debug(syslog(LOG_NOTICE, "%08u, Navigator default constructor", clock->now()));
    ltPid = new PIDcalculator(P_CONST, I_CONST, D_CONST, PERIOD_NAV_TSK, -16, 16); 
}

void Navigator::goOnDuty() {
    // register cyclic handler to EV3RT
    sta_cyc(CYC_NAV_TSK);
    //clock->sleep() seems to be still taking milisec parm
    clock->sleep(PERIOD_NAV_TSK/2/1000); // wait a while
    _debug(syslog(LOG_NOTICE, "%08u, Navigator handler set", clock->now()));
}

void Navigator::goOffDuty() {
    activeNavigator = NULL;
    // deregister cyclic handler from EV3RT
    stp_cyc(CYC_NAV_TSK);
    //clock->sleep() seems to be still taking milisec parm
    clock->sleep(PERIOD_NAV_TSK/2/1000); // wait a while
    _debug(syslog(LOG_NOTICE, "%08u, Navigator handler unset", clock->now()));
}

Navigator::~Navigator() {
    _debug(syslog(LOG_NOTICE, "%08u, Navigator destructor", clock->now()));
}

AnchorWatch::AnchorWatch(Motor* tm) {
    _debug(syslog(LOG_NOTICE, "%08u, AnchorWatch constructor", clock->now()));
}

void AnchorWatch::haveControl() {
    activeNavigator = this;
    syslog(LOG_NOTICE, "%08u, AnchorWatch has control", clock->now());
}

void AnchorWatch::operate() {
    //controlTail(TAIL_ANGLE_STAND_UP,10); /* 完全停止用角度に制御 */
}

AnchorWatch::~AnchorWatch() {
    _debug(syslog(LOG_NOTICE, "%08u, AnchorWatch destructor", clock->now()));
}

LineTracer::LineTracer(Motor* lm, Motor* rm, Motor* tm) {
    _debug(syslog(LOG_NOTICE, "%08u, LineTracer constructor", clock->now()));
    leftMotor   = lm;
    rightMotor  = rm;
    trace_pwmLR = 0;
    speed       = SPEED_NORM;
    frozen      = false;
}

void LineTracer::haveControl() {
    activeNavigator = this;
    syslog(LOG_NOTICE, "%08u, LineTracer has control", clock->now());
}

void LineTracer::operate() {
    //controlTail(TAIL_ANGLE_DRIVE,10); /* バランス走行用角度に制御 */

        if (frozen) {
            forward = turn = 0; /* 障害物を検知したら停止 */
        } else{
            forward = speed; //前進命令
 
            // PID control by Gray Scale with blue cut
            int16_t sensor = g_grayScaleBlueless;
            int16_t target = GS_TARGET;

            if (state == ST_tracing_L || state == ST_stopping_L) {
                turn = ltPid->compute(sensor, target);
            } else {
                turn = (-1) * ltPid->compute(sensor, target);
            }
        }

        /* 左右モータでロボットのステアリング操作を行う */
        pwm_L = forward - turn;
        pwm_R = forward + turn;

        leftMotor->setPWM(pwm_L);
        rightMotor->setPWM(pwm_R);

        if(b2 && !slalom_flg && turn == 0 && !angl_calc_flg){
            angl_calc_flg = true;
        }
}

void LineTracer::setSpeed(int8_t s) {
    speed = s;
}

void LineTracer::freeze() {
    frozen = true;
}

void LineTracer::unfreeze() {
    frozen = false;
}


LineTracer::~LineTracer() {
    _debug(syslog(LOG_NOTICE, "%08u, LineTracer destructor", clock->now()));
}

ChallengeRunner::ChallengeRunner(Motor* lm, Motor* rm, Motor* tm) : LineTracer(lm, rm, tm){
    _debug(syslog(LOG_NOTICE, "%08u, ChallengeRunner constructor", clock->now()));
    leftMotor   = lm;
    rightMotor  = rm;
    // //trace_pwmLR = 0;
    // speed       = SPEED_NORM;
    pwm_p_L = 10; //sano
    pwm_p_R = 10; //sano
    
    frozen      = false;
}

void ChallengeRunner::haveControl() {
    activeNavigator = this;
    syslog(LOG_NOTICE, "%08u, ChallengeRunner has control", clock->now());
}

void ChallengeRunner::operate() {

    if (!line_trace_flg){
        if (frozen) {
            pwm_L = 0;
            pwm_R = 0;
        } else{
            pwm_L = pwm_p_L;
            pwm_R = pwm_p_R;
        }
        leftMotor->setPWM(pwm_L);
        rightMotor->setPWM(pwm_R);
    } else {
        //printf("ライントレース\n");
        if (frozen) {
            forward = turn = 0; /* 障害物を検知したら停止 */
        } else{
            forward = speed; //前進命令
 
            // PID control by Gray Scale with blue cut
            int16_t sensor = g_grayScaleBlueless;
            int16_t target = GS_TARGET;

            if (state == ST_challenge_L) {
                turn = ltPid->compute(sensor, target);
            } else {
                turn = (-1) * ltPid->compute(sensor, target);
            }
        }
        /* 左右モータでロボットのステアリング操作を行う */
        pwm_L = forward - turn;
        pwm_R = forward + turn;

        leftMotor->setPWM(pwm_L);
        rightMotor->setPWM(pwm_R);
    }
    //printf("pwm_L=%d, pwm_R=%d, forward=%d\n", pwm_L,pwm_R,forward);
}

//　直角ターン用　左右の車輪に駆動にそれぞれ値を指定する sano
void ChallengeRunner::setPwmLR(int p_L,int p_R) {
    pwm_p_L = p_L;
    pwm_p_R = p_R;
}

ChallengeRunner::~ChallengeRunner() {
    _debug(syslog(LOG_NOTICE, "%08u, ChallengeRunner destructor", clock->now()));
}

Captain::Captain() {
    _debug(syslog(LOG_NOTICE, "%08u, Captain default constructor", clock->now()));
}

void Captain::takeoff() {
    /* 各オブジェクトを生成・初期化する */
    touchSensor = new TouchSensor(PORT_1);
    sonarSensor = new SonarSensor(PORT_2);
    colorSensor = new ColorSensor(PORT_3);
    gyroSensor  = new GyroSensor(PORT_4);
    leftMotor   = new Motor(PORT_C);
    rightMotor  = new Motor(PORT_B);
    tailMotor   = new Motor(PORT_D); // sano
    armMotor   = new Motor(PORT_A); //sano
    steering    = new Steering(*leftMotor, *rightMotor);
//    curTime =0; //sano
//    prevTime=0; //sano
    
    /* LCD画面表示 */
    ev3_lcd_fill_rect(0, 0, EV3_LCD_WIDTH, EV3_LCD_HEIGHT, EV3_LCD_WHITE);
    ev3_lcd_draw_string("EV3way-ET aflac2020", 0, CALIB_FONT_HEIGHT*1);
    
    observer = new Observer(leftMotor, rightMotor, armMotor, tailMotor, touchSensor, sonarSensor, gyroSensor, colorSensor);
    observer->freeze(); // Do NOT attempt to collect sensor data until unfreeze() is invoked
    observer->goOnDuty();
    limboDancer = new LimboDancer(leftMotor, rightMotor, tailMotor);
    seesawCrimber = new SeesawCrimber(leftMotor, rightMotor, tailMotor);
    lineTracer = new LineTracer(leftMotor, rightMotor, tailMotor);
    challengeRuuner = new ChallengeRunner(leftMotor, rightMotor, tailMotor);
    
    /* 尻尾モーターのリセット */
    //tailMotor->reset();
    
    ev3_led_set_color(LED_ORANGE); /* 初期化完了通知 */

    state = ST_takingOff;
    anchorWatch = new AnchorWatch(tailMotor);
    anchorWatch->goOnDuty();
    anchorWatch->haveControl();

    act_tsk(RADIO_TASK);
}

void Captain::decide(uint8_t event) {
    //syslog(LOG_NOTICE, "%08u, Captain::decide(): event %s received by state %s", clock->now(), eventName[event], stateName[state]);
    switch (state) {
        case ST_takingOff:
            switch (event) {
                case EVT_cmdStart_R:
                case EVT_cmdStart_L:
                case EVT_touch_On:
                    if (event == EVT_cmdStart_L || (event == EVT_touch_On && _LEFT)) {
                        state = ST_tracing_L;
                    } else {
                        state = ST_tracing_R;
                    }
                    syslog(LOG_NOTICE, "%08u, Departing...", clock->now());
                    
                    /* 走行モーターエンコーダーリセット */
                    leftMotor->reset();
                    rightMotor->reset();
                    
                    observer->reset();
                    
                    /* ジャイロセンサーリセット */
                    gyroSensor->reset();
                    ev3_led_set_color(LED_GREEN); /* スタート通知 */
                    
                    observer->freeze();
                    lineTracer->freeze();
                    lineTracer->haveControl();
                    
                    lineTracer->unfreeze();
                    observer->unfreeze();
                    syslog(LOG_NOTICE, "%08u, Departed", clock->now());
                   break;
                default:
                    break;
            }
            break;
        case ST_tracing_R:
            switch (event) {
                case EVT_sonar_On:
                    break;
                case EVT_sonar_Off:
                    //lineTracer->unfreeze();
                    break;
                case EVT_slalom_on:
                    state = ST_challenge_R;
                    line_trace_flg = false;
                    armMotor->setPWM(0);
                    tailMotor->setPWM(-100);
                    challengeRuuner->haveControl();
                    challengeRuuner->setPwmLR(20,20);
                    break;
                default:
                    break;
            }
            break;
        case ST_tracing_L:
            switch (event) {
                case EVT_sonar_On:
                    //lineTracer->freeze();
                    break;
                case EVT_sonar_Off:
                    //lineTracer->unfreeze();
                    break;
                case EVT_slalom_on:
                    state = ST_challenge_L;
                    line_trace_flg = false;
                    armMotor->setPWM(0);
                    tailMotor->setPWM(-100);
                    challengeRuuner->haveControl();
                    challengeRuuner->setPwmLR(20,20);
                    //challengeRuuner->freeze();
                    //printf("angleX=%d\n", observer->getDegree());
                    break;
                default:
                    break;
            }
            break;
        case ST_stopping_R:
        case ST_stopping_L:
            switch (event) {
                case EVT_dist_reached:
                    state = ST_landing;
                    line_trace_flg = false;
                    anchorWatch->haveControl(); // does robot stand still?
                    triggerLanding();
                    break;
                default:
                    break;
            }
            break;
        case ST_landing:
            break;
        default:
        break;
        case ST_challenge_L:
            switch (event) {
                case EVT_goto_obstcl:
                // printf("Degree=%d\n", observer->getDegree());
                //     if (observer->getDegree() > 0){
                //         challengeRuuner->setPwmLR(0,0);
                //     }else {
                //         challengeRuuner->setPwmLR(0,0);
                //     }
                    break;
                case EVT_slalom_go:
                    // challengeRuuner->setPwmLR(10,10);
                    // printf("degree1=%d\n", observer->getDegree());
                    // //clock->sleep(700); // wait a little
                    // printf("degree2=%d\n", observer->getDegree());
                    // if (observer->getDegree() > 0) {
                    //      challengeRuuner->setPwmLR(0,10);
                    // }else{
                    //      challengeRuuner->setPwmLR(10,0);
                    // }
                    // printf("distance1=%d\n", sonarSensor->getDistance());
                    // while (sonarSensor->getDistance() < 30 || sonarSensor->getDistance() > 45){
                    //     printf("distance2=%d\n", sonarSensor->getDistance());
                    //     clock->sleep(10); // wait a little
                    // }
                    // printf("distance3=%d\n", sonarSensor->getDistance());
                    // while(observer->getDegree() != 0){
                    //    printf("dgree=%d\n", observer->getDegree());
                    //    clock->sleep(10); // wait a little
                    // }
                    // // for (int i = 0; i < 300000; i++){
                    // //      clock->sleep(10); // wait a little
                    // //      //printf("distance=%d\n", sonarSensor->getDistance());
                    // // }
                    // // if (observer->getAccumAngleVl() > 0){
                    // //     printf("右\n");
                    // //     challengeRuuner->setPwmLR(-10,10);
                    // // } else{
                    // //     printf("左\n");
                    // //     challengeRuuner->setPwmLR(10,-10);
                    // // }
                    // // while (observer->getAccumAngleVl() != 0){
                    // //     printf("accumAngleVL=%d\n",observer->getAccumAngleVl());
                    // //     clock->sleep(10); // wait a little
                    // // }
                    // printf("方向修正完了\n");
                    // challengeRuuner->freeze();
                    // clock->sleep(5000); // wait a little
                    // challengeRuuner->unfreeze();
                    break;
                case EVT_slalom_avoid:
                    break;
                case EVT_turnCnr:
                    challengeRuuner->freeze();
                    clock->sleep(1000); // wait a little
                    challengeRuuner->unfreeze();
                    challengeRuuner->setPwmLR(100,0);
                    printf("ターンしています\n");
                    clock->sleep(735); // wait a little
                    printf("スリープ終了した\n");
                    //challengeRuuner->setPwmLR(0,0);
                    //printf("ターンをOffに\n");
                    challengeRuuner->freeze();
                    printf("回転後の停止\n");
                    clock->sleep(1000); // wait a little
                    challengeRuuner->unfreeze();
                    printf("動き出します\n");
                    line_trace_flg = true;
                    break;
                case EVT_turnb3:
                    challengeRuuner->haveControl(); // test
                    challengeRuuner->freeze();
                    clock->sleep(1000); // wait a little
                    challengeRuuner->unfreeze();
                    challengeRuuner->setPwmLR(10,-10);
                    //ソナーを回転しを見つける
                    for (int i = 0; i < 300000; i++){
                        //printf("dis_obj=%d,",sonarSensor->getDistance());
                        // if(sonarSensor->getDistance() > 180 && sonarSensor->getDistance() <250){
                        //     printf("物体を見つけた\n");
                        //     //ソナーセンサーの放射角度20度を補正
                        //     while(b3_aa < 20){
                                clock->sleep(10); // wait a little
                        //     }
                        //     lineTracer->freeze();
                        //     clock->sleep(500); // wait a little
                        //     lineTracer->unfreeze();
                        //     break;
                        // }
                    }
                    challengeRuuner->setPwmLR(30,30);
                    clock->sleep(10000); // wait a little
                    break;
                default:
                    break;
            }
        break;
    }
}

void Captain::triggerLanding() {
    syslog(LOG_NOTICE, "%08u, Landing...", clock->now());
    ER ercd = wup_tsk(MAIN_TASK); // wake up the main task
    assert(ercd == E_OK);
}

void Captain::land() {
    ter_tsk(RADIO_TASK);

    if (activeNavigator != NULL) {
        activeNavigator->goOffDuty();
    }
    leftMotor->reset();
    rightMotor->reset();
    
    delete anchorWatch;
    delete lineTracer;
    delete challengeRuuner; // sano
    delete seesawCrimber;
    delete limboDancer;
    observer->goOffDuty();
    delete observer;
    
    delete tailMotor; // sano
    delete armMotor; //sano
    delete rightMotor;
    delete leftMotor;
    delete gyroSensor;
    delete colorSensor;
    delete sonarSensor;
    delete touchSensor;
}

Captain::~Captain() {
    _debug(syslog(LOG_NOTICE, "%08u, Captain destructor", clock->now()));
}
