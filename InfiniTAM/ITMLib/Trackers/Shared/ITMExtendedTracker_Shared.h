// Copyright 2014-2015 Isis Innovation Limited and the authors of InfiniTAM

#pragma once

#include "../../Utils/ITMPixelUtils.h"

// huber norm
_CPU_AND_GPU_CODE_ inline float rho(float r, float huber_b)
{
	float tmp = fabs(r) - huber_b;
	tmp = MAX(tmp, 0.0f);
	return r*r - tmp*tmp;
}

_CPU_AND_GPU_CODE_ inline float rho_deriv(float r, float huber_b)
{
	return 2.0f * CLAMP(r, -huber_b, huber_b);
}

_CPU_AND_GPU_CODE_ inline float rho_deriv2(float r, float huber_b)
{
	if (fabs(r) < huber_b) return 2.0f;
	return 0.0f;
}

_CPU_AND_GPU_CODE_ inline float rho_v3f(const Vector3f &r, float huber_b)
{
	Vector3f tmp(fabs(r.x) - huber_b, fabs(r.y) - huber_b, fabs(r.z) - huber_b);
	tmp.x = MAX(tmp.x, 0.0f);
	tmp.y = MAX(tmp.y, 0.0f);
	tmp.z = MAX(tmp.z, 0.0f);

	return (r.x*r.x - tmp.x*tmp.x) + (r.y*r.y - tmp.y*tmp.y) + (r.z*r.z - tmp.z*tmp.z);
}

_CPU_AND_GPU_CODE_ inline Vector3f rho_deriv_v3f(const Vector3f &r, float huber_b)
{
	return 2.0f * Vector3f(CLAMP(r.x, -huber_b, huber_b),
						   CLAMP(r.y, -huber_b, huber_b),
						   CLAMP(r.z, -huber_b, huber_b));
}

_CPU_AND_GPU_CODE_ inline Vector3f rho_deriv2_v3f(const Vector3f &r, float huber_b)
{
	Vector3f tmp;
	tmp.x = (fabs(r.x) < huber_b) ? 2.f : 0.f;
	tmp.y = (fabs(r.y) < huber_b) ? 2.f : 0.f;
	tmp.z = (fabs(r.z) < huber_b) ? 2.f : 0.f;
	return tmp;
}

