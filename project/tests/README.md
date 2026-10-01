# 検証用プログラム

`AudioApiSmoke.vcxproj`はゲームとは別のコンソール用プロジェクトです。ゲーム本体へmainを追加するものではありません。

- Debug / Development x64で、Audioの無効VoiceId、読込キャッシュ、停止、一時停止・再開、BgmPlayer、SoundEffectPlayerのAPIを確認する目的です。
- 引数へ音声ファイルのパスを一つ渡します。音量0で実行するので、実際の音質・音量・BGMの開始タイミングは別途ゲームで確認が必要です。
- 2026-09-28：承認チェックの利用制限解消後、Debug / Development x64の両方でビルド・実行成功（`SUCCESS: failures=0`）。`ReadSample`の符号変換警告を明示的なDWORD指定で解消した後も、両構成で再実行して成功しました。

リポジトリ直下から、Visual StudioのDeveloper PowerShellで実行する例です。DevelopmentはConfigurationと実行ファイルのDebug部分をDevelopmentへ置き換えます。

```powershell
# ゲーム本体とは別にテストをビルドします。
MSBuild project/tests/AudioApiSmoke.vcxproj /m /p:Configuration=Debug /p:Platform=x64
# リポジトリ内の音源を使い、無音でAPIの戻り値と状態を確認します。
./generated/tests/Debug/AudioApiSmoke.exe project/resources/debug/Alarm01.wav
```

合格範囲はAPIの戻り値・保持状態です。Mixer変更後の再生中Voiceの実音量、欠損音源、音声デバイス切断、全コーデック、BGMと画面表示の同期を保証するテストではありません。

## 通常起動のScene順

`CG2_STARTUP_ROUTE_SMOKE=1`は撮影・録画をせず、`Loading → Title → Loading → Stage1`の更新と各SceneのDraw呼び出しを確認して終了します。`CG2_START_SCENE`は指定しません。

```powershell
# projectフォルダを作業ディレクトリとして、Release版の通常起動順を確認します。
$env:CG2_STARTUP_ROUTE_SMOKE = '1'
../generated/outputs/Release/CG2_00-01.exe
Remove-Item Env:CG2_STARTUP_ROUTE_SMOKE
Get-Content logs/startup_route_smoke.log
```

ログの`SUCCESS`と、両方のLoading・Title・Stage1のDraw回数が1以上であることを確認します。これはSceneの実行順のテストで、白背景の見た目やマウス操作を保証しません。

## 撮影Smokeの対応構成

`CG2_CAPTURE_SMOKE=1`による写真・録画テストは`USE_IMGUI`付きのDebug／Development構成だけに対応します。Releaseで指定した場合は待ち続けず、`logs/capture_manager_smoke.log`へ非対応の`FAILURE`を残して終了します。Releaseの画面確認にはこのSmokeを成功の根拠に使わないでください。

## 通常モデルとAnimationモデルの描画確認

既存のゲーム内テスト`CG2_DEBUG_UI_SMOKE=walk-scene`は、Debug Sceneへwalk.gltfを棚と同じ経路で追加し、30回・90回更新後のGame ViewをBMPへ保存して終了します。起動Sceneには`CG2_START_SCENE=DEBUG`を指定します。環境変数は検証用プロセスだけに指定し、検証後は元に戻してください。

- ログ：`project/logs/gameplay_ui_smoke_<日時>.log`
- 画像：`project/resources/Captures/Screenshots/CG2_walk_scene_frame<回数>_<日時>.bmp`
- 2026-09-28：Development x64で実行成功。画像を開いてwalkの人物、Animationモデル直後に描く通常の球体武器、地形の表示を確認。
- `SUCCESS`は追加・更新・画像保存の成功です。モデルの可視性は必ず画像を開いて確認してください。静止画2枚だけでは全Animation、鏡の反射、ユーザーのF5起動経路を保証できません。

## CSVの番号別処理

`LevelApiSmoke.vcxproj`は、CSVファイルを追加せずに`MapChipField`・`MapChipRegistry`・`StageMapChipFactory`を確認する独立プロジェクトです。ゲーム本体のSceneや保存中のstage1.jsonは変更しません。

```powershell
# Debugと同じ手順でDevelopmentにも切り替えられます。
MSBuild project/tests/LevelApiSmoke.vcxproj /m /p:Configuration=Debug /p:Platform=x64
./generated/tests/Debug/LevelApiSmoke.exe
```

空欄の列番号、P/E/B/G/C/Lの登録別実行、B0とB1の区別、未登録番号の無視、B0の1×1×1 Colliderと座標を確認します。さらに、Editorが変更する入れ子のPlayerStartとStageが読むPlayerStartが同じ元データであることも確認します。これは対応表と配置データのテストです。Stage1でE0などの敵・ギミックが表示される証拠ではありません。
