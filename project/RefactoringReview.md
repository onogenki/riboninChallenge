# チーム向けエンジン整理の再点検

各節は調査した時点の記録です。後から原因が分かった項目は、上にある新しい節の結論を優先します。

## 以前の写真にある壁越しの黄色・橙色の点（2026-09-29）

- 写真ファイルの保存時刻は2026-09-28 14:31。写真の点は黄色と橙色で各一つ。写真より後のコミット`c777ba4`（同日19:15）で、`StageSceneRenderer::Draw()`から発射装置二つへの`DrawOccludedSilhouette()`呼び出しが削除されている。その直前の`dba4664`には、壁越しに黄色`{1.00, 0.88, 0.12}`と橙色`{1.00, 0.48, 0.08}`で描く二つの呼び出しがある。点の数・色・時系列が一致するため、以前の写真の直接原因はこの意図的な壁越し描画と判断できる。
- 現行コードでは`DrawOccludedSilhouette()`の呼び出し元はなく、発射装置は通常の深度判定で描く。Development実画面の背面・斜め背面では点が出ず、Release実画面の正面も表示は正常。Releaseの同一角度は未確認なので、現行Releaseの全視点で遮蔽が正しいとまでは断定しない。
- `CameraController::SetWallAvoidanceEnabled()`はCamera位置だけの設定で、壁越し描画を自動で有効にしないことをコメントで明記した。`Object3d::DrawOccludedSilhouette()`は他Sceneが意図して使う可能性のある公開APIなので、今回は削除しない。

## Stage1の角度入力とRelease画面の確認（2026-09-29）

- `Stage Player`の角度入力欄は、現在角度を表示していたため、Edit Viewで数値を確定すると見た目だけ旧値へ戻っていた。`CameraController`が所有する目標角度を読むようにし、入力した`-135`度が欄に残り、Game Viewの通常Cameraが斜め後方へ移動することをDevelopment実画面で確認した。値は実行中だけで、配置データは変更していない。
- DevelopmentのGame Viewで正面・左右・背面・斜め後方を確認し、壁越しの黄色い点は再現しなかった。Release版を`CG2_START_SCENE=STAGE1`で直接起動すると、正面視点のPlayer・携帯鏡・床・左右の壁は描画された。Releaseでは元画像と同じ背面角度へ自動操作できておらず、黄色い点の原因・解消は未確認。
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
- Release／最後のDebug x64ビルドと`CG2_STARTUP_ROUTE_SMOKE`は成功。ログでLoading→Title→Loading→Stage1の各Draw回数が7／8／7／7回と確認できた。`.vcxproj`のClCompile／ClIncludeにワイルドカードなし、`git diff --check`は空白エラーなし。これは実キー入力や見た目の保証ではない。
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
- 分離前Development通常起動：Titleの初期画像、背景、平面を確認。比較画像は`resources/Captures/Screenshots/CG2_20260928_020707_862.png`。
- 分離後Development F5起動：Title初期画面を確認。比較画像は`resources/Captures/Screenshots/CG2_20260928_021142_929.png`。ウィンドウサイズが異なるため画素一致の比較ではない。
- Title Edit View：AnimatedCube_BaseColor.pngを追加。Inspectorで位置180,160、サイズ180x180、Anchor 0.5,0.5と実描画を確認。
- 同じTitleへmultiMesh.objとwalk.gltfを追加し、通常モデル・Animation・Spriteの同時描画を確認。walkの再生時刻と姿勢の変化も確認。画像は`resources/Captures/Screenshots/CG2_20260928_021338_127.png`。
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
- InspectorのStageMirrorのXを0.026から2へ一時変更し、`Edited in memory`表示、鏡面・反射像・Collider枠の移動を確認。保存画像：`resources/Captures/Screenshots/CG2_20260928_104144_389.png`。これは編集経路の確認であり、配置変更による不具合修正ではない。
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
- EnterによるTitle→Stage1は今回も自動操作で確認できなかった。起動手順全体は未確認のまま。後の照合ではゲームプロセスもCG2ウィンドウも終了しており、古い画面座標を使った操作は停止した。
- AudioApiSmoke：Debug／Developmentとも27項目PASS、SUCCESS: failures=0、終了コード0。無効ID・停止済みID、音源読込の再利用、Pause／Resume、BGMフェード停止、SE登録を確認。聴感、故障時、再生中Mixer変更は対象外。
- テストのWarningLevel 4でAudio::LoadFileのReadSampleにC4245が出た。特殊ストリーム番号をDWORDへ明示変換して3か所で共有し、変数名streamIndexも訂正。音源や再生順は変えず、再ビルド・再テストではこの警告が消えた。
- この型指定修正後も、ゲーム本体をDevelopment→Release→Debug x64の順で再ビルドし、全て終了コード0。出力にプロジェクト設定由来の警告・エラーなし。最後のDebug x64ビルドまで完了。
- ArchitectureGuideにAudioの責任・所有・更新順・登録例・終了手順を追加。Colliderは所有だけで自動実行されないこと、Triggerフラグが生OBB一覧には引き継がれないことを明記。
- Mixerは値の保存先のみ。再生中Voiceへの常時反映は実装されていない。BgmPlayerの破棄だけではStopされず、SEのClearは登録名だけを消す。使い方と制限を明記し、未実装を動作保証に置き換えない。
- メイン／テストの両vcxprojでClCompile・ClIncludeのワイルドカード0、重複0、filtersとの不一致0。対象差分のgit diff --checkは成功。

