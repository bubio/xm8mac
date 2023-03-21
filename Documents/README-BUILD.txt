(UTF-8: Japanese)

-----------------------------------------------------------------------

eXellent Multi-platform emulator type 8 - 'XM8'
based on ePC-8801MA

Copyright (C) 2015-2018 ＰＩ．
version 1.70 (2018/01/23)

-----------------------------------------------------------------------

□ソースコードについて


このディレクトリには、XM8の上記バージョンをビルドするために必要となるソース
コードを収録しています。

以下プラットフォーム毎に、開発環境の構築方法およびビルド方法を記します。



□Windows


＜開発環境の構築＞

"Windows"ディレクトリにVisual Studio 2010以降のバージョンで使用可能なソリ
ューションファイル、プロジェクトファイルを収録しています。

参考として、Visual Studio Express 2013を使用する例を示します。


(1)Visual Studio Express 2013 for Windows Desktop

Visual Studio Express 2013 with Update 5 for Windows Desktop
https://my.visualstudio.com/downloads/
(Microsoftアカウントが必要)

ページ内のの"Visual Studio Express 2013 for Windows Desktop with Update 5"を
クリックし、ダウンロードします。(必要に応じ事前に言語をJPに変えてください)


(2)SDL開発ライブラリ

Windows (Visual C++ 32bit/64bit)向けライブラリをダウンロードします。
https://www.libsdl.org/download-2.0.php

version 1.70ビルド時点の最新版はSDL 2.0.7(stable)です。

これをSource\Windows\SDLへ展開します。以下のようになります。

Source\Windows\SDL\include（インクルードファイル）
Source\Widnows\SDL\lib\x86（32bit向けライブラリ）
Source\Windows\SDL\lib\x64（64bit向けライブラリ）


＜ビルド＞

Visual Studioで"XM8.sln"を開き、所望のターゲット・プラットフォームを指定し
てリビルドすると"XM8.exe"が生成されます。

Releaseターゲットはランタイムライブラリを静的リンクします。Debugターゲット
はこれと異なり、ランタイムDLLを利用する設定としています。



□Linux


＜開発環境の構築＞
g++と標準のツールチェインを利用し、SDL 2.0をビルドする環境を整えます。詳し
くは以下を参照してください。
https://wiki.libsdl.org/Installation#Linux.2FUnix

作者環境であるXUbuntu 16.04での場合を例として挙げます。

sudo apt-get install g++ build-essential libsdl2-dev


＜ビルド>

makefileを利用してビルドすればOKです。例えば以下の通りです。

cd Source
cd Linux
make clean
make



□Android


＜開発環境の構築＞

2018年初頭現在、SDLを使用したAndroidアプリケーションの構築は非常に困難です。

過去に使われてきたEclipse + ADT Plugin + ndk-build + gccという組み合わせが
Android Studio + gradle + CMake + Clangと大幅に変わってしまい、これに対し
SDLが追い付いていない状態です。さらにgradle pluginのバージョン依存も加わり
複雑怪奇といってよい状況を呈しています。

version 1.70のビルドにあたってはWindows/Linuxと異なりSDL 2.0.7(stable)を
上記理由より早々に諦め、開発中バージョンであるSDL 2.0.8のスナップショット、
SDL-2.0.8-11774を採用しました。

上記バージョンを入手できたという前提で、作者のビルド環境を記載します。

・Android Studio             : 3.0.1
・JDK                        : 上記Android Studio内蔵のOpenJDK
・gradle                     : 4.1
・gradle plugin              : 3.0.1
・Android SDK Platform-Tools : 27.0.1
・Android SDK Tools          : 26.1.1
・Android SDK Platforms      : Android 8.0 (Oreo)

この他、CMakeとNDKが必要です。詳しくは以下を参照ください。

https://developer.android.com/studio/projects/add-native-code.html?hl=ja


SDL-2.0.8-11774のサブディレクトリ"include"と"src"の内容を、それぞれ以下の
フォルダにコピーします。

Source/Android/XM8/app/src/main/cpp/SDL/include (includeファイル)
Source/Android/XM8/app/src/main/cpp/SDL/src     (ソースファイルツリー)


Android Studioを起動し、"Open an existing Android Studio Project"で
Source/Android/XM8フォルダを指定します。自動的にgradle sync projectが行われ
ます。(CPUにもよりますが、遅いとsync完了まで5分程度かかります)

Android StudioではCMake・NDKとの連携が取られているため、version 1.61以前で
必要であったndk-buildの事前実行は不要です。

Run→Run"app" メニューでDebugバージョンのビルドと端末へのダウンロード、
Build→Generate Signed APK メニューでReleaseバージョンのビルドと署名付与が
行えます。

Releaseバージョンのビルドではarm64-v8a, armeabi-v7a, x86, x86-64の4つのアー
キテクチャに対しSDL本体とXM8のコンパイルが行われるため、フルビルドの場合は
数分～10分以上の時間を要します。
(EOF)
