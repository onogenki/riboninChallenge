# 旧52点レビューと現行コードの照合

2026-10-29時点の作業ツリーを、2026-10-19の外部AIレビュー28項目と照合した記録です。これは再採点ではありません。古い52点を現在の点数として扱いません。`済`は元レビューが挙げた具体例を解消または現在の用途で確認した意味で、全ソースを完全に検査した意味ではありません。

| 番号 | 評価項目 | 現行コードで確認したこと | 判定 |
| --- | --- | --- | --- |
| 1 | フォルダ構成 | DebugのPreview・Smoke、StageのRenderなどを役割別に配置済み。残るフォルダの件数だけを理由に分割しない。 | 一部 |
| 2 | VSフィルター | 主プロジェクトのcpp/h 284件がfiltersと一致。物理パスの親とフィルターも一致し、ワイルドカードIncludeは0件。 | 済 |
| 3 | Platform | SolutionはDebug・Development・Releaseのx64のみ。 | 済 |
| 4 | 外部ライブラリ | Assimp・DirectXTex・ImGui・JSONは`project/externals`へ分離。 | 済 |
| 5 | ファイル・フォルダ命名 | Solution・exeは`riboninChallenge`へ統一。`engine/ImGui`・`resources/backGround`など大文字小文字の規則はまだ混在。資源パスの改名は読込箇所も含めて別途検証が必要。 | 一部 |
| 6 | クラスコメント | 主要クラスとVector2/3/4・Matrix4x4に役割説明あり。全コメントの正確さは未監査。 | 一部 |
| 7 | 関数コメント | Model・Object3d・撮影APIとStageCameraEventsの主要公開APIで、入力・戻り値・所有関係や再構築条件を説明。全公開関数は未監査。 | 一部 |
| 8 | 処理コメント | Inverse、描画準備、Scene切替などの意図を追記済み。低レイヤー全域の説明は未監査。 | 一部 |
| 9 | 不要なコメントアウト | 旧レビューは元から100点。現行自作コードでも無効化した大きな旧処理は確認していない。 | 維持 |
| 10 | スペリング | `Charenge`・`Adapater`・`paletterSrv`・`RasiterZerState`・`enviromentColor`は現行の自作cpp/h/HLSLと主Solution/Projectにない。 | 済 |
| 11 | メンバー命名 | Sprite・DirectXCommon・LoadingSceneなどのprivateメンバーは末尾`_`へ整理。1段のタブ／スペースで宣言した候補を検索したところ、残る候補はVector・Transform・ECS等の公開データ構造や代入演算子だった。入れ子の構造体や異なる書式を含む全クラスは未監査。 | 一部 |
| 12 | 定数化 | 元レビューのParticle・PostEffectなどの具体的な値、StageCameraEventsの操作値、危険Lightの周期・停止・移動・予告時刻を名前付き定数へ整理。危険Lightの時間境界はStage1GameplaySmokeで確認。物理単位や用途が異なる数値を機械的に定数化しない。 | 一部 |
| 13 | グローバル変数 | Loggerの共有PathとManagerのSingletonは残る。名前空間化では共有状態は消えず、依存注入への全面変更は所有・解放順の再設計が必要。 | 方針維持 |
| 14 | グローバル関数 | 旧`Audio::ConvertString`の重複を解消。数学・文字列等の汎用関数は各名前空間へ配置。 | 済 |
| 15 | スマートポインタ | 自作本番コードのオブジェクトは`unique_ptr`、DirectX資源は`ComPtr`を使用。元レビューも100点。 | 維持 |
| 16 | const参照 | 旧レビューが挙げたParticleManagerのCreateParticleGroup/CreateRingParticleGroupは文字列をconst参照で受ける。Trimは文字列を加工し、SetReloadStatusは結果を所有する。撮影データは共有・移動してワーカーへ渡し、StageLevelEditorはObjectDataを一覧へ移すため値渡しを維持。全ての引数は未監査。 | 一部 |
| 17 | 関数の長さ・責務 | Stageの生成・描画準備・描画をFactory/Manager・RenderContext・Rendererへ分担。旧レビューが挙げたStage1GameplaySmoke::Initialize/UpdateとCaptureManager::UpdateSmokeBeforeCaptureは、現行では順序を示す短い呼び出し役。CaptureManager::Initializeの自動確認状態もCaptureManagerSmoke.cppへ移し、通常撮影の準備と役割を分けた。行数だけで再分割しない。 | 一部 |
| 18 | 重複処理 | 旧GameViewCaptureはなく、撮影はCaptureManagerへ集約。Audioの文字列変換も一本化。StageMapFileMonitorの監視開始時に重複していた時刻同期も削除。全領域の類似処理は未監査。 | 一部 |
| 19 | データ駆動 | Stage配置・LightはJSON、MapChipはCSV、Blender出力も利用。元レビューも100点。 | 維持 |
| 20 | 名前空間 | GPUParticle・ParticleManager・Sprite・Model・Object3d・Camera・DirectXCommon・SceneManager・CaptureManager等は`Engine`内。Object3dCollectionは従来の前方宣言との互換を維持するためグローバルのまま、Engine内に別名を公開する。SceneRenderPipeline、BaseScene、LevelLoaderなどもグローバルに残る。ImGuiManagerはStage固有UIとDock状態を併せ持つ。 | 一部 |
| 21 | モダンC++ | C++20、smart pointer、lambda、span等を使用。C++23への変更は現行機能の必要性が未確認で、点数だけのために移行しない。 | 方針維持 |
| 22 | コードの死骸 | 旧GameViewCaptureとMatrixScreenPrintfは現行コードにない。通常の骨付き描画はVertex Shader方式で、Compute SkinningはObject3d::DrawOccludedSilhouetteの骨付き処理で使うが、同関数の現行呼び出し元は見つかっていない。BaseSceneのIsFinishedも現行呼び出し元はなく、公開APIとして残る。 | 一部 |
| 23 | 未使用コード | 4D数学APIは公開ユーティリティ、Checkpoint/GoalはCSVで解釈する将来のStage用型。Stage1の完成ギミックとは数えない。 | 一部 |
| 24 | カプセル化 | Object3dのTransform/Skeleton、PlayerのMovementSettingsはconst参照。Player::GetObjectは描画・透明化用の変更可能な参照を公開しており、直接位置を変更するとColliderとずれる余地が残る。Object3dFactoryのfriendはモデル設定をFactoryに限定するため残す。 | 一部 |
| 25 | クラス分割・移譲 | Stageは進行順、StageMapRepository/Runtimeはデータ、StageRenderContextは描画準備、Rendererは描画。Stage2～5のファイル名定義はあるがデータは未作成で、現在完成しているのはStage1だけ。 | 一部 |
| 26 | ポリモーフィズム | Framework→Game、BaseScene→各Scene、SceneFactoryで仮想関数と基底virtualデストラクタを確認。Enemyは派生しないfinalクラス。ColliderのCheckは形状別オーバーロードのままで、旧レビューの仮想化提案は採用していない。 | 一部 |
| 27 | Stateパターン | SceneManagerはBaseScene経由で各Sceneの更新・描画を切り替え、StageやStageSelectSceneは次Sceneへの変更を要求する。Player・Pause・Title演出については旧レビューが挙げたStateクラス化を行っていない。 | 一部 |
| 28 | デザインパターン | Scene/Object生成のFactory、FrameworkのTemplate Method、Scene状態、登録表などを目的に応じて使用。元レビューも100点。 | 維持 |

