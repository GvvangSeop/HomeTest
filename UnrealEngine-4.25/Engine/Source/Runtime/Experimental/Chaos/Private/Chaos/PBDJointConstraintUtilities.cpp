// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/PBDJointConstraintUtilities.h"
#include "Chaos/DenseMatrix.h"
#include "Chaos/ParticleHandle.h"
#include "Chaos/Utilities.h"

//PRAGMA_DISABLE_OPTIMIZATION

namespace Chaos
{
	void FPBDJointUtilities::DecomposeSwingTwistLocal(const FRotation3& R0, const FRotation3& R1, FRotation3& R01Swing, FRotation3& R01Twist)
	{
		const FRotation3 R01 = R0.Inverse() * R1;
		R01.ToSwingTwistX(R01Swing, R01Twist);
	}

	void FPBDJointUtilities::GetSwingTwistAngles(const FRotation3& R0, const FRotation3& R1, FReal& TwistAngle, FReal& Swing1Angle, FReal& Swing2Angle)
	{
		FRotation3 R01Twist, R01Swing;
		FPBDJointUtilities::DecomposeSwingTwistLocal(R0, R1, R01Swing, R01Twist);
		TwistAngle = R01Twist.GetAngle();
		Swing1Angle = 4.0f * FMath::Atan2(R01Swing.Z, 1.0f + R01Swing.W);
		Swing2Angle = 4.0f * FMath::Atan2(R01Swing.Y, 1.0f + R01Swing.W);
	}

	FReal FPBDJointUtilities::GetTwistAngle(const FRotation3& InTwist)
	{
		FRotation3 Twist = InTwist.GetNormalized();
		ensure(FMath::Abs(Twist.W) <= 1.0f);
		FReal Angle = Twist.GetAngle();
		if (Angle > PI)
		{
			Angle = Angle - (FReal)2 * PI;
		}
		if (Twist.X < 0.0f)
		{
			Angle = -Angle;
		}
		return Angle;
	}


	void FPBDJointUtilities::GetTwistAxisAngle(
		const FRotation3& R0,
		const FRotation3& R1,
		FVec3& Axis,
		FReal& Angle)
	{
		FRotation3 R01Twist, R01Swing;
		FPBDJointUtilities::DecomposeSwingTwistLocal(R0, R1, R01Swing, R01Twist);

		Axis = R1 * FJointConstants::TwistAxis();
		Angle = FPBDJointUtilities::GetTwistAngle(R01Twist);
	}


	void FPBDJointUtilities::GetConeAxisAngleLocal(
		const FRotation3& R0,
		const FRotation3& R1,
		const FReal AngleTolerance,
		FVec3& AxisLocal,
		FReal& Angle)
	{
		// Decompose rotation of body 1 relative to body 0 into swing and twist rotations, assuming twist is X axis
		FRotation3 R01Twist, R01Swing;
		FPBDJointUtilities::DecomposeSwingTwistLocal(R0, R1, R01Swing, R01Twist);

		R01Swing.ToAxisAndAngleSafe(AxisLocal, Angle, FJointConstants::Swing1Axis(), AngleTolerance);
		if (Angle > PI)
		{
			Angle = Angle - (FReal)2 * PI;
		}
	}


	void FPBDJointUtilities::GetLockedSwingAxisAngle(
		const FRotation3& R0,
		const FRotation3& R1,
		const EJointAngularConstraintIndex SwingConstraintIndex,
		FVec3& Axis,
		FReal& Angle)
	{
		// NOTE: this differs from GetDualConeSwingAxisAngle in that it returns an axis with length Sin(SwingAngle)
		// and an Angle that is actually Sin(SwingAngle). This allows it to be used when we get closer to degenerate twist angles.
		FVec3 Twist1 = R1 * FJointConstants::TwistAxis();
		FVec3 Swing0 = R0 * FJointConstants::OtherSwingAxis(SwingConstraintIndex);
		Axis = FVec3::CrossProduct(Swing0, Twist1);
		Angle = -FVec3::DotProduct(Swing0, Twist1);
	}


