/*=========================================================================
 *
 *  Copyright Insight Software Consortium
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0.txt
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *=========================================================================*/
#include <vector>
#include <unordered_map>

#include "itkConnectedThresholdImageFilter.h"
#include "itkImage.h"
#include "itkCastImageFilter.h"
#include "itkRGBPixel.h"
#include "itkCurvatureFlowImageFilter.h"
#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"

typedef   float           InternalPixelType;
const     unsigned int    Dimension = 3;
typedef itk::Image< InternalPixelType, Dimension >  InternalImageType;
typedef unsigned char                            OutputPixelType;
typedef itk::Image< OutputPixelType, Dimension > OutputImageType;
typedef itk::CastImageFilter< InternalImageType, OutputImageType >
                                                 CastingFilterType;

// Compute statistics variables.
void ComputeStats(InternalImageType::Pointer& image,
                  std::unordered_map<long long, InternalImageType::IndexType>& region_points,
                  long long* mgv, long long* upper_dev, long long* lower_dev) {
  long long sum = 0;
  // Computing mean for all points
  for (const auto& kv : region_points) {
    sum += image->GetPixel(kv.second);
  }
  *mgv = sum / region_points.size();

  // Computing mean for upper and lower halves.
  long long upper_sum = 0, lower_sum = 0;
  long long upper_num = 0, lower_num = 0;
  for (const auto& kv : region_points) {
    if (image->GetPixel(kv.second) >= *mgv) {
      upper_sum += image->GetPixel(kv.second);
      upper_num++;
    }
    if (image->GetPixel(kv.second) <= *mgv) {
      lower_sum += image->GetPixel(kv.second);
      lower_num++;
    }
  }
  long long upper_mean = upper_sum / upper_num;
  long long lower_mean = lower_sum / lower_num;

  // Computing standard deviation for upper and lower halves
  *upper_dev = 0;
  *lower_dev = 0;
  for (const auto& kv : region_points) {
    if (image->GetPixel(kv.second) >= *mgv) {
      *upper_dev += (image->GetPixel(kv.second) - upper_mean) * (image->GetPixel(kv.second) - upper_mean);
    }
    if (image->GetPixel(kv.second) <= *mgv) {
      *lower_dev += (image->GetPixel(kv.second) - lower_mean) * (image->GetPixel(kv.second) - lower_mean);
    }
  }

  *upper_dev /= upper_num;
  *lower_dev /= lower_num;

  *upper_dev = sqrt(*upper_dev);
  *lower_dev = sqrt(*lower_dev);
}

// Compute thresholds for the first run.
void ComputeThreshold(InternalImageType::Pointer& image,
                      std::unordered_map<long long, InternalImageType::IndexType>& region_points,
                      long long* upper_threshold, long long* lower_threshold) {
  long long upper_dev, lower_dev, mgv;
  ComputeStats(image, region_points, &mgv, &upper_dev, &lower_dev);

  *upper_threshold = mgv + (upper_dev * 1.5 + 20 / sqrt(region_points.size()));
  *lower_threshold = mgv - (lower_dev * 1.5 + 20 / sqrt(region_points.size()));

  std::cout << "mgv " << mgv << " upper_dev " << upper_dev << " lower_dev " << lower_dev << std::endl;
  std::cout << "thresholds " << *upper_threshold << " " << *lower_threshold << std::endl;
}

// Compute thresholds for the final run.
void ComputeFinalThreshold(InternalImageType::Pointer& image,
                           std::unordered_map<long long, InternalImageType::IndexType>& region_points,
                           long long* upper_threshold, long long* lower_threshold) {
  long long upper_dev, lower_dev, mgv;
  ComputeStats(image, region_points, &mgv, &upper_dev, &lower_dev);

  *upper_threshold = mgv + (upper_dev * 2 * 2.58 + 20 / sqrt(region_points.size()));
  *lower_threshold = mgv - (lower_dev * 2 * 2.58 + 20 / sqrt(region_points.size()));

  std::cout << "mgv " << mgv << " upper_dev " << upper_dev << " lower_dev " << lower_dev << std::endl;
  std::cout << "final thresholds " << *upper_threshold << " " << *lower_threshold << std::endl;
}

// Compute unique value for every point.
long long ComputePointHash(InternalImageType::IndexType& point) {
  return point[0] * point[1] * point[2] + point[0]*100 + point[1]*10 + point[2];
}

// Check if point is within bounds.
bool CheckBounds(InternalImageType::IndexType& point,
                 InternalImageType::SizeType size) {
  if (!(point[0] < size[0] && point[0] >= 0)) return false;
  if (!(point[1] < size[1] && point[1] >= 0)) return false;
  if (!(point[2] < size[2] && point[2] >= 0)) return false;
  return true;
}

