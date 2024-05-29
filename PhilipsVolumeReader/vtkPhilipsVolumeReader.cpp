//#include "stdafx.h"

#include <vtkErrorCode.h>
#include <vtkImageData.h>
#include <vtkInformation.h>
#include <vtkInformationVector.h>
#include <vtkObjectFactory.h>
#include <vtkStreamingDemandDrivenPipeline.h>
#include <gdcmAttribute.h>
#include <gdcmImageReader.h>
#include "vtkPhilipsVolumeReader.h"

vtkStandardNewMacro(vtkPhilipsVolumeReader);

void vtkPhilipsVolumeReader::PrintSelf(ostream & os, vtkIndent indent)
{
  Superclass::PrintSelf(os, indent);
}

vtkPhilipsVolumeReader ::vtkPhilipsVolumeReader () : reader{new gdcm::ImageReader}
{
}

int vtkPhilipsVolumeReader ::CanReadFile(const char * fname)
{
  gdcm::Reader reader;
  reader.SetFileName(fname);
  if (!reader.CanRead()) return 0;
  if (!reader.Read()) return 0;

  auto const& ds1 = reader.GetFile().GetDataSet();
  gdcm::Attribute<0x0008, 0x0060> modality;
  modality.SetFromDataSet(ds1);
  if (modality.GetValue() != "US") return 0;

  gdcm::Attribute<0x0008, 0x0070> manufacturer;
  manufacturer.SetFromDataSet(ds1);
  if (manufacturer.GetValue() != "Philips Medical Systems ") return 0;
  return 1;
}

int vtkPhilipsVolumeReader ::FillOutputPortInformation(int port, vtkInformation * info)
{
  info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkImageData");
  return 1;
}

int vtkPhilipsVolumeReader ::RequestInformation(vtkInformation * request, vtkInformationVector ** inputVector, vtkInformationVector * outputVector)
{
  if (!this->read()) return 0;
  auto const& ds1 = reader->GetFile().GetDataSet();
  this->data_type_ = this->get_data_type(ds1);

  // dimensions
  {
    gdcm::Tag const tag_volume_dimensions{ 0x200d, 0x3301 };
    if (!ds1.FindDataElement(tag_volume_dimensions))
    {
      vtkErrorMacro(<< "Not found tag_volume_dimensions");
      return false;
    }
    const gdcm::DataElement& element_dim = ds1.GetDataElement(tag_volume_dimensions);
    gdcm::Element<gdcm::VR::DS, gdcm::VM::VM3> dims; // Use DS to interpret value stored in LO
    dims.SetFromDataElement(element_dim);
    this->DataExtent[0] = 0.;
    this->DataExtent[1] = dims[0] - 1;
    this->DataExtent[2] = 0;
    this->DataExtent[3] = dims[1] - 1;
    this->DataExtent[4] = 0;
    this->DataExtent[5] = dims[2] - 1;
  }

  // spacing
  {
    gdcm::Tag const tag{ 0x200d, 0x3303 };
    if (!ds1.FindDataElement(tag))
    {
      vtkErrorMacro(<< "Not found");
      return 0;
    }
    const gdcm::DataElement& e = ds1.GetDataElement(tag);
    gdcm::Element<gdcm::VR::DS, gdcm::VM::VM3> data; // Use DS to interpret value stored in LO
    data.SetFromDataElement(e);
    this->DataSpacing[0] = data[0];
    this->DataSpacing[1] = data[1];
    this->DataSpacing[2] = data[2];
  }

  // Single 3D Volume
  if (this->data_type_ == Native_3D_Cartesian)
  {
    this->ExecuteInformation3D(ds1);
  }
  // 4D
  else if (this->data_type_ == Multi_Volume)
  {
    this->ExecuteInformation4D(ds1);
  }
  else
  {
    // others
    vtkWarningMacro(<< "Not supported type");
    return 0;
  }

  this->DataScalarType = VTK_UNSIGNED_CHAR;

  for (int i = 0; i < this->GetNumberOfOutputPorts(); ++i)
  {
    if (!this->GetOutput(i))
    {
      vtkImageData* img = vtkImageData::New();
      this->GetExecutive()->SetOutputData(i, img);
      img->Delete();
    }

    vtkInformation* outInfo = outputVector->GetInformationObject(i);
    outInfo->Set(vtkStreamingDemandDrivenPipeline::WHOLE_EXTENT(), this->DataExtent, 6);
    outInfo->Set(vtkDataObject::SPACING(), this->DataSpacing, 3);
    outInfo->Set(vtkDataObject::ORIGIN(), this->DataOrigin, 3);
    vtkDataObject::SetPointDataActiveScalarInfo(outInfo, this->DataScalarType,
      this->NumberOfScalarComponents);
  }
  return 1;
}

