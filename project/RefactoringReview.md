# チーム向けエンジン整理の再点検

各節は調査した時点の記録です。後から原因が分かった項目は、日付と検証内容を照合して新しい結論を優先します。

## 10月19日の52点レビューとの照合（2026-10-21）

この節は現行作業ツリーの再点検であり、外部評価者による再採点ではありません。以下の「確認済み」は元レビューの具体的な指摘箇所についての結果で、全項目100点を意味しません。

| 元レビューの主な指摘 | 現在確認できたこと | 残る判断 |
| --- | --- | --- |
| ソリューション名と具体的なスペルミス | 現行のソリューション・プロジェクトは`riboninChallenge`。`Adapater`・`paletter`・`RasiterZerState`・`enviromentColor`と旧プロジェクト名は現行のengine・game・shader・主プロジェクトで該当なし | Git履歴は改名対象にしない。全識別子の辞書検査は未実施 |
| VSフィルタと実ファイルの一致 | 主プロジェクトの登録302件とfiltersの302件が一致。重複・差分・物理パスとのフィルタ階層違い・ソースIncludeのワイルドカードは各0件 | 今後のファイル追加でも同じ検査を続ける |
| 文字列変換と撮影の二重管理 | `Audio.cpp`は`StringUtility::ConvertString()`を呼び、旧`GameViewCapture`の自作ファイルは現行ツリーにない | 撮影全経路の同等性は、変更時の実画面と保存結果で引き続き確認する |
| 巨大なSmoke関数 | `Stage1GameplaySmoke::Initialize()`は入力保存と検証関数の呼出し、`CaptureManager::UpdateSmokeBeforeCapture()`はScene別関数への振分けになっている | 個々の検証関数を行数だけで再分割しない |
| カプセル化 | `Object3d::GetTransform()`・`GetSkeleton()`、`Player::GetMovementSettings()`はconst参照。`Object3dFactory`のfriend指定は、生成途中の初期化関数だけをFactoryへ開くため残す | `Sprite::GetTransform()`は可変参照の既存公開APIで、現行コードに呼出しは見つからない。互換性を判断する前に削除しない |
| Stageの責任と命名規則 | Map選択・ファイル監視・照明・Light Puzzleは専用クラスが存在し、Spriteや主要描画クラスのメンバ名は末尾`_`へ整理済み | Stage全体の責任と全クラスの命名規則を一律に完了とする根拠はまだない |

残る広域の評価項目は、フォルダ名の表記ゆれ、公開APIコメントの網羅性、グローバル状態、エンジン全体の名前空間、C++23への移行などです。現行設定はC++20です。これらは変更範囲や既存APIへの影響が大きいため、100点という数字だけを理由に一括変更しません。新しい外部評価と実際の利用箇所を照らし、必要と判断できた変更を個別に行います。

## Model・Textureの再読込で実行中の見た目を保持（2026-10-07）

- 原因：`ModelManager::ReloadModel()`は新しく読んだModelを同じアドレスへ移すが、`Model::Initialize()`は環境反射率を0へ戻し、`Model::SetTexture()`で指定した画像も元のモデルファイルの参照へ戻していた。再読込を実行するDebugの追加Smokeでは、終了コード0でもframe90の人物が黒く、手持ち球の画像も変わることを実画像で確認した。加えて、GPUが古いモデル・SRVを読んでいる間の差し替えを避ける待機順が明示されていなかった。
- 変更：`Model`が`SetTexture()`による明示画像だけを記録し、`CopyRuntimeSettingsFrom()`で実行中の材質値とその画像を新しいModelへ引き継ぐ。モデルファイル側の頂点・材質一覧・明示されていない画像参照は新しい読込結果のままにする。`ModelManager`はGPU完了後に古いModelの中身を差し替える。`TextureManager`は画像転送と既存のGPU完了待ちを終えてから同じSRV番号を更新するため、追加の待機を増やさない。通常フレームの更新・描画順、Light設定値、仮Titleは変えていない。
- 検証：Development x64を別出力へビルドし、新設の`walk-scene-reload` Smokeでframe30撮影→frame60でModel・Texture再読込→frame90撮影を実行。終了コード0・`SUCCESS`で、Modelアドレス、環境反射率、手持ち球の画像パスが再読込前後で一致した。今回の二枚のBMPを開き、人物のAnimationモデル、通常モデル、手持ち球、背景と照明が再読込後も描画されることを確認した。Stage1 Capture Smokeは終了コード0・12枚可視で、今回の画像にPlayerの赤白画像・部屋・鏡板・危険Lightを確認。起動経路Smokeも終了コード0でLoading二回・Title・Stage1のDrawを記録。最後のソリューションDebug x64ビルドは警告・エラーなく成功し、Debugでも`walk-scene-reload`は終了コード0・`SUCCESS`、frame90の人物・通常モデル・照明を実画像で確認した。今回の実行から新しいダンプは見つからず、`.vcxproj`の登録261件はfiltersと一致・ワイルドカード0、`git diff --check`も成功。
- 未確認：実際にモデルファイルの頂点や参照画像を書き換えた時のHot Reload、連続編集時のちらつき、通常Development F5から仮Titleの手入力Enter→本編、E・Mouseでの鏡操作と実音声。Smokeの画像が正常でも全フレームのGPU競合が起きないことを直接証明するものではない。

## Stage1の個体別画像欠損を生成失敗として伝える（2026-10-07）

- 原因：`Object3d::SetTextureOverride()`は画像を読めない時に旧設定を保ってログへ残すが、呼び出し元へ失敗を返さなかった。`Stage1::InitializePlayerAndCamera()`はPlayerの`monsterBall.png`が欠けても成功扱いになり、元モデルの画像で代用したまま本編へ進み得た。鏡・Laser発射装置・Switchの白画像指定も戻り値を確認できなかった。
- 変更：`SetTextureOverride()`が成功時`true`、失敗時`false`を返す。`CarryableMirror::Initialize()`、`FixedMirror::Initialize()`、`Stage1`の必須モデル生成は失敗を上位へ伝え、Stage全体の初期化を止める。正常時の画像、SRV番号、配置、Light、描画順、仮Titleの表示・入力は変更していない。
- 検証：Development x64を別出力へビルド。正常素材のStage1GameplaySmokeは今回の終了コード0・`SUCCESS`。Capture Smokeも終了コード0・写真12枚・可視12枚で、今回のPNGを開いてPlayerの球、部屋、鏡板、危険Lightの線を確認した。起動経路Smokeは終了コード0でLoading二回・Title・Stage1のDrawを記録した。一時実行環境で`monsterBall.png`だけを欠かすと、`Object3d: texture override load failed`とStage初期化失敗が今回のログに残り、終了コード1・新規ダンプなし。`white.png`欠損も終了コード1だが、これは`plane.obj`自身の材質読込で先に失敗するため、新しいMirror側の判定を直接証明するものではない。最後のソリューションDebug x64ビルドは成功し、プロジェクト設定由来の警告・エラーは出なかった。
- 未確認：最新の通常Development実行ファイルによるF5→仮Titleで手入力Enter→本編、E・Mouseでの鏡操作、すべての反射角度と実音声。通常Development exeは別プロセスが使用中なので上書きせず、同じ構成の別出力で確認した。仮Titleの変更は行っていない。

## 現在の差分をまとめた回帰確認（2026-10-07）

- この節は新しい機能や見た目の変更ではなく、現在の作業ツリー全体への確認結果。Spriteの生成元を照合し、実際に呼ぶ経路では画像読込を確認してから`Sprite::Initialize()`へ渡している。見つからなかった欠陥を直したことにはしない。
- 独立テストはAudio APIとLevel/CSV APIをDebug／Development x64の両方で再ビルド・再実行し、どちらも`SUCCESS: failures=0`・終了コード0。Audioの実際の聴感や、ゲーム画面内のCSV配置を証明するものではない。
- ゲーム本体のStage1GameplaySmokeはDebug／Development x64とも終了コード0・`SUCCESS`。鏡・Laser・Collider・Cameraの必須判定に加え、配置から切り離した危険Light反射経路`hazardReflectionRoute=1`を確認。現行配置の`hazardReflection=0`は観測値であり、実プレイ中の反射像の証明にはしない。
- Development x64のwalk-scene Smokeは終了コード0。今回作られたframe30／frame90のBMPを実際に開き、歩行モデル、通常モデルの球、背景、照明を確認。Stage1 Capture Smokeも終了コード0、写真12枚・可視判定12枚で、今回の開始・途中・本編のPNGを開き、Player・部屋・鏡板・危険Lightの線を確認した。画像保存や非黒画素だけでモデル描画成功とは判断していない。
- Debug x64の起動経路Smokeは終了コード0、Loading二回・TITLE・STAGE1のDrawを記録。今回のゲーム実行から新しいクラッシュダンプは見つからず、最新ダンプは2026-09-30のまま。Visual Studio本体の最終Debug x64ビルドとプロジェクト登録の結果は別途確認する。
- 未確認：最新の通常Development実行ファイルによるF5→Loading→仮Title→手入力Enter→本編の実操作、E・マウスでの鏡操作、反射像の全角度、実音声。通常Development exeは別プロセスが使用中なので停止・上書きせず、同じ構成の別出力で自動テストした。ユーザー所有の未追跡録画と`resources/backGround/`は触れていない。

## 円柱Particleの不正な分割数・寸法を登録前に拒否（2026-10-07）

- 原因：`ParticleManager::CreateCylinderParticleGroup()`は分割数3未満・半径や高さ0以下を`assert`だけで確認していた。Development／Releaseは`NDEBUG`でassertが無効なため、そのまま側面頂点の計算やGPU資源作成へ進み得た。これは入力検証不足で、現在の既定値32・正の寸法で起きた不具合ではない。
- 変更：分割数・三つの寸法が有限かつ正かを、画像読込・同名グループ削除より前に確認する。失敗時はグループ名をログに残して戻り、既存グループと粒子を保持する。正常値の形状・Texture・発生・描画は変更していない。
- 検証：既存の時間再生Smokeへ、分割数2、半径0、高さ0、無限大による同名置換と新規作成を追加。Development x64（assert無効）とDebug x64で終了コード0・`SUCCESS`・`invalidCylinder=rejected+originalPreserved`を確認。通常のDevelopment実行ファイルは別プロセスが使用中のため触れず、同じ構成の別出力で実行してから、その出力だけをパス照合後に削除した。
- 未確認：GPU資源枠を使い切った時や、極端に大きいが有限の分割数を指定した時。通常の粒子全種類の見た目を、この入力検査だけで検証済みとはしない。

## 終了メッセージ後の余分な更新を止める（2026-10-07）

- 原因：`Framework::Update()`は`WM_QUIT`を受けて`endRequest_`を立てても、同じフレームでWindowサイズ・入力・Audio・Sceneを更新していた。派生先の`Game::Update()`もScene Stressと起動経路Smokeを進めるため、閉じたWindowや失敗終了後に一回余分な処理が走り得た。
- 変更：`WinApp::ProcessMessage()`は`WM_QUIT`を通常のWindowProcへ渡さず終了コードを保持する。`Framework::Update()`は終了要求後に直ちに戻り、`Game::Update()`もそこで止める。通常フレームの初期化・更新・描画順、Scene切替、仮Titleの入力・見た目は変更していない。
- 検証：通常のDevelopment実行ファイルは別プロセスが使用中のため触れず、同じDevelopment x64構成の別出力でビルド。起動経路Smokeの成功は終了コード0、意図的な設定違反は終了コード1。Release x64では非対応のCapture Smokeが既知の失敗ログを残し、`WM_QUIT`由来の終了コード1を維持。別出力はパス照合後に削除した。
- 未確認：今回の変更後、通常F5からWindowを手動で閉じた際の実操作とEnterによる本編開始。起動経路SmokeのScene切替をEnter操作の証拠にはしない。

## Loadingの白画像欠落時の停止（2026-10-07）

- 原因：`LoadingScene::InitializeLoadingSprite()`は白画像の読込成否を確認せず`Sprite::Initialize()`へ渡していた。`Sprite::SetTexture()`は失敗時にパスを登録せず、続く`Sprite::Update()`が未登録画像の寸法を参照し得る。白画面のクリア色だけで欠損を隠すのも、画像読込成功の証明にはならない。
- 変更：LoadingSceneが`resources/debug/white.png`をSprite生成前に確認する。失敗時は画像名をログに残して終了コード1を要求し、次Sceneの予約とDrawを行わない。正常時の白Sprite、最大化画面のサイズ更新、Loading→次Sceneの順序、仮Titleの設定は変えていない。
- 検証：通常のDevelopment実行ファイルは別プロセスが使用中だったため触れず、同じDevelopment x64構成を別出力へビルド。今回の起動経路Smokeは終了コード0で、二度のLoading・TITLE・STAGE1が従来回数だけDrawされた。一時実行環境で白画像だけを欠かすと、該当ログ・終了コード1・新規ダンプ0を確認。一時環境と別出力は絶対パス照合後に削除した。
- 未確認：通常F5でのLoading→仮Title→手入力Enter→本編、GPU資源生成自体の失敗。自動SmokeはEnter入力の代わりではない。

## Debug画面の必須SkyBox画像と初期モデルの失敗経路（2026-10-07）

- 原因：`DebugScene::InitializeSceneResources()`は環境反射・SkyBoxのDDS読込結果を確認せず、未登録でも`Object3d::Draw()`や`SkyBox::Draw()`のSRV取得へ進み得た。`InitializeInitialContent()`の失敗で`Initialize()`を抜けても、同じフレームに`Update()`が走り、未生成のSkyBoxなどを参照し得た。
- 変更：DebugSceneが必須のmonsterBall・grass・uvChecker・DDSを生成前に確認する。失敗時は画像名または初期モデル生成失敗をログに残し、終了コード1を要求する。初期化完了まで`Update()`と`Draw()`を行わない。正常時のTexture読込順、SkyBox・環境反射設定、モデル・ライト・操作・描画順は維持。現在の仮Titleには触れていない。
- 検証：Development x64の通常出力は起動中プロセスがロックしていてリンク不可だったため、そのプロセスを終了せず、別出力先`DevelopmentAudit`へ同じ構成をビルド。直接DEBUG起動の時間再生Smokeは終了コード0・`SUCCESS`。一時環境でDDSだけ欠損させた場合と、DDSを揃えて初期モデルを欠損させた場合は、どちらも該当ログ・終了コード1・新規ダンプ0を確認。一時環境は絶対パス照合後に削除した。
- 最終確認：Release x64と最後のDebug x64もビルド成功。`.vcxproj`と`.filters`は各261件一致、ワイルドカード0、`git diff --check`成功。今回だけの`DevelopmentAudit`出力も絶対パス照合後に削除した。起動中だった通常のDevelopment実行ファイルやユーザー所有の録画は触れていない。
- 未確認：通常F5のDebug画面操作、GPU資源生成の失敗、すべてのParticle画像欠損時の見た目。

## 鏡面専用Bufferを描画ごとに固定（2026-10-07）

- 原因：`FixedMirror::DrawSurface()`が反射像を出す時、`Object3d::DrawMirror()`は同じ`Object3dGpuData`内の行列・Reflection用Upload Bufferを参照していた。次のフレームの`UpdateTransform()`と`BindForMirrorDraw()`がその領域を書き換えると、GPUが前の鏡面描画をまだ読んでいる場合に値が混ざり得る。これはコード上の寿命の不一致で、実画面の乱れを再現したわけではない。
- 変更：既存の`Object3dGpuData`へ鏡面専用の描画一回分の行列・Reflection Bufferを持たせ、SwapChain枠のFence待ち後だけ再利用する。`BindForMirrorDraw()`が確保・Mapに失敗したら`false`を返し、`Object3d::DrawMirror()`は古いRoot Parameterで板を描かない。不要になった常設Reflection Bufferはなくした。反射Camera、Tint値、反射Texture、Shader、鏡の位置・角度・サイズ、描画順は変えていない。
- Development／Release／最後にDebug x64ビルド成功。プロジェクト設定由来の警告・エラーなし。Visual StudioのDevelopment x64をF5で起動し、Loading後の現行TitleとStage1を実画面で確認。Scene一覧からStage1へ切替後、保存しないEdit ViewのCamera位置だけをX=0／Y=2／Z=3、Pitch=0へ一時変更して、`StageMirror`の正面に球体とLaserを含む反射像が表示されることを確認した。StageMap JSONは保存せず、Exit Game→Yesで終了。Visual Studioの今回のデバッグ出力はゲーム終了コード0、今回のログ`logs/20261007_043137.log`を確認し、新しいダンプはなかった。
- 最終コードのDebug Stage1GameplaySmokeは終了コード0・`SUCCESS`で、固定鏡と鏡床の反射判定、Laser・Camera・Colliderの必須項目を通過。DevelopmentのDebug walk-scene Smokeも終了コード0で、frame30／frame90の実画像を開き、通常モデル・Animationモデル・照明の表示を確認した。`.vcxproj`のClCompile／ClIncludeはワイルドカード0、重複0、filtersとの差0で、`git diff --check`も成功。
- 未確認：鏡面の全角度・連続移動中のちらつき、修正前後の定量比較、通常F5のTitleから手入力Enterで本編へ進む操作。壁越し表示専用Bufferの寿命は別経路で、現行本編に利用箇所は見つからない。

## 通常モデルのLight値を描画ごとに固定（2026-10-07）

- 原因：`Object3dGpuData::BindForObjectDraw()`は行列とCamera位置だけを描画ごとに退避していた。Directional／Point／Spot LightはObjectごとの同じUpload Bufferを`Object3dRenderContext::UpdateObject()`から毎フレーム書き換えていた。`DirectXCommon::PostDraw()`は次に使うSwapChain枠のFenceを待つが、別枠のGPU実行が終わる前にLight値を上書きし得る。実画面のちらつきを再現したわけではなく、コード上のデータ寿命の不一致を確認した。
- 変更：既存の`Object3dGpuData`内で、三種類のLightをまずCPU側のメンバ値に置く。通常／骨ありモデルの`BindForObjectDraw()`が行列・Camera位置とともに、描画一回分のUpload Bufferへ値をコピーする。GPUが参照するBufferは枠ごとのFence待ちが済むまで再利用しない。余分になったLightの常設Upload Buffer三つはなくした。初期Light値、Sceneが渡す数値、描画順、Shader、モデル・素材は変えていない。
- 最終コードのDevelopment／Release／最後にDebug x64ビルド成功。出力にプロジェクト設定由来の警告・エラーはない。DevelopmentのStage1 Capture Smokeは終了コード0、12枚保存・12枚可視。今回の実画像`resources/Captures/Screenshots/ribonin_challenge_20261007_042743_812.png`を開き、部屋・Player・鏡板・ピンク／緑／黄色の危険Lightを確認。Debug walk-scene Smokeも終了コード0で、frame30とframe90の画像を開き、通常モデルの球体武器、人物のAnimationモデルと照明を確認した。画像保存数だけで表示成功と判断していない。
- DebugのStage1GameplaySmokeも今回の実行で終了コード0・`SUCCESS`。鏡とLaser、危険Lightの合成反射経路、Camera、Collider等の必須判定を通過した。現行配置の`hazardReflection=0`は観測値で成功条件外。`.vcxproj`のClCompile／ClIncludeはワイルドカード0、重複0、filtersとの差0、`git diff --check`も成功。
- 未確認：動くLightのちらつきの実再現と修正前後の定量比較。鏡面専用・壁越し表示専用のConstant Buffer寿命は別経路で、今回の修正範囲外。通常F5のTitleから手入力Enterで本編へ入る操作も未確認。

