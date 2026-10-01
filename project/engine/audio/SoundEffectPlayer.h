#pragma once

#include <string>
#include <unordered_map>

class Audio;

// 効果音の名前とファイルを対応付け、ゲーム側が短い名前だけで再生できるようにする部品です。
// Audio本体は実際のXAudio2再生を担当し、このクラスは効果音一覧だけを担当します。
class SoundEffectPlayer
{
public:
	// Audioは所有しません。Frameworkで初期化済みの共通Audioを渡します。
	void Initialize(Audio* audio);
	// effectNameと音源ファイルを登録し、再生前に一度だけ音源を読み込みます。
	bool Register(const std::string& effectName, const std::string& filename);
	// 登録名の効果音を一回だけ鳴らします。未登録ならfalseです。
	bool Play(const std::string& effectName) const;
	bool IsRegistered(const std::string& effectName) const;
	// Scene終了時などに、名前とファイルの対応だけを消します。Audio本体の共有音源は残します。
	void Clear();

private:
	Audio* audio_ = nullptr;
	std::unordered_map<std::string, std::string> effectFilePaths_;
};
