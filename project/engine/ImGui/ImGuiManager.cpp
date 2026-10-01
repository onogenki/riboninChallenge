#include "ImGuiManager.h"
#include "SrvManager.h"
#include "Sprite.h"
#include "Object3d.h"
#include "ParticleEmitter.h"
#include "CameraManager.h"
#include "ModelManager.h"
#include "TextureManager.h"
#include "ParticleManager.h"
#include "SceneManager.h"
#include "PostEffect.h"
#include "CaptureManager.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#ifdef USE_IMGUI
#include "externals/imgui/imgui_internal.h"
#endif
using namespace MyMath;

void ImGuiManager::Initialize([[maybe_unused]] WinApp* winApp, [[maybe_unused]]DirectXCommon* directXCommon, [[maybe_unused]]SrvManager* srvManager)
{
#ifdef USE_IMGUI

	//ImGuiのコンテキストを生成
	ImGui::CreateContext();
	winApp_ = winApp;
	dxCommon_ = directXCommon;
	srvManager_ = srvManager;
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.IniFilename = "imgui_docking.ini";
	//ImGuiのスタイルを設定
	ImGui::StyleColorsDark();
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 3.0f;
	style.FrameRounding = 3.0f;
	style.TabRounding = 3.0f;

	ImGui_ImplWin32_Init(winApp->GetHwnd());

	assert(srvManager->GetDescriptorHeap() != nullptr && "SRV Descriptor Heap is null!");

	//DirectX12用の初期化情報
	ImGui_ImplDX12_InitInfo initInfo = {};

	//初期化情報を設定する
	initInfo.Device = directXCommon->GetDevice();
	initInfo.CommandQueue = directXCommon->GetCommandQueue();
	initInfo.NumFramesInFlight = static_cast<int>(directXCommon->GetSwapChainResourcesNum());
	initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	initInfo.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	initInfo.SrvDescriptorHeap = srvManager->GetDescriptorHeap();

	//SRV確保用関数の設定(ラムダ式)
	initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle,
		D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle)
		{
			SrvManager* srvManager = SrvManager::GetInstance();
			uint32_t index = srvManager->Allocate();
			if (index == SrvManager::kInvalidSrvIndex) {
				*out_cpu_handle = {};
				*out_gpu_handle = {};
				return;
			}
			*out_cpu_handle = srvManager->GetCPUDescriptorHandle(index);
			*out_gpu_handle = srvManager->GetGPUDescriptorHandle(index);
		};

	//SRV解放用関数の設定
	initInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle,
		D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle)
		{
			SrvManager::GetInstance()->Free(cpu_handle);
			//SrvManagerに解放機能を作っていないためここでは何もしない
		};

	//DirectX12用の初期化を行う
	ImGui_ImplDX12_Init(&initInfo);

#endif

}

void ImGuiManager::Begin(const char* sceneName)
{
#ifdef USE_IMGUI
	//ゲームの処理
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	BeginDockSpace(sceneName != nullptr ? sceneName : "Unknown");
#endif
}

void ImGuiManager::End()
{
#ifdef USE_IMGUI
	ImGui::End();
	//ImGuiの内部コマンドを生成する
	ImGui::Render();
#endif
}