## GPU粒子の固定画像を使用側で準備（2026-10-07）

- 原因：`DebugParticleEffects::InitializeResources()`が`circle2.png`を先読みしていたが、戻り値を確認せず、`GPUParticle::Draw()`は同じパスのSRVを無条件に取得していた。画像を読めない時はDebugでassert、assert無効時も`TextureManager::GetSrvHandleGPU()`の`at()`が未登録名で失敗する。実素材を欠損させた再現ではなく、現在の呼出順とコードから確認した失敗経路。
- 変更：既存の`GPUParticle`へ`PrepareTexture()`と準備成否を追加。Debug Sceneの先読み順の三番目で`DebugParticleEffects::InitializeResources()`がこれを呼ぶ。Texture実体とSRVの所有は従来どおり`TextureManager`、`GPUParticle`は自分の固定画像パスと「描画に使えるか」だけを持つ。`PrepareTexture()`は失敗パスをログへ残し、`Draw()`は未準備ならSRVへ進まない。`Finalize()`で準備状態を消す。Emitterや粒子Shader、発生時刻、画像指定値は変更していない。
- Development x64ビルド成功。Debug Sceneのwalk-scene Smokeは終了コード0で、実画像`resources/Captures/Screenshots/ribonin_challenge_walk_scene_frame30_20261007_042038_199.bmp`と`ribonin_challenge_walk_scene_frame90_20261007_042039_232.bmp`を開き、通常モデル・歩行Animation・手持ち球・足元付近の光を確認した。時間再生Smokeも終了コード0で、従来の粒子発生・逆再生と不足画像グループ拒否が`SUCCESS`。
- 未確認：実際に`circle2.png`が欠けた時のDebug画面とログ、光の全種類の表示。足元の光が見えることだけでGPU粒子全種類の描画を保証しない。

## 粒子グループの画像登録を生成前に確認（2026-10-07）

- 原因：`DebugParticleEffects::InitializeResources()`は4枚の画像を先読みするが、戻り値を確認しない。`ParticleManager::CreateParticleGroup()`／`CreateRingParticleGroup()`／`CreateCylinderParticleGroup()`も、画像パスを持つグループを登録する前に画像の登録成功を確認していなかった。画像を読めないまま粒子が発生すると、`ParticleManager::Draw()`の`TextureManager::GetSrvHandleGPU()`が未登録パスを参照する。これはコード上の失敗経路で、実際に欠損画像を置いた再現ではない。
- 変更：既存の`ParticleManager`へ`PrepareGroupResources()`を追加し、三種類の生成関数が同名グループを消す前に、画像読み込みと粒子用SRVの空きを確認する。失敗時はログを残して新しいグループを登録せず、既存の同名グループも保持する。Debug Sceneの先読み順、正常な画像のSRV番号、粒子形状・発生・描画設定は変更しない。
- 最終コードでDevelopment／Release／最後にDebug x64のビルドが成功し、出力にプロジェクト設定由来の警告・エラーなし。DevelopmentとDebugの時間再生Smokeは終了コード0で、CylinderとLightCoreのParticle発生・逆再生を含め`SUCCESS`。DevelopmentのDebug UI walk-scene Smokeも終了コード0で、通常モデル・歩行Animationを含む二枚の画像を実際に開き、人物・手持ち球・床・光の表示を確認した。画像の保存成功だけを表示確認とは扱わない。`.vcxproj`のワイルドカード0、重複0、filtersとの差0、`git diff --check`成功。
- 追加検証：Debugの時間再生Smokeで実素材を動かさず、存在しない画像パスを通常・Ring・Cylinderの同名置換と新規作成へ渡した。最終コードのDevelopment／Debug x64で終了コード0、`missingParticleTexture=rejected+originalPreserved`を確認。元の三グループは各1粒子を保持し、新規グループも空のTexture登録も作られず、今回のゲームログに失敗パスが4回記録された。Release x64もビルド成功。最後のビルドはDebug x64で、プロジェクト設定由来の警告・エラーはなかった。
- 未確認：実際の素材欠損状態でDebug画面全体を起動した時の表示、SRV枠を使い切った時の挙動、すべてのParticle種類の視覚確認。`GPUParticle`の固定画像参照、SkyBoxや環境Textureの失敗経路はこの変更の対象外で、次の点検対象とする。

## 通常F5でのTitle入力を再確認（2026-10-07）

- Visual Studioの`Development|x64`からF5で今回の実行ファイルを起動し、現行`TITLE`の3D背景・Title/ENTER画像とGame Viewの表示を確認した。`TITLE`は`GameSceneRegistration.cpp`により`Stage1(true)`として登録され、Enter後も同じSceneで`BeginStageStart()`を呼ぶ。旧`TitleScene`や`TITLE→LOADING→STAGE1`へのScene切替ではない。
- Game View内をクリックしてからWindows画面操作による短いEnterを二度送ったが、Player登場演出へは進まず、今回の`logs/20261007_040307.log`に`Temporary title: Enter started Stage1 gameplay.`もなかった。`Framework::Update()`は毎フレーム`Input::Update()`を先に呼び、`Input::TriggerKey()`は前フレーム0・現フレーム1の変化だけを読む。今回のログにキーボード取得失敗の記録はなく、短い自動押下がフレームへ入らなかった可能性と実入力の不具合は切り分けられていない。
- Exit Game→Yesで終了し、Visual Studioのデバッグ出力で終了コード0を確認。今回のログは終了時まで更新され、`Dumps`内の最新ファイルは2026-09-30のまま。コード・素材・配置は変更していない。ユーザーには実キーでの確認を依頼し、回答待ちの間はEnter経路を確認済みとしない。

## 危険Lightの反射経路を配置から分けて確認（2026-10-07）

- 調査：`Stage1GameplaySmoke::Update()`の`hazardReflection`は、現在動いている危険Lightが鏡へ当たった時だけ1になる観測値で、成功条件ではなかった。現行`stage1.json`の固定鏡はX約0・Z8、携帯鏡の初期位置はX20・Z30、鏡床はX5・Z11。`StageHazardLights::Update()`の危険Lightは別の位置・軌道で動く。たとえばZ30の横向きLightはX方向へ進むが、初期携帯鏡はZ方向を向くため面と平行になる。この配置で`hazardReflection=0`でも、反射処理の故障とは断定できない。
- 変更：自動確認時だけ、携帯鏡の表側から中心へ向かう長さ0.5の線分を`Stage1::ReflectHazardLightSegments()`へ渡す。`Stage1`は本編の反射コールバックを渡すだけにし、検証用の線分と合否は`Stage1GameplaySmoke::Initialize()`が所有する。`hazardReflectionRoute`を合格条件へ追加し、従来の`hazardReflection`は配置による観測値のまま残した。ゲーム中のライト・鏡の位置、反射回数、描画順は変更していない。
- Development／Release／最後にDebug x64をビルドし、出力にプロジェクト設定由来の警告・エラーはなかった。DevelopmentとDebugの`STAGE1`直接起動Gameplay Smokeは両方とも終了コード0・`SUCCESS`・`hazardReflectionRoute=1`。同じログの`hazardReflection=0`は現行配置の観測値であり、画面内の実ライトが鏡へ反射した証拠にはならない。
- Developmentの既存Stage1 Capture Smokeも終了コード0、12枚保存・12枚の可視画像を記録。実画像`resources/Captures/Screenshots/ribonin_challenge_20261007_040100_329.png`を開き、Player・白い携帯鏡・部屋・ピンク/黄/緑の危険Lightが描かれていることを確認した。保存数だけを表示成功の証拠にはしていない。`.vcxproj`のワイルドカード0、重複0、filtersとの差0、`git diff --check`成功。
- 未確認：実際の危険Lightを鏡へ向けたプレイ時の反射像、プレイヤー操作で鏡を動かした時のライト反射、通常F5からEnterで本編に入る経路。テスト専用の線分はこれらの実画面確認を代替しない。

## Texture読込失敗後の未登録参照を防止（2026-10-07）

- 原因：`TextureManager::LoadTexture()`は画像読込に失敗すると登録せず`false`を返すが、`Object3d::SetTextureOverride()`と`SceneEditor::ScanResourceShelf()`は戻り値を見ずに`GetSrvIndex()`へ進んでいた。取得側はDebugでassert、assert無効時も`at()`が未登録名で例外を出す。正常な画像では同じSRVを使うため、見た目の設定値は変更しない。
- 修正：Object3dは読込成功後だけ個体のTexture上書き番号を更新し、失敗時は以前の上書きを維持してパスをログへ残す。素材棚は読めない画像を登録・寸法取得・サムネイル表示へ進めず、パスをログへ残して次の素材を調べる。`LoadTexture()`自体が登録済み画像を成功として返すため、素材棚側の重複した`Contains()`判定は不要。
- 変更後のDevelopment x64ビルドと起動順Smokeは成功。Visual StudioのF5で現行`TITLE`の3D背景・二枚の画像を確認し、DEBUG Sceneでは通常モデル、Animationモデル、手持ち球の実描画と、素材棚42件・Textureサムネイルの表示を確認した。Exit Game→Yesで終了し、新しいダンプはなかった。最初のF5試行は画面を再取得する前に終了しており、理由未確定。再試行で画面確認できたことと、最初の終了理由は分けて扱う。
- `STAGE1`直接起動のGameplay Smokeも今回の実行で終了コード0。正常なTexture上書きを使うStage1の初期化と、鏡・Player・Cameraの判定を通った。ただし`hazardReflection=0`はこのSmokeの合格条件外で、危険Lightの反射像を確認した意味ではない。
- 未確認：実際に壊れた画像を素材棚へ置いた時のログ・UI、および欠損パスをObject3dへ渡した時の実行結果。GPU資源生成や転送で失敗する低レベル経路は今回のガードでは解決していない。

## クラスの説明と実際の所有者を照合（2026-10-07）

- `engine`と`game`のヘッダで定義される105件のclassは、直前に役割コメントがあることを確認した。存在だけで内容の正しさは保証できないため、所有と呼出順が紛らわしい箇所を利用側と照合した。
- `Mirror`は形と反射計算を持ち、画面描画は現在`FixedMirror`などの利用側が行う。旧コメントに残っていた「将来的に専用描画クラス」という説明を現行コードへ合わせた。
- `StageTitleOverlay`は二枚のSpriteとFade／点滅状態を持ち、`Stage1`が作成・更新時期・描画順を決める。`Object3d`は個体のTransform・Animation・Light描画データを持つ一方、環境反射率のSet/Getは共有`Model`へ委譲する。いずれもコード変更ではなく、所有者と影響範囲のコメント訂正。
- `Enemy`は現状AIを持たず、見た目とColliderの位置同期が役割。`Stage1::sceneObjects_`は床だけでなく発射装置・Switch・Doorも所有する。`StageHazardLights`は自分の初期位置と移動範囲を保持する。将来予定や昔の部品数で説明せず、現在の実装に合わせた。
- コメント訂正後のDevelopment x64ビルドは成功。`STAGE1`を直接起動するGameplay Smokeは今回のログで終了コード0、Collider・EnemyManager・鏡の反射判定・Cameraの項目が合格した。ただし`hazardReflection=0`はSmokeの合格条件から外れており、危険Lightの鏡反射や実画面を確認した証拠ではない。起動順Smokeも終了コード0で各SceneのDrawを記録したが、通常のEnter入力は通らない。

## 提出用Titleの切り抜きサイズを一か所で管理（2026-10-07）

- `StageTitleOverlay::Initialize()`は二枚のSpriteに切り抜きサイズを設定する。`StageTitleOverlay::Update()`は同じ数値を別に保持せず、`Sprite::GetTextureSize()`から幅と高さの比率を読む形へ整理した。設定値・配置式・Fade・点滅・描画順は変更していない。
- 変更後のDevelopment x64ビルドは成功。Visual StudioのF5でLoadingの後、現行`TITLE`の3D背景とTitle／ENTER画像を実画面で確認し、Exit Game→Yesで終了した。今回のログが作られ、新しいダンプはない。Developmentの起動順Smokeも終了コード0でLoading・Title・Stage1のDrawを記録したが、手入力Enterから本編への開始はこの変更後も未確認。

## Model公開APIの説明を加えた後の確認（2026-10-07）

- `Model.h`へ所有者・共有材質の影響・Animationの更新順を説明するコメントと、使用する標準ヘッダを明示した。関数の処理、描画設定、素材は変更していない。
- 変更後のDevelopment／Release x64ビルドは成功した。Developmentの起動順Smokeは今回の実行で終了コード0、Loading二回・Title・Stage1の各Drawを記録した。ただしSmokeはEnter入力と同じScene内の開始処理を通らない。
- Development x64をVisual StudioのF5で起動し、現行`TITLE`の3D背景とTitle／ENTER画像を実画面で確認した。Game Viewを選んで自動Enterを送ったが、`Temporary title: Enter started Stage1 gameplay.`は今回のログになく、画面もTitleのままだった。`Input::TriggerKey()`は各Frameの前回状態との変化を見るため、短い自動押下だけでは入力不具合と断定できない。Exit Game→Yesで通常終了し、今回作成のダンプはない。手入力での開始は未確認。

## Stage1の説明と共有Modelの変更範囲（2026-10-07）

- 提出用Titleは別Cameraを生成せず、`Stage1::InitializePlayerAndCamera()`が本編用の位置・回転を保存した後、同じ`mainCamera`を固定位置へ動かす。Enter後の`BeginStageStart()`で登場演出を始める。`Stage1.h`と`ArchitectureGuide.md`の「本編Cameraと分けて保持」という誤解を招く説明を訂正した。
- `Stage1::Draw()`はTitle中も`hasStageFrameBeenDrawn_`を立てるが、Title中の`Stage1::Update()`は早期returnして`UpdateStageBgm()`を呼ばない。直起動では最初のStage描画の次のUpdate、Title経由ではEnter後の本編UpdateでBGMを始める。コメントだけを実際の順番に合わせ、音量・音源・再生時刻は変更していない。
- `ModelManager`がModelをファイル名で共有し、`Model::Initialize()`が頂点・材質・GPU資源を準備する。通常・骨あり描画とAnimationは同じModelの別実装ファイル、個体のTransform・SkinCluster・描画設定は`Object3d`が担当する。行数だけを理由にModelを新クラスへ分けない。
- `Model::Initialize()`の確認用画像への置換条件は、画像パスが空、`None`を含む、先頭が`*`の三つ。以前の「画像指定のない材質だけ」というコメントを訂正した。確認用画像が見えても元画像の表示成功ではない。
- `DebugAnimationPreview::Initialize()`だけは、手持ち球の`Object3d`ではなく共有の`Model::SetTexture()`を呼ぶ。これは同じ`debug/sphere.obj`を参照し、個別の上書きを持たないObjectにも影響し得る。実際の別Objectの見た目の変化は未確認で、テクスチャ変更になり得るため修正はユーザー判断待ち。
- `Model.h`の公開APIに、Managerから借りる共通部、Object3dが所有するSkeleton/SkinCluster、Animation適用→骨行列→GPU Palette→描画の順番をコメントで明記した。`Model::IsLoaded()`はGPU Bufferの存在だけを返し、画像の読込成否を保証しない。`Model::SetEnvironmentCoefficient()`も共有Modelの材質を書き換えるため、個体別の値が必要かは見た目への影響を確認して判断する。

## 共有状態・未使用API・進行パターンの判断（2026-10-07）

- `Logger.cpp`の`gLogPath`はファイル内だけで見える保存先で、`Framework::Initialize()`が`SetLogPath()`から設定する。共有状態ではあるが、全箇所へ出力先を渡すために新しい所有クラスを増やしても挙動は同じなので、現時点では維持する。並行書込みの安全性まで確認した意味ではない。
- 数学関数は`MyMath`名前空間にまとめられ、ヘッダに`using namespace`は置いていない。名前空間から個別のクラスへ機械的に移す必要はない。
- `TitleScene::IsFinished()`の状態変数は、現行コードで`false`への代入しかなく、`SceneManager`もこのAPIを読まない。公開APIと戻り値を保ったまま、未使用の`isFinished_`と誤った「SceneManagerが終了する」説明を削除した。切替は`TitleScene::UpdateSceneTransition()`が`ChangeScene()`または`ChangeSceneWithLoading()`を予約する。Development x64で`TITLE_ORIGINAL`を直接起動し、12枚の導入画像からノイズ・Camera後退・下向きまで進んだ今回のログと、3D背景の実画面を確認した。Exit Gameから終了し、新しいクラッシュダンプはなかった。
- 通常のF5で現行`TITLE`を表示した後、Scene一覧を操作する前にゲームウィンドウが消えた回があった。Visual Studioのデバッグ出力は終了コード0で、新しいダンプもない。原因は未確定で、クラッシュや旧Titleの不具合とは断定しない。
- `D3DResourceLeakChecker`は呼出元がないが、終了時のDirectX診断に使える公開クラスである。`DirectXCommon`が関数内staticで生存する間に無条件で置くと、意図しない生存資源も報告し得る。単純な削除・自動挿入はせず、診断導入を別判断に残す。
- 従来の`TitleScene`は`PreparationStep`と`PresentationStep`の列挙型で短い決まった順番を表す。現在の処理に独立したStateオブジェクトを増やす根拠は見つからない。FactoryもSceneとObjectの生成責任に使われており、採点のためだけに別パターンを足さない。

## 提出用TitleのSprite所有者を分離（2026-10-07）

- `Stage1.cpp`内にあった既存の`StageTitleOverlay`を、同じStage1用の`StageTitleOverlay.h/.cpp`へ移した。二枚のSpriteの読込・生成・所有・配置・フェードと点滅はこのクラス、起動進行は`Stage1`、描画は従来どおり`SceneRenderPipeline::DrawSprites()`が担当する。画像名・切り抜き範囲・位置・表示時間・呼出順は変えていない。
- 新規二ファイルを`.vcxproj`と`.vcxproj.filters`へ各一件ずつ明示登録した。Development x64のビルドが成功し、Visual StudioのF5で`TITLE`のStage背景・二枚の画像を実画面で確認した。Exit Game→Yesで終了し、今回のログが作られ、新しいクラッシュダンプはなかった。
- 分離後のDevelopment x64起動順Smokeは終了コード0で、Loading→Title→Loading→Stage1のDraw回数を記録して成功した。ただし通常のEnter入力は通らないテストなので、Titleから本編への手入力は引き続き未確認。

