# Phase 5 接続監視修正実施記録

## 1. 実機確認で判明した問題

ASCENDのRAセッション中にmacOSのネットワークを切断・復旧しても表示が変化せず、
エミュレータをresetして新しいRA API要求が発生した場合だけ状態が変化した。

固定版rcheevosの`RC_CLIENT_EVENT_DISCONNECTED`は一般的なネットワーク到達性通知ではない。
実績解除またはLeaderboard送信が失敗して再送待ちになった場合にだけ発生するため、通信要求の
ない期間にネットワークを切断してもイベントは発生しない。

## 2. 修正

macOS 10.13で利用できるSystemConfigurationの`SCNetworkReachability`を使い、既定経路の
到達性を500ms間隔で取得する監視を追加した。外部サーバーへのprobeは送信しない。

- 初回観測はbaselineとし通知しない。
- `reachable → unreachable`を既存の`Disconnected` signalへ変換する。
- `unreachable → reachable`を既存の`Reconnected` signalへ変換する。
- `Unknown`は最後に確認できた状態を上書きしない。
- 同じ状態の連続観測と、rcheevosイベントとの重複は状態機械で抑止する。
- `ActiveDisconnected`でもフレーム評価を継続し、復旧時だけ`Active`へ戻す。

macOS以外ではmonitor factoryが未対応を返し、既存動作を維持する。

## 3. テスト

`ra_session_state_test`へbaseline、重複抑止、Unknown保持、切断、復旧の順序テストを追加した。
既存のActive／ActiveDisconnected／Offline状態機械とrcheevos event mappingも継続して検証する。

## 4. 手動再確認結果

ASCENDをRAセッションとして起動し、macOSの使用中ネットワークインターフェースを
切断・復旧する実機確認を行った。resetなしで`RA: disconnected`、
`RA: reconnected`へ変化し、問題がないことを確認した。

テスト媒体は`/Volumes/CrucialX6/roms/PC88/TEST/ascend.d88`を使用する。
