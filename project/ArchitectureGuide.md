# CG2 クラス構成メモ

## Scene

`BaseScene`

Title、Debug、Stage1が共通で使うSceneの土台です。全Sceneは`Initialize`、`Update`、`Draw`、`Finalize`を必ず実装します。
共通で持つのは`Object3dCommon`、`SpriteCommon`、`CameraManager`、`MainCamera`だけです。
不要なDirectX、Object、Audioヘッダは含めず、各Sceneが必要なものだけを自分のヘッダで読み込みます。
`CameraManager`や`Camera`を`unique_ptr`で前方宣言するため、BaseSceneの生成・解放は`BaseScene.cpp`で行います。

ヘッダでは`using namespace`を使いません。たとえば衝突用の箱は`MyMath::OBB`と明記し、
そのヘッダを読み込む別のクラスへ名前空間の影響を漏らさないようにします。

### `#include` と前方宣言

ヘッダで値として持つ型は、実体の大きさが必要なので`#include`します。
たとえば`StageLightPuzzle lightPuzzle_`は、Stage1が実体を持つため`StageLightPuzzle.h`が必要です。

ポインタまたは参照だけで扱う型は、`class Player;`のように前方宣言できます。
たとえば`std::unique_ptr<Player> player_`は、Playerの実体を使わない`Stage1.h`では前方宣言だけにし、
`player_->Update()`のように実体を使う`Stage1.cpp`で`Player.h`を読み込みます。

前方宣言した型を`std::unique_ptr`で**所有**する場合は、コンストラクタとデストラクタもcpp側へ置きます。
`Enemy`は`std::unique_ptr<Object3d>`を持つため、`Enemy::Enemy()`と`Enemy::~Enemy()`を`Enemy.cpp`へ置き、
Object3dの完全な型を知る場所で生成・破棄します。

この区別により、Headerを読んだ時に「このSceneが直接持つ部品」と「必要な時だけ使う部品」が分かり、
無関係なファイルまで再コンパイルされにくくなります。

`SceneFactory`

`"TITLE"`、`"DEBUG"`、`"STAGE1"`という名前から、対応するSceneを一体生成します。
ゲームのScene追加は`game/GameSceneRegistration.cpp`の`RegisterGameScenes()`へ、名前と生成関数を登録します。
その登録を`GameSceneRegistry`経由で`SceneFactory`へ渡すため、Stage追加だけでengine側へStageのヘッダを増やす必要はありません。

`SceneManager`

現在のSceneを所有し、切替時に`Finalize → Initialize`の順で呼びます。
各Sceneは`SceneManager::ChangeScene()`で次のScene名を予約するだけで、古いSceneの解放を自分で行いません。

通常起動は`Game::Initialize()`が`LOADING`を予約し、白い`LoadingScene`が`TITLE`を予約します。
Titleから本編に入ると`ChangeSceneWithLoading("STAGE1")`で再びLoadingを経由します。
`StartupRouteSmoke`は環境変数を指定したテスト時だけ`Game`に呼ばれ、この順番と各SceneのDraw回数を確認します。Sceneの生成や所有はしません。

### 入力を読む順番

`Framework::Update()`はScene更新より先に`Input::Update()`を呼びます。`Input`は前フレームの状態を保存した後、`UpdateKeyboard()`→`UpdateMouse()`→`UpdateGamepads()`→`UpdateCurrentDevice()`の順に更新します。`TriggerKey()`や`TriggerMouseButton()`は、保存した前フレームと現在値の差から「押した瞬間」を返します。
`TitleScene`のEnter判定と`Stage1::UpdateMirrorGameplay()`のEキー・マウス判定は、同じ`Input`から読みます。ReleaseでマウスのDirectInput取得に失敗した時だけWindows入力へ切り替えます。キーボードの取得失敗はログに残しますが、現在はWindows入力への切替処理を持ちません。

`Stage1`

ゲーム全体の順番を決めるクラスです。

- Player、鏡、Laserギミックを更新する。
- 通常Cameraと開始演出を更新する。
- Level、Camera、3D Object用クラスを呼び出す。

`Stage1`へ追加してよい処理は、「このStageだけの遊び」に関係するものです。
`Stage1.h`は値として持つ部品だけを直接`#include`し、`unique_ptr`で所有するPlayerやMirrorなどは前方宣言します。
実体を使う`Stage1.cpp`だけがそれらのヘッダを読み込むため、ほかのSceneへ不要な依存が広がりません。

### Stage1を読む順番

`Stage1`を解析する時は、関数を上から全部追うのではなく、次の順番で読みます。

1. `Initialize()`：共通描画設定 → Level読込 → Player → 通常Camera → StageStart の順に作ります。
2. `Update()`：外部データ確認 → Player → 鏡・Puzzle・危険Light → Camera → Edit View → 描画準備の順に更新します。
3. `Draw()`：鏡用RenderTexture更新 → 通常Game View描画 → 共通Pipeline終了の順です。
4. `Finalize()`：Stage1が所有するPlayer・Mirror・Runtime Objectを解放します。

Sceneは「部品を所有し、呼ぶ順番を決める」場所です。個別の計算はPlayer、CameraController、StageLightPuzzle、StageSceneRendererのような部品へ置きます。

### 初めて読む時の入口

迷った時は、次の順に一つずつ開きます。一度に全ファイルを読まないことが重要です。

1. `SceneFactory`：どの名前でどのSceneが作られるか。
2. `SceneManager::Update()`：いつSceneが切り替わり、`Initialize`と`Finalize`が呼ばれるか。
3. 対象Sceneの`Initialize`：何を所有して作るか。
4. 対象Sceneの`Update`：どの順番で部品を更新するか。
5. 対象Sceneの`Draw`：何を描くか。描画開始・終了は`SceneRenderPipeline`へ任せます。

「モデルを追加したい」と思った時は、Sceneの`Update`や`Draw`を直接増やす前に、
`Object3dFactory`で生成し、Sceneが所有するCollectionまたはManagerへ登録できるかを確認します。
「当たり判定を追加したい」と思った時は、PlayerやEnemyへ`CheckCollision()`を追加するのではなく、
まず既存の`SphereCollider`または`ObbCollider`を所有できるかを確認します。

### 関数・ファイルを分ける基準

「行数が多い」だけでは分けません。次のどちらかに当てはまる時に分けます。

1. 処理の開始条件や更新順が別で、同時に読む必要がない。
2. 必要な`#include`や扱うデータが明らかに別の責任になっている。

たとえば`Stage1EditorUi.cpp`はEdit View専用で、本編更新を読む時には不要なので分けます。
一方、`Player::UpdateWithControl()`の入力→移動→重力→Collider→見た目同期は、一回の移動処理として順番に読む意味があるため、細かく分けません。

分けた先の関数名またはファイル名だけで役割が分かることを目標にします。
`UpdateHelper1()`のような名前では分けず、`UpdateStageCamera()`や`Stage1MapLoading.cpp`のように担当を名前へ書きます。

### よく使う処理の早見表

| やりたいこと | 最初に呼ぶもの | 所有者 |
| --- | --- | --- |
| Sceneを切り替える | `SceneManager::ChangeScene("STAGE1")` | SceneManager |
| 新しい通常3Dモデルを作る | `Object3dFactory::Create()` または `Object3dCollection::Create()` | SceneまたはCollection |
| Playerのモデルを準備する | `Player::Initialize()` | Player |
| Playerと壁・Triggerを判定する | `player.CheckCollision(collider)` | Player |
| 一体のEnemyを出現させる | `enemyManager.Spawn(spawnData)` | EnemyManager |
| 設定不要のEnemyを一体出現させる | `enemyManager.Spawn("ghost.obj", position)` | EnemyManager |
| 複数Enemyをまとめて出現させる | `enemyManager.SpawnAll(spawnDataList)` | EnemyManager |
| 全3DモデルへCamera・Lightを反映する | `Object3dRenderContext::UpdateObjects()` | SceneのUpdate |
| 3D一覧を描画する | `SceneRenderPipeline::DrawObjects()` | SceneのDraw |