## 現在のTitle起動経路の再照合（2026-10-07）

- `GameSceneRegistration::RegisterGameScenes()`の`TITLE`は、現在`Stage1(true)`を生成する。固定CameraのStage背景に二枚のSpriteを重ね、`Stage1::Update()`がGame ViewでEnterを検出したら同じScene内で`BeginStageStart()`を呼ぶ。12枚の画像を使う従来の`TitleScene`は`TITLE_ORIGINAL`として残る。
- `ArchitectureGuide.md`の「Titleから再Loadingを経てStage1」など旧構成の説明を現行経路に合わせた。`TitleScene::IsFinished()`には、既存の`BaseScene`仮想APIの上書きであることを`override`で明示した。処理順・描画内容・入力条件は変更していない。
- Development x64のVisual Studio F5では、現在の`TITLE`画面で3DのStage背景とTitle／ENTER画像が表示された。短い自動Enter入力では本編開始を観測できず、通常の手入力による遷移は未確認。ゲームのExit Game→Yesで終了し、今回のログに開始記録や新しいクラッシュダンプはなかった。
- Development x64の`StartupRouteSmoke`はLoading→Title→Loading→Stage1の描画回数を記録して成功した。ただしテストが`TITLE`から`STAGE1`へ直接Scene切替を予約するため、通常のEnter判定と同じScene内の登場演出を検証しない。これを通常経路の成功証拠にしない。

## 共通資源の終了順（2026-10-07）

- `Framework::Initialize()`はTexture→Model→通常Particle→GPU Particleの順に共通資源を用意する。`SceneManager::FinalizeCurrentScene()`はGPU完了を待ってSceneを解放する。`Framework::Finalize()`はその後、GPU Particle→通常Particle→Model→Textureの順に解放するよう整理した。以前はTextureを先に解放していたが、現行の破棄処理でこれがクラッシュを起こした証拠はない。所有・参照関係を読みやすく揃えるための変更で、描画・入力・Title演出の処理順は変えていない。
- `ModelManager::Finalize()`とデストラクタは、Modelが参照する`ModelCommon`を残して先にModelを解放する。通常終了ではFrameworkから呼び、Frameworkの終了処理が呼ばれず静的なManagerが破棄される場合も同じ順序を使う。`Model`の破棄時に現在その参照を使うコードはないので、この変更を既存不具合の修正とは呼ばない。
- Debug x64のソリューションビルドと、通常Development実行ファイルを上書きしない別出力でのDevelopment x64ビルドは警告・エラーなし。両構成の今回の`StartupRouteSmoke`は、各プロセス終了コード0、Loading→Title→Loading→Stage1のDraw数が各1以上、今回のログが`SUCCESS`で、新しいクラッシュダンプなし。自動テストはTitleでの手入力や実画面の見た目を確認しない。ユーザーが編集中のTitleと通常F5経路は今回触れていない。

## Stage1のLight描画準備失敗を伝える（2026-10-07）

- `LaserRenderer::Initialize()`は失敗を`bool`で返していたが、`StageLightPuzzle::Initialize()`と`StageHazardLights::Initialize()`はその失敗を握りつぶし、Rendererがないまま当たり判定を進められた。光が見えないのにPlayerだけ当たる状態を防ぐため、二つのギミックから`Stage1::InitializeRenderSystems()`／`InitializeStageGimmicks()`へ失敗を伝え、Stage1を更新・描画へ進めず終了コード1で止める。成功時の生成順、Lightの色・太さ・軌道・判定値、Title表示は変更していない。
- Debug x64ソリューションと別出力のDevelopment x64をビルドし、警告・エラーなし。今回のDevelopment x64直起動`Stage1GameplaySmoke`は終了コード0、ログ`SUCCESS`。両構成の今回の`StartupRouteSmoke`も終了コード0、Loading→Title→Loading→Stage1を描画して`SUCCESS`。Development x64のStage1撮影Smokeは終了コード0、12/12枚を保存し、最初のPlayer画像と本編画面の光の線・部屋を実画像で確認した。今回の新しいクラッシュダンプはない。
- GPU資源不足やShader失敗を実際に起こす失敗経路は未再現なので、戻り値の配線を確認した段階。Development x64の通常F5からTitleでEnterを押す経路も、ユーザーが仮Titleを編集中のため今回未確認。撮影Smokeの成功だけで、その入力経路まで確認済みとはしない。

## Stage1のLight設定を読む場所（2026-10-08）

- `Stage1::InitializeStageGimmicks()`と`InitializeGameplaySmoke()`は、`StageLightPuzzle::Settings`を読むだけなので`const`参照にした。`ApplyStageMapData()`とEditorの鏡変更コールバックも、現在の回転量を読むだけの箇所では値を返す`GetLargeMirrorRotationAmount()`を使う。設定を書き換えるJSON反映・Editor操作は、従来どおり可変参照を使う。値・計算順・公開APIは変更していない。
- Debug x64と別出力のDevelopment x64をビルドし、警告・エラーなし。今回のDevelopment起動順Smokeは終了コード0、Loading→Title→Loading→Stage1のDraw数が各1以上。Stage1撮影Smokeは終了コード0、12/12枚保存し、本編画面の部屋と光の線を画像で確認した。新しいクラッシュダンプはない。鏡のEdit Viewでの手入力と通常F5のTitle→本編は今回未確認。

## Debug SceneのParticle準備結果（2026-10-08）

- `DebugParticleEffects::InitializeResources()`はTexture三枚とGPU粒子の固定Textureの読込結果を確認する。九つのGroupは`ParticleManager::CreateParticleGroup()`／`CreateRingParticleGroup()`／`CreateCylinderParticleGroup()`が直接返す`bool`を順に確認する。最初に追加した別の名前リストと`GetPlaybackSnapshot()`での再検索は不要になった。`DebugScene::InitializeSceneResources()`は失敗をログに残してSceneの更新・描画を始めない。成功時の読込・生成・Emitter設定順、モデル・Light・Titleの見た目は変えていない。
- Debug x64ソリューションと別出力のDevelopment x64のビルドは警告・エラーなし。今回のDevelopment `walk-scene`は終了コード0、frame90の実画像で通常モデル・Animationモデル・手持ち球・地形・照明を確認。`TimePlaybackSmoke`は終了コード0で、欠損Particle画像や不正な円柱寸法を生成関数が`false`で拒否し、既存Groupを保つことまで確認。起動順Smokeも終了コード0でLoading→Title→Loading→Stage1のDraw数を確認。新しいクラッシュダンプはない。
- `DebugParticleEffects`自身の画像欠損を実際に発生させるテストと、全九種類の粒子の見た目確認は未実施。生成関数の戻り値は登録完了を示すが、`InitializeInstancingResources()`より下のGPU資源不足時の挙動はまだ保証しない。通常F5でのTitle手入力も、仮Titleの作業を守るため今回未確認。

## Stage1の二つの描画Passの責任（2026-10-08）

- `Stage1::Draw()`は鏡Texture→通常Game View→Sprite→`SceneRenderPipeline::End()`の順だけを決める。`StageSceneRenderer::Draw()`は通常CameraでObject一覧、JSON配置物、鏡面、携帯鏡・Player、光を描く。`StageReflectionRenderer::DrawOne()`は一フレームに一枚の鏡を選び、反射Cameraで鏡面自身を除くSceneを描いてから通常Cameraの行列へ戻す。共通のObject/SpriteループとPostEffect／SwapChain終了は`SceneRenderPipeline`が持つ。
- 二つのStage用Rendererは対象が重なるが、鏡面を描くか、どのCamera行列を使い、いつ元へ戻すかが異なる。行数を減らすためだけの統合はせず、`StageSceneRenderer::Draw()`の「鏡面が最後」という実際と異なるコメントを正した。コードの描画順・Pipeline設定・見た目は変更していない。

## 6月の28項目評価に対する現行監査（2026-10-06、作業中）

これは外部評価者による再採点ではなく、現行コードで確認できた範囲と残作業です。「済」は記載した範囲の証拠があるという意味で、全体の100点を意味しません。点数を上げるためだけの分割・パターン追加・C++規格変更は、読みやすさと動作への影響を確認してから判断します。

| 番号 | 評価項目 | 現行コードで確認したこと・残ること | 状態 |
| --- | --- | --- | --- |
| 1 | フォルダ構成 | `engine`・`game`・`tests`を役割別に配置。全フォルダの適切さは未再判定 | 一部 |
| 2 | VSフィルタ一致 | メイン261件・Levelテスト11件・Audioテスト9件で、登録漏れ・余分・重複・ワイルドカードが各0件 | 済 |
| 3 | 不要Platform | メインはDebug／Development／Releaseのx64のみ | 済 |
| 4 | 外部ライブラリ分離 | `project/externals`にAssimp・DirectXTex・ImGui・JSONを分離 | 済 |
| 5 | ファイル・フォルダ命名 | 多くは役割名だが、全ファイルの表記ゆれは未再判定 | 一部 |
| 6 | クラスコメント | 自作のengine・gameヘッダで定義されるclass105件すべてに直前の役割コメントを確認。`Mirror`・`StageTitleOverlay`・`Object3d`は現在の所有者と影響範囲に合わせて訂正。各コメントの意味と全呼出元の照合は継続 | 一部 |
| 7 | 関数コメント | `GPUParticle`・`Object3dCommon`・`SpriteCommon`・`ModelCommon`に加え、`Model`の公開APIにも所有権・呼出順・共有材質の影響を記載。ほかの公開関数は継続点検 | 一部 |
| 8 | 処理の意図コメント | `Object3d::Update()`の「1/60秒」、Stage1のTitle用CameraとBGM順、Modelの確認用画像条件、Mirror描画の所有者、共有Modelの環境反射率を現行実装に合わせた。他にも古い説明がないか継続点検 | 一部 |
| 9 | コメントアウトされた不要コード | 元の指摘箇所`Sprite.cpp`は整理済み。自作のengine・game・testsで`#if 0`、ブロックコメント、コード形の行コメントを検索し、該当0件 | 済 |
| 10 | スペリング | 6月時点の指摘はなし。現行識別子全体の再監査は未完 | 一部 |
| 11 | 命名規則 | クラス名・多くのメンバ名は一定。古い`modelData`など、末尾`_`の有無は混在 | 一部 |
| 12 | 定数化 | `ParticleEmitter`の60FPS基準値と`Input.cpp`のスティック範囲・遊び幅を命名済み。提出用Titleの切り抜きサイズはSprite自身の設定値を再利用し、同じ数値の二重管理をなくした。ほかの数値は用途を確認して選別する | 一部 |
| 13 | グローバル変数 | 公開の共有変数は見当たらない。`Logger.cpp`のファイル内静的な保存先は必要な共有状態として維持。並行書込みは別途検証が必要 | 判断済 |
| 14 | グローバル関数 | 汎用数学関数は`MyMath`名前空間に属し、ヘッダに`using namespace`はない。クラスへの機械的な移動は不要 | 判断済 |
| 15 | スマートポインタ | 自作の本番C++コードに直接の`new`／`delete`は検索で見つからず、`unique_ptr`・`ComPtr`を使用 | 済 |
| 16 | const参照 | `Sprite`などの読み取り専用引数を整理。残る主な大きい値渡しは、同名グループ削除中の参照切れを防ぐ`ParticleManager`の名前、移したCSVセルを処理する`MapChipField::Trim()`、画素・フレームの所有権を渡す`CaptureManager`で、用途を確認して維持。独自構造体までの全件点検は継続 | 一部 |
| 17 | 関数の長さ・責務 | `ParticleManager::Update()`を風・運動・外観・GPU書込へ分け、`GPUParticle`を資源生成とEmitter/GPU更新へ分けた。`LevelLoader::Load()`/`Save()`からLightingのJSON読込・保存を同じ.cpp内の専用関数へ分け、読込・保存順を維持。`Stage1::ApplyStageMapData()`は分類を`StageMapObjectIndex`、生成をFactory／Runtimeへ委譲し、Sceneでは事前確認と反映順をまとめるため、行数だけで再分割しない。他の長い関数は継続監査 | 一部 |
| 18 | 重複処理 | Particleの既存グループ破棄、GPUParticleの同設定UAVバッファ5件の生成、Inputのゲームパッド4軸の正規化・遊び幅判定を共通化。他領域の重複は未監査 | 一部 |
| 19 | データ駆動 | Stage配置・LightはJSON、マップチップはCSVに対応。Title等の固定値を外出しすべきかは用途次第 | 一部 |
| 20 | 名前空間 | 自作ヘッダ内の`using namespace`は検索で0件。`Logger.h`は不要なDirectX依存と名前空間後の余分なセミコロンを除去。エンジンクラス自体の名前空間化は未判断 | 一部 |
| 21 | モダンC++ | `auto`・lambda・`unique_ptr`・`span`などを使用。ゲーム本体と独立テストの`.vcxproj`はC++20で、C++23移行は未実施 | 一部 |
| 22 | コードの死骸 | 空のリークチェッカー、`TitleScene`の常にfalseの状態変数、粒子グループの未使用`textureSrvIndex`を削除。`D3DResourceLeakChecker`は診断用の公開クラスとして保持し、無検討に自動挿入しない | 一部 |
| 23 | 未使用コード | `BaseScene::IsFinished()`と`TitleScene::IsFinished()`は既存APIとして保持し、後者の戻り値は従来どおりfalse。`D3DResourceLeakChecker`は診断用として保持。利用方針は上記で判断したが、他の未使用コードは継続監査 | 一部 |
| 24 | カプセル化 | Manager内部型をprivateに整理。`Model::GetModelData()`は読み取りだけと確認してconst参照へ変更。`Framework`の終了状態・SceneFactory所有をprivateにし、Gameは目的別関数で操作。`BaseScene`のprotected状態変数をなくし、各SceneがCameraをprivateで所有する。Debugの手持ち球だけは共有ModelのTextureを変更しており、個体別上書きへの変更は見た目への影響を確認してから判断する | 一部 |
| 25 | クラス分割・移譲 | Scene生成はFactory、描画準備はRenderContext、描画はPipelineへ分担。提出用TitleのSprite生成・所有・更新は既存の`StageTitleOverlay`を専用ファイルに分離。Modelは資源所有、ModelManagerは共有・再読込、Object3dは個体状態と確認し、行数だけの再分割は不要。他領域は継続監査 | 一部 |
| 26 | ポリモーフィズム | `Framework`→`Game`、`BaseScene`→各Scene、`AbstractSceneFactory`→`SceneFactory`は仮想関数の呼び分けと基底のvirtualデストラクタを確認。`Collider`は共有状態の継承であり、仮想関数の呼び分け例には数えない | 済 |
| 27 | Stateパターン | `SceneManager`が現在の`BaseScene`を所有してUpdate/Drawを委譲し、Loading・Title・Stage1などが次Sceneを予約する構成がScene単位のState Pattern。Title内の演出列挙型とStageStartは短い固定順なので別Stateクラスを増やさない | 判断済 |
| 28 | デザインパターン | `Framework::Run()`は共通順序を固定するTemplate Method、`AbstractSceneFactory::CreateScene()`と`SceneFactory`はFactory Method、SceneManagerはState Pattern。`Object3dFactory`は単純Factoryとして生成を集約。Singletonだけの構成ではない | 判断済 |

残る優先作業は、説明の誤りと未使用APIを利用実態から調べること、長い関数・公開可変データの責任を点検すること、そして変更後のDevelopment x64・F5通常操作を実画面で再確認することです。StateクラスやC++23を採点のためだけに導入すると、この案件の「簡潔で既存動作を守る」という条件と衝突し得るため、必要性を先に説明します。

`GPUParticle::Initialize()`、`SrvManager::Allocate()`、`DirectXCommon::CompileShader()`の失敗判定は後続の節で改善した。`DirectXCommon::CreateBufferResource()`のDebug `assert`も最新の節で空結果とログへ変更し、骨ありモデルと画像転送の呼び出し元まで失敗を伝えるようにした。ただしGPU資源不足時の実機試験はなく、RenderTextureやDepthStencilなど別の生成経路も残る。通常成功経路のビルド・描画テストだけで全資源の失敗処理を確認済みとはしない。

`LevelLoader`のLighting保存は、読込と同様に役割別関数へ移した後、既存のBlender座標の不一致を別に修正した。以前から読込時はY/Zを入れ替える一方、保存時は入れ替えていなかったため、保存・再読込で照明のVector3が変わる。独立テストで修正前の失敗と修正後の成功を確認した。現在の`stage1.json`はEngine座標なので、Engine座標の保存値は従来のままにする。

## 以前の写真にある壁越しの黄色・橙色の点（2026-09-29）

- 写真ファイルの保存時刻は2026-09-28 14:31。写真の点は黄色と橙色で各一つ。写真より後のコミット`c777ba4`（同日19:15）で、`StageSceneRenderer::Draw()`から発射装置二つへの`DrawOccludedSilhouette()`呼び出しが削除されている。その直前の`dba4664`には、壁越しに黄色`{1.00, 0.88, 0.12}`と橙色`{1.00, 0.48, 0.08}`で描く二つの呼び出しがある。点の数・色・時系列が一致するため、以前の写真の直接原因はこの意図的な壁越し描画と判断できる。
- 現行コードでは`DrawOccludedSilhouette()`の呼び出し元はなく、発射装置は通常の深度判定で描く。Development実画面の背面・斜め背面では点が出ず、Release実画面の正面も表示は正常。Releaseの同一角度は未確認なので、現行Releaseの全視点で遮蔽が正しいとまでは断定しない。
- `CameraController::SetWallAvoidanceEnabled()`はCamera位置だけの設定で、壁越し描画を自動で有効にしないことをコメントで明記した。`Object3d::DrawOccludedSilhouette()`は他Sceneが意図して使う可能性のある公開APIなので、今回は削除しない。

## Stage1の角度入力とRelease画面の確認（2026-09-29）

- `Stage Player`の角度入力欄は、現在角度を表示していたため、Edit Viewで数値を確定すると見た目だけ旧値へ戻っていた。`CameraController`が所有する目標角度を読むようにし、入力した`-135`度が欄に残り、Game Viewの通常Cameraが斜め後方へ移動することをDevelopment実画面で確認した。値は実行中だけで、配置データは変更していない。
- DevelopmentのGame Viewで正面・左右・背面・斜め後方を確認し、壁越しの黄色い点は再現しなかった。Release版を`RIBONIN_CHALLENGE_START_SCENE=STAGE1`で直接起動すると、正面視点のPlayer・携帯鏡・床・左右の壁は描画された。Releaseでは元画像と同じ背面角度へ自動操作できておらず、黄色い点の原因・解消は未確認。
- ReleaseのTitleに表示されたチェッカー画像は`TitleObjectManager::Initialize()`が`uvChecker.png`を初期Spriteとして指定しているため、それだけで読込失敗とは判定しない。Releaseの起動順Smokeを再実行し、Loading→Title→Loading→Stage1のDraw回数7／8／7／7と成功を確認した。これは実キー入力と各画面の見た目を保証しない。
- Development／Release／最後のDebug x64ビルドが成功。`.vcxproj`のClCompile／ClIncludeにワイルドカードはなく、`git diff --check`は空白エラーなし。