	void FPBDJointUtilities::GetDualConeSwingAxisAngle(
		const FRotation3& R0,
		const FRotation3& R1,
		const EJointAngularConstraintIndex SwingConstraintIndex,
		FVec3& Axis,
		FReal& Angle)
	{
		FVec3 Twist1 = R1 * FJointConstants::TwistAxis();
		FVec3 Swing0 = R0 * FJointConstants::OtherSwingAxis(SwingConstraintIndex);
		Axis = FVec3::CrossProduct(Swing0, Twist1);
		Angle = 0.0f;
		if (Utilities::NormalizeSafe(Axis, KINDA_SMALL_NUMBER))
		{
			FReal SwingTwistDot = FVec3::DotProduct(Swing0, Twist1);
			Angle = FMath::Asin(FMath::Clamp(-SwingTwistDot, -1.0f, 1.0f));
		}
	}


	void FPBDJointUtilities::GetSwingAxisAngle(
		const FRotation3& R0,
		const FRotation3& R1,
		const FReal AngleTolerance,
		const EJointAngularConstraintIndex SwingConstraintIndex,
		FVec3& Axis,
		FReal& Angle)
	{
		// Decompose rotation of body 1 relative to body 0 into swing and twist rotations, assuming twist is X axis
		FRotation3 R01Twist, R01Swing;
		FPBDJointUtilities::DecomposeSwingTwistLocal(R0, R1, R01Swing, R01Twist);
		const FReal R01SwingYorZ = (FJointConstants::AxisIndex(SwingConstraintIndex) == 2) ? R01Swing.Z : R01Swing.Y;	// Can't index a quat :(
		Angle = 4.0f * FMath::Atan2(R01SwingYorZ, 1.0f + R01Swing.W);
		const FVec3& AxisLocal = (SwingConstraintIndex == EJointAngularConstraintIndex::Swing1) ? FJointConstants::Swing1Axis() : FJointConstants::Swing2Axis();
		Axis = R0 * AxisLocal;
	}


	void FPBDJointUtilities::GetLockedAxes(const FRotation3& R0, const FRotation3& R1, FVec3& Axis0, FVec3& Axis1, FVec3& Axis2)
	{
		const FReal W0 = R0.W;
		const FReal W1 = R1.W;
		const FVec3 V0 = FVec3(R0.X, R0.Y, R0.Z);
		const FVec3 V1 = FVec3(R1.X, R1.Y, R1.Z);

		const FVec3 C = V1 * W0 + V0 * W1;
		const FReal D0 = W0 * W1;
		const FReal D1 = FVec3::DotProduct(V0, V1);
		const FReal D = D0 - D1;

		Axis0 = 0.5f * (V0 * V1.X + V1 * V0.X + FVec3(D, C.Z, -C.Y));
		Axis1 = 0.5f * (V0 * V1.Y + V1 * V0.Y + FVec3(-C.Z, D, C.X));
		Axis2 = 0.5f * (V0 * V1.Z + V1 * V0.Z + FVec3(C.Y, -C.X, D));

		// Handle degenerate case of 180 deg swing
		if (FMath::Abs(D0 + D1) < SMALL_NUMBER)
		{
			const FReal Epsilon = SMALL_NUMBER;
			Axis0.X += Epsilon;
			Axis1.Y += Epsilon;
			Axis2.Z += Epsilon;
		}
	}
	
	
	// @todo(ccaulfield): separate linear soft and stiff
	FReal FPBDJointUtilities::GetLinearStiffness(
		const FPBDJointSolverSettings& SolverSettings,
		const FPBDJointSettings& JointSettings)
	{
		return (SolverSettings.Stiffness > (FReal)0) ? SolverSettings.Stiffness : JointSettings.Stiffness;
	}

	FReal FPBDJointUtilities::GetSoftLinearStiffness(
		const FPBDJointSolverSettings& SolverSettings,
		const FPBDJointSettings& JointSettings)
	{
		return (SolverSettings.SoftLinearStiffness > (FReal)0) ? SolverSettings.SoftLinearStiffness : JointSettings.SoftLinearStiffness;
	}

	FReal FPBDJointUtilities::GetSoftLinearDamping(
		const FPBDJointSolverSettings& SolverSettings,
		const FPBDJointSettings& JointSettings)
	{
		return (SolverSettings.SoftLinearDamping > (FReal)0) ? SolverSettings.SoftLinearDamping : JointSettings.SoftLinearDamping;
	}

