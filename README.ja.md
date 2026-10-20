# PitchLab

[English](README.md), [Korean](README.ko.md), [Japanese](README.ja.md)

オーディオコールバックとDSPワーカーを中心に構築したリアルタイムのマイク・ピッチ処理ツールです。

| プロジェクト情報 | 内容 |
|---|---|
| 開発状況 | C++システム開発事例 |
| 使用技術 | C++, JUCE, WASAPI, DSP |

[開発事例を見る](https://ysl-ryudo.github.io/ja/work/pitchlab/), [Portfolio](https://ysl-ryudo.github.io/ja/)

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

| 領域 | 実装 |
|---|---|
| デバイスコールバック | 容量制限付きFIFOで入出力を受け渡し |
| DSPワーカー | ピッチ解析・処理・録音 |
| 検証 | 入力補正・WASAPI形式・診断・WAV回帰確認 |

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
