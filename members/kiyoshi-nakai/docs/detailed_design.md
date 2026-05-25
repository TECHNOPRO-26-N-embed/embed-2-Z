# 詳細設計書 — 組込み開発実習

<!-- 作成者: あなたの名前 / 日付: YYYY-MM-DD / グループ: 〇-〇 -->

> **このドキュメントの目的**
> 基本設計書（basic_design.md）で「**どのような構造で作るか**」を決めました。
> この詳細設計書では「**各処理を具体的にどう実装するか**」を決めます。
> 書き終わったとき、**コードの骨格がほぼ完成している**状態を目指してください。

> [!NOTE]
> **V字モデルにおける位置づけ**
> 詳細設計書 ←→ **単体テスト**（関数・部品ごとのテスト）が対応します。
> 「この関数が正しく動くか」の確認は Section 5 の単体テスト仕様書で計画します。
> ※ 必須機能全体が動くかの「結合テスト」は基本設計書（Section 6）に記載します。

---

## 0. 基本設計書との接続確認

| 項目 | basic_design.md から転記 |
|:--|:--|
| 作品タイトル | パックマンデモ |
| 状態の種類（1-2 状態遷移から） | 初期化中、待機中、移動中 |
| 実装する関数の数（2-2 関数一覧から） | 8個 |
| グローバル変数の合計バイト数（2-1 SRAM確認から） | 約23B |

---

## 1. グローバル変数・定数の設計

> ※ 基本設計書（2-1 データ設計）をもとに、**型と初期値まで**決めます。
> ここで設計した変数は、この後の関数設計でそのまま使います。

```
【ピン定義】（basic_design.md 3-1 から転記）
  PIN_JOY_VRX   = A0   // ジョイスティックX軸
  PIN_JOY_VRY   = A1   // ジョイスティックY軸
  PIN_JOY_SW    = 2    // ジョイスティックSW（INPUT_PULLUP）
  PIN_LCD_RS    = 7    // LCM1602A RS (4pin)
  PIN_LCD_EN    = 8    // LCM1602A E (6pin)
  PIN_LCD_D4    = 9    // LCM1602A D4 (11pin)
  PIN_LCD_D5    = 10   // LCM1602A D5 (12pin)
  PIN_LCD_D6    = 11   // LCM1602A D6 (13pin)
  PIN_LCD_D7    = 12   // LCM1602A D7 (14pin)
  PIN_LCD_RW    = GND  // LCM1602A RW (5pin, 書き込み専用でGND固定)
  PIN_LCD_VO    = VR   // LCM1602A VO (3pin, 10k可変抵抗中央)
  PIN_LCD_A     = 5V   // LCM1602A A (15pin, バックライト+)
  PIN_LCD_K     = GND  // LCM1602A K (16pin, バックライト-)
  PIN_5V        = 5V   // 電源
  PIN_GND       = GND  // GND


【状態管理】（basic_design.md 1-2 の状態名から転記）
  currentState  : uint8_t = 0   // 0:初期化中 1:待機中 2:移動中 3:色変更中
  isInitDone    : bool = false  // 初期化完了フラグ（trueで初期化完了）
  colorMode : uint8_t = 0      // 表示色モード（0:通常 1:色A 2:色B）
  currentCharacterIndex: uint8_t = 0 // キャラクター切替インデックス（0:パックマン, 1:ゴースト, 2:代替キャラ）

【タイマー（millis()用）】（basic_design.md 2-3 から転記）
  lastInputMillis    : unsigned long = 0   // 入力監視タイマー
  lastDebounceMillis : unsigned long = 0   // SWデバウンスタイマー
  lastIrMillis      : unsigned long = 0   // IR受信監視タイマー

【センサー・入力値】（basic_design.md 2-1 から転記）
  joyXRaw   : int  = 512   // ジョイスティックX軸生値
  joyYRaw   : int  = 512   // ジョイスティックY軸生値
  joyButtonPressed : bool = false // SW押下状態（デバウンス後）
  pacmanX   : uint8_t = 0  // パックマンX座標（0-15）
  pacmanY   : uint8_t = 0  // パックマンY座標（0-1）
  direction : uint8_t = 0  // 向き（0:右 1:左 2:上 3:下）
  irCode    : unsigned long = 0 // 受信した赤外線コード値
  colorMode : uint8_t = 0      // 表示色モード（0:通常 1:色A 2:色B）

【ジョイスティックしきい値・ヒステリシス】
  JOY_CENTER      : const int = 512   // 中央値（アナログ入力の中央値）
  JOY_THRESHOLD   : const int = 100   // 入力ありと判定するしきい値（例: ±100）
  JOY_HYSTERESIS  : const int = 20    // ヒステリシス幅（例: ±20）

【その他のフラグ・カウンター】
  // 必要に応じて追加
```