	FReal FPBDJointUtilities::GetTwistStiffness(
		const FPBDJointSolverSettings& SolverSettings,
		const FPBDJointSettings& JointSettings)
	{
		return (SolverSettings.Stiffness > (FReal)0) ? SolverSettings.Stiffness : JointSettings.Stiffness;
	}

	FReal FPBDJointUtilities::GetSoftTwistStiffness(
		const FPBDJointSolverSettings& SolverSettings,
		const FPBDJointSettings& JointSettings)
	{
		return (SolverSettings.SoftTwistStiffness > 0)? SolverSettings.SoftTwistStiffness : JointSettings.SoftTwistStiffness;
	}

	FReal FPBDJointUtilities::GetSoftTwistDamping(
		const FPBDJointSolverSettings& SolverSettings,
		const FPBDJointSettings& JointSettings)
	{
		return (SolverSettings.SoftTwistDamping > 0) ? SolverSettings.SoftTwistDamping : JointSettings.SoftTwistDamping;
	}

	FReal FPBDJointUtilities::GetSwingStiffness(
		const FPBDJointSolverSettings& SolverSettings,
		const FPBDJointSettings& JointSettings)
	{
		return (SolverSettings.Stiffness > (FReal)0) ? SolverSettings.Stiffness : JointSettings.Stiffness;
	}

	FReal FPBDJointUtilities::GetSoftSwingStiffness(
		const FPBDJointSolverSettings& SolverSettings,
		const FPBDJointSettings& JointSettings)
	{
		return (SolverSettings.SoftSwingStiffness > 0) ? SolverSettings.SoftSwingStiffness : JointSettings.SoftSwingStiffness;
	}

	FReal FPBDJointUtilities::GetSoftSwingDamping(
		const FPBDJointSolverSettings& SolverSettings,
		const FPBDJointSettings& JointSettings)
	{
		return (SolverSettings.SoftSwingDamping > 0) ? SolverSettings.SoftSwingDamping : JointSettings.SoftSwingDamping;
	}

	FReal FPBDJointUtilities::GetLinearDriveStiffness(
		const FPBDJointSolverSettings& SolverSettings,
		const FPBDJointSettings& JointSettings)
	{
		return (SolverSettings.LinearDriveStiffness > 0.0f) ? SolverSettings.LinearDriveStiffness : JointSettings.LinearDriveStiffness;
	}

	FReal FPBDJointUtilities::GetLinearDriveDamping(
		const FPBDJointSolverSettings& SolverSettings,
		const FPBDJointSettings& JointSettings)
	{
		return (SolverSettings.LinearDriveDamping > 0.0f) ? SolverSettings.LinearDriveDamping : JointSettings.LinearDriveDamping;
	}

	FReal FPBDJointUtilities::GetAngularTwistDriveStiffness(
		const FPBDJointSolverSettings& SolverSettings,
		const FPBDJointSettings& JointSettings)
	{
		if (JointSettings.bAngularTwistPositionDriveEnabled)
		{
			return (SolverSettings.AngularDriveStiffness > 0.0f) ? SolverSettings.AngularDriveStiffness : JointSettings.AngularDriveStiffness;
		}
		return 0.0f;
	}

	FReal FPBDJointUtilities::GetAngularTwistDriveDamping(
		const FPBDJointSolverSettings& SolverSettings,
		const FPBDJointSettings& JointSettings)
	{
		if (JointSettings.bAngularTwistVelocityDriveEnabled)
		{
			return (SolverSettings.AngularDriveDamping > 0.0f) ? SolverSettings.AngularDriveDamping : JointSettings.AngularDriveDamping;
		}
		return 0.0f;
	}

	FReal FPBDJointUtilities::GetAngularSwingDriveStiffness(
		const FPBDJointSolverSettings& SolverSettings,
		const FPBDJointSettings& JointSettings)
	{
		if (JointSettings.bAngularSwingPositionDriveEnabled)
		{
			return (SolverSettings.AngularDriveStiffness > 0.0f) ? SolverSettings.AngularDriveStiffness : JointSettings.AngularDriveStiffness;
		}
		return 0.0f;
	}

	FReal FPBDJointUtilities::GetAngularSwingDriveDamping(
		const FPBDJointSolverSettings& SolverSettings,
		const FPBDJointSettings& JointSettings)
	{
		if (JointSettings.bAngularSwingVelocityDriveEnabled)
		{
			return (SolverSettings.AngularDriveDamping > 0.0f) ? SolverSettings.AngularDriveDamping : JointSettings.AngularDriveDamping;
		}
		return 0.0f;
	}