この表の「所有者」が、解放と状態変更を行う場所です。ほかのクラスは所有せず、参照またはContextで借ります。

## 新しい機能を書く前の判断順

新しいコードを書く前に、最初に「誰がそのデータの寿命とルールを持つか」を決めます。
次の表で一つだけ所有者を選び、ほかのクラスは必要な時だけ参照またはContextで借ります。

| 追加したいもの | 最初に置く場所 | 例 |
| --- | --- | --- |
| 画面を切り替える、Scene全体の開始・終了 | Scene | Title → Stage1の切替 |
| JSON・CSVの読込、配置データ | Level | LevelLoader、StageMapRuntime |
| モデル一体のTransform、Animation、GPU描画準備 | 3D Object | Object3d、Object3dFactory |
| Playerだけの入力、重力、Jump、HP | Player | Player |
| 追従、壁回避、演出、Camera Area | Camera | CameraController、StageStart、StageCameraEvents |
| ボタン、Inspector、Shelf、編集画面 | UI | TitleEditor、DebugSceneEditor、StageEditor |
| StageだけのPuzzle、危険Light、鏡反射 | Stage Gameplay部品 | StageLightPuzzle、StageHazardLights |

たとえば「DoorをLaserで開く」はScene切替でもJSON読込でもないので、`StageLightPuzzle`が所有します。
`Stage1`はDoorモデルとPuzzle部品を所有して、Updateの順番だけを決めます。

`BaseScene::InitializeMainCamera()`は、Title・Debug・Stageが共通して行うMainCameraの生成・登録・有効化だけを担当します。
各Sceneは開始位置を渡し、補助CameraやPlayer追従などの固有処理はそれぞれのSceneへ残します。

## Level

`LevelLoader`

`stage1.json`を読み書きするクラスです。JSONを`ObjectData`へ変換します。

`MapChipField`

`stage1.csv`の`B0`、`P0`などを読むクラスです。モデルやPlayerは作りません。
Stage1は`B0`を`debug/block.obj`の1x1x1ブロック、`P0`をPlayer開始位置へ変換します。
CSVの1マスは`MapChipField::GetPosition()`で3D座標へ変換されるため、
MapChipFieldへモデル名や当たり判定の処理を混ぜません。

`MapChipRegistry`は「種類＋番号」と実行する処理を結びます。`Stage1::ApplyStageMapData()`で`P0`と`B0`を登録し、CSVで読んだ各マスを渡します。
未登録の番号は何も作りません。`StageMapChipFactory::CreateBlockData()`が`B0`を`debug/block.obj`と1×1×1のBOX Colliderへ変換します。

| CSVに書くもの | 現在の結果 | 処理を追加する場所 |
| --- | --- | --- |
| `P0` | Playerの開始位置。JSONのPlayerStartより優先 | `Stage1::ApplyStageMapData()`のP0登録 |
| `B0` | `debug/block.obj`とBOX Collider | 同関数のB0登録、外観・判定値は`StageMapChipFactory` |
| `E0/G0/C0/L0` | 種類として認識するが、まだ生成しない | 各ゲーム機能を作った後、同関数へ番号と処理を登録 |
| `B1`などの未登録番号 | 生成しない | 同関数へ別番号で登録 |
| `,`で区切った空欄 | 何も生成しない。列位置だけ一マス進む | 登録不要 |

CSVファイル自体は任意です。`resources/maps/stage1.csv`がなければJSONだけでStage1を開始できます。
番号別処理を追加する時は、`Stage1::ApplyStageMapData()`のP0/B0登録を見て、同じ場所に種類・番号・処理を一組登録します。
今はEnemyやGoalの実体がないため、対応表だけでそれらがゲームに出るわけではありません。

`StageMapRuntime`

JSON/CSVから渡された通常モデルを実行中の`Object3d`として管理します。

モデル読込・`Object3d`初期化は`Object3dFactory`を使う生成関数へ依頼します。
このクラス自身は`ModelManager`を直接呼ばないため、配置物の実行中データに専念できます。

- モデルの生成
- BOX Colliderの保持
- `control_points`による移動
- 所有する配置物へのCamera・Light・行列更新と通常描画

新しい普通の置物・床・壁を増やす時は、まずここで扱えるかを考えます。
Stage1は`Object3dRenderContext`を一度作り、`StageMapRuntime::UpdateRenderObjects()`へ渡すだけです。
そのため、Stage1がJSON配置物の一覧を直接ループして描画準備を行いません。
鏡反射時も、`StageReflectionRenderer`が反射Cameraと描画順を決め、
`StageMapRuntime::UpdateCameraForDraw()`と`Draw()`へ配置物だけの更新・描画を依頼します。

`Stage1MapLoading.cpp`

`Stage1`が所有する`stage1.json`と`stage1.csv`の、保存時刻監視・再読込・JSON保存だけを置く実装ファイルです。
読込後に実行中のObjectへ反映する判断は引き続き`Stage1::ApplyStageMapData()`が行います。
つまり、ファイルを読む責任と、読んだデータを本編の床・鏡・Cameraへ使う責任を、読み手が別々に追えます。
CSVの読込と座標変換は`MapChipField`、番号ごとの処理選択は`MapChipRegistry`、Stage1専用ブロックの見た目とCollider値は`StageMapChipFactory`が担当します。

`Stage1EditorUi.cpp`

Edit ViewのLevel編集UIと、鏡・Light Puzzle用のDebug UIを置く実装ファイルです。
Object、Player、Mirror、Light Puzzleの所有者は引き続き`Stage1`で、UIはContextに借りたデータを表示・編集するだけです。
本編の更新順を読みたい時は`Stage1.cpp`、Edit Viewを直したい時はこのファイルを開きます。
`Reload Now`はStage1所有のLevelDataを入れ替えるため、そのフレームのUIは古いContextを使わず終了し、次フレームに新しい参照を受け取ります。

`Stage1MirrorGameplay.cpp`

Playerの入力を携帯Mirrorへ渡す処理、固定Mirror・鏡床・携帯Mirrorを反射対象として集める処理、危険LightをMirrorで反射する処理を置く実装ファイルです。
DoorやSwitchの進行規則は`StageLightPuzzle`、いつMirror処理を呼ぶかは`Stage1::Update()`が担当します。
持っている鏡の向きはCameraではなく`Player::GetFacingYaw()`から決めます。`Stage1::UpdateMirrorGameplay()`がこの角度を渡し、`CarryableMirror::Update()`が位置・見た目・反射面・Colliderを同じ角度へ追従させます。

### 固定鏡の配置を変えたい時

`StageMirrorFactory`は、携帯鏡・鏡床の生成と、Levelの配置データを固定鏡へ変換するゲーム側の窓口です。
既存の`FixedMirror`へJSON依存を持ち込まず、汎用の`Object3dFactory`へ鏡専用の反射設定も混ぜません。

| 責任 | 担当 |
| --- | --- |
| JSONを読み、Mirrorタグを分類する | `LevelLoader` → `StageMapObjectIndex` |
| 未所持の携帯鏡を作る | `StageMirrorFactory::CreateCarryableMirror()` |
| 水平・両面反射の鏡床を作る | `StageMirrorFactory::CreateMirrorFloor()` |
| 生成と編集反映を選び、鏡一覧を所有する | `Stage1::ApplyStageMapData()` |
| 配置データから鏡を生成する | `StageMirrorFactory::CreateFixedMirrors()` |
| 既存の鏡へ位置・Y回転・幅・高さ・BOX設定を反映する | `StageMirrorFactory::ApplyFixedMirrorEdits()` |
| モデル・反射Texture・Colliderを所有して同期する | `FixedMirror` |

初期化・再読込では、Sceneが仮一覧を用意してFactoryへ渡し、生成成功後に本番一覧へ移します。
失敗時は途中まで生成した仮一覧を採用しません。この保証は固定鏡一覧についてのもので、Stage全体の変更取消を保証するものではありません。
Editorの位置変更では既存の鏡を更新し、反射Textureを作り直しません。

