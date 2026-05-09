#include "nwb/ecephys/FeatureExtraction.hpp"

#include <iostream>

#include "Utils.hpp"
#include "nwb/file/ElectrodesTable.hpp"

using namespace AQNWB::NWB;

REGISTER_SUBCLASS_IMPL(FeatureExtraction)

FeatureExtraction::FeatureExtraction(const std::string& path,
                                     std::shared_ptr<IO::BaseIO> io)
    : NWBDataInterface(path, io)
{
}

FeatureExtraction::~FeatureExtraction() {}

Status FeatureExtraction::initialize(
    const IO::BaseArrayDataSetConfig& featuresConfig,
    const IO::BaseArrayDataSetConfig& timesConfig,
    const std::vector<std::string>& featureLabels,
    const std::vector<int>& electrodeIndices,
    SizeType numChannels,
    SizeType numFeatures,
    const std::string& description)
{
  auto ioPtr = getIO();
  if (!ioPtr) {
    std::cerr << "FeatureExtraction::initialize: IO object is not valid."
              << std::endl;
    return Status::Failure;
  }

  m_numChannels = numChannels;
  m_numFeatures = numFeatures;

  if (featureLabels.size() != m_numFeatures) {
    std::cerr << "FeatureExtraction::initialize: featureLabels.size()="
              << featureLabels.size()
              << " must match num_features axis (" << m_numFeatures << ")."
              << std::endl;
    return Status::Failure;
  }
  if (electrodeIndices.size() != m_numChannels) {
    std::cerr << "FeatureExtraction::initialize: electrodeIndices.size()="
              << electrodeIndices.size()
              << " must match num_channels axis (" << m_numChannels << ")."
              << std::endl;
    return Status::Failure;
  }

  // ``NWBDataInterface::initialize`` chains to ``Container::initialize``
  // which creates the on-disk group at getPath() and sets the standard
  // NWB common attributes (neurodata_type, namespace).  Without this the
  // subsequent createArrayDataSet calls have no parent to attach to.
  //
  // Note: ``FeatureExtraction``'s NWB schema (``nwb_ecephys`` in
  // ``src/spec/core.hpp``) defines ``description`` ONLY as a 1-D text
  // *dataset* (one entry per feature dim) — there is no group-level
  // ``description`` attribute on FeatureExtraction itself, unlike most
  // other NWBDataInterface subclasses.  Setting both a same-named
  // attribute and dataset at the same path makes hdmf read fail with
  // "description already exists in attributes, cannot set in datasets".
  // The ``description`` parameter is therefore unused by the writer; we
  // log it for parity with other classes but don't emit it on disk.
  (void) description;
  Status containerStatus = NWBDataInterface::initialize();
  if (containerStatus != Status::Success) {
    return Status::Failure;
  }

  // features [num_events, num_channels, num_features], float32, extendable.
  Status overall = Status::Success;
  try {
    ioPtr->createArrayDataSet(featuresConfig,
                              AQNWB::mergePaths(getPath(), "features"));
  } catch (const std::runtime_error& e) {
    std::cerr << "FeatureExtraction::initialize: failed to create features "
                 "dataset: "
              << e.what() << std::endl;
    return Status::Failure;
  }

  // times [num_events], float64, extendable.
  try {
    ioPtr->createArrayDataSet(timesConfig,
                              AQNWB::mergePaths(getPath(), "times"));
  } catch (const std::runtime_error& e) {
    std::cerr << "FeatureExtraction::initialize: failed to create times "
                 "dataset: "
              << e.what() << std::endl;
    return Status::Failure;
  }

  // description: 1-D text dataset, one entry per feature.  Use the
  // ``createStringDataSet`` overload that takes a vector of strings (write
  // once at init).
  try {
    ioPtr->createStringDataSet(
        AQNWB::mergePaths(getPath(), "description"), featureLabels);
  } catch (const std::runtime_error& e) {
    std::cerr << "FeatureExtraction::initialize: failed to create description "
                 "dataset: "
              << e.what() << std::endl;
    return Status::Failure;
  }

  // electrodes: 1-D int32 DynamicTableRegion linking each channel to a row
  // in ElectrodesTable.  Mirrors ElectricalSeries::initialize lines 111–137.
  IO::ArrayDataSetConfig electrodesConfig(IO::BaseDataType::I32,
                                          SizeArray {m_numChannels},
                                          SizeArray {m_numChannels});
  try {
    ioPtr->createArrayDataSet(electrodesConfig,
                              AQNWB::mergePaths(getPath(), "electrodes"));

    auto electrodesRecorder = recordElectrodes();
    overall = overall
        && electrodesRecorder->writeDataBlock(SizeArray {m_numChannels},
                                              IO::BaseDataType::I32,
                                              electrodeIndices.data());
    auto electrodesPath = AQNWB::mergePaths(getPath(), "electrodes");
    overall = overall
        && ioPtr->createCommonNWBAttributes(
                  electrodesPath, "hdmf-common", "DynamicTableRegion");
    overall = overall
        && ioPtr->createAttribute(
                  "the electrodes that generated this feature extraction",
                  electrodesPath,
                  "description");
    overall = overall
        && ioPtr->createReferenceAttribute(
                  ElectrodesTable::electrodesTablePath,
                  electrodesPath,
                  "table");
  } catch (const std::runtime_error& e) {
    std::cerr << "FeatureExtraction::initialize: failed to create electrodes "
                 "dataset: "
              << e.what() << std::endl;
    return Status::Failure;
  }

  return overall;
}

Status FeatureExtraction::writeEvents(const SizeType& numEvents,
                                      const void* featuresInput,
                                      const void* timesInput)
{
  if (numEvents == 0) {
    return Status::Success;
  }

  // Append ``numEvents`` rows to ``features`` along axis 0.
  Status status = Status::Success;
  {
    auto featuresRecorder = recordFeatures();
    if (!featuresRecorder) {
      std::cerr << "FeatureExtraction::writeEvents: features recorder unavailable."
                << std::endl;
      return Status::Failure;
    }
    SizeArray dataShape {numEvents, m_numChannels, m_numFeatures};
    SizeArray positionOffset {m_eventsRecorded, 0, 0};
    status = status
        && featuresRecorder->writeDataBlock(
                dataShape, positionOffset, IO::BaseDataType::F32, featuresInput);
  }

  // Append ``numEvents`` rows to ``times`` along axis 0.
  {
    auto timesRecorder = recordTimes();
    if (!timesRecorder) {
      std::cerr << "FeatureExtraction::writeEvents: times recorder unavailable."
                << std::endl;
      return Status::Failure;
    }
    SizeArray dataShape {numEvents};
    SizeArray positionOffset {m_eventsRecorded};
    status = status
        && timesRecorder->writeDataBlock(
                dataShape, positionOffset, IO::BaseDataType::F64, timesInput);
  }

  if (status == Status::Success) {
    m_eventsRecorded += numEvents;
  }
  return status;
}
