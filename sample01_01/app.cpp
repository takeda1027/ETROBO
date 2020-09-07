// tag::walker_def[]
#include "app.h"
#include "Walker.h"
#include "Clock.h"
using namespace ev3api;

Walker walker;                  // walkerのインスタンスを作成
Clock clock;

void walker_task(intptr_t exinf) {// 周期的に起動される関数
  walker.run();               // 走行
  ext_tsk();
}
// end::walker_def[]
// tag::main_task[]
void main_task(intptr_t unused) {
  const uint32_t duration = 100*1000; // 100msec毎にボタンを検出する

  walker.init(); //walkerの初期化

  sta_cyc(WALKER_CYC); // walkerタスク起動の周期タスクを起動させる

  while (!ev3_button_is_pressed(LEFT_BUTTON)) { //ボタンが押されるまで繰り返す
      clock.sleep(duration);   // 指定した時間を待つ
  }
  stp_cyc(WALKER_CYC); // walkerタスクの周期を止める
  walker.terminate(); // walkerの動作を止める
  ext_tsk(); // メインタスクの処理を終了
}
// end::main_task[]