#ifdef USE_IMGUI
void ImGuiManager::BeginDockSpace(const char* sceneName)
{
	gameViewDrawList_ = nullptr;
	gameViewImageMin_ = {};
	gameViewImageSize_ = {};
	if (layoutSceneName_ != sceneName) {
		layoutSceneName_ = sceneName;
		layoutResetFrames_ = 120;
	}
	if (layoutResetFrames_ > 0) {
		resetDockLayout_ = true;
		--layoutResetFrames_;
	}

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);

	const ImGuiWindowFlags hostFlags =
		ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_NoBackground;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("Debug Dockspace", nullptr, hostFlags);
	ImGui::PopStyleVar(3);

	if (ImGui::BeginMenuBar()) {
		if (ImGui::BeginMenu("Display")) {
			const WinApp::WindowMode currentMode = winApp_->GetWindowMode();
			if (ImGui::MenuItem("Windowed (Current Size)", nullptr, currentMode == WinApp::WindowMode::Windowed)) {
				winApp_->SetWindowMode(WinApp::WindowMode::Windowed);
			}
			if (ImGui::MenuItem("Maximized (With Close Button)", nullptr, currentMode == WinApp::WindowMode::Maximized)) {
				winApp_->SetWindowMode(WinApp::WindowMode::Maximized);
			}
			if (ImGui::MenuItem("Borderless Fullscreen", nullptr, currentMode == WinApp::WindowMode::BorderlessFullscreen)) {
				winApp_->SetWindowMode(WinApp::WindowMode::BorderlessFullscreen);
			}
			ImGui::Separator();
			ImGui::TextDisabled("Current: %u x %u", winApp_->GetClientWidth(), winApp_->GetClientHeight());
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Windows")) {
			ImGui::MenuItem("Game View", nullptr, &showGameView_);
			ImGui::MenuItem("Edit View", nullptr, &showEditView_);
			ImGui::MenuItem("Scene", nullptr, &showSceneWindow_);
			ImGui::TextDisabled("Top Tools contains FPS / Capture / Post Effect");
			ImGui::Separator();
			ImGui::MenuItem("Inspector", nullptr, &showModelWindow_);
			ImGui::Separator();
			ImGui::MenuItem("ImGui Demo", nullptr, &showDemoWindow_);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Layout")) {
			if (ImGui::MenuItem("Reset to Default")) {
				ResetLayoutToDefault();
			}
			ImGui::EndMenu();
	}
	ImGui::TextDisabled("Scene: %s", sceneName);
	ImGui::SameLine();
	ImGui::TextDisabled("FPS: %.1f", ImGui::GetIO().Framerate);
	ImGui::EndMenuBar();
	}

	const ImGuiID dockspaceId = ImGui::GetID("MainDockspaceV10");
	ImGuiDockNode* dockNode = ImGui::DockBuilderGetNode(dockspaceId);
	const bool hasUsableViewport = viewport->WorkSize.x >= 640.0f && viewport->WorkSize.y >= 360.0f;
	const bool hasTinySavedLayout =
		dockNode != nullptr && (dockNode->Size.x < 128.0f || dockNode->Size.y < 128.0f);
	if (hasUsableViewport && (resetDockLayout_ || dockNode == nullptr || hasTinySavedLayout)) {
		BuildDefaultDockLayout(dockspaceId);
		resetDockLayout_ = false;
	}
	ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

	if (showGameView_) {
		GameViewWindow();
	}
	if (showEditView_) {
		EditViewWindow();
	}

	const std::string currentSceneName = sceneName ? sceneName : "";
	if (showModelWindow_ && IsGameViewActive()) {
		if (inspectorDockId_ != 0) {
			ImGui::SetNextWindowDockID(inspectorDockId_, ImGuiCond_Always);
		}
		ImGui::Begin("Inspector", &showModelWindow_);
		ImGui::TextUnformatted("Game View");
		ImGui::Separator();
		ImGui::TextWrapped("Scene editing is locked. Switch to Edit View to select or transform objects.");
		ImGui::End();
	} else if (showModelWindow_ &&
		currentSceneName != "Debug" &&
		currentSceneName != "Stage1" &&
		currentSceneName != "Title") {
		if (inspectorDockId_ != 0) {
			ImGui::SetNextWindowDockID(inspectorDockId_, ImGuiCond_Always);
		}
		ImGui::Begin("Inspector", &showModelWindow_);
		ImGui::TextDisabled("Scene-specific controls are shown below when available.");
		ImGui::Separator();
		ImGui::End();
	}
	if (showSceneWindow_) {
		SceneWindow(sceneName);
	}
	if (ImGui::Begin("Top Tools")) {
		const ImGuiTableFlags tableFlags =
			ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_BordersInnerV;
		if (ImGui::BeginTable("SharedTopToolsTable", 3, tableFlags, ImVec2(-1.0f, 0.0f))) {
			ImGui::TableSetupColumn("FPS", ImGuiTableColumnFlags_WidthFixed, 160.0f);
			ImGui::TableSetupColumn("Capture", ImGuiTableColumnFlags_WidthFixed, 460.0f);
			ImGui::TableSetupColumn("PostEffect", ImGuiTableColumnFlags_WidthStretch, 1.0f);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted("FPS");
			static float sharedFpsValues[90] = {};
			static int sharedFpsValueOffset = 0;
			sharedFpsValues[sharedFpsValueOffset] = ImGui::GetIO().Framerate;
			sharedFpsValueOffset = (sharedFpsValueOffset + 1) % 90;
			char overlay[32];
			snprintf(overlay, sizeof(overlay), "%.1f FPS", ImGui::GetIO().Framerate);
			const float fpsGraphWidth = (std::max)(120.0f, ImGui::GetContentRegionAvail().x);
			ImGui::PlotLines(
				"##SharedTopToolsFPS",
				sharedFpsValues,
				90,
				sharedFpsValueOffset,
				overlay,
				0.0f,
				120.0f,
				ImVec2(fpsGraphWidth, 58.0f));

			ImGui::TableSetColumnIndex(1);
			CaptureManager::GetInstance()->DrawImGui();

			ImGui::TableSetColumnIndex(2);
			ImGui::TextUnformatted("Post Effect");
			bool isGrayscale = PostEffect::GetInstance()->IsGrayscale();
			bool isSepia = PostEffect::GetInstance()->IsSepia();
			bool isVignette = PostEffect::GetInstance()->IsVignette();
			bool isSmoothing = PostEffect::GetInstance()->IsSmoothing();
			bool isGaussianFilter = PostEffect::GetInstance()->IsGaussianFilter();
			bool isRadialBlur = PostEffect::GetInstance()->IsRadialBlur();
			bool isDissolve = PostEffect::GetInstance()->IsDissolve();
			bool isRandomNoise = PostEffect::GetInstance()->IsRandomNoise();
			bool isLuminanceBasedOutline = PostEffect::GetInstance()->IsLuminanceBasedOutline();
			bool isDepthBasedOutline = PostEffect::GetInstance()->IsDepthBasedOutline();
			if (ImGui::Checkbox("Gray", &isGrayscale) && isGrayscale) {
				isSepia = false;
				isVignette = false;
				isSmoothing = false;
				isGaussianFilter = false;
				isRadialBlur = false;
				isDissolve = false;
				isLuminanceBasedOutline = false;
				isDepthBasedOutline = false;
			}
			ImGui::SameLine();
			if (ImGui::Checkbox("Sepia", &isSepia) && isSepia) {
				isGrayscale = false;
				isVignette = false;
				isSmoothing = false;
				isGaussianFilter = false;
				isRadialBlur = false;
				isDissolve = false;
				isLuminanceBasedOutline = false;
				isDepthBasedOutline = false;
			}
			ImGui::SameLine();
			if (ImGui::Checkbox("Vignette", &isVignette) && isVignette) {
				isGrayscale = false;
				isSepia = false;
				isSmoothing = false;
				isGaussianFilter = false;
				isRadialBlur = false;
				isDissolve = false;
				isLuminanceBasedOutline = false;
				isDepthBasedOutline = false;
			}
			ImGui::SameLine();
			if (ImGui::Checkbox("Smoothing", &isSmoothing) && isSmoothing) {
				isGrayscale = false;
				isSepia = false;
				isVignette = false;
				isGaussianFilter = false;
				isRadialBlur = false;
				isDissolve = false;
				isLuminanceBasedOutline = false;
				isDepthBasedOutline = false;
			}
			ImGui::SameLine();
			if (ImGui::Checkbox("Gaussian", &isGaussianFilter) && isGaussianFilter) {
				isGrayscale = false;
				isSepia = false;
				isVignette = false;
				isSmoothing = false;
				isRadialBlur = false;
				isDissolve = false;
				isLuminanceBasedOutline = false;
				isDepthBasedOutline = false;
			}
			ImGui::SameLine();
			if (ImGui::Checkbox("Radial Blur", &isRadialBlur) && isRadialBlur) {
				isGrayscale = false;
				isSepia = false;
				isVignette = false;
				isSmoothing = false;
				isGaussianFilter = false;
				isDissolve = false;
				isLuminanceBasedOutline = false;
				isDepthBasedOutline = false;
			}
			if (ImGui::Checkbox("Dissolve", &isDissolve) && isDissolve) {
				isGrayscale = false;
				isSepia = false;
				isVignette = false;
				isSmoothing = false;
				isGaussianFilter = false;
				isRadialBlur = false;
				isRandomNoise = false;
				isLuminanceBasedOutline = false;
				isDepthBasedOutline = false;
			}
			ImGui::SameLine();
			if (ImGui::Checkbox("Random Noise", &isRandomNoise) && isRandomNoise) {
				isGrayscale = false;
				isSepia = false;
				isVignette = false;
				isSmoothing = false;
				isGaussianFilter = false;
				isRadialBlur = false;
				isDissolve = false;
				isLuminanceBasedOutline = false;
				isDepthBasedOutline = false;
			}
			ImGui::SameLine();
			if (ImGui::Checkbox("Outline", &isLuminanceBasedOutline) && isLuminanceBasedOutline) {
				isGrayscale = false;
				isSepia = false;
				isVignette = false;
				isSmoothing = false;
				isGaussianFilter = false;
				isRadialBlur = false;
				isDissolve = false;
				isDepthBasedOutline = false;
			}
			ImGui::SameLine();
			if (ImGui::Checkbox("Depth Outline", &isDepthBasedOutline) && isDepthBasedOutline) {
				isGrayscale = false;
				isSepia = false;
				isVignette = false;
				isSmoothing = false;
				isGaussianFilter = false;
				isRadialBlur = false;
				isDissolve = false;
				isLuminanceBasedOutline = false;
			}
			if (isGrayscale || isSepia || isVignette || isSmoothing || isGaussianFilter || isRadialBlur || isLuminanceBasedOutline || isDepthBasedOutline) {
				isDissolve = false;
				isRandomNoise = false;
			}
			PostEffect::GetInstance()->SetGrayscale(isGrayscale);
			PostEffect::GetInstance()->SetSepia(isSepia);
			PostEffect::GetInstance()->SetVignette(isVignette);
			PostEffect::GetInstance()->SetSmoothing(isSmoothing);
			PostEffect::GetInstance()->SetGaussianFilter(isGaussianFilter);
			PostEffect::GetInstance()->SetRadialBlur(isRadialBlur);
			PostEffect::GetInstance()->SetDissolve(isDissolve);
			PostEffect::GetInstance()->SetRandomNoise(isRandomNoise);
			PostEffect::GetInstance()->SetLuminanceBasedOutline(isLuminanceBasedOutline);
			PostEffect::GetInstance()->SetDepthBasedOutline(isDepthBasedOutline);
			if (isDissolve) {
				int dissolveMask = static_cast<int>(PostEffect::GetInstance()->GetDissolveMask());
				const char* dissolveMaskItems[] = { "Noise0", "Noise1" };
				if (ImGui::Combo("Dissolve Mask", &dissolveMask, dissolveMaskItems, _countof(dissolveMaskItems))) {
					PostEffect::GetInstance()->SetDissolveMask(static_cast<PostEffect::DissolveMask>(dissolveMask));
				}
				bool isDissolveEdge = PostEffect::GetInstance()->IsDissolveEdge();
				if (ImGui::Checkbox("Dissolve Edge", &isDissolveEdge)) {
					PostEffect::GetInstance()->SetDissolveEdge(isDissolveEdge);
				}
				float dissolveThreshold = PostEffect::GetInstance()->GetDissolveThreshold();
				float dissolveEdgeWidth = PostEffect::GetInstance()->GetDissolveEdgeWidth();
				float dissolveEdgeColor[3]{};
				PostEffect::GetInstance()->GetDissolveEdgeColor(dissolveEdgeColor[0], dissolveEdgeColor[1], dissolveEdgeColor[2]);
				if (ImGui::SliderFloat("Dissolve Threshold", &dissolveThreshold, 0.0f, 1.0f)) {
					PostEffect::GetInstance()->SetDissolveThreshold(dissolveThreshold);
				}
				if (isDissolveEdge) {
					if (ImGui::SliderFloat("Dissolve Edge Width", &dissolveEdgeWidth, 0.001f, 0.2f)) {
						PostEffect::GetInstance()->SetDissolveEdgeWidth(dissolveEdgeWidth);
					}
					if (ImGui::ColorEdit3("Dissolve Edge Color", dissolveEdgeColor)) {
						PostEffect::GetInstance()->SetDissolveEdgeColor(dissolveEdgeColor[0], dissolveEdgeColor[1], dissolveEdgeColor[2]);
					}
				}
			}
			if (isRandomNoise) {
				float randomNoiseIntensity = PostEffect::GetInstance()->GetRandomNoiseIntensity();
				if (ImGui::SliderFloat("Random Noise Intensity", &randomNoiseIntensity, 0.0f, 1.0f)) {
					PostEffect::GetInstance()->SetRandomNoiseIntensity(randomNoiseIntensity);
				}
			}
			ParticleManager* particleManager = ParticleManager::GetInstance();
			bool particlesReturning = particleManager->IsReturning();
			if (ImGui::Checkbox("Particle return##TopTools", &particlesReturning)) {
				particleManager->SetReturning(particlesReturning);
			}
			ImGui::TextDisabled("Capture uses Game View in every scene.");
			ImGui::EndTable();
		}
	}
	ImGui::End();

	if (IsGameViewActive()) {
		RuntimeMonitorWindow(sceneName);
	} else if (currentSceneName != "Debug" &&
		currentSceneName != "Title" &&
		currentSceneName != "Stage1") {
		if (ImGui::Begin("Model Shelf")) {
			ImGui::TextUnformatted("Edit View Model Shelf");
			ImGui::Separator();
			ImGui::TextWrapped("This scene has not registered a resource shelf yet.");
			if (SceneManager::GetInstance()->GetCurrentSceneName() != "DEBUG") {
				if (ImGui::Button("Go to Debug")) {
					SceneManager::GetInstance()->ChangeScene("DEBUG");
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Reset Dock Layout")) {
				ResetLayoutToDefault();
			}
		}
		ImGui::End();
	}
	if (showDemoWindow_) {
		ImGui::ShowDemoWindow(&showDemoWindow_);
	}
}

