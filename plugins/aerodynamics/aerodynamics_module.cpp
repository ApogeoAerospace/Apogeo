#include "aerodynamics_module.h"

#include <hdf5.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace {

std::string resolveExistingPath(const std::string& input_path, const std::filesystem::path& base_dir = {}) {
    namespace fs = std::filesystem;
    std::error_code ec;

    const fs::path original(input_path);
    if (original.is_absolute() && fs::exists(original, ec)) {
        return original.string();
    }

    std::vector<fs::path> candidates;
    candidates.push_back(original);
    if (!base_dir.empty()) {
        candidates.push_back(base_dir / original);
    }

    const fs::path cwd = fs::current_path(ec);
    if (!ec && !cwd.empty()) {
        candidates.push_back(cwd / original);

        fs::path cursor = cwd;
        for (int i = 0; i < 6; ++i) {
            candidates.push_back(cursor / original);
            if (!cursor.has_parent_path()) {
                break;
            }
            cursor = cursor.parent_path();
        }
    }

    for (const auto& candidate : candidates) {
        if (candidate.empty()) {
            continue;
        }

        const fs::path normalized = candidate.lexically_normal();
        if (fs::exists(normalized, ec)) {
            return normalized.string();
        }
    }

    return input_path;
}

std::vector<unsigned long long> toPublicDimensions(const std::vector<hsize_t>& dims) {
    std::vector<unsigned long long> out;
    out.reserve(dims.size());
    for (const hsize_t dim : dims) {
        out.push_back(static_cast<unsigned long long>(dim));
    }
    return out;
}

bool readDoubleVectorDataset(hid_t file, const std::string& dataset_path, std::vector<double>& values) {
    if (H5Lexists(file, dataset_path.c_str(), H5P_DEFAULT) <= 0) {
        return false;
    }

    const hid_t dataset = H5Dopen2(file, dataset_path.c_str(), H5P_DEFAULT);
    if (dataset < 0) {
        return false;
    }

    const hid_t dataspace = H5Dget_space(dataset);
    if (dataspace < 0) {
        H5Dclose(dataset);
        return false;
    }

    const int rank = H5Sget_simple_extent_ndims(dataspace);
    if (rank != 1) {
        H5Sclose(dataspace);
        H5Dclose(dataset);
        return false;
    }

    hsize_t dims[1] = {0};
    if (H5Sget_simple_extent_dims(dataspace, dims, nullptr) < 0 || dims[0] == 0) {
        H5Sclose(dataspace);
        H5Dclose(dataset);
        return false;
    }

    values.resize(static_cast<size_t>(dims[0]));
    const herr_t status = H5Dread(dataset, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, values.data());

    H5Sclose(dataspace);
    H5Dclose(dataset);
    return status >= 0;
}

bool readDatasetDimensions(hid_t group, const char* name, std::vector<unsigned long long>& dims_out) {
    const hid_t dataset = H5Dopen2(group, name, H5P_DEFAULT);
    if (dataset < 0) {
        return false;
    }

    const hid_t dataspace = H5Dget_space(dataset);
    if (dataspace < 0) {
        H5Dclose(dataset);
        return false;
    }

    const int rank = H5Sget_simple_extent_ndims(dataspace);
    if (rank <= 0) {
        H5Sclose(dataspace);
        H5Dclose(dataset);
        return false;
    }

    std::vector<hsize_t> dims(static_cast<size_t>(rank));
    if (H5Sget_simple_extent_dims(dataspace, dims.data(), nullptr) < 0) {
        H5Sclose(dataspace);
        H5Dclose(dataset);
        return false;
    }

    dims_out = toPublicDimensions(dims);

    H5Sclose(dataspace);
    H5Dclose(dataset);
    return true;
}

struct CoefficientScanContext {
    std::map<std::string, std::vector<unsigned long long>>* dimensions = nullptr;
};

herr_t collectCoefficientDataset(hid_t group, const char* name, const H5L_info_t*, void* user_data) {
    auto* context = static_cast<CoefficientScanContext*>(user_data);
    if (!context || !context->dimensions || !name) {
        return 0;
    }

    std::vector<unsigned long long> dims;
    if (readDatasetDimensions(group, name, dims)) {
        (*context->dimensions)[name] = dims;
    }

    return 0;
}