void GrowRegions(InternalImageType::Pointer& image,
                 InternalImageType::IndexType& seed,
                 long long* upper_threshold,
                 long long* lower_threshold) {
  std::unordered_map<long long, InternalImageType::IndexType> visited_points;
  std::queue<InternalImageType::IndexType> q;
  InternalImageType::RegionType region = image->GetLargestPossibleRegion();
  InternalImageType::SizeType size = region.GetSize();

  // Adding the first point to the region.
  visited_points[ComputePointHash(seed)] = seed;
  // Adding all 26 neighbors of the seed point
  for (int i = -1; i <= 1; i++) {
    for (int j = -1; j <= 1; j++) {
      for (int k = -1; k <= 1; k++) {
        // We should not add cental point
        if (!(i == 0 && j == 0 && k ==0)) {
          InternalImageType::IndexType seed_copy = seed;
          seed_copy[0] += i;
          seed_copy[1] += j;
          seed_copy[2] += k;
          if (!CheckBounds(seed_copy, size)) continue;
          q.push(seed_copy);
          std::cout << "new point " << image->GetPixel(seed_copy) << std::endl;
          visited_points[ComputePointHash(seed_copy)] = seed_copy;
        }
      }
    }
  }
  ComputeThreshold(image, visited_points, upper_threshold, lower_threshold);
  long long region_size = visited_points.size();

  // Running a loop - removing front element from the queue and adding its neighbors
  // to the back of the queue.
  int iter = 0;
  while (q.size()) {
    iter++;
    //std::cout << "iter " << iter << " " << visited_points.size() << " queue " << q.size() << std::endl;
    if (iter > 10000000) {
      break;
    }
    InternalImageType::IndexType elem = q.front();
    q.pop();
    for (int i = -1; i <= 1; i++) {
      for (int j = -1; j <= 1; j++) {
        for (int k = -1; k <= 1; k++) {
          // We should not add cental point
          if (!(i == 0 && j == 0 && k ==0)) {
            InternalImageType::IndexType seed_copy = elem;
            seed_copy[0] += i;
            seed_copy[1] += j;
            seed_copy[2] += k;
            if (!CheckBounds(seed_copy, size)) continue;
            // If pixel value is withing the thresholds, we proceed with it.
            //std::cout << "seed_copy " << seed_copy << " " << image->GetPixel(seed_copy) << std::endl;

            if (image->GetPixel(seed_copy) >= *lower_threshold && image->GetPixel(seed_copy) <= *upper_threshold) {
              //std::cout << "fits in thresh" << std::endl;
              // If we already visited this point, don't add it.
              if (visited_points.find(ComputePointHash(seed_copy)) !=
                  visited_points.end()) {
                continue;
              }
              //std::cout << "new point " << image->GetPixel(seed_copy) << std::endl;

              q.push(seed_copy);
              visited_points[ComputePointHash(seed_copy)] = seed_copy;

              // Recomputing thresholds if number of points doubled.
              if (visited_points.size() / region_size >= 2) {
                ComputeThreshold(image, visited_points, upper_threshold, lower_threshold);
                region_size = visited_points.size();
              }
            }
          }
        }
      }
    }
  }

  // Finally, we need to compute the final thresholds for the second run.
  ComputeFinalThreshold(image, visited_points, upper_threshold, lower_threshold);
}

int main(int argc, char *argv[])
{
   if(argc < 6) {
      std::cerr << "Missing Parameters " << std::endl;
      std::cerr << "Usage: " << argv[0];
      std::cerr << " inputImage  outputImage seedX seedY seedZ" << std::endl;
      return EXIT_FAILURE;
    }
    typedef   float           InternalPixelType;
    const     unsigned int    Dimension = 3;
    typedef itk::Image< InternalPixelType, Dimension >  InternalImageType;
    typedef unsigned char                            OutputPixelType;
    typedef itk::Image< OutputPixelType, Dimension > OutputImageType;
    typedef itk::CastImageFilter< InternalImageType, OutputImageType >
                                                     CastingFilterType;

    CastingFilterType::Pointer caster = CastingFilterType::New();

    typedef  itk::ImageFileReader< InternalImageType > ReaderType;
    typedef  itk::ImageFileWriter<  OutputImageType  > WriterType;
    ReaderType::Pointer reader = ReaderType::New();
    WriterType::Pointer writer = WriterType::New();
    reader->SetFileName(argv[1]);
    writer->SetFileName(argv[2]);

    // Smoothing the image before appplying the region growing.
    typedef itk::CurvatureFlowImageFilter< InternalImageType, InternalImageType >
       CurvatureFlowImageFilterType;
    CurvatureFlowImageFilterType::Pointer smoothing =
       CurvatureFlowImageFilterType::New();
    smoothing->SetInput(reader->GetOutput());

    typedef itk::ConnectedThresholdImageFilter< InternalImageType,
        InternalImageType > ConnectedFilterType;
    ConnectedFilterType::Pointer connectedThreshold = ConnectedFilterType::New();
    connectedThreshold->SetInput(smoothing->GetOutput());
    caster->SetInput(connectedThreshold->GetOutput());
    writer->SetInput(caster->GetOutput());
    smoothing->SetNumberOfIterations(2);
    smoothing->SetTimeStep(0.05);

    // The color we aregoing to paint the region with.
    connectedThreshold->SetReplaceValue(255);

    // Setting up seeds.
    InternalImageType::IndexType  index;
    index[0] = atoi(argv[3]);
    index[1] = atoi(argv[4]);
    index[2] = atoi(argv[5]);
    connectedThreshold->SetSeed(index);

    InternalImageType::Pointer image = smoothing->GetOutput();
    smoothing->Update();
    long long upper_threshold, lower_threshold;
    GrowRegions(image, index, &upper_threshold, &lower_threshold);

    connectedThreshold->SetLower(lower_threshold);
    connectedThreshold->SetUpper(upper_threshold);

    // Running all filters at once
    try {
       writer->Update();
    }
    catch(itk::ExceptionObject & excep) {
       std::cerr << "Exception caught !" << std::endl;
       std::cerr << excep << std::endl;
    }
    return EXIT_SUCCESS;
}