void ImGuiManager::ResetLayoutToDefault()
{
	showGameView_ = true;
	showEditView_ = true;
	showSceneWindow_ = true;
	showFpsWindow_ = true;
	showSpriteWindow_ = true;
	showModelWindow_ = true;
	showParticleWindow_ = true;
	showCameraWindow_ = true;
	showDemoWindow_ = false;
	resetDockLayout_ = true;
}

void ImGuiManager::BuildDefaultDockLayout(ImGuiID dockspaceId)
{
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::DockBuilderRemoveNode(dockspaceId);
	ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
	ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->WorkSize);

	ImGuiID centerId = dockspaceId;
	const ImGuiID leftId = ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Left, 0.20f, nullptr, &centerId);
	const ImGuiID bottomId = ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Down, 0.39f, nullptr, &centerId);
	const ImGuiID topId = ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Up, 0.18f, nullptr, &centerId);
	const ImGuiID rightId = ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Right, 0.40f, nullptr, &centerId);
	inspectorDockId_ = rightId;

	ImGui::DockBuilderDockWindow("Edit View", centerId);
	ImGui::DockBuilderDockWindow("Game View", centerId);
	ImGui::DockBuilderDockWindow("Scene", leftId);
	ImGui::DockBuilderDockWindow("Top Tools", topId);
	ImGui::DockBuilderDockWindow("Inspector", rightId);
	ImGui::DockBuilderDockWindow("Model Shelf", bottomId);
	ImGui::DockBuilderDockWindow("Runtime Monitor", bottomId);
	ImGui::DockBuilderFinish(dockspaceId);
}

