#pragma once
#include <memory>
#include <vtkImageReader2.h>
#include <gdcmReader.h>
#include <gdcmDataSet.h>
#include "vtkPhilipsVolumeReader_export.h"

class vtkImageData;

class VTKPHILIPSVOLUMEREADER_EXPORT vtkPhilipsVolumeReader : public vtkImageReader2
{
  vtkPhilipsVolumeReader(vtkPhilipsVolumeReader  const&) = delete;
  vtkPhilipsVolumeReader  operator=(vtkPhilipsVolumeReader  const&) = delete;

  enum data_type
  {
    Native_3D_Cartesian,
    Multi_Volume,
    Others = -1
  };

public:
  static vtkPhilipsVolumeReader* New();
  vtkTypeMacro(vtkPhilipsVolumeReader, vtkImageReader2);
  virtual void PrintSelf(ostream& os, vtkIndent indent) override;

  vtkPhilipsVolumeReader();
  virtual ~vtkPhilipsVolumeReader() {};
  virtual int CanReadFile(const char* fname) override;

protected:
  int FillOutputPortInformation(int port, vtkInformation* info) override;
  int RequestInformation(vtkInformation* request, vtkInformationVector** inputVector, vtkInformationVector* outputVector) override;
  bool ExecuteInformation3D(gdcm::DataSet const& ds1);
  bool ExecuteInformation4D(gdcm::DataSet const& ds1);

  int RequestData(vtkInformation *request, vtkInformationVector **inputVector, vtkInformationVector *outputVector) override;
  int ExecuteData3D(int const output_port, vtkImageData* output);
  int ExecuteData4D(int const output_port, vtkImageData* output);

  bool read();
  data_type get_data_type(gdcm::DataSet const& ds);

  vtkTimeStamp read_time;
  std::unique_ptr<gdcm::Reader> reader;
  data_type data_type_;
};