bool vtkPhilipsVolumeReader ::ExecuteInformation3D(gdcm::DataSet const & ds1)
{
  auto const& image = static_cast<gdcm::ImageReader*>(this->reader.get())->GetImage();
  size_t frames = image.GetDimension(2);
  
  if ((this->DataExtent[5] - this->DataExtent[4] + 1) == frames)
  {
    // B-mode only
    this->SetNumberOfOutputPorts(1);
  }
  else
  {
    // B + Dopller
    this->SetNumberOfOutputPorts(2);
  }

  return 1;
}

bool vtkPhilipsVolumeReader ::ExecuteInformation4D(gdcm::DataSet const& ds1)
{
  // num frames in UDM_USD_DATATYPE_DIN_3D_ECHO
  {
    const gdcm::Tag tseq1(0x200d, 0x3016);
    if (!ds1.FindDataElement(tseq1))
    {
      std::cerr << "Not found" << std::endl;
      return false;
    }
    const gdcm::DataElement& seq1 = ds1.GetDataElement(tseq1);
    gdcm::SmartPointer<gdcm::SequenceOfItems> sqi1 = seq1.GetValueAsSQ();
    const size_t nitems = sqi1->GetNumberOfItems();

    for (size_t i = 0; i < nitems; ++i)
    {
      gdcm::Item& item = sqi1->GetItem(i + 1);
      gdcm::DataSet& item_data_set = item.GetNestedDataSet();

      gdcm::PrivateTag const data_type{ 0x200d, 0x300d, "Philips US Imaging DD 033" };
      if (!item_data_set.FindDataElement(data_type))
      {
        std::cerr << "not found" << std::endl;
        continue;
      }
      gdcm::ByteValue const* data_type_name = item_data_set.GetDataElement(data_type).GetByteValue();
      std::string type(data_type_name->GetPointer(), data_type_name->GetLength());
      std::cout << type << std::endl;
      if (type != "UDM_USD_DATATYPE_DIN_3D_ECHO") continue;

      gdcm::PrivateTag const tseq2{ 0x200d, 0x3020, "Philips US Imaging DD 033" };
      if (!(item_data_set.FindDataElement(tseq2)))
      {
        std::cerr << "not found tseq2" << std::endl;
        continue;
      }

      gdcm::SmartPointer<gdcm::SequenceOfItems> sqi2 = item_data_set.GetDataElement(tseq2).GetValueAsSQ();
      std::cout << sqi2->GetNumberOfItems() << std::endl;
      gdcm::DataSet const& ds3 = sqi2->GetItem(1).GetNestedDataSet();

      // number of frames
      gdcm::PrivateTag const tag_number_of_frames{ 0x200d, 0x3010, "Philips US Imaging DD 033" };
      if (!(ds3.FindDataElement(tag_number_of_frames)))
      {
        std::cerr << "not found tag_number_of_frames" << std::endl;
        continue;
      }

      gdcm::Element<gdcm::VR::IS, gdcm::VM::VM1> element_num_frames;
      gdcm::DataElement const& data_element = ds3.GetDataElement(tag_number_of_frames);
      element_num_frames.SetFromDataElement(data_element);
      int const num_frames = element_num_frames.GetValue();
      std::cout << num_frames << std::endl;
      this->SetNumberOfOutputPorts(num_frames);
      return 1;
    }
  }
  return 0;
}

int vtkPhilipsVolumeReader ::RequestData(vtkInformation * request, vtkInformationVector ** inputVector, vtkInformationVector * outputVector)
{
  int outputPort = request->Get(vtkDemandDrivenPipeline::FROM_OUTPUT_PORT());
  if (outputPort == -1) outputPort = 0;

  vtkInformation *outInfo = outputVector->GetInformationObject(outputPort);
  this->SetErrorCode(vtkErrorCode::NoError);
  if (outInfo)
  {
    vtkImageData* data = this->AllocateOutputData(outInfo->Get(vtkDataObject::DATA_OBJECT()), outInfo);
    if (this->read())
    {
      if (this->data_type_ == Multi_Volume)
      {
        this->ExecuteData4D(outputPort, data);
      }
      else if (this->data_type_ == Native_3D_Cartesian)
      {
        this->ExecuteData3D(outputPort, data);
      }
      else
      {
        return 0;
      }
    }
  }
  return 1;
}

int vtkPhilipsVolumeReader ::ExecuteData3D(int const output_port, vtkImageData * output)
{
  auto const& image = static_cast<gdcm::ImageReader*>(reader.get())->GetImage();
  std::unique_ptr<char[]> buf{ new char[image.GetBufferLength()] };
  image.GetBuffer(buf.get());
  void* ptr = output->GetScalarPointer();
  std::memcpy(ptr, buf.get() + output_port * output->GetNumberOfPoints(), output->GetNumberOfPoints());
  return 1;
}