配置の調整は`resources/levels/stage1.json`のMirrorタグを持つオブジェクト、またはEdit Viewの`StageMirror`で行います。
`plane.obj`の元サイズが幅・高さともに2なので、`scaling.x/y × 2`が鏡の幅・高さです。
BOXの`collider.size`はローカル全辺長で、Factoryが半辺長へ変換し、`FixedMirror`がモデルのTransformに合わせます。
表示・衝突の仕組みを変える時だけ`FixedMirror`を読み、配置調整のために描画基盤を書き換えないようにします。

携帯鏡と鏡床は、Factoryが作った`unique_ptr`をSceneが受け取って所有します。
Factoryは生成だけで、毎フレームの更新・描画やSceneの切替はしません。

```cpp
// Stage1の初期化例です。位置・幅・高さは直接数値で指定できます。
carryableMirror_ = StageMirrorFactory::CreateCarryableMirror(
	object3dCommon, "debug/plane.obj", { -2.5f, -0.8f, 4.5f }, 3.6f, 3.6f);
// 必須の携帯鏡を作れなかった場合、見えないままゲームを続行しません。
if (!carryableMirror_) {
	return false;
}
```

これは`Stage1::InitializeStageGimmicks()`内の初期値です。その後のJSON読込で携帯鏡の位置を上書きするので、
実際の開始位置は`stage1.json`の`CarryableMirrorStart`を編集します。
現在はPlayerStartより前方の`(20, -0.8, 30)`に置いています。持った後の距離はJSONではなく`CarryableMirror.h`の`HoldSettings`で調整します。
鏡床の位置・幅・高さは`Stage1.h`の`mirrorFloorPosition_`／`mirrorFloorWidth_`／`mirrorFloorHeight_`です。
鏡床の水平化・Collider同期・両面反射設定は`CreateMirrorFloor()`にそろえ、Stage側で個別に設定し忘れない形にしています。
Stage1では従来通り、携帯鏡の生成失敗は初期化中断、鏡床の失敗はその部品なしで続行します。

`Stage1Lighting.cpp`

Puzzle Laserと危険Lightの線分を、壁や床を照らすSpotLight配列へ変換する実装ファイルです。
JSONで置いたキー・フィル・バックライトを先に残し、空いているSpotLight枠だけをLaser用に使います。

`CameraController::Update()`

通常の三人称Cameraは、次の順番だけを読みます。

1. `UpdateFocus()`：Playerの位置と進行方向へFocusと視線を追従させる。
2. `UpdateOrbit()`：距離・周回角度・操作していない時の自動背後戻しを更新する。
3. `UpdateCameraTransform()`：壁回避した位置と走行中のFOVをCameraへ反映する。

開始演出のCamera移動は`StageStart`、通常追従と壁回避は`CameraController`です。二つを混ぜず、Stage1はどちらを優先するかだけを決めます。

壁越し表示はCameraの壁回避とは別の機能です。`Object3d::DrawOccludedSilhouette()`を明示的に呼んだObjectだけ、壁の奥へ半透明の形を重ねます。Stage1のLaser発射装置には現在呼んでいないため、通常の深度判定で壁に隠れます。Cameraの位置を変えるだけで壁越し表示は有効になりません。

`StageCollisionWorld`

床・Door・固定Mirror・置いた携帯Mirror・JSON配置物を、用途別のOBB一覧へまとめます。
`GetSolidObbs()`はPlayerとCamera壁回避用、`GetLightBlockingObbs()`はLaser遮蔽用です。
個別の`SphereCollider`や`ObbCollider`を置き換えるものではなく、Stageが持つColliderを集める場所です。
そのためStage1は、Player・Camera・Laserへ必要な一覧を渡すだけで、Colliderを作るループを書きません。

## Collider

`Collider`

当たり判定だけを表す共通の親クラスです。`Player`や`Enemy`はColliderを継承しません。
代わりに、それぞれが必要なColliderを**所有**します。

- `SphereCollider`：Playerや空中のEnemy向けの球形判定
- `ObbCollider`：床・壁・鏡・置物向けの回転できる箱形判定

例えば`Player`は`SphereCollider`を、`FixedMirror`と`StageMapRuntime`は`ObbCollider`を持ちます。
そのため新しいEnemyを作る時も、「EnemyがSphereColliderまたはObbColliderを持つ」と書くだけで、判定形状を統一できます。

PlayerやEnemyを扱うSceneのコードでは、形状を取り出さずに`player.CheckCollision(wallCollider)`のように書きます。
球と箱はどちら側から呼んでも判定でき、返る法線は「呼び出した側を相手から離す向き」です。

Enemyのように球Colliderを持つ相手も、`player.CheckCollision(enemy.GetCollider())`と同じ形で判定できます。
Sceneが`Collision::SphereSphere()`を直接呼ぶ必要はありません。

動く球を床・壁の複数OBBから押し戻す時は、`Player`や`Enemy`が判定ループを書き直しません。
`Collision::ResolveSphereObbs()`へ球・位置・速度・箱一覧を渡すだけです。
入力・重力などのゲーム固有処理は`Player`、共通の判定計算は`Collision`という分担です。

`SphereCollider::ResolveSolidObbs()`は上の呼び出しを包む、ゲーム物体向けの短い窓口です。
Playerや将来の移動Enemyは、Colliderに速度と床・壁OBB一覧を渡すだけで押し戻しと接地判定を受け取れます。
生成時は`SetShape(中心, 半径)`で球の形を一度に設定し、移動中だけ`SetCenter()`で中心を同期します。
物体同士の判定は、形状を取り出さず`player.CheckCollision(mirror.GetCollider())`のように書きます。
Laserのように計算専用の形状が必要な時だけ、`GetSphere()`や`GetObb()`を使います。

`Player::Initialize()`はモデル・Colliderの準備が成功した時だけ`true`を返します。
Stageは失敗時にPlayerを使うCameraや開始演出へ進まないため、途中まで初期化されたPlayerを使いません。

`Player::MovementSettings`は移動速度・重力・ジャンプ・Mirror構え中の倍率をまとめるPlayer専用設定です。
Stage固有の処理を増やさず、`player.GetMovementSettings().gravity`のようにPlayerの動きだけを調整できます。

TriggerやCamera Areaのように「重なったか」だけを知りたい時は、`Collision`名前空間を直接呼ばずに次のように書けます。

```cpp
if (player.CheckCollision(cameraArea.collider).isCollision) {
	// PlayerがCamera Areaへ入った時の処理
}
```

`StageCameraEvents`のEvent TriggerとCamera Areaも`ObbCollider`を所有します。
JSONを読んだ時またはEditorで編集した時に`SyncTransform()`を一度行うため、
毎フレーム生の`OBB`を組み立て直しません。

PlayerとEnemyのように、両方が球Colliderを所有する場合も同じです。

```cpp
if (player.CheckCollision(enemy.GetCollider()).isCollision) {
	// PlayerとEnemyが接触した時の処理
}
```

`Check()`は重なり情報だけを返します。床や壁へ押し戻す時だけ`ResolveSolidObbs()`を使うため、判定の目的がコードから分かります。

Colliderをメンバーへ追加するだけでは、移動や判定は自動実行されません。
ゲーム物体のUpdateで「移動→Colliderへ位置を同期→必要な判定・押し戻し→モデルへ最終位置を反映」を行います。
具体例は`Player::UpdateWithControl()`です。Sceneへ同じ処理をもう一度書く必要はありません。

`SetTrigger(true)`は「通り抜ける判定」の目印です。`Check()`では重なりを検出します。
`SphereCollider::ResolveSolidObbs()`は自分がTriggerなら押し戻しませんが、引数は生のOBB一覧なので、相手のEnabled／Trigger情報は含まれません。
一覧を作る側で床・壁だけを選びます。`StageCollisionWorld`が本編の一覧作成の入口です。

Colliderを持つPlayer、Enemy、Mirror、JSON配置物は、同じ名前の規則を使います。

