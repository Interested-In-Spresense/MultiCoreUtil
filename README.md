# MultiCoreUtil

Spresense マルチコア開発向けユーティリティライブラリです。  
Utility library for Sony Spresense multi-core development.

------------------------------------------------------------------------

## 概要 / Overview

`MultiCoreUtil` は、Sony Spresense のマルチコア開発を簡単にするためのユーティリティライブラリです。  
`MultiCoreUtil` provides utility classes for easier multi-core development on Sony Spresense.

Spresense の `MP` (MultiProcessor) ライブラリを利用したアプリケーション向けに、

- 共有メモリ管理
- コア間データ転送
- 同期制御

などを簡単に扱える機能を提供します。

It provides helper features for applications using the Spresense `MP` (MultiProcessor) library, including:

- shared memory management
- inter-core data transfer
- synchronization utilities

------------------------------------------------------------------------

## 内容 / Features

現在、以下の機能を含みます。  
Currently includes the following components.

### SharedMemoryAllocator

共有メモリ領域をまとめて確保・管理するユーティリティです。  
Utility for allocating and managing shared memory regions.

主な用途：

- MainCore / SubCore 間共有メモリ
- センサデータ共有
- 音声バッファ共有
- フレームバッファ共有

Typical use cases:

- shared memory between MainCore and SubCore
- sensor data sharing
- audio buffer sharing
- frame buffer sharing

------------------------------------------------------------------------

### InterCoreRingBuffer

コア間通信向けリングバッファです。  
Ring buffer for inter-core communication.

以下の特徴を持ちます。

- 固定長
- ノンブロッキング
- 型安全
- テンプレートベース

Features:

- fixed-size
- non-blocking
- type-safe
- template-based implementation

リアルタイム処理やセンサデータ転送に向いています。  
Suitable for realtime processing and sensor data transfer.

#### 使用例 / Example

```cpp
struct SensorData {

  float acc[3];
  float gyro[3];
};

InterCoreRingBuffer<SensorData, 128> ringBuf;
```

------------------------------------------------------------------------

### MultiCoreSpinLock

軽量スピンロック実装です。  
Lightweight spin lock implementation.

共有データの保護や、複数コア間の同期制御に利用します。  
Used for shared data protection and synchronization across cores.

------------------------------------------------------------------------

## 必要環境 / Requirements

- Sony Spresense
- Arduino IDE
- Spresense Arduino SDK
- MP (MultiProcessor) library

------------------------------------------------------------------------

## インストール / Installation

Arduino の libraries フォルダへ clone してください。  
Clone this repository into your Arduino libraries folder.

```bash
cd ~/Arduino/libraries
git clone https://github.com/Interested-In-Spresense/MultiCoreUtil.git
```

Arduino IDE を再起動すると利用できます。  
Restart Arduino IDE after installation.

------------------------------------------------------------------------

## 利用方法 / Usage

必要なヘッダを include してください。  
Include the required headers.

```cpp
#include <SharedMemoryAllocator.h>
#include <InterCoreRingBuffer.h>
#include <MultiCoreSpinLock.h>
```

------------------------------------------------------------------------

## サンプル / Examples

`examples/` フォルダにサンプルコードがあります。  
Example sketches are available in the `examples/` directory.

------------------------------------------------------------------------

## 注意事項 / Notes

- 一部機能は MainCore 側専用です  
Some features are intended for MainCore-side usage only.

- `InterCoreRingBuffer` は Single Producer / Single Consumer 前提です  
`InterCoreRingBuffer` assumes Single Producer / Single Consumer usage.

- リアルタイム用途では十分なバッファサイズを確保してください  
Use sufficient buffer sizes for realtime applications.

- 大きなデータは共有メモリ＋ポインタ運用を推奨します  
For large data, shared memory + pointer-based transfer is recommended.

------------------------------------------------------------------------

## 関連リポジトリ / Related Repository

### sketches

https://github.com/Interested-In-Spresense/sketches

Spresense 向け各種サンプルスケッチ集です。  
Collection of example sketches for Spresense.

#### 関連サンプル / Related Examples

- `MultiCore/ManySensor`
- `MultiCore/SpinLockVerification`

------------------------------------------------------------------------

## License

LGPL v2.1 or later
