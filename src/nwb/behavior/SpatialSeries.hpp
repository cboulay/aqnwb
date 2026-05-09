#pragma once

#include <string>

#include "Utils.hpp"
#include "io/BaseIO.hpp"
#include "io/ReadIO.hpp"
#include "nwb/base/TimeSeries.hpp"
#include "spec/core.hpp"

namespace AQNWB::NWB
{
/**
 * @brief Direction, e.g., of gaze or travel, or position.
 *
 * The data field is a 1-D or 2-D array storing position or direction relative
 * to some reference frame.  Array structure: ``[num_measurements]`` or
 * ``[num_measurements, num_dimensions]``.  ``num_dimensions`` is conventionally
 * 1, 2, or 3 per the NWB SpatialSeries schema, but the writer does not
 * enforce that constraint — callers writing higher-dimensional data should
 * be aware that ``nwbinspector`` will flag it.  Each ``SpatialSeries`` has a
 * ``reference_frame`` text dataset describing what "zero" means in the
 * coordinate system the data lives in.
 */
class SpatialSeries : public TimeSeries
{
public:
  // Register the SpatialSeries as a subclass of TimeSeries
  REGISTER_SUBCLASS(SpatialSeries,
                    TimeSeries,
                    AQNWB::SPEC::CORE::namespaceName)

protected:
  /**
   * @brief Constructor.
   * @param path The location of the SpatialSeries in the file.
   * @param io A shared pointer to the IO object.
   */
  SpatialSeries(const std::string& path, std::shared_ptr<IO::BaseIO> io);

public:
  /**
   * @brief Destructor.
   */
  ~SpatialSeries() override;

  /**
   * @brief Initializes the SpatialSeries.
   *
   * @param dataConfig Configuration for the data dataset (dtype, shape,
   *                   chunking).  The shape is typically a 1-D or 2-D vector
   *                   ``[num_times]`` or ``[num_times, num_dimensions]``.
   * @param unit Base unit of measurement for working with the data
   *             (e.g. ``"meters"``, ``"pixels"``, ``"radians"``).  Unlike
   *             ``ElectricalSeries`` which fixes ``unit`` to ``"volts"``,
   *             ``SpatialSeries`` lets the caller choose.
   * @param description Human-readable description of the series.
   * @param referenceFrame Description of what the zero point / zero axes
   *                       mean in this coordinate system.  Optional; pass
   *                       an empty string to omit the dataset.
   * @param comments Free-form comments about the recording.
   * @param conversion Scalar multiplier from raw data to ``unit``.
   * @param resolution Smallest meaningful difference between data values.
   * @param offset Scalar offset added after scaling by ``conversion``.
   * @return The status of the initialization operation.
   */
  Status initialize(const IO::BaseArrayDataSetConfig& dataConfig,
                    const std::string& unit,
                    const std::string& description = "no description",
                    const std::string& referenceFrame = "",
                    const std::string& comments = "no comments",
                    const float& conversion = 1.0f,
                    const float& resolution = -1.0f,
                    const float& offset = 0.0f);

  /**
   * @brief Writes a block of multi-dimensional samples in a single operation.
   *
   * Caller provides interleaved row-major data shaped ``[numSamples,
   * numDimensions]`` (or ``[numSamples]`` for 1-D series).
   *
   * @param numSamples Number of time samples (rows) to write.
   * @param numDimensions Number of spatial dimensions per sample.  Use 1
   *                      when data is 1-D.
   * @param dataInput Pointer to the data buffer.
   * @param timestampsInput Pointer to a ``numSamples``-long timestamps
   *                        buffer, or ``nullptr`` if the series uses
   *                        implicit timing.
   * @param controlInput Optional pointer to control values.
   * @return The write status.
   */
  Status writeAllChannels(const SizeType& numSamples,
                          const SizeType& numDimensions,
                          const void* dataInput,
                          const void* timestampsInput = nullptr,
                          const void* controlInput = nullptr);

  DEFINE_DATASET_FIELD(
      readData, recordData, float, "data", Recorded position or direction data)

  DEFINE_ATTRIBUTE_FIELD(readDataUnit,
                        std::string,
                        "data/unit",
                        Base unit of measurement for working with the data)

  DEFINE_DATASET_FIELD(readReferenceFrame,
                       recordReferenceFrame,
                       std::string,
                       "reference_frame",
                       Description defining what "straight-ahead" means)

private:
  /**
   * @brief Number of samples already written.  Tracked to compute the
   * offset for the next ``writeAllChannels`` call.
   */
  SizeType m_samplesRecorded = 0;
};
}  // namespace AQNWB::NWB