```cpp
// Player自身へ判定を依頼する。
player.CheckCollision(mirror.GetCollider());

// Laserや床・壁一覧へ渡す、計算用の形状データを受け取る。
const MyMath::Sphere& playerSphere = player.GetSphere();
const MyMath::OBB& mirrorObb = mirror.GetObb();
```

`CheckCollision()`はPlayer・Enemyを扱うScene用の短い窓口です。
`GetCollider()`は汎用Collider処理を直接使う時だけ、`GetSphere()`と`GetObb()`は形状データが必要な時だけ使います。

## Audio：読む・鳴らす・止める担当

| 部品 | 所有するもの | ゲーム側で使う入口 |
| --- | --- | --- |
| `Audio` | 読込済み音源、再生中Voice、XAudio2 | 共通基盤。通常は下の再生部品から使う |
| `BgmPlayer` | 一曲分のVoiceIdとフェード状態。Audioは借りる | `Initialize`、`Play`、`Update`、`Stop` |
| `SoundEffectPlayer` | 効果音名とファイルの対応表。Audioは借りる | `Register("Jump", パス)`、`Play("Jump")` |
| `AudioMixer` | BGM用とSE用の音量倍率 | `SetBgmVolume`、`SetSoundEffectVolume` |

`Framework::Initialize()`がAudioを初期化します。各Sceneはもう一度Audioを初期化せず、共有Audioを借ります。
毎フレームは`Framework::Update()`のAudio更新（終了済みVoiceの回収）→Scene更新→Scene描画の順です。
Sceneが持つBgmPlayerは、SceneのUpdateから一度だけ更新します。

```cpp
// Sceneのメンバーとして所有します。毎フレーム作り直しません。
BgmPlayer bgm_;
SoundEffectPlayer soundEffects_;

// Sceneの初期化で、Frameworkが準備したAudioを借ります。
bgm_.Initialize(Audio::GetInstance());
soundEffects_.Initialize(Audio::GetInstance());

// 登録は読込までで、まだ音は出ません。失敗した場合はゲーム側でログなどへ報告します。
if (!soundEffects_.Register("Notice", "resources/debug/Alarm01.wav")) {
	// 音源の配置・ファイル名を確認するためのエラー処理をここへ書きます。
}

// 必要なイベントが起きた時だけ鳴らします。Updateで無条件に呼ばないでください。
soundEffects_.Play("Notice");

// Sceneの終了時は即停止します。フェード停止には、その後もUpdateが必要です。
bgm_.Stop();
soundEffects_.Clear();
```

上は配置場所ごとの抜粋です。全行を一つの関数へ入れる例ではありません。
`BgmPlayer::Play()`は呼んだ時に再生開始します。画面表示まで待つ機能はBgmPlayer自身にはありません。
本編の例は`Stage1::UpdateStageBgm()`です。`Stage1::Draw()`を通った後のUpdateで、一度だけPlayします。
`Audio::LoadFile()`は再生せず事前読込だけを行えます。

`IsPlaying()`は「未終了のVoiceを保持している」という意味で、一時停止中もtrueです。停止中かは`IsPaused()`と併せて判断します。
`SoundEffectPlayer::Clear()`は名前の登録だけを消し、再生済みSEを止めたり共有音源を解放したりしません。
`BgmPlayer`の破棄だけでは停止しないので、SceneのFinalizeで`Stop()`を呼びます。`Audio::Unload()`は全Sceneで共有する音源を消すため、通常はFrameworkの終了処理だけで使います。

### Mixerの現在の制限

現在のAudioMixerは音量の保存先であり、再生中のVoiceを管理するミキサーバスではありません。
SE音量は次に`SoundEffectPlayer::Play()`した音へ適用されます。
BGM音量はPlay・SetVolume・フェード更新時に適用されます。フェードが終わった後は、Mixerの値だけを変えても自動反映されません。
設定変更直後に`bgm_.SetVolume(曲自体の音量)`を呼ぶと合成音量を反映できますが、進行中のフェードを終える点に注意してください。
この制限の解消は動作変更になるため、構造整理とは分けて検証します。

## Camera

`CameraController`

通常の三人称CameraをPlayerへ追従させるクラスです。

`StageStart`

ゲーム開始時だけPlayerとCameraを動かす演出クラスです。

`StageCameraEvents`

JSONの`EVENT_TRIGGER`、`EVENT_CAMERA`、`CAMERA_AREA`を実行するクラスです。

- PlayerがTriggerへ入るとEvent Cameraを有効にする。
- Event中の手動Camera操作を行う。
- Camera Area内だけ通常Cameraの設定を変更する。

新しいCamera演出がJSONのTriggerやAreaを使うなら、`StageCameraEvents`へ追加します。

## Stage Gameplay

`StageHazardLights`

時間で動く4種類の危険Lightの軌道、反射後のLaser線分、Playerとの接触判定、専用Rendererによる描画を担当します。
Stage1は固定鏡・持てる鏡・床・壁の反射ルールをコールバックで渡し、
受け取った線分をSpotLightへ使います。通常CameraとMirror反射Cameraへの描画は、`StageHazardLights::Draw()`へ任せます。

`StageLightPuzzle`

Charge Laser、Door Laser、二つのSwitch、大型Mirrorの回転、Doorの開閉とDoor Collider、二本のLaserの描画用Rendererを担当します。
`Stage1`は鏡・Door・発射装置のモデルを所有し、毎フレーム必要な非所有ポインタと床・壁OBBを渡すだけです。
通常CameraとMirror反射Cameraへの描画も`StageLightPuzzle::Draw()`へ任せるため、
別Stageで同じPuzzleを使う時は、`StageLightPuzzle`へモデルと設定値を渡して更新できます。

`StageReflectionRenderer`

固定鏡の反射Cameraで、通常Object・JSON配置物・Player・LaserをRenderTextureへ描画します。
一フレームに一枚だけ反射Textureを更新し、描画後はObjectのCamera行列を通常Game Cameraへ戻します。
`Stage1`は鏡・Objectを所有したままContextへ渡すだけなので、反射描画のループを本編更新や通常描画と混ぜません。

`StageSceneRenderer`

通常Game Viewへ、部屋、JSON配置物、鏡、Player、Puzzle Laser、危険Lightを決まった順番で描画します。
鏡面用Pipelineを使った後に通常Object用Pipelineへ戻す処理もここにあり、`Stage1::Draw()`は
「反射Textureを更新する → Game Viewを描く → 共通Pipelineを閉じる」という順番だけを読めます。

新しい危険Lightの軌道を増やす時は、まず`StageHazardLights`へ追加します。
DoorやSwitchを開くStage固有のPuzzleルールはここへ混ぜず、`Stage1`のLight Puzzle側へ書きます。

`StageCollisionWorld`

Stage固有の床、Door、固定／携帯Mirror、JSON配置物から、PlayerとCameraが使う`solidObbs`を作ります。
Lightが遮られる床・Door・壁だけは`lightBlockingObbs`へ分け、Mirrorは反射計算を優先するため含めません。

`Stage1::UpdateStagePlayer`

開始演出中、自動確認中、通常のGame View入力中のどれでPlayerを更新するかを決めます。
この関数の戻り値がtrueの間は開始演出中なので、Mirror操作と通常Camera更新を止めます。

`Stage1::UpdateStageCamera`

開始演出後は通常の追従Cameraを更新し、必要なら開始演出Cameraをその位置へ補間します。
その後にEvent Cameraを更新し、Edit ViewではEvent Cameraを解除してMainCameraへ戻します。

## 3D Object

`Object3d`

一つのモデルを描画するためのクラスです。Sceneのルールを持ちません。
Transform・Animation・Model描画を担当します。

`Object3dGpuData`

