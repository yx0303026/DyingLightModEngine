#pragma once

#include "sdk/EngineAbi.h"

class EngineHunterMotionTracker final
{
public:
	void Reset() noexcept;
	void ObservePosition(
		const DlModVector3V1 &position,
		double timeSeconds) noexcept;
	void ObserveVelocity(
		const DlModVector3V1 &velocity,
		double timeSeconds) noexcept;
	void SetSkeletonPosition(
		const DlModVector3V1 &position,
		double timeSeconds) noexcept;
	bool Read(DlModHunterMotionV1 &motion, double timeSeconds) const noexcept;

private:
	static constexpr double MaximumSampleAgeSeconds = 0.25;
	static constexpr double MinimumSampleSeconds = 1.0 / 240.0;
	static constexpr double VelocityChangeWindowSeconds = 1.0 / 30.0;
	static constexpr double RecoverySeconds = 0.10;
	static constexpr double MaximumAcceleration = 120.0;
	static constexpr double PositionSlack = 2.0;

	DlModVector3V1 velocities_[3]{};
	double velocityTimes_[3]{};
	uint32_t sampleCount_ = 0;
	DlModVector3V1 position_{};
	double positionTime_ = 0.0;
	double processedVelocityTime_ = 0.0;
	double suppressUntil_ = 0.0;
	DlModVector3V1 velocity_{};
	double velocityTime_ = 0.0;
	DlModVector3V1 acceleration_{};
	double accelerationTime_ = 0.0;
	DlModVector3V1 candidateVelocity_{};
	double candidateSince_ = 0.0;
	uint32_t candidateCount_ = 0;
	DlModVector3V1 skeletonPosition_{};
	double skeletonPositionTime_ = 0.0;
	bool hasVelocity_ = false;
	bool hasAcceleration_ = false;
	bool hasSkeletonPosition_ = false;

	void ClearAcceleration() noexcept;
	void AcceptVelocity(
		const DlModVector3V1 &velocity,
		double timeSeconds) noexcept;
};