template<bool shortIteration, bool rotationOnly, bool useWeights>
_CPU_AND_GPU_CODE_ inline bool computePerPointGH_exDepth_Ab(THREADPTR(float) *A, THREADPTR(float) &b,
	const THREADPTR(int) & x, const THREADPTR(int) & y, const CONSTPTR(float) &depth, THREADPTR(float) &depthWeight,
	const CONSTPTR(Vector2i) & viewImageSize, const CONSTPTR(Vector4f) & viewIntrinsics, const CONSTPTR(Vector2i) & sceneImageSize,
	const CONSTPTR(Vector4f) & sceneIntrinsics, const CONSTPTR(Matrix4f) & approxInvPose, const CONSTPTR(Matrix4f) & scenePose, const CONSTPTR(Vector4f) *pointsMap,
	const CONSTPTR(Vector4f) *normalsMap, float spaceThresh, float viewFrustum_min, float viewFrustum_max, int tukeyCutOff, int framesToSkip, int framesToWeight)
{
	depthWeight = 0;

	if (depth <= 1e-8f) return false; //check if valid -- != 0.0f

	Vector4f tmp3Dpoint, tmp3Dpoint_reproj; Vector3f ptDiff;
	Vector4f curr3Dpoint, corr3Dnormal; Vector2f tmp2Dpoint;

	tmp3Dpoint.x = depth * ((float(x) - viewIntrinsics.z) / viewIntrinsics.x);
	tmp3Dpoint.y = depth * ((float(y) - viewIntrinsics.w) / viewIntrinsics.y);
	tmp3Dpoint.z = depth;
	tmp3Dpoint.w = 1.0f;

	// transform to previous frame coordinates
	tmp3Dpoint = approxInvPose * tmp3Dpoint;
	tmp3Dpoint.w = 1.0f;

	// project into previous rendered image
	tmp3Dpoint_reproj = scenePose * tmp3Dpoint;
	if (tmp3Dpoint_reproj.z <= 0.0f) return false;
	tmp2Dpoint.x = sceneIntrinsics.x * tmp3Dpoint_reproj.x / tmp3Dpoint_reproj.z + sceneIntrinsics.z;
	tmp2Dpoint.y = sceneIntrinsics.y * tmp3Dpoint_reproj.y / tmp3Dpoint_reproj.z + sceneIntrinsics.w;

	if (!((tmp2Dpoint.x >= 0.0f) && (tmp2Dpoint.x <= sceneImageSize.x - 2) && (tmp2Dpoint.y >= 0.0f) && (tmp2Dpoint.y <= sceneImageSize.y - 2)))
		return false;

	curr3Dpoint = interpolateBilinear_withHoles(pointsMap, tmp2Dpoint, sceneImageSize);
	if (curr3Dpoint.w < 0.0f) return false;

	ptDiff.x = curr3Dpoint.x - tmp3Dpoint.x;
	ptDiff.y = curr3Dpoint.y - tmp3Dpoint.y;
	ptDiff.z = curr3Dpoint.z - tmp3Dpoint.z;
	float dist = ptDiff.x * ptDiff.x + ptDiff.y * ptDiff.y + ptDiff.z * ptDiff.z;

	if (dist > tukeyCutOff * spaceThresh) return false;

	corr3Dnormal = interpolateBilinear_withHoles(normalsMap, tmp2Dpoint, sceneImageSize);
	//if (corr3Dnormal.w < 0.0f) return false;

	depthWeight = MAX(0.0f, 1.0f - (depth - viewFrustum_min) / (viewFrustum_max - viewFrustum_min));
	depthWeight *= depthWeight;

	if (useWeights)
	{
		if (curr3Dpoint.w < framesToSkip) return false;
		depthWeight *= (curr3Dpoint.w - framesToSkip) / framesToWeight;
	}

	b = corr3Dnormal.x * ptDiff.x + corr3Dnormal.y * ptDiff.y + corr3Dnormal.z * ptDiff.z;

	// TODO check whether normal matches normal from image, done in the original paper, but does not seem to be required
	if (shortIteration)
	{
		if (rotationOnly)
		{
			A[0] = +tmp3Dpoint.z * corr3Dnormal.y - tmp3Dpoint.y * corr3Dnormal.z;
			A[1] = -tmp3Dpoint.z * corr3Dnormal.x + tmp3Dpoint.x * corr3Dnormal.z;
			A[2] = +tmp3Dpoint.y * corr3Dnormal.x - tmp3Dpoint.x * corr3Dnormal.y;
		}
		else { A[0] = corr3Dnormal.x; A[1] = corr3Dnormal.y; A[2] = corr3Dnormal.z; }
	}
	else
	{
		A[0] = +tmp3Dpoint.z * corr3Dnormal.y - tmp3Dpoint.y * corr3Dnormal.z;
		A[1] = -tmp3Dpoint.z * corr3Dnormal.x + tmp3Dpoint.x * corr3Dnormal.z;
		A[2] = +tmp3Dpoint.y * corr3Dnormal.x - tmp3Dpoint.x * corr3Dnormal.y;
		A[!shortIteration ? 3 : 0] = corr3Dnormal.x; A[!shortIteration ? 4 : 1] = corr3Dnormal.y; A[!shortIteration ? 5 : 2] = corr3Dnormal.z;
	}

	return true;
}