## Stage1の発射装置の描画経路を明確化（2026-09-29）

- `StageSceneRenderer::Context`にあった発射装置二つのポインタは、Rendererでは一度も使われていなかった。発射装置は`Stage1`の`sceneObjects_`に所有され、`SceneRenderPipeline::DrawObjects()`を通って通常の深度判定で描かれる。未使用の受け渡しだけを削除し、生成・配置・描画順は変更していない。
- `stage1.json`では新しい部屋の左右壁がX=13／35にあり、旧配置の`StageMirror`はX=0.03、`MapSphere1`はX=3.90、`PathSphere1`はX=-4.00。`StageLightPuzzle::Settings`の発射装置・SwitchもX=0／-6／-3付近にある。部屋外の配置物が複数あることは確定したが、以前の画像に映る黄色い点との同一性と、壁越しに見える直接原因は未確定。
- Release／Development／最後のDebug x64ビルドが成功。Releaseの起動順SmokeはStage1のDrawを7回実行した。DevelopmentのStage1撮影Smokeは`SUCCESS: scene=STAGE1 photos=12 visible=12/12`。開始演出後の写真ではPlayer・携帯鏡・正面の床壁が描画され、正面からは黄色い点を再現しなかった。後ろ向きの壁は写していないため、遮蔽問題の解決とはしない。`.vcxproj`のClCompile／ClIncludeにワイルドカードなし、`git diff --check`は空白エラーなし。

## Release撮影Smokeが待ち続ける問題（2026-09-29）

- 原因：撮影Smokeの有効化はReleaseでも行うが、進行する`CaptureManager::UpdateAfterDraw()`本体は`USE_IMGUI`内にある。Releaseでは写真も終了判定も動かず、開始ログだけが残っていた。
- 修正：Releaseで撮影Smokeを指定した時は`FinishSmoke(false, ...)`で非対応理由を記録し、速やかに終了する。通常の撮影機能やStage1描画は変更していない。Releaseで再実行し、ログに`FAILURE: Capture smoke requires a Development or Debug build with USE_IMGUI.`が残ることを確認した。
- Development構成では従来どおり12枚のStage1写真を保存してSmoke成功。Releaseの見た目と後ろの壁の遮蔽はこのテストでは確認できない。

## Titleの必須素材が読めない時の安全な経路（2026-09-29）

- 原因：`TitleScene::Initialize()`が初期モデル失敗で途中終了しても、次フレームの`UpdateSceneContent()`は未生成の`skyBox_`を更新していた。初期画像・SkyBox DDSの読込結果も使う前に確認していなかった。
- 修正：既存の`TitleObjectManager`が初期画像の失敗を返し、`TitleScene`が初期化成否を所有する。失敗はログへ残し、未生成のモデル・Sprite・SkyBoxを更新／描画しない。Scene切替入力は使える。正常時の画像名・配置・描画順は変更していない。新しいクラス・ファイルは不要。
- Release／最後のDebug x64ビルドと起動順Smokeは成功し、Loading→Title→Loading→Stage1の各Drawを確認した。Release F5の実画面では、従来と同じ初期UV画像・背景・平面が表示された。`.vcxproj`のClCompile／ClIncludeにワイルドカードなし、`git diff --check`は空白エラーなし。素材を実際に欠損させた時の画面とログは未確認なので、失敗経路の動作はコード確認に留める。

## Title画像追加の失敗経路とRelease起動の再確認（2026-09-29）

- `TitleObjectManager::AddTexture()`のShelf用・直接配置用の両方で`LoadTexture()`の結果を確認する。欠損画像なら`false`を返し、`GetMetaData()`やSprite一覧への登録へ進まない。所有者や既存画像の配置は変えない。
- Release／最後のDebug x64ビルドと`RIBONIN_CHALLENGE_STARTUP_ROUTE_SMOKE`は成功。ログでLoading→Title→Loading→Stage1の各Draw回数が7／8／7／7回と確認できた。`.vcxproj`のClCompile／ClIncludeにワイルドカードなし、`git diff --check`は空白エラーなし。これは実キー入力や見た目の保証ではない。
- Visual StudioのRelease F5で白いLoadingの後にTitleの初期UV画像・背景・平面を実画面で確認した。自動Enter入力ではStage1へ遷移しなかったため、Releaseでの壁越し表示・鏡のマウス操作は未確認。ShellからStage1を直接起動した試行ではゲームWindowが現れず、検証用プロセスを終了した。これを描画不具合の証拠にはしない。
- 欠損画像を実際に追加した時のUI表示は未確認。Title初期画像の読込失敗経路は今回の変更対象外。

## Stage Lightingの操作欄が重なる問題（2026-09-29）

- 原因：`StageLightingWindow()`だけがInspectorへのドッキングを初回表示時に限定していたため、保存済みレイアウトに浮動位置があると、Stage Playerなどの操作欄を覆った。
- 修正：既存のStage編集タブと同じく、Stage Lightingも毎回Inspectorへドッキングする。照明値や描画処理は変更しない。
- DevelopmentのF5からStage1 Edit Viewを開き、Stage LightingとStage PlayerがInspector内の別タブになり、それぞれの数値欄を表示できることを実画面で確認。Release／最後のDebug x64ビルド成功。`.vcxproj`のClCompile／ClIncludeにワイルドカードなし、`git diff --check`は空白エラーなし。値の保存やReleaseの表示確認は今回行っていない。

## Stage1の照明だけを編集する経路（2026-09-29）

- 原因：`StageEditor::DrawLevelControls()`は照明値を一つ変えただけでも`Stage1::ApplyStageMapData(false)`を呼び、そこから`ApplyStageStartSettings()`が未所持の携帯鏡をJSONの置き場所へ戻していた。照明調整と配置の再反映が同じ経路だった。
- 修正：Stage1が既存の`ApplyStageLighting()`だけを呼ぶ操作をEditorへ渡す。照明の読込・保存形式、他のLevel編集の反映経路は変えていない。新しいクラス・ファイルは不要。
- DevelopmentのF5でStage1のEdit Viewを開き、`Directional: intensity`を0.500から0.100へ変更すると、画面の明るさが変わり、鏡とPlayerの描画も続くことを確認。値は保存せずゲームを終了した。照明変更後の「移動済みの鏡がその場に残る」ことは直接の画面操作では未確認で、呼出経路の照合による判断に留める。
- Development／Release／最後のDebug x64ビルド成功。ReleaseのStage1GameplaySmokeはSUCCESSだが、照明UIと鏡移動を組み合わせる試験ではない。`.vcxproj`のClCompile／ClIncludeにワイルドカードなし、`git diff --check`は空白エラーなし。

## 携帯鏡の持ち位置を数値で調整（2026-09-29）

- `CarryableMirror`が持ち位置の`HoldSettings`を所有し、`Update()`が位置・鏡面角度・Colliderへ同じTransformを適用する。縦・横それぞれの前方距離と高さ、縦持ちの左右を一か所にまとめた。既存の反射規則、モデル、初期配置は変更していない。
- `StageEditor`の`Stage Player`をInspectorへドッキングし、`Carried Mirror`の数値を実行中に変更できるようにした。数値のダブルクリック入力と、値が画面に残ることをDevelopment実画面で確認した。コピー用ボタンはC++初期値へ貼る設定文を作るだけで、JSONやソースコードを自動保存しない。
- Development／Release／最後のDebug x64ビルドが成功。ReleaseのStage1GameplaySmokeはSUCCESSで、所持・横持ち・反射・Collider等の必須判定が通った。`.vcxproj`のClCompile／ClIncludeにワイルドカードはなく、`git diff --check`は空白エラーなし。
- 未確認：調整後の鏡を実際に持った時の見た目、Releaseの実マウス操作、裏面の明暗、Stage Lightingが浮動表示のときのInspector操作全般。自動テストの`hazardReflection=0`は合格条件外。全ImGuiの値変更が正常と判断しない。

## Stage1の部屋外表示・編集値の継続調査（2026-09-29）

- `stage1.json`の広いテスト室は左右の壁がX=13／35、入口・出口の壁がZ=-5／75にある。一方、`StageLightPuzzle::Settings`のLaser発射点・SwitchはX=0やX=-6付近で、旧パズル配置が部屋の外に残っている。したがって「部屋外に描画対象がある」ことは配置データから確認できる。
- 以前の画面の黄色い点がその発射装置・Switchそのものか、壁で隠れない原因が深度描画かは未確認。モデルを無条件に非表示にすると旧パズルの遊びや反射検証も消えるため、画面上の対象を同定してから描画範囲・配置を直す。
- `Stage Player`では、実行中のPlayer位置と、次回開始用のJSON `PlayerStart`を分けて編集する。CSVの`P0`があるとJSONは開始位置として使われないので、その場合はJSON編集欄を表示しない。`LevelApiSmoke`でStageが読む入れ子のPlayerStartとEditorが変える元データの一致を確認した。Debug／Development／Release x64ビルドと同テストは成功。実画面での値変更・Save Map・再起動後の位置はまだ未確認。

## 鏡描画と通常描画で共有するGPUデータの点検（2026-09-29）

- 通常ObjectとLaserのPipelineは深度テストが有効で、壁を単純に無視する設定ではない。これだけで黄色い点の原因を特定したことにはならない。
- `StageReflectionRenderer::DrawOne()`は鏡CameraでObjectとLaserを描いた後、同じフレームで通常Cameraの行列へ戻す。従来の`LaserRenderer::Draw()`は同じ頂点・Constant Bufferを再利用し、二つの描画命令を`DirectXCommon::PostDraw()`でまとめてGPUへ送る前に上書きしていた。
- `LaserRenderer`はSwapChain枠ごと・Draw回ごとのUpload領域を所有するよう修正した。`DirectXCommon`のフレーム番号で再利用時だけ書込位置を戻し、枠のFence待ちより前に同じ領域を使わない。既存の鏡→通常の描画順、Laserの色・太さ・本数は変更していない。DevelopmentのStage1 Capture Smokeは12枚保存して成功し、開始演出と通常Cameraの画像を開いてPlayer・部屋・鏡板・Laserを確認。Releaseの起動経路とStage1GameplaySmokeは成功。Development→Release→Debug x64ビルドはすべて成功、プロジェクト設定由来の警告・エラーなし。
- `Object3dGpuData::UpdateTransform()`が書き換える行列とCamera位置は、`BindForObjectDraw()`でSwapChain枠・Draw回ごとのUpload領域へコピーするよう修正した。鏡Cameraで記録済みの通常モデル・Animationモデルを、同フレーム中の通常Cameraへの復元で上書きしない。Development Stage1撮影12枚とDebug Sceneの`walk.gltf`撮影2枚を開き、球体・鏡板・部屋・人物・通常モデルの表示を確認。Releaseの通常起動順とStage1GameplaySmokeも成功した。
- 固定鏡の反射像そのもの、鏡専用・壁越し描画のBuffer寿命、部屋外の黄色い点は未確認。ReleaseではCapture Smokeの終了処理が`USE_IMGUI`外なので、同テストでのStage1画面確認は行えなかった。黄色い点の直接原因をこのBufferだとは断定しない。
- Visual StudioのRelease x64からF5で実起動し、赤い余白のない白いLoadingとTitleの表示を確認した。Windows自動入力のEnterではTitleから遷移せず、DirectInputに届いていない可能性もあるため、人のEnter入力・Stage1でのMouse操作が正常とはまだ判定しない。検証用プロセスは終了した。

## 目的と完了条件

初見のメンバーが、機能の入口・所有者・更新順・調整場所を追えるようにする。
行数削減やファイル数増加そのものは目標にしない。既存の描画、素材、配置、操作は維持する。

- Scene／生成／描画準備／描画／Collider／Editorの分担を現在のコードで再点検する。
- 役割が混在している箇所だけを段階的に整理し、各変更を検証してから次へ進む。
- 利用ガイドの説明と実コードを一致させ、初見の人が読む入口を示す。
- Debug x64と普段のDevelopment x64をビルドし、影響する操作を実画面で確認する。
- 全体の見直しが終わるまで、部分的な成功をエンジン全体の完了としない。

## 今回確認した範囲

| 対象 | 現在の証拠・判断 | 状態 |
| --- | --- | --- |
| Title | Sceneにモデル・Spriteの生成、所有一覧、初期配置の保護境界が混在していた。TitleObjectManagerへ移管 | 分離済み。下記の操作を実画面確認 |
| 生成共通API | Object3dFactoryが読込・初期化を担当。Object3dCollectionは通常3Dの寿命管理だけを担当 | Titleでも既存Factoryを再利用 |
| 描画準備／描画 | Object3dRenderContextとSceneRenderPipelineが既存。Titleの呼出順を維持 | Title以外の詳細点検は未完 |
| Debug | DebugSceneContent、DebugSceneRenderer、Editor部品が既存 | 生成・所有とContext境界の詳細点検は未完 |
| Stage1 | 本編。固定鏡のLevelデータ変換と携帯鏡・鏡床の生成をStageMirrorFactoryへ移管。一般配置物は既存Collectionで生成 | 鏡の生成責任を整理。Scene全体の点検は継続 |
| Player／Collider／Level／Audio | ArchitectureGuideに窓口の説明あり | 実装と利用例の照合は未完 |
| README／ArchitectureGuide | READMEの旧Scene名と通常Skinning経路の説明を修正し、初見向け入口を追加。Title所有者と調整場所を更新 | その他の説明は引き続き実装と照合する |

## Title分離で変えないもの

- 初期のplane.obj、uvChecker.png、SkyBox、座標、Light値。
- モデルの追加位置、Animation分類とLoop再生、画像の180px制限と4列配置。
- 通常モデル → Animation → SkyBox → Sprite → PostEffect／ImGuiの描画順。
- 初期配置を残す一括削除と、Titleを抜ける際のGPU完了待ち。
- Object3d、Shader、TextureManagerなど共有描画基盤の動作。

## 検証記録（2026-09-28）

- Debug x64：ビルド成功。
- Development x64：最初は検証用ゲームの実行ファイルロックでLNK1104。終了後の再ビルド成功。Visual StudioのF5でもビルド後に起動できた。
- .vcxprojのClCompile／ClInclude：ワイルドカードなし。
- TitleObjectManager.cpp／.h：.vcxprojと.filtersへ各1件の明示登録を確認。
- 分離前Development通常起動：Titleの初期画像、背景、平面を確認。比較画像は`resources/Captures/Screenshots/ribonin_challenge_20260928_020707_862.png`。
- 分離後Development F5起動：Title初期画面を確認。比較画像は`resources/Captures/Screenshots/ribonin_challenge_20260928_021142_929.png`。ウィンドウサイズが異なるため画素一致の比較ではない。
- Title Edit View：AnimatedCube_BaseColor.pngを追加。Inspectorで位置180,160、サイズ180x180、Anchor 0.5,0.5と実描画を確認。
- 同じTitleへmultiMesh.objとwalk.gltfを追加し、通常モデル・Animation・Spriteの同時描画を確認。walkの再生時刻と姿勢の変化も確認。画像は`resources/Captures/Screenshots/ribonin_challenge_20260928_021338_127.png`。
- Restart Current SceneでTitleを再起動し、追加数0と初期背景・画像への復帰を確認。
- Scene一覧でSTAGE1へ切り替え、開始演出中と通常Cameraへの復帰後の白い携帯Mirrorを確認。JSONやモデルの保存・変更はしていない。
- 未確認：Clear Title Addedの実操作、Gamepad、追加画像の5枚目以降の折返し、欠損素材時の動作。
- Enterキーは自動操作で遷移を確認できなかった。Scene一覧からの切替成功を、Enter→Loading→Stage1の検証成功とは扱わない。入力・Scene遷移コードは今回変更していない。

この記録は作業状況であり、未確認の項目を成功の根拠として使わない。

## 固定鏡の生成・編集反映の分離（2026-09-28）

- 対象：`Stage1::CreateFixedMirrors`と`ApplyFixedMirrorEdits`を`game/mirror/StageMirrorFactory`へ移管。Sceneには再生成の判断・仮一覧からの採用・所有を残した。
- 既存クラスを使う判断：`Object3dFactory`は汎用モデル生成、`FixedMirror`は一枚の鏡の状態を担当するため、Levelデータの変換はゲーム側のFactoryへ置いた。
- 維持したもの：plane.objの既定値、幅・高さのScale×2、反射Textureの512指定、Y回転、BOXの半辺長変換、BOX指定なしの既存形状維持、呼出順。Shader・共有描画APIはこの段階では変更していない。
- Debug x64／Development x64：ビルド成功。最終Debugビルドも成功。出力にプロジェクト設定由来の警告・エラーなし。
- `.vcxproj`のClCompile／ClIncludeにワイルドカードなし。StageMirrorFactory.cpp／.hはプロジェクトと.filtersへそれぞれ1件ずつ登録。
- DevelopmentのF5起動でTitleの初期画像・背景・平面を確認。Scene一覧からStage1へ入り、開始演出中とEdit Viewで白い携帯鏡を確認した。
- 固定鏡は初期Playerから離れた座標にあるため、保存せず編集CameraをX=0、Z=-5へ移動して確認。元のStageMirror配置で鏡面と反射像を確認した。
- InspectorのStageMirrorのXを0.026から2へ一時変更し、`Edited in memory`表示、鏡面・反射像・Collider枠の移動を確認。保存画像：`resources/Captures/Screenshots/ribonin_challenge_20260928_104144_389.png`。これは編集経路の確認であり、配置変更による不具合修正ではない。
- Restart Current Sceneで一時編集を破棄し、初期化とJSON再読込を確認。その後ゲームを終了した。JSONは保存せず、検証前後のstage1.jsonのSHA256は`EC63BC9BC4E7D4626B88F198540EED531D38A4138DA552F823A413AEBF7457F1`で一致。
- Debug／Development本編Smoke：両方で終了コード0、SUCCESS。Collider、Enemy生成、鏡の所持・解除・遮蔽・Laser反射、Door、Camera、落下の必須判定は通過した。ただし両方の`hazardReflection=0`は成功条件に含まれていないため、この機能は確認済みとしない。
- 未確認：EnterによるTitle→Loading→Stage1（自動入力で遷移を確認できず）、固定鏡の枚数変更、読込失敗時の実画面、BOX指定なしの実編集、幅・高さ・回転の全組合せ。配置の平行移動だけで全ての反射精度が正しいとは判断しない。

## 続けて照合する箇所