---

## 2. 各関数の詳細設計

> ※ 基本設計書（2-2 関数一覧）で定義した各関数の「中身」を設計します。
> **疑似コード**（日本語＋処理の流れ）で書いてください。実際のC++コードは書かなくてOKです。

---

### `setup()` — 初期化処理

```
【処理の流れ】
1. ピンモードを設定する
   - PIN_BUTTON  → INPUT_PULLUP
   - PIN_LED_*   → OUTPUT
   - PIN_BUZZER  → OUTPUT

2. ライブラリの初期化（使うものだけ）
   - 例: lcd.begin(16, 2)
   - 例: servo.attach(PIN_SERVO)

3. Serial.begin(9600)（デバッグ用）

4. 起動確認（任意）: 緑LEDを1秒点灯して消灯
```

**↓ 自分の setup() を設計してください**
```
【処理の流れ】
1. ジョイスティックのピンモードを設定する
  - PIN_JOY_VRX, PIN_JOY_VRY → アナログ入力（デフォルト）
  - PIN_JOY_SW → INPUT_PULLUP
2. LCDライブラリの初期化
  - lcd.begin(16, 2)（パラレル接続）
  - LCM1602Aは4bitモードでRS/E/D4-D7を使用し、RWはGND固定
  - VOは10k可変抵抗でコントラスト調整し、A/Kはバックライト配線
  - 必要ならカスタム文字登録
  - 起動時の"Init..."表示を確認し、失敗時はエラー表示・isInitDone=false
  - 成功時はisInitDone=true
3. Serial.begin(9600)（デバッグ用）
4. LCDに"Init..."を一瞬表示
5. 変数初期化（座標・状態・タイマー）
```

---


### `readIR()` — Remote Controlの赤外線コードを取得する

**basic_design.md 2-2 との対応：** IR Receiver Moduleから赤外線コードを読み取る

**引数：** なし

**戻り値：** unsigned long（受信コード、未受信時は0）

```
【処理の流れ】
1. IR受信ライブラリで信号を監視
2. 新しい信号を受信した場合のみirCodeを更新
3. 受信がなければ0を返す
4. lastIrMillisで50ms周期制御

【エラー・異常ケース】
- 未登録コードやノイズは無視
```

---

### `updateColorByRemote()` — IRコードに応じて色モードを切り替える

**basic_design.md 2-2 との対応：** IRコード値に応じてcolorModeを更新し、LCD表示に反映

**引数：** unsigned long irCode

**戻り値：** void

```
【処理の流れ】
1. irCodeが登録済みの色切替コードか判定
2. 該当する場合はcolorModeを更新（例: 0=通常, 1=色A, 2=色B）
3. colorModeに応じてLCD表示内容（キャラの表示パターンや記号）を切り替え
4. 未登録コードや0の場合は何もしない
5. 色変更後はcurrentState=1または2に復帰

【エラー・異常ケース】
- LCD通信失敗時はエラー表示
- 無効なコードは無視
```

> ※ loop() は「状態ごとに何をするか」だけ書く。細かい処理は各関数に任せる。

```
【処理の流れ】

＜毎ループ実行すること＞
  - 入力を読む（readButton(), readSensor() などを呼ぶ）
  - 現在時刻を取得: now = millis()

＜currentState が 0（待機中）のとき＞
  - センサー値を監視する
  - 検知条件を満たしたら → currentState = 1

＜currentState が 1（動作中）のとき＞
  - メイン処理を行う
  - 終了条件を満たしたら → currentState = 2

＜currentState が 2（完了）のとき＞
  - 完了表示をする
  - リセットボタンが押されたら → currentState = 0

＜currentState が 3（エラー）のとき＞
  - エラー表示をする / リセットを待つ
```