`Object3d`一体がGPUへ渡す行列、Light、Camera、鏡反射用のConstant Bufferを担当します。
GPU Resourceを作る・値を更新する・Root Parameterへ結び付ける処理だけを持ち、
PlayerやStageのルールは持ちません。
通常モデルとAnimationモデルの描画では、鏡CameraとGame Cameraが同じフレームで値を上書きしないよう、
`BindForObjectDraw`が描画一回分の行列とCamera位置をSwapChain枠ごとのBufferへ固定します。

この分離により、モデルの動きやAnimationを追いたい時は`Object3d`、
DirectXのConstant Bufferを追いたい時は`Object3dGpuData`だけを読めばよくなります。

`Object3dFactory`

モデル読込、`Object3d`初期化、モデル設定、同名AnimationのLoop開始をまとめて行うクラスです。
SceneはFactoryでObject3dを作った後に、そのSceneだけのTransformを設定します。
Camera・Lightは生成時に個別設定せず、毎フレーム`Object3dRenderContext`が全Objectへ一括設定します。

`SceneFactory`とは名前が似ていますが役割が違います。

- `SceneFactory`：Title、Debug、Stage1などのSceneを作る
- `Object3dFactory`：Scene内に置く一個の3Dモデルを作る

`Object3dFactory::Create()`は`unique_ptr<Object3d>`を新規作成する時、
`Object3dFactory::InitializeObject()`はPlayerやMirrorのように値として所有済みの`Object3d`を準備する時に使います。
どちらもモデル読込、`Object3d::Initialize()`、`SetModel()`をFactory内で一度だけ実行します。
`Object3d`の上記三関数はFactory専用なので、Sceneが初期化やモデル設定の一部を忘れたObject3dを作れません。

SkeletalモデルをLoop再生したいSceneは、生成後に次の一行だけ書きます。

```cpp
Object3dFactory::LoadAndPlayAnimation(*object, "ghost.gltf");
```

Factoryが`resources`から同名Animationを読み、再生できた時だけ`true`を返します。
TitleとDebugは「再生するか」だけを決め、Animationファイルの読込やLoop設定を書き直しません。

生成経路の規則は次の二つです。

- 新しい`unique_ptr<Object3d>`：`Object3dFactory::Create()`
- 値として持つ`Object3d`：`Object3dFactory::InitializeObject()`

Sceneやゲーム物体が`std::make_unique<Object3d>()`、`Object3d::Initialize()`、`SetModel()`を直接呼ばないため、
モデル読込と初期化の手順は一か所だけになります。

`Object3dRenderContext`

Sceneが所有するCamera・DirectionalLight・PointLight・SpotLightを一つにまとめ、`Object3d`へ渡して更新します。
Title、Debug、Stage1、Enemyで同じ「描画準備」の書き方を使います。

Sceneが所有する`std::vector<std::unique_ptr<Object3d>>`は、
`Object3dRenderContext::UpdateObjects()`へ渡すと一覧をまとめて更新できます。
Scene側でCamera・Light設定のループを書き直しません。

`Object3dRenderContext::NormalizeDirectionalLight()`は、Sceneが持つDirectionalLightの方向を
単位ベクトルへそろえます。方向が0なら安全な下向きを設定するため、Title・DebugなどのSceneで
同じ正規化処理を書き直しません。

`Object3dCollection`

一つのSceneが所有する通常3D Objectの生成・一覧・解放だけを担当します。
`Create("debug/floor.obj")`はFactory経由でObjectを作り、Collectionが所有します。
Sceneは返された非所有ポインタで位置・拡縮・Textureだけを設定するため、`unique_ptr`を作ってから`push_back`する重複を避けられます。

```cpp
floor_ = sceneObjects_.Create("debug/floor.obj");
if (!floor_) {
	return;
}
floor_->SetTranslate({ 0.0f, -3.5f, 5.0f });
```

`DebugSceneContent`のような追加用部品も、生成時にはCamera・Lightを設定しません。
生成後の全Objectを`Object3dRenderContext`へ渡すため、Camera・Lightの設定場所は一か所です。

`SceneRenderPipeline`

各SceneがモデルやSpriteをRenderTextureへ描画した後の、PostEffect、SwapChain切替、ImGui、Presentを共通の順番で実行します。
Title、Debug、Stage1は「何を描くか」をそれぞれの`Draw()`に残し、画面へ出す最後の処理を重複して書きません。

通常の3D一覧とSprite一覧は、次の共通関数で描きます。

```cpp
SceneRenderPipeline::DrawObjects(object3dCommon, normalObjects);
SceneRenderPipeline::DrawSprites(spriteCommon, sprites);
```

`DrawObjects()`は、Object3d用の共通Draw設定を行い、空の要素を飛ばしてから描画します。
Debugのように通常モデルとAnimationモデルが混在する一覧だけは、
`ObjectDrawFilter::kNonSkeletalOnly`または`kSkeletalOnly`を指定します。
鏡の反射Textureのように「Cameraを一体ずつ切り替えて描く」特殊な処理は共通化せず、
`StageReflectionRenderer`に残します。

Stage1では、`CreateRuntimeObject()`がJSON配置物の生成だけをFactoryへ依頼します。
Camera・Light・行列更新は、Stage1のUpdate内で一回作る`Object3dRenderContext`が全Objectへまとめて適用します。
「生成」と「毎フレームの描画準備」を混ぜないため、追加モデルも同じ経路で描画できます。

`FixedMirror`や`CarryableMirror`のように、内部に`Object3d`を値として持つ部品は例外です。
それらは自分の初期化中にモデルを設定しますが、Camera・Lightの更新は他のObjectと同じくRenderContextへ任せます。

`FixedMirror`、`CarryableMirror`、`Laser`

鏡とLaserの個別機能を担当します。パズル全体のルールは`Stage1`に書きます。

Laserが持つ反射後の全線分と球の接触は、`Laser::IsHitSphere()`で調べます。
Stage1やSmoke Testが線分を一つずつ回して`Collision::SegmentSphere()`を呼ばないため、
「LaserがPlayerまたはSwitchへ当たったか」が一行で分かります。

```cpp
if (laser.IsHitSphere(player.GetSphere(), laserRadius)) {
	// 反射後を含むLaserがPlayerに届いた。
}
```

`Enemy`、`EnemyManager`

`Enemy`は一体分のモデル・位置・Colliderを所有します。`EnemyManager`はEnemyの寿命を所有し、`Spawn()`で生成します。

```cpp
Enemy::SpawnData spawnData{};
spawnData.modelName = "ghost.obj";
spawnData.position = { 3.0f, 0.0f, 8.0f };
spawnData.colliderRadius = 0.8f;
enemyManager.Initialize(object3dCommon);
enemyManager.Spawn(spawnData);
```

モデル名・位置・Collider半径だけでよい時は、次の短い入口を使えます。

```cpp
enemyManager.Spawn("ghost.obj", { 3.0f, 0.0f, 8.0f }, 0.8f);
```

敵を追加するStageは、Enemyを`new`せず`EnemyManager`へ生成を依頼します。

Enemyの描画準備は、Stageが一度だけ作った`Object3dRenderContext`をManagerへ渡します。

```cpp
Object3dRenderContext renderContext(camera, directionalLight, pointLight, spotLight);
enemyManager.Update(renderContext);
enemyManager.Draw();
```

そのため、EnemyごとにCameraと三種類のLightを渡したり、Enemyごとに描画Contextを作り直したりしません。
個別Enemyを確認する時も、`GetCount()`と`GetEnemy(index)`を使い、Managerが所有する`unique_ptr`の配列は外へ出しません。

敵データが複数あるStageは、`Enemy::SpawnData`の一覧を`EnemyManager::SpawnAll()`へ渡せます。
Sceneは開始時に`Initialize(object3dCommon)`を一度だけ呼び、以後は出現位置などのデータだけを決めます。
Enemyの所有・モデル生成・生成失敗時の扱いはEnemyManagerに任せます。
`Stage1GameplaySmoke`は、Manager経由で二体を生成して`GetCount()`を確認し、`Clear()`後に0体へ戻ることも確認します。
そのため、Enemyがまだ本編Stageへ配置されていない段階でも、生成経路だけを検証できます。