- `InitializeStageGimmicks`とCSV記号からの生成：携帯鏡・鏡床は既存Factoryへ移管済み。一般配置物の設定とCSV記号からの生成を引き続き照合する。未実装のE0/G0/C0/L0を実装済みと説明しない。
- `EnemyManager.h/.cpp`：Object3dCommonをScene所有とするコメントは、GetInstanceのローカルstatic実体と照合して訂正済み。EnemyはManagerが所有し、共通3D設定は非所有参照。処理の変更はない。API全体の点検は引き続き行う。
- RenderContext／RenderPipeline：通常モデルとAnimationの混在時の状態設定、反射Cameraと通常Cameraの描画準備の順序を点検する。リファクタリングに不具合修正を混ぜない。
- Player／Collider／Level／Audio、DebugScene：公開APIの責任・寿命・失敗時の扱いとガイドを照合する。

## Release・携帯鏡・配置編集の修正（2026-09-28、検証待ち）

今回の依頼を優先し、機能変更・不具合修正として実施。上のリファクタリング時のビルド成功は、以下の変更後の成功を意味しません。

- LoadingScene：固定のWinApp初期サイズとSpriteの投影サイズが異なっていた。背景サイズを毎更新、DirectXCommonの実サイズに合わせた。通常Spriteは自動拡縮しない。
- 起動順はLoadingScene→TitleScene。Titleの描画はモデル→SkyBox→Spriteを一フレーム内で行う。Spriteの描画後にSkyBoxの読み込み完了を判定する仕組みではない。
- ImGuiManager::IsMouseOverGameView：USE_IMGUIなしでは常にfalseだったため、携帯鏡のMouse操作が無効だった。Releaseではクライアント領域内をtrueにする。Debug/DevelopmentのInspector上の入力除外は維持。
- CarryableMirror：縦持ち横ずれ-1.75→0、横持ち距離2.70→3.40。置いた直後に拾い直せるよう取得距離3→4。
- Object3d専用のCameraForGPUに裏面明度を追加。既定1で通常モデルを維持し、携帯鏡だけ0.60。共有Modelの材質を書き換えない。HLSLは元の法線と視線で裏面を判定する。plane.objの+Z法線とMirrorの反射面を照合済み。ただし画面確認は未実施。
- StageSceneRenderer：発射装置の壁越しシルエット呼出を削除。通常描画と壁の深度判定は残す。モデルを削除・移動して隠す修正ではない。
- TitleObjectManager：初期Spriteの位置0,0・サイズ512,512を明示。AddTextureの4引数版で位置・サイズを直接指定できる。Shelfの2引数版は同じ180,160へ仮配置し、長辺180以内で縦横比を保つ。追加順の4列自動配置は止めたため、複数画像はInspectorで移動する。
- Sprite Inspector：選択中Spriteの実際の色を読むよう修正。Copy Placement C++ボタンで位置・サイズ・Anchor・回転を直接数値のC++としてコピーする。保存機能ではない。
- StageEditor：Edit ViewのStage Player→Player Positionから、Player::SetPositionで本体とColliderを一緒に移動する。実行中の位置のみ。開始演出中はGame Viewに戻ると演出位置が優先される。開始地点の保存はPlayerStartのLevel編集で行う。
- 固定鏡の角度：Inspectorの値をStageLightPuzzleの基準角へ同期。JSONは基準角、充電による回転は実行時の加算に分離し、次フレームの上書きを防ぐ。
- プロジェクト照合で既存GameSceneRegistry.cpp/.hのfilters登録漏れを発見し、SceneManagementへ明示登録。
- Audioは直前の作業分（VoiceId検索の共通化とAPIコメント、独立Smokeテスト）が未検証のまま残る。今回の依頼に便乗した追加変更はしていない。

未確認：Debug/Development/Release x64の変更後ビルド、通常F5→Loading→Title→Stage1、最大化・通常サイズの白背景、Releaseの左右Mouse操作、鏡の縦横・表裏、拾い直し、壁の遮蔽、通常モデルとwalk.gltfの描画、各ImGuiの実操作。全ImGuiをチェック済みとは扱わない。

ビルド実行は承認チェックの利用制限で止まっている。迂回して実行せず、再開可能になってから上記を検証する。

## 検証再開・AudioとColliderの使い方の照合（2026-09-28）

上記の承認制限は解除予定時刻後の通常の承認再試行で解消した。前のターンはコード修正による進捗、今回は実際のビルド・API検証・画面確認による進捗であり、完了扱いにはしない。

- Release・携帯鏡・配置編集の変更後、ゲーム本体のDebug／Development／Release x64ビルドが成功。これだけでは鏡の操作やShaderの実表示が正しい証拠にはしない。
- DevelopmentをVisual StudioのF5から起動し、TitleのSprite・SkyBox・平面の表示を確認。Loadingの白画面を捉えたのはVisual Studio側のキャプチャなので、Releaseの赤い余白修正の検証には数えない。
- Title Edit ViewでSpriteのXを0→100へ直接入力し、Inspectorの100表示と画像の移動を確認。Copy Placement C++を押し、クリップボードがSetPosition({100.000000f,0.000000f})、Size 512x512、Anchor 0,0、Rotation 0のコードになったことを確認した。実行中の編集のみで素材や配置ファイルへ保存していない。
- EnterによるTitle→Stage1は今回も自動操作で確認できなかった。起動手順全体は未確認のまま。後の照合ではゲームプロセスも当時のゲームウィンドウも終了しており、古い画面座標を使った操作は停止した。
- AudioApiSmoke：Debug／Developmentとも27項目PASS、SUCCESS: failures=0、終了コード0。無効ID・停止済みID、音源読込の再利用、Pause／Resume、BGMフェード停止、SE登録を確認。聴感、故障時、再生中Mixer変更は対象外。
- テストのWarningLevel 4でAudio::LoadFileのReadSampleにC4245が出た。特殊ストリーム番号をDWORDへ明示変換して3か所で共有し、変数名streamIndexも訂正。音源や再生順は変えず、再ビルド・再テストではこの警告が消えた。
- この型指定修正後も、ゲーム本体をDevelopment→Release→Debug x64の順で再ビルドし、全て終了コード0。出力にプロジェクト設定由来の警告・エラーなし。最後のDebug x64ビルドまで完了。
- ArchitectureGuideにAudioの責任・所有・更新順・登録例・終了手順を追加。Colliderは所有だけで自動実行されないこと、Triggerフラグが生OBB一覧には引き継がれないことを明記。
- Mixerは値の保存先のみ。再生中Voiceへの常時反映は実装されていない。BgmPlayerの破棄だけではStopされず、SEのClearは登録名だけを消す。使い方と制限を明記し、未実装を動作保証に置き換えない。
- メイン／テストの両vcxprojでClCompile・ClIncludeのワイルドカード0、重複0、filtersとの不一致0。対象差分のgit diff --checkは成功。

当時の次の描画点検対象：SceneRenderPipeline::DrawObjectsはループ前に一度だけ通常Pipelineを設定し、Object3d::Drawの骨あり経路はSkinningへ切り替えていた。骨あり→通常を同じ一覧へ入れた場合の状態持越しは、直後の「モデル描画設定の持越し修正」で対応済み。現在はObject3d::Drawの両経路が描画直前に設定を選び、StageMapRuntime::Drawと反射描画もこの関数を通る。Titleの分離済み一覧だけを、混在順序の検証成功とは扱わない。

## モデル描画設定の持越し修正（2026-09-28）

- 原因：Object3d::Drawは骨あり経路でSkinning用Root Signature・Pipelineを選ぶ一方、通常モデルの経路では前の設定をそのまま使っていた。SceneRenderPipeline、StageMapRuntime、StageReflectionRendererの呼出経路を照合し、個々の通常モデルの直前に設定し直す保証がないことを確認した。過去の鏡消失すべての原因だったと断定するものではない。
- 修正：通常経路でもSetCommonDrawSettingを呼び、その後でObject専用の行列・Light・Textureを渡す。新しいクラスは作らず、もともと頂点形式を選ぶObject3d::Drawへ責任をそろえた。DrawMirror・DrawOccludedSilhouetteは独立した専用経路のまま。Shader、モデル、配置は今回変更していない。
- DebugSceneRendererにあった武器直前だけの設定し直しを外した。これにより既存Debug Sceneが「Animationモデル→通常の武器モデル」をObject3d::Drawだけで描く回帰確認経路になる。描画順は維持。
- Development、Release、最後にDebug x64をビルドし、すべて終了コード0。出力にプロジェクト設定由来の警告・エラーなし。メインvcxprojのワイルドカード0、重複0、filtersとの差0。
- Developmentの既存walk-scene自動テストは終了コード0。`ribonin_challenge_walk_scene_frame30_20260928_212104_166.bmp`と`ribonin_challenge_walk_scene_frame90_20260928_212105_198.bmp`を実際に開き、walkの人物・通常モデルの球体武器・背景地形が描かれていることを確認した。保存成功だけを表示確認にはしていない。2枚はAnimationの周期が近いため、これだけで歩行の全動作を保証しない。
- 画面操作用スキルで、Releaseの通常起動後のTitleと、検証用RIBONIN_CHALLENGE_START_SCENE=STAGE1で入った本編の携帯鏡表示を確認。設定は起動プロセスだけに指定し、JSON・素材は保存していない。検証用ゲームは終了済み。
- 修正後のStage1GameplaySmokeもRelease／Debugで終了コード0、SUCCESS。所持・解除、横持ち、傾き、Laser、Collider、Door、Camera、落下の必須判定を通過した。両構成のhazardReflection=0は合格条件に含まれていないため、危険Light反射全体を確認済みとはしない。直接Updateへ引数を渡す試験なので、実Mouse入力経路の証明にもならない。
- 未確認：TitleのEnter遷移、Releaseの白いLoadingの全フレーム、E取得とMouseでの縦横切替・表裏・回転。自動キー入力ではEnter・Eに画面変化が確認できなかったため、入力操作の検証成功には数えない。反射Cameraから通常Cameraへ戻る共有GPUデータの寿命は別途点検が必要。

## 携帯鏡・鏡床の生成責任を既存Factoryへ集約（2026-09-29）

- 調査：Stage1::InitializeStageGimmicksで、携帯鏡の生成と鏡床の反射Texture準備・水平化・Collider同期・両面反射設定を直接行っていた。固定鏡の生成窓口StageMirrorFactoryがすでにあるため、新しいクラスやファイルは作らず既存Factoryへ移管した。
- CreateCarryableMirror／CreateMirrorFloorは成功時にunique_ptr、失敗時にnullptrを返す。Stage1が所有し、Factoryは更新・描画やScene進行へ依存しない。携帯鏡失敗時の初期化中断と、鏡床失敗時の部品なし続行は従来通り。
- 維持：plane.obj、初期座標、幅・高さ、未所持状態、鏡床Yaw=0・Pitch=-1.57079633・反射Texture256、Collider同期→両面反射設定の順。JSON反映・Player生成・開始演出の順は変えていない。
- Development→Release→Debug x64のビルド成功。最後もDebug x64。メインvcxprojのワイルドカード0・重複0・filtersとの差0、git diff --check成功。新規ファイル登録は不要。
- Developmentの既存Capture Smokeは終了コード0、12枚保存。実画像`ribonin_challenge_20260929_022607_933.png`と`ribonin_challenge_20260929_022613_354.png`を開き、開始演出中のPlayerと通常Cameraでの携帯鏡表示を確認。画像保存数や非黒画素だけで可視性を判定していない。
- Release／DebugのStage1GameplaySmokeは終了コード0、必須判定SUCCESS。携帯鏡の取得・解除・横持ち・反射、鏡床の両面Laser反射、Collider、Door、Camera、落下を確認。hazardReflection=0は以前と同じで成功条件に含まれないため、危険Light反射全体は確認済みとしない。
- ガイドにFactory呼出例・所有者・配置の調整箇所を追記。Sceneの新規登録先も、現在のgame/GameSceneRegistration.cppへ訂正した。
- 未確認：この変更後のF5→Title→Enter→Stage1、実Mouse操作、鏡床の反射像の目視、欠損モデル時の実行。今回はScene指定で起動したため、通常起動経路の検証成功とは扱わない。前ターンの描画修正や既存の未保存変更も維持した。

## CSVの番号別対応表とStage1の配置生成（2026-09-29）

- 調査：MapChipFieldはP/E/B/G/C/Lを種類へ変換していたが、Stage1のswitchはP0/B0以外が空で、種類ごとの番号を処理へ結び付ける共通表はなかった。ステージ本編にCSVファイルは置かれていない。
- MapChipRegistryを追加し、種類とsubIdの組へ処理を登録する。Stage1::ApplyStageMapDataはP0の開始位置とB0のブロックだけを登録し、未登録番号・未知記号は生成しない。P1などをP0へ勝手に置き換えない。今後E0/G0/C0/L0を実装する時は、実体の所有者を用意した上で同じ場所に番号と処理を登録する。
- StageMapChipFactoryへB0の`block.obj`・配置名・1×1×1 BOX Collider値の変換を移した。MapChipFieldはCSV解析と座標、StageMapRuntimeは実モデル・Colliderの所有、Stage1は登録と更新順を担当する。
- MapChipField::LoadCsv(std::istream&)を追加し、ファイル版も同じ解析へ委譲。ゲームにCSVを作らず、独立LevelApiSmokeの文字列ストリームで空欄、P/E/B/G/C/Lの番号別実行、B1と未登録B2の差、B0の座標・Colliderを検証した。Debug／Developmentとも終了コード0、`SUCCESS: failures=0`。このテストは敵・ゴールを実際に生成するものではない。
- 本編のDevelopment→Release→Debug x64ビルド成功。DebugのStage1GameplaySmokeはCSVなしで終了コード0、鏡・Camera・Collider等の必須判定がSUCCESS。hazardReflection=0は既存通り成功条件外。
- DevelopmentのCapture SmokeはCSVなしのStage1で終了コード0、12枚保存。実画像`ribonin_challenge_20260929_025914_485.png`を開き、Player・携帯鏡・部屋の表示を確認。保存件数だけで表示成功と判断しない。
- `.vcxproj`と`.filters`へ新しいcpp/hをそれぞれ1件ずつ明示登録。メインと独立テストのワイルドカード0・重複0・filtersとの差0、git diff --check成功。
- 撮影画像はゲームが実行時に作るため、`.gitignore`でScreenshots内の画像を無視し、既存`.gitkeep`は共有する。画像を削除・移動したわけではない。
- 未確認：実CSVをStage1へ置いた場合の画面・Hot Reload、F5→Loading→Title→Enter→Stage1。ユーザーはStage1へCSVを置かない運用を希望しているため、今回は新規CSVを作っていない。全体の責任分担点検は継続する。

## Debugの画像追加・プレビュー失敗経路（2026-10-07）

- 原因：`DebugSceneContent::AddTexture()`と`DebugAssetPreview::EnterTexture()`は`Sprite::Initialize()`の後で画像読込結果を確認せず、欠損画像でもScene一覧やPreviewへ進めていた。`Sprite::Update()`は登録済み画像の寸法を取得するため、後のフレームで失敗し得る。
- 変更：二つの入口で`TextureManager::LoadTexture()`を先に確認し、失敗時はSprite生成・Camera復帰位置の保存・ECS登録・Inspector選択を行わない。成功時の配置・見た目・更新順は変えていない。仮Titleの画像・演出・配置も変更していない。
- 検証：Development x64のDebug UI Smokeで、存在しない画像を追加・プレビューする二経路が`false`を返し、Sprite件数・Preview・選択・Texture登録が変わらないことを確認。同じ実行で通常画像のPreview・追加も成功。終了コード0、今回作成された`gameplay_ui_smoke_20261007_100931_294.log`はSUCCESS。Developmentのwalk-scene Smokeの終了コード0と、今回のframe30/frame90画像上で通常モデル・Animationモデルを確認。最後のDebug x64ビルド成功、`.vcxproj`と`.filters`の各261件一致、ワイルドカード0、`git diff --check`成功。
- 未確認：通常F5からのLoading→仮Title→Stage1操作、DebugSceneの初期配置素材やLoadingSceneの画像が欠けた場合。これらは別の初期化失敗経路なので、今回の追加・プレビュー検査で確認済みとはしない。

## Stage1のCollider収集と利用順（2026-10-07）

- 調査：`Player`は球Colliderを所有し、移動・重力の後に`ResolveSolidObbs()`で押し戻し、確定位置をモデルへ渡す。`FixedMirror`・`CarryableMirror`・`StageMapRuntime::RuntimeObject`はOBB Colliderを所有し、`StageCollisionWorld`はその形状を用途別に収集する。床だけは汎用`Object3d`と、`StageCollisionWorld`が持つローカルBOX設定から毎回OBBを作る。
- 更新順：`Stage1::Update()`でGame View中なら`StageMapRuntime::UpdatePaths()`が移動とCollider同期を行い、`UpdateStagePlayer()`内の`StageCollisionWorld::Rebuild()`で最新形状を収集し、その後`Player::Update()`へ渡す。Cameraは全壁一覧ではなく`GetCameraBoundaryObbs()`の外壁タグだけを使い、Laserは`GetLightBlockingObbs()`を使う。ガイドとヘッダの誤ったCamera用説明を訂正した。判定式・配置・処理順は変えていない。
- 検証：Development x64ビルド成功。今回実行したStage1GameplaySmokeは終了コード0、`stage1_gameplay_smoke.log`に`SUCCESS`、`sphereColliderApi=1`、`droppedMirrorSolid=1`、`cameraWall=1`、`grounded=1`、`fell=1`を確認した。ただし`hazardReflection=0`は成功条件ではなく、危険Lightの実反射を証明しない。
- 未確認：通常F5からの手入力経路、床Colliderを独立したゲーム物体として所有させる設計への変更、Mirrorを置いた同じフレーム内でのPlayer再判定。これらを今回のコメント訂正で解決したとは扱わない。

## Stage1のColliderフラグをOBB収集時に保持（2026-10-07）

- 原因：`StageCollisionWorld::Rebuild()`はMirrorと通常配置物のColliderから生のOBBを取得した後、`IsEnabled()`と`IsTrigger()`を確認していなかった。現在のStage1配置にはフラグを切り替える操作がないため、既存画面で誤判定を再現した事実ではない。将来無効・Triggerにした際に壁として残る設計上の問題を確認した。
- 変更：固定鏡・置いた携帯鏡・JSON/CSV配置物を用途別一覧へ入れる前に、有効かつ非Triggerかを確認する。床・Doorの扱い、登録順、位置・見た目・操作は変更しない。新しいクラスやファイルは追加していない。
- 検証：既存のStage1GameplaySmokeへ、実素材を使わない通常配置物を一件作り、有効時にはPlayer・Camera外壁・Light遮蔽の各一覧に入り、無効時とTrigger時には入らない検査を追加した。Development x64ビルドと今回実行したSmokeは終了コード0、`stageMapColliderFlags=1`、その他の必須判定もSUCCESS。Stage1 Capture Smokeは12枚保存・12枚可視で終了コード0。開始時と通常Cameraの画像を実際に開き、Player・部屋・Lightの表示を確認した。
- 未確認：通常F5からの手入力経路、実ゲーム内でColliderフラグを切り替える操作（現在なし）、危険Lightの実反射。`hazardReflection=0`は成功条件に含めない。

