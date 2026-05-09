#include "nwb/behavior/SpatialSeries.hpp"

#include "Utils.hpp"

using namespace AQNWB::NWB;

// SpatialSeries
// Initialize the static registered_ member to trigger registration
REGISTER_SUBCLASS_IMPL(SpatialSeries)

SpatialSeries::SpatialSeries(const std::string& path,
                             std::shared_ptr<IO::BaseIO> io)
    : TimeSeries(path, io)
{
}

SpatialSeries::~SpatialSeries() {}

Status SpatialSeries::initialize(const IO::BaseArrayDataSetConfig& dataConfig,
                                 const std::string& unit,
                                 const std::string& description,
                                 const std::string& referenceFrame,
                                 const std::string& comments,
                                 const float& conversion,
                                 const float& resolution,
                                 const float& offset)
{
  auto ioPtr = getIO();
  if (!ioPtr) {
    std::cerr << "SpatialSeries::initialize: IO object is not valid."
              << std::endl;
    return Status::Failure;
  }

  // Delegate the standard TimeSeries datasets/attributes (data, timestamps,
  // unit, conversion, resolution, offset, description, comments) to the
  // base class.  ``unit`` is caller-supplied here — that's the one piece
  // that differs from ElectricalSeries (which fixes ``unit`` to ``"volts"``).
  auto tsInitStatus = TimeSeries::initialize(dataConfig,
                                             unit,
                                             description,
                                             comments,
                                             conversion,
                                             resolution,
                                             offset);

  // The reference_frame dataset is optional in the schema (``quantity: "?"``),
  // so we only emit it when the caller supplies a non-empty string.  This
  // keeps the on-disk shape minimal for callers who don't have meaningful
  // reference-frame metadata to record.
  if (!referenceFrame.empty()) {
    try {
      ioPtr->createStringDataSet(
          AQNWB::mergePaths(getPath(), "reference_frame"), referenceFrame);
    } catch (const std::runtime_error& e) {
      std::cerr << "SpatialSeries::initialize: failed to create "
                   "reference_frame dataset: "
                << e.what() << std::endl;
      return Status::Failure;
    }
  }

  return tsInitStatus;
}

Status SpatialSeries::writeAllChannels(const SizeType& numSamples,
                                       const SizeType& numDimensions,
                                       const void* dataInput,
                                       const void* timestampsInput,
                                       const void* controlInput)
{
  // Build a 1-D ``[numSamples]`` shape when the data is scalar per
  // sample, or a 2-D ``[numSamples, numDimensions]`` shape otherwise.
  // The dataset's actual rank is determined by the BaseArrayDataSetConfig
  // the caller passed to ``initialize``; this method just writes a single
  // contiguous block at the current row offset.
  SizeArray dataShape;
  SizeArray positionOffset;
  if (numDimensions <= 1) {
    dataShape = {numSamples};
    positionOffset = {m_samplesRecorded};
  } else {
    dataShape = {numSamples, numDimensions};
    positionOffset = {m_samplesRecorded, 0};
  }

  Status status = TimeSeries::writeData(
      dataShape, positionOffset, dataInput, timestampsInput, controlInput);

  if (status == Status::Success) {
    m_samplesRecorded += numSamples;
  }
  return status;
}
