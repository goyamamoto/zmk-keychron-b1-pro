# Keychron B1 Pro 用 ZMK

Keychron B1 Pro US（PID `0x0711`）用のファームウェアです。Keychron 版 ZMK（Zephyr 3.2）ではなく、**upstream の ZMK と Zephyr 4.1** で動きます。**ZMK Studio** が使えます。含まれるのはオープンソースのコード（ZMK、Zephyr、このリポジトリ）だけなので、書き込み用のファームウェアをリリースで配布しています。ビルドは 2 種類あります。キーが刻印どおりのものと、日本語入力向けの機能（キーボード配列を日本語にしたホスト向けの US-JIS モード、スペース横の IME キー）を加えたものです。

English: [README.md](README.md)

概要:

- **ZMK Studio**: ブラウザから USB 経由でキーマップをその場で変更できます。ビルドや書き込みは不要です（[zmk.studio](https://zmk.studio)）。
- **最新の ZMK**: v0.4.0 リリース候補時点の ZMK `main` と Zephyr 4.1。バージョンを固定し、再現可能な形でビルドします。
- 2 種類のビルド: キーが刻印どおりの `keychron-b1-pro.uf2` と、次を加えた `keychron-b1-pro-usjis.uf2`。
  - **US-JIS モード**（Fn+Tab）: キーボード配列を日本語にした Windows で、`` ` ~ @ ^ & * ( ) _ = + [ { ] } \ | : ' " `` が US キーキャップの刻印どおりに入力されます。
  - **スペース横の IME キー**: タップで IME オフ／オン、長押しで Alt／Cmd。
  - **Caps Lock と左 Ctrl の入れ替え**。
- 接続スイッチ、Mac/Win スイッチ、LED、Fn+B の電池残量表示、充電は、純正ファームウェアと同じように動きます。
- **非対応**: 2.4 GHz 接続（upstream の ZMK には 2.4 GHz 対応がありません。2.4G 位置ではキーボードの電源が切れます）と Keychron Launcher。2.4 GHz と Launcher が使える Keychron 版 ZMK ベースのものは [goyamamoto/zmk-kb1-usjis](https://github.com/goyamamoto/zmk-kb1-usjis) にあります。

## 対応機種: まず PID を確認してください

Keychron は同じ B1 Pro の名前で、キーマトリクスの異なる複数の版を売っています。このファームウェアはそのうち一つだけに対応しています。別の版に書き込むと、キーが違う文字になったり効かなかったりします。

| USB PID | Keychron の版 | 配列 | このファームウェア |
| --- | --- | --- | --- |
| `0x0711` | B1 Pro US | ANSI | **対応** |
| `0x071a` | B1 Pro US「n」版 | ANSI | 非対応（キーマトリクスが異なる） |
| `0x0714` | B1 Pro | ANSI | 非対応 |
| `0x0712`, `0x071b`, `0x0713`, `0x071c` | UK 版、JIS 版 | ISO、JIS | 非対応 |

純正ファームウェアのときの PID の調べ方:

- Keychron Launcher: Settings → Device Info。
- macOS: `ioreg -p IOUSB -l -w0 | grep -A25 'Keychron B1 Pro@' | grep -E '"(idVendor|idProduct)"'`。値は 10 進数で、`1809` が `0x0711` です。
- Windows: デバイス マネージャー → キーボード → 詳細 → ハードウェア ID に `VID_3434&PID_0711` が含まれます。
- Linux: `lsusb` に `3434:0711` と出ます。

## 書き込み

1. 最新リリースから 2 つのうちどちらかをダウンロードします（自分でビルドする場合は[ビルド](#ビルド)を参照）。
   - `keychron-b1-pro.uf2`: キーは刻印どおり、日本語向けの機能なし。
   - `keychron-b1-pro-usjis.uf2`: US-JIS モード、IME キー、Caps Lock／左 Ctrl の入れ替えあり（[キー](#キー)を参照）。
2. キーボード裏の穴にあるリセットスイッチを押したまま USB をつなぎます。`NRF52BOOT` というドライブが現れます。
3. `.uf2` ファイルをそのドライブにコピーします。ドライブが消え、キーボードが再起動します。
4. 純正ファームウェアから移る場合は、純正が残した Bluetooth のペアリング情報を Fn+Shift+Esc の 10 秒長押しで消し、ペアリングし直してください（Fn+1 を 3 秒長押し）。

ブートローダーは書き換えないので、どのファームウェアからでも同じ手順で書き込めます。Keychron のファームウェアに戻すときは、Keychron 公式の B1 Pro 用ファームウェアを同じ手順で書き込みます。

USB ID は ZMK のもの（`1d50:615e`）なので、Keychron Launcher はこのキーボードを認識しません。

## キー

US キーキャップの配列に、Mac/Win スイッチで選ぶ Mac レイヤーと Win レイヤー、それぞれの Fn レイヤーがあります。

| キー | Mac | Win |
| --- | --- | --- |
| F 列（Fn なし） | 画面の明るさ、Mission Control、Launchpad、検索、ロック、メディア、音量 | F1–F12 |
| Fn + F 列 | F1–F12 | 画面の明るさ、タスクビュー、エクスプローラー、検索、ロック、メディア、音量 |
| Fn+1 … Fn+4 | Bluetooth プロファイル 1–4。3 秒長押しでそのプロファイルを消して再ペアリング | 同じ |
| Fn+Shift+Esc を 10 秒 | Bluetooth のペアリング情報をすべて消去（Fn+Esc だけなら Esc） | 同じ |
| Fn+B（押している間） | RGB LED で電池残量を表示 | 同じ |
| Fn+Del | ZMK Studio のロック解除 | 同じ |
| Fn+= | 検索 | 電卓 |
| Fn+I | Insert | Insert |
| Fn+\\ | スクリーンショット（Cmd+Shift+4） | 切り取り（Win+Shift+S） |
| Fn+右 Shift | 絵文字（Ctrl+Cmd+Space） | 絵文字（Win+.） |
| Fn+矢印 | Home、Page Up、Page Down、End | 同じ |
| Fn + スペース右のキー | 右 Ctrl | 右 Ctrl |

`keychron-b1-pro-usjis.uf2` では次のキーが異なります。

| キー | Mac | Win |
| --- | --- | --- |
| Fn+Tab | US-JIS モードのオン／オフ（効くのは Win のときだけ） | 同じ |
| スペース横のキー | タップ: 英数／かな（IME オフ／オン）。長押し: Cmd | タップ: 無変換／変換。長押し: Alt |
| Caps Lock の位置 | 左 Ctrl | 左 Ctrl |
| 左 Ctrl の位置 | Caps Lock | Caps Lock |

これらは [config/keymap-options.h](config/keymap-options.h) のオプション（`B1_USJIS`、`B1_IME_TAP`、`B1_SWAP_CTRL_CAPS`）で、`-usjis` のビルドは書かれた値のまま、刻印どおりのビルドはすべて無効にして使います。変えて再ビルドするか、ZMK Studio でキーを変えてください。Windows では Microsoft IME で、無変換に「IME-オフ」、変換に「IME-オン」を割り当てる設定が必要です（設定 → 時刻と言語 → 言語と地域 → 日本語 → Microsoft IME → キーとタッチのカスタマイズ）。macOS では設定は要りません。

## ZMK Studio

USB でつなぎ（接続スイッチはケーブル位置）、Web Serial に対応したブラウザで [zmk.studio](https://zmk.studio) を開き、Fn+Del でロックを解除します。Studio には B1 Pro の物理配列とすべてのレイヤーが表示され、変更はキーボードに保存されます。自由に使える予備のレイヤーが 2 つあります。ZMK の behavior に加え、このファームウェアの **Indicator**（電池残量表示）と、`-usjis` のビルドでは **US-JIS**（Toggle、On、Off）も選べます。Studio は USB 経由だけで使え、Bluetooth 経由では使えません。

配列の最後の 5 つ（キーの下に小さく描かれたもの）は、Mac/Win スイッチ、接続スイッチの 2 つの入力、充電器の信号です。変更しないでください。

## 接続スイッチと電源

スイッチは左から BT／ケーブル／2.4G です。出力先はスイッチの位置だけで決まり、ZMK の USB と Bluetooth の自動切り替えは使いません。

| 位置 | 出力 | 電源が切れる条件 |
| --- | --- | --- |
| BT | 選択中のプロファイルが接続中なら Bluetooth、それ以外は出力なし | 切れない |
| ケーブル | ホストが USB を構成したら USB、それ以外は出力なし | USB 給電がなくなって 5 秒後（純正と同じ） |
| 2.4G | 出力なし | この位置に 300 ms 置いたとき |

電源を切ると nRF52840 の System OFF になります。

| 切れる理由 | 起きるきっかけ |
| --- | --- |
| 2.4G 位置、USB なしのケーブル位置 | スイッチを動かす、USB を挿す |
| 電池電圧の低下: 3045 mV 未満（純正と同じしきい値）、USB 給電なしで 60 秒ごとに確認 | スイッチを動かす、USB を挿す |
| USB 給電なしで 2 時間操作なし（純正と同じ） | キー、スイッチを動かす、USB を挿す |

起きるとキーボードは再起動します。電源が切れている間に電流を流すのは、スイッチ入力のプルアップ抵抗と電池電圧の分圧抵抗だけです。

## LED

意味は純正ファームウェアに合わせています。

| LED | 表示 |
| --- | --- |
| Num Lock 以外すべて | 起動後 2.5 秒点灯 |
| 青（Bluetooth）、BT 位置のときだけ | ペアリング待ち: ゆっくり点滅（最長 60 秒）。再接続中: 速い点滅（最長 30 秒）。接続: 3 秒点灯 |
| Caps Lock | ホストが通知する Caps Lock の状態 |
| RGB | Fn+B を押している間: 電池残量。緑（70 % 以上）、青（30 % 以上）、赤（それ未満）。それ以外は、電池が少ないとき 1 分ごとに赤く 3 回点滅、充電中は赤、充電完了で緑（充電表示は BT とケーブル位置） |

## US-JIS モード

`keychron-b1-pro-usjis.uf2` の機能で、キーボード配列を日本語（106/109）にした Windows 向けです。Fn+Tab でオン／オフします。オンで OS スイッチが Win のとき、記号キーが US キーキャップの刻印どおりに入力されます。Shift+2 で `@`、`=` で `=`、Shift+; で `:` などです（[仕様書](docs/usjis-substitution.md)の表 C01–C20）。Mac 位置では置き換えません（macOS には不要です）。初期状態はオフで、状態は電源を切っても保持されます。キーを押している間に切り替えた場合は、すべてのキーを離したときに反映されます。モードを示す LED はありません。日本語配列のホストでは、オンなら `=` キーで `=`、オフなら `^` が入力されます。

複数のキーを同時に押したときの扱いは決まったルールに従い、キーや修飾キーが押されたままになることはありません。[仕様書](docs/usjis-substitution.md)と[設計](docs/usjis-architecture.md)を参照してください（どちらも英語）。置き換え表は [Keyboard Quantizer](https://github.com/sekigon-gonnoc/vial-qmk) の US キー／JIS OS 用キーオーバーライドの外から見える動作をもとに作りました。そのコード、表、コメントは使っていません。

## ビルド

必要なのは Git と Docker だけです。ツールチェーンは固定した `zmkfirmware/zmk-build-arm:4.1` イメージの中で動きます。Apple Silicon ではイメージはエミュレーションで動きます。

```sh
bash scripts/build-firmware.sh            # 固定したソースを取得（ネットワーク）してからビルド（オフライン）
bash scripts/build-firmware.sh build      # 取得済みのソースから再ビルド、ネットワーク不要
```

初回はイメージと固定したソースを `workspace/firmware/` にダウンロードします（数 GB）。既定を変えるときは、ビルド前に `config/keymap-options.h` や `config/keychron_b1_pro.keymap` を編集します。1 回の実行で 2 つのファームウェアをビルドします。出力は `build/firmware/` です。

- `keychron-b1-pro.uf2` と `keychron-b1-pro-usjis.uf2`（ほかに `.hex`、`.elf`、`.map`、それぞれの `.config` と `.dts`）
- `THIRD-PARTY-NOTICES.txt`: ファームウェアにリンクされた部品とそのライセンス。リンクマップから生成し、表記に含まれないライセンスのコードがリンクされるとビルドが失敗します。
- `build-info.json`: リポジトリのコミット、west の各プロジェクトのコミット、イメージのダイジェスト、ツールのバージョン、出力の SHA-256。

固定しているもの: ZMK `9ebbeff0a8b69a42f14aec022cdf16c7a107b9e0`、Zephyr `10ba6d0cb38bc3d258775d27982f707599320085`（v4.1.0+zmk-fixes）は [config/west.yml](config/west.yml)、イメージはダイジェストで [scripts/build-firmware.sh](scripts/build-firmware.sh) に書いてあります。ビルド日時はコミット時刻なので、同じコミットからは同じファイルができます。

## 仕組み

- **ボード**（`boards/keychron/keychron_b1_pro/`）: nRF52840（基板に 32 kHz 水晶がないため内蔵 RC 発振器を使用）、キーマトリクス、5 つの直接入力、LED、電池電圧の分圧、ZMK Studio 用の物理配列。
- **ダイオードのないマトリクス**（`src/kscan_gpio_matrix_nodiode.c`、`src/ghost_filter.c`）: 列は走査中だけ駆動し、その後放電してフローティングにします。ゴーストキー（長方形の 3 隅から 4 隅目が現れるもの）を除き、押されているキーは必ず離せます。
- **接続スイッチと電源**（`src/behavior_conn_switch.c`、`src/conn_policy.c`、`src/kb1_power.c`、`src/switch_waker.c`、`src/key_waker.c`）: 出力の決め方、ZMK の soft off、電源を切ったときと反対のレベルで起きるよう設定するウェイク入力。
- **起動時のスイッチ**（`src/early_events.c`）: ZMK Studio を有効にすると、ZMK のキーマップは初期化の最後にキーの割り当てを得ます。これはキー走査の開始より後なので、起動時にすでにオンのスイッチ（Win、BT）の入力が捨てられてしまいます。このような早すぎる入力を保留し、あとで再送します。
- **LED と表示**（`src/kb1_leds.c`、`src/led_logic.c`、`src/behavior_kb1_indicator.c`）。
- **US-JIS**（`src/usjis.c`、`src/usjis_resolver.c`、`src/behavior_usjis.c`）: ZMK の `hid_listener` の直前に置くリスナー。位置はビルド時と起動時に確認します。キーマップに `&usjis` behavior があるとき（`B1_USJIS`）だけビルドされます。

## テスト

```sh
bash scripts/run-host-tests.sh   # 純粋なロジックをホストで。ビルド後はデバイスツリーの検査も
bash scripts/run-zmk-tests.sh    # native_sim 上の ZMK テスト（Docker、ビルドのワークスペースを使用）
```

ホストテストは、ゴーストフィルタ（ダイオードのないマトリクスが実際に読む値を入力）、出力の決め方、LED のロジックを確かめます。ZMK テストは、ZMK アプリ全体をこのリポジトリのコードと一緒に ZMK の `native_sim` テストボード向けにビルドし、模擬のキー入力を与えて、送られるすべての HID レポートを期待値と比べます。対象は、US-JIS の表 C01–C20 と仕様書のシナリオ（`tests/zmk/generate.py` が仕様書から書き起こす）、Fn+Tab と IME キーを含む B1 Pro のレイヤー、ZMK Studio 有効時に起動時からオンのスイッチです。GitHub Actions が push のたびにビルドと両方のテストを実行します。

## リポジトリの構成

```text
.
├── .github/     ワークフロー（ビルドとテスト、リリース）、Dependabot
├── boards/      keychron_b1_pro ボード
├── config/      west マニフェスト、キーマップ、キーマップのオプション、設定
├── docs/        US-JIS の仕様書と設計（英語）
├── dts/         このリポジトリのドライバと behavior の devicetree バインディング
├── include/     ヘッダと dt-bindings
├── scripts/     ビルド、ライセンス表記、テストの実行
├── src/         ドライバ、behavior、モジュール
├── tests/       ホストテストと ZMK テスト
└── zephyr/      Zephyr モジュールの情報
```

## 不具合の報告

不具合の報告やプルリクエストは日本語でも英語でもかまいません。セキュリティの問題は非公開で報告してください（[SECURITY.md](SECURITY.md)）。

## ライセンス

このリポジトリは [MIT License](LICENSE) です。ビルドしたファームウェアには ZMK（MIT）、Zephyr（Apache-2.0）などの部品も含まれます。各リリースの `THIRD-PARTY-NOTICES.txt` にそのライセンスを記載しています。