template<bool useWeights>
_CPU_AND_GPU_CODE_ inline bool computePerPointGH_exRGB_Ab(THREADPTR(float) *localGradient, THREADPTR(float) &colourDifferenceSq, THREADPTR(float) *localHessian,
	THREADPTR(float) &depthWeight, const THREADPTR(Vector4f) &pt_world,  THREADPTR(Vector4f) colour_world, const DEVICEPTR(Vector4u) *rgb_live,
	const CONSTPTR(Vector2i) &imgSize, int x, int y, const Vector4f &projParams, const Matrix4f &approxPose, const Matrix4f &approxInvPose,
	const Matrix4f &scenePose, const DEVICEPTR(Vector4s) *gx, const DEVICEPTR(Vector4s) *gy, float colourThresh, float viewFrustum_min, float viewFrustum_max,
	float tukeyCutoff, float framesToSkip, float framesToWeight, int numPara)
{
	if (pt_world.w <= 1e-3f || colour_world.w <= 1e-3f) return false;
	if (useWeights && pt_world.w < framesToSkip) return false;

	// convert the point in camera coordinates using the candidate pose
	Vector4f pt_camera = pt_world;
	pt_camera.w = 1.f; // Coerce it to be a point
	pt_camera = approxPose * pt_camera;

	if (pt_camera.z <= viewFrustum_min || pt_camera.z >= viewFrustum_max) return false;

	depthWeight = 1.0f - (pt_camera.z - viewFrustum_min) / (viewFrustum_max - viewFrustum_min); // Evaluate outside of the macro
	depthWeight = MAX(depthWeight, 0.f);
	depthWeight *= depthWeight;

	if (useWeights)
	{
		depthWeight *= (pt_world.w - framesToSkip) / framesToWeight;
	}

	const float fx = projParams.x;
	const float fy = projParams.y;
	const float cx = projParams.z;
	const float cy = projParams.w;
	const float inv_cam_z = 1.f / pt_camera.z;
	const float inv_cam_z_sq = inv_cam_z * inv_cam_z;

	// project the point onto the live image
	const Vector2f pt_image_live(fx * pt_camera.x * inv_cam_z + cx,
								 fy * pt_camera.y * inv_cam_z + cy);

	if (pt_image_live.x < 0 || pt_image_live.x >= imgSize.x - 1 ||
		pt_image_live.y < 0 || pt_image_live.y >= imgSize.y - 1) return false;

	const Vector4f colour_obs = interpolateBilinear(rgb_live, pt_image_live, imgSize) / 255.f;
	const Vector3f gx_obs = interpolateBilinear(gx, pt_image_live, imgSize).toVector3() / 255.f; // gx and gy are computed from the live image
	const Vector3f gy_obs = interpolateBilinear(gy, pt_image_live, imgSize).toVector3() / 255.f;

	if (colour_obs.w <= 1e-3f) return false;
	if (dot(gx_obs, gx_obs) < 1e-5 || dot(gy_obs, gy_obs) < 1e-5) return false;

	const Vector3f colour_diff_d(colour_obs.x - colour_world.x,
								 colour_obs.y - colour_world.y,
								 colour_obs.z - colour_world.z);

	if (fabs(colour_diff_d.x) > tukeyCutoff * colourThresh &&
		fabs(colour_diff_d.y) > tukeyCutoff * colourThresh &&
		fabs(colour_diff_d.z) > tukeyCutoff * colourThresh) return false;

	// Derivatives computed as in
	// Blanco, J. (2010). A tutorial on se (3) transformation parameterizations and on-manifold optimization.
	// University of Malaga, Tech. Rep
	// Equation A.13

	const Vector3f rot_row_0 = approxInvPose.getRow(0).toVector3();
	const Vector3f rot_row_1 = approxInvPose.getRow(1).toVector3();
	const Vector3f rot_row_2 = approxInvPose.getRow(2).toVector3();

	// Derivatives of the projection operation
	const Vector3f d_proj_x(fx * inv_cam_z,
							0.f,
							-fx * pt_camera.x * inv_cam_z_sq);

	const Vector3f d_proj_y(0.f,
							fy * inv_cam_z,
							-fy * pt_camera.y * inv_cam_z_sq);

	Vector3f A[6];

	for (int para = 0; para < numPara; para++)
	{
		// Derivatives wrt. the current parameter
		Vector3f d_point_col;

		switch (para)
		{
		case 0: //rx
			d_point_col = rot_row_1 * pt_world.z - rot_row_2 * pt_world.y;
			break;
		case 1: // ry
			d_point_col = rot_row_2 * pt_world.x - rot_row_0 * pt_world.z;
			break;
		case 2: // rz
			d_point_col = rot_row_0 * pt_world.y - rot_row_1 * pt_world.x;
			break; //rz
		case 3: //tx
			// Rotation matrix negated and transposed (matrix storage is column major, though)
			// We negate it one more time (-> no negation) because the ApplyDelta uses the KinectFusion
			// skew symmetric matrix, that matrix has negated rotation components.
			// In order to use the rgb tracker we would need to negate the entire computed step, but given
			// the peculiar structure of the increment matrix we only need to negate the translation component.
			d_point_col = rot_row_0;
			break;
		case 4: //ty
			d_point_col = rot_row_1;
			break;
		case 5: //tz
			d_point_col = rot_row_2;
			break;
		};

		// Chain rule projection-pose
		Vector2f d_proj_dpi;
		d_proj_dpi.x = dot(d_proj_x, d_point_col);
		d_proj_dpi.y = dot(d_proj_y, d_point_col);

		// Chain rule image gradient-projection-pose
		A[para].x = d_proj_dpi.x * gx_obs.x + d_proj_dpi.y * gy_obs.x;
		A[para].y = d_proj_dpi.x * gx_obs.y + d_proj_dpi.y * gy_obs.y;
		A[para].z = d_proj_dpi.x * gx_obs.z + d_proj_dpi.y * gy_obs.z;
	}

	const Vector3f huber_coef_gradient = rho_deriv_v3f(colour_diff_d, colourThresh);
	const Vector3f huber_coef_hessian = rho_deriv2_v3f(colour_diff_d, colourThresh);

	for (int para = 0, counter = 0; para < numPara; para++)
	{
		// Equivalent of Ab
		localGradient[para] = depthWeight * dot(huber_coef_gradient, A[para]);

		// compute triangular part of A'A
		const Vector3f weighted_A_para(huber_coef_hessian.x * A[para].x,
									   huber_coef_hessian.y * A[para].y,
									   huber_coef_hessian.z * A[para].z);
		for (int col = 0; col <= para; col++)
		{
			// dot(A[para],A[col]) but with huber weighting
			localHessian[counter++] = depthWeight * depthWeight * dot(weighted_A_para, A[col]);
		}
	}

	// compute b
	colourDifferenceSq = depthWeight * rho_v3f(colour_diff_d, colourThresh);

	return true;
}