## Development x64の通常F5起動の再確認（2026-10-07）

- Visual Studioで`Development`・`x64`を表示上で確認してF5起動した。起動直後の白いLoading画面と、続く仮Titleの「リボニンの挑戦 3D」「ENTER」SpriteおよびStage背景を実画面で確認した。Scene欄は`TITLE`、Runtime Monitorは`Stage1`であり、現在のTitleが`Stage1(true)`で動く構成と一致する。
- Game Viewへ入力する直前に画面操作ツールが別の入力を検知し、「Exit Game?」確認画面を観測したため、以後のUI入力を停止した。後でゲームプロセスの終了を確認したが、この操作の終了コードは取得していない。
- この回ではEnter後の`BeginStageStart()`やStage1通常Cameraへの遷移を確認できていない。白画面・仮Titleの実表示だけを検証済みとし、F5→Loading→Title→Stage1全経路の成功とは扱わない。コード・素材・配置は変更していない。

## Audioの読込失敗を成功登録しない確認（2026-10-07）

- 原因：`Audio::LoadFile()`はMedia Foundationの形式設定とサンプルを連続バッファへ変換・ロックする戻り値を確認せず、失敗時に未取得ポインタを使う可能性があった。デコード結果が空でも、共有音源へ登録して`true`を返し得た。現行音源`Alarm01.wav`で障害が発生した事実ではなく、失敗経路の堅牢性の問題。
- 変更：形式設定、サンプル変換、バッファLock/Unlockを確認し、空データは登録しない。有効なPCMデータの再生順、音量、BGM/SEの責任分担は変えない。読込成功時の音源データはコピーせず共有一覧へ移す。
- 検証：独立`AudioApiSmoke`のDebug／Development x64ビルド・実行がともに終了コード0、32項目PASS、`SUCCESS: failures=0`。音声でない実行ファイルを二度とも拒否し、その後の既存音源読込・BGM/SE操作・再初期化も通過。ゲーム本体Development x64ビルド成功。
- 未確認：Media Foundation内部の各失敗戻り値を強制する試験、空の有効音声ファイル、実際の聴感・音量、F5通常起動でのBGM開始タイミング、再生中Mixer変更の即時反映（現仕様では行わない）。

## 現在のTitleと起動順Smokeの検証範囲（2026-10-07）

- 調査：`GameSceneRegistration`は`TITLE`を`Stage1(true)`、`STAGE1`を`Stage1()`として登録する。通常Enterは`Stage1::Update()`内で`BeginStageStart()`を呼び、Scene切替を行わない。既存`StartupRouteSmoke`はテスト側から`ChangeSceneWithLoading("STAGE1")`を予約するため、Enterとは異なる経路である。
- 検証：Development x64で`RIBONIN_CHALLENGE_START_SCENE`を指定せず`RIBONIN_CHALLENGE_STARTUP_ROUTE_SMOKE=1`を実行。今回の`startup_route_smoke.log`は`SUCCESS`、終了コード0、LoadingのDrawが各7回、TITLEが8回、STAGE1が7回。これはSceneManagerの切替・描画順だけを証明する。
- 変更：Smokeのヘッダ・関数コメントとテストREADMEへ、通常Enterを通らない点を明記。起動・Scene・入力の実装は変更していない。
- 未確認：F5からGame Viewへ入力したEnterで同じStage1が本編演出へ進むこと。テストの成功を通常操作成功の代用にしない。

## 起動テストの終了結果とStage1初期化失敗（2026-10-07）

- 原因：`WinApp::ProcessMessage()`は`WM_QUIT`の終了コードを捨て、`WinMain`は`Framework::Run()`の後で常に0を返していた。さらに`StartupRouteSmoke`は`WM_QUIT`を使わず終了するため、失敗してもOSには成功と見えていた。`Stage1::Initialize()`は必須部品の生成失敗で戻るが、`SceneManager`は同じフレームにそのStageの`Update()`を呼ぶ。
- 変更：`WinApp`が終了コードを保持し、`Framework::Run()`が解放前に取得して`WinMain`へ返す。起動経路Smokeは自身の成否を`Game`へ返す。`Stage1`は必須モデル・マップ・Playerの準備完了まで`Update()`と`Draw()`を実行せず、失敗箇所をログへ記録して終了コード1を要求する。正常時のScene切替・更新・描画順、仮Titleの画像・演出・操作は変えていない。
- 検証：Development x64の起動経路Smokeは成功時0、テスト条件違反時1を返した。Release x64で非対応の撮影Smokeを要求すると、既知の失敗ログと`WM_QUIT`由来の終了コード1が一致した。Stage1変更後のDevelopment x64起動経路Smokeは終了コード0、Loading・TITLE・STAGE1のDrawを確認。一時フォルダーへ必要なシェーダーと画像だけ複製し、必須モデル欠落とマップ欠落の二条件を実行したところ、それぞれ原因ログと終了コード1を確認し、クラッシュダンプは作られなかった。一時フォルダーはパス確認後に削除した。
- 未確認：PlayerまたはCamera生成だけが失敗する条件、通常F5でのEnter後の実画面・入力。自動SmokeのScene切替をEnter操作の検証とは扱わない。

## 床・DoorのCollider所有者の確認（2026-10-08）

- 現状：床とDoorの見た目は`Stage1::InitializeStageGimmicks()`で生成し、`Object3dCollection`が寿命を持つ。床のローカルBOX設定とワールドOBBは`StageCollisionWorld`、Doorの開閉状態とワールドOBBは`StageLightPuzzle`が持つ。鏡・Player・通常配置物は各ゲーム物体がColliderを所有するため、床とDoorは同じ形ではない。
- 呼び出し順：`Stage1::ApplyFloorData()`が床のTransformとBOX設定を別々の所有者へ渡す。`StageCollisionWorld::Rebuild()`が床モデルのTransformから床OBBを作り、Puzzleが更新したDoor OBBも用途別の一覧へ加える。`Stage1::UpdateStagePlayer()`はその一覧をPlayerへ渡す。
- 判断：汎用描画部品`Object3d`へColliderを加えると、判定不要の全モデルへ責任が広がる。床だけを新しい所有クラスへ移すと、現在の`Object3dCollection`による生成・描画・解放との関係を組み替える必要がある。仮Titleの作業中にこの変更を進めず、見た目・操作・判定順を維持した。今回変更したのは所有関係を説明する文書だけ。
- 残る課題：床とDoorが個別にColliderを所有する形へ統一するなら、モデルの生成・更新・描画・解放の所有者と、Editorの床OBB参照まで含めて小さく移行し、Development x64の通常F5経路で実画面を確認する。現状を「Collider所有の統一が完了」とは扱わない。

## Stage1の判定一覧と鏡・Door更新のフレーム順（2026-10-08）

- 現行の`Stage1::Update()`はGame View中の`StageMapRuntime::UpdatePaths()`、`UpdateStagePlayer()`、`UpdateMirrorGameplay()`、`UpdateLightPuzzle()`の順。`UpdateStagePlayer()`内の`StageCollisionWorld::Rebuild()`は、その時点の鏡・Door形状からPlayer用一覧を作る。この後で携帯鏡の位置やDoor開閉形状が変わる場合、当該フレームのPlayer判定には更新前の形状が入り、次のフレームに追いつく。一方、`UpdateLightPuzzle()`は鏡操作後に`GetLaserReflectors()`を呼ぶため、Laser反射には更新後の携帯鏡を使う。Light遮蔽一覧はRebuild時点の床・Door・JSON配置物の形状を使う。
- これはコードから分かる実行順であり、操作中に見える不具合を再現したという意味ではない。鏡操作・Player移動・Laser計算の順序を変えると挙動が変わり得るため、この回は処理を入れ替えず、`StageCollisionWorld`と`Stage1`の誤解を招く所有コメントだけを直した。仮Titleには触れていない。
- 未確認：鏡を置く瞬間やDoorが動く瞬間の実画面で、一フレーム差が操作上の問題になるか。必要なら先に通常のDevelopment x64操作で再現条件を記録し、その症状に限って更新順を変更する。

## 固定鏡の編集失敗を部分反映しない（2026-10-08）

- 原因：`StageMirrorFactory::ApplyFixedMirrorEdits()`は一枚ずつ入力を確認してすぐ反映していた。後ろのデータや既存Mirrorが`nullptr`なら、前の鏡だけ変更したまま`false`を返す。現行`Stage1::ApplyStageMapData()`は呼出前に全件を確認しており、通常プレイでこの症状を再現した事実ではない。Factoryを単独で使った場合の失敗経路を整えた。
- 変更：枚数と全要素の存在を先に確認し、そろった時だけ従来と同じ順番で位置・Y回転・サイズ・Colliderを反映する。正常入力の見た目・反射Texture・描画順は変えない。`FixedMirror.h`のずれていたクラスコメントの字下げも既存書式へ合わせた。仮Titleは変更していない。
- 検証：別出力のDevelopment x64でビルド成功。`Stage1GameplaySmoke`へ二枚目だけ`nullptr`の入力を追加し、戻り値`false`と一枚目の中心位置維持を確認。今回作成されたログは`SUCCESS`・`mirrorEditAtomicity=1`、終了コード0。Stage1撮影Smokeも終了コード0・12/12枚可視で、今回の画像を実際に開き、Player・鏡板・部屋・Light線の表示を確認した。新しいクラッシュダンプはなかった。
- 未確認：通常のDevelopment x64 F5から仮TitleでEnterを押す経路、Editor上で不正な二枚目を与える操作（現行Stage1が事前に拒否する）。撮影Smokeは入力経路の代用にしない。

## Stage編集の再構築失敗時にデータと選択を戻す（2026-10-08）

- 調査：`StageMapRuntime::ApplyEdits()`は配置名で既存モデルとJSONを対応付ける。現在の`stage1.json`の配置名は重複せず、`StageLevelEditor::MakeUniqueName()`はEditor追加時に連番で重複を避ける。手編集された同名のJSONまで一意に扱う仕組みではないが、現在の配置に合わせて識別方法を大きく変更する根拠はない。
- 原因：`StageLevelEditor::RemoveSelectedObject()`と`ClearEditorAddedObjects()`は、実行中モデルへの再構築が失敗してもLevelDataからの削除と選択変更を残していた。後者はさらにJSON保存を呼び、保存が失敗しても成功表示で上書きした。`CommitAddedObject()`と`AddEventPair()`も生成失敗時に追加データは消すが、選択番号を戻していなかった。
- 変更：削除・一括削除は再構築失敗時にLevelDataと選択を復元し、保存へ進まない。二つの追加経路も失敗時に選択を元へ戻す。保存だけ失敗した場合は、実行中の変更を残して「メモリ上では削除、保存は失敗」と表示する。正常成功時の生成順・配置・モデル・Collider・保存タイミングは変えず、仮Titleも変更していない。
- 検証：通常のDevelopment実行ファイルを上書きせず、別出力のDevelopment x64をビルド。`Stage1GameplaySmoke`の模擬LevelDataで、削除・一括削除・Camera Area追加・Event一組追加の再構築失敗を起こし、元の二件と選択番号を保つこと、失敗時に保存しないことを確認。保存のみ失敗した場合の正しい状態表示も確認した。今回のログは`SUCCESS`・`stageEditorRollback=1`、終了コード0。新しいクラッシュダンプはない。
- 未確認：実際のEdit Viewで再構築失敗を起こす操作、手編集JSONの重複名、通常Development x64 F5から仮TitleでEnterを押す経路。模擬データの自動確認をEditor実画面の検証とは扱わない。

## Level JSONを完成した一時ファイルで置き換える（2026-10-08）

- 原因：`LevelLoader::Save()`は`.tmp`を書き終えた後、既存JSONへ`copy_file(...overwrite_existing)`していた。コピー途中で失敗すると元ファイルまで途中状態になり得るため、「一時ファイル経由なら元ファイルを守れる」というコメントの保証が不足していた。実際に`stage1.json`が壊れた事実ではない。
- 変更：`.tmp`への書込み後に明示的に`close()`の結果を確認する。成功時だけ、同じフォルダの完成済み`.tmp`を`MoveFileExW`の`MOVEFILE_REPLACE_EXISTING`で移す。コピーによる途中上書きをやめ、置換できなければ`.tmp`を消さず`false`を返す。`SaveStageMap()`も旧ファイルの無変更を断定せず、確認すべき二つのファイル名を表示する。JSONの内容・順番・Levelの配置やLight設定は変更しない。仮Titleのファイルには触れていない。
- 検証：独立`LevelApiSmoke`は専用の一時フォルダだけを使う。既存JSONへのEngine／Blender座標での保存と再読込、新規JSONへの保存、既存JSONを削除共有なしで開いたまま置換失敗させた時の元データ維持と`.tmp`残存を追加確認。Debug／Development x64とも終了コード0・`SUCCESS: failures=0`。`LevelLoader::Load()`は`.json`だけを開き、失敗後の`.tmp`を自動採用しない。`SaveStageMap()`は保存成功後にだけ`FileHotReload::Synchronize()`を呼ぶ。本番の`stage1.json`はテストで書き換えていない。
- 未確認：実際のEdit Viewからの保存、停電などによる永続化保証、特殊なファイル権限・ネットワーク共有上の置換。`MoveFileExW`の戻り値だけでこれらを保証したとは扱わない。

## Stage1のModel Shelfで保存失敗を成功表示しない（2026-10-08）

- 原因：`StageLevelEditor::DrawModelShelf()`と`HandleShelfDropOnEditView()`は`AddModel()`の後にJSON保存を呼ぶが、保存の戻り値を棚のメッセージへ反映していなかった。共通`SceneEditor`は追加結果だけを見て`Added`と表示する。一括削除も`ClearEditorAddedObjects()`が再構築や保存に失敗した後、共通UIの`Cleared`で棚のメッセージを上書きしていた。`SaveStageMap()`が表示するInspector側の失敗文とは食い違う。
- 変更：Stage1の棚だけで追加・一括削除の最終結果を受け取り、共通UIが更新した後に正しい文章へ置き換える。モデル追加が成功して保存に失敗した時は「メモリには追加、保存は失敗」、一括削除が再構築に失敗した時は「元のStageを維持」と区別する。保存機能のない呼出元も「メモリのみ」と表示する。共通UIと仮Title、正常時の配置・保存順・見た目は変更していない。
- 検証：Development x64を通常実行ファイルと別の出力先へビルドし、今回のStage1GameplaySmokeを終了コード0・`SUCCESS`・`stageEditorRollback=1`で確認。一括削除の失敗理由が戻り値とInspector状態表示で一致する模擬テストも通った。`.vcxproj`にワイルドカードIncludeなし、`git diff --check`成功。
- 未確認：Model Shelfを実際にクリック・Dropし、保存を故意に失敗させた時の棚の文字表示。通常Development x64 F5から仮Titleで手入力Enter→Stage1の経路。この自動SmokeをUI目視の代用にはしない。

## Inspector失敗経路とGPU初期化の未解決範囲（2026-10-08）

- Inspector調査：`ImGuiManager::LevelHotReloadWindow()`は編集中の`LevelData`へ直接書き、`StageEditor::DrawLevelControls()`がその後に`Stage1::ApplyStageMapData(false)`で実行中のStageへ反映する。反映に失敗した場合、Inspector側の値は元に戻らない。ただし現行の通常操作は必須のFloor/Mirrorを削除できず、`ApplyStageMapData(false)`の失敗条件（未読込データ、床の欠落、鏡の欠落・件数不一致・空ポインタ）を数値編集だけで起こす経路は確認できていない。失敗を再現した不具合と断定しない。
- GPU初期化調査：`Framework::Initialize()`は`ParticleManager::Initialize()`と`GPUParticle::Initialize()`を戻り値なしで呼ぶ。後者はSRVの7枠やGPU資源作成を`assert`で確認し、前者もShader・Pipeline・Buffer作成失敗を上位へ返さない。Developmentで資源生成が失敗した場合に安全終了を保証するには、各生成処理の戻り値、Managerの初期化結果、`Framework::Run()`のループ開始条件と解放順まで一続きで変更・検証する必要がある。入口に`if`だけ足して解決済みとは扱わない。
- `assert`の点検：現在の`project/game`と`project/engine`で確認した115件の`assert`は、少なくとも同一行に必須の`Initialize()`・`Load()`・`Create()`・`Map()`などを入れていない。`GPUParticle::Initialize()`の`CanAllocate(7)`は残枠を読む判定で、枠の取得自体は後続の`Allocate()`で行う。ただし`assert`だけに依存した失敗判定はDevelopmentでは消える。`ParticleManager::CreateParticleGroup()`などは同名の旧グループを外した後に新しいGPU領域を作るため、資源生成失敗を扱うなら置換の成否も同じ変更範囲で確認する必要がある。
- 判断：この回は仮Title作業中の通常F5経路とGPU故障条件を検証できず、Inspectorの全データを毎フレーム複製する変更も処理量と責任を増やすため行っていない。実際の画面や設定は変更していない。これらと前節のUI目視確認が残るため、100点完了とは判定しない。

## 基底クラスの状態変数を派生先から隠す（2026-10-08）

- 原因：`Framework`の`endRequest_`・`exitCode_`・`sceneFactory_`を`Game`が直接書き換え、`BaseScene`のCamera・描画設定ポインタを各Sceneがprotected経由で直接所有・変更していた。画像の例と同じく、継承先から基底側の状態を自由に変更できる設計だった。`Collider`のprotectedは派生Colliderの生成に必要なコンストラクタだけで、状態変数はprivateだった。
- 変更：`Framework::SetSceneFactory()`がFactory所有とSceneManagerへの参照登録を同じ順で行い、`RequestEnd()`が終了要求と結果を一緒に記録する。`Game`は基底の状態変数へ直接触れない。`BaseScene`はCameraの共通初期化だけを補助し、各SceneがCameraをprivateで所有し、必要なSceneだけが描画共通設定を非所有ポインタで保持する。Camera位置・Light・素材・Scene切替と更新順は変更しない。
- 検証：Debug x64と通常出力のDevelopment x64のソリューションビルド成功。Development起動順Smokeは今回の終了コード0、Loading二回・Title・Stage1のDraw回数が7・8・7・7で`SUCCESS`。不正な直接起動との併用では終了コード1と`FAILURE`になり、`RequestEnd()`の成否も区別した。Stage1GameplaySmokeは終了コード0・`SUCCESS`で、Collider・鏡・Cameraの既存判定が通過。Visual StudioのDevelopment F5で仮Title、Scene一覧からStage1・Debug・従来Titleの実画面を確認し、Stage1の球・部屋・危険Light、Debugの通常モデル・Animationモデル、従来Titleの初期Spriteと3D描画を確認した。`.vcxproj`のClCompile/ClIncludeワイルドカード0、`git diff --check`成功。
- 未確認：今回のF5で仮TitleのEnterを自動入力して本編へ進む経路、ゲームパッド、Camera・照明の全角度、モデル共有Textureの個体別上書き。Scene一覧での切替とSmokeをEnter操作の成功とは扱わない。

