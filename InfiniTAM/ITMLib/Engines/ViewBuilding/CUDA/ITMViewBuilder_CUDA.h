// Copyright 2014-2017 Oxford University Innovation Limited and the authors of InfiniTAM

#pragma once

#include "../Interface/ITMViewBuilder.h"

namespace ITMLib
{
	class ITMViewBuilder_CUDA : public ITMViewBuilder
	{
	public:
		void ConvertDisparityToDepth(ITMFloatImage *depth_out, const ITMShortImage *depth_in, const ITMIntrinsics *depthIntrinsics, 
			Vector2f disparityCalibParams);
		void ConvertDepthAffineToFloat(ITMFloatImage *depth_out, const ITMShortImage *depth_in, Vector2f depthCalibParams);

		void DepthFiltering(ITMFloatImage *image_out, const ITMFloatImage *image_in);
		void zr300_depth_denoise_piece(ITMFloatImage *image_out, const ITMFloatImage *image_in);
		void zr300_depth_denoise_point(ITMFloatImage *image_out, const ITMFloatImage *image_in);
		void BilateralFiltering(ITMFloatImage *image_out, const ITMFloatImage *image_in);

		void depth_Align2_color(ITMFloatImage *depth_out, const ITMFloatImage *depth_in, const ITMExtrinsics *depthExtrinsics, const ITMIntrinsics *depthIntrinsics, const ITMIntrinsics *colorIntrinsics);

		void ComputeNormalAndWeights(ITMFloat4Image *normal_out, ITMFloatImage *sigmaZ_out, const ITMFloatImage *depth_in, Vector4f intrinsic);

		void UpdateView(ITMView **view, ITMUChar4Image *rgbImage, ITMShortImage *rawDepthImage, bool useBilateralFilter, bool modelSensorNoise = false, bool storePreviousImage = true);
		void UpdateView(ITMView **view, ITMUChar4Image *rgbImage, ITMShortImage *depthImage, bool useBilateralFilter, ITMIMUMeasurement *imuMeasurement, bool modelSensorNoise = false, bool storePreviousImage = true);
		void UpdateView(ITMView **view, ITMUChar4Image *rgbImage, ITMShortImage *rawDepthImage, bool useBilateralFilter, cv::Mat *grayimg, std::vector<DataReader::IMUData> *relatedIMU, double imgtime, bool modelSensorNoise = false, bool storePreviousImage = true);

		ITMViewBuilder_CUDA(const ITMRGBDCalib& calib);
		~ITMViewBuilder_CUDA(void);
	};
}
