#include "tracking/EngineHunterMotionTracker.h"

#include <algorithm>
#include <cmath>

namespace
{
	bool IsFinite(const DlModVector3V1 &value) noexcept
	{
		return std::isfinite(value.x) && std::isfinite(value.y) &&
			std::isfinite(value.z);
	}

	DlModVector3V1 Difference(
		const DlModVector3V1 &left,
		const DlModVector3V1 &right) noexcept
	{
		return { left.x - right.x, left.y - right.y, left.z - right.z };
	}

	DlModVector3V1 AddScaled(
		const DlModVector3V1 &base,
		const DlModVector3V1 &delta,
		double scale) noexcept
	{
		return {
			static_cast<float>(base.x + delta.x * scale),
			static_cast<float>(base.y + delta.y * scale),
			static_cast<float>(base.z + delta.z * scale)
		};
	}

	double LengthSquared(const DlModVector3V1 &value) noexcept
	{
		return static_cast<double>(value.x) * value.x +
			static_cast<double>(value.y) * value.y +
			static_cast<double>(value.z) * value.z;
	}

	bool IsFresh(double sampleTime, double readTime) noexcept
	{
		const double age = readTime - sampleTime;
		return sampleTime > 0.0 && age >= 0.0 && age <= 0.25;
	}
}

void EngineHunterMotionTracker::Reset() noexcept
{
	*this = EngineHunterMotionTracker{};
}

void EngineHunterMotionTracker::ClearAcceleration() noexcept
{
	sampleCount_ = 0;
	acceleration_ = {};
	accelerationTime_ = 0.0;
	hasAcceleration_ = false;
}

void EngineHunterMotionTracker::ObservePosition(
	const DlModVector3V1 &position,
	double timeSeconds) noexcept
{
	if (!IsFinite(position) || !std::isfinite(timeSeconds) || timeSeconds <= 0.0) {
		Reset();
		return;
	}
	if (positionTime_ > 0.0 && timeSeconds <= positionTime_) {
		return;
	}

	const double elapsed = timeSeconds - positionTime_;
	if (positionTime_ > 0.0 && elapsed <= MaximumSampleAgeSeconds &&
		hasVelocity_ && timeSeconds >= velocityTime_ &&
		timeSeconds - velocityTime_ <= MaximumSampleAgeSeconds) {
		const DlModVector3V1 expected = AddScaled(position_, velocity_, elapsed);
		const double tolerance =
			PositionSlack + 0.5 * MaximumAcceleration * elapsed * elapsed;
		if (LengthSquared(Difference(position, expected)) > tolerance * tolerance) {
			ClearAcceleration();
			candidateCount_ = 0;
			suppressUntil_ = timeSeconds + RecoverySeconds;
		}
	} else if (positionTime_ > 0.0 && elapsed > MaximumSampleAgeSeconds) {
		ClearAcceleration();
	}
	position_ = position;
	positionTime_ = timeSeconds;
}

void EngineHunterMotionTracker::AcceptVelocity(
	const DlModVector3V1 &velocity,
	double timeSeconds) noexcept
{
	velocity_ = velocity;
	velocityTime_ = timeSeconds;
	hasVelocity_ = true;
	candidateCount_ = 0;
	if (timeSeconds < suppressUntil_) {
		ClearAcceleration();
		return;
	}

	if (sampleCount_ == 3) {
		velocities_[0] = velocities_[1];
		velocities_[1] = velocities_[2];
		velocityTimes_[0] = velocityTimes_[1];
		velocityTimes_[1] = velocityTimes_[2];
		sampleCount_ = 2;
	}
	velocities_[sampleCount_] = velocity;
	velocityTimes_[sampleCount_] = timeSeconds;
	++sampleCount_;
	if (sampleCount_ < 3) {
		return;
	}

	const double before = velocityTimes_[1] - velocityTimes_[0];
	const double after = velocityTimes_[2] - velocityTimes_[1];
	const double previousWeight = after / ((before + after) * before);
	const double nextWeight = before / ((before + after) * after);
	const DlModVector3V1 previousDelta = Difference(velocities_[1], velocities_[0]);
	const DlModVector3V1 nextDelta = Difference(velocities_[2], velocities_[1]);
	acceleration_ = {
		static_cast<float>(previousDelta.x * previousWeight + nextDelta.x * nextWeight),
		static_cast<float>(previousDelta.y * previousWeight + nextDelta.y * nextWeight),
		static_cast<float>(previousDelta.z * previousWeight + nextDelta.z * nextWeight)
	};
	const double magnitude = std::sqrt(LengthSquared(acceleration_));
	if (magnitude > MaximumAcceleration) {
		const double scale = MaximumAcceleration / magnitude;
		acceleration_.x = static_cast<float>(acceleration_.x * scale);
		acceleration_.y = static_cast<float>(acceleration_.y * scale);
		acceleration_.z = static_cast<float>(acceleration_.z * scale);
	}
	accelerationTime_ = velocityTimes_[1];
	hasAcceleration_ = true;
}

