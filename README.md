[![.github/workflows/DebugBuild.yml](https://github.com/onogenki/CG2_00-01/actions/workflows/DebugBuild.yml/badge.svg)](https://github.com/onogenki/CG2_00-01/actions/workflows/DebugBuild.yml)

# CG2_00-01

## 初めて使う方へ

1. `project/CG2_00-01.sln`をVisual Studioで開き、x64でビルドします。開発中の確認にはDebug、普段の起動構成にはDevelopmentがあります。
2. F5で起動するとLoadingを経由してTitleへ進みます。`CG2_START_SCENE`などの自動確認用環境変数を設定した場合は経路が変わります。
3. 本編は`STAGE1`、機能確認・授業用画面は`DEBUG`です。画面左のScene一覧から選択できます。
4. 操作確認はGame View、配置や素材の確認はEdit Viewを使います。Model Shelfの項目をダブルクリックすると、そのSceneへ追加できます。
5. コードを変更する前に[クラス構成と使い方](project/ArchitectureGuide.md)の「初めて読む時の入口」「よく使う処理の早見表」を確認してください。

モデルを一体作る入口は`Object3dFactory`、Camera・Lightの反映は`Object3dRenderContext`、Scene描画の共通手順は`SceneRenderPipeline`です。
Titleの初期配置や追加位置は`TitleObjectManager`、本編の進行は`Stage1`を読みます。
Editorで追加した要素を自動的に全Sceneへ共有する仕組みではありません。Stage1のJSON保存と、Title／Debugの実行中の追加を区別してください。

再整理の進行状況と未確認事項は[再点検記録](project/RefactoringReview.md)に記載します。部分的な確認を全機能の動作保証とはしません。

## 実装した機能

### Skinning / Animation

- Skinningモデルの表示
- 通常のAnimation描画はVertex Shaderによるスキニング（`Object3d::Draw()`）
- Compute Shaderによるスキニングの実装も保持。通常描画の経路とは区別する
- アニメーション補間
- Boneのデバッグ表示
- MultiMesh対応
- MultiMaterial対応
  - モデル読み込み時にMeshごとのMaterialとIndex範囲を保持する。
  - 描画時はMeshごとに対応するテクスチャを設定して描画する。

### 手に持つ武器

- `human.gltf` の手Joint `ボーン.016` に追従する武器を実装した。
- `sphere.obj` に `monsterBall.png` を設定し、モンスターボールとして表示する。

### GPU Particle

- Compute ShaderでParticleの初期化、射出、更新を行う。
- FreeListを用いて寿命切れParticleを再利用する。
- Particleの生存期間、移動、alphaの減衰をGPU上で管理する。
- アクティブなParticle数をGPU上で数え、ExecuteIndirectで描画する。
- `walk.gltf` の左右の足JointからParticleを発生させる。
- 2個のGPU Emitterを使用する。
- Sphere / Box / Cone / Meshを混ぜてParticleを発生させる。
  - Mesh EmitterはShader内で定義した四面体の三角形面を使用する。
- Emit Compute Shaderは64 threadでParticle生成を並列処理する。
- 加速度とdragを持つGPU FieldをUpdate Compute Shaderへ追加した。
- Trail型Particleを追加した。
- Directional Lightの色、方向、強度をGPU Particleの描画へ反映する。

## 操作

- DebugSceneでWASDを押すと、歩行確認用の`walk.gltf` が移動する。
- 移動中は歩行アニメーションを再生する。
- 歩行中、左右の足元からGPU Particleが発生する。

## 補足

- 手に武器を持たせる機能は実装済み。
- GPU Particleは歩行モデルの足元から発生する設定であり、手からParticleを発生させる設定にはしていない。
