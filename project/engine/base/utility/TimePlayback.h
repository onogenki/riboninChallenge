#pragma once

#include "Transform.h"

#include <algorithm>
#include <cmath>
#include <vector>

// 戻す操作が有効かどうかだけを共有する軽量な状態クラス。
class ReturnPlaybackState
{
public:
	bool IsReturning() const { return returning_; }
	// UIなどから戻す状態を切り替える。
	void SetReturning(bool returning) { returning_ = returning; }
	// シーン切り替え時に状態を初期値へ戻す。
	void Reset() { returning_ = false; }

private:
	bool returning_ = false;
};

// Transformの編集履歴を時系列のキーフレームとして保存し、
// 戻す・再生のどちらにも同じ履歴を利用するコントローラー。
class TransformPlaybackController
{
public:
	// 編集前後のTransformを記録する。途中から編集した場合は未来側の履歴を破棄する。
	void RecordEdit(const Transform& before, const Transform& after, float elapsedSeconds)
	{
		if (NearlyEqual(before, after)) {
			return;
		}

		const bool createsNewBranch =
			!HasHistory() || returning_ || movingForward_ || canMoveForward_ ||
			playbackTime_ < duration_ - kEpsilon_;
		if (createsNewBranch) {
			keyframes_.clear();
			duration_ = 0.0f;
			playbackTime_ = 0.0f;
			keyframes_.push_back({ before, 0.0f });
		}

		const float editDuration = (std::max)(elapsedSeconds, kMinimumEditDurationSeconds_);
		duration_ += editDuration;
		keyframes_.push_back({ after, duration_ });
		CompactKeyframesIfNeeded();
		playbackTime_ = duration_;
		returning_ = false;
		movingForward_ = false;
		canMoveForward_ = false;
	}

	// trueで履歴を先頭へ向かって再生し、falseで停止する。
	void SetReturning(bool returning)
	{
		if (returning && HasHistory()) {
			returning_ = true;
			movingForward_ = false;
			canMoveForward_ = false;
			return;
		}
		if (!returning && returning_) {
			returning_ = false;
			canMoveForward_ = HasHistory() && playbackTime_ < duration_ - kEpsilon_;
		}
	}

	// 戻した位置から最新の編集状態へ向けて再生を開始する。
	bool StartMoveForward()
	{
		if (!HasHistory() || returning_ || !canMoveForward_ || playbackTime_ >= duration_ - kEpsilon_) {
			return false;
		}
		movingForward_ = true;
		canMoveForward_ = false;
		return true;
	}

	// 現在の再生位置を進め、補間したTransformを呼び出し元へ反映する。
	void Update(Transform& transform, float deltaTime)
	{
		const float step = (std::max)(0.0f, deltaTime);
		if (returning_) {
			playbackTime_ = (std::max)(0.0f, playbackTime_ - step);
			transform = Sample(playbackTime_);
			if (playbackTime_ <= kEpsilon_) {
				playbackTime_ = 0.0f;
				transform = keyframes_.front().transform;
				returning_ = false;
				canMoveForward_ = true;
			}
		} else if (movingForward_) {
			playbackTime_ = (std::min)(duration_, playbackTime_ + step);
			transform = Sample(playbackTime_);
			if (playbackTime_ >= duration_ - kEpsilon_) {
				playbackTime_ = duration_;
				transform = keyframes_.back().transform;
				movingForward_ = false;
			}
		}
	}

	bool IsReturning() const { return returning_; }
	bool IsMovingForward() const { return movingForward_; }
	bool HasHistory() const { return keyframes_.size() >= 2 && duration_ > kEpsilon_; }
	bool CanMoveForward() const { return canMoveForward_ && !returning_ && !movingForward_; }
	float GetProgress() const { return duration_ > kEpsilon_ ? playbackTime_ / duration_ : 1.0f; }
	float GetPlaybackTime() const { return playbackTime_; }
	float GetDuration() const { return duration_; }

	void Reset()
	{
		keyframes_.clear();
		playbackTime_ = 0.0f;
		duration_ = 0.0f;
		returning_ = false;
		movingForward_ = false;
		canMoveForward_ = false;
	}

private:
	static bool NearlyEqual(float lhs, float rhs)
	{
		return std::abs(lhs - rhs) <= kEpsilon_;
	}

	static bool NearlyEqual(const Vector3& lhs, const Vector3& rhs)
	{
		return NearlyEqual(lhs.x, rhs.x) && NearlyEqual(lhs.y, rhs.y) && NearlyEqual(lhs.z, rhs.z);
	}

	static bool NearlyEqual(const Transform& lhs, const Transform& rhs)
	{
		return NearlyEqual(lhs.scale, rhs.scale) &&
			NearlyEqual(lhs.rotate, rhs.rotate) &&
			NearlyEqual(lhs.translate, rhs.translate);
	}

	static Vector3 Interpolate(const Vector3& from, const Vector3& to, float progress)
	{
		return {
			from.x + (to.x - from.x) * progress,
			from.y + (to.y - from.y) * progress,
			from.z + (to.z - from.z) * progress
		};
	}

	static Transform Interpolate(const Transform& from, const Transform& to, float progress)
	{
		return {
			Interpolate(from.scale, to.scale, progress),
			Interpolate(from.rotate, to.rotate, progress),
			Interpolate(from.translate, to.translate, progress)
		};
	}

	// Transformと、そのTransformに到達するまでの累積秒数。
	struct Keyframe
	{
		Transform transform{};
		float time = 0.0f;
	};

	// 指定時刻の前後キーフレームを線形補間してTransformを返す。
	Transform Sample(float time) const
	{
		if (keyframes_.empty()) {
			return {};
		}
		if (time <= keyframes_.front().time) {
			return keyframes_.front().transform;
		}
		if (time >= keyframes_.back().time) {
			return keyframes_.back().transform;
		}

		const auto upper = std::lower_bound(
			keyframes_.begin(),
			keyframes_.end(),
			time,
			[](const Keyframe& keyframe, float value) { return keyframe.time < value; });
		const auto lower = upper - 1;
		const float segmentDuration = upper->time - lower->time;
		const float progress = segmentDuration > kEpsilon_
			? (time - lower->time) / segmentDuration
			: 1.0f;
		return Interpolate(lower->transform, upper->transform, progress);
	}

	// 長時間の編集で履歴が無制限に増えないよう中間キーフレームを間引く。
	void CompactKeyframesIfNeeded()
	{
		if (keyframes_.size() <= kMaximumKeyframes_) {
			return;
		}

		std::vector<Keyframe> compacted;
		compacted.reserve(keyframes_.size() / 2 + 2);
		compacted.push_back(keyframes_.front());
		for (size_t index = 2; index + 1 < keyframes_.size(); index += 2) {
			compacted.push_back(keyframes_[index]);
		}
		compacted.push_back(keyframes_.back());
		keyframes_.swap(compacted);
	}

	static constexpr size_t kMaximumKeyframes_ = 2048;
	static constexpr float kMinimumEditDurationSeconds_ = 1.0f / 240.0f;
	static constexpr float kEpsilon_ = 0.0001f;
	std::vector<Keyframe> keyframes_;
	float playbackTime_ = 0.0f;// 現在再生している履歴上の時刻。
	float duration_ = 0.0f;// 履歴全体の長さ。
	bool returning_ = false;// 先頭へ向かう逆再生中かどうか。
	bool movingForward_ = false;// 最新状態へ向かう順再生中かどうか。
	bool canMoveForward_ = false;// 戻した後に順再生を開始できるかどうか。
};