## 共有画像と粒子初期化の次の修正範囲（2026-10-08）

- 共有画像：`DebugAnimationPreview::Initialize()`の手持ち球は`GetModel()->SetTexture("Resources/debug/monsterBall.png")`を呼ぶ。`Object3dFactory::Create()`は`ModelManager`から名前で共有Modelを受け、`ModelManager`の一覧はFramework終了まで残る。したがってDebugを経由すると、手持ち球以外に後から作る`debug/sphere.obj`も同じ材質画像を使い得る。`sphere.obj`の元画像は`Model::Initialize()`が設定する`uvChecker.png`。一個体だけの画像指定なら既存の`Object3d::SetTextureOverride()`を使えるが、他の球の見た目が現在と変わるため、画像変更の範囲を確認してから別修正にする。モデル・Texture・配置はこの調査では変更していない。
- GPU初期化：`GPUParticle::Initialize()`は7枠の確認後に5つのCompute用Buffer、7つのSRV/UAV、Shader・Pipeline、描画用Bufferを順に作り、`InitializeParticles()`で初めてGPU命令を出す。現行`CreateParticleBufferResource()`・Pipeline生成はHRESULTを`assert`だけで判定し、`DirectXCommon::CreateBufferResource()`と`CompileShader()`も失敗後に空の資源を返し得る。`ParticleManager::Initialize()`にも同じ失敗経路があり、`Framework::Initialize()`は結果を受け取らない。単一箇所にnullptr判定を足すだけではDevelopmentでの安全な失敗終了にはならず、生成結果、部分生成の解放、Run開始判定を一続きで扱う必要がある。通常成功経路のビルドだけで検証済みとはしない。

## 粒子グループの置換と失敗時の所有（2026-10-09）

- 原因：`ParticleManager::CreateParticleGroup()`・`CreateRingParticleGroup()`・`CreateCylinderParticleGroup()`は同名の旧グループを先に削除し、その後で新しいGPU領域を作っていた。画像欠損は削除前に検出できたが、GPU領域やMapの失敗時は旧グループを失う。特にDevelopmentでは`DirectXCommon::CreateBufferResource()`の`assert`が無効になり、空Resourceの参照で終了し得た。
- 変更：共通の`InitializeInstancingResources()`がBuffer・Map・SRV番号の成否を返す。RingとCylinderの頂点Buffer・Mapも確認し、途中失敗時は新Group用SRVを返す。三種類とも新Groupが完成してから`RemoveExistingGroup()`を呼ぶ。正常時のTexture、形状、頂点順、描画設定、Sceneの更新順は変えていない。
- 検証：Development x64と最後のDebug x64のソリューションビルドはともに成功し、プロジェクト設定由来の警告・エラーは出なかった。`RIBONIN_CHALLENGE_TIME_PLAYBACK_SMOKE=1`をDebug Sceneで実行し、終了コード0・今回の`time_playback_smoke_20261009_031938_714.log`に`SUCCESS`と`particleGroupReplacement=completedThenSwapped`を確認。欠損画像と不正なCylinder寸法で旧グループを保持し、三種類の正常置換後に粒子数が0から再開するテストも通過。Visual StudioのDevelopment x64 F5で仮TitleからScene一覧でDebugへ切り替え、通常・Animationモデルと発光のある実画面を確認。白いLoadingと仮Titleも別のF5で確認した。`.vcxproj`と`.filters`のワイルドカードIncludeは0、`git diff --check`成功、新しいクラッシュダンプなし。
- 未確認：GPUメモリ不足とMap失敗の実機再現、個々の粒子形状の目視判別、仮TitleのEnterを通る本編開始。画面操作ツールの短いEnter入力では本編へ移らなかったため、これを成功とも不具合再現とも断定しない。`ParticleManager`・`GPUParticle`自体の初期化失敗をFrameworkまで返す処理も残る。100点完了とはしない。

## 円柱粒子の頂点数と未使用状態（2026-10-09）

- 原因：`ParticleManager::CreateCylinderParticleGroup()`は`divide * 6`を`uint32_t`で計算し、その結果をGPU領域とVertexBufferViewのサイズに使っていた。極端に大きい`divide`では乗算やViewの32-bitサイズがあふれ、確保した領域より多く頂点を書き得た。通常のDebug粒子は`divide=32`で、この症状が発生した事実ではない。`ParticleGroup::textureSrvIndex`は定義のみで読書きされていなかった。
- 変更：`VertexData`一頂点の大きさと一分割6頂点から、32-bit Viewに収まる上限を生成前に計算し、超える値は旧Groupを保持して`false`を返す。未使用の`textureSrvIndex`を削除する。通常の32分割、頂点順、材質、描画設定は変えていない。
- 検証：Development x64ビルドと今回の`time_playback_smoke_20261009_032757_172.log`は成功・終了コード0。`divide=UINT32_MAX`を拒否し、既存Cylinderの粒子一個を保持したことを`oversizedCylinder=rejected+originalPreserved`で確認。正常な三種のGroup置換テストも引き続き成功した。Development x64の通常F5で仮TitleからScene一覧のDebugへ切り替え、通常・Animationモデルと発光する粒子の実画面を確認した。
- 未確認：GPU資源不足・Map失敗の実機再現、仮TitleのEnterからStage1へ進む手入力経路。今回のScene一覧からの切替をEnter操作の確認とは扱わない。

## PostEffectの必須画像がない起動を安全に終える（2026-10-09）

- 原因：`PostEffect::Initialize()`は`noise0.png`または`noise1.png`を読めないとログ後に例外を投げるが、`Framework::Initialize()`は受け取っていなかった。一時環境で`noise0.png`を欠損させた修正前Developmentは原因ログを残しつつ終了コード`-1073740791`になった。白いLoading画像の欠損は既存処理で終了コード1・ダンプなしだった。
- 当時の変更：PostEffectを作るFrameworkが例外を受け取り、他の共通部品を従来の順番で準備した後、終了コード1を要求した。後のShader欠損試験でこの継続が危険と分かり、次節で失敗直後に後続初期化を止める形へ改めた。`Game::Initialize()`は終了要求があればSceneFactoryとLoadingを登録せず、`Framework::Run()`はUpdate/Drawを始めない。
- 検証：Development x64ビルド成功。一時環境でShaderと必要なノイズ画像だけを複製し、`noise0.png`欠損、`noise1.png`欠損、Loadingの`white.png`欠損を個別実行。三条件とも今回の終了コード1、対象画像名を含む原因ログあり、ダンプ0。試験フォルダーは確認後に削除し、本番素材は未変更。正常資源での起動順Smokeは終了コード0・`SUCCESS`、Loading二回・Title・Stage1のDraw回数7・8・7・7。Visual StudioのDevelopment x64 F5で白いLoading、仮Titleを確認し、Scene一覧からStage1のPlayer・部屋・Light、Debugの通常・Animationモデルと粒子を実画面で確認した。
- 当時の未確認：仮Titleで手入力Enterから本編に進む経路、PostEffect以外の共通GPU資源生成失敗、メモリ不足やShaderコンパイル失敗。次節でShader欠損は追加検証したが、Scene一覧切替とSmokeをEnter操作の検証とは扱わない。

## 必須Shader欠損時の初期化停止（2026-10-09）

- 原因：`DirectXCommon::CompileShader()`はファイル読込・DXC実行・Blob取得を`assert`でしか判定せず、Developmentでは失敗後に空の結果を参照し得た。初回の`Fullscreen.VS.hlsl`欠損試験ではPostEffectの例外をFrameworkが受けても次のParticleManager初期化へ進み、終了コード`-1073740791`になった。
- 変更：`CompileShader()`が読込・コンパイル状態・Blob取得の失敗をパス付きログと例外で伝える。従来Debugで停止していたShader警告・エラーもDevelopmentで失敗とする。`Framework::Initialize()`は共通部品を元の順に初期化し、最初の例外で後続を止める。`Finalize()`はAudio・ImGuiの初期化が終わった場合だけ各終了処理を呼ぶ。正常時のShader設定、Scene順、素材、カメラ、Light、操作は変更していない。
- 検証：Development x64ビルド成功。Shaderだけを欠いた一時環境で`Fullscreen.VS.hlsl`と`Particle.VS.hlsl`を個別に試し、双方とも終了コード1・対象パスを含むログ・ダンプ0を確認。修正後の`noise0.png`欠損も終了コード1・原因ログ・ダンプ0。通常資源の起動順Smokeは今回の終了コード0、Loading二回・Title・Stage1のDraw回数7・8・7・7で`SUCCESS`。Visual StudioのDevelopment x64 F5では仮Title、Scene一覧からStage1の部屋と危険Light、Debugの通常モデル・Animationモデルと発光粒子を実画面で確認。一時環境の三フォルダーは対象を確認して削除した。
- 未確認：仮Titleで手入力Enterから本編へ進む経路、GPUメモリ不足・Map失敗、Shaderの文法エラーや警告を実際に発生させる試験。短い画面操作Enterでは画面遷移を確認できなかったが、入力がゲームの一フレームで採取された証拠はないため、ゲーム側の不具合とは断定しない。`GPUParticle`・`ParticleManager`のGPU資源失敗判定にはまだ`assert`依存が残り、100点完了とはしない。

## ParticleManagerの部分初期化と終了処理（2026-10-09）

- 原因：`ParticleManager::Initialize()`のRoot Signature・Pipeline生成は`assert`だけで成否を見ており、標準板頂点とMaterialのBuffer・Mapは結果を確認していなかった。また`Finalize()`はGroupだけを消し、共通GPU資源と借りたManager参照を保持したままだった。通常の描画で失敗を再現した事実ではなく、失敗時と終了時の所有に関する問題である。
- 変更：既存の`ParticleManager`内でDevice・SRV管理、Root Signature、Pipeline、標準板頂点、Materialの各生成・Map結果を確認し、失敗時は理由をログへ残してFrameworkへ伝える。`Finalize()`はGroupのSRVを先に返し、共通GPU資源と借用ポインタを解放する。正常時の生成順、粒子の頂点・色・風・描画設定は変更していない。
- 検証：Development x64ビルド成功。一時環境で`Particle.VS.hlsl`だけを欠損させると終了コード1・対象Shader名を含むログ・ダンプ0で終了し、途中生成資源があっても終了処理を通った。通常資源の起動順Smokeは今回の終了コード0、Loading二回・Title・Stage1のDraw回数7・8・7・7で`SUCCESS`。Debug Sceneの時間再生Smokeは終了コード0、今回の`time_playback_smoke_20261009_035829_789.log`に`SUCCESS`と`particleGroupReplacement=completedThenSwapped`を確認。Development x64のF5で仮Title、Scene一覧からDebugの発光粒子・通常モデル・Animationモデル、Stage1の部屋と危険Lightを実画面で確認した。試験用フォルダーは対象を確認して削除した。
- 未確認：GPUメモリ不足、BufferのMap失敗、Root SignatureやPipelineのHRESULT失敗を実機で発生させた時の終了コードとGPU解放の計測。`DirectXCommon::CreateBufferResource()`のDebugでは`assert`が先に働くため、この経路全体を安全終了と判定しない。GPU足跡用`GPUParticle`の同種の初期化失敗判定、仮Titleから手入力Enterで本編へ進む経路も残る。100点完了とはしない。

## GPUParticleの部分初期化とSRV所有（2026-10-09）

- 原因：`GPUParticle::Initialize()`はSRVの空き、五つのDefault Heap Buffer、七つのSRV/UAV、Compute・描画Pipeline、Upload BufferとMapの失敗を`assert`だけ、または無確認で通していた。Developmentで初期化に失敗すると未完成資源の参照や無効DescriptorでGPU命令へ進み得る。通常表示でこの失敗を再現した事実ではない。
- 変更：既存の`GPUParticle`内でDevice・Command List・七枠の空き、各Buffer・Descriptor・Pipeline・Mapの結果を確認する。失敗時は資源名を含む例外をFrameworkへ渡し、SceneとGPU粒子初期化を始めない。`Finalize()`は途中取得した有効なSRV/UAVとBufferを返し、描画用ViewとEmitter状態も初期値に戻す。生成順、Shader、粒子形状、色、Light設定と描画方法は変えていない。
- 検証：Development x64ビルド成功。一時環境で最初の`InitializeParticle.CS.hlsl`と後段の`GPUParticle.PS.hlsl`を個別に欠かし、双方とも対象パスを含むログ・終了コード1・ダンプ0を確認。正常資源の起動順Smokeは終了コード0、Loading二回・Title・Stage1のDraw回数7・8・7・7で`SUCCESS`。時間再生Smokeも終了コード0、今回の`time_playback_smoke_20261009_114146_594.log`に`SUCCESS`。Development x64のVisual Studio F5で仮Title、Scene一覧からDebugのAnimationモデルと発光粒子、Stage1の部屋と危険Lightを実画面で確認した。欠損試験用の二フォルダーは対象を確認して削除した。
- 未確認：GPUメモリ不足、Descriptor枠不足、Map失敗、PipelineのHRESULT失敗を実機で強制した時の終了と解放。`DirectXCommon::CreateBufferResource()`のDebug `assert`は残る。Debug画面の発光のうちGPU足跡粒子だけを画像から識別したとは言えず、仮Titleからの手入力Enterも未確認。100点完了とはしない。

## PostEffectのカプセル化と部分初期化（2026-10-09）

- 原因：`PostEffect`の設定値とGPU資源は既にprivateで、Sceneは公開関数から操作している。一方、Root SignatureとGraphics Pipelineの失敗は`assert`だけ、三つの定数BufferとMapは無確認だった。失敗してもDevelopmentで未完成の資源を使い得る。また明示的な終了処理がなく、途中生成した資源と借用ポインタを保持していた。通常画面でこの失敗を再現した事実ではない。
- 変更：既存の`PostEffect`内で依存、Root Signature、各Pipeline、三つのBufferとMapの成否を確認する。失敗時はFrameworkへ伝え、後続初期化とScene開始を止める。`Finalize()`で自身のBufferとPipelineを解放し、借りたManager参照を外す。マスク画像の所有と解放は従来どおりTextureManagerに任せる。成功時の生成・描画順、各効果、素材は変更していない。
- 検証：Development x64ビルド成功。起動順Smokeは今回の終了コード0、Loading二回・Title・Stage1のDraw回数7・8・7・7で`SUCCESS`。末尾の`BoxFilter.PS.hlsl`だけを欠いた隔離環境では対象Shader名を含むログ、終了コード1、ダンプ0を確認し、検証用フォルダーを削除した。Visual StudioのDevelopment x64 F5でTitle、Scene一覧からStage1の部屋・光とDebugの通常・Animationモデルと粒子を実画面で確認し、Shift+F5で終了した。今回のF5後に新しいダンプなし。
- 未確認：GPUメモリ不足、BufferのMap失敗、Root SignatureとPipelineのHRESULT失敗を実機で強制した時の挙動。`DirectXCommon::CreateBufferResource()`のDebug `assert`は残る。短いEnter送信ではTitleからのScene切替を観測できず、通常手入力経路の成功・失敗を判定しない。100点完了とはしない。

## SpriteCommonの失敗をSceneへ返す（2026-10-09）

- 原因：2D表示で共用する`SpriteCommon::Initialize()`は、Root SignatureとPipelineの生成失敗を`assert`だけで判定し、戻り値で成否を返していなかった。DevelopmentではShaderまたはGPU資源が欠けても、Loadingなどが描画へ進み得た。共通の`DirectXCommon::CreateBufferResource()`には多数の利用先があり、一括で契約を変えることは今回の範囲から外した。
- 変更：既存の`SpriteCommon`内でDirectX依存、Root Signature、Shader、Pipelineの成否を確認し、失敗時は未完成資源を外して`false`を返す。Loading・TitleObjectManager・DebugScene・StageTitleOverlayの四つの初期化元が結果を受け取る。提出用Titleを持つStage1もOverlay失敗時には終了を要求し、未準備の共通描画設定でSpriteを使わない。正常時のPipeline設定、描画順、画像と配置は変えていない。
- 検証：Development x64ビルド成功。起動順Smokeは終了コード0、Loading二回・Title・Stage1のDraw回数7・8・7・7で`SUCCESS`。隔離環境で`Sprite.VS.hlsl`と`Sprite.PS.hlsl`を個別に欠かすと、双方とも対象Shader名を含むログ、終了コード1、ダンプ0。最初の試験環境は白背景も欠けていたため、その結果をShader判定に数えず、白背景を含めて作り直した。各試験用フォルダーは確認後に削除した。Development x64のVisual Studio F5でTitleの2D表示、Scene一覧からStage1の部屋・光、DebugのSprite・通常・Animationモデルと粒子を実画面で確認し、Shift+F5で終了した。Scene一覧切替をEnter経路の確認とは扱わない。
- 未確認：Root SignatureとPipelineのHRESULT失敗を実機で強制した場合、Sprite個体のBuffer/Map失敗、仮TitleのOverlayだけが失敗する条件でのStage1終了。最終変更後もDevelopment x64のF5で仮Titleを目視でき、短いEnter送信では本編移行を観測できなかったが、入力がフレーム内に保持された証拠がないため実キーEnter経路の成功・失敗は判定しない。100点完了とはしない。

## Sprite個体の初期化と画像切替の失敗を呼出元へ返す（2026-10-09）

- 原因：`Sprite`の頂点・Index・Material・Transformの四つのBuffer生成とMapに失敗判定がなく、`Initialize()`は完成前でも戻り値で失敗を伝えなかった。`SetTexture()`は`assert`と早期returnに依存し、呼出元には失敗が分からなかった。画像を取得できない場合も古いSpriteを変更せず残す契約が明示されていなかった。
- 変更：`Sprite`が自身のGPU資源をprivateのまま管理し、`Initialize()`で各生成・Map結果を確認する。失敗時は途中の資源を外して`false`を返す。Loading・TitleObjectManager・StageTitleOverlay・DebugScene・DebugAssetPreview・DebugSceneContentは成功したSpriteだけを登録する。`SetTexture()`は取得できない画像名なら`false`を返し、以前の画像とサイズを保つ。成功時の頂点・画像・設定順と表示は変更しない。
- 検証：Development x64のSolutionビルド成功。今回の起動順Smokeは終了コード0・`SUCCESS`・Loading/Title/Stage1のDraw回数7/8/7/7。Debug UI Smokeは終了コード0・`SUCCESS`で、欠損画像への`SetTexture()`拒否後も元のSprite画像とサイズを保持し、TextureManagerへ空登録を残さない条件を通過した。今回保存したGame Viewを開き、Sprite・通常モデル・Animationモデルの表示を目視した。検証で新規作成されたBMPとAVIだけを確認後に削除した。Visual StudioのDevelopment x64 F5では仮Titleを目視し、Scene一覧からStage1の部屋・Light、DebugのSprite・通常モデル・Animationモデルを確認してShift+F5で停止した。最後のDebug x64 Solutionビルドも成功し、プロジェクト設定由来の警告・エラーはなかった。`.vcxproj`のワイルドカードIncludeはなく、`git diff --check`も成功。
- 未確認：GPUメモリ不足とBufferのMap失敗を実機で強制した場合の終了・資源解放。`DirectXCommon::CreateBufferResource()`のDebug側`assert`は残り、その経路全体を安全終了とは言えない。F5の短いEnter送信では仮TitleからStage1への遷移を確認できず、入力がゲームのフレーム内で採取された証拠がないため成功・失敗を判定しない。100点完了とはしない。