	FReal FPBDJointUtilities::GetAngularSLerpDriveStiffness(
		const FPBDJointSolverSettings& SolverSettings,
		const FPBDJointSettings& JointSettings)
	{
		if (JointSettings.bAngularSLerpPositionDriveEnabled)
		{
			return (SolverSettings.AngularDriveStiffness > 0.0f) ? SolverSettings.AngularDriveStiffness : JointSettings.AngularDriveStiffness;
		}
		return 0.0f;
	}

	FReal FPBDJointUtilities::GetAngularSLerpDriveDamping(
		const FPBDJointSolverSettings& SolverSettings,
		const FPBDJointSettings& JointSettings)
	{
		if (JointSettings.bAngularSLerpVelocityDriveEnabled)
		{
			return (SolverSettings.AngularDriveDamping > 0.0f) ? SolverSettings.AngularDriveDamping : JointSettings.AngularDriveDamping;
		}
		return 0.0f;
	}

	FReal FPBDJointUtilities::GetLinearProjection(
		const FPBDJointSolverSettings& SolverSettings,
		const FPBDJointSettings& JointSettings)
	{
		return (SolverSettings.LinearProjection > 0.0f) ? SolverSettings.LinearProjection : JointSettings.LinearProjection;
	}

	FReal FPBDJointUtilities::GetAngularProjection(
		const FPBDJointSolverSettings& SolverSettings,
		const FPBDJointSettings& JointSettings)
	{
		return (SolverSettings.AngularProjection > 0.0f) ? SolverSettings.AngularProjection : JointSettings.AngularProjection;
	}

	bool FPBDJointUtilities::GetLinearSoftAccelerationMode(
		const FPBDJointSolverSettings& SolverSettings,
		const FPBDJointSettings& JointSettings)
	{
		return JointSettings.LinearSoftForceMode == EJointForceMode::Acceleration;
	}

	bool FPBDJointUtilities::GetAngularSoftAccelerationMode(
		const FPBDJointSolverSettings& SolverSettings,
		const FPBDJointSettings& JointSettings)
	{
		return JointSettings.AngularSoftForceMode == EJointForceMode::Acceleration;
	}

	bool FPBDJointUtilities::GetDriveAccelerationMode(
		const FPBDJointSolverSettings& SolverSettings,
		const FPBDJointSettings& JointSettings)
	{
		return JointSettings.AngularDriveForceMode == EJointForceMode::Acceleration;
	}

	FReal FPBDJointUtilities::GetAngularPositionCorrection(
		const FPBDJointSolverSettings& SolverSettings,
		const FPBDJointSettings& JointSettings)
	{
		// Disable the angular limit hardness improvement if linear limits are set up
		// @todo(ccaulfield): fix angular constraint position correction in ApplyRotationCorrection and ApplyRotationCorrectionSoft
		bool bPositionCorrectionEnabled = ((JointSettings.LinearMotionTypes[0] == EJointMotionType::Locked) && (JointSettings.LinearMotionTypes[1] == EJointMotionType::Locked) && (JointSettings.LinearMotionTypes[2] == EJointMotionType::Locked));
		if (bPositionCorrectionEnabled)
		{
			return SolverSettings.AngularConstraintPositionCorrection;
		}
		return 0.0f;
	}