## 最新の検証と次の判定材料

Object3dCommonの初期化時間を環境変数`RIBONIN_CHALLENGE_PROFILE_OBJECT3D=1`で記録できるようにしました。2026-10-29 11:45のDevelopment自動起動では3回が152.064、153.437、149.443ミリ秒でした。Shaderコンパイルを含むPipeline一式のCPU側時間で、Shader単体の時間や全体の起動時間ではありません。1回の測定なので環境や負荷による変動は未確認です。計測指定なしではこのログを出しません。

2026-10-29 11:44のDevelopment起動経路でもSkinning.CS.hlslのコンパイル開始・成功は各3回でした。ログを外部から定期的に読み取る試行では、開始と終了が同じ観測回に入る回があり、正確なコンパイル時間を計測できませんでした。0ミリ秒という意味ではなく、この測定では高速化の効果を判断できません。

Object3dCommonのScene間再利用を一度試しましたが、Scene切替時に編集済みShaderや共通.hlsliを再読込する従来の動作が変わるため、再利用変更を取り除きました。各Sceneは再びInitializeを呼びます。再利用を再検討する場合は、Shaderとinclude先の変更・欠損を検出し、既存のエラー処理を維持できることの確認が必要です。

StageMapRepositoryの失敗確認は、旧JSONデータ・Stage番号・監視先を維持することを対象にしています。Stage::ApplyStageMapDataは床などを反映する前に鏡の数とnullを確認しますが、Scene全体が例外発生時にも元へ戻ることはこのテストでは確認していません。

Stage1のGameplay Smokeでは、鏡に向けた検証用の線分を本編の危険Light反射経路へ渡す確認が成功しました（hazardReflectionRoute=1）。実際に時間で動く危険Lightが鏡に当たった記録は約7.3秒の実行中にはなく（hazardReflection=0）、この項目はSmoke全体の成功条件に含まれていません。実際の配置で反射するかは未確認で、この結果だけでは不具合とも正常とも判断できません。

Development／Release／Debug x64のSolutionビルド、Loading→Title→Stage Select→Stage1の起動経路Smoke、Title・Debug・Stage1の撮影Smokeを確認しています。撮影初期化の責務分離後はDevelopmentでTitle・Debugの撮影と録画・リプレイ保存を再確認し、Releaseでは非対応Smokeが失敗ログと終了コード1を返すことを確認しました。撮影画像ではStage1のPlayer・背景・LightとDebugの通常モデル・Animationモデルを確認しました。DirectXCommon::PostDraw/WaitForGPUは、Release/Developmentでも失敗判定が残るようHRESULTと待機結果を確認します。GPU障害でFrameworkのFinalizeが途中停止しても、CaptureManagerの破棄時に所有する撮影スレッドを停止します。FrameworkのDump保存は同分内の上書きとHandleの閉じ忘れを防ぎ、同一プロセスのExceptionPointersに合わせてClientPointersをFALSEにしました。GPU障害と実際のクラッシュによるDump生成は再現していません。これらの自動確認はF5の手入力、白フェードの目視、BGMの聴感を代替しません。

最新の外部AIレビュー点数は不明です。旧52点の具体的な0点項目は大きく改善したため、現行コードの目安は80点前後と見ていますが、これは再採点の結果ではありません。全クラスの命名・コメント等を網羅した証拠はなく、厳密な点数を断定できません。現行の未コミット変更をユーザーが確認し、必要なら通常のcommit・push後に同じ評価条件で再レビューすると、点数と新しい指摘を照合できます。Git操作はユーザーが担当します。