## protected状態変数の再点検（2026-10-09）

- `project/engine`と`project/game`のヘッダーで`protected:`が残るのは`Framework`、`BaseScene`、`Collider`の三クラスだけ。前二者は派生クラスが使う関数のみ、`Collider`は派生Colliderを作るコンストラクタのみで、いずれもprotectedの状態変数はない。画像で指摘された「派生先が基底の変数を直接書く」問題は、現在の自作コードのこの範囲では残っていないため、追加のアクセス指定変更はしない。
- 次の共通GPU失敗処理は`DirectXCommon::CreateBufferResource()`のDebug `assert`を含む。呼出しがSprite以外のModel・Skybox・Lightなどにも広がるため、各所有者の失敗の返し方と解放順をそろえてから扱う。入口の`assert`だけを消して完了とはしない。

## SkyBox個体の未完成GPU資源をSceneへ渡さない（2026-10-09）

- 原因：`Object3dFactory::CreateSkyBox()`はDDSの読込を確認していたが、`SkyBox::Initialize()`が頂点・Index・行列・Materialの四つのBufferとMapを無確認で使い、失敗結果をFactoryへ返していなかった。元の`TitleScene`はFactoryを通さず直接作っていた。DDS自体が壊れていたという確認ではなく、GPU資源生成の失敗経路の問題である。
- 変更：`SkyBox::Initialize()`が依存と四つのBuffer・Mapを確認し、失敗時は自分の途中資源と借用ポインタを外して`false`を返す。Factoryは未完成のSkyBoxを返さず、TitleSceneもStage1・Debugと同じFactoryを使う。正常時のDDS、24頂点・36Index、背景の見た目、描画順は変更しない。
- 検証：Development x64 Solutionビルド成功。今回の起動順Smokeは終了コード0・`SUCCESS`・Draw回数7/8/7/7。Visual StudioのDevelopment x64 F5で仮Title、Scene一覧から元のTitleSceneの空背景、Stage1の部屋とLight、Debugの背景・Sprite・通常モデル・Animationモデルを実画面で確認し、Shift+F5で停止した。最後のDebug x64 Solutionビルドも成功し、プロジェクト設定由来の警告・エラーはなかった。今回のF5後に新しいクラッシュダンプはない。`.vcxproj`のワイルドカードIncludeなし、`git diff --check`成功。
- 未確認：GPUメモリ不足やMap失敗を実機で強制した場合の終了と解放、仮Titleで実キーEnter→Stage1の操作経路。`DirectXCommon::CreateBufferResource()`のDebug `assert`は残り、SkyBox以外の呼出元もあるため、共通処理全体の保証とはしない。100点完了とはしない。

## Object3d一体分のGPU描画データを完成後に登録する（2026-10-09）

- 原因：`Object3dGpuData::Initialize()`は行列・Camera位置・壁越し輪郭色のBuffer生成とMapの結果を見ずに書き込み、`Object3d::Initialize()`とFactoryへ失敗を伝えなかった。通常のGPU資源生成で失敗を再現したわけではなく、失敗時に未完成のObjectをSceneへ渡し得る契約の問題である。
- 変更：既存の`Object3dGpuData`内で三つのBufferとMapを個別に確認し、失敗時は途中資源とCPU側のMap先を外して`false`を返す。`Object3d::Initialize()`と`Object3dFactory::InitializeObject()`も結果を順に伝え、Player・鏡・通常モデル・Animationモデルの生成元は未完成のObjectを受け取らない。成功時のLight初期値、行列、描画順は変えない。
- 検証：Development x64 Solutionビルド成功。今回の起動順Smokeは終了コード0・`SUCCESS`・Draw回数7/8/7/7。Stage1 Gameplay Smokeは終了コード0・`SUCCESS`でPlayer・鏡・Laser関連の判定を通過。Debug UI Smokeも終了コード0・`SUCCESS`で、今回のBMPを開いてSprite・通常モデル・歩行Animationモデルを実際に確認した。試験が新しく作ったBMPとAVIの二件だけを確認後に削除した。Visual StudioのDevelopment x64 F5では仮Title、Scene一覧からStage1のPlayer・鏡・LightとDebugの通常・Animationモデルを実画面で確認してShift+F5で停止した。今回のF5後に新しいクラッシュダンプはない。
- 未確認：GPUメモリ不足・Map失敗を実機で強制した場合、壁越し輪郭専用描画（現行本編の呼出元なし）、仮Titleで実キーEnter→Stage1の経路。Scene一覧での切替をEnter入力の確認とみなさない。共通`DirectXCommon::CreateBufferResource()`のDebug `assert`も残り、100点完了とはしない。

## 継承・State・生成パターンを責任から点検する（2026-10-09）

- ポリモーフィズム：`Framework::Run()`は仮想`Initialize()`・`Update()`・`Draw()`を呼び、`Game`がゲーム固有の三つを上書きする。`BaseScene`の四つの純粋仮想関数を`LoadingScene`・`Stage1`・`TitleScene`・`DebugScene`が実装し、`SceneManager`が基底の`unique_ptr`から呼ぶ。`AbstractSceneFactory`も`SceneFactory`の生成関数を仮想呼出しする。三つの基底はいずれもvirtualデストラクタを持つ。`Collider`は共通状態を継承するが仮想の判定関数がなく、この評価のポリモーフィズム例には数えない。
- State Pattern：`SceneManager`が現在Sceneを所有して呼出しを委譲し、`LoadingScene::Update()`などが次Sceneを予約するので、Scene単位では成立している。`TitleScene`の準備・演出列挙型や`StageStart`の二段階演出は、現時点では固定順の状態機械でありState Patternとは呼ばない。仮Titleの変更中にStateクラスを増やして演出順を変えない。
- 他のパターン：`Framework::Run()`は共通ライフサイクルのTemplate Method、`AbstractSceneFactory::CreateScene()`を`SceneFactory`が実装する生成はFactory Method。`GameSceneRegistration`はゲーム側Scene生成関数を登録し、エンジン層をゲーム固有型から分離する。`Object3dFactory`はモデル一体の生成をまとめる単純Factoryで、GoFのFactory Methodと混同しない。Singletonだけを使っているわけではない。
- 変更：何も追加せず基底へ共通でない振る舞いを押し込まない。独自処理のない`Game::Finalize()`の上書きだけを削除し、`Framework::Run()`から従来と同じ`Framework::Finalize()`へ到達させる。Scene・モデル・入力・Camera・Light設定は変更しない。
- 検証：Development x64 Solutionビルド成功。削除後の起動順Smokeは終了コード0・`SUCCESS`・Draw回数7/8/7/7で、基底の終了処理まで戻った。Development x64 F5で仮TitleとScene一覧からStage1を目視し、Shift+F5で停止。実キーEnter経路は未確認であり、Scene一覧の切替で代用しない。

### Scene生成窓口の入力確認

- 原因：`SceneFactory::RegisterScene()`が空の`SceneCreator`を受け付けると、登録自体は成功するが、後の`CreateScene()`で呼び出した時に例外となる。現在登録されているSceneはいずれも有効で、これは失敗経路の予防である。
- 変更：Factory自身が空の名前・生成関数を登録前に拒否し、既存の有効な登録順とSceneの種類は変えない。Stateクラスや継承階層は増やさない。
- 検証：Development x64 Solutionビルド成功。通常起動順Smokeは今回の`startup_route_smoke.log`（2026-10-09 17:53更新）で終了コード0・`SUCCESS`、Loading二回・Title・Stage1のDraw回数7/8/7/7。Visual StudioのDevelopment x64 F5で仮Titleの二枚のSpriteと部屋・光を実画面で確認し、Scene一覧からStage1を選ぶとPlayer・部屋・鏡・光が表示された。Shift+F5で停止し、今回の新しいクラッシュダンプはない。最後のDebug x64 Solutionビルドも警告・エラーなしで成功し、`.vcxproj`のワイルドカード0、`.filters`との差0、`git diff --check`成功。空の生成関数を実際に登録する故障試験と、実キーEnterから本編へ入る操作は未確認である。Scene一覧での切替をEnter経路の証拠とはしない。

## ModelのGPU Bufferを完成後にManagerへ登録する（2026-10-09）

- 原因：`Model::Initialize()`は材質画像の欠損を`false`で返せるが、その前のIndex・頂点・材質Bufferの生成とMapを確認せず、失敗時もポインタを使い得た。GPU不足を実機で再現したのではなく、未完成Modelを使わせ得る失敗経路を確認した。
- 変更：既存の`Model`が三つのBufferとMapを個別に判定し、失敗なら自身の途中GPU資源とMap先を外して`false`を返す。画像読込失敗時も同じ片付けを行う。`ModelManager::LoadModel()`の既存の戻り値判定により、未完成Modelを登録しない。新しい管理クラスやScene処理は増やさず、正常時の頂点、材質初期値、画像、Camera、Light、描画順は変えない。
- 検証：Development x64 Solutionビルド成功。起動順Smokeは今回のログで終了コード0・`SUCCESS`・Draw回数7/8/7/7。Stage1 Gameplay Smokeは終了コード0・`SUCCESS`。Debug UI Smokeと`walk-scene`は終了コード0・`SUCCESS`で、今回のBMPを開き通常モデルと歩行Animationモデルをframe30/90で目視した。試験が今回生成したBMP三枚とAVI一件だけを確認後に削除した。Development x64 F5でも仮タイトル表示と背後のStage1の部屋・Lightを目視し、Shift+F5で停止した。
- 未確認：GPUメモリ不足・Map失敗を強制した実行、画面操作ツールからのEnterが本編開始に届く経路。共通`DirectXCommon::CreateBufferResource()`のDebug `assert`と、他の呼出元の失敗処理は残るため、GPU資源全体の安全性や100点完了とはしない。

## 共通Buffer失敗を骨ありモデルとTexture転送まで伝える（2026-10-09）

- 原因：`DirectXCommon::CreateBufferResource()`はDebugで`assert`し、Developmentでは空Resourceを返し得た。大半の所有者は空を判定していたが、`Model::CreateSkinCluster()`と`DirectXCommon::WriteToIntermediateResource()`は直後に参照した。Texture転送のCommand Allocator・Command List・Closeも`assert`だけで判定し、`TextureManager`へ失敗を返さなかった。
- 変更：Buffer生成は失敗をログに記録して空を返す。`Model::CreateSkinCluster()`は各Buffer・Map・出力頂点生成・Descriptorを確認し、途中失敗時は取得済みDescriptorとMapを片付けて`false`を返す。`Object3d::InitializeAnimation()`とFactoryは未完成の骨ありObjectを渡さない。Texture本体と中間Buffer・Map・Command Listの失敗は`TextureManager::CreateTextureData()`へ伝え、初回SRV番号を返し、再読込なら旧Texture/SRVを保持する。正常時の頂点、材質、Light、DDS、描画順は変えない。以前の節の「共通BufferのDebug assertが残る」は、この変更前の時点の記録である。
- 検証：Development x64 Solutionビルド成功。今回の起動順Smokeは終了コード0・`SUCCESS`・Draw回数7/8/7/7、Stage1 Gameplay Smokeは終了コード0・`SUCCESS`。`walk-scene-reload`は終了コード0・`RELOAD`・`SUCCESS`で、再読込前後のBMPを開き歩行Animationモデル・通常モデル・画像を目視した。今回生成したBMP二枚だけを確認後に削除した。Development x64 F5では仮タイトルと背後のStage1の部屋・Lightを実画面で確認し、Shift+F5で停止した。
- 未確認：GPU不足・Map失敗・Command List失敗を実機で強制した時の全所有者の終了と解放。`DirectXCommon`のRenderTexture・DepthStencilなど別の生成経路には`assert`が残る。短いEnter送信のF5操作で本編開始を確認できておらず、100点完了とはしない。

## フレーム時間更新の到達不能コードを除く（2026-10-09）

- 原因：`DirectXCommon::UpdateFixFPS()`は実経過時間の更新後に無条件で`return`しており、その後の旧1μs待機ループは一度も実行されない。旧ループだけが読む`kMinCheckTime_`も現在のFPS制御に無関係だった。
- 変更：現在実行されている「目標時刻まで待つ→`deltaTime_`を更新する」経路を残し、到達不能な旧処理と未使用メンバだけを削除した。`DirectXCommonRenderPass`からの呼出順、60FPS目標、経過時間の上限、Camera・Light・素材の設定は変更していない。
- 検証：Development x64 Solutionビルド成功。今回の起動順Smokeは終了コード0・`SUCCESS`・Draw回数7/8/7/7。Stage1 Gameplay SmokeとDebug Sceneの時間再生Smokeも終了コード0・`SUCCESS`。`walk-scene`は終了コード0・`SUCCESS`で、今回のframe30/90 BMPを開き通常モデル、歩行Animationモデル、Sprite、照明を目視した。今回生成したBMP二枚だけをパスと更新時刻を確認して削除した。最後のDebug x64 Solutionビルドは警告・エラーなしで成功。`.vcxproj`のワイルドカード0、`.filters`との差0、`git diff --check`成功。
- 未確認：手操作F5のEnter経路と長時間のフレーム時間の計測。RenderTexture・DepthStencilの生成失敗経路は別件として残る。

## 起動Sceneの生成失敗をGameへ返す（2026-10-09）

- 原因：`SceneManager::ChangeScene()`は未登録名やFactory未設定時に`false`を返すが、`Game::Initialize()`は戻り値を見ずにUpdate/Drawへ進めていた。初回Sceneがない場合、画面を描かない更新ループが続く。
- 変更：Scene生成と切替予約は従来どおりFactoryとSceneManagerが行い、起動成功・失敗の判断だけを`Game`が担当する。初回予約に失敗した時は`RequestEnd(1)`で後続のテスト設定とUpdate/Drawへ進まない。有効な開始名、生成順、Camera・Light・素材は変更しない。
- 検証：Development x64 Solutionビルド成功。未登録名`__INVALID_SCENE_SMOKE__`では今回のログに対象名を含む失敗が残り、終了コード1。通常の起動順Smokeでは終了コード0・`SUCCESS`・Draw回数7/8/7/7。Visual StudioのDevelopment x64 F5で仮Titleの画像・部屋・光を目視し、Scene一覧からStage1のPlayer・鏡・光を確認してShift+F5で停止した。今回の新しいクラッシュダンプはない。
- 未確認：画面操作ツールによる短いEnter送信ではTitle内の開始を確認できなかった。Scene一覧でのStage1切替は、Enter経路の確認とは区別する。

## Debugの共有球材質を変更せず保留する判断（2026-10-09）

- 原因の調査：`DebugAnimationPreview::Initialize()`は共有`Model::SetTexture()`で手持ち球へ画像を指定するため、同じ`debug/sphere.obj`にも画像が波及する。個体別指定への試行はDevelopment x64のビルド・再読込・Stage1の実行と撮影で成功したが、`resources/levels/scene.json`にも同じ球が配置されており、元のDebug画面のその球の画像が変わり得ることを後から確認した。
- 変更：見た目維持を優先し、この試行は採用せずコード・テストの説明を元へ戻した。共有状態の整理は、Debug配置球とScene切替後の見た目を合わせて決めるまで保留する。上記の試行時の成功ログを、最終状態の動作確認とは扱わない。
- 最終状態の検証：Development x64 Solutionビルド成功。差し戻したコードで`walk-scene-reload`を再実行し、今回のログは終了コード0・`RELOAD`・`SUCCESS`。frame90のBMPを開き通常モデル・歩行Animationモデル・手持ち球・照明を確認した。通常起動順Smokeも差し戻し後に終了コード0・`SUCCESS`・Draw回数7/8/7/7。Stage1 Gameplay Smokeも差し戻し後の今回のログで終了コード0・`SUCCESS`。最後のDebug x64 Solutionビルドも警告・エラーなしで成功。`.vcxproj`のワイルドカード0、`.filters`との差0、`git diff --check`成功。Stage1撮影Smokeは試行中に終了コード0・`SUCCESS`と画面を確認したが、差し戻し後に再実行していない。Visual Studio F5での手操作は今回行っていない。
- 未確認：DebugからStage1へ手操作で切り替えた同一プロセスの全オブジェクトの見た目、手操作F5のEnter経路。球材質の波及という設計課題は残る。

## Loading経由の無効な行き先を予約前に拒否する（2026-10-09）

- 原因：`SceneManager::ChangeSceneWithLoading()`は空名以外なら先に`LOADING`を予約し、次のScene名が未登録でも`true`を返した。白画面に移った後で`LoadingScene::Update()`の`ChangeScene()`だけが失敗し、白画面が続く。登録済みでも`LOADING`自身を行き先にするとLoadingを繰り返し得る。通常の呼出元は登録済みの`TITLE`・`STAGE1`なので、これを実際の通常操作の症状とは断定しない。
- 変更：Scene所有者の`SceneManager`がFactoryの登録一覧で行き先を確認し、未登録名・空名・`LOADING`自身ならLoadingの予約前に`false`を返す。登録済みでも後でFactoryの生成に失敗した時は、`LoadingScene::Update()`が白画面の無限再試行をやめ、ログを残して終了コード1を通知する。切替待機中の白画面と有効Sceneの生成・表示順は従来どおり。`StartupRouteSmoke`は無効な二種類が予約を作らないことを確認してから、有効なStage1へ進む。クラスやファイルは増やしていない。
- 検証：Development x64 Solutionビルド成功。追加後の起動順Smokeは終了コード0・`SUCCESS`、Loading二回・Title・Stage1のDraw回数7/8/7/7。今回作られたログに、未登録名と`LOADING`自身の拒否理由がそれぞれ残った。
- 未確認：登録済みSceneの生成関数が後で`nullptr`を返す故障は、実行で強制していない。Visual StudioのF5実画面確認はWindows画面操作の実行環境が起動せず、今回実施できなかった。Smokeは実キーEnterで本編へ進む経路ではない。