void ImGuiManager::SceneViewWindow(const char* windowName, SceneViewMode mode, bool& isOpen)
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.015f, 0.015f, 0.015f, 1.0f));
	const bool isVisible = ImGui::Begin(
		windowName,
		&isOpen,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();

	if (isVisible) {
		const bool isOnlyOpenSceneView =
			(mode == SceneViewMode::Game && !showEditView_) ||
			(mode == SceneViewMode::Edit && !showGameView_);
		const bool shouldActivate =
			ImGui::GetWindowDockID() != 0 ||
			isOnlyOpenSceneView ||
			ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) ||
			ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
		if (shouldActivate) {
			activeSceneViewMode_ = mode;
		}
		const ImVec2 available = ImGui::GetContentRegionAvail();
		const float textureWidth = static_cast<float>(dxCommon_->GetClientWidth());
		const float textureHeight = static_cast<float>(dxCommon_->GetClientHeight());
		if (available.x > 0.0f && available.y > 0.0f && textureWidth > 0.0f && textureHeight > 0.0f) {
			const float scale = (available.x / textureWidth < available.y / textureHeight)
				? available.x / textureWidth
				: available.y / textureHeight;
			const ImVec2 imageSize(textureWidth * scale, textureHeight * scale);
			const ImVec2 contentStart = ImGui::GetCursorScreenPos();
			const ImVec2 imageMin(
				contentStart.x + (available.x - imageSize.x) * 0.5f,
				contentStart.y + (available.y - imageSize.y) * 0.5f);
			if (activeSceneViewMode_ == mode) {
				gameViewImageSize_ = imageSize;
				gameViewImageMin_ = imageMin;
				gameViewDrawList_ = ImGui::GetWindowDrawList();
			}

			const uint32_t textureSrvIndex = PostEffect::GetInstance()->IsEnabled()
				? dxCommon_->GetPostEffectTextureSrvIndex()
				: dxCommon_->GetRenderTextureSrvIndex();
			const D3D12_GPU_DESCRIPTOR_HANDLE textureHandle = srvManager_->GetGPUDescriptorHandle(textureSrvIndex);
			ImGui::SetCursorScreenPos(imageMin);
			ImGui::Image(ImTextureRef(static_cast<ImTextureID>(textureHandle.ptr)), imageSize);
		}
	}
	ImGui::End();
}

