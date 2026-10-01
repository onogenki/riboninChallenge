#pragma once

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include <algorithm>
#include <cmath>

// SceneEditorの3D・2Dギズモが共通で使う、画面上の点と線分の最短距離計算です。
namespace SceneEditorViewportMath
{
	inline float DistanceToSegment(const ImVec2& point, const ImVec2& start, const ImVec2& end)
	{
		const ImVec2 segment(end.x - start.x, end.y - start.y);
		const ImVec2 fromStart(point.x - start.x, point.y - start.y);
		const float lengthSquared = segment.x * segment.x + segment.y * segment.y;
		const float amount = lengthSquared > 0.0001f
			? std::clamp((fromStart.x * segment.x + fromStart.y * segment.y) / lengthSquared, 0.0f, 1.0f)
			: 0.0f;
		const float dx = point.x - (start.x + segment.x * amount);
		const float dy = point.y - (start.y + segment.y * amount);
		return std::sqrt(dx * dx + dy * dy);
	}
}
#endif
