#pragma once

#include <string>
#include <vector>

#include "Utils.hpp"
#include "io/BaseIO.hpp"
#include "io/ReadIO.hpp"
#include "nwb/base/NWBDataInterface.hpp"
#include "nwb/file/ElectrodesTable.hpp"
#include "spec/core.hpp"

namespace AQNWB::NWB
{
/**
 * @brief Features extracted from electrical recordings.
 *
 * Stores a 3-D ``[num_events, num_channels, num_features]`` ``features``
 * dataset, a ``description`` text dataset naming each feature dimension
 * (e.g. ``["spk", "sbp"]``), a 1-D ``times`` dataset with the time of each
 * event, and an ``electrodes`` ``DynamicTableRegion`` linking the channel
 * axis back to rows of ``ElectrodesTable``.  Modeled on the existing
 * ``ElectricalSeries`` writer (``src/nwb/ecephys/ElectricalSeries.cpp``)
 * for the electrodes-region wiring; the dataset shapes follow the
 * NWB schema embedded in ``src/spec/core.hpp`` (``nwb_ecephys`` constant).
 *
 * Recording flow:
 *  - ``initialize()`` lays out all four datasets and attributes once.
 *  - Each call to ``writeEvents()`` appends ``numEvents`` rows along
 *    axis 0 of both ``features`` and ``times``.  The other two datasets
 *    (``description``, ``electrodes``) are write-once at init.
 */
class FeatureExtraction : public NWBDataInterface
{
public:
  REGISTER_SUBCLASS(FeatureExtraction,
                    NWBDataInterface,
                    AQNWB::SPEC::CORE::namespaceName)

protected:
  /**
   * @brief Constructor.
   * @param path Path to the FeatureExtraction group in the file.
   * @param io A shared pointer to the IO object.
   */
  FeatureExtraction(const std::string& path, std::shared_ptr<IO::BaseIO> io);

public:
  /**
   * @brief Destructor.
   */
  ~FeatureExtraction() override;

  /**
   * @brief Initialize the FeatureExtraction object: lay out all four
   * datasets (``features``, ``times``, ``description``, ``electrodes``)
   * and attach the schema-required attributes / DynamicTableRegion.
   *
   * @param featuresConfig Configuration for the ``features`` dataset.  Must
   *                       be 3-D ``[0, num_channels, num_features]`` (the
   *                       0 leaves the time axis extendable).  ``dtype``
   *                       must be ``F32`` per the NWB schema.
   * @param timesConfig Configuration for the ``times`` dataset.  Must be
   *                    1-D ``[0]`` for an extendable time axis, ``dtype=F64``.
   * @param featureLabels A 1-D vector of strings naming each feature dim,
   *                      e.g. ``{"spk", "sbp"}``.  Length must equal the
   *                      ``num_features`` axis of ``featuresConfig`` (passed
   *                      explicitly via ``numFeatures``).
   * @param electrodeIndices Row indices into ``ElectrodesTable`` that the
   *                        channel axis maps to, length equals the
   *                        ``num_channels`` axis of ``featuresConfig``
   *                        (passed explicitly via ``numChannels``).
   * @param numChannels Length of the ``num_channels`` axis on ``features``.
   *                    ``BaseArrayDataSetConfig`` doesn't expose its shape
   *                    polymorphically, so the caller supplies it directly.
   * @param numFeatures Length of the ``num_features`` axis on ``features``.
   * @param description Per-instance description (stored as the standard
   *                    NWBContainer description attribute).
   * @return ``Status::Success`` on success; ``Status::Failure`` if the
   *         shapes are inconsistent or any HDF5 op fails.
   */
  Status initialize(const IO::BaseArrayDataSetConfig& featuresConfig,
                    const IO::BaseArrayDataSetConfig& timesConfig,
                    const std::vector<std::string>& featureLabels,
                    const std::vector<int>& electrodeIndices,
                    SizeType numChannels,
                    SizeType numFeatures,
                    const std::string& description = "no description");

  /**
   * @brief Append ``numEvents`` rows along axis 0 of both ``features``
   * and ``times``.
   *
   * @param numEvents Number of new events (rows) to append.
   * @param featuresInput Pointer to a contiguous ``[numEvents,
   *                      numChannels, numFeatures]`` float32 buffer.
   * @param timesInput Pointer to a ``numEvents``-long float64 timestamps
   *                   buffer.
   * @return Write status.
   */
  Status writeEvents(const SizeType& numEvents,
                     const void* featuresInput,
                     const void* timesInput);

  /**
   * @brief Number of channels (axis 1 of ``features``).  Set during
   * ``initialize`` from the config; the writer uses this to compute
   * write offsets.
   */
  SizeType numChannels() const { return m_numChannels; }

  /**
   * @brief Number of feature dimensions (axis 2 of ``features``).
   */
  SizeType numFeatures() const { return m_numFeatures; }

  DEFINE_DATASET_FIELD(readFeatures,
                       recordFeatures,
                       float,
                       "features",
                       Multi-dimensional array of features extracted from each event)

  DEFINE_DATASET_FIELD(readTimes,
                       recordTimes,
                       double,
                       "times",
                       Times of events that features correspond to)

  DEFINE_DATASET_FIELD(readDescriptionDataset,
                       recordDescriptionDataset,
                       std::string,
                       "description",
                       Description of features for each of the extracted features)

  DEFINE_DATASET_FIELD(readElectrodes,
                       recordElectrodes,
                       int,
                       "electrodes",
                       The indices of the electrodes that generated this feature extraction)

  DEFINE_REFERENCED_REGISTERED_FIELD(
      readElectrodesTable,
      ElectrodesTable,
      "electrodes/table",
      The electrodes table retrieved from the object referenced in the
      `electrodes / table` attribute.)

private:
  SizeType m_numChannels = 0;
  SizeType m_numFeatures = 0;
  SizeType m_eventsRecorded = 0;
};
}  // namespace AQNWB::NWB
