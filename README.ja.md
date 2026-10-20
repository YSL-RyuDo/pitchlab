# PitchLab

[English](README.md), [Korean](README.ko.md), [Japanese](README.ja.md)

オーディオコールバックとDSPワーカーを中心に構築したリアルタイムのマイク・ピッチ処理ツールです。

| プロジェクト情報 | 内容 |
|---|---|
| 開発状況 | C++システム開発事例 |
| 使用技術 | C++, JUCE, WASAPI, DSP |

[開発事例を見る](https://ysl-ryudo.github.io/ja/work/pitchlab/), [Portfolio](https://ysl-ryudo.github.io/ja/)

## ゲーム全体で担当した実装

JUCEと外部DSPを利用し、アプリの音声フロー・制御・UIを制作しました。目標音選択、入力調整、ゲート・バッファリング、録音・オフライン処理を担当しました。JUCEとSignalsmithのアルゴリズムは依存ライブラリとして明示し、自作とはしません。

**プレイの流れ**

音声デバイス選択 → 入力調整 → モニターとピッチ処理 → 音階・プリセット → 録音・オフライン処理

ソースリンクは確認した版に固定し、担当リンクは自分のマージ以外の変更を示します。共同ファイルは単独著作を意味しません。非公開ソースには閲覧権限が必要です。

### 入力調整と信号制御

入力調整、ノイズゲート、録音レベル、出力保護を実装しました。ピッチ変換ライブラリの呼出しに加え、その前後の信号を扱うマイクアプリの流れを構築しました。

[InputCalibration.h (main)](https://github.com/YSL-RyuDo/pitchlab/blob/603505b89cb8b0e39274f671aa515051acebd6e8/Source/InputCalibration.h), [担当変更 5bcbac3](https://github.com/YSL-RyuDo/pitchlab/commit/5bcbac396627611e5ec12c8384fdc0733d636f8c), [NoiseGate.h (main)](https://github.com/YSL-RyuDo/pitchlab/blob/603505b89cb8b0e39274f671aa515051acebd6e8/Source/NoiseGate.h), [担当変更 ecaf34c](https://github.com/YSL-RyuDo/pitchlab/commit/ecaf34c27b0b65439bd64ca7b47dad27d001ac0b)

### 目標ピッチと音選択

検出ピッチ、MIDI変換、許可音マスク、近傍音選択による目標音ロジックを実装しました。ボーカル鍵盤で利用者が音を選べます。ピッチ変換自体には組み込んだDSP依存を利用します。

[PitchEngine.h (main)](https://github.com/YSL-RyuDo/pitchlab/blob/603505b89cb8b0e39274f671aa515051acebd6e8/Source/PitchEngine.h), [担当変更 5bcbac3](https://github.com/YSL-RyuDo/pitchlab/commit/5bcbac396627611e5ec12c8384fdc0733d636f8c), [VocalKeyboard.h (main)](https://github.com/YSL-RyuDo/pitchlab/blob/603505b89cb8b0e39274f671aa515051acebd6e8/Source/VocalKeyboard.h), [担当変更 f37751d](https://github.com/YSL-RyuDo/pitchlab/commit/f37751d1d22e348f90ad6a1ef9437526ff157709)

### ストリーミング、録音とオフライン処理

コールバックとワーカー間のバッファ、録音、ファイルのオフライン処理を実装しました。ライブモニターは音声期限を守る必要があり、オフライン処理はそのコールバック外で実行できます。バッファの遅延・同期コストは後述します。

[AudioEngine.h (main)](https://github.com/YSL-RyuDo/pitchlab/blob/603505b89cb8b0e39274f671aa515051acebd6e8/Source/AudioEngine.h), [担当変更 fe484c8](https://github.com/YSL-RyuDo/pitchlab/commit/fe484c8be966d0f91203373fd5f9eddd060ce5d0), [Recorder.h (main)](https://github.com/YSL-RyuDo/pitchlab/blob/603505b89cb8b0e39274f671aa515051acebd6e8/Source/Recorder.h), [担当変更 a8b3d93](https://github.com/YSL-RyuDo/pitchlab/commit/a8b3d93b8bba5cc75ee4124d7b143c226a4f4e50), [OfflineRenderer.h (main)](https://github.com/YSL-RyuDo/pitchlab/blob/603505b89cb8b0e39274f671aa515051acebd6e8/Source/OfflineRenderer.h), [担当変更 5bcbac3](https://github.com/YSL-RyuDo/pitchlab/commit/5bcbac396627611e5ec12c8384fdc0733d636f8c)

## 担当した内容

専用DSPワーカーと容量制限付きFIFOで、音程解析・処理・録音をデバイスコールバックから分離しました。

WASAPIの入力形式処理を調査し、入力レベル調整、出力保護、診断機能を実装しました。

DSP、ストリーミング、入力調整、ノイズゲート、WAV出力の回帰検査を追加しました。

## 貢献の根拠

[Audio callback / 오디오 콜백 / 音声コールバック](https://github.com/YSL-RyuDo/pitchlab/blob/5bcbac396627611e5ec12c8384fdc0733d636f8c/Source/AudioEngine.h)

[Development history / 개발 이력 / 開発履歴](https://github.com/YSL-RyuDo/pitchlab/commits/main/?author=YSL-RyuDo)

コミットは記載した変更の根拠であり、貢献率の点数ではありません。外部素材・チームメンバーの貢献は区別します。

## 実装と設計

マイク処理はリアルタイム入力とピッチ分析・補正・録音を組み合わせます。コールバックと処理ワーカー間に容量を制限した受け渡しを設けました。

DSPワーカーと容量制限付きFIFOで処理・録音をコールバックから分離し、WASAPI入力形式を調査しました。入力校正、診断、WAV回帰検証も追加しました。

## コードの動作と実装構造の理由

確認した実装が扱う制約と利点・代償を説明します。設計効果の説明はコード分析であり、比較する全代案を当時試作したという意味ではありません。担当リンクは共同制作内の変更を示し、ソースの閲覧権限は維持します。

### オーディオコールバックからDSPを分ける理由

**実装の流れ**

AudioEngineは固定容量の入出力FIFOでオーディオコールバックとDSPワーカー間のサンプルを渡します。コールバックは入力検査、サンプル転送、準備済み出力を書き込みます。オーバーフロー、出力不足、非有限値ではストリーム異常を記録し、モニター出力を停止します。

**この構造が必要な理由**

デバイスには周期的な出力期限があり、ピッチ処理の作業量は変わり得ます。分離するとコールバックの仕事を絞り、バッファ不足を不正音声の出力継続ではなく明示的な異常として扱えます。固定容量はメモリ量とあふれ時の動作も明確にします。

**代案と現在の実装の折合い**

コールバック内DSPはスレッド間バッファと遅延を省けますが、可変処理が音声期限に直接影響します。別ワーカーはバッファと同期を加えてコールバックの仕事を絞ります。適切さはスレッド数より処理時間と出力不足の測定で判断します。

**利点・代償と検証範囲**

バッファはスケジューリングの揺れを吸収する代わりに遅延を増やします。異常時の無音化は不正データの継続出力を防ぎますが、モニターを中断するため復帰経路が必要です。構造だけでWindows上の実測遅延やハードリアルタイム性を保証するものではありません。

[pitchlab/AudioEngine.h](https://github.com/YSL-RyuDo/pitchlab/blob/e191ed10d7d2cc8fc7dabd6192936b1ee3d4ea19/Source/AudioEngine.h)

### ピッチ補正の目標音選択

**実装の流れ**

PitchEngineは周波数とMIDIを変換し、許可音を12ビットマスクで表し、近傍から最も近い許可音を探します。利用者は音階やボーカル鍵盤で許可音を変えられます。変換段には統合DSPライブラリを使います。

**この構造が必要な理由**

固定半音シフトと音階への補正は別の処理です。目標選択を純粋関数にするとマイクI/Oから方針を分離し、デバイスなしで境界条件を検査できます。

**代案と現在の実装の折合い**

半音丸めは単純ですが全半音を許可します。音階制限や任意マスクで音楽的制御ができます。最近傍方針は説明しやすい一方、音境界での揺れを抑える時間方向の処理も必要です。

**利点・代償と検証範囲**

確認関数では空マスクは丸めた音へ戻ります。検出信頼度、無声音、遷移、聴感上の歪みは別の検証対象です。この方針は基礎ピッチ変換アルゴリズムの独自開発を意味しません。

[pitchlab/PitchEngine.h (main)](https://github.com/YSL-RyuDo/pitchlab/blob/603505b89cb8b0e39274f671aa515051acebd6e8/Source/PitchEngine.h), [pitchlab/VocalKeyboard.h (main)](https://github.com/YSL-RyuDo/pitchlab/blob/603505b89cb8b0e39274f671aa515051acebd6e8/Source/VocalKeyboard.h)

## 実行・コードの確認

C++/JUCEビルドは[CMakeLists.txt](CMakeLists.txt)、デバイス・録音設定は既存の詳細資料を参照してください。

## 開発・検証範囲

ゲームサーバーベンチマークではなく補助的なC++技術プロジェクトです。合成信号・ファイル検査は全マイク・ヘッドホンでの品質を証明しません。アーティスト名のプリセットは公式設定ではなく近似で、個人の音声録音は含めません。

## 詳細資料

[既存の詳細開発・利用・検証資料（韓国語）](docs/DEVELOPMENT_REFERENCE.ko.md)

[TEST-RESULTS.txt](TEST-RESULTS.txt)

[LIVE-VOICE-TEST-RESULTS.txt](LIVE-VOICE-TEST-RESULTS.txt)

[ThirdParty/JUCE-LICENSE.md](ThirdParty/JUCE-LICENSE.md)

## 関連リポジトリ

[pitchlab](https://github.com/YSL-RyuDo/pitchlab), 非公開・アクセス権が必要