**↓ 自分の loop() を設計してください**
```

【処理の流れ】

＜毎ループ実行すること＞
  - ジョイスティック入力を読む（readJoystick()）
  - 必要に応じてSW入力も読む（readButton()）
  - IR信号を読む（readIR()）
  - 現在時刻を取得: now = millis()
  - 状態遷移制御: updateState()

＜currentState が 0（初期化中）のとき＞
  - LCDに"Init..."を表示
  - isInitDone==true なら currentState = 1 に遷移
  - isInitDone==false ならエラー表示・リトライや停止処理

＜currentState が 1（待機中）のとき＞
  - 100ms周期でジョイスティック入力を監視
  - 入力があれば currentState = 2
  - IR入力があれば currentState = 3
  - LCDにパックマンを静止表示（colorMode反映）

＜currentState が 2（移動中）のとき＞
  - 100ms周期でジョイスティック方向を判定し、座標を更新（updateHorizontalMove(), updateVerticalMove()）
  - IR入力があれば currentState = 3
  - LCDにパックマンを描画（drawPacman()、colorMode反映）
  - 端到達または一定時間入力なしで currentState = 1

＜currentState が 3（色変更中）のとき＞
  - updateColorByRemote()でcolorModeを更新
  - LCDに新しい色状態を反映
  - 色反映完了後、currentState = 1または2に復帰
```

---

### （関数ごとに以下のブロックをコピーして追加してください）

> ※ 基本設計書 2-2 の関数一覧に記載した関数を1つずつ設計します。

---


---

### `readJoystick()` — ジョイスティックX/Yの生値を取得する

**basic_design.md 2-2 との対応：** ジョイスティックX/Yのアナログ値をA0/A1から読み取る

**引数：** なし

**戻り値：** 構造体またはグローバル変数に格納（joyXRaw, joyYRaw）

```
【処理の流れ】
1. analogRead(PIN_JOY_VRX)でjoyXRawを更新
2. analogRead(PIN_JOY_VRY)でjoyYRawを更新
3. X/Yそれぞれについて：
   - JOY_CENTER±JOY_THRESHOLDを超えたら「入力あり」と判定
   - しきい値をまたぐときのみ移動フラグを立てる（JOY_HYSTERESISでバタつき防止）
   - 例：JOY_CENTER+JOY_THRESHOLD+JOY_HYSTERESISを超えたら右移動、
     JOY_CENTER-JOY_THRESHOLD-JOY_HYSTERESISを下回ったら左移動
   - しきい値内（±JOY_THRESHOLD）では移動なし

【エラー・異常ケース】
- 0付近や1023付近で固定されていれば断線・異常値として無視
```

---

### `readButton()` — チャタリング処理済みのボタン状態を返す

**basic_design.md 2-2 との対応：** ジョイスティックSWのデジタル値をD2から読み、デバウンス処理を行う

**引数：** なし

**戻り値：** bool（押下時true）

```
【処理の流れ】
1. digitalRead(PIN_JOY_SW)で現在値を取得
2. 前回値と異なればlastDebounceMillisを更新
3. 50ms以上変化がなければ確定値としてjoyButtonPressedを更新
4. 押下時trueを返す

【エラー・異常ケース】
- チャタリングで50ms未満の変化は無視
```

---

### `updateState()` — 入力に応じて状態を遷移させる

**basic_design.md 2-2 との対応：** 入力値やタイマーに応じてcurrentStateを変更

**引数：** なし

**戻り値：** void

```
【処理の流れ】
1. currentState=0（初期化中）→初期化完了で1へ
2. currentState=1（待機中）→ジョイスティック入力ありで2へ、IR入力ありで3へ
3. currentState=2（移動中）→端到達または入力停止で1へ、IR入力ありで3へ
4. currentState=3（色変更中）→色反映完了で1または2へ

【エラー・異常ケース】
- 状態が不正な場合は0へ戻す
```

---

### `updateHorizontalMove()` — X軸値から左右移動を判定して座標更新

**basic_design.md 2-2 との対応：** X軸値がしきい値を超えたらpacmanXを±1する

**引数：** int joyX

**戻り値：** void

