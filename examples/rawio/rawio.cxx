// Read and write raw image files (*.raw) that have (*.mhd);
// the output should be identical in content and size
// to the input. In theory we should be able to process layers
// (regions) as an intermediate step via itk::ImageRegionIterator.
//
// This is based on example:
// http://www.na-mic.org/svn/Slicer3-lib-mirrors/trunk/Insight/Testing/Code/IO/itkMetaImageStreamingWriterIOTest.cxx 
//
// This example assumes pixel values are 16-bit signed integers.

#if defined(_MSC_VER)
#pragma warning ( disable : 4786 )
#endif

#ifdef __BORLANDC__
#define ITK_LEAN_AND_MEAN
#endif

#include <fstream>
#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"
#include "itkMetaImageIO.h"
#include "stdint.h"

int main(int ac, char* av[])
{
  if(ac<3)
  {
    std::cerr << "Usage: " << av[0] << " input output\n";
    std::cerr << "  where both input and output are RAW image header"
		<< " files (*.mhd).\n";
	std::cerr << "  e.g. " << av[0] << " ../in.mhd ../out.mhd\n";
  }
      
  // remove the output file
  itksys::SystemTools::RemoveFile(av[2]);
    
  typedef int16_t                    PixelType;
  typedef itk::Image<PixelType,3>   ImageType;

  itk::MetaImageIO::Pointer metaImageIO = itk::MetaImageIO::New();

  typedef itk::ImageFileReader<ImageType>         ReaderType;
  typedef itk::ImageFileWriter<ImageType >        WriterType;

  ReaderType::Pointer reader = ReaderType::New();
  reader->SetImageIO(metaImageIO);
  reader->SetFileName(av[1]);
  reader->SetUseStreaming(true);
  metaImageIO->SetUseStreamedReading(true);

  ImageType::RegionType region;
  ImageType::SizeType size;
  ImageType::SizeType fullsize;
  ImageType::IndexType index;
  
  // unsigned int m_NumberOfPieces = 10;
  
  // We decide how we want to read the image and we split accordingly
  // The image is read slice by slice
  reader->GenerateOutputInformation();
  fullsize = reader->GetOutput()->GetLargestPossibleRegion().GetSize();

  index.Fill(0);
  size[0] = fullsize[0];
  size[1] = fullsize[1];
  size[2] = fullsize[2];

  std::cout << "x: size[0] = " << fullsize[0] << "\n";
  std::cout << "y: size[1] = " << fullsize[1] << "\n";
  std::cout << "z: size[2] = " << fullsize[2] << "\n";

  int layerSize = fullsize[0] * fullsize[1];
  int numLayers = fullsize[2]; 
  std::cout << "layer size: " << layerSize << "\n";
  std::cout << "# of layers: " << numLayers << "\n";

  // unsigned int zsize = fullsize[2]/m_NumberOfPieces;

  // Setup the writer
  WriterType::Pointer writer = WriterType::New();
  writer->SetFileName(av[2]);
  
  // for(unsigned int i=0;i<m_NumberOfPieces;i++)
  //   {
  //   std::cout << "Reading piece " << i+1 << " of " << m_NumberOfPieces << std::endl;

  //   index[2] += size[2];

  //   // At the end we need to adjust the size to make sure
  //   // we are reading everything
  //   if(i == m_NumberOfPieces-1)
  //     {
  //     size[2] = fullsize[2]-index[2];
  //     }
  //   else
  //     {
  //     size[2] = zsize;
  //     }

  //   region.SetIndex(index);
  //   region.SetSize(size);
  //   reader->GetOutput()->SetRequestedRegion(region);
  //   try
  //     {
  //     reader->Update();
  //     }
  //   catch (itk::ExceptionObject &ex)
  //     {
  //     std::cout << "ERROR : " << ex << std::endl;
  //     return EXIT_FAILURE;
  //     }
   
    // Write the image     
    itk::ImageIORegion  ioregion(3);
    itk::ImageIORegion::IndexType index2;
    index2.push_back(region.GetIndex()[0]);
    index2.push_back(region.GetIndex()[1]);
    index2.push_back(region.GetIndex()[2]);
    ioregion.SetIndex(index2);
    itk::ImageIORegion::SizeType size2;
    size2.push_back(region.GetSize()[0]);
    size2.push_back(region.GetSize()[1]);
    size2.push_back(region.GetSize()[2]);
    ioregion.SetSize(size2);
    writer->SetIORegion(ioregion);
    writer->SetInput(reader->GetOutput());
    
    try
    {
      writer->Update();
    }
    catch (itk::ExceptionObject & err)
    {
      std::cerr << "ExceptionObject caught !" << std::endl;
      std::cerr << err << std::endl;
      return EXIT_FAILURE;
    }
  //   } // end for pieces
   
   // writer->SetInput(reader->GetOutput());
   // write->update();
     
  return EXIT_SUCCESS;
}