template<bool shortIteration, bool rotationOnly, bool useWeights>
_CPU_AND_GPU_CODE_ inline bool computePerPointGH_exDepth(THREADPTR(float) *localNabla, THREADPTR(float) *localHessian, THREADPTR(float) &localF,
	const THREADPTR(int) & x, const THREADPTR(int) & y, const CONSTPTR(float) &depth, THREADPTR(float) &depthWeight, CONSTPTR(Vector2i) & viewImageSize, const CONSTPTR(Vector4f) & viewIntrinsics,
	const CONSTPTR(Vector2i) & sceneImageSize, const CONSTPTR(Vector4f) & sceneIntrinsics, const CONSTPTR(Matrix4f) & approxInvPose, const CONSTPTR(Matrix4f) & scenePose,
	const CONSTPTR(Vector4f) *pointsMap, const CONSTPTR(Vector4f) *normalsMap, float spaceThreash, float viewFrustum_min, float viewFrustum_max, int tukeyCutOff, int framesToSkip, int framesToWeight)
{
	const int noPara = shortIteration ? 3 : 6;
	float A[noPara];
	float b;

	bool ret = computePerPointGH_exDepth_Ab<shortIteration, rotationOnly, useWeights>(A, b, x, y, depth, depthWeight, viewImageSize, viewIntrinsics, sceneImageSize, sceneIntrinsics,
		approxInvPose, scenePose, pointsMap, normalsMap, spaceThresh, viewFrustum_min, viewFrustum_max, tukeyCutOff, framesToSkip, framesToWeight);

	if (!ret) return false;

	localF = rho(b, spaceThresh) * depthWeight;

#if (defined(__CUDACC__) && defined(__CUDA_ARCH__)) || (defined(__METALC__))
#pragma unroll
#endif
	for (int r = 0, counter = 0; r < noPara; r++)
	{
		localNabla[r] = rho_deriv(b, spaceThresh) * depthWeight * A[r];
#if (defined(__CUDACC__) && defined(__CUDA_ARCH__)) || (defined(__METALC__))
#pragma unroll
#endif
		for (int c = 0; c <= r; c++, counter++) localHessian[counter] = rho_deriv2(b, spaceThresh) * depthWeight * A[r] * A[c];
	}

	return true;
}