```
【処理の流れ】
1. joyXRawが中央値から±しきい値を超えたら左右移動判定
2. pacmanXを範囲内で±1更新
3. 境界（0,15）を超えないよう制限

【エラー・異常ケース】
- 連続入力時の多重移動を防ぐため、一定時間ごとにのみ判定
```

---

### `updateVerticalMove()` — Y軸値から上下移動を判定して座標更新

**basic_design.md 2-2 との対応：** Y軸値がしきい値を超えたらpacmanYを±1する

**引数：** int joyY

**戻り値：** void

```
【処理の流れ】
1. joyYRawが中央値から±しきい値を超えたら上下移動判定
2. pacmanYを範囲内で±1更新
3. 境界（0,1）を超えないよう制限

【エラー・異常ケース】
- 連続入力時の多重移動を防ぐため、一定時間ごとにのみ判定
```

---

### `drawPacman()` — 現在座標・色モードでキャラクターを描画する

**basic_design.md 2-2 との対応：** pacmanX, pacmanY, directionに応じてLCDに描画

**引数：** uint8_t x, uint8_t y

**戻り値：** void

```
【処理の流れ】
1. lcd.setCursor(x, y)でカーソル移動
2. colorModeに応じてキャラクターの表示パターンや記号を切り替え（例：通常=●、色A=○、色B=★など）
3. lcd.write(キャラクター文字コード)
4. 必要に応じて前回位置を消去

【エラー・異常ケース】
- LCD通信失敗時はエラー表示
```

---

### `updateDisplay()` — 現在のstateと座標に応じてLCDを更新する

**basic_design.md 2-2 との対応：** 状態ごとにLCD表示内容を切り替える

**引数：** int state

**戻り値：** void

```
【処理の流れ】
1. currentState=0: "Init..."表示
2. currentState=1: パックマン静止表示
3. currentState=2: パックマン移動表示
4. 必要に応じて補助情報も表示

【エラー・異常ケース】
- LCD通信失敗時はエラー表示
```

---

### `toggleCharacter()` — SW押下で表示キャラを切り替える

**basic_design.md 2-2 との対応：** SW押下時にパックマン/他キャラを切り替え

**キャラ切替仕様詳細：**
- 切替対象キャラ：パックマン、ブランク（空白）、ゴースト（例：2種）
- 切替順序：パックマン → ゴースト1 → ゴースト2 → パックマン …（循環）
- 初期値：パックマン
- 切替動作：SW押下ごとに次のキャラに切り替え、LCDに即時反映
- キャラはカスタム文字（LCD CGRAM）で定義し、配列で管理
- 現在キャラのインデックスをグローバル変数で保持

**引数：** bool pressed

**戻り値：** void

```
【処理の流れ】
1. joyButtonPressedがtrueになったらcurrentCharacterIndex++
2. currentCharacterIndexが配列長を超えたら0に戻す
3. LCDに新キャラ（currentCharacterIndexで指定）を描画
4. 切替時にカスタム文字定義が必要なら再登録

【エラー・異常ケース】
- 切替時にLCD通信失敗した場合は元キャラに戻す
```

---

## 3. 重要ロジックの詳細設計

### 3-1. チャタリング防止（デバウンス処理）

> ※ ボタンを使う場合は必ず設計してください。

```
【考え方】
  ボタンが押されたとき、50ms 以内の連続入力は「同じ1回の押下」として無視する。

【処理の流れ】
  1. ボタンのデジタル値を読む（digitalRead）
  2. 前回確定した時刻（lastDebounceTime）からの経過時間を計算する
  3. 経過時間 < DEBOUNCE_DELAY（例: 50ms）→ 無視する
  4. 経過時間 ≥ DEBOUNCE_DELAY → ボタンの状態として確定する
  5. lastDebounceTime を更新する

【必要な変数（Section 1 に追加済みか確認）】
  lastDebounceTime : unsigned long   // 前回確定した時刻
  DEBOUNCE_DELAY   : const int = 50  // チャタリング判定時間（ms）
```

---

### 3-2. millis() を使ったタイマー管理

