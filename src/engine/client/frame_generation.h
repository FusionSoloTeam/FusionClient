#ifndef ENGINE_CLIENT_FRAME_GENERATION_H
#define ENGINE_CLIENT_FRAME_GENERATION_H

#include <algorithm>
#include <chrono>

constexpr bool FrameGenerationAllowedInScene(bool EditorActive, bool GameClientAllows, bool GameActive) { return EditorActive || (GameClientAllows && GameActive); }
constexpr bool FrameGenerationAllowsSourceRate(bool EditorActive, bool HasHeadroom) { return EditorActive || HasHeadroom; }

struct SFrameGenerationTiming
{
	int m_Multiplier = 1;
	int m_BaseRate = 0;
	int m_OutputCount = 0; // the cadence sets this per frame; unset falls back to the multiplier
	int m_DisplayRate = 0; // only V-Sync pacing is display-driven

	constexpr int OutputCount() const { return m_OutputCount > 0 ? m_OutputCount : m_Multiplier; }

	constexpr std::chrono::nanoseconds PresentInterval() const
	{
		if(OutputCount() < 2)
			return std::chrono::nanoseconds::zero();
		if(m_DisplayRate > 0)
			return std::chrono::nanoseconds(std::chrono::seconds(1)) / m_DisplayRate;
		return m_BaseRate > 0 ? std::chrono::nanoseconds(std::chrono::seconds(1)) / (OutputCount() * m_BaseRate) : std::chrono::nanoseconds::zero();
	}

	constexpr float Phase(int Step) const { return (float)Step / (float)OutputCount(); }
};

constexpr SFrameGenerationTiming FrameGenerationTiming(int Multiplier, int /*DisplayRate*/, int RenderLimit, int UpdateLimit, bool /*VSync*/ = false)
{
	if(Multiplier < 2 || Multiplier > 4)
		return {1, RenderLimit};

	int BaseRate = RenderLimit;
	if(UpdateLimit > 0 && (BaseRate == 0 || UpdateLimit < BaseRate))
		BaseRate = UpdateLimit;
	return {Multiplier, BaseRate};
}

constexpr std::chrono::nanoseconds FrameGenerationSourceGapLimit(int SourceRate)
{
	const auto Minimum = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(100));
	return SourceRate > 0 ? std::max(Minimum, std::chrono::nanoseconds(std::chrono::seconds(2)) / SourceRate) : Minimum;
}

class CFrameGenerationSourceRate
{
	int m_Limit = -1;
	int m_SourceRate = 0;
	bool m_HasRenderHeadroom = false;
	bool m_BelowLimit = false;
	std::chrono::nanoseconds m_BelowLimitSince{};

public:
	bool HasRenderHeadroom() const { return m_HasRenderHeadroom; }
	int Update(int Limit, float SourceFrameTime, bool Generating, std::chrono::nanoseconds Now = {})
	{
		if(Limit != m_Limit)
		{
			m_Limit = Limit;
			// a raised cap must be tried rather than inheriting the generation overhead
			m_SourceRate = Limit;
			m_BelowLimit = false;
		}
		if(SourceFrameTime > 0.0f)
		{
			const int Measured = std::max(1, (int)(std::min(10000.0f, 1.0f / SourceFrameTime) + 0.5f));
			// generation time is not throughput; stop only when the cap is missed
			const bool ReachesLimit = Limit > 0 && Measured * 10 >= Limit * 9;
			if(!Generating || ReachesLimit || Limit <= 0)
			{
				m_HasRenderHeadroom = ReachesLimit;
				m_BelowLimit = false;
			}
			else if(!m_BelowLimit)
			{
				m_BelowLimit = true;
				m_BelowLimitSince = Now;
			}
			else if(Now - m_BelowLimitSince >= std::chrono::milliseconds(500))
				m_HasRenderHeadroom = false;
			if(!Generating || m_SourceRate == 0)
				m_SourceRate = Limit > 0 ? std::min(Measured, Limit) : Measured;
		}
		return m_SourceRate;
	}
};

class CFrameGenerationCadence
{
	// fractional slack is carried across frames so the output count stays stable
	double m_Credit = 0.0;

public:
	int NextOutputCount(int Multiplier, int DisplayRate, int SourceRate)
	{
		if(Multiplier < 2 || SourceRate <= 0)
		{
			m_Credit = 0.0;
			return 1;
		}
		if(DisplayRate <= 0)
		{
			m_Credit = 0.0;
			return 1;
		}
		const double Ratio = std::clamp((double)DisplayRate / SourceRate, 1.0, (double)Multiplier);
		m_Credit += Ratio - 1.0;
		// avoid losing a frame to floating point rounding on exact ratios
		const int Extra = std::min(Multiplier - 1, (int)(m_Credit + 1e-9));
		m_Credit = std::max(0.0, m_Credit - Extra);
		return 1 + Extra;
	}

	void Reset() { m_Credit = 0.0; }
};

class CFrameGenerationRateGate
{
	bool m_Allowed = false;
	bool m_Changing = false;
	std::chrono::nanoseconds m_ChangeSince{};

public:
	bool Allow(int DisplayRate, float SourceFrameTime, std::chrono::nanoseconds Now)
	{
		if(DisplayRate <= 0)
		{
			m_Allowed = false;
			m_Changing = false;
			return false;
		}
		if(SourceFrameTime <= 0.0f)
		{
			m_Changing = false;
			return m_Allowed;
		}

		// no spare slots at full refresh rate; hysteresis avoids spurious rebuilds
		const float Threshold = m_Allowed ? 1.0f : 0.97f;
		const bool Wanted = SourceFrameTime * DisplayRate > 1.0f / Threshold;
		if(Wanted == m_Allowed)
			m_Changing = false;
		else if(!m_Changing)
		{
			m_Changing = true;
			m_ChangeSince = Now;
		}
		else if(Now - m_ChangeSince >= std::chrono::milliseconds(500))
		{
			m_Allowed = Wanted;
			m_Changing = false;
		}
		return m_Allowed;
	}

	void Reset()
	{
		m_Allowed = false;
		m_Changing = false;
	}
};

#endif // ENGINE_CLIENT_FRAME_GENERATION_H
