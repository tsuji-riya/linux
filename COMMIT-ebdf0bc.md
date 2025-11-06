# コミット ebdf0bc84b5a22ba180a884b6815d1d4030cb2e4 の説明

## 概要

このコミットは、Linux カーネル 2.6.32 ("Man-Eating Seals of Antiquity") のソースコードを本リポジトリに初期インポートしたものです。dsm232 パッチを適用した形でインポートされています。

## コミット情報

- **コミットハッシュ**: `ebdf0bc84b5a22ba180a884b6815d1d4030cb2e4`
- **作成者**: tsuji-riya <tsuji.riya@gmail.com>
- **日付**: 2025年11月1日 14:52:29 (JST)
- **コミットメッセージ**: "Apply dsm232 linux kernel patch"

## 変更内容の統計

- **変更されたファイル数**: 30,485 ファイル
- **追加された行数**: 12,527,137 行
- **削除された行数**: 0 行（新規作成）

## 主要なコンポーネント

このコミットでは、完全な Linux カーネル 2.6.32 のソースツリーが追加されました。主要なディレクトリとコンポーネントは以下の通りです：

### コアカーネルコンポーネント
- **arch/**: アーキテクチャ固有のコード（x86, ARM, PowerPC など 25 アーキテクチャ以上）
- **kernel/**: カーネルのコア機能（スケジューラ、タイマー、シグナル処理など）
- **mm/**: メモリ管理サブシステム
- **fs/**: ファイルシステムの実装（ext2, ext3, ext4, NFS, CIFS など 70 以上）
- **net/**: ネットワークスタック（IPv4, IPv6, TCP/IP など）
- **ipc/**: プロセス間通信機構

### ドライバとハードウェアサポート
- **drivers/**: デバイスドライバ（ネットワーク、ストレージ、グラフィックスなど 88 サブシステム）
- **sound/**: サウンドサブシステム (ALSA)
- **block/**: ブロックレイヤーとI/Oスケジューラ

### セキュリティとインフラストラクチャ
- **security/**: セキュリティモジュール（SELinux, AppArmor, SMACK, TOMOYO）
- **crypto/**: 暗号化API
- **lib/**: 共通ライブラリとユーティリティ関数

### ビルドとドキュメント
- **scripts/**: ビルドスクリプトとツール
- **Documentation/**: カーネルドキュメント
- **tools/**: perf などのパフォーマンス分析ツール
- **samples/**: サンプルコードと例
- **Makefile, Kbuild**: ビルドシステムのエントリポイント

### その他の重要なファイル
- **README**: カーネルのインストールとコンパイルに関する情報
- **COPYING**: GNU GPL ライセンス条項
- **CREDITS**: 貢献者リスト
- **MAINTAINERS**: メンテナーと各サブシステムの連絡先

## Linux 2.6.32 について

Linux 2.6.32 は、2009年12月にリリースされた長期サポート (LTS) カーネルバージョンです。主な特徴：

- **コードネーム**: "Man-Eating Seals of Antiquity"
- **リリース日**: 2009年12月2日
- **LTS サポート**: 2016年まで延長サポート
- **主な機能**:
  - 改善されたデスクトップ応答性
  - Btrfs ファイルシステムのサポート向上
  - perf イベントカウンタのサポート
  - 仮想化機能の強化
  - 多数のドライバとハードウェアサポートの追加

## dsm232 パッチについて

このコミットメッセージで言及されている "dsm232 パッチ" は、標準の Linux 2.6.32 カーネルに適用された特定のパッチセットまたはカスタマイゼーションを指していると考えられます。詳細な変更内容については、パッチ作成者または関連ドキュメントを参照してください。

## 技術的な詳細

### サポートされるアーキテクチャ
このカーネルは、以下を含む複数のプロセッサアーキテクチャをサポートしています：
- x86 (32-bit と 64-bit)
- ARM
- PowerPC/PowerPC64
- MIPS
- SPARC/UltraSPARC
- IA-64
- S/390
- その他多数

### ファイルシステムサポート
以下を含む多数のファイルシステムをサポート：
- ext2, ext3, ext4
- Btrfs (実験的)
- XFS, JFS, ReiserFS
- NFS, CIFS
- procfs, sysfs, debugfs
- その他多数

### ネットワークプロトコル
- IPv4/IPv6
- TCP, UDP, SCTP
- Netfilter/iptables
- ワイヤレスネットワーキング (802.11)
- Bluetooth
- その他多数のプロトコル

## 使用方法

このカーネルをビルドして使用するには、README ファイルの指示に従ってください：

```bash
# 設定
make menuconfig

# ビルド
make

# モジュールのインストール（要root権限）
make modules_install

# カーネルのインストール（要root権限）
make install
```

## 参考資料

- Linux カーネル公式サイト: https://www.kernel.org/
- Linux 2.6.32 リリースノート: https://kernelnewbies.org/Linux_2_6_32
- カーネルドキュメント: `Documentation/` ディレクトリ内

---

# Commit ebdf0bc84b5a22ba180a884b6815d1d4030cb2e4 Explanation (English)

## Overview

This commit represents the initial import of the Linux kernel 2.6.32 ("Man-Eating Seals of Antiquity") source code into this repository, applied with the dsm232 patch.

## Commit Information

- **Commit Hash**: `ebdf0bc84b5a22ba180a884b6815d1d4030cb2e4`
- **Author**: tsuji-riya <tsuji.riya@gmail.com>
- **Date**: November 1, 2025, 14:52:29 (JST)
- **Commit Message**: "Apply dsm232 linux kernel patch"

## Change Statistics

- **Files Changed**: 30,485 files
- **Lines Added**: 12,527,137 lines
- **Lines Deleted**: 0 lines (new creation)

## Major Components

This commit adds the complete Linux kernel 2.6.32 source tree. The major directories and components include:

### Core Kernel Components
- **arch/**: Architecture-specific code (x86, ARM, PowerPC, etc. - 25+ architectures)
- **kernel/**: Core kernel functionality (scheduler, timers, signal handling, etc.)
- **mm/**: Memory management subsystem
- **fs/**: Filesystem implementations (ext2, ext3, ext4, NFS, CIFS, etc. - 70+ filesystems)
- **net/**: Network stack (IPv4, IPv6, TCP/IP, etc.)
- **ipc/**: Inter-process communication mechanisms

### Drivers and Hardware Support
- **drivers/**: Device drivers (network, storage, graphics, etc. - 88 subsystems)
- **sound/**: Sound subsystem (ALSA)
- **block/**: Block layer and I/O schedulers

### Security and Infrastructure
- **security/**: Security modules (SELinux, AppArmor, SMACK, TOMOYO)
- **crypto/**: Cryptographic API
- **lib/**: Common libraries and utility functions

### Build and Documentation
- **scripts/**: Build scripts and tools
- **Documentation/**: Kernel documentation
- **tools/**: Performance analysis tools like perf
- **samples/**: Sample code and examples
- **Makefile, Kbuild**: Build system entry points

### Other Important Files
- **README**: Information about kernel installation and compilation
- **COPYING**: GNU GPL license terms
- **CREDITS**: List of contributors
- **MAINTAINERS**: Maintainers and contacts for each subsystem

## About Linux 2.6.32

Linux 2.6.32 is a Long-Term Support (LTS) kernel version released in December 2009. Key features:

- **Codename**: "Man-Eating Seals of Antiquity"
- **Release Date**: December 2, 2009
- **LTS Support**: Extended support until 2016
- **Major Features**:
  - Improved desktop responsiveness
  - Enhanced Btrfs filesystem support
  - perf event counter support
  - Virtualization improvements
  - Numerous driver and hardware support additions

## About the dsm232 Patch

The "dsm232 patch" mentioned in the commit message likely refers to a specific patchset or customization applied to the standard Linux 2.6.32 kernel. For detailed information about the changes, please refer to the patch author or related documentation.

## Technical Details

### Supported Architectures
This kernel supports multiple processor architectures including:
- x86 (32-bit and 64-bit)
- ARM
- PowerPC/PowerPC64
- MIPS
- SPARC/UltraSPARC
- IA-64
- S/390
- Many others

### Filesystem Support
Supports numerous filesystems including:
- ext2, ext3, ext4
- Btrfs (experimental)
- XFS, JFS, ReiserFS
- NFS, CIFS
- procfs, sysfs, debugfs
- Many others

### Network Protocols
- IPv4/IPv6
- TCP, UDP, SCTP
- Netfilter/iptables
- Wireless networking (802.11)
- Bluetooth
- Many other protocols

## Usage

To build and use this kernel, follow the instructions in the README file:

```bash
# Configuration
make menuconfig

# Build
make

# Install modules (requires root)
make modules_install

# Install kernel (requires root)
make install
```

## References

- Linux Kernel Official Site: https://www.kernel.org/
- Linux 2.6.32 Release Notes: https://kernelnewbies.org/Linux_2_6_32
- Kernel Documentation: Within the `Documentation/` directory