void ImGuiManager::GameViewWindow()
{
	SceneViewWindow("Game View", SceneViewMode::Game, showGameView_);
}

void ImGuiManager::EditViewWindow()
{
	SceneViewWindow("Edit View", SceneViewMode::Edit, showEditView_);
}

void ImGuiManager::RuntimeMonitorWindow(const char* sceneName)
{
	if (!ImGui::Begin("Runtime Monitor")) {
		ImGui::End();
		return;
	}

	ImGui::TextUnformatted("Game View Runtime Monitor");
	ImGui::Separator();
	ImGui::Text("Scene: %s", sceneName ? sceneName : "Unknown");
	ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
	ImGui::Text("Frame Time: %.2f ms", ImGui::GetIO().DeltaTime * 1000.0f);
	ImGui::Text("Render Size: %u x %u", dxCommon_->GetClientWidth(), dxCommon_->GetClientHeight());
	ImGui::Text("Post Effect: %s", PostEffect::GetInstance()->IsEnabled() ? "Enabled" : "Disabled");
	ImGui::Spacing();
	ImGui::TextWrapped("Object selection and transform editing are disabled in Game View. Use this mode to test gameplay without accidental edits.");
	ImGui::End();
}

void ImGuiManager::SceneWindow(const char* sceneName)
{
	if (!ImGui::Begin("Scene", &showSceneWindow_)) {
		ImGui::End();
		return;
	}
	SceneManager* sceneManager = SceneManager::GetInstance();
	const std::string& currentScene = sceneManager->GetCurrentSceneName();
	const char* currentLabel = currentScene.empty() ? sceneName : currentScene.c_str();

	ImGui::Text("Current Scene");
	ImGui::Separator();
	ImGui::TextUnformatted(currentLabel);
	ImGui::Spacing();

		ImGui::BeginDisabled(sceneManager->HasPendingScene());
	if (ImGui::BeginCombo("Scene", currentLabel)) {
		for (const std::string& availableScene : sceneManager->GetAvailableSceneNames()) {
			const bool isCurrent = availableScene == currentScene;
			if (ImGui::Selectable(availableScene.c_str(), isCurrent) && !isCurrent) {
				sceneManager->ChangeScene(availableScene);
			}
			if (isCurrent) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	if (ImGui::Button("Restart Current Scene")) {
		sceneManager->RestartCurrentScene();
	}
	if (ImGui::Button("Reload Loaded Model / Texture")) {
		ModelManager::GetInstance()->ReloadAllLoadedModels();
		TextureManager::GetInstance()->ReloadAllLoadedTextures();
	}
	ImGui::TextDisabled("Reload keeps the current Scene and replaces loaded model / PNG / DDS data.");
	ImGui::EndDisabled();

	if (sceneManager->HasPendingScene()) {
		const std::string& pendingSceneName = sceneManager->GetPendingSceneName();
		if (!pendingSceneName.empty()) {
			ImGui::TextDisabled("Loading: %s", pendingSceneName.c_str());
		} else {
			ImGui::TextDisabled("Scene reset is settling...");
		}
	}

	ImGui::Separator();
	if (ImGui::Button("Exit Game")) {
		ImGui::OpenPopup("Exit Game?");
	}
	if (ImGui::BeginPopupModal("Exit Game?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::TextUnformatted("Exit game?");
		ImGui::Separator();
		if (ImGui::Button("Yes", ImVec2(96.0f, 0.0f))) {
			winApp_->RequestClose();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("No", ImVec2(96.0f, 0.0f))) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	ImGui::Spacing();
	ImGui::TextDisabled("Drag any debug window by its tab to rearrange it.");
	ImGui::TextDisabled("Use Layout > Reset to Default to restore the layout.");
	ImGui::End();
}
#endif

// Inputのクライアント座標を使い、Releaseではウィンドウ全体をゲーム操作領域とします。
bool ImGuiManager::IsMouseOverGameView(float mouseScreenX, float mouseScreenY) const
{
#ifdef USE_IMGUI
	return gameViewImageSize_.x > 0.0f &&
		gameViewImageSize_.y > 0.0f &&
		mouseScreenX >= gameViewImageMin_.x &&
		mouseScreenX <= gameViewImageMin_.x + gameViewImageSize_.x &&
		mouseScreenY >= gameViewImageMin_.y &&
		mouseScreenY <= gameViewImageMin_.y + gameViewImageSize_.y;
#else
	const DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	return mouseScreenX >= 0.0f && mouseScreenY >= 0.0f &&
		mouseScreenX < static_cast<float>(dxCommon->GetClientWidth()) &&
		mouseScreenY < static_cast<float>(dxCommon->GetClientHeight());
#endif
}

bool ImGuiManager::GetGameViewRect(float& x, float& y, float& width, float& height) const
{
#ifdef USE_IMGUI
	if (gameViewImageSize_.x <= 0.0f || gameViewImageSize_.y <= 0.0f) {
		return false;
	}
	x = gameViewImageMin_.x;
	y = gameViewImageMin_.y;
	width = gameViewImageSize_.x;
	height = gameViewImageSize_.y;
	return true;
#else
	(void)x;
	(void)y;
	(void)width;
	(void)height;
	return false;
#endif
}

bool ImGuiManager::GetGameViewScreenRect(int& x, int& y, int& width, int& height) const
{
#ifdef USE_IMGUI
	float localX = 0.0f;
	float localY = 0.0f;
	float localWidth = 0.0f;
	float localHeight = 0.0f;
	if (!GetGameViewRect(localX, localY, localWidth, localHeight)) {
		return false;
	}

	x = static_cast<int>(std::round(localX));
	y = static_cast<int>(std::round(localY));
	width = static_cast<int>(std::round(localWidth));
	height = static_cast<int>(std::round(localHeight));
	return width > 0 && height > 0;
#else
	(void)x;
	(void)y;
	(void)width;
	(void)height;
	return false;
#endif
}

bool ImGuiManager::IsEditViewActive() const
{
#ifdef USE_IMGUI
	return activeSceneViewMode_ == SceneViewMode::Edit;
#else
	return false;
#endif
}

bool ImGuiManager::IsGameViewActive() const
{
#ifdef USE_IMGUI
	return activeSceneViewMode_ == SceneViewMode::Game;
#else
	return true;
#endif
}

unsigned int ImGuiManager::GetInspectorDockId() const
{
#ifdef USE_IMGUI
	return inspectorDockId_;
#else
	return 0;
#endif
}

void ImGuiManager::DemoWindow()
{
#ifdef USE_IMGUI
	ImGui::ShowDemoWindow();
#endif
}

//FPS
void ImGuiManager::FPSWindow()
{

#ifdef USE_IMGUI
	// ウィンドウ作成
	if (!ImGui::Begin("FPS", &showFpsWindow_)) {
		ImGui::End();
		return;
	}

	// FPS波形グラフの描画
	static float fps_values[90] = {};
	static int values_offset = 0;

	// ImGuiの便利機能で、現在のFPS（1秒間のコマ数）を取得して配列に保存
	fps_values[values_offset] = ImGui::GetIO().Framerate;

	// 90個データを入れたらまた0番目から古いデータを上書きしていく(配列にデータを追加し続けるとパンクするため）
	values_offset = (values_offset + 1) % 90;

	// グラフの上に表示するテキスト（現在のFPS）を作成
	char overlay[32];
	snprintf(overlay, sizeof(overlay), "FPS: %.1f", ImGui::GetIO().Framerate);//FPSの計算

	// グラフ描画（0.0f〜120.0f の範囲で表示。サイズは横幅おまかせ、縦幅80ピクセル）
	ImGui::PlotLines("Performance", fps_values, 90, values_offset, overlay, 0.0f, 120.0f, ImVec2(0, 80));

	ImGui::End();
#endif

}

//スプライトデバック
void ImGuiManager::Draw(DirectXCommon* dxCommon)
{
#ifdef USE_IMGUI

	ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();

	//デスクリプタヒープの配列をセットするコマンド
	SrvManager* srvManager = SrvManager::GetInstance();
	ID3D12DescriptorHeap* ppHeaps[] = { srvManager->GetDescriptorHeap() };

	commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	//描画コマンドを発行
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
#endif
}

void ImGuiManager::Finalize()
{
#ifdef USE_IMGUI

	//ImGuiの終了処理
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
#endif
}