次の描画点検対象：SceneRenderPipeline::DrawObjectsはループ前に一度だけ通常Pipelineを設定し、Object3d::Drawの骨あり経路はSkinningへ切り替える。骨あり→通常を同じ一覧へ入れた場合の状態持越しを調べる。StageMapRuntime::Drawと反射描画も同じ点検が必要。Titleの分離済み一覧で描けたことを、混在順序の検証成功としない。

## モデル描画設定の持越し修正（2026-09-28）

- 原因：Object3d::Drawは骨あり経路でSkinning用Root Signature・Pipelineを選ぶ一方、通常モデルの経路では前の設定をそのまま使っていた。SceneRenderPipeline、StageMapRuntime、StageReflectionRendererの呼出経路を照合し、個々の通常モデルの直前に設定し直す保証がないことを確認した。過去の鏡消失すべての原因だったと断定するものではない。
- 修正：通常経路でもSetCommonDrawSettingを呼び、その後でObject専用の行列・Light・Textureを渡す。新しいクラスは作らず、もともと頂点形式を選ぶObject3d::Drawへ責任をそろえた。DrawMirror・DrawOccludedSilhouetteは独立した専用経路のまま。Shader、モデル、配置は今回変更していない。
- DebugSceneRendererにあった武器直前だけの設定し直しを外した。これにより既存Debug Sceneが「Animationモデル→通常の武器モデル」をObject3d::Drawだけで描く回帰確認経路になる。描画順は維持。
- Development、Release、最後にDebug x64をビルドし、すべて終了コード0。出力にプロジェクト設定由来の警告・エラーなし。メインvcxprojのワイルドカード0、重複0、filtersとの差0。
- Developmentの既存walk-scene自動テストは終了コード0。`CG2_walk_scene_frame30_20260928_212104_166.bmp`と`CG2_walk_scene_frame90_20260928_212105_198.bmp`を実際に開き、walkの人物・通常モデルの球体武器・背景地形が描かれていることを確認した。保存成功だけを表示確認にはしていない。2枚はAnimationの周期が近いため、これだけで歩行の全動作を保証しない。
- 画面操作用スキルで、Releaseの通常起動後のTitleと、検証用CG2_START_SCENE=STAGE1で入った本編の携帯鏡表示を確認。設定は起動プロセスだけに指定し、JSON・素材は保存していない。検証用ゲームは終了済み。
- 修正後のStage1GameplaySmokeもRelease／Debugで終了コード0、SUCCESS。所持・解除、横持ち、傾き、Laser、Collider、Door、Camera、落下の必須判定を通過した。両構成のhazardReflection=0は合格条件に含まれていないため、危険Light反射全体を確認済みとはしない。直接Updateへ引数を渡す試験なので、実Mouse入力経路の証明にもならない。
- 未確認：TitleのEnter遷移、Releaseの白いLoadingの全フレーム、E取得とMouseでの縦横切替・表裏・回転。自動キー入力ではEnter・Eに画面変化が確認できなかったため、入力操作の検証成功には数えない。反射Cameraから通常Cameraへ戻る共有GPUデータの寿命は別途点検が必要。

## 携帯鏡・鏡床の生成責任を既存Factoryへ集約（2026-09-29）