	FVec3 FPBDJointUtilities::ConditionInertia(const FVec3& InI, const FReal MaxRatio)
	{
		FReal IMin = InI.Min();
		FReal IMax = InI.Max();
		if ((MaxRatio > 0) && (IMin > 0))
		{
			FReal Ratio = IMax / IMin;
			if (Ratio > MaxRatio)
			{
				FReal MinIMin = IMax / MaxRatio;
				return FVec3(
					FMath::Lerp(MinIMin, IMax, (InI.X - IMin) / (IMax - IMin)),
					FMath::Lerp(MinIMin, IMax, (InI.Y - IMin) / (IMax - IMin)),
					FMath::Lerp(MinIMin, IMax, (InI.Z - IMin) / (IMax - IMin)));
			}
		}
		return InI;
	}

	
	FVec3 FPBDJointUtilities::ConditionParentInertia(const FVec3& IParent, const FVec3& IChild, const FReal MinRatio)
	{
		if (MinRatio > 0)
		{
			FReal IParentMax = IParent.Max();
			FReal IChildMax = IChild.Max();
			if ((IParentMax > 0) && (IChildMax > 0))
			{
				FReal Ratio = IParentMax / IChildMax;
				if (Ratio < MinRatio)
				{
					FReal Multiplier = MinRatio / Ratio;
					return IParent * Multiplier;
				}
			}
		}
		return IParent;
	}

	
	FReal FPBDJointUtilities::ConditionParentMass(const FReal MParent, const FReal MChild, const FReal MinRatio)
	{
		if ((MinRatio > 0) && (MParent > 0) && (MChild > 0))
		{
			FReal Ratio = MParent / MChild;
			if (Ratio < MinRatio)
			{
				FReal Multiplier = MinRatio / Ratio;
				return MParent * Multiplier;
			}
		}
		return MParent;
	}

	
	// @todo(ccaulfield): should also take into account the length of the joint connector to prevent over-rotation
	void FPBDJointUtilities::ConditionInverseMassAndInertia(
		FReal& InOutInvMParent,
		FReal& InOutInvMChild,
		FVec3& InOutInvIParent,
		FVec3& InOutInvIChild,
		const FReal MinParentMassRatio,
		const FReal MaxInertiaRatio)
	{
		FReal MParent = 0.0f;
		FVec3 IParent = FVec3(0);
		FReal MChild = 0.0f;
		FVec3 IChild = FVec3(0);

		// Set up inertia so that it is more uniform (reduce the maximum ratio of the inertia about each axis)
		if (InOutInvMParent > 0)
		{
			MParent = 1.0f / InOutInvMParent;
			IParent = ConditionInertia(FVec3(1.0f / InOutInvIParent.X, 1.0f / InOutInvIParent.Y, 1.0f / InOutInvIParent.Z), MaxInertiaRatio);
		}
		if (InOutInvMChild > 0)
		{
			MChild = 1.0f / InOutInvMChild;
			IChild = ConditionInertia(FVec3(1.0f / InOutInvIChild.X, 1.0f / InOutInvIChild.Y, 1.0f / InOutInvIChild.Z), MaxInertiaRatio);
		}

		// Set up relative mass and inertia so that the parent cannot be much lighter than the child
		if ((InOutInvMParent > 0) && (InOutInvMChild > 0))
		{
			MParent = ConditionParentMass(MParent, MChild, MinParentMassRatio);
			IParent = ConditionParentInertia(IParent, IChild, MinParentMassRatio);
		}

		// Map back to inverses
		if (InOutInvMParent > 0)
		{
			InOutInvMParent = (FReal)1 / MParent;
			InOutInvIParent = FVec3((FReal)1 / IParent.X, (FReal)1 / IParent.Y, (FReal)1 / IParent.Z);
		}
		if (InOutInvMChild > 0)
		{
			InOutInvMChild = (FReal)1 / MChild;
			InOutInvIChild = FVec3((FReal)1 / IChild.X, (FReal)1 / IChild.Y, (FReal)1 / IChild.Z);
		}
	}


	FVec3 FPBDJointUtilities::GetSphereLimitedPositionError(const FVec3& CX, const FReal Radius)
	{
		FReal CXLen = CX.Size();
		if (CXLen < Radius)
		{
			return FVec3(0, 0, 0);
		}
		else if (CXLen > SMALL_NUMBER)
		{
			FVec3 Dir = CX / CXLen;
			return CX - Radius * Dir;
		}
		return CX;
	}


	FVec3 FPBDJointUtilities::GetCylinderLimitedPositionError(const FVec3& InCX, const FVec3& Axis, const FReal Limit, const EJointMotionType AxisMotion)
	{
		FVec3 CXAxis = FVec3::DotProduct(InCX, Axis) * Axis;
		FVec3 CXPlane = InCX - CXAxis;
		FReal CXPlaneLen = CXPlane.Size();
		if (AxisMotion == EJointMotionType::Free)
		{
			CXAxis = FVec3(0, 0, 0);
		}
		if (CXPlaneLen < Limit)
		{
			CXPlane = FVec3(0, 0, 0);
		}
		else if (CXPlaneLen > KINDA_SMALL_NUMBER)
		{
			FVec3 Dir = CXPlane / CXPlaneLen;
			CXPlane = CXPlane - Limit * Dir;
		}
		return CXAxis + CXPlane;
	}


