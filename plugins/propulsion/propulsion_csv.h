/** @file propulsion_csv.h
 * @brief Strict version-1 engine CSV ingestion, separate from runtime physics.
 */
#pragma once
#include "propulsion_module.h"

namespace propulsion {
/** @brief Reads and validates an entire engine curve.
 * @param path Absolute path or path relative to cwd.
 * @return Fully validated table.
 * @throws std::runtime_error With file, physical line and validation reason.
 */
EngineCurve read_engine_curve_csv(const std::string& path);
} // namespace propulsion