## Debug

`DebugScene::Initialize()`

DebugSceneの開始処理は、次の順番だけを読みます。

1. `InitializeRenderSystems()`：3D・2D・Particleの共通描画状態を初期化する
2. `InitializeCameras()`：MainCameraと補助Cameraを作る
3. `InitializeSceneResources()`：Texture、SkyBox、Audio、Particle素材を読む
4. `InitializeInitialContent()`：Terrain、Animation確認モデル、JSON、初期Spriteを配置する
5. `InitializeLightsAndEmitters()`：LightとEmitterを作る
6. `InitializeDebugTools()`：ECS登録、Shelf、Capture、自動確認を開始する

この分割は、初期化だけのために新しいクラスを増やすものではありません。
DebugSceneが所有するCamera・Object・Spriteの責任は変えず、開始順と失敗する場所を読みやすくしています。

`DebugScene.cpp`は、開始・通常更新・UI・描画という本来の画面処理を読む場所です。
自動確認だけは`DebugSceneAutomation.cpp`へ分けています。環境変数の判定、UI Smoke、時間再生 Smoke、Debug Particleの確認更新を調べる時だけ、そのファイルを開きます。
Scene所有データを各Debug部品へ渡すContextの組み立ては`DebugSceneContexts.cpp`です。

`GameViewCapture`

Debug画面で使うスクリーンショット、AVI録画、30秒リプレイ保存を担当します。
`DebugScene`は保存形式やAVIハンドルを持たず、更新とUI表示を依頼するだけです。

`DebugParticleEffects`

Particle確認用のTexture・Group・GPU Emitter設定、ImGui設定、キー操作、発生状態を担当します。
`DebugScene`はParticleの種類ごとの素材名や設定を持たず、初期化、発生位置の更新、表示を依頼します。
`UpdateFrame()`へ発生位置と経過時間を渡すと、継続発生とDebug Shortcut入力を同じ順番で更新します。

`DebugAnimationPreview`

Debug画面の歩行Animation、手ボーンへ付ける確認用Weapon、足跡GPU Particleをまとめて更新します。
人型・歩行モデルはDebugSceneのAnimation一覧へ追加して寿命を任せ、WeaponとAnimationデータはこの部品が所有します。
DebugSceneは個別の手ボーン名・歩行入力・Weapon生成を知らず、この部品を更新するだけです。
Skeleton Debug表示もこの部品が担当し、DebugSceneは有効状態とGame ViewのCameraだけを渡します。
そのため、本編のPlayerやEnemyの移動処理とは混ざりません。

`DebugEcsInspector`

Debug画面のECSタブだけを表示します。Entity一覧の選択と、3D Entityへ付ける簡易BOX Colliderの編集を担当します。
`DebugEntityRegistry`がECS本体と選択中Entityを所有し、`DebugEcsInspector`へ非所有ポインタで渡します。
このクラスは画面全体ではないため、`BaseScene`を継承しません。

`DebugCollisionOverlay`

Debug画面のモデルとECS BOX Colliderを、Game Viewへ線として重ねて表示します。
青は通常、赤は重なっているAABBです。ゲーム本編の衝突解決は行わず、確認用の表示だけを担当します。
モデル頂点からAABBを作る処理は`Object3d`と`Model`を知る必要があるため、純粋な数値計算だけを置く`MyMath`には入れません。

`DebugAssetPreview`

Debug画面でモデルまたはTextureを一件だけ大きく確認する時の、所有データ・Camera復帰位置・Mouse操作を担当します。
`DebugScene`はShelfから選んだモデルを生成し、中心と大きさを渡すだけです。
プレビューは画面全体ではないため、`BaseScene`を継承しません。

`SceneEditor::HandleShelfDropOnEditView()`

ShelfからEdit ViewへDropする共通処理です。Drop先の判定、枠線、結果メッセージ、プレビュー終了を一か所で扱います。
Sceneごとに置く座標が違う場合だけ、`addModelAtDropPosition`または`addTextureAtDropPosition`のコールバックを渡します。

`DebugTimePlaybackSmoke`

Transform、Animation、Particleの巻き戻しを自動確認し、結果をログへ出すDebug専用クラスです。
`DebugScene`は確認対象のObjectと、モデルを追加・削除する操作窓口だけを渡します。

`DebugUiSmoke`

モデル棚、3D/Textureプレビュー、Inspector選択、Game View保存を自動確認するDebug専用クラスです。
`DebugScene`はモデル一覧を所有したまま、プレビュー・追加・削除の操作窓口だけを渡します。
そのため、Smoke Testの段階・ログ・終了処理を通常のScene更新から分けられます。

`DebugInspectorTabs`

DebugSceneのInspector内にあるParticle、Effect、Camera、ECSのDebug専用タブを表示します。
DebugSceneはEmitter、CameraManager、ECS Worldを所有したまま渡し、この部品はUIによる編集だけを担当します。
TitleやStageで使う共通UIではないため、`SceneEditor`へ混ぜません。

`DebugModelShelf`

DebugSceneのresources一覧、棚の選択、追加・削除後のメッセージを所有します。
モデルやTextureの実際の生成、Drop先の座標変換、Preview Cameraの操作はDebugSceneが担当し、棚はContextの関数を呼ぶだけです。
Resourcesフォルダを開くボタンはShelf自身のUI操作なので、DebugSceneへ専用コールバックを増やさず、この部品が直接担当します。

`DebugSceneEditor`

Debug画面のInspector、Model Shelf、3D・2D Edit View、Collider表示を編集画面として順番に並べます。
Object・Sprite・Light・Preview・ECSの所有権は持たず、`DebugScene`が渡したContextのデータと操作だけを使います。
そのため`DebugScene`は「何を所有するか」と「追加・Preview時に何をするか」に集中できます。

`SceneEditor`は公開関数を変えず、Model Shelf、Viewport、Inspector、resources走査の実装を別cppへ分けます。
Title・Debug・Stageはこれまで通り`SceneEditor::DrawInspector()`を呼び、Inspectorだけを調べたい時は`SceneEditorInspector.cpp`を読みます。
Spriteの選択・移動・回転・拡縮は`SceneEditorSpriteViewport.cpp`へ置き、3Dモデル用ギズモとは分けて読みます。
編集Cameraの右・中ドラッグ・ホイール操作は`SceneEditorViewportCamera.cpp`だけが担当します。
Model ShelfからGame ViewへDropした時の追加処理は`SceneEditorShelfDrop.cpp`へ置き、棚の追加・Preview結果メッセージは共通の`SceneEditorShelfMessages.h`を使います。
resources配下のモデル・Texture走査とサムネイル形状の作成は`SceneEditorShelfScan.cpp`だけが担当します。
Model Shelfの表示件数は`CalculateShelfStatistics()`、選択中カードの取得は`FindSelectedShelfEntry()`へ分けています。
そのため`DrawModelShelf()`を読む時は、集計や検索の詳細を飛ばして、Refresh・追加・カード表示の順に追えます。

`DebugScene`はObject・Camera・Lightの所有者です。各Debug部品へ渡すContextの組み立ては`DebugSceneContexts.cpp`へ置き、`DebugScene.cpp`では開始・更新・描画の流れを優先して読みます。

`DebugEditViewport`

DebugSceneの3Dモデル用・Sprite用Edit Viewに、現在の一覧と選択状態を表示します。
選択が変わった時はContextの関数でDebugSceneへ返すため、ObjectとSpriteの寿命はSceneだけが所有します。

`DebugEditOverlay`

Edit View上へPreview操作とDrag & Dropの説明を重ねて表示します。
Previewを開始・終了する機能やモデルを追加する機能は持たず、DebugSceneから現在の表示状態だけを受け取ります。

`DebugSceneContent`

DebugSceneのModel Shelfや自動確認が使う、3Dモデル生成、Texture Sprite追加、一括削除を担当します。
Object・Sprite・ECSの所有者はDebugSceneのままです。追加後のECS登録と選択状態更新だけをContextの関数でSceneへ返します。

