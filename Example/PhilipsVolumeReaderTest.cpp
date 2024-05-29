#include <iostream>
#include <sstream>
#include <string>
#include <vtkPhilipsVolumeReader.h>
#include <vtkNew.h>
#include <vtkXMLImageDataWriter.h>

int main(int argc, char* argv[])
{
  if (argc < 2)
  {
    std::cout << "Usage: PhilipsUSReaderTest.exe input_dicom" << std::endl;
    exit(EXIT_FAILURE);
  }

  std::string input_name(argv[1]);
  std::cout << input_name << std::endl;

  vtkNew<vtkPhilipsVolumeReader> reader;
  reader->SetFileName(input_name.c_str());
  reader->Update();


  for (int i = 0; i < reader->GetNumberOfOutputPorts(); ++i)
  {
    std::ostringstream oss;
    oss << input_name << "-" << i << ".vti";
    vtkNew<vtkXMLImageDataWriter> writer;
    writer->SetFileName(oss.str().c_str());
    writer->SetInputConnection(reader->GetOutputPort(i));
    writer->Write();
  }
}