void EngineHunterMotionTracker::ObserveVelocity(
	const DlModVector3V1 &velocity,
	double timeSeconds) noexcept
{
	if (!IsFinite(velocity) || !std::isfinite(timeSeconds) || timeSeconds <= 0.0) {
		Reset();
		return;
	}
	const double elapsed = timeSeconds - processedVelocityTime_;
	if ((processedVelocityTime_ > 0.0 && elapsed <= 0.0) ||
		(hasVelocity_ && timeSeconds - velocityTime_ < MinimumSampleSeconds)) {
		return;
	}

	const bool restart = !hasVelocity_ || processedVelocityTime_ <= 0.0 ||
		elapsed > MaximumSampleAgeSeconds;
	processedVelocityTime_ = timeSeconds;
	if (restart) {
		ClearAcceleration();
		AcceptVelocity(velocity, timeSeconds);
		return;
	}

	const double trustedElapsed = timeSeconds - velocityTime_;
	const double maximumDelta = MaximumAcceleration *
		(std::clamp)(trustedElapsed, VelocityChangeWindowSeconds, MaximumSampleAgeSeconds);
	if (LengthSquared(Difference(velocity, velocity_)) > maximumDelta * maximumDelta) {
		const double candidateDelta = MaximumAcceleration *
			(std::max)(elapsed, VelocityChangeWindowSeconds);
		if (candidateCount_ == 0 ||
			LengthSquared(Difference(velocity, candidateVelocity_)) > candidateDelta * candidateDelta) {
			candidateSince_ = timeSeconds;
			candidateCount_ = 1;
		} else if (candidateCount_ < 3) {
			++candidateCount_;
		}
		candidateVelocity_ = velocity;
		if (candidateCount_ >= 3 && timeSeconds - candidateSince_ >= RecoverySeconds) {
			ClearAcceleration();
			AcceptVelocity(velocity, timeSeconds);
		}
		return;
	}
	AcceptVelocity(velocity, timeSeconds);
}

void EngineHunterMotionTracker::SetSkeletonPosition(
	const DlModVector3V1 &position,
	double timeSeconds) noexcept
{
	hasSkeletonPosition_ = IsFinite(position) && std::isfinite(timeSeconds) && timeSeconds > 0.0;
	skeletonPosition_ = hasSkeletonPosition_ ? position : DlModVector3V1{};
	skeletonPositionTime_ = hasSkeletonPosition_ ? timeSeconds : 0.0;
}

bool EngineHunterMotionTracker::Read(
	DlModHunterMotionV1 &motion,
	double timeSeconds) const noexcept
{
	motion = {};
	motion.structSize = sizeof(motion);
	motion.abiVersion = MODSDK_ABI_VERSION;
	if (!std::isfinite(timeSeconds) || timeSeconds <= 0.0) {
		return false;
	}
	motion.readTimeSeconds = timeSeconds;
	if (IsFresh(positionTime_, timeSeconds)) {
		motion.position = position_;
		motion.positionTimeSeconds = positionTime_;
		motion.validFields |= DLMOD_HUNTER_HAS_POSITION;
	}
	if (hasVelocity_ && IsFresh(velocityTime_, timeSeconds)) {
		motion.velocity = velocity_;
		motion.velocityTimeSeconds = velocityTime_;
		motion.validFields |= DLMOD_HUNTER_HAS_VELOCITY;
	}
	if ((motion.validFields & DLMOD_HUNTER_HAS_VELOCITY) != 0 &&
		hasAcceleration_ && IsFresh(accelerationTime_, timeSeconds)) {
		motion.acceleration = acceleration_;
		motion.accelerationTimeSeconds = accelerationTime_;
		motion.validFields |= DLMOD_HUNTER_HAS_ACCELERATION;
	}
	if (hasSkeletonPosition_ && IsFresh(skeletonPositionTime_, timeSeconds)) {
		motion.skeletonTransformPosition = skeletonPosition_;
		motion.skeletonTransformPositionTimeSeconds = skeletonPositionTime_;
		motion.validFields |= DLMOD_HUNTER_HAS_SKELETON_POSITION;
	}
	return motion.validFields != 0;
}