`DebugSceneRenderer`

DebugSceneの通常モデル・Sprite・Particle描画と、モデル／Texture Preview描画の分岐を担当します。
PostEffectとSwapChainへの共通出力は`SceneRenderPipeline`へ任せ、DebugSceneは描画対象を渡した後で自動確認の保存処理だけを行います。

`DebugEntityRegistry`

DebugSceneのObject・SpriteをECS Entityとして登録し、Inspectorで選択中のEntityを管理します。
初期配置とLevel再読込は選択を変えずに登録し、Model Shelfからの追加だけは新しいEntityをInspector選択にします。

`DebugSceneSelection`

DebugSceneの「どの3DモデルまたはSpriteを選択しているか」と、追加直後にInspectorを選ぶ短い状態だけを担当します。
ObjectやSpriteの寿命、ECS Entityの登録は持ちません。
Entityとの対応付けは`DebugEntityRegistry`、選択番号は`DebugSceneSelection`という分担です。

`DebugLevelRuntime`

DebugSceneの`scene.json`読込、Hot Reload、JSON由来モデル範囲の差し替えを担当します。
Debugが最初から置いたモデルとEditorが追加したモデルは残し、JSON由来のモデルだけを入れ替えます。
Hot Reload後のECS再同期と3D選択解除もこの部品が行います。

`DebugGameViewCameraController`

DebugのGame Viewだけで使う、Mouse DragによるCamera移動・回転・Wheel拡縮を担当します。
Edit ViewのCamera操作やAsset PreviewのCamera操作と重ならないよう判定し、DebugSceneはCameraとPreviewを渡すだけです。

`DebugViewportPlacement`

DebugのGame View上でDropしたMouse座標を、3D用の仮想床座標またはSprite用の画面ピクセル座標へ変換します。
3DはCameraの逆ViewProjectionからMouse Rayを作り、Y=0の床との交点を配置位置にします。

`StagePuzzleDebugUi`

Stage1の固定鏡とLight Puzzleを調整するImGui表示を担当します。
Stage1は鏡・Laser・Door・JSONデータを所有し、鏡が編集された時のJSON同期だけを担当します。
このUI部品は本編のMirror反射やDoor開閉のルールを持ちません。
Stage1の床・鏡・追加Collider・Event Trigger・Event Cameraを線で確認する表示も担当します。
この表示は本編の当たり判定を変更せず、Edit Viewで確認するためだけに使います。

`Stage1GameplaySmoke`

環境変数で起動するStage1の自動確認について、毎フレームの結果収集・成功判定・ログ出力・終了処理を担当します。
Stage1本編はPlayer・Mirror・Door・Cameraを更新し、Smoke Testの記録処理を持ちません。

## Title UI

`TitleEditor`

Title画面のModel Shelf、Inspector、3D/2D Edit Viewの選択状態を担当します。
TitleSceneはCamera・Light・Scene切替を担当し、モデル・Spriteは`TitleObjectManager`が所有します。
`TitleEditor`へ一覧と「追加・削除する操作窓口」だけを渡します。
初期モデル・初期画像・SkyBox DDSのどれかが読めない場合、`TitleScene::Initialize()`がログを残し、未生成物の更新・描画を止めます。Scene切替入力だけは受け付けます。

初期配置の通常モデル・Animationモデル・Spriteの数はそれぞれ保持します。
`Clear Title Added`は、この境界より後に追加したものだけを消すため、将来のTitle演出用Animationを消しません。

TitleEditorは`BaseScene`を継承しません。画面全体ではなく、TitleScene内で使うUI部品だからです。

### Titleの配置を変えたい時

`TitleObjectManager`は、Title用の通常モデル・Animationモデル・Spriteと、初期配置の件数をまとめて所有します。
汎用の`Object3dCollection`にはTitleだけの画像配置ルールを追加せず、3Dモデルの生成には既存の`Object3dFactory`を使います。
追加画像が読めない時は`AddTexture()`が`false`を返し、画像の寸法参照やSprite登録を行いません。`TitleEditor`はその失敗を受け取り、未作成の画像を選択しません。

| 変更したいこと | 読む関数 |
| --- | --- |
| 最初から置く背景モデル・画像 | `TitleObjectManager::Initialize()` |
| Shelfから追加するモデルの位置・Animation | `TitleObjectManager::AddModel()` |
| 追加画像の位置・サイズの直接指定 | `TitleObjectManager::AddTexture()`の4引数版。2引数版はShelf用の仮配置（180,160、長辺180以内） |
| 追加分だけを消し、初期配置を残す | `TitleObjectManager::ClearAdded()` |
| Camera・Lightの更新と描画準備 | `TitleScene::UpdateSceneContent()` → `Object3dRenderContext` |
| 描画する順番 | `TitleScene::Draw()` → `SceneRenderPipeline` |
| Enter／Spaceで切り替えるScene | `TitleScene::UpdateSceneTransition()` |

実行順は、Scene初期化 → Managerの初期配置 → 毎フレームのCamera更新 → 描画準備 → Editor操作 → 描画です。
Editorから追加した物は次フレームの描画準備でCamera・Lightを受け取ります。
終了時はSceneがGPU完了を待ってからManagerを解放します。Editorは一覧を借りるだけで、別の所有一覧を作りません。

## Stage Edit UI

`StageEditor`

Stage1のEdit View全体の窓口です。Collider確認、JSONの追加・削除・保存、Lighting編集、Viewport操作を順番に表示します。
発射装置モデルは`Stage1`の`sceneObjects_`が所有し、`StageSceneRenderer`ではほかの通常Objectと同じ一覧から描きます。Rendererへ発射装置を個別に渡す必要はありません。
`Stage Lighting`で照明を変えた時は`Stage1::ApplyStageLighting()`だけを呼びます。床・鏡・PlayerStartなどの配置は再反映しないため、遊びながら明るさを調整しても、置いた携帯鏡の位置は戻しません。設定を次回も使うには`Stage Map Editor`の`Save Map`でJSONを保存します。
`Stage Lighting`と`Stage Player`はInspector内の別タブです。片方を開いても、もう片方の操作欄を覆いません。
Stage1はJSON・Player・鏡・実行中Objectを所有し続け、`StageEditor`には再構築・保存・再読込の操作だけを渡します。
このクラスは画面全体ではないため、`BaseScene`を継承しません。

`Stage Player`の`Player Position`は、今動いているPlayerだけを移動します。`PlayerStart Position`は次回開始位置として`stage1.json`の元データを編集します。現在地を開始位置にしたい時は`Copy Current Player to PlayerStart`を押し、Stage Map Editorの`Save Map`で保存してからStage1を再起動します。`stage1.csv`に`P0`がある時はCSVが優先されるため、JSONの開始位置編集は表示しません。

`Game Camera Yaw (deg)`は通常Cameraの目標角度を度数で指定します。例えば`-135`を入力してGame Viewへ戻ると、Playerの斜め後方へCameraが回ります。Edit View中はCamera更新が止まるため、表示には現在角度ではなく`CameraController::GetTargetOrbitYaw()`の目標角度を使います。この値は実行中だけで、JSONには保存しません。Releaseには編集UIがないため、この欄も表示されません。

同じ`Stage Player`の`Carried Mirror`は、鏡を持っている間の位置を実行中に調整します。`Vertical Distance`／`Horizontal Distance`はPlayerから前方への距離、`Vertical Side`は縦持ち時の左右、各`Height`は高さです。数値をダブルクリックして直接入力できます。値は再起動で戻るため、決まったら`Copy Mirror C++ Defaults`で設定文をコピーし、`CarryableMirror.h`の`HoldSettings`へ貼り付けます。反射方向と当たり判定は、`CarryableMirror::Update()`が同じ位置・角度から更新します。

`StageEditViewport`

Stage1のJSON由来の床・鏡・通常モデルを、Edit Viewで選択・移動・回転・拡縮するUI部品です。
Stage1はLevelDataと実行中のObjectを所有したまま、編集後に`ApplyStageMapData(false)`で見た目とColliderへ反映します。
CSVマップチップを編集する部品ではありません。

