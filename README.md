# MIDI Bridge (VST3)
ノートをトリガーに、PC/CCの固定値を仮想MIDIポートへ送るプラグイン。

## 使い方
1. loopMIDIで仮想ポートを作る(例: Amp Bridge)
2. 受け側アプリのMIDI入力にそのポートを選ぶ
3. MIDI Bridgeを楽器トラックに立て、上部のドロップダウンで同じポートを選ぶ
4. 表で Type/Ch/No./Value/Trigger note を設定。Sendで手動テスト
5. トラックにトリガーノートを置くと、その行のPC/CCが送信される