int vtkPhilipsVolumeReader ::ExecuteData4D(int const output_port, vtkImageData* output)
{
  auto const& ds1 = this->reader->GetFile().GetDataSet();

  const gdcm::Tag tseq1(0x200d, 0x3016);
  if (!ds1.FindDataElement(tseq1))
  {
    std::cerr << "Not found" << std::endl;
    return false;
  }
  const gdcm::DataElement& seq1 = ds1.GetDataElement(tseq1);
  gdcm::SmartPointer<gdcm::SequenceOfItems> sqi1 = seq1.GetValueAsSQ();
  const size_t nitems = sqi1->GetNumberOfItems();

  for (size_t i = 0; i < nitems; ++i)
  {
    gdcm::Item& item = sqi1->GetItem(i + 1);
    gdcm::DataSet& item_data_set = item.GetNestedDataSet();

    gdcm::PrivateTag const data_type{ 0x200d, 0x300d, "Philips US Imaging DD 033" };
    if (!item_data_set.FindDataElement(data_type))
    {
      std::cerr << "not found" << std::endl;
      continue;
    }
    gdcm::ByteValue const* data_type_name = item_data_set.GetDataElement(data_type).GetByteValue();
    std::string type(data_type_name->GetPointer(), data_type_name->GetLength());

    if (type != "UDM_USD_DATATYPE_DIN_3D_ECHO") continue;

    gdcm::PrivateTag const tseq2{ 0x200d, 0x3020, "Philips US Imaging DD 033" };
    if (!(item_data_set.FindDataElement(tseq2)))
    {
      std::cerr << "not found tseq2" << std::endl;
      continue;
    }

    gdcm::SmartPointer<gdcm::SequenceOfItems> sqi2 = item_data_set.GetDataElement(tseq2).GetValueAsSQ();
    std::cout << sqi2->GetNumberOfItems() << std::endl;
    gdcm::DataSet const& ds3 = sqi2->GetItem(1).GetNestedDataSet();

    gdcm::PrivateTag const tag_number_of_frames{ 0x200d, 0x3010, "Philips US Imaging DD 033" };
    if (!(ds3.FindDataElement(tag_number_of_frames)))
    {
      std::cerr << "not found tag_number_of_frames" << std::endl;
      continue;
    }

    gdcm::Element<gdcm::VR::IS, gdcm::VM::VM1> element_num_frames;
    gdcm::DataElement const& data_element = ds3.GetDataElement(tag_number_of_frames);
    element_num_frames.SetFromDataElement(data_element);
    int const num_frames = element_num_frames.GetValue();

    gdcm::PrivateTag const tzalloc(0x200d, 0x3011, "Philips US Imaging DD 033");
    if (!ds3.FindDataElement(tzalloc))
    {
      std::cerr << "not found tzalloc" << std::endl;
      continue;
    }
    const gdcm::DataElement& zalloc = ds3.GetDataElement(tzalloc);
    gdcm::Element<gdcm::VR::IS, gdcm::VM::VM1> elzalloc;
    elzalloc.SetFromDataElement(zalloc);
    const int zallocref = elzalloc.GetValue();
    std::cout << zallocref << std::endl;

    gdcm::PrivateTag const tag_data(0x200d, 0x300e, "Philips US Imaging DD 033");
    if (!ds3.FindDataElement(tag_data))
    {
      std::cerr << "not found tzalloc" << std::endl;
      continue;
    }
    const gdcm::DataElement& element_data = ds3.GetDataElement(tag_data);
    gdcm::ByteValue const* data = element_data.GetByteValue();
    output->GetScalarPointer();
    output->GetNumberOfPoints();
    std::memcpy(
      output->GetScalarPointer(),
      data->GetPointer() + output_port * zallocref + 16,
      output->GetNumberOfPoints()
    );
  }
  return 0;
}

bool vtkPhilipsVolumeReader ::read()
{
  if (this->GetMTime() > this->read_time)
  {
    reader->SetFileName(this->FileName);
    if (!reader->Read()) return false;
    this->read_time.Modified();
  }
  return true;
}

vtkPhilipsVolumeReader ::data_type vtkPhilipsVolumeReader ::get_data_type(gdcm::DataSet const& ds)
{
  gdcm::Tag const data_type_tag{ 0x200d, 0x2005 };
  gdcm::DataElement const& data_type = ds.GetDataElement(data_type_tag);
  std::string data_type_name{ data_type.GetByteValue()->GetPointer(), data_type.GetByteValue()->GetLength() };

  // Single 3D Volume
  if (data_type_name == "Native 3D Cartesian ")
  {
    return Native_3D_Cartesian;
  }
  // 4D
  else if (data_type_name == "3D_Multi_Volume ")
  {
    return Multi_Volume;
  }

  return Others;
}