bool dimensionsMatchAxes(
    const std::vector<unsigned long long>& dims,
    const std::vector<aerodynamics::AeroAxis>& axes
) {
    if (dims.size() != axes.size()) {
        return false;
    }

    for (size_t i = 0; i < dims.size(); ++i) {
        if (dims[i] != axes[i].values.size()) {
            return false;
        }
    }

    return true;
}

} // namespace

namespace aerodynamics {

bool AerodynamicsModule::loadFromConfig(const std::string& config_path) {
    const std::string resolved_config_path = resolveExistingPath(config_path);
    std::ifstream file(resolved_config_path);
    if (!file.is_open()) {
        return false;
    }

    try {
        nlohmann::json config;
        file >> config;

        const auto& database_config = config.at("vehicle_models").at("aero_database");
        const std::string format = database_config.at("source").value("format", "");
        if (format != "hdf5") {
            return false;
        }

        const std::string database_uri = database_config.at("source").at("uri").get<std::string>();
        const std::vector<std::string> axis_names = database_config.at("axes").get<std::vector<std::string>>();
        if (axis_names.empty()) {
            return false;
        }

        const std::filesystem::path config_dir = std::filesystem::path(resolved_config_path).parent_path();
        const std::string resolved_database_path = resolveExistingPath(database_uri, config_dir);
        return loadDatabase(resolved_database_path, axis_names);
    } catch (...) {
        return false;
    }
}

bool AerodynamicsModule::loadDatabase(const std::string& database_path, const std::vector<std::string>& axis_names) {
    if (axis_names.empty()) {
        return false;
    }

    loaded_ = false;
    axes_.clear();
    coefficient_dimensions_.clear();
    database_path_.clear();

    H5E_auto2_t old_error_func = nullptr;
    void* old_error_client_data = nullptr;
    H5Eget_auto2(H5E_DEFAULT, &old_error_func, &old_error_client_data);
    H5Eset_auto2(H5E_DEFAULT, nullptr, nullptr);

    const std::string resolved_database_path = resolveExistingPath(database_path);
    const hid_t file = H5Fopen(resolved_database_path.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);
    H5Eset_auto2(H5E_DEFAULT, old_error_func, old_error_client_data);

    if (file < 0) {
        return false;
    }

    for (const std::string& axis_name : axis_names) {
        AeroAxis axis;
        axis.name = axis_name;
        if (!readDoubleVectorDataset(file, "/axes/" + axis_name, axis.values)) {
            H5Fclose(file);
            axes_.clear();
            return false;
        }
        axes_.push_back(axis);
    }

    if (H5Lexists(file, "/coefficients", H5P_DEFAULT) <= 0) {
        H5Fclose(file);
        axes_.clear();
        return false;
    }

    const hid_t coefficient_group = H5Gopen2(file, "/coefficients", H5P_DEFAULT);
    if (coefficient_group < 0) {
        H5Fclose(file);
        axes_.clear();
        return false;
    }

    CoefficientScanContext context{&coefficient_dimensions_};
    hsize_t index = 0;
    H5Literate(coefficient_group, H5_INDEX_NAME, H5_ITER_INC, &index, collectCoefficientDataset, &context);

    H5Gclose(coefficient_group);
    H5Fclose(file);

    if (coefficient_dimensions_.empty()) {
        axes_.clear();
        return false;
    }

    for (const auto& item : coefficient_dimensions_) {
        if (!dimensionsMatchAxes(item.second, axes_)) {
            axes_.clear();
            coefficient_dimensions_.clear();
            return false;
        }
    }

    database_path_ = resolved_database_path;
    loaded_ = true;
    return true;
}

bool AerodynamicsModule::isLoaded() const {
    return loaded_;
}

const std::string& AerodynamicsModule::databasePath() const {
    return database_path_;
}

const std::vector<AeroAxis>& AerodynamicsModule::axes() const {
    return axes_;
}

const std::map<std::string, std::vector<unsigned long long>>& AerodynamicsModule::coefficientDimensions() const {
    return coefficient_dimensions_;
}

AeroCoefficients AerodynamicsModule::getCoefficients(const state_vector::GeneralState* state_vector) const {
    (void)state_vector;
    return {};
}

} // namespace aerodynamics