`StageLevelEditor`

Stage JSONへモデル、Sphere、Event TriggerとEvent Cameraの組、Camera Area、Path Sphereを追加・削除する編集コマンドとModel Shelfを担当します。
Stage1はJSON保存と、`ApplyStageMapData()`による実行中モデル・Colliderへの反映を担当します。

## 処理の順番

1. `Stage1::Initialize`がJSONとCSVを読む。保存後の再読込と保存処理は`Stage1MapLoading.cpp`を読む。
2. `StageMapRuntime`が通常モデルを作る。
3. `StageCameraEvents`がEvent CameraとCamera Areaを作る。
4. `Stage1::Update`がPlayer、ギミック、Cameraを更新する。
5. `Stage1::Draw`が3DモデルとLaserを描画する。

## 自動確認

通常のゲーム起動では、次の環境変数は設定しません。自動確認する時だけ使います。

| 確認対象 | 環境変数 | 確認する内容 |
| --- | --- | --- |
| Debug UI | `CG2_START_SCENE=DEBUG` と `CG2_DEBUG_UI_SMOKE=1` | Model Shelf、追加、Inspector、Game View保存 |
| Stage1本編 | `CG2_START_SCENE=STAGE1` と `CG2_STAGE1_GAMEPLAY_SMOKE=1` | Collider、EnemyManager、Mirror、Laser、Door、Camera、落下 |
| Title再起動 | `CG2_START_SCENE=TITLE`、`CG2_SCENE_STRESS_SCENE=TITLE`、`CG2_SCENE_STRESS_RESTARTS=2` | Titleの初期化、描画、破棄を二回繰り返す |

Stage1確認の成功ログには`SUCCESS`、`sphereColliderApi=1`、`enemyManager=1`が出ます。
Title再起動確認は終了コード0で成功です。どの確認もゲーム本編のデータを保存・変更しません。

## 新しい処理を置く判断

| やりたいこと | 書く場所 |
| --- | --- |
| JSON/CSVを読む | LevelLoader / MapChipField |
| Stage JSONの読込・保存入口 | LevelLoader::Load / LevelLoader::Save |
| Stage1の外部ファイル監視・再読込・保存処理 | Stage1MapLoading |
| Stage1のEdit View・Puzzle Debug UI | Stage1EditorUi |
| Stage1の携帯Mirror操作・反射対象一覧・危険Light反射 | Stage1MirrorGameplay |
| Stage1のLaser・危険LightをSpotLightへ変換 | Stage1Lighting |
| JSON/CSVから通常モデルを作る | StageMapRuntime |
| 携帯鏡・鏡床を生成し、Levelの固定鏡データを生成・編集反映する | StageMirrorFactory |
| Playerを追従する通常Camera | CameraController |
| TriggerでCameraを変える | StageCameraEvents |
| 時間で動く危険Light | StageHazardLights |
| 一回のLaser描画用の頂点・Camera定数を保持する | LaserRenderer |
| Stage1だけの鏡パズルの規則 | StageLightPuzzle（呼び出す順番はStage1） |
| Stage全体のColliderを用途別一覧へまとめる | StageCollisionWorld |
| Stage1のEdit View全体 | StageEditor |
| 一つのモデルの描画 | Object3d |
| 一つのモデルのGPU Constant Buffer | Object3dGpuData |
| モデルを読んでObject3dを作る | Object3dFactory |
| SceneのCamera・LightをObject3dへ渡して更新する | Object3dRenderContext |
| Scene描画後のPostEffect・SwapChain・ImGui出力 | SceneRenderPipeline |
| DirectX起動、画面サイズ変更、描画先Resourceの管理 | DirectXCommon |
| DirectXの内部起動手順 | CreateDevice → CreateCommandObjects → CreateSwapChain → CreateDepthStencilResource |
| Game ViewのGPU読み戻しとCapture用Readback Slot | DirectXCommonCapture |
| Scene・PostEffect・SwapChainの描画先切替とPresent | DirectXCommonRenderPass |
| Shader・Descriptor・Texture/Buffer Resourceの作成と転送 | DirectXCommonResources |
| Particle Groupの生成、通常Emit、更新、GPU描画 | ParticleManager |
| Hit・Ring・Lightなど既成演出の粒子発生 | ParticleManagerEffects |
| 頂点・材質のGPU Resource作成と通常・Skinning描画 | Model |
| Skeleton構築、Animation読込、Keyframe補間と適用 | ModelAnimation |
| SkinClusterのGPU Resource、Compute Skinning、骨ありモデル描画 | ModelSkinning |
| 通常3DモデルのRootSignature・Graphics Pipeline | Object3dCommon |
| 鏡用・Skinning用・Skybox用の専用Pipeline | Object3dCommonMirror / Object3dCommonSkinning / Object3dCommonSkybox |
| 球・箱の当たり判定を持つ | SphereCollider / ObbCollider |
| Debug画面の画像・動画保存 | GameViewCapture |
| スクリーンショット・動画・リプレイの保存 | CaptureManager |
| AVI・MP4の保存と動画エンコーダー | CaptureManagerVideo |
| 直近30秒のリプレイ圧縮・保存 | CaptureManagerReplay |
| CaptureManagerの自動確認と結果ログ | CaptureManagerSmoke |
| Game View上へCollider・Laser・移動経路を重ねて表示 | ImGuiManagerGameViewDebug |
| Stage1の照明・鏡・Light Puzzleの編集UI | ImGuiManagerStagePuzzle |
| Stage1のJSON配置・Event Camera・Camera Areaを編集するInspector UI | ImGuiManagerStageLevelEditor |
| Sprite・Model・Particle・Cameraの共通Inspector UI | ImGuiManagerInspector |
| Debug画面の歩行Animation・手持ちWeapon・足跡Particle | DebugAnimationPreview |
| Debug画面のUI自動確認 | DebugUiSmoke |
| Debug画面の自動確認開始・進行 | DebugSceneAutomation |
| Debug画面のECS Inspector | DebugEcsInspector |
| Debug画面のParticle・Effect・Camera・ECSタブ | DebugInspectorTabs |
| Debug画面のresources一覧・Model Shelf状態 | DebugModelShelf |
| Debug画面のInspector・Shelf・Edit View・Collider表示の組み立て | DebugSceneEditor |
| Debug画面の3D・2D Edit View | DebugEditViewport |
| Debug画面のPreview・Drop案内表示 | DebugEditOverlay |
| Debug画面のモデル・Texture生成と一括削除 | DebugSceneContent |
| Debug画面の通常描画・Preview描画・PostEffect出力 | DebugSceneRenderer |
| Debug画面のECS Entity登録・同期・Inspector選択 | DebugEntityRegistry |
| Debug画面の3Dモデル・Sprite選択番号とInspector自動選択 | DebugSceneSelection |
| Debug画面のscene.json読込・Hot Reload・JSONモデル差し替え | DebugLevelRuntime |
| Debug Game ViewのMouse Camera操作 | DebugGameViewCameraController |
| Debug画面のDrop座標から3D・2D配置座標への変換 | DebugViewportPlacement |
| Debug画面のCollider可視化 | DebugCollisionOverlay |
| Debug画面のモデル・Textureプレビュー | DebugAssetPreview |
| Stage1の鏡・Light Puzzle調整UI | StagePuzzleDebugUi |
| Stage1のCollider・Event・Camera確認表示 | StagePuzzleDebugUi |
| Stage1自動確認の結果収集・ログ出力 | Stage1GameplaySmoke |
| Title画面のModel Shelf・Inspector・Edit View | TitleEditor |
| Titleのモデル・Spriteの生成・配置・所有・追加分の解放 | TitleObjectManager |
| Stage JSONの3D選択・ギズモ編集 | StageEditViewport |
| Stage JSONのモデル・Event・Camera Area追加／削除 | StageLevelEditor |
| Enemyを生成・一覧管理する | EnemyManager |
