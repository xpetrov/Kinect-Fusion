#include <iostream>

#include <Eigen/Core>

#include "Raytracer.hpp"

bool searchRay(VoxelGrid& voxelGrid, Eigen::Vector3d origin, Eigen::Vector3d ray, double& length,
               const double stepSizeVoxel, const double epsilon)
{
	Eigen::Vector3d point = origin + ray*length;
	float pointValue = voxelGrid.getValueAtPoint(point);
	float previousPointValue = pointValue;

	double stepSize = voxelGrid.voxelSize * stepSizeVoxel;
	double previousLength = length;

	while (true)
	{
		previousLength = length;
		length += stepSize;
		point = origin + ray*length;

		if (not voxelGrid.withinGrid(point)) { return false; }

		previousPointValue = pointValue;
		pointValue = voxelGrid.getValueAtPoint(point);

		if (previousPointValue > 0.0 and pointValue < 0.0) { break; }
	}

	while(true)
	{
		double middleLength = (previousLength + length)/2;
		float middleValue = voxelGrid.getValueAtPoint(origin + ray*middleLength);

		if (middleValue > epsilon)
		{
			previousLength = middleLength;
		}
		else if (middleValue < -epsilon)
		{
			length = middleLength;
		}
		else
		{
			break;
		}

		if (std::abs(length - previousLength) < 1e-2)
		{
			break;
		}
	}

	return true;
}


void raytraceImage(VoxelGrid& voxelGrid, Pose cameraPose, Eigen::Matrix3d cameraIntrisic,
                      const unsigned int resolutionWidth, const unsigned int resolutionHeight,
                      const double stepSizeVoxel, const double epsilon,
                      cv::Mat& depthImage, cv::Mat& normalImage)
{
	depthImage.setTo(cv::Scalar(-std::numeric_limits<float>::infinity()));
	normalImage.setTo(cv::Vec3f(-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()));

	double fx = cameraIntrisic(0, 0)*resolutionWidth;
	double fy = cameraIntrisic(1, 1)*resolutionHeight;
	double cx = cameraIntrisic(0, 2)*resolutionWidth - 0.5;
	double cy = cameraIntrisic(1, 2)*resolutionHeight - 0.5;

	Eigen::Vector3d origin = cameraPose.translation;

	for (int v = 0; v < resolutionHeight; ++v)
	{
		for (int u = 0; u < resolutionWidth; ++u)
		{
			double rayX = ((double)u - cx)/fx;
			double rayY = ((double)v - cy)/fy;
			Eigen::Vector3d ray(rayX, rayY, 1);
			//ray.normalize();

			ray = cameraPose.transformVector(ray);
			cv::Vec3f normal;
			double length;

			if (voxelGrid.projectRayToVoxelPoint(origin, ray, length) and // Does the ray hit the voxel grid
			    searchRay(voxelGrid, origin, ray, length, stepSizeVoxel, epsilon)) // Does the ray hit a zero crossing
			{
				depthImage.at<float>(v, u) = (float)length;

                if(depthImage.at<float>(v, u) == 0.0f)
                    std::cout << "Invalid depth value at: (" << v <<" , " << u <<") will exit now!\n";
               assert(depthImage.at<float>(v, u) != 0.0f);


				Eigen::Vector3d point = origin + ray*length;

				const double voxelSize = voxelGrid.voxelSize;

				float valueXForward = voxelGrid.getValueAtPoint(point + Eigen::Vector3d(voxelSize, 0, 0));
				float valueXBackward = voxelGrid.getValueAtPoint(point + Eigen::Vector3d(-voxelSize, 0, 0));

				float valueYForward = voxelGrid.getValueAtPoint(point + Eigen::Vector3d(0, voxelSize, 0));
				float valueYBackward = voxelGrid.getValueAtPoint(point + Eigen::Vector3d(0, -voxelSize, 0));

				float valueZForward = voxelGrid.getValueAtPoint(point + Eigen::Vector3d(0, 0, voxelSize));
				float valueZBackward = voxelGrid.getValueAtPoint(point + Eigen::Vector3d(0, 0, -voxelSize));

				Eigen::Vector3d normalVec(
						(valueXForward - valueXBackward)/2,
						(valueYForward - valueYBackward)/2,
						(valueZForward - valueZBackward)/2
				);
				normalVec = cameraPose.orientation.transpose()*normalVec;
				normalVec.normalize();

				normal(0) = (float)normalVec.x();
				normal(1) = (float)normalVec.y();
				normal(2) = (float)normalVec.z();

				normalImage.at<cv::Vec3f>(v, u) = normal;
			}
			else
			{
				depthImage.at<float>(v, u) = -std::numeric_limits<float>::infinity();
				//depthImage.at<float>(v, u) = 0.0f;
				//normalImage.at<cv::Vec3f>(v, u) = normal;
                normalImage.at<cv::Vec3f>(v, u) = cv::Vec3f(-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity());
            }
		}
	}
}