	FVec3 FPBDJointUtilities::GetLineLimitedPositionError(const FVec3& CX, const FVec3& Axis, const FReal Limit, const EJointMotionType AxisMotion)
	{
		FReal CXDist = FVec3::DotProduct(CX, Axis);
		if ((AxisMotion == EJointMotionType::Free) || (FMath::Abs(CXDist) < Limit))
		{
			return CX - CXDist * Axis;
		}
		else if (CXDist >= Limit)
		{
			return CX - Limit * Axis;
		}
		else
		{
			return CX + Limit * Axis;
		}
	}


	FVec3 FPBDJointUtilities::GetLimitedPositionError(const FPBDJointSettings& JointSettings, const FRotation3& R0, const FVec3& InCX)
	{
		// This function is only used for projection and is only relevant for hard limits.
		// Treat soft-limits as free for error calculation.
		const TVector<EJointMotionType, 3>& Motion =
		{
			((JointSettings.LinearMotionTypes[0] == EJointMotionType::Limited) && JointSettings.bSoftLinearLimitsEnabled) ? EJointMotionType::Free : JointSettings.LinearMotionTypes[0],
			((JointSettings.LinearMotionTypes[1] == EJointMotionType::Limited) && JointSettings.bSoftLinearLimitsEnabled) ? EJointMotionType::Free : JointSettings.LinearMotionTypes[1],
			((JointSettings.LinearMotionTypes[2] == EJointMotionType::Limited) && JointSettings.bSoftLinearLimitsEnabled) ? EJointMotionType::Free : JointSettings.LinearMotionTypes[2],
		};

		if ((Motion[0] == EJointMotionType::Locked) && (Motion[1] == EJointMotionType::Locked) && (Motion[2] == EJointMotionType::Locked))
		{
			return InCX;
		}
		else if ((Motion[0] == EJointMotionType::Limited) && (Motion[1] == EJointMotionType::Limited) && (Motion[2] == EJointMotionType::Limited))
		{
			// Spherical distance constraints
			return GetSphereLimitedPositionError(InCX, JointSettings.LinearLimit);
		}
		else if ((Motion[1] == EJointMotionType::Limited) && (Motion[2] == EJointMotionType::Limited))
		{
			// Circular Limit (X Axis)
			FVec3 Axis = R0 * FVec3(1, 0, 0);
			return GetCylinderLimitedPositionError(InCX, Axis, JointSettings.LinearLimit, Motion[0]);
		}
		else if ((Motion[0] == EJointMotionType::Limited) && (Motion[2] == EJointMotionType::Limited))
		{
			// Circular Limit (Y Axis)
			FVec3 Axis = R0 * FVec3(0, 1, 0);
			return GetCylinderLimitedPositionError(InCX, Axis, JointSettings.LinearLimit, Motion[1]);
		}
		else if ((Motion[0] == EJointMotionType::Limited) && (Motion[1] == EJointMotionType::Limited))
		{
			// Circular Limit (Z Axis)
			FVec3 Axis = R0 * FVec3(0, 0, 1);
			return GetCylinderLimitedPositionError(InCX, Axis, JointSettings.LinearLimit, Motion[2]);
		}
		else
		{
			// Line/Square/Cube Limits (no way to author square or cube limits, but would work if we wanted it)
			FVec3 CX = InCX;
			if (Motion[0] != EJointMotionType::Locked)
			{
				FVec3 Axis = R0 * FVec3(1, 0, 0);
				CX = GetLineLimitedPositionError(CX, Axis, JointSettings.LinearLimit, Motion[0]);
			}
			if (Motion[1] != EJointMotionType::Locked)
			{
				FVec3 Axis = R0 * FVec3(0, 1, 0);
				CX = GetLineLimitedPositionError(CX, Axis, JointSettings.LinearLimit, Motion[1]);
			}
			if (Motion[2] != EJointMotionType::Locked)
			{
				FVec3 Axis = R0 * FVec3(0, 0, 1);
				CX = GetLineLimitedPositionError(CX, Axis, JointSettings.LinearLimit, Motion[2]);
			}
			return CX;
		}
	}
}