```
【考え方】
  「前回実行した時刻」を記録しておき、「今の時刻 − 前回時刻 ≥ 周期」なら実行する。

【処理の流れ（例: LED点滅）】
  1. now = millis()
  2. now - lastMillis_LED >= LED_INTERVAL かどうか確認
  3. 条件を満たした場合: LEDのON/OFFを切り替え、lastMillis_LED = now
  4. 条件を満たさない場合: 何もしない（次のループで再チェック）

【自分のシステムで millis() を使う処理】
  ・ジョイスティック入力の周期監視（100msごとに移動判定）
  ・SWデバウンス判定（50msごとに状態確定）
  ・LCD表示更新（100msごとに再描画）
```

---

### 3-3. その他の重要ロジック（任意）

> **【任意】** 複雑なロジックがある場合のみ記入してください。
> 例：「距離に応じたLED点灯パターン」「ゲームの衝突判定」「温度の閾値判定」

```
【処理の流れ】
1.
2.
3.

【入力値と出力値の関係】

```

---

## 4. デバッグ出力計画（任意）

> **【任意】** 関数設計（Section 2）と並行して記入すると効果的です。
> 「動かない」ときに何を確認すればいいかを事前に計画しておきます。
> 実装後は不要な Serial.println() を削除すること。

| No | 確認したい内容 | 挿入する関数 | Serial.println の内容例 |
|:---|:---|:---|:---|
| 1 | センサー値が正しく取れているか | `readSensor()` | `Serial.println(sensorValue);` |
| 2 | 状態遷移が正しく起きているか | `loop()` | `Serial.println(currentState);` |
| 3 | チャタリング処理が効いているか | `readButton()` | `Serial.println("btn confirmed");` |
| 4 |  |  |  |

---

## 5. 単体テスト仕様書（V字モデル：詳細設計 ↔ 単体テスト）

> ※ 各関数・部品が「単体で正しく動くか」を確認するテスト項目を設計します。
> 「実際の結果」欄は実装後に記入します。

### 5-1. 入力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | readButton() | タクトスイッチを1回押す | true が返る | | [ ] |
| 2 | readButton() | スイッチを素早く2回押す | 1回分だけ true になる | | [ ] |
| 3 | readSensor() | センサーを正常範囲で使う | 仕様範囲内の値が返る | | [ ] |
| 4 | readSensor() | センサーを遮蔽・範囲外に向ける | 誤動作しない | | [ ] |
| 5 | readIR() | Remote Controlのボタンを押す | 登録済みコード時のみ値が返る | | [ ] |
| 6 | updateColorByRemote() | 登録済みIRコードを渡す | colorModeが正しく切り替わる | | [ ] |

### 5-2. 出力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | updateOutput(0) | state=0（待機中）を渡す | 緑LEDが点滅する | | [ ] |
| 2 | updateOutput(1) | state=1（動作中）を渡す | 赤LEDが点灯、ブザーが鳴る | | [ ] |
| 3 | drawPacman() | colorModeを切り替えて描画 | LCD上のキャラ表示が変化する | | [ ] |

### 5-3. タイミング・並行動作テスト

| No | テスト内容 | テスト手順 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | delay()による処理停止がないか | LED点滅中にボタンを押す | ボタン入力が無視されない | | [ ] |
| 2 | millis()タイマーの周期精度 | 点滅をストップウォッチで確認 | 設計した周期（例:500ms）通りに点滅 | | [ ] |

---

## 6. AIレビュー記録

> グループレビューの前に必ず実施してください。

### Q1: 実装上の問題確認

> 「この詳細設計書に書いた関数と処理フローをもとに Arduino でコードを書きます。バグになりやすい箇所・処理の抜け・型の問題はありますか？」

**AIの回答（要約）：**

**対応した内容：**

---

### Q2: 単体テスト仕様の確認

> 「Section 5 の単体テスト仕様書で、各関数の動作が正しく検証できていますか？テストが不足している項目や、境界値テストが必要な箇所を教えてください。」

**AIの回答（要約）：**

**対応した内容：**

---

## 7. グループレビュー記録

### 7-1. 指摘一覧

| No | 指摘内容 | 指摘者 | 対応 |
|:---|:---|:---|:---|
| 1 |  |  |  |
| 2 |  |  |  |
| 3 |  |  |  |

### 7-2. レビューを受けて変更した点

-
-

---

*初版: YYYY-MM-DD / AIレビュー: YYYY-MM-DD / グループレビュー後更新: YYYY-MM-DD*