_CPU_AND_GPU_CODE_ inline bool computePerPointProjectedColour_exRGB(THREADPTR(int) x, THREADPTR(int) y, DEVICEPTR(Vector4f) *out_rgb, const DEVICEPTR(Vector4u) *in_rgb, const DEVICEPTR(Vector4f) *in_points, const CONSTPTR(Vector2i) &imageSize, const CONSTPTR(int) sceneIdx, const CONSTPTR(Vector4f) &intrinsics, const CONSTPTR(Matrix4f) &scenePose)
{
	Vector4f pt_world = in_points[sceneIdx];

	// Invalid point
	if (pt_world.w <= 1e-3f) return false;

	pt_world.w = 1.f; // Coerce it to be a point
	Vector4f pt_image = scenePose * pt_world;

	// Point behind the camera
	if(pt_image.z <= 0.f) return false;

	Vector2f pt_image_proj;
	// Project the point onto the previous frame
	pt_image_proj.x = intrinsics.x * pt_image.x / pt_image.z + intrinsics.z;
	pt_image_proj.y = intrinsics.y * pt_image.y / pt_image.z + intrinsics.w;

	// Projection outside the previous rgb frame
	if (pt_image_proj.x < 0 || pt_image_proj.x >= imageSize.x - 1 ||
		pt_image_proj.y < 0 || pt_image_proj.y >= imageSize.y - 1) return false;

	out_rgb[sceneIdx] = interpolateBilinear(in_rgb, pt_image_proj, imageSize) / 255.f;
	return true;
}

_CPU_AND_GPU_CODE_ inline void projectPreviousPoint_exRGB(THREADPTR(int) x, THREADPTR(int) y, DEVICEPTR(Vector4f) *out_rgb, const DEVICEPTR(Vector4u) *in_rgb, const DEVICEPTR(Vector4f) *in_points, const CONSTPTR(Vector2i) &imageSize, const CONSTPTR(Vector2i) &sceneSize, const CONSTPTR(Vector4f) &intrinsics, const CONSTPTR(Matrix4f) &scenePose)
{
	if (x >= sceneSize.x || y >= sceneSize.y) return;

	int sceneIdx = y * sceneSize.x + x;

	if (!computePerPointProjectedColour_exRGB(x, y, out_rgb, in_rgb, in_points,
		 imageSize, sceneIdx, intrinsics, scenePose))
	{
		out_rgb[sceneIdx] = Vector4f(0.f, 0.f, 0.f, -1.f);
	}
}