- 調査：Stage1::InitializeStageGimmicksで、携帯鏡の生成と鏡床の反射Texture準備・水平化・Collider同期・両面反射設定を直接行っていた。固定鏡の生成窓口StageMirrorFactoryがすでにあるため、新しいクラスやファイルは作らず既存Factoryへ移管した。
- CreateCarryableMirror／CreateMirrorFloorは成功時にunique_ptr、失敗時にnullptrを返す。Stage1が所有し、Factoryは更新・描画やScene進行へ依存しない。携帯鏡失敗時の初期化中断と、鏡床失敗時の部品なし続行は従来通り。
- 維持：plane.obj、初期座標、幅・高さ、未所持状態、鏡床Yaw=0・Pitch=-1.57079633・反射Texture256、Collider同期→両面反射設定の順。JSON反映・Player生成・開始演出の順は変えていない。
- Development→Release→Debug x64のビルド成功。最後もDebug x64。メインvcxprojのワイルドカード0・重複0・filtersとの差0、git diff --check成功。新規ファイル登録は不要。
- Developmentの既存Capture Smokeは終了コード0、12枚保存。実画像`CG2_20260929_022607_933.png`と`CG2_20260929_022613_354.png`を開き、開始演出中のPlayerと通常Cameraでの携帯鏡表示を確認。画像保存数や非黒画素だけで可視性を判定していない。
- Release／DebugのStage1GameplaySmokeは終了コード0、必須判定SUCCESS。携帯鏡の取得・解除・横持ち・反射、鏡床の両面Laser反射、Collider、Door、Camera、落下を確認。hazardReflection=0は以前と同じで成功条件に含まれないため、危険Light反射全体は確認済みとしない。
- ガイドにFactory呼出例・所有者・配置の調整箇所を追記。Sceneの新規登録先も、現在のgame/GameSceneRegistration.cppへ訂正した。
- 未確認：この変更後のF5→Title→Enter→Stage1、実Mouse操作、鏡床の反射像の目視、欠損モデル時の実行。今回はScene指定で起動したため、通常起動経路の検証成功とは扱わない。前ターンの描画修正や既存の未保存変更も維持した。

## CSVの番号別対応表とStage1の配置生成（2026-09-29）

- 調査：MapChipFieldはP/E/B/G/C/Lを種類へ変換していたが、Stage1のswitchはP0/B0以外が空で、種類ごとの番号を処理へ結び付ける共通表はなかった。ステージ本編にCSVファイルは置かれていない。
- MapChipRegistryを追加し、種類とsubIdの組へ処理を登録する。Stage1::ApplyStageMapDataはP0の開始位置とB0のブロックだけを登録し、未登録番号・未知記号は生成しない。P1などをP0へ勝手に置き換えない。今後E0/G0/C0/L0を実装する時は、実体の所有者を用意した上で同じ場所に番号と処理を登録する。
- StageMapChipFactoryへB0の`block.obj`・配置名・1×1×1 BOX Collider値の変換を移した。MapChipFieldはCSV解析と座標、StageMapRuntimeは実モデル・Colliderの所有、Stage1は登録と更新順を担当する。
- MapChipField::LoadCsv(std::istream&)を追加し、ファイル版も同じ解析へ委譲。ゲームにCSVを作らず、独立LevelApiSmokeの文字列ストリームで空欄、P/E/B/G/C/Lの番号別実行、B1と未登録B2の差、B0の座標・Colliderを検証した。Debug／Developmentとも終了コード0、`SUCCESS: failures=0`。このテストは敵・ゴールを実際に生成するものではない。
- 本編のDevelopment→Release→Debug x64ビルド成功。DebugのStage1GameplaySmokeはCSVなしで終了コード0、鏡・Camera・Collider等の必須判定がSUCCESS。hazardReflection=0は既存通り成功条件外。
- DevelopmentのCapture SmokeはCSVなしのStage1で終了コード0、12枚保存。実画像`CG2_20260929_025914_485.png`を開き、Player・携帯鏡・部屋の表示を確認。保存件数だけで表示成功と判断しない。
- `.vcxproj`と`.filters`へ新しいcpp/hをそれぞれ1件ずつ明示登録。メインと独立テストのワイルドカード0・重複0・filtersとの差0、git diff --check成功。
- 撮影画像はゲームが実行時に作るため、`.gitignore`でScreenshots内の画像を無視し、既存`.gitkeep`は共有する。画像を削除・移動したわけではない。
- 未確認：実CSVをStage1へ置いた場合の画面・Hot Reload、F5→Loading→Title→Enter→Stage1。ユーザーはStage1へCSVを置かない運用を希望しているため、今回は新規CSVを作っていない。全体の責任分担点検は継続する。
